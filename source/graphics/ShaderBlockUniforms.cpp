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

#include "ShaderBlockUniforms.h"

#include "graphics/ShaderProgram.h"
#include "maths/Vector4D.h"
#include "ps/ThreadUtil.h"
#include "ps/Profile.h"

#include <sstream>

void CShaderBlockUniforms::GetBindings()
{
	//m_BoundValueAssignments.clear();
	UniformBlockManager& uniformBlockManager = g_Renderer.GetUniformBlockManager();
	for (const UnboundValueAssignment& unboundAssignment : m_UnboundValueAssignments)
	{
		UniformBinding binding = uniformBlockManager.GetBinding(unboundAssignment.BlockName, unboundAssignment.UniformName, unboundAssignment.IsInstanced);
		if (!binding.Active())
		{
			// TODO: Uniforms can be disabled by a shader define, but also because of an error.
			LOGERROR("Could not bind to uniform %s in block %s", unboundAssignment.UniformName.c_str(), unboundAssignment.BlockName.c_str());
			continue;
		}
		m_BoundValueAssignments.emplace_back(BoundValueAssignment { binding, unboundAssignment.Value });
	}
	m_UniformsBound = true;
}
