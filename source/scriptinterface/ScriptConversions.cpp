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

#include "graphics/Entity.h"
#include "ps/utf16string.h"
#include "ps/CLogger.h"
#include "ps/CStr.h"
#include "scriptinterface/ScriptExtraHeaders.h" // for typed arrays

#define FAIL(msg) STMT(JS_ReportError(cx, msg); return false)

// Implicit type conversions often hide bugs, so warn about them
#define WARN_IF_NOT(c, v) STMT(if (!(c)) { JS_ReportWarning(cx, "Script value conversion check failed: %s (got type %s)", #c, JS_GetTypeName(cx, JS_TypeOfValue(cx, v))); })

template<> bool ScriptInterface::FromJSVal<bool>(JSContext* cx, jsval v1, bool& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	WARN_IF_NOT(v.isBoolean(), v);
	out = JS::ToBoolean(v);
	return true;
}

template<> bool ScriptInterface::FromJSVal<float>(JSContext* cx, jsval v1, float& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	double ret;
	WARN_IF_NOT(v.isNumber(), v);
	if (!JS::ToNumber(cx, v, &ret))
		return false;
	out = ret;
	return true;
}

template<> bool ScriptInterface::FromJSVal<double>(JSContext* cx, jsval v1, double& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	double ret;
	WARN_IF_NOT(v.isNumber(), v);
	if (!JS::ToNumber(cx, v, &ret))
		return false;
	out = ret;
	return true;
}

template<> bool ScriptInterface::FromJSVal<i32>(JSContext* cx, jsval v1, i32& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	int32_t ret;
	WARN_IF_NOT(v.isNumber(), v);
	if (!JS::ToInt32(cx, v, &ret))
		return false;
	out = ret;
	return true;
}

template<> bool ScriptInterface::FromJSVal<u32>(JSContext* cx, jsval v1, u32& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	uint32_t ret;
	WARN_IF_NOT(v.isNumber(), v);
	if (!JS::ToUint32(cx, v, &ret))
		return false;
	out = ret;
	return true;
}

template<> bool ScriptInterface::FromJSVal<u16>(JSContext* cx, jsval v1, u16& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	uint16_t ret;
	WARN_IF_NOT(v.isNumber(), v);
	if (!JS::ToUint16(cx, v, &ret))
		return false;
	out = ret;
	return true;
}

template<> bool ScriptInterface::FromJSVal<u8>(JSContext* cx, jsval v1, u8& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	uint16_t ret;
	WARN_IF_NOT(v.isNumber(), v);
	if (!JS::ToUint16(cx, v, &ret))
		return false;
	out = (u8)ret;
	return true;
}

template<> bool ScriptInterface::FromJSVal<long>(JSContext* cx, jsval v1, long& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	int32_t tmp;
	bool ok = JS::ToInt32(cx, v, &tmp);
	out = (long)tmp;
	return ok;
}

template<> bool ScriptInterface::FromJSVal<unsigned long>(JSContext* cx, jsval v1, unsigned long& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	int32_t tmp;
	bool ok = JS::ToInt32(cx, v, &tmp);
	out = (unsigned long)tmp;
	return ok;
}

// see comment below (where the same preprocessor condition is used)
#if MSC_VERSION && ARCH_AMD64

template<> bool ScriptInterface::FromJSVal<size_t>(JSContext* cx, jsval v1, size_t& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	int temp;
	if(!FromJSVal<int>(cx, v, temp))
		return false;
	if(temp < 0)
		return false;
	out = (size_t)temp;
	return true;
}

template<> bool ScriptInterface::FromJSVal<ssize_t>(JSContext* cx, jsval v1, ssize_t& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	int temp;
	if(!FromJSVal<int>(cx, v, temp))
		return false;
	if(temp < 0)
		return false;
	out = (ssize_t)temp;
	return true;
}

#endif


template<> bool ScriptInterface::FromJSVal<CScriptVal>(JSContext* UNUSED(cx), jsval v, CScriptVal& out)
{
	out = v;
	return true;
}

template<> bool ScriptInterface::FromJSVal<CScriptValRooted>(JSContext* cx, jsval v, CScriptValRooted& out)
{
	out = CScriptValRooted(cx, v);
	return true;
}

template<> bool ScriptInterface::FromJSVal<std::wstring>(JSContext* cx, jsval v1, std::wstring& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	WARN_IF_NOT(JSVAL_IS_STRING(v) || JSVAL_IS_NUMBER(v), v); // allow implicit number conversions
	JSString* ret = JS::ToString(cx, v);
	if (!ret)
		FAIL("Argument must be convertible to a string");
	size_t length;
	const jschar* ch = JS_GetStringCharsAndLength(cx, ret, &length);
	if (!ch)
		FAIL("JS_GetStringsCharsAndLength failed"); // out of memory
	out = std::wstring(ch, ch + length);
	return true;
}

