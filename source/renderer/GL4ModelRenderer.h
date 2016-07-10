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
 
#ifndef INCLUDED_GL4MODELRENDERER
#define INCLUDED_GL4MODELRENDERER

#include "lib/allocators/arena.h"
#include "lib/allocators/allocator_adapters.h"

#include "ModelRendererShared.h"

#include "ModelRenderer.h"
#include "GL4InstancingModelRenderer.h"


// TODO: Not separated in source file anymore... so quite useless
/**
 * Internal data of the ShaderModelRenderer.
 *
 * Separated into the source file to increase implementation hiding (and to
 * avoid some causes of recompiles).
 */
template <typename GL4ModelRendererT>
struct GL4ModelRendererInternals
{
	GL4ModelRendererInternals(GL4ModelRendererT* r) : m_Renderer(r) { }

	/// Back-link to "our" renderer
	GL4ModelRendererT* m_Renderer;

	/// List of submitted models for rendering in this frame
	std::vector<CModel*> submissions[CRenderer::CULL_MAX];
	
	struct HeapCounters
	{
		HeapCounters() : pass(0), idx(0), idxTechStart(0), modelIx(0) {}
		size_t pass;
		size_t idx;
		size_t idxTechStart;
		size_t modelIx;
	} heapCounters;
};

template <bool TGpuSkinning, typename RenderModifierT>
class GL4ModelRenderer : public ModelRenderer
{
	friend struct GL4ModelRendererInternals<GL4ModelRenderer<TGpuSkinning, RenderModifierT> >;
	
public:
	
	// Note: Cosider using the function CreateGL4ModelRenderer instead of calling the constructor directly
	GL4ModelRenderer(shared_ptr<GL4InstancingModelRenderer<TGpuSkinning> > vertexRenderer, RenderModifierT renderModifier):
		m_VertexRenderer(vertexRenderer),
		m_RenderModifier(renderModifier)
	{
		m = new GL4ModelRendererInternals<GL4ModelRenderer<TGpuSkinning, RenderModifierT> >(this);
	}
	
	~GL4ModelRenderer() { }
	
	GL4ModelRenderer(const GL4ModelRenderer& other) = delete;

	/**
	 * Initialise global settings.
	 * Should be called before using the class.
	 */
	static void Init();
	
	/**
	 * Submit: Submit a model for rendering this frame.
	 *
	 * preconditions : The model must not have been submitted to any
	 * ModelRenderer in this frame. Submit may only be called
	 * after EndFrame and before PrepareModels.
	 *
	 * @param model The model that will be added to the list of models
	 * submitted this frame.
	 */
	void Submit(int cullGroup, CModel* model);

	/**
	 * PrepareModels: Calculate renderer data for all previously
	 * submitted models.
	 *
	 * Must be called before any rendering calls and after all models
	 * for this frame have been submitted.
	 */
	void PrepareModels();

	/**
	 * EndFrame: Remove all models from the list of submitted
	 * models.
	 */
	void EndFrame();

	/**
	 * Render: Render submitted models, using the given RenderModifier to setup
	 * the fragment stage.
	 *
	 * @note It is suggested that derived model renderers implement and use
	 * this Render functions. However, a highly specialized model renderer
	 * may need to "disable" this function and provide its own Render function
	 * with a different prototype.
	 *
	 * preconditions  : PrepareModels must be called after all models have been
	 * submitted and before calling Render.
	 *
	 * @param modifier The RenderModifier that specifies the fragment stage.
	 * @param flags If flags is 0, all submitted models are rendered.
	 * If flags is non-zero, only models that contain flags in their
	 * CModel::GetFlags() are rendered.
	 */
	virtual void Render(const CShaderDefines& context, int cullGroup, int flags);

private:
	typedef ProxyAllocator<SMRTechBucket, Allocators::DynamicArena> TechBucketsAllocator;
	
	struct RenderCmd
	{
		RenderCmd() : tech(nullptr), pass(0), mdldef(nullptr), numInst(0) {}

		// We're not using CShaderTechniquePtr o CModelDefPtr to avoid overhead.
		// RenderCmd should really just be used temporarily during the render loop.
		CShaderTechnique* tech;
		int pass;
		CModelDef* mdldef;
		int numInst;
	};

	/// private member functions
	
	bool PrepareUniformBuffers(size_t maxInstancesPerDraw, int flags,
						const std::vector<SMRTechBucket, TechBucketsAllocator>& techBuckets,
						std::vector<RenderCmd>& renderCmds);
	
	/// private data members
	
	shared_ptr<GL4InstancingModelRenderer<TGpuSkinning> > m_VertexRenderer;
	RenderModifierT m_RenderModifier;
	
	GL4ModelRendererInternals<GL4ModelRenderer<TGpuSkinning, RenderModifierT> >* m;
	MultiDrawIndirectCommands m_MultiDrawIndirectCommands;
};

// This function is for convenience. Template arguments can be deduced by the function arguments (which is not possible
// when the constructor is called directly)
template <bool A, typename T>
ModelRendererPtr CreateGL4ModelRenderer(
	shared_ptr<GL4InstancingModelRenderer<A> > vertexRenderer, T renderModifier)
{
	return ModelRendererPtr(new GL4ModelRenderer<A, T>(vertexRenderer, renderModifier));
}
 
#endif // INCLUDED_GL4MODELRENDERER
