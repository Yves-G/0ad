/* Copyright (C) 2012 Wildfire Games.
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

/*
 * Implementation of InstancingModelRenderer
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

#include "renderer/InstancingModelRendererShared.h"
#include "renderer/InstancingModelRenderer.h"
#include "renderer/Renderer.h"
#include "renderer/RenderModifiers.h"
#include "renderer/VertexArray.h"


///////////////////////////////////////////////////////////////////////////////////////////////
// InstancingModelRenderer implementation

struct InstancingModelRendererInternals
{
	bool gpuSkinning;
	
	bool calculateTangents;

	/// Previously prepared modeldef
	IModelDef* imodeldef;

	/// Index base for imodeldef
	u8* imodeldefIndexBase;
	
	MultiDrawIndirectCommands multiDrawIndirectCommands;
};


// Construction and Destruction
InstancingModelRenderer::InstancingModelRenderer(bool gpuSkinning, bool calculateTangents)
{
	m = new InstancingModelRendererInternals;
	m->gpuSkinning = gpuSkinning;
	m->calculateTangents = calculateTangents;
	m->imodeldef = 0;
}

InstancingModelRenderer::~InstancingModelRenderer()
{
	delete m;
}


// Build modeldef data if necessary - we have no per-CModel data
CModelRData* InstancingModelRenderer::CreateModelData(const void* key, CModel* model)
{
	CModelDefPtr mdef = model->GetModelDef();
	IModelDef* imodeldef = (IModelDef*)mdef->GetRenderData(m);

	if (m->gpuSkinning)
 		ENSURE(model->IsSkinned());
	else
		ENSURE(!model->IsSkinned());

	if (!imodeldef)
	{
		imodeldef = new IModelDef(mdef, m->gpuSkinning, m->calculateTangents);
		mdef->SetRenderData(m, imodeldef);
	}

	return new CModelRData(key);
}


void InstancingModelRenderer::UpdateModelData(CModel* UNUSED(model), CModelRData* UNUSED(data), int UNUSED(updateflags))
{
	// We have no per-CModel data
}


// Setup one rendering pass.
void InstancingModelRenderer::BeginPass(int streamflags)
{
	ENSURE(streamflags == (streamflags & (STREAM_POS|STREAM_NORMAL|STREAM_UV0|STREAM_UV1)));
}

// Cleanup rendering pass.
void InstancingModelRenderer::EndPass(int UNUSED(streamflags))
{
	CVertexBuffer::Unbind();
}


// Prepare UV coordinates for this modeldef
void InstancingModelRenderer::PrepareModelDef(const CShaderProgramPtr& shader, int streamflags, const CModelDef& def)
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
	if (m->gpuSkinning)
	{
		shader->VertexAttribPointer(str_a_skinJoints, 4, GL_UNSIGNED_BYTE, GL_FALSE, stride, base + m->imodeldef->m_BlendJoints.offset);
		shader->VertexAttribPointer(str_a_skinWeights, 4, GL_UNSIGNED_BYTE, GL_TRUE, stride, base + m->imodeldef->m_BlendWeights.offset);
	}

	shader->AssertPointersBound();
}

void InstancingModelRenderer::PrepareModel(const CShaderProgramPtr& shader, CModel* model)
{
	UniformBlockManager& uniformBlockManager = g_Renderer.GetUniformBlockManager();
	if (m->gpuSkinning)
	{
		CModelDefPtr mdldef = model->GetModelDef();
		
		// Bind matrices for current animation state.
		// Add 1 to NumBones because of the special 'root' bone.
		// HACK: NVIDIA drivers return uniform name with "[0]", Intel Windows drivers without;
		// try uploading both names since one of them should work, and this is easier than
		// canonicalising the uniform names in CShaderProgramGLSL
		UniformBinding binding = uniformBlockManager.GetBinding(CStrIntern("GPUSkinningUBO"), str_skinBlendMatrices_0, true);
		uniformBlockManager.SetUniform<UniformBlockManager::MODEL_INSTANCED>(binding, mdldef->GetNumBones() + 1, model->GetAnimatedBoneMatrices());
		//shader->Uniform(str_skinBlendMatrices_0, mdldef->GetNumBones() + 1, model->GetAnimatedBoneMatrices());
		//shader->Uniform(str_skinBlendMatrices, mdldef->GetNumBones() + 1, model->GetAnimatedBoneMatrices());
	}
}

// Render one model
void InstancingModelRenderer::RenderModel(const CShaderProgramPtr& shader, int UNUSED(streamflags), CModel* model, CModelRData* UNUSED(data))
{
	
	CModelDefPtr mdldef = model->GetModelDef();
	/*
	if (m->gpuSkinning)
	{
		// Bind matrices for current animation state.
		// Add 1 to NumBones because of the special 'root' bone.
		// HACK: NVIDIA drivers return uniform name with "[0]", Intel Windows drivers without;
		// try uploading both names since one of them should work, and this is easier than
		// canonicalising the uniform names in CShaderProgramGLSL
		shader->Uniform(str_skinBlendMatrices_0, mdldef->GetNumBones() + 1, model->GetAnimatedBoneMatrices());
		shader->Uniform(str_skinBlendMatrices, mdldef->GetNumBones() + 1, model->GetAnimatedBoneMatrices());
	}*/

	// render the lot
	size_t numFaces = mdldef->GetNumFaces();

	if (!g_Renderer.m_SkipSubmit)
	{
		// Draw with DrawRangeElements where available, since it might be more efficient
#if CONFIG2_GLES
		glDrawElements(GL_TRIANGLES, (GLsizei)numFaces*3, GL_UNSIGNED_SHORT, m->imodeldefIndexBase);
#else
		pglDrawRangeElementsEXT(GL_TRIANGLES, 0, (GLuint)m->imodeldef->m_Array.GetNumVertices()-1,
				(GLsizei)numFaces*3, GL_UNSIGNED_SHORT, m->imodeldefIndexBase);
#endif
	}

	// bump stats
	g_Renderer.m_Stats.m_DrawCalls++;
	g_Renderer.m_Stats.m_ModelTris += numFaces;

}

