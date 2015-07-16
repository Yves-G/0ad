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
 
#include "UniformBuffer.h"

#include "ShaderProgram.h"
#include "ShaderProgramPtr.h"

#include "ps/Profile.h"

UniformBuffer::UniformBuffer(GLuint program, const UniformBlockIdentifier& blockIdentifier) :
	m_UBOSourceBuffer(nullptr),
	m_BlockName(blockIdentifier.Name),
	m_UBODirtyBytes(0)
{
		GLint numberOfUniformsInBlock;
		pglGetActiveUniformBlockiv(program, blockIdentifier.ID, GL_UNIFORM_BLOCK_ACTIVE_UNIFORMS, &numberOfUniformsInBlock);
		
		m_UniformIndices.resize(numberOfUniformsInBlock);
		m_UniformOffsets.resize(numberOfUniformsInBlock);
		m_ArrayStrides.resize(numberOfUniformsInBlock);
		m_MatrixStrides.resize(numberOfUniformsInBlock);
		
		pglGetActiveUniformBlockiv(program, blockIdentifier.ID, GL_UNIFORM_BLOCK_ACTIVE_UNIFORM_INDICES, (GLint*)&m_UniformIndices[0]);
		for (GLuint uniformIndex : m_UniformIndices)
		{
			GLint uniformNameLength;
			pglGetActiveUniformsiv(program, 1, &uniformIndex, GL_UNIFORM_NAME_LENGTH, &uniformNameLength);
			//glGetActiveUniformName(program, uniformIndex, 1, &uniformNameLength, nullptr);
			std::vector<GLchar> uniformName(uniformNameLength);
			//glGetActiveUniformName(program, uniformIndex, 1, nullptr, &uniformName[0]);
			GLint arraySize = 0;
			GLenum type = 0;
  			GLsizei actualLength = 0;
			pglGetActiveUniformARB(program, uniformIndex, uniformNameLength, &actualLength, &arraySize, &type, &uniformName[0]);
			// uniform names have the form blockName.uniformName, but we just want the uniformName part
			auto it = std::find(uniformName.begin(), uniformName.end(), '.');
			if (it == uniformName.end())
				it = uniformName.begin();
			else
				it++; // one char after the dot
			m_UniformNames.emplace_back(std::string(it, uniformName.end() - 1));
			m_UniformTypes.emplace_back(type);
		}
		
		pglGetActiveUniformsiv(program, numberOfUniformsInBlock, &m_UniformIndices[0], GL_UNIFORM_OFFSET, &m_UniformOffsets[0]);
		pglGetActiveUniformsiv(program, numberOfUniformsInBlock, &m_UniformIndices[0], GL_UNIFORM_ARRAY_STRIDE, &m_ArrayStrides[0]);
		pglGetActiveUniformsiv(program, numberOfUniformsInBlock, &m_UniformIndices[0], GL_UNIFORM_MATRIX_STRIDE, &m_MatrixStrides[0]);
		
		pglGetActiveUniformBlockiv(program, blockIdentifier.ID, GL_UNIFORM_BLOCK_DATA_SIZE, &m_UBOBlockSize);
		m_UBOSourceBuffer = std::unique_ptr<GLubyte>(new GLubyte[m_UBOBlockSize]);
	#if DEBUG_UNIFORM_BUFFER
		memset(m_UBOSourceBuffer.get(), 0xFD, m_UBOBlockSize);
	#endif // DEBUG_UNIFORM_BUFFER
		pglGenBuffersARB(1, &m_UBOBufferID);
		pglBindBufferARB(GL_UNIFORM_BUFFER, m_UBOBufferID);
		pglBufferDataARB(GL_UNIFORM_BUFFER, m_UBOBlockSize, NULL, GL_DYNAMIC_DRAW);
}

void UniformBuffer::GetBlockIdentifiers(GLuint program, std::vector<UniformBlockIdentifier>& out)
{
	GLint numBlocks;
	//GLuint program = shader.GetProgram();
	pglGetProgramiv(program, GL_ACTIVE_UNIFORM_BLOCKS, &numBlocks);
	for(GLuint blockId = 0; blockId < numBlocks; ++blockId)
	{
		GLint nameLen;
		pglGetActiveUniformBlockiv(program, blockId, GL_UNIFORM_BLOCK_NAME_LENGTH, &nameLen);
		
		std::vector<GLchar> name;
		name.resize(nameLen);
		pglGetActiveUniformBlockName(program, blockId, nameLen, NULL, &name[0]);

		out.push_back(UniformBlockIdentifier { blockId, CStrIntern(&name[0]) }); //Remove the null terminator.
	}
}

