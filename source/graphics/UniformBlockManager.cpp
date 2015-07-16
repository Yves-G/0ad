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
#include "graphics/ShaderProgram.h"

void UniformBlockManager::RegisterUniformBlocks(const CShaderProgram& shader)
{
	const std::vector<UniformBlockIdentifier>& blockIdentifiers = shader.GetUniformBlockIdentifiers();
	
	for (const UniformBlockIdentifier& blockIdentifier : blockIdentifiers)
	{
		const auto& nameIndexLookup = m_UniformBufferIndices.find(blockIdentifier.Name);
		if (nameIndexLookup != m_UniformBufferIndices.end())
			continue; // block already added
		
		// No such uniform block available so far, create one from the current shader program
		m_UniformBuffers.emplace_back(shader.GetProgram(), blockIdentifier);
		m_DirtyBuffers.resize(m_UniformBuffers.size());
		m_UniformBufferIndices.emplace(blockIdentifier.Name, m_UniformBuffers.size() - 1);
	}
}


bool UniformBlockManager::EnsureBlockBinding(const CShaderProgramPtr& shader)
{

	// TODO: Check if there are enough free binding points
	
	const std::vector<UniformBlockIdentifier>& blockIdentifiers = shader->GetUniformBlockIdentifiers();

	for (const UniformBlockIdentifier& blockIdentifier : blockIdentifiers)
	{
		const auto& pointBinding = m_PointBindings.find(blockIdentifier.Name);
		if (pointBinding != m_PointBindings.end())
		{
			// check if the block in the shader is bound to the same point where the buffer is mapped
			if (shader->GetUniformBlockBindingPoint(blockIdentifier.ID) == pointBinding->second.point)
				continue; // this block is correctly bound
			else // the binding point is mapped to the buffer, but the shader points to the wrong binding point
			{
				shader->UniformBlockBinding(blockIdentifier, pointBinding->second.point);
				//glUniformBlockBinding(shader, shader.GetBlockID(blockIdentifier.Name), pointBinding.second.point);
			}
		}
		else	// currently there's no buffer for this block attached to any points
		{
			const auto& nameIndexLookup  = m_UniformBufferIndices.find(blockIdentifier.Name);
			if (nameIndexLookup == m_UniformBufferIndices.end())
			{
				// All blocks of this shader should have been registered using RegisterUniformBlocks.
				// This code path should never be reached, unless there's a code issue
				// TODO: replace with debug_warn or ENSURE?
				std::cerr << "Trying to bind a uniform block which hasn't been loaded yet by the UniformBlockManager!";
				return false;
			}

			pglBindBufferBase(GL_UNIFORM_BUFFER, m_NextFreeBindingPoint, m_UniformBuffers[nameIndexLookup->second].m_UBOBufferID);
			m_PointBindings.emplace(blockIdentifier.Name, PointBufferBinding { m_NextFreeBindingPoint });
			m_NextFreeBindingPoint++;
		}
	}
}