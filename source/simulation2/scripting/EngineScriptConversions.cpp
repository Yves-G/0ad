/* Copyright (C) 2012 Wildfire Games.
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

#include "scriptinterface/ScriptInterface.h"
#include "scriptinterface/ScriptExtraHeaders.h" // for typed arrays

#include "maths/Fixed.h"
#include "maths/FixedVector2D.h"
#include "maths/FixedVector3D.h"
#include "ps/CLogger.h"
#include "ps/Overlay.h"
#include "ps/utf16string.h"
#include "simulation2/helpers/Grid.h"
#include "simulation2/system/IComponent.h"
#include "simulation2/system/ParamNode.h"

#define FAIL(msg) STMT(JS_ReportError(cx, msg); return false)

template<> void ScriptInterface::ToJSVal<IComponent*>(JSContext* cx, JS::Value& ret, IComponent* const& val)
{
	JSAutoRequest rq(cx);
	if (val == NULL)
	{
		ret = JSVAL_NULL;
		return;
	}

	// If this is a scripted component, just return the JS object directly
	jsval instance = val->GetJSInstance();
	if (!JSVAL_IS_NULL(instance))
	{
		ret = instance;
		return;
	}

	// Otherwise we need to construct a wrapper object
	// (TODO: cache wrapper objects?)
	JSClass* cls = val->GetJSClass();
	if (!cls)
	{
		// Report as an error, since scripts really shouldn't try to use unscriptable interfaces
		LOGERROR(L"IComponent does not have a scriptable interface");
		ret = JSVAL_VOID;
		return;
	}

	JSObject* obj = JS_NewObject(cx, cls, NULL, NULL);
	if (!obj)
	{
		LOGERROR(L"Failed to construct IComponent script object");
		ret = JSVAL_VOID;
		return;
	}
	JS_SetPrivate(obj, static_cast<void*>(val));

	ret = OBJECT_TO_JSVAL(obj);
}

template<> void ScriptInterface::ToJSVal<CParamNode>(JSContext* cx, JS::Value& ret, CParamNode const& val)
{
	JSAutoRequest rq(cx);
	ret = val.ToJSVal(cx, true);

	// Prevent modifications to the object, so that it's safe to share between
	// components and to reconstruct on deserialization
	if (!JSVAL_IS_PRIMITIVE(ret))
		JS_DeepFreezeObject(cx, JSVAL_TO_OBJECT(ret));
}

template<> void ScriptInterface::ToJSVal<const CParamNode*>(JSContext* cx, JS::Value& ret, const CParamNode* const& val)
{
	if (val)
		ToJSVal(cx, ret, *val);
	else
		ret = JSVAL_VOID;
}

template<> bool ScriptInterface::FromJSVal<CColor>(JSContext* cx, jsval v, CColor& out)
{
	if (JSVAL_IS_PRIMITIVE(v))
		FAIL("jsval not an object");

	JSAutoRequest rq(cx);
	JSObject* obj = JSVAL_TO_OBJECT(v);

	JS::RootedValue r(cx);
	JS::RootedValue g(cx);
	JS::RootedValue b(cx);
	JS::RootedValue a(cx);
	if (!JS_GetProperty(cx, obj, "r", r.address()) || !FromJSVal(cx, r, out.r))
		FAIL("Failed to get property CColor.r");
	if (!JS_GetProperty(cx, obj, "g", g.address()) || !FromJSVal(cx, g, out.g))
		FAIL("Failed to get property CColor.g");
	if (!JS_GetProperty(cx, obj, "b", b.address()) || !FromJSVal(cx, b, out.b))
		FAIL("Failed to get property CColor.b");
	if (!JS_GetProperty(cx, obj, "a", a.address()) || !FromJSVal(cx, a, out.a))
		FAIL("Failed to get property CColor.a");
	// TODO: this probably has GC bugs if a getter returns an unrooted value

	return true;
}

template<> void ScriptInterface::ToJSVal<CColor>(JSContext* cx, JS::Value& ret, CColor const& val)
{
	JSAutoRequest rq(cx);
	JSObject* obj = JS_NewObject(cx, NULL, NULL, NULL);
	if (!obj)
	{
		ret = JSVAL_VOID;
		return;
	}

	JS::RootedValue r(cx);
	JS::RootedValue g(cx);
	JS::RootedValue b(cx);
	JS::RootedValue a(cx);
	ToJSVal(cx, r.get(), val.r);
	ToJSVal(cx, g.get(), val.g);
	ToJSVal(cx, b.get(), val.b);
	ToJSVal(cx, a.get(), val.a);

	JS_SetProperty(cx, obj, "r", r.address());
	JS_SetProperty(cx, obj, "g", g.address());
	JS_SetProperty(cx, obj, "b", b.address());
	JS_SetProperty(cx, obj, "a", a.address());

	ret = OBJECT_TO_JSVAL(obj);
}

template<> bool ScriptInterface::FromJSVal<fixed>(JSContext* cx, jsval v, fixed& out)
{
	JSAutoRequest rq(cx);
	double ret;
	if (!JS_ValueToNumber(cx, v, &ret))
		return false;
	out = fixed::FromDouble(ret);
	// double can precisely represent the full range of fixed, so this is a non-lossy conversion

	return true;
}

template<> void ScriptInterface::ToJSVal<fixed>(JSContext* UNUSED(cx), JS::Value& ret, const fixed& val)
{
	ret = JS_NumberValue(val.ToDouble());
}

template<> bool ScriptInterface::FromJSVal<CFixedVector3D>(JSContext* cx, jsval v, CFixedVector3D& out)
{
	if (JSVAL_IS_PRIMITIVE(v))
		return false; // TODO: report type error

	JSAutoRequest rq(cx);
	JSObject* obj = JSVAL_TO_OBJECT(v);

	JS::RootedValue p(cx);

	if (!JS_GetProperty(cx, obj, "x", p.address())) return false; // TODO: report type errors
	if (!FromJSVal(cx, p, out.X)) return false;

	if (!JS_GetProperty(cx, obj, "y", p.address())) return false;
	if (!FromJSVal(cx, p, out.Y)) return false;

	if (!JS_GetProperty(cx, obj, "z", p.address())) return false;
	if (!FromJSVal(cx, p, out.Z)) return false;

	return true;
}

template<> void ScriptInterface::ToJSVal<CFixedVector3D>(JSContext* cx, JS::Value& ret, const CFixedVector3D& val)
{
	JSAutoRequest rq(cx);

	// apply the Vector3D prototype to the return value;
 	ScriptInterface::CxPrivate* pCxPrivate = ScriptInterface::GetScriptInterfaceAndCBData(cx);
	JSObject* obj = JS_NewObject(cx, NULL, JSVAL_TO_OBJECT(pCxPrivate->pScriptInterface->GetCachedValue(ScriptInterface::CACHE_VECTOR3DPROTO).get()), NULL);

	if (!obj)
	{
		ret = JSVAL_VOID;
		return;
	}

	JS::RootedValue x(cx);
	JS::RootedValue y(cx);
	JS::RootedValue z(cx);
	ToJSVal(cx, x.get(), val.X);
	ToJSVal(cx, y.get(), val.Y);
	ToJSVal(cx, z.get(), val.Z);

	JS_SetProperty(cx, obj, "x", x.address());
	JS_SetProperty(cx, obj, "y", y.address());
	JS_SetProperty(cx, obj, "z", z.address());

	ret = OBJECT_TO_JSVAL(obj);
}

template<> bool ScriptInterface::FromJSVal<CFixedVector2D>(JSContext* cx, jsval v, CFixedVector2D& out)
{
	JSAutoRequest rq(cx);
	if (JSVAL_IS_PRIMITIVE(v))
		return false; // TODO: report type error
	JSObject* obj = JSVAL_TO_OBJECT(v);

	JS::RootedValue p(cx);

	if (!JS_GetProperty(cx, obj, "x", p.address())) return false; // TODO: report type errors
	if (!FromJSVal(cx, p, out.X)) return false;

	if (!JS_GetProperty(cx, obj, "y", p.address())) return false;
	if (!FromJSVal(cx, p, out.Y)) return false;

	return true;
}

template<> void ScriptInterface::ToJSVal<CFixedVector2D>(JSContext* cx, JS::Value& ret, const CFixedVector2D& val)
{
	JSAutoRequest rq(cx);

	// apply the Vector2D prototype to the return value
 	ScriptInterface::CxPrivate* pCxPrivate = ScriptInterface::GetScriptInterfaceAndCBData(cx);
	JSObject* obj = JS_NewObject(cx, NULL, JSVAL_TO_OBJECT(pCxPrivate->pScriptInterface->GetCachedValue(ScriptInterface::CACHE_VECTOR2DPROTO).get()), NULL);

	if (!obj)
	{
		ret = JSVAL_VOID;
		return;
	}

	JS::RootedValue x(cx);
	JS::RootedValue y(cx);
	ToJSVal(cx, x.get(), val.X);
	ToJSVal(cx, y.get(), val.Y);

	JS_SetProperty(cx, obj, "x", x.address());
	JS_SetProperty(cx, obj, "y", y.address());

	ret = OBJECT_TO_JSVAL(obj);
}

template<> void ScriptInterface::ToJSVal<Grid<u8> >(JSContext* cx, JS::Value& ret, const Grid<u8>& val)
{
	JSAutoRequest rq(cx);
	uint32_t length = (uint32_t)(val.m_W * val.m_H);
	uint32_t nbytes = (uint32_t)(length * sizeof(uint8_t));
	JSObject* objArr = JS_NewUint8Array(cx, length);
	memcpy((void*)JS_GetUint8ArrayData(objArr), val.m_Data, nbytes);

	JS::RootedValue data(cx, OBJECT_TO_JSVAL(objArr));
	JS::RootedValue w(cx);
	JS::RootedValue h(cx);
	ScriptInterface::ToJSVal(cx, w.get(), val.m_W);
	ScriptInterface::ToJSVal(cx, h.get(), val.m_H);

	JSObject* obj = JS_NewObject(cx, NULL, NULL, NULL);
	JS_SetProperty(cx, obj, "width", w.address());
	JS_SetProperty(cx, obj, "height", h.address());
	JS_SetProperty(cx, obj, "data", data.address());

	ret = OBJECT_TO_JSVAL(obj);
}
 
template<> void ScriptInterface::ToJSVal<Grid<u16> >(JSContext* cx, JS::Value& ret, const Grid<u16>& val)
 {
	JSAutoRequest rq(cx);
	uint32_t length = (uint32_t)(val.m_W * val.m_H);
	uint32_t nbytes = (uint32_t)(length * sizeof(uint16_t));
	JSObject* objArr = JS_NewUint16Array(cx, length);
	memcpy((void*)JS_GetUint16ArrayData(objArr), val.m_Data, nbytes);
 
	JS::RootedValue data(cx, OBJECT_TO_JSVAL(objArr));
	JS::RootedValue w(cx);
	JS::RootedValue h(cx);
	ScriptInterface::ToJSVal(cx, w.get(), val.m_W);
	ScriptInterface::ToJSVal(cx, h.get(), val.m_H);

	JSObject* obj = JS_NewObject(cx, NULL, NULL, NULL);
	JS_SetProperty(cx, obj, "width", w.address());
	JS_SetProperty(cx, obj, "height", h.address());
	JS_SetProperty(cx, obj, "data", data.address());

	ret = OBJECT_TO_JSVAL(obj);
}
