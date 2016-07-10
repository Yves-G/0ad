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

#ifndef INCLUDED_MULTIDRAWINDIRECTCOMMANDS
#define INCLUDED_MULTIDRAWINDIRECTCOMMANDS

#include <vector>

#include "lib/ogl.h"

#define MAX_INSTANCING_DRAWIDS	2000

class MultiDrawIndirectCommands
{
public:
	MultiDrawIndirectCommands() :
		m_CurrentDrawID(0),
		m_DrawCommands(MAX_INSTANCING_DRAWIDS)
	 {
	 	pglGenBuffersARB(1, &m_BufferID);
	 }

	struct DrawElementsIndirectCommand
	{
		GLuint  count;
		GLuint  instanceCount;
		GLuint  firstIndex;
		GLuint  baseVertex;
		GLuint  baseInstance;
	};
	
	/**
	 * Discard all commands in order to start creating new ones
	 */
	void ResetCommands()
	{
		m_DrawCommands.clear();
		m_DrawCommands.reserve(MAX_INSTANCING_DRAWIDS);
	}
	
	/**
	 * We use the current 
	 */
	void ResetDrawID()
	{
		m_CurrentDrawID = 0;
	}

	void AddCommand(GLuint primCount, GLuint instanceCount, GLuint firstIndex, GLuint baseVertex)
	{
		m_DrawCommands.push_back(DrawElementsIndirectCommand { primCount, instanceCount, firstIndex, baseVertex, m_CurrentDrawID });
		m_CurrentDrawID++;
	}
	
	/**
	 * Increments the instanceCount of the last command added
	 */
	void AddInstance()
	{
		m_DrawCommands.back().instanceCount++;
		m_CurrentDrawID++;
	}

	void BindAndUpload()
	{
		pglBindBufferARB(GL_DRAW_INDIRECT_BUFFER, m_BufferID);
		pglBufferDataARB(GL_DRAW_INDIRECT_BUFFER, m_DrawCommands.size() * sizeof(DrawElementsIndirectCommand), &m_DrawCommands[0], GL_STATIC_DRAW);
		m_CurrentDrawID = 0;
	}

	size_t GetCommandCount()
	{
		return m_DrawCommands.size();
	}
	
	void Draw(u32 modelsCount)
	{
			ENSURE(m_DrawCommands.size() >= modelsCount);
			ENSURE(m_DrawCommands.size() > m_CurrentDrawID);
			ENSURE(m_DrawCommands[m_CurrentDrawID].instanceCount != 0);
			ENSURE(m_DrawCommands[m_CurrentDrawID].count > 3);

			//pglDrawRangeElementsEXT(GL_TRIANGLES, 0, (GLuint)m->imodeldef->m_Array.GetNumVertices()-1,
			//	(GLsizei)numFaces*3, GL_UNSIGNED_SHORT, m->imodeldefIndexBase);
			ogl_WarnIfError();
			pglMultiDrawElementsIndirect(GL_TRIANGLES,
			GL_UNSIGNED_SHORT,
  			(void*)(m_CurrentDrawID * sizeof(DrawElementsIndirectCommand)),
  			modelsCount,
  			sizeof(DrawElementsIndirectCommand));
  			ogl_WarnIfError();
  			m_CurrentDrawID += modelsCount;
	}

private:
	std::vector<DrawElementsIndirectCommand> m_DrawCommands;
	GLuint m_BufferID;
	
	// A drawID is used by the shaders to access instanced unforms.
	// NOTE: This is independent of how many instances we can draw per
	// command. It just depends on how many instanced uniforms are stored
	// in uniform blocks, which is nothing we control inside this class.
	GLuint m_CurrentDrawID;
};

#endif // INCLUDED_MULTIDRAWINDIRECTCOMMANDS
