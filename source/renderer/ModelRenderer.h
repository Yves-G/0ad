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

/*
 * Home to the ModelRenderer class, an abstract base class that manages
 * a per-frame list of submitted models, as well as simple helper
 * classes.
 */

#ifndef INCLUDED_MODELRENDERER
#define INCLUDED_MODELRENDERER

#include <memory>

#include "ModelRendererShared.h"
#include "graphics/MeshManager.h"
#include "graphics/SColor.h"
#include "renderer/Renderer.h"
#include "renderer/ModelRendererShared.h"
#include "renderer/VertexArray.h"

class CModel;
class CShaderDefines;

/**
 * Class ModelRenderer: Abstract base class for all model renderers.
 *
 * A ModelRenderer manages a per-frame list of models.
 *
 * It is supposed to be derived in order to create new ways in which
 * the per-frame list of models can be managed (for batching, for
 * transparent rendering, etc.) or potentially for rarely used special
 * effects.
 *
 * A typical ModelRenderer will delegate vertex transformation/setup
 * to a ModelVertexRenderer.
 * It will delegate fragment stage setup to a RenderModifier.
 *
 * For most purposes, you should use a BatchModelRenderer with
 * specialized ModelVertexRenderer and RenderModifier implementations.
 *
 * It is suggested that a derived class implement the provided generic
 * Render function, however in some cases it may be necessary to supply
 * a Render function with a different prototype.
 *
 * ModelRenderer also contains a number of static helper functions
 * for building vertex arrays.
 */
class ModelRenderer
{
public:
	ModelRenderer() { }
	virtual ~ModelRenderer() { }

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
	virtual void Submit(int cullGroup, CModel* model) = 0;

	/**
	 * PrepareModels: Calculate renderer data for all previously
	 * submitted models.
	 *
	 * Must be called before any rendering calls and after all models
	 * for this frame have been submitted.
	 */
	virtual void PrepareModels() = 0;

	/**
	 * EndFrame: Remove all models from the list of submitted
	 * models.
	 */
	virtual void EndFrame() = 0;

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
	virtual void Render(const CShaderDefines& context, int cullGroup, int flags) = 0;

	/**
	 * CopyPositionAndNormals: Copy unanimated object-space vertices and
	 * normals into the given vertex array.
	 *
	 * @param mdef The underlying CModelDef that contains mesh data.
	 * @param Position Points to the array that will receive
	 * position vectors. The array behind the iterator
	 * must be large enough to hold model->GetModelDef()->GetNumVertices()
	 * vertices.
	 * @param Normal Points to the array that will receive normal vectors.
	 * The array behind the iterator must be as large as the Position array.
	 */
	static void CopyPositionAndNormals(
			const CModelDefPtr& mdef,
			const VertexArrayIterator<CVector3D>& Position,
			const VertexArrayIterator<CVector3D>& Normal);

	/**
	 * BuildPositionAndNormals: Build animated vertices and normals,
	 * transformed into world space.
	 *
	 * @param model The model that is to be transformed.
	 * @param Position Points to the array that will receive
	 * transformed position vectors. The array behind the iterator
	 * must be large enough to hold model->GetModelDef()->GetNumVertices()
	 * vertices. It must allow 16 bytes to be written to each element
	 * (i.e. provide 4 bytes of padding after each CVector3D).
	 * @param Normal Points to the array that will receive transformed
	 * normal vectors. The array behind the iterator must be as large as
	 * the Position array.
	 */
	static void BuildPositionAndNormals(
			CModel* model,
			const VertexArrayIterator<CVector3D>& Position,
			const VertexArrayIterator<CVector3D>& Normal);

	/**
	 * BuildColor4ub: Build lighting colors for the given model,
	 * based on previously calculated world space normals.
	 *
	 * @param model The model that is to be lit.
	 * @param Normal Array of the model's normal vectors, animated and
	 * transformed into world space.
	 * @param Color Points to the array that will receive the lit vertex color.
	 * The array behind the iterator must large enough to hold
	 * model->GetModelDef()->GetNumVertices() vertices.
	 */
	static void BuildColor4ub(
			CModel* model,
			const VertexArrayIterator<CVector3D>& Normal,
			const VertexArrayIterator<SColor4ub>& Color);

	/**
	 * BuildUV: Copy UV coordinates into the given vertex array.
	 *
	 * @param mdef The model def.
	 * @param UV Points to the array that will receive UV coordinates.
	 * The array behind the iterator must large enough to hold
	 * mdef->GetNumVertices() vertices.
	 */
	static void BuildUV(
			const CModelDefPtr& mdef,
			const VertexArrayIterator<float[2]>& UV,
			int UVset);

	/**
	 * BuildIndices: Create the indices array for the given CModelDef.
	 *
	 * @param mdef The model definition object.
	 * @param Indices The index array, must be able to hold
	 * mdef->GetNumFaces()*3 elements.
	 */
	static void BuildIndices(
			const CModelDefPtr& mdef,
			const VertexArrayIterator<u16>& Indices);
	
	/**
	 * GenTangents: Generate tangents for the given CModelDef.
	 *
	 * @param mdef The model definition object.
	 * @param newVertices An out vector of the unindexed vertices with tangents added. 
	 * The new vertices cannot be used with existing face index and must be welded/reindexed.
	 */
	static void GenTangents(const CModelDefPtr& mdef, std::vector<float>& newVertices, bool gpuSkinning);
};


/**
 * Implementation of ModelRenderer that loads the appropriate shaders for
 * rendering each model, and that batches by shader (and by mesh and texture).
 * 
 * Note that the term "Shader" is somewhat misleading, as this handled
 * fixed-function rendering using the same API as real GLSL/ARB shaders.
 */
template <typename VertexRendererT, typename RenderModifierT>
class ShaderModelRenderer : public ModelRenderer
{

public:
	// Note: Cosider using the function CreateShaderModelRenderer instead of calling the constructor directly
	ShaderModelRenderer(VertexRendererT vertexRenderer, RenderModifierT renderModifier):
		m_VertexRenderer(vertexRenderer),
		m_RenderModifier(renderModifier)
	{	
	}

	virtual ~ShaderModelRenderer();

	// Batching implementations
	virtual void Submit(int cullGroup, CModel* model);
	virtual void PrepareModels();
	virtual void EndFrame();
	virtual void Render(const CShaderDefines& context, int cullGroup, int flags);

private:
	
	RenderModifierT m_RenderModifier;
	VertexRendererT m_VertexRenderer;
	
	/// List of submitted models for rendering in this frame
	std::vector<CModel*> m_Submissions[CRenderer::CULL_MAX];
};

// This function is for convenience. Template arguments can be deduced by the function arguments (which is not possible
// when the constructor is called directly)
template <typename T0, typename T1>
ModelRendererPtr CreateShaderModelRenderer(T0 vertexRenderer, T1 renderModifier)
{
	return ModelRendererPtr(new ShaderModelRenderer<T0, T1>(vertexRenderer, renderModifier));
}

#endif // INCLUDED_MODELRENDERER
