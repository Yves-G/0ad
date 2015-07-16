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

#ifndef INCLUDED_UNIFORMBINDINGMANAGER
#define INCLUDED_UNIFORMBINDINGMANAGER

#include <map>

#include "graphics/UniformBinding.h"
#include "UniformBuffer.h"
#include "graphics/ShaderProgramPtr.h"


/**
 * Knows about available binding points. Gets informed whenever changing of bindings might be required and 
 * manages the binding to avoid unnecessary rebinding.
 *
 * Basically I'm trying to prepare data for as many draw calls as possible (trying to minimize reuse of binding points for different blocks)
 * It should be possible to reuse some bindings instead of just setting them up again after ResetBindings is called. I'm currently not trying
 * to do this because it would add more overhead and complexity and because I don't know if it would be worth it.
 *
 */
class UniformBlockManager
{
public:
	UniformBlockManager() :
		m_NextFreeBindingPoint(0),
		m_MaxUniformBufferBindings(0),
		m_CurrentInstance(0)
	{
	}
	
	void Initialize()
	{
		glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &m_MaxUniformBufferBindings);
	}

	void ResetBindings()
	{ 
		m_PointBindings.clear();
		m_NextFreeBindingPoint = 0;
	}
	
	void RegisterUniformBlocks(const CShaderProgram& shader);

	/**
	 * @return: returns false if no more free binding points are available.
	 * In this case you have to use the bindings that have been set up so far (draw)
	 * and then call ResetBindings, which will allow the UniformBindingManager to
	 * reuse the binding points again and possibly map them to different buffers.
	 */  
	bool EnsureBlockBinding(const CShaderProgramPtr& shader);
	
	enum InstancingMode { MODEL_INSTANCED, MATERIAL_INSTANCED, NOT_INSTANCED };
	
	template<InstancingMode M, typename T>
	void SetUniform(UniformBinding id, T v)
	{
		// TODO: Is this actually required to trigger a compiler error in case of invalid enums?
		static_assert(M == MODEL_INSTANCED || M == MATERIAL_INSTANCED || M == NOT_INSTANCED, 
						"template parameter M must be either MODEL_INSTANCED or MATERIAL_INSTANCED!");
		
		ENSURE(m_DirtyBuffers.size() > id.m_BlockId);
		ENSURE(m_UniformBuffers.size() == m_DirtyBuffers.size());
		
		GLuint instanceId(0);
		
		if (id.m_IsInstanced)
		{
			if (M == MODEL_INSTANCED)
				instanceId = m_CurrentInstance;
			else if (M == MATERIAL_INSTANCED)
				instanceId = m_CurrentMaterial;
		}
		
		m_DirtyBuffers[id.m_BlockId] = true;
		m_UniformBuffers[id.m_BlockId].SetUniform(id, v, instanceId);
	}
	
	/**
	 * Sets an instanced array of T (array of array of T).
	 */
	template<InstancingMode M, typename T> 
	void SetUniform(const UniformBinding& id, size_t count, const T* v)
	{
		// TODO: Is this actually required to trigger a compiler error in case of invalid enums?
		static_assert(M == MODEL_INSTANCED || M == MATERIAL_INSTANCED || M == NOT_INSTANCED, 
						"template parameter M must be either MODEL_INSTANCED or MATERIAL_INSTANCED!");
		
		ENSURE(m_DirtyBuffers.size() > id.m_BlockId);
		ENSURE(m_UniformBuffers.size() == m_DirtyBuffers.size());
		
		GLuint instanceId(0);
		
		if (id.m_IsInstanced)
		{
			if (M == MODEL_INSTANCED)
				instanceId = m_CurrentInstance;
			else if (M == MATERIAL_INSTANCED)
				instanceId = m_CurrentMaterial;
		}

		m_DirtyBuffers[id.m_BlockId] = true;
		m_UniformBuffers[id.m_BlockId].SetUniform(id, count, v, instanceId);
	}
	
	template <InstancingMode M>
	void SetUniformF4(UniformBinding id, float v0, float v1, float v2, float v3)
	{
		// TODO: Is this actually required to trigger a compiler error in case of invalid enums?
		static_assert(M == MODEL_INSTANCED || M == MATERIAL_INSTANCED || M == NOT_INSTANCED, 
						"template parameter M must be either MODEL_INSTANCED or MATERIAL_INSTANCED!");
		
		ENSURE(m_DirtyBuffers.size() > id.m_BlockId);
		ENSURE(m_UniformBuffers.size() == m_DirtyBuffers.size());
		
		GLuint instanceId(0);
		
		if (id.m_IsInstanced)
		{
			if (M == MODEL_INSTANCED)
				instanceId = m_CurrentInstance;
			else if (M == MATERIAL_INSTANCED)
				instanceId = m_CurrentMaterial;
		}

		m_DirtyBuffers[id.m_BlockId] = true;
		m_UniformBuffers[id.m_BlockId].SetUniformF4(id, v0, v1, v2, v3, instanceId);
	}
	
	/**
	 * Uploads all dirty blocks to the GPU.
	 */
	void Upload()
	{
		for (int i=0; i<m_DirtyBuffers.size(); ++i)
		{
			if (!m_DirtyBuffers[i])
				continue;
			
			m_UniformBuffers[i].Upload();
			m_DirtyBuffers[i] = false;
		}
		/*
		for (int i=0; i<m_UniformBuffers.size(); ++i)
			m_UniformBuffers[i].Upload();*/
	}
	
	
	/**
	 * Get a binding for the specified uniform in the specified block
	 *
	 * Lookups by name are more expensive than direct access to indices.
	 * Bindings store these indices and are used by SetUniform.
	 * 
	 * @return Returns a binding for the specified uniform in the specified block or 
	 * an inactive binding if the block does not exist or the uniform does not exist
	 * within that block. 
	 */
	UniformBinding GetBinding(CStrIntern blockName, CStrIntern uniformName, bool isInstanced) const
	{
		UniformBinding binding;
		binding.m_IsInstanced = isInstanced;
		const auto& nameIndexLookup  = m_UniformBufferIndices.find(blockName);
		if (nameIndexLookup == m_UniformBufferIndices.end())
		{
			std::cerr << "GetBinding: not found block name: " << blockName.c_str() << std::endl;
			return binding;
		}
		
		m_UniformBuffers[nameIndexLookup->second].GetBinding(binding, uniformName);
		if (binding.m_UniformId != -1)
			binding.m_BlockId = nameIndexLookup->second; // the index of the buffer in m_UniformBuffers
		return binding;
	}
	
	template <InstancingMode M>
	inline void SetCurrentInstance(GLuint instance);
	
	/*
	GLuint GetCurrentInstance() { return m_CurrentMaterial; }
	void SetCurrentInstance(GLuint instance) { m_CurrentMaterial = instance; }
	
	GLuint GetCurrentMaterial() { return m_CurrentMaterial; }
	void SetCurrentMaterial(GLuint instance) { m_CurrentMaterial = instance; }
	*/

