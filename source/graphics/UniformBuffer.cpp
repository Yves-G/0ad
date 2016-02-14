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

#include "ps/CLogger.h"
#include "ps/Profile.h"

#include <fstream>

constexpr GLenum InterfaceBlock::MemberProps[];

InterfaceBlock::InterfaceBlock(GLuint program, const InterfaceBlockIdentifier& blockIdentifier, const int interfaceBlockType) :
	m_UBOSourceBuffer(),
	m_BlockName(blockIdentifier.Name),
	m_UBODirtyBytes(0),
	m_InterfaceBlockType(interfaceBlockType),
	m_BufferResized(false)
{
	if (interfaceBlockType == GL_UNIFORM_BLOCK)
	{
		m_MemberBufferType = GL_UNIFORM_BUFFER;
		m_MemberType = GL_UNIFORM;
	}
	else if (interfaceBlockType == GL_SHADER_STORAGE_BLOCK)
	{
		m_MemberBufferType = GL_SHADER_STORAGE_BUFFER;
		m_MemberType = GL_BUFFER_VARIABLE;
	}
	
	
	const GLenum blockProperties[1] = { GL_NUM_ACTIVE_VARIABLES };
	const GLenum activeUnifProp[1] = { GL_ACTIVE_VARIABLES };
	const int numTempMembers = 2;
	const GLenum tmpMemberProps[numTempMembers] { GL_NAME_LENGTH, GL_TYPE };
	GLint numActiveUnifs = 0;
	
	pglGetProgramResourceiv(program, interfaceBlockType, blockIdentifier.ID, 1, blockProperties, 1, NULL, &numActiveUnifs);

	if(!numActiveUnifs)
		return; // TODO: error handling
		
	m_UniformIndices.resize(numActiveUnifs);
	pglGetProgramResourceiv(program, interfaceBlockType, blockIdentifier.ID, 1, activeUnifProp, numActiveUnifs, NULL, (GLint*)&m_UniformIndices[0]);
	
	m_MemberProps.resize(numActiveUnifs * PROPS::COUNT);
	std::vector<GLint> tmpMemberValues(numTempMembers);
	for(int unifIx = 0; unifIx < numActiveUnifs; ++unifIx)
	{
		pglGetProgramResourceiv(program, m_MemberType,
			m_UniformIndices[unifIx], PROPS::COUNT,
			MemberProps, PROPS::COUNT,
			NULL, &m_MemberProps[unifIx * PROPS::COUNT]);
		
		pglGetProgramResourceiv(program, m_MemberType,
			m_UniformIndices[unifIx], numTempMembers,
			tmpMemberProps, numTempMembers,
			NULL, &tmpMemberValues[0]);
			
		m_UniformTypes.emplace_back(tmpMemberValues[1]);
		
		std::vector<char> nameData(tmpMemberValues[0]);
		pglGetProgramResourceName(program, m_MemberType, m_UniformIndices[unifIx], nameData.size(), NULL, &nameData[0]);
		
		// uniform names have the form blockName.uniformName, but we just want the uniformName part
		auto it = std::find(nameData.begin(), nameData.end(), '.');
		if (it == nameData.end())
			it = nameData.begin();
		else
			it++; // one char after the dot
		
		m_UniformNames.emplace_back(std::string(it, nameData.end() - 1));
	}
		
	const GLenum bufferDataSizeProperty[1] = { GL_BUFFER_DATA_SIZE };
	pglGetProgramResourceiv(program, interfaceBlockType, blockIdentifier.ID, 1, bufferDataSizeProperty, 1, NULL, &m_UBOBlockSize);
	m_UBOSourceBuffer.resize(m_UBOBlockSize);
#if 1
	memset(m_UBOSourceBuffer.data(), 0xFD, m_UBOBlockSize);
#endif // DEBUG_UNIFORM_BUFFER
	pglGenBuffersARB(1, &m_UBOBufferID);
	pglBindBufferARB(m_MemberBufferType, m_UBOBufferID);
	pglBufferDataARB(m_MemberBufferType, m_UBOBlockSize, NULL, GL_DYNAMIC_DRAW);
	std::cout << "GenBuffer - Name: " << m_BlockName.c_str() << " ID: " << m_UBOBufferID << " size: " << m_UBOBlockSize << std::endl;
}

