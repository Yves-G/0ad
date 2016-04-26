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

#ifndef INCLUDED_MODELRENDERERSHAREDIMPL
#define INCLUDED_MODELRENDERERSHAREDIMPL

#include "graphics/Material.h"
#include "graphics/Model.h"
#include "renderer/ModelRendererShared.h"

struct SMRCompareTechBucket
{
	bool operator()(const SMRTechBucket& a, const SMRTechBucket& b)
	{
		return a.tech < b.tech;
	}
};

struct SMRMaterialBucketKey
{
	SMRMaterialBucketKey(CStrIntern effect, const CShaderDefines& defines)
		: effect(effect), defines(defines) { }

	CStrIntern effect;
	CShaderDefines defines;

	bool operator==(const SMRMaterialBucketKey& b) const
	{
		return (effect == b.effect && defines == b.defines);
	}

private:
	SMRMaterialBucketKey& operator=(const SMRMaterialBucketKey&);
};

struct SMRMaterialBucketKeyHash
{
	size_t operator()(const SMRMaterialBucketKey& key) const
	{
		size_t hash = 0;
		boost::hash_combine(hash, key.effect.GetHash());
		boost::hash_combine(hash, key.defines.GetHash());
		return hash;
	}
};

struct SMRSortByDistItem
{
	size_t techIdx;
	CModel* model;
	float dist;
};

struct SMRCompareSortByDistItem
{
	bool operator()(const SMRSortByDistItem& a, const SMRSortByDistItem& b)
	{
		// Prefer items with greater distance, so we draw back-to-front
		return (a.dist > b.dist);

		// (Distances will almost always be distinct, so we don't need to bother
		// tie-breaking on modeldef/texture/etc)
	}
};

struct SMRBatchModel
{
	bool operator()(CModel* a, CModel* b)
	{
		if (a->GetModelDef() < b->GetModelDef())
			return true;
		if (b->GetModelDef() < a->GetModelDef())
			return false;

		if (a->GetMaterial()->GetDiffuseTexture() < b->GetMaterial()->GetDiffuseTexture())
			return true;
		if (b->GetMaterial()->GetDiffuseTexture() < a->GetMaterial()->GetDiffuseTexture())
			return false;

		return a->GetMaterial()->GetStaticUniforms() < b->GetMaterial()->GetStaticUniforms();
	}
};

#endif // INCLUDED_MODELRENDERERSHAREDIMPL
