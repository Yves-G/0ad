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

#ifndef INCLUDED_UNIFORMBUFFER
#define INCLUDED_UNIFORMBUFFER

#include <iostream>
#include <vector>
#include <memory>
#include <algorithm>

#include "lib/ogl.h"


#include "maths/Matrix3D.h"
#include "maths/Vector2D.h"
#include "ps/Shapes.h" // CColor
#include "ps/CStrIntern.h"
#include "graphics/UniformBinding.h"

//#include "ps/Profile.h"

class CShaderProgram;
//class UniformBinding;

#define  DEBUG_UNIFORM_BUFFER 0

/**
 * A uniform buffer holds data for one type of uniform block which can be used across multiple different shaders.
 * The buffer gets prepared in RAM by a series of calls to SetUniform and then the whole buffer data is uploaded 
 * to the uniform buffer on the GPU.
 *
 */
class UniformBuffer
{
public:
	UniformBuffer(GLuint program, const UniformBlockIdentifier& blockIdentifier);
	
	// A uniform buffer owns memory. It can be moved but not copied.
	UniformBuffer(const UniformBuffer&) = delete;
	UniformBuffer(UniformBuffer&&) = default;
	
	static void GetBlockIdentifiers(GLuint program, std::vector<UniformBlockIdentifier>& out);
	
	/**
	 * @param id: id.first contains a number identifying the uniform in the block 
	 * (between 0 and the number of uniforms in this block). 
	 */
	
	 /*
	void SetUniform(Binding id, GLuint instanceID, CMatrix3D matrix);
	void SetUniform(Binding id, CMatrix3D matrix);
	void SetUniform(Binding id, float v);
	*/
	
	// TODO: More templated?
	inline void SetUniform(const UniformBinding& id, CColor& v, GLuint instanceId);
	inline void SetUniform(const UniformBinding& id, CMatrix3D matrix, GLuint instanceId);
	void SetUniform(const UniformBinding& id, CVector4D v, GLuint instanceId);
	void SetUniform(const UniformBinding& id, CVector3D v, GLuint instanceId);
	void SetUniform(const UniformBinding& id, CVector2D v, GLuint instanceId);
	void SetUniform(const UniformBinding& id, float v, GLuint instanceId);
	inline void SetUniform(const UniformBinding& id, GLuint v, GLuint instanceId);
	
	/**
	 * Sets an instanced array of matrices (array of array of matrices).
	 */
	void SetUniform(const UniformBinding& id, size_t count, const CMatrix3D* v, GLuint instanceId);
	
	/**
	 * Always takes 4 float values, but converts them according to the type determined by id and calls 
	 * the appropriate SetUniform overload.
	 * Supported types: float, vec2, vec3 and vec4
	 */
	void SetUniformF4(UniformBinding id, float v0, float v1, float v2, float v3, GLuint instanceId);
	
	void Upload();
	
	void GetBinding(UniformBinding& id, CStrIntern uniformName) const;
	
	GLuint m_UBOBufferID; // TODO: should be private in the final code

private:
	GLint m_UBOBlockSize;
	std::unique_ptr<GLubyte> m_UBOSourceBuffer;
	
	std::vector<CStrIntern> m_UniformNames;
	std::vector<int> m_UniformTypes;
	std::vector<GLuint> m_UniformIndices;
	std::vector<GLint> m_UniformOffsets;
	std::vector<GLint> m_ArrayStrides;
	std::vector<GLint> m_MatrixStrides;
	
	GLint m_UBODirtyBytes; // upper bound of modified content

	CStrIntern m_BlockName;
};

inline void UniformBuffer::SetUniform(const UniformBinding& id, CColor& v, GLuint instanceId)
{
	//PROFILE3("SetUniform (CColor&)");
		
	GLuint offset = m_UniformOffsets[id.m_UniformId] + m_ArrayStrides[id.m_UniformId] * instanceId;
#if DEBUG_UNIFORM_BUFFER
	for (int k = 0; k < sizeof(float); ++k)
	{
		ENSURE(*(m_UBOSourceBuffer.get() + offset + 0 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.get() + offset + 1 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.get() + offset + 2 * sizeof(float) + k) == 0xFD);
	}
#endif // DEBUG_UNIFORM_BUFFER
	*((float*)(m_UBOSourceBuffer.get() + offset + 0 * sizeof(float))) = v.r;
	*((float*)(m_UBOSourceBuffer.get() + offset + 1 * sizeof(float))) = v.g;
	*((float*)(m_UBOSourceBuffer.get() + offset + 2 * sizeof(float))) = v.b;
	
	// TODO: What if we need the alpha-channel?
	//*((float*)(m_UBOSourceBuffer.get() + offset + 3 * sizeof(float))) = v.a;
	
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(offset + 3 * sizeof(float)));
	ENSURE(m_UBOBlockSize >= m_UBODirtyBytes);
}

inline void UniformBuffer::SetUniform(const UniformBinding& id, GLuint v, GLuint instanceId)
{
	//PROFILE3("SetUniform (GLuint)");
		
	GLuint offset = m_UniformOffsets[id.m_UniformId] + m_ArrayStrides[id.m_UniformId] * instanceId;
#if DEBUG_UNIFORM_BUFFER
	for (int k = 0; k < sizeof(GLuint); ++k)
		ENSURE(*(m_UBOSourceBuffer.get() + offset + k) == 0xFD);
#endif // DEBUG_UNIFORM_BUFFER
	*((GLuint*)(m_UBOSourceBuffer.get() + offset)) = v;
	
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(offset + 1 * sizeof(GLuint)));
	ENSURE(m_UBOBlockSize >= m_UBODirtyBytes);
}

inline void UniformBuffer::SetUniform(const UniformBinding& id, CMatrix3D matrix, GLuint instanceId)
{
	//PROFILE3("SetUniform (CMatrix3D)");
	/*
	GLuint offset;
	for (int i=0; i<4; ++i)
	{
		offset = m_UniformOffsets[id.m_UniformId] + m_MatrixStrides[id.m_UniformId] * i;
		for (int j=0; j<4; ++j)
		{
#if DEBUG_UNIFORM_BUFFER
			for (int k = 0; k < sizeof(float); ++k)
				ENSURE(*(m_UBOSourceBuffer.get() + m_ArrayStrides[id.m_UniformId] * instanceId + offset + k) == 0xFD);
#endif // DEBUG_UNIFORM_BUFFER
			*((float*)(m_UBOSourceBuffer.get() + m_ArrayStrides[id.m_UniformId] * instanceId + offset)) = matrix[i * 4 + j];
			offset += sizeof(GLfloat);
		}	
	}
	
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(m_ArrayStrides[id.m_UniformId] * instanceId + offset));
	*/
	
	GLubyte* dstPtr = m_UBOSourceBuffer.get() + m_UniformOffsets[id.m_UniformId] + m_ArrayStrides[id.m_UniformId] * instanceId;
	for (int i=0; i<4; ++i)
	{
		for (int j=0; j<4; ++j)
			*(float*)(dstPtr + j * sizeof(GLfloat)) = matrix[i * 4 + j];
		dstPtr += m_MatrixStrides[id.m_UniformId];
	}
	
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(dstPtr - m_UBOSourceBuffer.get()));
	/*ENSURE(m_UBOBlockSize >= m_UBODirtyBytes);
	*/
}


#endif // INCLUDED_UNIFORMBUFFER