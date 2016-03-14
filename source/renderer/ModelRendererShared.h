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

#ifndef INCLUDED_MODELRENDERERSHARED
#define INCLUDED_MODELRENDERERSHARED

class CModel;

class RenderModifier;
typedef shared_ptr<RenderModifier> RenderModifierPtr;

class LitRenderModifier;
typedef shared_ptr<LitRenderModifier> LitRenderModifierPtr;

class ModelVertexRenderer;
typedef shared_ptr<ModelVertexRenderer> ModelVertexRendererPtr;

class ModelRenderer;
typedef shared_ptr<ModelRenderer> ModelRendererPtr;

struct SMRTechBucket
{
	CShaderTechniquePtr tech;
	CModel** models;
	size_t numModels;

	// Model list is stored as pointers, not as a std::vector,
	// so that sorting lists of this struct is fast
};

/**
 * Class CModelRData: Render data that is maintained per CModel.
 * ModelRenderer implementations may derive from this class to store
 * per-CModel data.
 *
 * The main purpose of this class over CRenderData is to track which
 * ModelRenderer the render data belongs to (via the key that is passed
 * to the constructor). When a model changes the renderer it uses
 * (e.g. via run-time modification of the renderpath configuration),
 * the old ModelRenderer's render data is supposed to be replaced by
 * the new data.
 */
class CModelRData : public CRenderData
{
public:
	CModelRData(const void* key) : m_Key(key) { }

	/**
	 * GetKey: Retrieve the key that can be used to identify the
	 * ModelRenderer that created this data.
	 *
	 * @return The opaque key that was passed to the constructor.
	 */
	const void* GetKey() const { return m_Key; }

private:
	/// The key for model renderer identification
	const void* m_Key;
};

#endif // INCLUDED_MODELRENDERERSHARED
