/* Copyright (C) 2015 Wildfire Games.
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

#ifndef INCLUDED_UNIFORMBINDING
#define INCLUDED_UNIFORMBINDING

#include "lib/ogl.h"

#include "ps/CStrIntern.h"

struct UniformBinding
{
	UniformBinding(int blockId, int uniformId, int type, bool isInstanced) : 
		m_BlockId(blockId), m_UniformId(uniformId), m_Type(type), m_IsInstanced(isInstanced) { }
	UniformBinding() : m_BlockId(-1), m_UniformId(-1), m_Type(-1), m_IsInstanced(false) { }


	bool Active() { return m_BlockId != -1 && m_UniformId != -1; }

	int m_BlockId;
	int m_UniformId;
	int m_Type;
	bool m_IsInstanced;
};

/**
 * Identifies a uniform block by name and ID.
 * The ID is only valid for one specific shader and might be different for other shaders 
 */ 
struct UniformBlockIdentifier
{
	GLuint ID;
	CStrIntern Name;
};

#endif // INCLUDED_UNIFORMBINDING
