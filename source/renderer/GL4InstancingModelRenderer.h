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

/*
 * Special ModelVertexRender that only works for non-animated models,
 * but is very fast for instanced models.
 */

#ifndef INCLUDED_GL4INSTANCINGMODELRENDERER
#define INCLUDED_GL4INSTANCINGMODELRENDERER

#include "graphics/UniformBlockManager.h"
#include "graphics/Model.h"
#include "graphics/ModelDef.h"
#include "graphics/MultiDrawIndirectCommands.h"
#include "renderer/InstancingModelRendererShared.h"

// TODO: Quite pointless to have this in the header, right?
//struct InstancingModelRendererInternals;
class IModelDef;
class CModelRData;

struct GL4InstancingModelRendererInternals
{	
	bool calculateTangents;

	/// Previously prepared modeldef
	IModelDef* imodeldef;

	/// Index base for imodeldef
	u8* imodeldefIndexBase;
	
	MultiDrawIndirectCommands multiDrawIndirectCommands;
};

/**
 * Render non-animated (but potentially moving) models using a ShaderRenderModifier.
 * This computes and binds per-vertex data; the modifier is responsible
 * for setting any shader uniforms etc (including the instancing transform).
 */
template<bool TGpuSkinning>
class GL4InstancingModelRenderer
{
public:
	GL4InstancingModelRenderer(bool calculateTangents);
	GL4InstancingModelRenderer(const GL4InstancingModelRenderer& other) = delete;
	~GL4InstancingModelRenderer();

	// Implementations
	CModelRData* CreateModelData(const void* key, CModel* model);
	void UpdateModelData(CModel* model, CModelRData* data, int updateflags);

	void BeginPass(int streamflags);
	void EndPass(int streamflags);
	void PrepareModelDef(const CShaderProgramPtr& shader, int streamflags, const CModelDef& def);
	void ResetDrawID();
	void ResetCommands();
	void AddInstance();
	inline void SetRenderModelInstanced(CModel* model);
	inline void RenderModelsInstanced(u32 modelsCount);
	void BindAndUpload();
	inline void PrepareModel(const CShaderProgramPtr& shader, CModel* model);

protected:
	GL4InstancingModelRendererInternals* m;
};

template<bool TGpuSkinning>
void GL4InstancingModelRenderer<TGpuSkinning>::PrepareModel(const CShaderProgramPtr& UNUSED(shader), CModel* model)
{
	if (TGpuSkinning)
	{
		UniformBlockManager& uniformBlockManager = g_Renderer.GetUniformBlockManager();
		CModelDefPtr mdldef = model->GetModelDef();
		
		UniformBinding binding = uniformBlockManager.GetBinding(CStrIntern("GPUSkinningBlock"), str_skinBlendMatrices, true);
		uniformBlockManager.SetUniform<UniformBlockManager::MODEL_INSTANCED>(binding, mdldef->GetNumBones() + 1, model->GetAnimatedBoneMatrices());
	}
}

template<bool TGpuSkinning>
void GL4InstancingModelRenderer<TGpuSkinning>::RenderModelsInstanced(u32 modelsCount)
{
	if (!g_Renderer.m_SkipSubmit)
		m->multiDrawIndirectCommands.Draw(modelsCount);
}

template<bool TGpuSkinning>
void GL4InstancingModelRenderer<TGpuSkinning>::SetRenderModelInstanced(CModel* model)
{
	CModelDefPtr mdldef = model->GetModelDef();	
	size_t numFaces = mdldef->GetNumFaces();
	
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

#endif // INCLUDED_GL4INSTANCINGMODELRENDERER
