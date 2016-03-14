/* Copyright (C) 2016 Wildfire Games.
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

#include "InstancingModelRendererShared.h"
#include "renderer/ModelRenderer.h"

#include "third_party/mikktspace/weldmesh.h"

IModelDef::IModelDef(const CModelDefPtr& mdef, bool gpuSkinning, bool calculateTangents)
	: m_IndexArray(GL_STATIC_DRAW), m_Array(GL_STATIC_DRAW)
{
	size_t numVertices = mdef->GetNumVertices();

	m_Position.type = GL_FLOAT;
	m_Position.elems = 3;
	m_Array.AddAttribute(&m_Position);

	m_Normal.type = GL_FLOAT;
	m_Normal.elems = 3;
	m_Array.AddAttribute(&m_Normal);

	m_UVs.resize(mdef->GetNumUVsPerVertex());
	for (size_t i = 0; i < mdef->GetNumUVsPerVertex(); i++)
	{
		m_UVs[i].type = GL_FLOAT;
		m_UVs[i].elems = 2;
		m_Array.AddAttribute(&m_UVs[i]);
	}
	
	if (gpuSkinning)
	{
		m_BlendJoints.type = GL_UNSIGNED_BYTE;
		m_BlendJoints.elems = 4;
		m_Array.AddAttribute(&m_BlendJoints);

		m_BlendWeights.type = GL_UNSIGNED_BYTE;
		m_BlendWeights.elems = 4;
		m_Array.AddAttribute(&m_BlendWeights);
	}
	
	if (calculateTangents)
	{
		// Generate tangents for the geometry:-
		
		m_Tangent.type = GL_FLOAT;
		m_Tangent.elems = 4;
		m_Array.AddAttribute(&m_Tangent);
		
		// floats per vertex; position + normal + tangent + UV*sets [+ GPUskinning]
		int numVertexAttrs = 3 + 3 + 4 + 2 * mdef->GetNumUVsPerVertex();
		if (gpuSkinning)
		{
			numVertexAttrs += 8;
		}
		
		// the tangent generation can increase the number of vertices temporarily
		// so reserve a bit more memory to avoid reallocations in GenTangents (in most cases)
		std::vector<float> newVertices;
		newVertices.reserve(numVertexAttrs * numVertices * 2);
		
		// Generate the tangents
		ModelRenderer::GenTangents(mdef, newVertices, gpuSkinning);
		
		// how many vertices do we have after generating tangents?
		int newNumVert = newVertices.size() / numVertexAttrs;
		
		std::vector<int> remapTable(newNumVert);
		std::vector<float> vertexDataOut(newNumVert * numVertexAttrs);

		// re-weld the mesh to remove duplicated vertices
		int numVertices2 = WeldMesh(&remapTable[0], &vertexDataOut[0],
					&newVertices[0], newNumVert, numVertexAttrs);

		// Copy the model data to graphics memory:-
		
		m_Array.SetNumVertices(numVertices2);
		m_Array.Layout();

		VertexArrayIterator<CVector3D> Position = m_Position.GetIterator<CVector3D>();
		VertexArrayIterator<CVector3D> Normal = m_Normal.GetIterator<CVector3D>();
		VertexArrayIterator<CVector4D> Tangent = m_Tangent.GetIterator<CVector4D>();
		
		VertexArrayIterator<u8[4]> BlendJoints;
		VertexArrayIterator<u8[4]> BlendWeights;
		if (gpuSkinning)
		{
			BlendJoints = m_BlendJoints.GetIterator<u8[4]>();
			BlendWeights = m_BlendWeights.GetIterator<u8[4]>();
		}
		
		// copy everything into the vertex array
		for (int i = 0; i < numVertices2; i++)
		{
			int q = numVertexAttrs * i;
			
			Position[i] = CVector3D(vertexDataOut[q + 0], vertexDataOut[q + 1], vertexDataOut[q + 2]);
			q += 3;
			
			Normal[i] = CVector3D(vertexDataOut[q + 0], vertexDataOut[q + 1], vertexDataOut[q + 2]);
			q += 3;

			Tangent[i] = CVector4D(vertexDataOut[q + 0], vertexDataOut[q + 1], vertexDataOut[q + 2], 
					vertexDataOut[q + 3]);
			q += 4;
			
			if (gpuSkinning)
			{
				for (size_t j = 0; j < 4; ++j)
				{
					BlendJoints[i][j] = (u8)vertexDataOut[q + 0 + 2 * j];
					BlendWeights[i][j] = (u8)vertexDataOut[q + 1 + 2 * j];
				}
				q += 8;
			}
			
			for (size_t j = 0; j < mdef->GetNumUVsPerVertex(); j++)
			{
				VertexArrayIterator<float[2]> UVit = m_UVs[j].GetIterator<float[2]>();
				UVit[i][0] = vertexDataOut[q + 0 + 2 * j];
				UVit[i][1] = vertexDataOut[q + 1 + 2 * j];
			}
		}

		// upload vertex data
		m_Array.Upload();
		m_Array.FreeBackingStore();

		m_IndexArray.SetNumVertices(mdef->GetNumFaces() * 3);
		m_IndexArray.Layout();
		
		VertexArrayIterator<u16> Indices = m_IndexArray.GetIterator();
		
		size_t idxidx = 0;	

		// reindex geometry and upload index
		for (size_t j = 0; j < mdef->GetNumFaces(); ++j) 
		{	
			Indices[idxidx++] = remapTable[j * 3 + 0];
			Indices[idxidx++] = remapTable[j * 3 + 1];
			Indices[idxidx++] = remapTable[j * 3 + 2];
		}
		
		m_IndexArray.Upload();
		m_IndexArray.FreeBackingStore();
	}
	else
	{
		// Upload model without calculating tangents:-
		
		m_Array.SetNumVertices(numVertices);
		m_Array.Layout();

		VertexArrayIterator<CVector3D> Position = m_Position.GetIterator<CVector3D>();
		VertexArrayIterator<CVector3D> Normal = m_Normal.GetIterator<CVector3D>();

		ModelRenderer::CopyPositionAndNormals(mdef, Position, Normal);

		for (size_t i = 0; i < mdef->GetNumUVsPerVertex(); i++)
		{
			VertexArrayIterator<float[2]> UVit = m_UVs[i].GetIterator<float[2]>();
			ModelRenderer::BuildUV(mdef, UVit, i);
		}

		if (gpuSkinning)
		{
			VertexArrayIterator<u8[4]> BlendJoints = m_BlendJoints.GetIterator<u8[4]>();
			VertexArrayIterator<u8[4]> BlendWeights = m_BlendWeights.GetIterator<u8[4]>();
			for (size_t i = 0; i < numVertices; ++i)
			{
				const SModelVertex& vtx = mdef->GetVertices()[i];
				for (size_t j = 0; j < 4; ++j)
				{
					BlendJoints[i][j] = vtx.m_Blend.m_Bone[j];
					BlendWeights[i][j] = (u8)(255.f * vtx.m_Blend.m_Weight[j]);
				}
			}
		}

		m_Array.Upload();
		m_Array.FreeBackingStore();

		m_IndexArray.SetNumVertices(mdef->GetNumFaces()*3);
		m_IndexArray.Layout();
		ModelRenderer::BuildIndices(mdef, m_IndexArray.GetIterator());
		m_IndexArray.Upload();
		m_IndexArray.FreeBackingStore();
	}
}


