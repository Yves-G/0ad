/* Copyright (C) 2013 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "ScriptInterface.h"
// #include "DebuggingServer.h" // JS debugger temporarily disabled during the SpiderMonkey upgrade (check trac ticket #2348 for details)
#include "ScriptStats.h"
#include "AutoRooters.h"

#include "lib/debug.h"
#include "lib/utf8.h"
#include "ps/CLogger.h"
#include "ps/Filesystem.h"
#include "ps/Profile.h"
#include "ps/utf16string.h"

#include <cassert>
#include <map>

#define BOOST_MULTI_INDEX_DISABLE_SERIALIZATION
#include <boost/preprocessor/punctuation/comma_if.hpp>
#include <boost/preprocessor/repetition/repeat.hpp>
#include <boost/random/linear_congruential.hpp>
#include <boost/flyweight.hpp>
#include <boost/flyweight/key_value.hpp>
#include <boost/flyweight/no_locking.hpp>
#include <boost/flyweight/no_tracking.hpp>

#include "valgrind.h"

#define STACK_CHUNK_SIZE 8192

#include "scriptinterface/ScriptExtraHeaders.h"

/**
 * @file
 * Abstractions of various SpiderMonkey features.
 * Engine code should be using functions of these interfaces rather than
 * directly accessing the underlying JS api.
 */

////////////////////////////////////////////////////////////////

/**
 * Abstraction around a SpiderMonkey JSRuntime.
 * Each ScriptRuntime can be used to initialize several ScriptInterface
 * contexts which can then share data, but a single ScriptRuntime should
 * only be used on a single thread.
 *
 * (One means to share data between threads and runtimes is to create
 * a ScriptInterface::StructuredClone.)
 */


void GCSliceCallbackHook(JSRuntime* UNUSED(rt), JS::GCProgress progress, const JS::GCDescription& UNUSED(desc))
{
	/*
	 * During non-incremental GC, the GC is bracketed by JSGC_CYCLE_BEGIN/END
	 * callbacks. During an incremental GC, the sequence of callbacks is as
	 * follows:
	 *   JSGC_CYCLE_BEGIN, JSGC_SLICE_END  (first slice)
	 *   JSGC_SLICE_BEGIN, JSGC_SLICE_END  (second slice)
	 *   ...
	 *   JSGC_SLICE_BEGIN, JSGC_CYCLE_END  (last slice)
	*/


	if (progress == JS::GC_SLICE_BEGIN)
	{
		if (CProfileManager::IsInitialised() && ThreadUtil::IsMainThread())
			g_Profiler.Start("GCSlice");
		g_Profiler2.RecordRegionEnter("GCSlice");
	}
	else if (progress == JS::GC_SLICE_END)
	{
		if (CProfileManager::IsInitialised() && ThreadUtil::IsMainThread())
			g_Profiler.Stop();
    	g_Profiler2.RecordRegionLeave("GCSlice");
	}
	else if (progress == JS::GC_CYCLE_BEGIN)
	{
		if (CProfileManager::IsInitialised() && ThreadUtil::IsMainThread())
			g_Profiler.Start("GCSlice");
		g_Profiler2.RecordRegionEnter("GCSlice");
	}
	else if (progress == JS::GC_CYCLE_END)
	{
		if (CProfileManager::IsInitialised() && ThreadUtil::IsMainThread())
			g_Profiler.Stop();
    	g_Profiler2.RecordRegionLeave("GCSlice");
	}

	// The following code can be used to print some information aobut garbage collection
	// Search for "Nonincremental reason" if there are problems running GC incrementally.
	#if 0
	if (progress == JS::GCProgress::GC_CYCLE_BEGIN)
		printf("starting cycle ===========================================\n");

	const jschar* str = desc.formatMessage(rt);
	int len = 0;
	
	for(int i = 0; i < 10000; i++)
	{
		len++;
		if(!str[i])
			break;
	}
	
	wchar_t outstring[len];
	
	for(int i = 0; i < len; i++)
	{
		outstring[i] = (wchar_t)str[i];
	}
	
	printf("---------------------------------------\n: %ls \n---------------------------------------\n", outstring);
	#endif
}
 
class ScriptRuntime
{
public:
	ScriptRuntime(int runtimeSize): 
		m_rooter(NULL),
		m_LastGCBytes(0)
	{
		if (!m_Initialized)
			ENSURE(JS_Init());
			
		m_rt = JS_NewRuntime(runtimeSize, JS_USE_HELPER_THREADS);
		ENSURE(m_rt); // TODO: error handling

		JS_SetNativeStackQuota(m_rt, 128 * sizeof(size_t) * 1024);
		if (g_ScriptProfilingEnabled)
		{
			// Profiler isn't thread-safe, so only enable this on the main thread
			if (ThreadUtil::IsMainThread())
			{
				if (CProfileManager::IsInitialised())
				{
					JS_SetExecuteHook(m_rt, jshook_script, this);
					JS_SetCallHook(m_rt, jshook_function, this);
				}
			}
		}
		
		JS::SetGCSliceCallback(m_rt, GCSliceCallbackHook);
		
		JS_SetGCParameter(m_rt, JSGC_MAX_MALLOC_BYTES, 384 * 1024 * 1024);
		JS_SetGCParameter(m_rt, JSGC_MAX_BYTES, 384 * 1024 * 1024);
		JS_SetGCParameter(m_rt, JSGC_MODE, JSGC_MODE_INCREMENTAL);
		//JS_SetGCParameter(m_rt, JSGC_SLICE_TIME_BUDGET, 5);
		JS_SetGCParameter(m_rt, JSGC_ALLOCATION_THRESHOLD, 256);
		
		// The whole heap-growth mechanism seems to work only for non-incremental GCs.
		// We disable it to make it more clear if full GCs happen triggered by this JSAPI internal mechanism.
		JS_SetGCParameter(m_rt, JSGC_DYNAMIC_HEAP_GROWTH, false);
		
		JS_AddExtraGCRootsTracer(m_rt, jshook_trace, this);
		
		m_dummyContext = JS_NewContext(m_rt, STACK_CHUNK_SIZE);
		ENSURE(m_dummyContext);
	}
	