template<> bool ScriptInterface::FromJSVal<Path>(JSContext* cx, jsval v, Path& out)
{
	std::wstring string;
	if (!FromJSVal(cx, v, string))
		return false;
	out = string;
	return true;
}

template<> bool ScriptInterface::FromJSVal<std::string>(JSContext* cx, jsval v1, std::string& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	WARN_IF_NOT(v.isString() || v.isNumber(), v); // allow implicit number conversions
	JSString* ret = JS::ToString(cx, v);
	if (!ret)
		FAIL("Argument must be convertible to a string");
	char* ch = JS_EncodeString(cx, ret); // chops off high byte of each jschar
	if (!ch)
		FAIL("JS_EncodeString failed"); // out of memory
	out = std::string(ch, ch + JS_GetStringLength(ret));
	JS_free(cx, ch);
	return true;
}

template<> bool ScriptInterface::FromJSVal<CStr8>(JSContext* cx, jsval v, CStr8& out)
{
	return ScriptInterface::FromJSVal(cx, v, static_cast<std::string&>(out));
}

template<> bool ScriptInterface::FromJSVal<CStrW>(JSContext* cx, jsval v, CStrW& out)
{
	return ScriptInterface::FromJSVal(cx, v, static_cast<std::wstring&>(out));
}

template<> bool ScriptInterface::FromJSVal<Entity>(JSContext* cx, jsval v1, Entity& out)
{
	JSAutoRequest rq(cx);
	JS::RootedValue v(cx, v1);
	if (!v.isObject())
		FAIL("Argument must be an object");

	JS::RootedObject obj(cx, &v.toObject());
	JS::RootedValue templateName(cx);
	JS::RootedValue id(cx);
	JS::RootedValue player(cx);
	JS::RootedValue position(cx);
	JS::RootedValue rotation(cx);

	// TODO: Report type errors
	if(!JS_GetProperty(cx, obj, "player", &player) || !FromJSVal(cx, player, out.playerID))
		FAIL("Failed to read Entity.player property");
	if (!JS_GetProperty(cx, obj, "templateName", &templateName) || !FromJSVal(cx, templateName, out.templateName))
		FAIL("Failed to read Entity.templateName property");
	if (!JS_GetProperty(cx, obj, "id", &id) || !FromJSVal(cx, id, out.entityID))
		FAIL("Failed to read Entity.id property");
	if (!JS_GetProperty(cx, obj, "position", &position) || !FromJSVal(cx, position, out.position))
		FAIL("Failed to read Entity.position property");
	if (!JS_GetProperty(cx, obj, "rotation", &rotation) || !FromJSVal(cx, rotation, out.rotation))
		FAIL("Failed to read Entity.rotation property");

	return true;
}

////////////////////////////////////////////////////////////////
// Primitive types:

template<> void ScriptInterface::ToJSVal<bool>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const bool& val)
{
	ret.set(JS::BooleanValue(val));
}

template<> void ScriptInterface::ToJSVal<float>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const float& val)
{
	ret.set(JS::NumberValue(val));
}

template<> void ScriptInterface::ToJSVal<double>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const double& val)
{
	ret.set(JS::NumberValue(val));
}

template<> void ScriptInterface::ToJSVal<i32>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const i32& val)
{
	ret.set(JS::NumberValue(val));
}

template<> void ScriptInterface::ToJSVal<u16>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const u16& val)
{
	ret.set(JS::NumberValue(val));
}

template<> void ScriptInterface::ToJSVal<u8>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const u8& val)
{
	ret.set(JS::NumberValue(val));
}

template<> void ScriptInterface::ToJSVal<u32>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const u32& val)
{
	ret.set(JS::NumberValue(val));
}

template<> void ScriptInterface::ToJSVal<long>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const long& val)
{
	ret.set(JS::NumberValue((int)val));
}

template<> void ScriptInterface::ToJSVal<unsigned long>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const unsigned long& val)
{
	ret.set(JS::NumberValue((int)val));
}

// (s)size_t are considered to be identical to (unsigned) int by GCC and 
// their specializations would cause conflicts there. On x86_64 GCC, s/size_t 
// is equivalent to (unsigned) long, but the same solution applies; use the 
// long and unsigned long specializations instead of s/size_t. 
// for some reason, x64 MSC treats size_t as distinct from unsigned long:
#if MSC_VERSION && ARCH_AMD64

template<> void ScriptInterface::ToJSVal<size_t>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const size_t& val)
{
	ret.set(JS::NumberValue((int)val));
}

template<> void ScriptInterface::ToJSVal<ssize_t>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const ssize_t& val)
{
	ret.set(JS::NumberValue((int)val));
}

#endif

template<> void ScriptInterface::ToJSVal<CScriptVal>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const CScriptVal& val)
{
	ret.set(val.get());
}

template<> void ScriptInterface::ToJSVal<JS::HandleValue>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const JS::HandleValue& val)
{
	ret.set(val);
}


template<> void ScriptInterface::ToJSVal<CScriptValRooted>(JSContext* UNUSED(cx), JS::MutableHandleValue ret, const CScriptValRooted& val)
{
	ret.set(val.get());
}

