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

#include "lib/ogl.h"
#include "maths/Vector3D.h"
#include "maths/Vector4D.h"

#include "ps/CLogger.h"
#include "ps/Profile.h"


#include "graphics/Color.h"
#include "graphics/LightEnv.h"
#include "graphics/Model.h"
#include "graphics/ModelDef.h"
#include "graphics/UniformBlockManager.h"

#include "renderer/GL4InstancingModelRenderer.h"
#include "renderer/InstancingModelRendererShared.h"
#include "renderer/ModelRenderer.h"
#include "renderer/Renderer.h"
#include "renderer/RenderModifiers.h"
#include "renderer/VertexArray.h"

// Construction and Destruction
template<bool TGpuSkinning>
GL4InstancingModelRenderer<TGpuSkinning>::GL4InstancingModelRenderer(bool calculateTangents)
{
	m = new GL4InstancingModelRendererInternals;
	m->calculateTangents = calculateTangents;
	m->imodeldef = 0;
}

template<bool TGpuSkinning>
GL4InstancingModelRenderer<TGpuSkinning>::~GL4InstancingModelRenderer()
{
	delete m;
}


// Build modeldef data if necessary - we have no per-CModel data
template<bool TGpuSkinning>
CModelRData* GL4InstancingModelRenderer<TGpuSkinning>::CreateModelData(const void* key, CModel* model)
{
	CModelDefPtr mdef = model->GetModelDef();
	IModelDef* imodeldef = (IModelDef*)mdef->GetRenderData(m);

	if (TGpuSkinning)
 		ENSURE(model->IsSkinned());
	else
		ENSURE(!model->IsSkinned());

	if (!imodeldef)
	{
		imodeldef = new IModelDef(mdef, TGpuSkinning, m->calculateTangents);
		mdef->SetRenderData(m, imodeldef);
	}

	return new CModelRData(key);
}

template<bool TGpuSkinning>
void GL4InstancingModelRenderer<TGpuSkinning>::UpdateModelData(CModel* UNUSED(model), CModelRData* UNUSED(data), int UNUSED(updateflags))
{
	// We have no per-CModel data
}


// Setup one rendering pass.
template<bool TGpuSkinning>
void GL4InstancingModelRenderer<TGpuSkinning>::BeginPass(int streamflags)
{
	ENSURE(streamflags == (streamflags & (STREAM_POS|STREAM_NORMAL|STREAM_UV0|STREAM_UV1)));
}

// Cleanup rendering pass.
template<bool TGpuSkinning>
void GL4InstancingModelRenderer<TGpuSkinning>::EndPass(int UNUSED(streamflags))
{
	CVertexBuffer::Unbind();
}


// Prepare UV coordinates for this modeldef
template<bool TGpuSkinning>
void GL4InstancingModelRenderer<TGpuSkinning>::PrepareModelDef(const CShaderProgramPtr& shader, int streamflags, const CModelDef& def)
{
	m->imodeldef = (IModelDef*)def.GetRenderData(m);

	ENSURE(m->imodeldef);

	u8* base = m->imodeldef->m_Array.Bind();
	GLsizei stride = (GLsizei)m->imodeldef->m_Array.GetStride();
	
	// HACK: Hardcode the vertex attribute to 15 (gl_MultiTexCoord7) because that doesn't seem to be
	// used anywhere currently
	u8* instancingDataBasePtr = m->imodeldef->m_Array.GetInstancingDataBasePtr();
	pglVertexAttribIPointerEXT(15, 1, GL_UNSIGNED_INT, 0, (const GLvoid*)instancingDataBasePtr);
	pglVertexBindingDivisor(15, 1);

	m->imodeldefIndexBase = m->imodeldef->m_IndexArray.Bind();

	if (streamflags & STREAM_POS)
		shader->VertexPointer(3, GL_FLOAT, stride, base + m->imodeldef->m_Position.offset);

	if (streamflags & STREAM_NORMAL)
		shader->NormalPointer(GL_FLOAT, stride, base + m->imodeldef->m_Normal.offset);
	
	if (m->calculateTangents)
		shader->VertexAttribPointer(str_a_tangent, 4, GL_FLOAT, GL_TRUE, stride, base + m->imodeldef->m_Tangent.offset);

	if (streamflags & STREAM_UV0)
		shader->TexCoordPointer(GL_TEXTURE0, 2, GL_FLOAT, stride, base + m->imodeldef->m_UVs[0].offset);
	
	if ((streamflags & STREAM_UV1) && def.GetNumUVsPerVertex() >= 2)
		shader->TexCoordPointer(GL_TEXTURE1, 2, GL_FLOAT, stride, base + m->imodeldef->m_UVs[1].offset);

	// GPU skinning requires extra attributes to compute positions/normals
	if (TGpuSkinning)
	{
		shader->VertexAttribPointer(str_a_skinJoints, 4, GL_UNSIGNED_BYTE, GL_FALSE, stride, base + m->imodeldef->m_BlendJoints.offset);
		shader->VertexAttribPointer(str_a_skinWeights, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, base + m->imodeldef->m_BlendWeights.offset);
	}

	shader->AssertPointersBound();
}

// TODO: there are too many functions here that do nothing else than forwarding
// the functions from multiDrawIndirectCommands.

template<bool TGpuSkinning>
void GL4InstancingModelRenderer<TGpuSkinning>::ResetDrawID()
{
	m->multiDrawIndirectCommands.ResetDrawID();
}

template<bool TGpuSkinning>
void GL4InstancingModelRenderer<TGpuSkinning>::ResetCommands()
{
	m->multiDrawIndirectCommands.ResetCommands();
}

template<bool TGpuSkinning>
void GL4InstancingModelRenderer<TGpuSkinning>::AddInstance()
{
	m->multiDrawIndirectCommands.AddInstance();
}

template<bool TGpuSkinning>
void GL4InstancingModelRenderer<TGpuSkinning>::BindAndUpload()
{
		m->multiDrawIndirectCommands.BindAndUpload();
}

template class GL4InstancingModelRenderer<true>;
template class GL4InstancingModelRenderer<false>;