	void MaybeIncrementalGC()
	{
		PROFILE3("MaybeIncrementalGC");
		if (JS::IsIncrementalGCEnabled(m_rt))
		{
			// The idea is to get the heap size after a completed GC and trigger the next GC when the heap size has
			// reached m_LastGCBytes + X. 
			// In practice it doesn't quite work like that. When the incremental marking is completed, the sweeping kicks in.
			// The sweeping actually frees memory and it does this in a background thread (if JS_USE_HELPER_THREADS is set).
			// While the sweeping is happening we already run scripts again and produce new garbage.
			int gcBytes = JS_GetGCParameter(m_rt, JSGC_BYTES);
			//printf("gcBytes: %d \n", gcBytes);
			if (m_LastGCBytes > gcBytes || m_LastGCBytes == 0)
			{
				//printf("Setting m_LastGCBytes: %d \n", gcBytes); // debugging
				m_LastGCBytes = gcBytes;
			}
			
			// Run an additional incremental GC slice if the currently running incremental GC isn't over yet 
			// ... or
			// start a new incremental GC if the JS heap size has grown enough for a GC to make sense
			if (JS::IsIncrementalGCInProgress(m_rt) || (gcBytes - m_LastGCBytes > 20 * 1024 * 1024))
			{
				/* Use for debugging
				if (JS::IsIncrementalGCInProgress(m_rt))
					printf("Running incremental garbage collection because an incremental cycle is in progress. \n");
				else
					printf("Running incremental garbage collection because JSGC_BYTES - m_LastGCBytes > X ---- JSGC_BYTES: %d      m_LastGCBytes: %d", gcBytes, m_LastGCBytes);
				*/
				PrepareContextsForIncrementalGC();
				JS::IncrementalGC(m_rt, JS::gcreason::REFRESH_FRAME, 10);
				m_LastGCBytes = gcBytes;
			}
		}
	}
	
	void RegisterContext(JSContext* cx)
	{
		m_Contexts.push_back(cx);
	}
	
	void UnRegisterContext(JSContext* cx)
	{
		m_Contexts.remove(cx);
	}

	~ScriptRuntime()
	{
		JS_RemoveExtraGCRootsTracer(m_rt, jshook_trace, this);
		JS_DestroyContext(m_dummyContext);
		JS_DestroyRuntime(m_rt);
	}
	
	JSRuntime* m_rt;
	AutoGCRooter* m_rooter;

private:
	
	// Workaround for: https://bugzilla.mozilla.org/show_bug.cgi?id=890243
	JSContext* m_dummyContext;
	static bool m_Initialized;
	
	int m_LastGCBytes;
	
	void PrepareContextsForIncrementalGC()
	{
		for (std::list<JSContext*>::iterator itr = m_Contexts.begin(); itr != m_Contexts.end(); itr++)
		{
			JS::PrepareZoneForGC(js::GetCompartmentZone(js::GetContextCompartment(*itr)));
		}
	}
	
	std::list<JSContext*> m_Contexts;


	static void* jshook_script(JSContext* UNUSED(cx), JSAbstractFramePtr UNUSED(fp), bool UNUSED(isConstructing), bool before, bool* UNUSED(ok), void* closure)
	{
		if (before)
			g_Profiler.StartScript("script invocation");
		else
			g_Profiler.Stop();

		return closure;
	}

	// To profile scripts usefully, we use a call hook that's called on every enter/exit,
	// and need to find the function name. But most functions are anonymous so we make do
	// with filename plus line number instead.
	// Computing the names is fairly expensive, and we need to return an interned char*
	// for the profiler to hold a copy of, so we use boost::flyweight to construct interned
	// strings per call location.

	// Identifies a location in a script
	struct ScriptLocation
	{
		JSContext* cx;
		JSScript* script;
		jsbytecode* pc;

		bool operator==(const ScriptLocation& b) const
		{
			return cx == b.cx && script == b.script && pc == b.pc;
		}

		friend std::size_t hash_value(const ScriptLocation& loc)
		{
			std::size_t seed = 0;
			boost::hash_combine(seed, loc.cx);
			boost::hash_combine(seed, loc.script);
			boost::hash_combine(seed, loc.pc);
			return seed;
		}
	};

	// Computes and stores the name of a location in a script
	struct ScriptLocationName
	{
		ScriptLocationName(const ScriptLocation& loc)
		{
			JSContext* cx = loc.cx;
			JSScript* script = loc.script;
			jsbytecode* pc = loc.pc;

			std::string filename = JS_GetScriptFilename(script);
			size_t slash = filename.rfind('/');
			if (slash != filename.npos)
				filename = filename.substr(slash+1);

			uint line = JS_PCToLineNumber(cx, script, pc);

			std::stringstream ss;
			ss << "(" << filename << ":" << line << ")";
			name = ss.str();
		}

		std::string name;
	};

	// Flyweight types (with no_locking because the call hooks are only used in the
	// main thread, and no_tracking because we mustn't delete values the profiler is
	// using and it's not going to waste much memory)
	typedef boost::flyweight<
		std::string,
		boost::flyweights::no_tracking,
		boost::flyweights::no_locking
	> StringFlyweight;
	typedef boost::flyweight<
		boost::flyweights::key_value<ScriptLocation, ScriptLocationName>,
		boost::flyweights::no_tracking,
		boost::flyweights::no_locking
	> LocFlyweight;

	static void* jshook_function(JSContext* cx, JSAbstractFramePtr fp, bool UNUSED(isConstructing), bool before, bool* UNUSED(ok), void* closure)
	{
		if (!before)
		{
			g_Profiler.Stop();
			return closure;
		}

		JSFunction* fn = fp.maybeFun();
		if (!fn)
		{
			g_Profiler.StartScript("(function)");
			return closure;
		}

		// Try to get the name of non-anonymous functions
		JSString* name = JS_GetFunctionId(fn);
		if (name)
		{
			char* chars = JS_EncodeString(cx, name);
			if (chars)
			{
				g_Profiler.StartScript(StringFlyweight(chars).get().c_str());
				JS_free(cx, chars);
				return closure;
			}
		}

		// No name - compute from the location instead
		JS::AutoFilename fileName;
		unsigned lineno;
		JS::DescribeScriptedCaller(cx, &fileName, &lineno);
		
		std::stringstream ss;
		ss << "(" << fileName.get() << ":" << lineno << ")";		
		g_Profiler.StartScript(ss.str().c_str());

		return closure;
	}

	static void jshook_trace(JSTracer* trc, void* data)
	{
		ScriptRuntime* m = static_cast<ScriptRuntime*>(data);

		if (m->m_rooter)
			m->m_rooter->Trace(trc);
	}
};

bool ScriptRuntime::m_Initialized = false;

shared_ptr<ScriptRuntime> ScriptInterface::CreateRuntime(int runtimeSize)
{
	return shared_ptr<ScriptRuntime>(new ScriptRuntime(runtimeSize));
}

////////////////////////////////////////////////////////////////

struct ScriptInterface_impl
{
	ScriptInterface_impl(const char* nativeScopeName, const shared_ptr<ScriptRuntime>& runtime);
	~ScriptInterface_impl();
	void Register(const char* name, JSNative fptr, uint nargs);