/*
void UniformBuffer::SetUniform(Binding id, GLuint instanceID, CMatrix3D matrix)
{
	for (int i=0; i<4; ++i)
	{
		GLuint offset = m_UniformOffsets[id.second] + m_MatrixStrides[id.second] * i;
		for (int j=0; j<4; ++j)
		{
			*((float*)(m_UBOSourceBuffer.get() + m_ArrayStrides[id.second] * instanceID + offset)) = matrix[i * 4 + j];
			offset += sizeof(GLfloat);
		}	
	}
}

void UniformBuffer::SetUniform(Binding id, CMatrix3D matrix)
{
	for (int i=0; i<4; ++i)
	{
		GLuint offset = m_UniformOffsets[id.second] + m_MatrixStrides[id.second] * i;
		for (int j=0; j<4; ++j)
		{
			*((float*)(m_UBOSourceBuffer.get() + offset)) = matrix[i * 4 + j];
			offset += sizeof(GLfloat);
		}	
	}
}*/

// TODO: There's a lot of duplicated code here.
// Maybe the old approach of just passing 4 floats would be better
void UniformBuffer::SetUniform(const UniformBinding& id, CVector4D v, GLuint instanceId)
{
	GLuint offset = m_UniformOffsets[id.m_UniformId] + m_ArrayStrides[id.m_UniformId] * instanceId;
#if DEBUG_UNIFORM_BUFFER
	for (int k = 0; k < sizeof(float); ++k)
	{
		ENSURE(*(m_UBOSourceBuffer.get() + offset + 0 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.get() + offset + 1 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.get() + offset + 2 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.get() + offset + 3 * sizeof(float) + k) == 0xFD);
	}
#endif // DEBUG_UNIFORM_BUFFER
	*((float*)(m_UBOSourceBuffer.get() + offset + 0 * sizeof(float))) = v.X;
	*((float*)(m_UBOSourceBuffer.get() + offset + 1 * sizeof(float))) = v.Y;
	*((float*)(m_UBOSourceBuffer.get() + offset + 2 * sizeof(float))) = v.Z;
	*((float*)(m_UBOSourceBuffer.get() + offset + 3 * sizeof(float))) = v.W;
	
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(offset + 4 * sizeof(float)));
	ENSURE(m_UBOBlockSize >= m_UBODirtyBytes);
}

void UniformBuffer::SetUniform(const UniformBinding& id, CVector3D v, GLuint instanceId)
{
	GLuint offset = m_UniformOffsets[id.m_UniformId] + m_ArrayStrides[id.m_UniformId] * instanceId;
#if DEBUG_UNIFORM_BUFFER
	for (int k = 0; k < sizeof(float); ++k)
	{
		ENSURE(*(m_UBOSourceBuffer.get() + offset + 0 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.get() + offset + 1 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.get() + offset + 2 * sizeof(float) + k) == 0xFD);
	}
#endif // DEBUG_UNIFORM_BUFFER
	*((float*)(m_UBOSourceBuffer.get() + offset + 0 * sizeof(float))) = v.X;
	*((float*)(m_UBOSourceBuffer.get() + offset + 1 * sizeof(float))) = v.Y;
	*((float*)(m_UBOSourceBuffer.get() + offset + 2 * sizeof(float))) = v.Z;
	
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(offset + 3 * sizeof(float)));
	ENSURE(m_UBOBlockSize >= m_UBODirtyBytes);
}

void UniformBuffer::SetUniform(const UniformBinding& id, CVector2D v, GLuint instanceId)
{
	GLuint offset = m_UniformOffsets[id.m_UniformId] + m_ArrayStrides[id.m_UniformId] * instanceId;
#if DEBUG_UNIFORM_BUFFER
	for (int k = 0; k < sizeof(float); ++k)
	{
		ENSURE(*(m_UBOSourceBuffer.get() + offset + 0 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.get() + offset + 1 * sizeof(float) + k) == 0xFD);
	}
#endif // DEBUG_UNIFORM_BUFFER
	*((float*)(m_UBOSourceBuffer.get() + offset + 0 * sizeof(float))) = v.X;
	*((float*)(m_UBOSourceBuffer.get() + offset + 1 * sizeof(float))) = v.Y;
	
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(offset + 2 * sizeof(float)));
	ENSURE(m_UBOBlockSize >= m_UBODirtyBytes);
}

