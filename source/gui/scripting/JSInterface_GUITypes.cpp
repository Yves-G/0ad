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

#include "JSInterface_GUITypes.h"
#include "ps/CStr.h"

/**** GUISize ****/
JSClass JSI_GUISize::JSI_class = {
	"GUISize", 0,
		JS_PropertyStub, JS_DeletePropertyStub,
		JS_PropertyStub, JS_StrictPropertyStub,
		JS_EnumerateStub, JS_ResolveStub,
		JS_ConvertStub, NULL,
		NULL, NULL, JSI_GUISize::construct, NULL
};

JSPropertySpec JSI_GUISize::JSI_props[] = 
{
	/*
	JS_PSGS( "left", JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE ),
	{ "top", JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE },
	{ "right", JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE },
	{ "bottom", JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE },
	{ "rleft", JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE },
	{ "rtop", JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE },
	{ "rright", JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE },
	{ "rbottom", JS_PropertyStub, JS_StrictPropertyStub, JSPROP_ENUMERATE }, */
	//{ "left"		,JS_PropertyStub , JS_StrictPropertyStub, JSPROP_ENUMERATE},
	//{ "top", 		JSPROP_ENUMERATE},
	//{ "right",		JSPROP_ENUMERATE},
	//{ "bottom",		JSPROP_ENUMERATE},
	//{ "rleft",		JSPROP_ENUMERATE},
	//{ "rtop",		JSPROP_ENUMERATE},
	//{ "rright",		JSPROP_ENUMERATE},
	//{ "rbottom",	JSPROP_ENUMERATE},
	JS_PS_END
};

JSFunctionSpec JSI_GUISize::JSI_methods[] = 
{
	JS_FS("toString", JSI_GUISize::toString, 0, 0),
	JS_FS_END
};

bool JSI_GUISize::construct(JSContext* cx, uint argc, jsval* vp)
{
	JSAutoRequest rq(cx);
	JS::RootedObject obj(cx, JS_NewObject(cx, &JSI_GUISize::JSI_class, JS::NullPtr(), JS::NullPtr()));

	if (argc == 8)
	{
		JS::RootedValue v0(cx, JS_ARGV(cx, vp)[0]);
		JS::RootedValue v1(cx, JS_ARGV(cx, vp)[1]);
		JS::RootedValue v2(cx, JS_ARGV(cx, vp)[2]);
		JS::RootedValue v3(cx, JS_ARGV(cx, vp)[3]);
		JS::RootedValue v4(cx, JS_ARGV(cx, vp)[4]);
		JS::RootedValue v5(cx, JS_ARGV(cx, vp)[5]);
		JS::RootedValue v6(cx, JS_ARGV(cx, vp)[6]);
		JS::RootedValue v7(cx, JS_ARGV(cx, vp)[7]);
		JS_SetProperty(cx, obj, "left",		v0);
		JS_SetProperty(cx, obj, "top",		v1);
		JS_SetProperty(cx, obj, "right",	v2);
		JS_SetProperty(cx, obj, "bottom",	v3);
		JS_SetProperty(cx, obj, "rleft",	v4);
		JS_SetProperty(cx, obj, "rtop",		v5);
		JS_SetProperty(cx, obj, "rright",	v6);
		JS_SetProperty(cx, obj, "rbottom",	v7);
	}
	else if (argc == 4)
	{
		JS::RootedValue zero(cx, JSVAL_ZERO);
		JS::RootedValue v0(cx, JS_ARGV(cx, vp)[0]);
		JS::RootedValue v1(cx, JS_ARGV(cx, vp)[1]);
		JS::RootedValue v2(cx, JS_ARGV(cx, vp)[2]);
		JS::RootedValue v3(cx, JS_ARGV(cx, vp)[3]);
		JS_SetProperty(cx, obj, "left",		v0);
		JS_SetProperty(cx, obj, "top",		v1);
		JS_SetProperty(cx, obj, "right",	v2);
		JS_SetProperty(cx, obj, "bottom",	v3);
		JS_SetProperty(cx, obj, "rleft",	zero);
		JS_SetProperty(cx, obj, "rtop",		zero);
		JS_SetProperty(cx, obj, "rright",	zero);
		JS_SetProperty(cx, obj, "rbottom",	zero);
	}
	else
	{
		JS::RootedValue zero(cx, JSVAL_ZERO);
		JS_SetProperty(cx, obj, "left",		zero);
		JS_SetProperty(cx, obj, "top",		zero);
		JS_SetProperty(cx, obj, "right",	zero);
		JS_SetProperty(cx, obj, "bottom",	zero);
		JS_SetProperty(cx, obj, "rleft",	zero);
		JS_SetProperty(cx, obj, "rtop",		zero);
		JS_SetProperty(cx, obj, "rright",	zero);
		JS_SetProperty(cx, obj, "rbottom",	zero);
	}

	JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
	return true;
}