private:	

	GLuint m_NextFreeBindingPoint;
	GLint m_MaxUniformBufferBindings;
	
	// The UniformBuffer class queries these for instanced data
	GLuint m_CurrentInstance;
	GLuint m_CurrentMaterial;

	struct PointBufferBinding
	{
		GLuint point;
	};
	
	// which buffer (by uniform block name) is bound to which binding point
	//
	// TODO: Could the concept of bindings be applied here too?
	// Do one runtime call to get the binding ID from a block name, which is then
	// used to access a vector. Vector access by index causes less overhead than map
	// lookups.
	std::vector<UniformBuffer> m_UniformBuffers;
	std::map<CStrIntern, unsigned int> m_UniformBufferIndices; // used for easy lookup
	
	// TODO: Use index into m_UniformBuffers as key?
	std::map<CStrIntern, PointBufferBinding> m_PointBindings;
	//std::map<std::string, UniformBuffer> m_UniformBuffers;
	
	std::vector<bool> m_DirtyBuffers;
};

template<>
inline void UniformBlockManager::SetCurrentInstance<UniformBlockManager::MODEL_INSTANCED>(GLuint instance)
{
	m_CurrentInstance = instance;
}

template<>
inline void UniformBlockManager::SetCurrentInstance<UniformBlockManager::MATERIAL_INSTANCED>(GLuint instance)
{
	m_CurrentMaterial= instance;
}

#endif // INCLUDED_UNIFORMBINDINGMANAGER