	shared_ptr<ScriptRuntime> m_runtime;
	JSContext* m_cx;
	JSObject* m_glob; // global scope object
	JSCompartment* m_comp;
	boost::rand48* m_rng;
	JSObject* m_nativeScope; // native function scope object

	typedef std::map<ScriptInterface::CACHED_VAL, CScriptValRooted> ScriptValCache;
	ScriptValCache m_ScriptValCache;
};

namespace
{

JSClass global_class = {
	"global", JSCLASS_GLOBAL_FLAGS,
	JS_PropertyStub, JS_DeletePropertyStub, 
	JS_PropertyStub, JS_StrictPropertyStub,
	JS_EnumerateStub, JS_ResolveStub, 
	JS_ConvertStub, NULL, 
	NULL, NULL, NULL, NULL
};

void ErrorReporter(JSContext* cx, const char* message, JSErrorReport* report)
{
	std::stringstream msg;
	bool isWarning = JSREPORT_IS_WARNING(report->flags);
	msg << (isWarning ? "JavaScript warning: " : "JavaScript error: ");
	if (report->filename)
	{
		msg << report->filename;
		msg << " line " << report->lineno << "\n";
	}

	msg << message;

	// If there is an exception, then print its stack trace
	JS::RootedValue excn(cx);
	if (JS_GetPendingException(cx, &excn) && excn.isObject())
	{
		JS::RootedObject excnObj(cx, &excn.toObject());
		// TODO: this violates the docs ("The error reporter callback must not reenter the JSAPI.")

		// Hide the exception from EvaluateScript
		JSExceptionState* excnState = JS_SaveExceptionState(cx);
		JS_ClearPendingException(cx);

		jsval rval;
		const char dumpStack[] = "this.stack.trimRight().replace(/^/mg, '  ')"; // indent each line
		if (JS_EvaluateScript(cx, excnObj, dumpStack, ARRAY_SIZE(dumpStack)-1, "(eval)", 1, &rval))
		{
			std::string stackTrace;
			if (ScriptInterface::FromJSVal(cx, rval, stackTrace))
				msg << "\n" << stackTrace;

			JS_RestoreExceptionState(cx, excnState);
		}
		else
		{
			// Error got replaced by new exception from EvaluateScript
			JS_DropExceptionState(cx, excnState);
		}
	}

	if (isWarning)
		LOGWARNING(L"%hs", msg.str().c_str());
	else
		LOGERROR(L"%hs", msg.str().c_str());

	// When running under Valgrind, print more information in the error message
//	VALGRIND_PRINTF_BACKTRACE("->");
}

// Functions in the global namespace:

bool print(JSContext* cx, uint argc, jsval* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
	for (uint i = 0; i < argc; ++i)
	{
		std::wstring str;
		if (!ScriptInterface::FromJSVal(cx, args[i], str))
			return false;
		debug_printf(L"%ls", str.c_str());
	}
	fflush(stdout);
	rec.rval().set(JS::UndefinedValue());
	return true;
}

bool logmsg(JSContext* cx, uint argc, jsval* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
	if (argc < 1)
	{
		rec.rval().set(JS::UndefinedValue());
		return true;
	}

	std::wstring str;
	if (!ScriptInterface::FromJSVal(cx, args[0], str))
		return false;
	LOGMESSAGE(L"%ls", str.c_str());
	rec.rval().set(JS::UndefinedValue());
	return true;
}

bool warn(JSContext* cx, uint argc, jsval* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
	if (argc < 1)
	{
		rec.rval().set(JS::UndefinedValue());
		return true;
	}

	std::wstring str;
	if (!ScriptInterface::FromJSVal(cx, args[0], str))
		return false;
	LOGWARNING(L"%ls", str.c_str());
	rec.rval().set(JS::UndefinedValue());
	return true;
}

bool error(JSContext* cx, uint argc, jsval* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
	if (argc < 1)
	{
		rec.rval().set(JS::UndefinedValue());
		return true;
	}

	std::wstring str;
	if (!ScriptInterface::FromJSVal(cx, args[0], str))
		return false;
	LOGERROR(L"%ls", str.c_str());
	rec.rval().set(JS::UndefinedValue());
	return true;
}

bool deepcopy(JSContext* cx, uint argc, jsval* vp)
{
	JSAutoRequest rq(cx);
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
	if (argc < 1)
	{
		rec.rval().set(JS::UndefinedValue());
		return true;
	}

	JS::RootedValue ret(cx);
	if (!JS_StructuredClone(cx, args[0], &ret, NULL, NULL))
		return false;

	rec.rval().set(ret);
	return true;
}

bool ProfileStart(JSContext* cx, uint argc, jsval* vp)
{
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);
	JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
	const char* name = "(ProfileStart)";

	if (argc >= 1)
	{
		std::string str;
		if (!ScriptInterface::FromJSVal(cx, args[0], str))
			return false;

		typedef boost::flyweight<
			std::string,
			boost::flyweights::no_tracking,
			boost::flyweights::no_locking
		> StringFlyweight;

		name = StringFlyweight(str).get().c_str();
	}

	if (CProfileManager::IsInitialised() && ThreadUtil::IsMainThread())
		g_Profiler.StartScript(name);

	g_Profiler2.RecordRegionEnter(name);

	rec.rval().set(JS::UndefinedValue());
	return true;
}

bool ProfileStop(JSContext* UNUSED(cx), uint UNUSED(argc), jsval* vp)
{
	JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
	if (CProfileManager::IsInitialised() && ThreadUtil::IsMainThread())
		g_Profiler.Stop();

	g_Profiler2.RecordRegionLeave("(ProfileStop)");

	rec.rval().set(JS::UndefinedValue());
	return true;
}

// Math override functions:

// boost::uniform_real is apparently buggy in Boost pre-1.47 - for integer generators
// it returns [min,max], not [min,max). The bug was fixed in 1.47.
// We need consistent behaviour, so manually implement the correct version:
static double generate_uniform_real(boost::rand48& rng, double min, double max)
{
	while (true)
	{
		double n = (double)(rng() - rng.min());
		double d = (double)(rng.max() - rng.min()) + 1.0;
		ENSURE(d > 0 && n >= 0 && n <= d);
		double r = n / d * (max - min) + min;
		if (r < max)
			return r;
	}
}

bool Math_random(JSContext* cx, uint UNUSED(argc), jsval* vp)
{
	JS::CallReceiver rec = JS::CallReceiverFromVp(vp);
	double r;
	if(!ScriptInterface::GetScriptInterfaceAndCBData(cx)->pScriptInterface->MathRandom(r))
		return false;

	rec.rval().set(JS::NumberValue(r));
	return true;
}

} // anonymous namespace

