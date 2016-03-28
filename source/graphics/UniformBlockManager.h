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

#include "graphics/ModelAbstract.h"
#include "graphics/UniformBinding.h"
#include "graphics/UniformBuffer.h"
#include "graphics/ShaderProgramPtr.h"
#include "graphics/ShaderBlockUniforms.h"


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
		m_NextFreeUBOBindingPoint(1),
		m_NextFreeSSBOBindingPoint(1),
		m_MaxUBOBindings(0),
		m_MaxSSBOBindings(0),
		m_CurrentInstance(0),
		m_AvailableBindingsFlag(0),
		m_PlayerColorBlockName("PlayerColorBlock"),
		m_ShadingColorBlockName("ShadingColorBlock"),
		m_MaterialIDBlockName("MaterialIDBlock")
	{
	}
	
	void Initialize()
	{
		glGetIntegerv(GL_MAX_UNIFORM_BUFFER_BINDINGS, &m_MaxUBOBindings);
	}
/*
	void ResetBindings()
	{ 
		m_PointBindings.clear();
		m_NextFreeUBOBindingPoint = 0;
		m_NextFreeSSBOBindingPoint = 0;
	}
*/
	
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
		ENSURE(m_InterfaceBlockBuffers.size() == m_DirtyBuffers.size());
		
		GLuint instanceId(0);
		
		if (id.m_IsInstanced)
		{
			if (M == MODEL_INSTANCED)
				instanceId = m_CurrentInstance;
			else if (M == MATERIAL_INSTANCED)
				instanceId = m_CurrentMaterial;
		}
		
		m_DirtyBuffers[id.m_BlockId] = true;
		m_InterfaceBlockBuffers[id.m_BlockId].SetUniform(id, v, instanceId);
	}
	
	template<typename T>
	void SetUniform(UniformBinding id, GLuint instanceId, T v)
	{		
		ENSURE(m_DirtyBuffers.size() > id.m_BlockId);
		ENSURE(m_InterfaceBlockBuffers.size() == m_DirtyBuffers.size());
		
		m_DirtyBuffers[id.m_BlockId] = true;
		m_InterfaceBlockBuffers[id.m_BlockId].SetUniform(id, v, instanceId);
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
		ENSURE(m_InterfaceBlockBuffers.size() == m_DirtyBuffers.size());
		
		GLuint instanceId(0);
		
		if (id.m_IsInstanced)
		{
			if (M == MODEL_INSTANCED)
				instanceId = m_CurrentInstance;
			else if (M == MATERIAL_INSTANCED)
				instanceId = m_CurrentMaterial;
		}

		m_DirtyBuffers[id.m_BlockId] = true;
		m_InterfaceBlockBuffers[id.m_BlockId].SetUniform(id, count, v, instanceId);
	}
	
	template <InstancingMode M>
	void SetUniformF4(UniformBinding id, float v0, float v1, float v2, float v3)
	{
		// TODO: Is this actually required to trigger a compiler error in case of invalid enums?
		static_assert(M == MODEL_INSTANCED || M == MATERIAL_INSTANCED || M == NOT_INSTANCED, 
						"template parameter M must be either MODEL_INSTANCED or MATERIAL_INSTANCED!");
		
		ENSURE(m_DirtyBuffers.size() > id.m_BlockId);
		ENSURE(m_InterfaceBlockBuffers.size() == m_DirtyBuffers.size());
		
		GLuint instanceId(0);
		
		if (id.m_IsInstanced)
		{
			if (M == MODEL_INSTANCED)
				instanceId = m_CurrentInstance;
			else if (M == MATERIAL_INSTANCED)
				instanceId = m_CurrentMaterial;
		}

		m_DirtyBuffers[id.m_BlockId] = true;
		m_InterfaceBlockBuffers[id.m_BlockId].SetUniformF4(id, v0, v1, v2, v3, instanceId);
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
			
			m_InterfaceBlockBuffers[i].Upload();
			//std::cout << "Uploading InterfaceBlock: " << m_InterfaceBlockBuffers[i].m_BlockName.c_str() << std::endl;
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
		const auto& blockLookup  = m_InterfaceBlockIndices.find(blockName);
		if (blockLookup != m_InterfaceBlockIndices.end())
		{
			m_InterfaceBlockBuffers[blockLookup->second].GetBinding(binding, uniformName);
			if (binding.m_UniformId != -1)
				binding.m_BlockId = blockLookup->second; // the index of the buffer in m_UniformBuffers
			return binding;
		}	
		
		std::cerr << "GetBinding: not found block name: " << blockName.c_str() << std::endl;
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

	GLuint m_NextFreeUBOBindingPoint;
	GLuint m_NextFreeSSBOBindingPoint;
	
	GLint m_MaxUBOBindings;
	GLint m_MaxSSBOBindings;
	
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
	std::vector<InterfaceBlock> m_InterfaceBlockBuffers;
	std::map<CStrIntern, unsigned int> m_InterfaceBlockIndices; // used for easy lookup
	
	// TODO: Use index into m_UniformBuffers as key?
	std::map<CStrIntern, PointBufferBinding> m_UBOPointBindings;
	std::map<CStrIntern, PointBufferBinding> m_SSBOPointBindings;
	//std::map<std::string, UniformBuffer> m_UniformBuffers;
	
	std::vector<bool> m_DirtyBuffers;
	
	
	
/**
 * Keeps model data updated.
 * It's not quite a trivial task to keep model data update in all the situations that might change
 * certain parts of it. Also we can't get any bindings to uniforms or shader storage blocks before
 * all shaders are loaded (and reloading might happen later).
 *   
 * We keep track of all Models in order to generate the data for all models as soon as the shaders
 * were loaded. We also might want to recreate data again later when hotloading shaders or 
 * changing rendering settings for example.
 * We also listen to different event that might trigger changes to model data and update the data
 * if needed.
 */
public:

// Events that might trigger changes to model data
	void ModelAdded(CModelAbstract* model)
	{
		ENSURE(m_Models.find(model) == m_Models.end());
		m_Models.insert(model);
		GenModelData(model, m_AvailableBindingsFlag);
	}
	void ModelRemoved(CModelAbstract* model)
	{
		ENSURE(m_Models.find(model) != m_Models.end());

		m_Models.erase(model);
	}
	
	void MaterialSealed(const CMaterial& material)
	{
		// If the same material already has data in m_UnboundStaticBlockUniforms, that means it's
		// not yet loaded because that uniform block has not been seen in any loaded shader yet.
		// In this case it's pointless to call UpdateMaterialBinding (which is a bit expensive).
		auto itr = m_UnboundStaticBlockUniforms.find(material.GetId());
		
		// Updating the data is required no matter if there's already data stored here or not.
		// (Values of the material could have changed)
		m_UnboundStaticBlockUniforms[material.GetId()] = material.GetStaticBlockUniforms();
		
		if (itr != m_UnboundStaticBlockUniforms.end())
			return;

		UpdateMaterialBinding();
	}

	void PlayerIDChanged(CModelAbstract* model)
	{
		if (m_PlayerColorBinding.Active())
			SetPlayerColor(model);
	}
	
	void ShadingColorChanged(CModelAbstract* model)
	{
		if (m_ShadingColorBinding.Active())
			SetShadingColor(model);
	}
	
	void MaterialChanged(CModelAbstract* model)
	{
		if (m_MaterialIDBinding.Active())
			SetMaterialID(model);
	}
	// void PlayerIDColorChanged(...); // a color of a player probably can't change in-game

private:
	
	struct PairIDBlockUniform
	{
		int m_MaterialID;
		CShaderBlockUniforms m_BlockUniforms;
	};

	/// Events that might trigger changes to model data
	
	void InterfaceBlockAdded(const InterfaceBlockIdentifier& blockIdentifier);
	
	void MaterialBound(const int materialID, CShaderBlockUniforms& shaderBlockUniforms);
	
	/// Functions
	
	void SetPlayerColor(const CModelAbstract* model)
	{
		SetUniform(m_PlayerColorBinding, model->GetID(), g_Game->GetPlayerColor(model->GetPlayerID()));
	}
	
	void SetShadingColor(const CModelAbstract* model)
	{
		SetUniform(m_ShadingColorBinding, model->GetID(), model->GetShadingColor());
	}
	
	void SetMaterialID(CModelAbstract* model)
	{
		SetUniform(m_MaterialIDBinding, (GLuint)model->GetID(), (GLuint)model->GetMaterial().GetId());
	}

	void GenModelData(CModelAbstract* model, const int flags)
	{
		if (flags & PLAYER_COLOR)
			SetPlayerColor(model);
		if (flags & SHADING_COLOR)
			SetShadingColor(model);
		if (flags & MATERIAL_ID)
			SetMaterialID(model);
	}

	void GenAllModelData(const int flags)
	{
		for (auto* model : m_Models)
			GenModelData(model, flags);
	}
	
	void UpdateMaterialBinding()
	{
		for (std::map<int, CShaderBlockUniforms>::iterator itr = m_UnboundStaticBlockUniforms.begin(); 
			itr != m_UnboundStaticBlockUniforms.end();)
		{
			if (!itr->second.GetBindings())
			{
				++itr;
				continue;
			}
			
			MaterialBound(itr->first, itr->second);
			itr = m_UnboundStaticBlockUniforms.erase(itr);
		}
	}
	
	/// data members
	enum DATA_FLAGS { PLAYER_COLOR = 1, SHADING_COLOR = 2, MATERIAL_ID = 4 };
	int m_AvailableBindingsFlag;
	std::set<CModelAbstract*> m_Models;

	// TODO: Add this to where the other static CStrIntern are defined
	const CStrIntern m_PlayerColorBlockName;
	const CStrIntern m_ShadingColorBlockName;
	const CStrIntern m_MaterialIDBlockName;
	UniformBinding m_PlayerColorBinding;
	UniformBinding m_ShadingColorBinding;
	UniformBinding m_MaterialIDBinding;
	
	//std::list<PairIDBlockUniform> m_UnboundStaticBlockUniforms;
	std::map<int, CShaderBlockUniforms> m_UnboundStaticBlockUniforms;
	//std::set<CMaterial*> m_UnboundMaterials;
	//std::set<CMaterial*> m_BoundMaterials;
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
