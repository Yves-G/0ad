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
class InterfaceBlock
{
public:
	InterfaceBlock(GLuint program, const InterfaceBlockIdentifier& blockIdentifier, const int InterfaceBlockType);
	
	// A uniform buffer owns memory. It can be moved but not copied.
	InterfaceBlock(const InterfaceBlock&) = delete;
	// TODO: Use "= default" for this move constructor when all supported Visual Studio versions support it
	InterfaceBlock(InterfaceBlock&&);
	
	//template <int BLOCKTYPE>
	//static void GetBlockIdentifiers(GLuint program, std::vector<InterfaceBlockIdentifier>& out);
	static void GetBlockIdentifiers(GLuint program, std::vector<InterfaceBlockIdentifier>& out,
		int& numUBOBlocks, int& numSSBOBlocks);
	
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
	int m_InterfaceBlockType;
	int m_MemberType;
	int m_MemberBufferType;
	
	GLint m_UBOBlockSize;
	bool m_BufferResized;
	
	// TODO: Using a vector is most convenient for resizing the buffer. However, I'm not sure if using resize could
	// cause the vector capacity to be significantly larger than the requested size. In this use case, we should control
	// when we need to acquire how much more memory and not a generic vector class.
	std::vector<GLubyte> m_UBOSourceBuffer;
	//std::unique_ptr<GLubyte> m_UBOSourceBuffer;
	
	std::vector<CStrIntern> m_UniformNames;
	std::vector<int> m_UniformTypes;
	std::vector<GLuint> m_UniformIndices;
	
	static const GLenum MemberProps[];
	enum PROPS { PROP_OFFSET, PROP_ARRAY_STRIDE, PROP_MATRIX_STRIDE, COUNT };
	std::vector<GLint> m_MemberProps;
	
	// The data is laid out so that data for one member are stored together.
	// This might theoretically improve caching
	inline GLint GetMemberProp(int memberIx, PROPS prop) { return m_MemberProps[memberIx * PROPS::COUNT + prop]; }
	
	void IncreaseBufferSize(GLint minRequiredBlockSize)
	{
		// Increase buffer size to the required minimum block size, but at least by 10%
		m_UBOBlockSize = std::max(GLint(m_UBOBlockSize * 1.1), minRequiredBlockSize);
		m_UBOSourceBuffer.resize(m_UBOBlockSize);
		m_BufferResized = true;
	}
	
	GLint m_UBODirtyBytes; // upper bound of modified content
public:
	CStrIntern m_BlockName;
};

inline void InterfaceBlock::SetUniform(const UniformBinding& id, CColor& v, GLuint instanceId)
{
	//PROFILE3("SetUniform (CColor&)");
	GLuint offset = GetMemberProp(id.m_UniformId, PROPS::PROP_OFFSET) + GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE) * instanceId;
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(offset + 3 * sizeof(float)));
	
	if (m_UBOBlockSize < m_UBODirtyBytes)
		IncreaseBufferSize(m_UBODirtyBytes);

#if DEBUG_UNIFORM_BUFFER
	for (int k = 0; k < sizeof(float); ++k)
	{
		ENSURE(*(m_UBOSourceBuffer.data() + offset + 0 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.data() + offset + 1 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.data() + offset + 2 * sizeof(float) + k) == 0xFD);
	}
#endif // DEBUG_UNIFORM_BUFFER
	*((float*)(m_UBOSourceBuffer.data() + offset + 0 * sizeof(float))) = v.r;
	*((float*)(m_UBOSourceBuffer.data() + offset + 1 * sizeof(float))) = v.g;
	*((float*)(m_UBOSourceBuffer.data() + offset + 2 * sizeof(float))) = v.b;
	
	// TODO: What if we need the alpha-channel?
	//*((float*)(m_UBOSourceBuffer.data() + offset + 3 * sizeof(float))) = v.a;
}

inline void InterfaceBlock::SetUniform(const UniformBinding& id, GLuint v, GLuint instanceId)
{
	//PROFILE3("SetUniform (GLuint)");
	GLuint offset = GetMemberProp(id.m_UniformId, PROPS::PROP_OFFSET) + GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE) * instanceId;
	
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(offset + 1 * sizeof(GLuint)));

	if (m_UBOBlockSize < m_UBODirtyBytes)
		IncreaseBufferSize(m_UBODirtyBytes);

#if DEBUG_UNIFORM_BUFFER
	for (int k = 0; k < sizeof(GLuint); ++k)
		ENSURE(*(m_UBOSourceBuffer.data() + offset + k) == 0xFD);
#endif // DEBUG_UNIFORM_BUFFER
	*((GLuint*)(m_UBOSourceBuffer.data() + offset)) = v;
}

inline void InterfaceBlock::SetUniform(const UniformBinding& id, CMatrix3D matrix, GLuint instanceId)
{
	//PROFILE3("SetUniform (CMatrix3D)");
	/*
	GLuint offset;
	for (int i=0; i<4; ++i)
	{
		offset = GetMemberProp(id.m_UniformId, PROPS::PROP_OFFSET) + GetMemberProp(id.m_UniformId, PROPS::PROP_MATRIX_STRIDE) * i;
		for (int j=0; j<4; ++j)
		{
#if DEBUG_UNIFORM_BUFFER
			for (int k = 0; k < sizeof(float); ++k)
				ENSURE(*(m_UBOSourceBuffer.data() + GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE) * instanceId + offset + k) == 0xFD);
#endif // DEBUG_UNIFORM_BUFFER
			*((float*)(m_UBOSourceBuffer.data() + GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE) * instanceId + offset)) = matrix[i * 4 + j];
			offset += sizeof(GLfloat);
		}	
	}
	
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE) * instanceId + offset));
	*/
	GLuint offset = GetMemberProp(id.m_UniformId, PROPS::PROP_OFFSET) + GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE) * instanceId;
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(GetMemberProp(id.m_UniformId, PROPS::PROP_OFFSET) + GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE) * (instanceId + 1)));
	
	if (m_UBOBlockSize < m_UBODirtyBytes)
		IncreaseBufferSize(m_UBODirtyBytes);

	for (int i=0; i<4; ++i)
	{
		for (int j=0; j<4; ++j)
			*(float*)(m_UBOSourceBuffer.data() + offset + j * sizeof(GLfloat)) = matrix[i * 4 + j];
		offset += GetMemberProp(id.m_UniformId, PROPS::PROP_MATRIX_STRIDE);
	}
}


#endif // INCLUDED_UNIFORMBUFFER