void InterfaceBlock::GetBlockIdentifiers(GLuint program, std::vector<InterfaceBlockIdentifier>& out, 
	int& numUBOBlocks, int& numSSBOBlocks)
{
	GLint numBlocks;
	GLint maxNameLength;
	int blockTypes[] = { GL_UNIFORM_BLOCK, GL_SHADER_STORAGE_BLOCK };
	
	for (int blockType : blockTypes)
	{	
		pglGetProgramInterfaceiv(program, blockType, GL_ACTIVE_RESOURCES, &numBlocks);
		pglGetProgramInterfaceiv(program, blockType, GL_MAX_NAME_LENGTH, &maxNameLength);
		
		if (blockType == GL_UNIFORM_BLOCK)
			numUBOBlocks = numBlocks;
		else if (blockType == GL_SHADER_STORAGE_BLOCK)
			numSSBOBlocks = numBlocks;
		
		std::vector<char> nameData(maxNameLength);
		for(GLuint blockId = 0; blockId < numBlocks; ++blockId)
		{
			pglGetProgramResourceName(program, blockType, blockId, nameData.size(), NULL, &nameData[0]);
			out.push_back(InterfaceBlockIdentifier { blockType, blockId, CStrIntern(&nameData[0]) });
			GLuint block_index = 0;
    		block_index = pglGetProgramResourceIndex(program, blockType, out.back().Name.c_str());
    		ENSURE(blockId == block_index);
		}
	}
}


// TODO: There's a lot of duplicated code here.
// Maybe the old approach of just passing 4 floats would be better
void InterfaceBlock::SetUniform(const UniformBinding& id, CVector4D v, GLuint instanceId)
{
	GLuint offset = GetMemberProp(id.m_UniformId, PROPS::PROP_OFFSET) + GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE) * instanceId;

	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(offset + 4 * sizeof(float)));
	
	if (m_UBOBlockSize < m_UBODirtyBytes)
		IncreaseBufferSize(m_UBODirtyBytes);
		

#if DEBUG_UNIFORM_BUFFER
	for (int k = 0; k < sizeof(float); ++k)
	{
		ENSURE(*(m_UBOSourceBuffer.data() + offset + 0 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.data() + offset + 1 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.data() + offset + 2 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.data() + offset + 3 * sizeof(float) + k) == 0xFD);
	}
#endif // DEBUG_UNIFORM_BUFFER
	*((float*)(m_UBOSourceBuffer.data() + offset + 0 * sizeof(float))) = v.X;
	*((float*)(m_UBOSourceBuffer.data() + offset + 1 * sizeof(float))) = v.Y;
	*((float*)(m_UBOSourceBuffer.data() + offset + 2 * sizeof(float))) = v.Z;
	*((float*)(m_UBOSourceBuffer.data() + offset + 3 * sizeof(float))) = v.W;
}

void InterfaceBlock::SetUniform(const UniformBinding& id, CVector3D v, GLuint instanceId)
{
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
	*((float*)(m_UBOSourceBuffer.data() + offset + 0 * sizeof(float))) = v.X;
	*((float*)(m_UBOSourceBuffer.data() + offset + 1 * sizeof(float))) = v.Y;
	*((float*)(m_UBOSourceBuffer.data() + offset + 2 * sizeof(float))) = v.Z;
}

void InterfaceBlock::SetUniform(const UniformBinding& id, CVector2D v, GLuint instanceId)
{
	GLuint offset = GetMemberProp(id.m_UniformId, PROPS::PROP_OFFSET) + GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE) * instanceId;
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(offset + 2 * sizeof(float)));
	if (m_UBOBlockSize < m_UBODirtyBytes)
		IncreaseBufferSize(m_UBODirtyBytes);
#if DEBUG_UNIFORM_BUFFER
	for (int k = 0; k < sizeof(float); ++k)
	{
		ENSURE(*(m_UBOSourceBuffer.data() + offset + 0 * sizeof(float) + k) == 0xFD);
		ENSURE(*(m_UBOSourceBuffer.data() + offset + 1 * sizeof(float) + k) == 0xFD);
	}
#endif // DEBUG_UNIFORM_BUFFER
	*((float*)(m_UBOSourceBuffer.data() + offset + 0 * sizeof(float))) = v.X;
	*((float*)(m_UBOSourceBuffer.data() + offset + 1 * sizeof(float))) = v.Y;
}

void InterfaceBlock::SetUniform(const UniformBinding& id, size_t count, const CMatrix3D* v, GLuint instanceId)
{	
	//   mat0  mat1  mat2  mat3 ..64
	// [[0123][0123][0123][0123]....]  Inst0
	// [[0123][0123][0123][0123]....]  Inst1
	// [[0123][0123][0123][0123]....]  Inst2
	// [[0123][0123][0123][0123]....]  Inst3
	// [[0123][0123][0123][0123]....]  Inst4
	// ... 
	CMatrix3D* ptr = const_cast<CMatrix3D*>(v);
	GLint memberOffset = GetMemberProp(id.m_UniformId, PROPS::PROP_OFFSET);
	
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, 
		(size_t)(memberOffset +  64 * GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE) * (instanceId + 1)));
	
	if (m_UBOBlockSize < m_UBODirtyBytes)
		IncreaseBufferSize(m_UBODirtyBytes);
	
	// NOTE: Make sure not to move this line before IncreaseBufferSize because it might invalidate the pointer!
	GLubyte* dstPtr = (m_UBOSourceBuffer.data() + memberOffset);
	// TODO: 64 is MAX_BONES bones in the shader... need a way to query this
	dstPtr += 64 * GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE) * instanceId; // point to current instance
	
	for (int h=0; h<count; ++h) // current matrix
	{		
		for (int i=0; i<4; ++i) // current matrix column/row of the current matrix
		{
			GLuint offset = GetMemberProp(id.m_UniformId, PROPS::PROP_MATRIX_STRIDE) * i;
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
		dstPtr += GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE);
		if (h == 63)
			break;
	}
	
	/*
	if (id.first != -1)
	{
		if (id.second == GL_FLOAT_MAT4)
			pglUniformMatrix4fvARB(id.first, count, GL_FALSE, &v->_11);
		else
			LOGERROR("CShaderProgramGLSL::Uniform(): Invalid uniform type (expected mat4)");
	}*/
}