// Produces "10", "-10", "50%", "50%-10", "50%+10", etc
CStr ToPercentString(double pix, double per)
{
	if (per == 0)
		return CStr::FromDouble(pix);
	else
		return CStr::FromDouble(per)+"%"+( pix == 0.0 ? CStr() : pix > 0.0 ? CStr("+")+CStr::FromDouble(pix) : CStr::FromDouble(pix) );
}

bool JSI_GUISize::toString(JSContext* cx, uint argc, jsval* vp)
{
	UNUSED2(argc);

	CStr buffer;

	try
	{
		ScriptInterface* pScriptInterface = ScriptInterface::GetScriptInterfaceAndCBData(cx)->pScriptInterface;
		double val, valr;
#define SIDE(side) \
		pScriptInterface->GetProperty(JS_THIS_VALUE(cx, vp), #side, val); \
		pScriptInterface->GetProperty(JS_THIS_VALUE(cx, vp), "r"#side, valr); \
		buffer += ToPercentString(val, valr);
		SIDE(left);
		buffer += " ";
		SIDE(top);
		buffer += " ";
		SIDE(right);
		buffer += " ";
		SIDE(bottom);
#undef SIDE
	}
	catch (PSERROR_Scripting_ConversionFailed&)
	{
		JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, "<Error converting value to numbers>")));
		return true;
	}

	JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buffer.c_str())));
	return true;
}


/**** GUIColor ****/


JSClass JSI_GUIColor::JSI_class = {
	"GUIColor", 0,
		JS_PropertyStub, JS_DeletePropertyStub,
		JS_PropertyStub, JS_StrictPropertyStub,
		JS_EnumerateStub, JS_ResolveStub,
		JS_ConvertStub, NULL,
		NULL, NULL, JSI_GUIColor::construct, NULL
};

JSPropertySpec JSI_GUIColor::JSI_props[] = 
{
	/*{ "r",	JSPROP_ENUMERATE},
	{ "g",	JSPROP_ENUMERATE},
	{ "b",	JSPROP_ENUMERATE},
	{ "a",	JSPROP_ENUMERATE},*/
	{ 0 }
};

JSFunctionSpec JSI_GUIColor::JSI_methods[] = 
{
	JS_FS("toString", JSI_GUIColor::toString, 0, 0),
	JS_FS_END
};

bool JSI_GUIColor::construct(JSContext* cx, uint argc, jsval* vp)
{
	JSAutoRequest rq(cx);
	JS::RootedObject obj(cx, JS_NewObject(cx, &JSI_GUIColor::JSI_class, JS::NullPtr(), JS::NullPtr()));
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

	if (argc == 4)
	{
		JS_SetProperty(cx, obj, "r", args[0]);
		JS_SetProperty(cx, obj, "g", args[1]);
		JS_SetProperty(cx, obj, "b", args[2]);
		JS_SetProperty(cx, obj, "a", args[3]);
	}
	else
	{
		// Nice magenta:
		JS::RootedValue c(cx, JS::NumberValue(1.0));
		JS_SetProperty(cx, obj, "r", c);
		JS_SetProperty(cx, obj, "b", c);
		JS_SetProperty(cx, obj, "a", c);
		c = JS::NumberValue(0.0);
		JS_SetProperty(cx, obj, "g", c);
	}

	JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
	return true;
}