// TODO: there are too many functions here that do nothing else than forwarding
// the functions from multiDrawIndirectCommands.


void InstancingModelRenderer::ResetDrawID()
{
	m->multiDrawIndirectCommands.ResetDrawID();
}

void InstancingModelRenderer::ResetCommands()
{
	m->multiDrawIndirectCommands.ResetCommands();
}


void InstancingModelRenderer::AddInstance()
{
	m->multiDrawIndirectCommands.AddInstance();
}

void InstancingModelRenderer::BindAndUpload()
{
		m->multiDrawIndirectCommands.BindAndUpload();
}

void InstancingModelRenderer::RenderModelsInstanced(u32 modelsCount)
{
	if (!g_Renderer.m_SkipSubmit)
		m->multiDrawIndirectCommands.Draw(modelsCount);
}

void InstancingModelRenderer::SetRenderModelInstanced(const CShaderProgramPtr& shader, int UNUSED(streamflags), CModel* model, CModelRData* UNUSED(data))
{
	/*
	CModelDefPtr mdldef = model->GetModelDef();

	if (m->gpuSkinning)
	{
		// Bind matrices for current animation state.
		// Add 1 to NumBones because of the special 'root' bone.
		// HACK: NVIDIA drivers return uniform name with "[0]", Intel Windows drivers without;
		// try uploading both names since one of them should work, and this is easier than
		// canonicalising the uniform names in CShaderProgramGLSL
		shader->Uniform(str_skinBlendMatrices_0, mdldef->GetNumBones() + 1, model->GetAnimatedBoneMatrices());
		shader->Uniform(str_skinBlendMatrices, mdldef->GetNumBones() + 1, model->GetAnimatedBoneMatrices());
	}*/

	CModelDefPtr mdldef = model->GetModelDef();
	
	size_t numFaces = mdldef->GetNumFaces();
	
	//u64 indexBase = (u64)m->imodeldefIndexBase;
	//indexBase = indexBase / 2;
	
	IModelDef* imodeldef = (IModelDef*)mdldef->GetRenderData(m);
	ENSURE(imodeldef);
	u8* imodeldefIndexBase = imodeldef->m_IndexArray.GetBindAddress();
	// Hardcoded sizeof(index-buffer-element-type) as 2 byte assuming u16
	size_t imodeldefIndexBaseUnit = (size_t)imodeldefIndexBase / 2;
	
	
	m->multiDrawIndirectCommands.AddCommand((GLsizei)numFaces*3, // primCount
											1, // instanceCount
											imodeldefIndexBaseUnit, // m->imodeldefIndexBase, // firstIndex
											0); // baseVertex... I guess that should be 0 because the offset for instancing data was already passed to glVertexPinter

	// bump stats
	g_Renderer.m_Stats.m_DrawCalls++; // TODO: how with instancing...?
	g_Renderer.m_Stats.m_ModelTris += numFaces;

}