template<> void ScriptInterface::ToJSVal<std::wstring>(JSContext* cx, JS::MutableHandleValue ret, const std::wstring& val)
{
	JSAutoRequest rq(cx);
	utf16string utf16(val.begin(), val.end());
	JSString* str = JS_NewUCStringCopyN(cx, reinterpret_cast<const jschar*> (utf16.c_str()), utf16.length());
	if (str)
		ret.set(JS::StringValue(str));
	else
		ret.set(JS::UndefinedValue());
}

template<> void ScriptInterface::ToJSVal<Path>(JSContext* cx, JS::MutableHandleValue ret, const Path& val)
{
	ToJSVal(cx, ret, val.string());
}

template<> void ScriptInterface::ToJSVal<std::string>(JSContext* cx, JS::MutableHandleValue ret, const std::string& val)
{
	JSAutoRequest rq(cx);
	JSString* str = JS_NewStringCopyN(cx, val.c_str(), val.length());
	if (str)
		ret.set(JS::StringValue(str));
	else
		ret.set(JS::UndefinedValue());
}

template<> void ScriptInterface::ToJSVal<const wchar_t*>(JSContext* cx, JS::MutableHandleValue ret, const wchar_t* const& val)
{
	ToJSVal(cx, ret, std::wstring(val));
}

template<> void ScriptInterface::ToJSVal<const char*>(JSContext* cx, JS::MutableHandleValue ret, const char* const& val)
{
	JSAutoRequest rq(cx);
	JSString* str = JS_NewStringCopyZ(cx, val);
	if (str)
		ret.set(JS::StringValue(str));
	else
		ret.set(JS::UndefinedValue());
}

template<> void ScriptInterface::ToJSVal<CStrW>(JSContext* cx, JS::MutableHandleValue ret, const CStrW& val)
{
	ToJSVal(cx, ret, static_cast<const std::wstring&>(val));
}

template<> void ScriptInterface::ToJSVal<CStr8>(JSContext* cx, JS::MutableHandleValue ret, const CStr8& val)
{
	ToJSVal(cx, ret, static_cast<const std::string&>(val));
}

////////////////////////////////////////////////////////////////
// Compound types:

template<typename T> static void ToJSVal_vector(JSContext* cx, JS::MutableHandleValue ret, const std::vector<T>& val)
{
	JSAutoRequest rq(cx);
	JS::RootedObject obj(cx, JS_NewArrayObject(cx, 0));
	if (!obj)
	{
		ret.set(JS::UndefinedValue());
		return;
	}
	for (uint32_t i = 0; i < val.size(); ++i)
	{
		JS::RootedValue el(cx);
		ScriptInterface::ToJSVal<T>(cx, &el, val[i]);
		JS_SetElement(cx, obj, i, el);
	}
	ret.set(JS::ObjectValue(*obj));
}

template<typename T> static bool FromJSVal_vector(JSContext* cx, jsval v, std::vector<T>& out)
{
	JSAutoRequest rq(cx);
	JS::RootedObject obj(cx);
	if (!v.isObject())
		FAIL("Argument must be an array");
	obj = &v.toObject();
	if (!(JS_IsArrayObject(cx, obj) || JS_IsTypedArrayObject(obj)))
		FAIL("Argument must be an array");
	
	uint32_t length;
	if (!JS_GetArrayLength(cx, obj, &length))
		FAIL("Failed to get array length");
	out.reserve(length);
	for (uint32_t i = 0; i < length; ++i)
	{
		JS::RootedValue el(cx);
		if (!JS_GetElement(cx, obj, i, &el))
			FAIL("Failed to read array element");
		T el2;
		if (!ScriptInterface::FromJSVal<T>(cx, el, el2))
			return false;
		out.push_back(el2);
	}
	return true;
}

// Instantiate various vector types:

#define VECTOR(T) \
	template<> void ScriptInterface::ToJSVal<std::vector<T> >(JSContext* cx, JS::MutableHandleValue ret, const std::vector<T>& val) \
	{ \
		ToJSVal_vector(cx, ret, val); \
	} \
	template<> bool ScriptInterface::FromJSVal<std::vector<T> >(JSContext* cx, jsval v, std::vector<T>& out) \
	{ \
		return FromJSVal_vector(cx, v, out); \
	}

VECTOR(int)
VECTOR(u32)
VECTOR(u16)
VECTOR(std::string)
VECTOR(std::wstring)
VECTOR(CScriptValRooted)


class IComponent;
template<> void ScriptInterface::ToJSVal<std::vector<IComponent*> >(JSContext* cx, JS::MutableHandleValue ret, const std::vector<IComponent*>& val)
{
	ToJSVal_vector(cx, ret, val);
}

template<> bool ScriptInterface::FromJSVal<std::vector<Entity> >(JSContext* cx, jsval v, std::vector<Entity>& out)
{
	return FromJSVal_vector(cx, v, out);
}