bool JSI_GUIColor::toString(JSContext* cx, uint argc, jsval* vp)
{
	UNUSED2(argc);

	double r, g, b, a;
	ScriptInterface* pScriptInterface = ScriptInterface::GetScriptInterfaceAndCBData(cx)->pScriptInterface;
	pScriptInterface->GetProperty(JS_THIS_VALUE(cx, vp), "r", r);
	pScriptInterface->GetProperty(JS_THIS_VALUE(cx, vp), "g", g);
	pScriptInterface->GetProperty(JS_THIS_VALUE(cx, vp), "b", b);
	pScriptInterface->GetProperty(JS_THIS_VALUE(cx, vp), "a", a);
	char buffer[256];
	// Convert to integers, to be compatible with the GUI's string SetSetting
	snprintf(buffer, 256, "%d %d %d %d",
		(int)(255.0 * r),
		(int)(255.0 * g),
		(int)(255.0 * b),
		(int)(255.0 * a));
	JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buffer)));
	return true;
}

/**** GUIMouse ****/


JSClass JSI_GUIMouse::JSI_class = {
	"GUIMouse", 0,
		JS_PropertyStub, JS_DeletePropertyStub,
		JS_PropertyStub, JS_StrictPropertyStub,
		JS_EnumerateStub, JS_ResolveStub,
		JS_ConvertStub, NULL,
		NULL, NULL, JSI_GUIMouse::construct, NULL
};

JSPropertySpec JSI_GUIMouse::JSI_props[] = 
{
	/*{ "x",			JSPROP_ENUMERATE},
	{ "y",			JSPROP_ENUMERATE},
	{ "buttons",	JSPROP_ENUMERATE},*/
	{ 0 }
};

JSFunctionSpec JSI_GUIMouse::JSI_methods[] = 
{
	JS_FS("toString", JSI_GUIMouse::toString, 0, 0),
	JS_FS_END
};

bool JSI_GUIMouse::construct(JSContext* cx, uint argc, jsval* vp)
{
	JSAutoRequest rq(cx);
	JS::RootedObject obj(cx, JS_NewObject(cx, &JSI_GUIMouse::JSI_class, JS::NullPtr(), JS::NullPtr()));
	JS::CallArgs args = JS::CallArgsFromVp(argc, vp);

	if (argc == 3)
	{
		JS::RootedValue v0(cx, args[0]);
		JS::RootedValue v1(cx, args[1]);
		JS::RootedValue v2(cx, args[2]);
		JS_SetProperty(cx, obj, "x", v0);
		JS_SetProperty(cx, obj, "y", v1);
		JS_SetProperty(cx, obj, "buttons", v2);
	}
	else
	{
		JS::RootedValue zero (cx, JS::NumberValue(0));
		JS_SetProperty(cx, obj, "x", zero);
		JS_SetProperty(cx, obj, "y", zero);
		JS_SetProperty(cx, obj, "buttons", zero);
	}

	JS_SET_RVAL(cx, vp, OBJECT_TO_JSVAL(obj));
	return true;
}

bool JSI_GUIMouse::toString(JSContext* cx, uint argc, jsval* vp)
{
	UNUSED2(argc);

	i32 x, y, buttons;
	ScriptInterface* pScriptInterface = ScriptInterface::GetScriptInterfaceAndCBData(cx)->pScriptInterface;
	pScriptInterface->GetProperty(JS_THIS_VALUE(cx, vp), "x", x);
	pScriptInterface->GetProperty(JS_THIS_VALUE(cx, vp), "y", y);
	pScriptInterface->GetProperty(JS_THIS_VALUE(cx, vp), "buttons", buttons);

	char buffer[256];
	snprintf(buffer, 256, "%d %d %d", x, y, buttons);
	JS_SET_RVAL(cx, vp, STRING_TO_JSVAL(JS_NewStringCopyZ(cx, buffer)));
	return true;
}


// Initialise all the types at once:
void JSI_GUITypes::init(ScriptInterface& scriptInterface)
{
	scriptInterface.DefineCustomObjectType(&JSI_GUISize::JSI_class,  JSI_GUISize::construct,  1, JSI_GUISize::JSI_props,  JSI_GUISize::JSI_methods,  NULL, NULL);
	scriptInterface.DefineCustomObjectType(&JSI_GUIColor::JSI_class, JSI_GUIColor::construct, 1, JSI_GUIColor::JSI_props, JSI_GUIColor::JSI_methods, NULL, NULL);
	scriptInterface.DefineCustomObjectType(&JSI_GUIMouse::JSI_class, JSI_GUIMouse::construct, 1, JSI_GUIMouse::JSI_props, JSI_GUIMouse::JSI_methods, NULL, NULL);
}
