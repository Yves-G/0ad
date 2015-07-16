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

#ifndef INCLUDED_SHADERBLOCKUNIFORMS
#define INCLUDED_SHADERBLOCKUNIFORMS

#include "graphics/ShaderProgramPtr.h"
#include "graphics/UniformBlockManager.h"
#include "ps/CStr.h"
#include "ps/CStrIntern.h"
#include "ps/CLogger.h"
#include "renderer/Renderer.h"



/**
 * Represets value assignments to a number of uniforms from one or more blocks.
 * 
 * 1. Add your value assignments. Use names to identify uniform blocks and uniforms within these blocks.
 * 2. Establish bindings. Validations if the block and uniform names exists happen here.
 * 3. Now call SetUniforms to copy the values to the buffers using the established bindings.
 */
class CShaderBlockUniforms
{
public:
	
	CShaderBlockUniforms() :
		m_UniformsBound(false)
	{}
	
	struct UnboundValueAssignment
	{
		CStrIntern BlockName;
		CStrIntern UniformName;
		bool IsInstanced;
		CVector4D Value;
	};
	
	struct BoundValueAssignment
	{
		UniformBinding Binding;
		CVector4D Value;
	};
	
	void Add(CStrIntern blockName, CStrIntern uniformName, bool isInstanced, CVector4D value)
	{
		m_UnboundValueAssignments.emplace_back(UnboundValueAssignment { blockName, uniformName, isInstanced, value });
	}
	
	void GetBindings();

	template<UniformBlockManager::InstancingMode M>
	inline void SetUniforms(u32 instanceId);
	
private:
	
	bool m_UniformsBound;
	std::vector<BoundValueAssignment> m_BoundValueAssignments;
	std::vector<UnboundValueAssignment> m_UnboundValueAssignments;
};

template<UniformBlockManager::InstancingMode M>
inline void CShaderBlockUniforms::SetUniforms(u32 instancId)
{
	UniformBlockManager& uniformBlockManager = g_Renderer.GetUniformBlockManager();
	
	// Loading of shaders and materials is quite dyncamic and does not always happen in the same order.
	// We try to set up the bindings here because it this point they must be available in any case 
	// (else it can be considered an error).
	if (!m_UniformsBound)
	{
		GetBindings();
	}
		
	uniformBlockManager.SetCurrentInstance<M>(instancId);
	
	for (BoundValueAssignment valueAssignment : m_BoundValueAssignments)
	{
		uniformBlockManager.SetUniformF4<M>(valueAssignment.Binding, valueAssignment.Value.X, 
											valueAssignment.Value.Y, valueAssignment.Value.Z, valueAssignment.Value.W);
		/*
		switch (valueAssignment.Binding.m_Type)
		{
		case GL_FLOAT:
			uniformBlockManager.SetUniform(valueAssignment.Binding, valueAssignment.Value.X);
			break;
		default:
			LOGERROR("Missing definition how to convert this type from CVector4D!");			
		}*/
	}
}

#endif // INCLUDED_SHADERBLOCKUNIFORMS
