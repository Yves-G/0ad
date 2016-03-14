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
 * This file is for code shared between the GL4InstancingModelRenderer and the
 * InstancingModelRenderer.
 */

#ifndef INCLUDED_INSTANCINGMODELRENDERERSHARED
#define INCLUDED_INSTANCINGMODELRENDERERSHARED

#include "graphics/MeshManager.h"
#include "graphics/ModelDef.h"
#include "renderer/VertexArray.h"

struct IModelDef : public CModelDefRPrivate
{
	/// Static per-CModel vertex array
	VertexArray m_Array;

	/// Position and normals are static
	VertexArray::Attribute m_Position;
	VertexArray::Attribute m_Normal;
	VertexArray::Attribute m_Tangent;
	VertexArray::Attribute m_BlendJoints; // valid iff gpuSkinning == true
	VertexArray::Attribute m_BlendWeights; // valid iff gpuSkinning == true

	/// The number of UVs is determined by the model
	std::vector<VertexArray::Attribute> m_UVs;

	/// Indices are the same for all models, so share them
	VertexIndexArray m_IndexArray;

	IModelDef(const CModelDefPtr& mdef, bool gpuSkinning, bool calculateTangents);
};

#endif // INCLUDED_INSTANCINGMODELRENDERERSHARED