void InterfaceBlock::SetUniform(const UniformBinding& id, float v, GLuint instanceId)
{
	GLuint offset = GetMemberProp(id.m_UniformId, PROPS::PROP_OFFSET) + GetMemberProp(id.m_UniformId, PROPS::PROP_ARRAY_STRIDE) * instanceId;
	m_UBODirtyBytes = std::max((size_t)m_UBODirtyBytes, (size_t)(offset + 1 * sizeof(float)));
	
	if (m_UBOBlockSize < m_UBODirtyBytes)
		IncreaseBufferSize(m_UBODirtyBytes);

#if DEBUG_UNIFORM_BUFFER
for (int i = 0; i < sizeof(float); ++i)
	ENSURE(*(m_UBOSourceBuffer.data() + offset + i) == 0xFD);
#endif // DEBUG_UNIFORM_BUFFER
	*((float*)(m_UBOSourceBuffer.data() + offset)) = v;
}

void InterfaceBlock::SetUniformF4(UniformBinding id, float v0, float v1, float v2, float v3, GLuint instanceId)
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

void InterfaceBlock::Upload()
{
	ENSURE(m_UBOBlockSize >= m_UBODirtyBytes);
	/*
	if (m_BlockName.string() == "ModelUBO")
	{
		std::ofstream myfile;
		myfile.open ("dumpfile.txt", std::fstream::out | std::fstream::app);
		myfile << "start...\n";
		for (int i=0, drawIx=0; i < (m_UBOBlockSize); i += m_MemberProps[PROPS::PROP_ARRAY_STRIDE], ++drawIx)
		{
			if (*(GLuint*)&(m_UBOSourceBuffer.data()[i]) == 1110)
			{
			myfile << drawIx << ": " << *(GLuint*)&(m_UBOSourceBuffer.data()[i]) << " ";
			if (i%99 == 0)
				myfile << "\n";
			}
		}
		
		myfile << "...end\n\n\n";
  		myfile.close();
	}*/
	pglBindBufferARB(m_MemberBufferType, m_UBOBufferID);
	
	// Resize the buffer on the graphics card if the source buffer had to be resized
	if (m_BufferResized)
	{
		pglBufferDataARB(m_MemberBufferType, m_UBOBlockSize, m_UBOSourceBuffer.data(), GL_DYNAMIC_DRAW);
		m_BufferResized = false;
	}
	else
	{
		pglBufferSubDataARB(m_MemberBufferType, 0, m_UBODirtyBytes, m_UBOSourceBuffer.data());
	}
	m_UBODirtyBytes = 0;
#if DEBUG_UNIFORM_BUFFER
	memset(m_UBOSourceBuffer.data(), 0xFD, m_UBOBlockSize);
#endif // DEBUG_UNIFORM_BUFFER
}

/**
 * Gets a binding from a uniform name
 * The idea is to call GetBinding once and then use only the binding because that's faster.
 */
// TODO: check GetUniformVertexIndex in ShaderProgram.cpp.
// It might make sense to store to use an std::map for the mapping too
void InterfaceBlock::GetBinding(UniformBinding& id, CStrIntern uniformName) const
{
	auto it = std::find(m_UniformNames.begin(), m_UniformNames.end(), uniformName);
	if (it == m_UniformNames.end())
	{
		// TODO: Better error handling. Uniforms can be disabled due to shader defines, so
		// it's not necessarily an error if we can't find the uniform binding.
		//std::cerr << "ERROR: Unknown uniform name." << uniformName.c_str() << std::endl;
		return;
	}

	id.m_BlockType = m_InterfaceBlockType;
	int pos = std::distance(m_UniformNames.begin(), it);
	id.m_UniformId = pos; // TODO: third (type) not used yet
	id.m_Type = m_UniformTypes[pos];
}

// Explicit instantiation is required for the allowed block identifier types
//template void InterfaceBlock::GetBlockIdentifiers<GL_UNIFORM_BLOCK>(GLuint program, std::vector<InterfaceBlockIdentifier>& out);
//template void InterfaceBlock::GetBlockIdentifiers<GL_SHADER_STORAGE_BLOCK>(GLuint program, std::vector<InterfaceBlockIdentifier>& out);