bool ScriptInterface::MathRandom(double& nbr)
{
	if (m->m_rng == NULL)
		return false;
	nbr = generate_uniform_real(*(m->m_rng), 0.0, 1.0);
	return true;
}

ScriptInterface_impl::ScriptInterface_impl(const char* nativeScopeName, const shared_ptr<ScriptRuntime>& runtime) :
	m_runtime(runtime)
{
	bool ok;

	m_cx = JS_NewContext(m_runtime->m_rt, STACK_CHUNK_SIZE);
	ENSURE(m_cx);

	JS_SetParallelIonCompilationEnabled(m_runtime->m_rt, true);

	// For GC debugging:
	// JS_SetGCZeal(m_cx, 2);

	JS_SetContextPrivate(m_cx, NULL);

	JS_SetErrorReporter(m_cx, ErrorReporter);
	
	JS_SetGlobalJitCompilerOption(m_runtime->m_rt, JSJITCOMPILER_ION_ENABLE, 1);
	JS_SetGlobalJitCompilerOption(m_runtime->m_rt, JSJITCOMPILER_BASELINE_ENABLE, 1);
	
	// TODO: figure out how to enable:
	//		- CompileAndGo
	//		- TypeInference
	// .. and think more about where to enable the options (e.g. runtime vs context)   and check the other options more closely.
	JS::ContextOptionsRef(m_cx).setExtraWarnings(1)
		.setWerror(0)
		.setVarObjFix(1)
		//.setCompileAndGo(1)
		//.setDontReportUncaught( )
		//.setNoDefaultCompartmentObject( )
		//.setNoScriptRval( )
		.setStrictMode(1);
		//.setBaseline(options & JSOPTION_BASELINE)
		//.setTypeInference(1)
		//.setIon(options & JSOPTION_ION)
		//.setAsmJS(0);

	JS::CompartmentOptions opt;
	opt.setVersion(JSVERSION_LATEST);
	
	JSAutoRequest rq(m_cx);
	JS::RootedObject globalRootedVal(m_cx, JS_NewGlobalObject(m_cx, &global_class, NULL, JS::OnNewGlobalHookOption::DontFireOnNewGlobalHook, opt));
	// TODO: check again if we're doing everything correctly https://bugzilla.mozilla.org/show_bug.cgi?id=897322
	JS_FireOnNewGlobalObject(m_cx, globalRootedVal);
	m_comp = JS_EnterCompartment(m_cx, globalRootedVal);
	ok = JS_InitStandardClasses(m_cx, globalRootedVal);
	ENSURE(ok);
	m_glob = globalRootedVal.get();


	JS_DefineProperty(m_cx, m_glob, "global", JS::ObjectValue(*m_glob), NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY
			| JSPROP_PERMANENT);

	m_nativeScope = JS_DefineObject(m_cx, m_glob, nativeScopeName, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY
			| JSPROP_PERMANENT);

	JS_DefineFunction(m_cx, globalRootedVal, "print", ::print,        0, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineFunction(m_cx, globalRootedVal, "log",   ::logmsg,       1, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineFunction(m_cx, globalRootedVal, "warn",  ::warn,         1, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineFunction(m_cx, globalRootedVal, "error", ::error,        1, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
	JS_DefineFunction(m_cx, globalRootedVal, "deepcopy", ::deepcopy,  1, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);

	Register("ProfileStart", ::ProfileStart, 1);
	Register("ProfileStop", ::ProfileStop, 0);

	runtime->RegisterContext(m_cx);
}

ScriptInterface_impl::~ScriptInterface_impl()
{
	// Important: this must come before JS_DestroyContext because CScriptValRooted needs the context to unroot the values!
	m_ScriptValCache.clear();

	m_runtime->UnRegisterContext(m_cx);
	{
		JSAutoRequest rq(m_cx);
		JS_LeaveCompartment(m_cx, m_comp);
	}
	JS_DestroyContext(m_cx);
}

void ScriptInterface_impl::Register(const char* name, JSNative fptr, uint nargs)
{
	JSAutoRequest rq(m_cx);
	JS::RootedObject nativeScope(m_cx, m_nativeScope);
	JSFunction* func = JS_DefineFunction(m_cx, nativeScope, name, fptr, nargs, JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);

	if (!func)
		return;

	if (g_ScriptProfilingEnabled)
	{
		// Store the function name in a slot, so we can pass it to the profiler.

		// Use a flyweight std::string because we can't assume the caller has
		// a correctly-aligned string and that its lifetime is long enough
		typedef boost::flyweight<
			std::string,
			boost::flyweights::no_tracking
			// can't use no_locking; Register might be called in threads
		> LockedStringFlyweight;

		LockedStringFlyweight fw(name);
		JS_SetReservedSlot(JS_GetFunctionObject(func), 0, PRIVATE_TO_JSVAL((void*)fw.get().c_str()));
	}
}

ScriptInterface::ScriptInterface(const char* nativeScopeName, const char* debugName, const shared_ptr<ScriptRuntime>& runtime) :
	m(new ScriptInterface_impl(nativeScopeName, runtime))
{
	// Profiler stats table isn't thread-safe, so only enable this on the main thread
	if (ThreadUtil::IsMainThread())
	{
		if (g_ScriptStatsTable)
			g_ScriptStatsTable->Add(this, debugName);
	}
	
	// JS debugger temporarily disabled during the SpiderMonkey upgrade (check trac ticket #2348 for details)
	/*
	if (g_JSDebuggerEnabled && g_DebuggingServer != NULL)
	{
		if(!JS_SetDebugMode(GetContext(), true))
			LOGERROR(L"Failed to set Spidermonkey to debug mode!");
		else
			g_DebuggingServer->RegisterScriptinterface(debugName, this);
	} */

	m_CxPrivate.pScriptInterface = this;
	JS_SetContextPrivate(m->m_cx, (void*)&m_CxPrivate);
}

ScriptInterface::~ScriptInterface()
{
	if (ThreadUtil::IsMainThread())
	{
		if (g_ScriptStatsTable)
			g_ScriptStatsTable->Remove(this);
	}
	
	// Unregister from the Debugger class
	// JS debugger temporarily disabled during the SpiderMonkey upgrade (check trac ticket #2348 for details)
	//if (g_JSDebuggerEnabled && g_DebuggingServer != NULL)
	//	g_DebuggingServer->UnRegisterScriptinterface(this);
}

void ScriptInterface::ShutDown()
{
	JS_ShutDown();
}

void ScriptInterface::SetCallbackData(void* pCBData)
{
	m_CxPrivate.pCBData = pCBData;
}

ScriptInterface::CxPrivate* ScriptInterface::GetScriptInterfaceAndCBData(JSContext* cx)
{
	CxPrivate* pCxPrivate = (CxPrivate*)JS_GetContextPrivate(cx);
	return pCxPrivate;
}

CScriptValRooted ScriptInterface::GetCachedValue(CACHED_VAL valueIdentifier)
{
	return m->m_ScriptValCache[valueIdentifier];
}


bool ScriptInterface::LoadGlobalScripts()
{
	// Ignore this failure in tests
	if (!g_VFS)
		return false;

	// Load and execute *.js in the global scripts directory
	VfsPaths pathnames;
	vfs::GetPathnames(g_VFS, L"globalscripts/", L"*.js", pathnames);
	for (VfsPaths::iterator it = pathnames.begin(); it != pathnames.end(); ++it)
	{
		if (!LoadGlobalScriptFile(*it))
		{
			LOGERROR(L"LoadGlobalScripts: Failed to load script %ls", it->string().c_str());
			return false;
		}
	}

	JSAutoRequest rq(m->m_cx);
	JS::RootedValue proto(m->m_cx);
	JS::RootedObject global(m->m_cx, m->m_glob);
	if (JS_GetProperty(m->m_cx, global, "Vector2Dprototype", &proto))
		m->m_ScriptValCache[CACHE_VECTOR2DPROTO] = CScriptValRooted(m->m_cx, proto);
	if (JS_GetProperty(m->m_cx, global, "Vector3Dprototype", &proto))
		m->m_ScriptValCache[CACHE_VECTOR3DPROTO] = CScriptValRooted(m->m_cx, proto);
	return true;
}

bool ScriptInterface::ReplaceNondeterministicRNG(boost::rand48& rng)
{
	JSAutoRequest rq(m->m_cx);
	JS::RootedValue math(m->m_cx);
	JS::RootedObject global(m->m_cx, m->m_glob);
	if (JS_GetProperty(m->m_cx, global, "Math", &math) && math.isObject())
	{
		JS::RootedObject mathObj(m->m_cx, &math.toObject());
		JSFunction* random = JS_DefineFunction(m->m_cx, mathObj, "random", Math_random, 0,
			JSPROP_ENUMERATE | JSPROP_READONLY | JSPROP_PERMANENT);
		if (random)
		{
			m->m_rng = &rng;
			return true;
		}
	}

	LOGERROR(L"ReplaceNondeterministicRNG: failed to replace Math.random");
	return false;
}

void ScriptInterface::Register(const char* name, JSNative fptr, size_t nargs)
{
	m->Register(name, fptr, (uint)nargs);
}

JSContext* ScriptInterface::GetContext() const
{
	return m->m_cx;
}

JSRuntime* ScriptInterface::GetJSRuntime() const
{
	return m->m_runtime->m_rt;
}

shared_ptr<ScriptRuntime> ScriptInterface::GetRuntime() const
{
	return m->m_runtime;
}

AutoGCRooter* ScriptInterface::ReplaceAutoGCRooter(AutoGCRooter* rooter)
{
	AutoGCRooter* ret = m->m_runtime->m_rooter;
	m->m_runtime->m_rooter = rooter;
	return ret;
}

jsval ScriptInterface::CallConstructor(jsval ctor, JS::HandleValueArray argv)
{
	JSAutoRequest rq(m->m_cx);
	if (!ctor.isObject())
	{
		LOGERROR(L"CallConstructor: ctor is not an object");
		return JS::UndefinedValue();
	}

	return JS::ObjectValue(*JS_New(m->m_cx, &ctor.toObject(), argv));
}

jsval ScriptInterface::NewObjectFromConstructor(jsval ctor)
{
	JSAutoRequest rq(m->m_cx);
	
	if (!ctor.isObject())
	{
		LOGERROR(L"NewObjectFromConstructor: ctor is not an object");
		return JS::UndefinedValue();
	}
	// Get the constructor's prototype
	// (Can't use JS_GetPrototype, since we want .prototype not .__proto__)
	JS::RootedValue protoVal(m->m_cx);
	JS::RootedObject ctorObj(m->m_cx, &ctor.toObject());
	if (!JS_GetProperty(m->m_cx, ctorObj, "prototype", &protoVal))
	{
		LOGERROR(L"NewObjectFromConstructor: can't get prototype");
		return JS::UndefinedValue();
	}

	if (!protoVal.isObject())
	{
		LOGERROR(L"NewObjectFromConstructor: prototype is not an object");
		return JS::UndefinedValue();
	}

	JS::RootedObject proto(m->m_cx, &protoVal.toObject());
	JS::RootedObject parent(m->m_cx, JS_GetParent(ctorObj));

	if (!proto || !parent)
	{
		LOGERROR(L"NewObjectFromConstructor: null proto/parent");
		return JS::UndefinedValue();
	}

	JSObject* obj = JS_NewObject(m->m_cx, NULL, proto, parent);
	if (!obj)
	{
		LOGERROR(L"NewObjectFromConstructor: object creation failed");
		return JS::UndefinedValue();
	}

	return JS::ObjectValue(*obj);
}

void ScriptInterface::DefineCustomObjectType(JSClass *clasp, JSNative constructor, uint minArgs, JSPropertySpec *ps, JSFunctionSpec *fs, JSPropertySpec *static_ps, JSFunctionSpec *static_fs)
{
	JSAutoRequest rq(m->m_cx);
	std::string typeName = clasp->name;

	if (m_CustomObjectTypes.find(typeName) != m_CustomObjectTypes.end())
	{
		// This type already exists
		throw PSERROR_Scripting_DefineType_AlreadyExists();
	}

	JS::RootedObject global(m->m_cx, m->m_glob);
	JSObject * obj = JS_InitClass(	m->m_cx, global, JS::NullPtr(),
									clasp,
									constructor, minArgs,				// Constructor, min args
									ps, fs,								// Properties, methods
									static_ps, static_fs);				// Constructor properties, methods

	if (obj == NULL)
		throw PSERROR_Scripting_DefineType_CreationFailed();

	CustomType type;

	type.m_Prototype = obj;
	type.m_Class = clasp;
	type.m_Constructor = constructor;

	m_CustomObjectTypes[typeName] = type;
}

JSObject* ScriptInterface::CreateCustomObject(const std::string & typeName)
{
	std::map < std::string, CustomType > ::iterator it = m_CustomObjectTypes.find(typeName);

	if (it == m_CustomObjectTypes.end())
		throw PSERROR_Scripting_TypeDoesNotExist();

	JS::RootedObject prototype(m->m_cx, (*it).second.m_Prototype);
	return JS_NewObject(m->m_cx, (*it).second.m_Class, prototype, JS::NullPtr());
}


bool ScriptInterface::CallFunctionVoid(jsval val, const char* name)
{
	JSAutoRequest rq(m->m_cx);
	JS::RootedValue jsRet(m->m_cx);
	JS::RootedValue val1(m->m_cx, val);
	return CallFunction_(val1, name, JS::HandleValueArray::empty(), &jsRet);
}

bool ScriptInterface::CallFunction_(JS::HandleValue val, const char* name, JS::HandleValueArray argv, JS::MutableHandleValue ret)
{
	JSAutoRequest rq(m->m_cx);
	JS::RootedObject obj(m->m_cx);
	if (!JS_ValueToObject(m->m_cx, val, &obj) || !obj)
		return false;
	
	// Check that the named function actually exists, to avoid ugly JS error reports
	// when calling an undefined value
	bool found;
	if (!JS_HasProperty(m->m_cx, obj, name, &found) || !found)
		return false;

	bool ok = JS_CallFunctionName(m->m_cx, obj, name, argv, ret);

	return ok;
}

jsval ScriptInterface::GetGlobalObject()
{
	JSAutoRequest rq(m->m_cx);
	return JS::ObjectValue(*JS::CurrentGlobalOrNull(m->m_cx));
}

JSClass* ScriptInterface::GetGlobalClass()
{
	return &global_class;
}

bool ScriptInterface::SetGlobal_(const char* name, jsval value, bool replace)
{
	JSAutoRequest rq(m->m_cx);
	JS::RootedObject global(m->m_cx, m->m_glob);
	if (!replace)
	{
		bool found;
		if (!JS_HasProperty(m->m_cx, global, name, &found))
			return false;
		if (found)
		{
			JS_ReportError(m->m_cx, "SetGlobal \"%s\" called multiple times", name);
			return false;
		}
	}

	bool ok = JS_DefineProperty(m->m_cx, global, name, value, NULL, NULL, JSPROP_ENUMERATE | JSPROP_READONLY
 			| JSPROP_PERMANENT);
	return ok;
}

bool ScriptInterface::GetPropertyJS(jsval obj, const char* name, JS::MutableHandleValue out)
{
	JSContext* cx = GetContext();
	JSAutoRequest rq(cx);
	if (! GetProperty_(obj, name, out))
		return false;
	return true;
}

bool ScriptInterface::SetProperty_(jsval obj, const char* name, jsval value, bool constant, bool enumerate)
{
	JSAutoRequest rq(m->m_cx);
	uint attrs = 0;
	if (constant)
		attrs |= JSPROP_READONLY | JSPROP_PERMANENT;
	if (enumerate)
		attrs |= JSPROP_ENUMERATE;

	if (!obj.isObject())
		return false;
	JS::RootedObject object(m->m_cx, &obj.toObject());

	if (! JS_DefineProperty(m->m_cx, object, name, value, NULL, NULL, attrs))
		return false;
	return true;
}

bool ScriptInterface::SetProperty_(jsval obj, const wchar_t* name, jsval value, bool constant, bool enumerate)
{
	JSAutoRequest rq(m->m_cx);
	uint attrs = 0;
	if (constant)
		attrs |= JSPROP_READONLY | JSPROP_PERMANENT;
	if (enumerate)
		attrs |= JSPROP_ENUMERATE;

	if (!obj.isObject())
		return false;
	JS::RootedObject object(m->m_cx, &obj.toObject());

	utf16string name16(name, name + wcslen(name));
	if (! JS_DefineUCProperty(m->m_cx, object, reinterpret_cast<const jschar*>(name16.c_str()), name16.length(), value, NULL, NULL, attrs))
		return false;
	return true;
}

bool ScriptInterface::SetPropertyInt_(jsval obj, int name, jsval value, bool constant, bool enumerate)
{
	JSAutoRequest rq(m->m_cx);
	uint attrs = 0;
	if (constant)
		attrs |= JSPROP_READONLY | JSPROP_PERMANENT;
	if (enumerate)
		attrs |= JSPROP_ENUMERATE;

	if (!obj.isObject())
		return false;
	JS::RootedObject object(m->m_cx, &obj.toObject());

	if (! JS_DefinePropertyById(m->m_cx, object, INT_TO_JSID(name), value, NULL, NULL, attrs))
		return false;
	return true;
}

bool ScriptInterface::GetProperty_(jsval obj, const char* name, JS::MutableHandleValue out)
{
	JSAutoRequest rq(m->m_cx);
	if (!obj.isObject())
		return false;
	JS::RootedObject object(m->m_cx, &obj.toObject());
	
	if (!JS_GetProperty(m->m_cx, object, name, out))
		return false;
	return true;
}

bool ScriptInterface::GetPropertyInt_(jsval obj, int name, JS::MutableHandleValue out)
{
	JSAutoRequest rq(m->m_cx);
	JS::RootedId nameId(m->m_cx, INT_TO_JSID(name));
	if (!obj.isObject())
		return false;
	JS::RootedObject object(m->m_cx, &obj.toObject());
	
	if (!JS_GetPropertyById(m->m_cx, object, nameId, out))
		return false;
	return true;
}

bool ScriptInterface::HasProperty(jsval obj, const char* name)
{
	// TODO: proper errorhandling 
	JSAutoRequest rq(m->m_cx);
	if (!obj.isObject())
		return false;
	JS::RootedObject object(m->m_cx, &obj.toObject());

	bool found;
	if (!JS_HasProperty(m->m_cx, object, name, &found))
		return false;
	return found;
}

bool ScriptInterface::EnumeratePropertyNamesWithPrefix(JS::HandleValue objVal, const char* prefix, std::vector<std::string>& out)
{
	JSAutoRequest rq(m->m_cx);
	
	if (!objVal.isObjectOrNull())
	{
		LOGERROR(L"EnumeratePropertyNamesWithPrefix expected object type!");
		return false;
	}
		
	if(objVal.isNull())
		return true; // reached the end of the prototype chain
	
	JS::RootedObject obj(m->m_cx, &objVal.toObject());
	JS::RootedObject it(m->m_cx, JS_NewPropertyIterator(m->m_cx, obj));
	if (!it)
		return false;

	while (true)
	{
		JS::RootedId idp(m->m_cx);
		JS::RootedValue val(m->m_cx);
		if (! JS_NextProperty(m->m_cx, it, idp.address()) || ! JS_IdToValue(m->m_cx, idp, &val))
			return false;

		if (val.isUndefined())
			break; // end of iteration
		if (!val.isString())
			continue; // ignore integer properties

		JS::RootedString name(m->m_cx, val.toString());
		size_t len = strlen(prefix)+1;
		std::vector<char> buf(len);
		size_t prefixLen = strlen(prefix) * sizeof(char);
		JS_EncodeStringToBuffer(m->m_cx, name, &buf[0], prefixLen);
		buf[len-1]= '\0';
		if(0 == strcmp(&buf[0], prefix))
		{
			size_t len;
			const jschar* chars = JS_GetStringCharsAndLength(m->m_cx, name, &len);
			out.push_back(std::string(chars, chars+len));
		}
	}

	// Recurse up the prototype chain
	JS::RootedObject prototype(m->m_cx);
	if (JS_GetPrototype(m->m_cx, obj, &prototype))
	{
		JS::RootedValue prototypeVal(m->m_cx, JS::ObjectOrNullValue(prototype));
		if (! EnumeratePropertyNamesWithPrefix(prototypeVal, prefix, out))
			return false;
	}

	return true;
}

bool ScriptInterface::SetPrototype(JS::HandleValue objVal, JS::HandleValue protoVal)
{
	JSAutoRequest rq(m->m_cx);
	if (!objVal.isObject() || !protoVal.isObject())
		return false;
	JS::RootedObject obj(m->m_cx, &objVal.toObject());
	JS::RootedObject proto(m->m_cx, &protoVal.toObject());
	return JS_SetPrototype(m->m_cx, obj, proto);
}

bool ScriptInterface::FreezeObject(jsval objVal, bool deep)
{
	JSAutoRequest rq(m->m_cx);
	if (!objVal.isObject())
		return false;
	JS::RootedObject obj(m->m_cx, &objVal.toObject());

	if (deep)
		return JS_DeepFreezeObject(m->m_cx, obj);
	else
		return JS_FreezeObject(m->m_cx, obj);
}

bool ScriptInterface::LoadScript(const VfsPath& filename, const std::string& code)
{
	JSAutoRequest rq(m->m_cx);
	JS::RootedObject global(m->m_cx, m->m_glob);
	utf16string codeUtf16(code.begin(), code.end());
	
	uint lineNo = 1;

	JS::CompileOptions options(m->m_cx);
	//options.setFileAndLine(utf8_from_wstring(filename.string()).c_str(), lineNo);
	options.setCompileAndGo(true);

	JS::RootedFunction func(m->m_cx,
		JS_CompileUCFunction(m->m_cx, global, utf8_from_wstring(filename.string()).c_str(), 0, NULL,
			reinterpret_cast<const jschar*> (codeUtf16.c_str()), (uint)(codeUtf16.length()), options)
	);
	if (!func)
		return false;

	JS::RootedValue rval(m->m_cx);
	bool ok = JS_CallFunction(m->m_cx, JS::NullPtr(), func, JS::HandleValueArray::empty(), &rval);

	return ok;
}

bool ScriptInterface::LoadGlobalScript(const VfsPath& filename, const std::wstring& code)
{
	JSAutoRequest rq(m->m_cx);
	JS::RootedObject global(m->m_cx, m->m_glob);
	utf16string codeUtf16(code.begin(), code.end());
	uint lineNo = 1;

	JS::RootedValue rval(m->m_cx);
	bool ok = JS_EvaluateUCScript(m->m_cx, global,
			reinterpret_cast<const jschar*> (codeUtf16.c_str()), (uint)(codeUtf16.length()),
			utf8_from_wstring(filename.string()).c_str(), lineNo, &rval);

	return ok;
}

bool ScriptInterface::LoadGlobalScriptFile(const VfsPath& path)
{
	JSAutoRequest rq(m->m_cx);
	JS::RootedObject global(m->m_cx, m->m_glob);
	if (!VfsFileExists(path))
	{
		LOGERROR(L"File '%ls' does not exist", path.string().c_str());
		return false;
	}

	CVFSFile file;

	PSRETURN ret = file.Load(g_VFS, path);

	if (ret != PSRETURN_OK)
	{
		LOGERROR(L"Failed to load file '%ls': %hs", path.string().c_str(), GetErrorString(ret));
		return false;
	}

	std::wstring code = wstring_from_utf8(file.DecodeUTF8()); // assume it's UTF-8

	utf16string codeUtf16(code.begin(), code.end());
	uint lineNo = 1;

	JS::RootedValue rval(m->m_cx);
	bool ok = JS_EvaluateUCScript(m->m_cx, global,
			reinterpret_cast<const jschar*> (codeUtf16.c_str()), (uint)(codeUtf16.length()),
			utf8_from_wstring(path.string()).c_str(), lineNo, &rval);

	return ok;
}

bool ScriptInterface::Eval(const char* code)
{
	jsval rval;
	return Eval_(code, rval);
}

bool ScriptInterface::Eval_(const char* code, jsval& rval)
{
	JSAutoRequest rq(m->m_cx);
	JS::RootedObject global(m->m_cx, m->m_glob);
	utf16string codeUtf16(code, code+strlen(code));

	JS::RootedValue rval1(m->m_cx);
	bool ok = JS_EvaluateUCScript(m->m_cx, global, (const jschar*)codeUtf16.c_str(), (uint)codeUtf16.length(), "(eval)", 1, &rval1);
	rval = rval1;
	return ok;
}

bool ScriptInterface::Eval_(const wchar_t* code, jsval& rval)
{
	JSAutoRequest rq(m->m_cx);
	JS::RootedObject global(m->m_cx, m->m_glob);
	utf16string codeUtf16(code, code+wcslen(code));

	JS::RootedValue rval1(m->m_cx);
	bool ok = JS_EvaluateUCScript(m->m_cx, global, (const jschar*)codeUtf16.c_str(), (uint)codeUtf16.length(), "(eval)", 1, &rval1);
	rval = rval1;
	return ok;
}

CScriptValRooted ScriptInterface::ParseJSON(const std::string& string_utf8)
{
	JSAutoRequest rq(m->m_cx);
	std::wstring attrsW = wstring_from_utf8(string_utf8);
 	utf16string string(attrsW.begin(), attrsW.end());
	JS::RootedValue vp(m->m_cx);
	if (!JS_ParseJSON(m->m_cx, reinterpret_cast<const jschar*>(string.c_str()), (u32)string.size(), &vp))
		LOGERROR(L"JS_ParseJSON failed!");
	return CScriptValRooted(m->m_cx, vp);
}

CScriptValRooted ScriptInterface::ReadJSONFile(const VfsPath& path)
{
	if (!VfsFileExists(path))
	{
		LOGERROR(L"File '%ls' does not exist", path.string().c_str());
		return CScriptValRooted();
	}

	CVFSFile file;

	PSRETURN ret = file.Load(g_VFS, path);

	if (ret != PSRETURN_OK)
	{
		LOGERROR(L"Failed to load file '%ls': %hs", path.string().c_str(), GetErrorString(ret));
		return CScriptValRooted();
	}

	std::string content(file.DecodeUTF8()); // assume it's UTF-8

	return ParseJSON(content);
}

struct Stringifier
{
	static bool callback(const jschar* buf, u32 len, void* data)
	{
		utf16string str(buf, buf+len);
		std::wstring strw(str.begin(), str.end());

		Status err; // ignore Unicode errors
		static_cast<Stringifier*>(data)->stream << utf8_from_wstring(strw, &err);
		return true;
	}

	std::stringstream stream;
};

struct StringifierW
{
	static bool callback(const jschar* buf, u32 len, void* data)
	{
		utf16string str(buf, buf+len);
		static_cast<StringifierW*>(data)->stream << std::wstring(str.begin(), str.end());
		return true;
	}

	std::wstringstream stream;
};

std::string ScriptInterface::StringifyJSON(jsval obj, bool indent)
{
	JSAutoRequest rq(m->m_cx);
	Stringifier str;
	JS::RootedValue obj1(m->m_cx, obj);
	JS::RootedValue indentVal(m->m_cx, indent ? JS::Int32Value(2) : JS::UndefinedValue());
	
	if (!JS_Stringify(m->m_cx, &obj1, JS::NullPtr(), indentVal, &Stringifier::callback, &str))
	{
		JS_ClearPendingException(m->m_cx);
		LOGERROR(L"StringifyJSON failed");
		JS_ClearPendingException(m->m_cx);
		return std::string();
	}

	return str.stream.str();
}


std::wstring ScriptInterface::ToString(jsval obj, bool pretty)
{
	JSAutoRequest rq(m->m_cx);
	if (JSVAL_IS_VOID(obj))
		return L"(void 0)";

	// Try to stringify as JSON if possible
	// (TODO: this is maybe a bad idea since it'll drop 'undefined' values silently)
	if (pretty)
	{
		StringifierW str;
		JS::RootedValue obj1(m->m_cx, obj);
		JS::RootedValue indentVal(m->m_cx, JS::Int32Value(2));

		// Temporary disable the error reporter, so we don't print complaints about cyclic values
		JSErrorReporter er = JS_SetErrorReporter(m->m_cx, NULL);

		bool ok = JS_Stringify(m->m_cx, &obj1, JS::NullPtr(), indentVal, &StringifierW::callback, &str);

		// Restore error reporter
		JS_SetErrorReporter(m->m_cx, er);

		if (ok)
			return str.stream.str();

		// Clear the exception set when Stringify failed
		JS_ClearPendingException(m->m_cx);
	}

	// Caller didn't want pretty output, or JSON conversion failed (e.g. due to cycles),
	// so fall back to obj.toSource()

	std::wstring source = L"(error)";
	CallFunction(obj, "toSource", source);
	return source;
}

void ScriptInterface::ReportError(const char* msg)
{
	JSAutoRequest rq(m->m_cx);
	// JS_ReportError by itself doesn't seem to set a JS-style exception, and so
	// script callers will be unable to catch anything. So use JS_SetPendingException
	// to make sure there really is a script-level exception. But just set it to undefined
	// because there's not much value yet in throwing a real exception object.
	JS_SetPendingException(m->m_cx, JS::UndefinedHandleValue);
	// And report the actual error
	JS_ReportError(m->m_cx, "%s", msg);

	// TODO: Why doesn't JS_ReportPendingException(m->m_cx); work?
}

bool ScriptInterface::IsExceptionPending(JSContext* cx)
{
	JSAutoRequest rq(cx);
	return JS_IsExceptionPending(cx) ? true : false;
}

const JSClass* ScriptInterface::GetClass(JSObject* obj)
{
	return JS_GetClass(obj);
}

void* ScriptInterface::GetPrivate(JSObject* obj)
{
	// TODO: use JS_GetInstancePrivate
	return JS_GetPrivate(obj);
}

void ScriptInterface::DumpHeap()
{
#if MOZJS_DEBUG_ABI
	JS_DumpHeap(GetJSRuntime(), stderr, NULL, JSTRACE_OBJECT, NULL, (size_t)-1, NULL);
#endif
	fprintf(stderr, "# Bytes allocated: %u\n", JS_GetGCParameter(GetJSRuntime(), JSGC_BYTES));
	JS_GC(GetJSRuntime());
	fprintf(stderr, "# Bytes allocated after GC: %u\n", JS_GetGCParameter(GetJSRuntime(), JSGC_BYTES));
}

void ScriptInterface::MaybeIncrementalRuntimeGC()
{
	m->m_runtime->MaybeIncrementalGC();
}

void ScriptInterface::MaybeGC()
{
	JS_MaybeGC(m->m_cx);
}

void ScriptInterface::ForceGC()
{
	PROFILE2("JS_GC");
	JS_GC(this->GetJSRuntime());
}

jsval ScriptInterface::CloneValueFromOtherContext(ScriptInterface& otherContext, jsval val)
{
	PROFILE("CloneValueFromOtherContext");
	shared_ptr<StructuredClone> structuredClone = otherContext.WriteStructuredClone(val);
	JSAutoRequest rq(otherContext.GetContext());
	JS::RootedValue clone(otherContext.GetContext());
	ReadStructuredClone(structuredClone, &clone); 
	return clone.get();
}

ScriptInterface::StructuredClone::StructuredClone() :
	m_Data(NULL), m_Size(0)
{
}

ScriptInterface::StructuredClone::~StructuredClone()
{
	if (m_Data)
		JS_ClearStructuredClone(m_Data, m_Size);
}

shared_ptr<ScriptInterface::StructuredClone> ScriptInterface::WriteStructuredClone(jsval v)
{
	JSAutoRequest rq(m->m_cx);
	JS::RootedValue v1(m->m_cx, v);
	u64* data = NULL;
	size_t nbytes = 0;
	if (!JS_WriteStructuredClone(m->m_cx, v1, &data, &nbytes, NULL, NULL, JS::UndefinedHandleValue))
	{
		debug_warn(L"Writing a structured clone with JS_WriteStructuredClone failed!");
		return shared_ptr<StructuredClone>();
	}

	shared_ptr<StructuredClone> ret (new StructuredClone);
	ret->m_Data = data;
	ret->m_Size = nbytes;
	return ret;
}

void ScriptInterface::ReadStructuredClone(const shared_ptr<ScriptInterface::StructuredClone>& ptr, JS::MutableHandleValue ret)
{
	JSAutoRequest rq(m->m_cx);
	JS_ReadStructuredClone(m->m_cx, ptr->m_Data, ptr->m_Size, JS_STRUCTURED_CLONE_VERSION, ret, NULL, NULL);
}
