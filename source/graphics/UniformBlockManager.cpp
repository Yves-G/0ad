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
 
#include "precompiled.h"

#include "UniformBlockManager.h"

#include "graphics/MaterialManager.h"
#include "graphics/ShaderProgram.h"

void UniformBlockManager::RegisterUniformBlocks(const CShaderProgram& shader)
{
	const std::vector<InterfaceBlockIdentifier>& blockIdentifiers = shader.GetUniformBlockIdentifiers();
	
	for (const InterfaceBlockIdentifier& blockIdentifier : blockIdentifiers)
	{
		const auto& nameIndexLookup = m_InterfaceBlockIndices.find(blockIdentifier.Name);
		if (nameIndexLookup != m_InterfaceBlockIndices.end())
			continue; // block already added
		
		// No such uniform block available so far, create one from the current shader program
		m_InterfaceBlockBuffers.emplace_back(shader.GetProgram(), blockIdentifier, blockIdentifier.BlockType);
		m_DirtyBuffers.resize(m_InterfaceBlockBuffers.size());
		m_InterfaceBlockIndices.emplace(blockIdentifier.Name, m_InterfaceBlockBuffers.size() - 1);
		InterfaceBlockAdded(blockIdentifier);
	}
}

void UniformBlockManager::InterfaceBlockAdded(const InterfaceBlockIdentifier& blockIdentifier)
{
	std::cout << "InterfaceBlock added: " << blockIdentifier.Name.c_str() << std::endl;
	
	// TODO: It is not optimal to loop through all materials each time a block is added when there
	// are probably just one or two blocks which contain material data. On the other hand, this 
	// function should not run very often.
	const std::map<VfsPath, CMaterial>& materials = g_Renderer.GetMaterialManager().GetAllMaterials();
	for (const auto& pathMatPair : materials)
		WriteMaterialValues(pathMatPair.second);
	
	int flags = 0;
	if (blockIdentifier.Name == m_PlayerColorBlockName)
	{
		flags |= PLAYER_COLOR;
		m_PlayerColorBinding = GetBinding(m_PlayerColorBlockName, str_playerColor_0, true);
	}
	else if (blockIdentifier.Name == m_ShadingColorBlockName)
	{
		flags |= SHADING_COLOR;
		m_ShadingColorBinding = GetBinding(m_ShadingColorBlockName, str_shadingColor_0, true);
	}
	else if (blockIdentifier.Name == m_MaterialIDBlockName)
	{
		flags |= MATERIAL_ID;
		m_MaterialIDBinding = GetBinding(m_MaterialIDBlockName, CStrIntern("materialID[0]"), true);
	}
	
	if(!flags)
		return;
	
	m_AvailableBindingsFlag |= flags;
	GenAllModelData(flags);
}


bool UniformBlockManager::EnsureBlockBinding(const CShaderProgramPtr& shader)
{

	// TODO: Check if there are enough free binding points
	
	const std::vector<InterfaceBlockIdentifier>& blockIdentifiers = shader->GetUniformBlockIdentifiers();

	for (const InterfaceBlockIdentifier& blockIdentifier : blockIdentifiers)
	{
		std::map<CStrIntern, PointBufferBinding>::iterator pointBinding;
		std::map<CStrIntern, PointBufferBinding>::iterator endItr;
		
		// TODO: Maybe another define could be use that starts with 0 and then that could be
		// used as index like this in an array of maps:
		//
		// Declaration:
		// enum PS_INTERFACE_BLOCK_TYPES { PS_UNIFORM_BLOCK, PS_SHADER_STORAGE_BLOCK };
		// std::map<CStrIntern, PointBufferBinding> m_PointBindings[2];
		//
		// Use:
		// m_PointBindings[PS_UNIFORM_BLOCK].find(blockIdentifier.Name);
		//
		// This should make the code a bit cleaner and remove some branching

		
		if (blockIdentifier.BlockType == GL_UNIFORM_BLOCK)
		{
			pointBinding = m_UBOPointBindings.find(blockIdentifier.Name);
			endItr = m_UBOPointBindings.end();
		}
		else if (blockIdentifier.BlockType == GL_SHADER_STORAGE_BLOCK)
		{
			pointBinding = m_SSBOPointBindings.find(blockIdentifier.Name);
			endItr = m_SSBOPointBindings.end();
		}
		
		if (pointBinding != endItr)
		{
			// check if the block in the shader is bound to the same point where the buffer is mapped
			if (shader->GetUniformBlockBindingPoint(blockIdentifier.BlockType, blockIdentifier.ID) == pointBinding->second.point)
				continue; // this block is correctly bound
			else // the binding point is mapped to the buffer, but the shader points to the wrong binding point
			{
				shader->UniformBlockBinding(blockIdentifier, pointBinding->second.point);
				//glUniformBlockBinding(shader, shader.GetBlockID(blockIdentifier.Name), pointBinding.second.point);
			}
		}
		else	// currently there's no buffer for this block attached to any points
		{
			const auto& nameIndexLookup  = m_InterfaceBlockIndices.find(blockIdentifier.Name);
			if (nameIndexLookup == m_InterfaceBlockIndices.end())
			{
				// All blocks of this shader should have been registered using RegisterUniformBlocks.
				// This code path should never be reached, unless there's a code issue
				// TODO: replace with debug_warn or ENSURE?
				std::cerr << "Trying to bind a uniform block which hasn't been loaded yet by the UniformBlockManager!";
				return false;
			}

			if (blockIdentifier.BlockType == GL_UNIFORM_BLOCK)
			{
				pglBindBufferBase(GL_UNIFORM_BUFFER, m_NextFreeUBOBindingPoint, m_InterfaceBlockBuffers[nameIndexLookup->second].m_UBOBufferID);
				m_UBOPointBindings.emplace(blockIdentifier.Name, PointBufferBinding { m_NextFreeUBOBindingPoint });
				shader->UniformBlockBinding(blockIdentifier, m_NextFreeUBOBindingPoint);
				m_NextFreeUBOBindingPoint++;
			} 
			else if (blockIdentifier.BlockType == GL_SHADER_STORAGE_BLOCK)
			{
				std::cout << "pglBindBufferBase: BindingPoint: " << m_NextFreeSSBOBindingPoint << " BufferName: " << m_InterfaceBlockBuffers[nameIndexLookup->second].m_BlockName.c_str() << " BufferID: " << m_InterfaceBlockBuffers[nameIndexLookup->second].m_UBOBufferID << std::endl;
				pglBindBufferBase(GL_SHADER_STORAGE_BUFFER, m_NextFreeSSBOBindingPoint, m_InterfaceBlockBuffers[nameIndexLookup->second].m_UBOBufferID);
				m_SSBOPointBindings.emplace(blockIdentifier.Name, PointBufferBinding { m_NextFreeSSBOBindingPoint });
				shader->UniformBlockBinding(blockIdentifier, m_NextFreeSSBOBindingPoint);
				m_NextFreeSSBOBindingPoint++;
			}
		}
	}
}