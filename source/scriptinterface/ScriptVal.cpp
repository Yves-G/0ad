/* Copyright (C) 2010 Wildfire Games.
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
#include "ScriptVal.h"


CScriptValRooted::CScriptValRooted(JSContext* cx, jsval val) : m_IsInitialized(true)
{
	m_Val.reset(new JS::PersistentRooted<JS::Value>(cx, val));
}

CScriptValRooted::CScriptValRooted(JSContext* cx, CScriptVal val) : m_IsInitialized(true)
{
	m_Val.reset(new JS::PersistentRooted<JS::Value>(cx, val.get()));
}

jsval CScriptValRooted::get() const
{
	if (m_Val)
		return m_Val->get();
	return JS::UndefinedValue();
}

jsval& CScriptValRooted::getRef() const
{
	ENSURE(m_Val);
	return m_Val->get();

}

bool CScriptValRooted::undefined() const
{
	return !(m_Val && !m_Val->get().isNull());
}

bool CScriptValRooted::uninitialised() const
{
	return !m_IsInitialized;
}