void UniformBuffer::SetUniform(const UniformBinding& id, size_t count, const CMatrix3D* v, GLuint instanceId)
{	
	//   mat0  mat1  mat2  mat3 ..64
	// [[0123][0123][0123][0123]....]  Inst0
	// [[0123][0123][0123][0123]....]  Inst1
	// [[0123][0123][0123][0123]....]  Inst2
	// [[0123][0123][0123][0123]....]  Inst3
	// [[0123][0123][0123][0123]....]  Inst4
	// ... 
	CMatrix3D* ptr = const_cast<CMatrix3D*>(v);
	GLubyte* dstPtr = (m_UBOSourceBuffer.get() + m_UniformOffsets[id.m_UniformId]);
	// TODO: 64 is MAX_BONES bones in the shader... need a way to query this
	dstPtr += 64 * m_ArrayStrides[id.m_UniformId] * instanceId; // point to current instance
	
	for (int h=0; h<count; ++h) // current matrix
	{		
		for (int i=0; i<4; ++i) // current matrix column/row of the current matrix
		{
			GLuint offset = m_MatrixStrides[id.m_UniformId] * i;
			for (int j=0; j<4; ++j)
			{
				float value = (*ptr)[i * 4 + j];
				
#if DEBUG_UNIFORM_BUFFER
				for (int k = 0; k < sizeof(float); ++k)
					ENSURE(*(dstPtr + offset + k) == 0xFD);
#endif // DEBUG_UNIFORM_BUFFER

				*((float*)(dstPtr + offset)) = value;
				offset += sizeof(GLfloat);
			}	
		}
		ptr++; // points to the current matrix in src
		dstPtr += m_ArrayStrides[id.m_UniformId];
		if (h == 63)
			break;
	}
	
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(dstPtr - m_UBOSourceBuffer.get()));
	ENSURE(m_UBOBlockSize >= m_UBODirtyBytes);
	/*
	if (id.first != -1)
	{
		if (id.second == GL_FLOAT_MAT4)
			pglUniformMatrix4fvARB(id.first, count, GL_FALSE, &v->_11);
		else
			LOGERROR("CShaderProgramGLSL::Uniform(): Invalid uniform type (expected mat4)");
	}*/
}

void UniformBuffer::SetUniform(const UniformBinding& id, float v, GLuint instanceId)
{
	GLuint offset = m_UniformOffsets[id.m_UniformId] + m_ArrayStrides[id.m_UniformId] * instanceId;
	
#if DEBUG_UNIFORM_BUFFER
for (int i = 0; i < sizeof(float); ++i)
	ENSURE(*(m_UBOSourceBuffer.get() + offset + i) == 0xFD);
#endif // DEBUG_UNIFORM_BUFFER
	*((float*)(m_UBOSourceBuffer.get() + offset)) = v;
	
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(offset + 1 * sizeof(float)));
	ENSURE(m_UBOBlockSize >= m_UBODirtyBytes);
}

/*
void UniformBuffer::SetUniform(Binding id, float v)
{
	GLuint offset = m_UniformOffsets[id.second];
	*((float*)(m_UBOSourceBuffer.get() + offset)) = v;
}

void UniformBuffer::SetUniform(Binding id, GLuint instanceID, float v)
{
	GLuint offset = m_UniformOffsets[id.second];
	*((float*)(m_UBOSourceBuffer.get() + offset + m_ArrayStrides[id.second] * instanceID)) = v;
}*/

void UniformBuffer::SetUniformF4(UniformBinding id, float v0, float v1, float v2, float v3, GLuint instanceId)
{
	if (id.m_Type == GL_FLOAT)
		SetUniform(id, v0, instanceId);
	else if (id.m_Type == GL_FLOAT_VEC2)
		SetUniform(id, CVector2D(v0, v1), instanceId);//pglUniform2fARB(id.m_Type, v0, v1);
	else if (id.m_Type == GL_FLOAT_VEC3)
		SetUniform(id, CVector3D(v0, v1, v2), instanceId); //pglUniform3fARB(id.m_Type, v0, v1, v2);
	else if (id.m_Type == GL_FLOAT_VEC4)
		SetUniform(id, CVector4D(v0, v1, v2, v3), instanceId); //pglUniform4fARB(id.m_Type, v0, v1, v2, v3);*/
	else
		LOGERROR("CShaderProgramGLSL::Uniform(): Invalid uniform type (expected float, vec2, vec3, vec4)");
}

void UniformBuffer::Upload()
{
	ENSURE(m_UBOBlockSize >= m_UBODirtyBytes);
	pglBindBufferARB(GL_UNIFORM_BUFFER, m_UBOBufferID);
	pglBufferSubDataARB(GL_UNIFORM_BUFFER, 0, m_UBODirtyBytes, m_UBOSourceBuffer.get());
	m_UBODirtyBytes = 0;
#if DEBUG_UNIFORM_BUFFER
	memset(m_UBOSourceBuffer.get(), 0xFD, m_UBOBlockSize);
#endif // DEBUG_UNIFORM_BUFFER
}

/**
 * Gets a binding from a uniform name
 * The idea is to call GetBinding once and then use only the binding because that's faster.
 */
// TODO: check GetUniformVertexIndex in ShaderProgram.cpp.
// It might make sense to store to use an std::map for the mapping too
void UniformBuffer::GetBinding(UniformBinding& id, CStrIntern uniformName) const
{
	for (CStrIntern name : m_UniformNames)
	{
		const char* cName = name.c_str();
		int x = 0;
	}
	auto it = std::find(m_UniformNames.begin(), m_UniformNames.end(), uniformName);
	if (it == m_UniformNames.end())
	{
		// TODO: Better error handling. Uniforms can be disabled due to shader defines, so
		// it's not necessarily an error if we can't find the uniform binding.
		//std::cerr << "ERROR: Unknown uniform name." << uniformName.c_str() << std::endl;
		return;
	}

	int pos = std::distance(m_UniformNames.begin(), it);
	id.m_UniformId = pos; // TODO: third (type) not used yet
	id.m_Type = m_UniformTypes[pos];
}
