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

#include "precompiled.h"

#include "Material.h"
#include "graphics/MaterialManager.h"

#include "UniformBlockManager.h"

static CColor BrokenColor(0.3f, 0.3f, 0.3f, 1.0f);

CMaterialRef:: CMaterialRef(CMaterial* material) :
	m_pMaterial(material)
{
	g_Renderer.GetMaterialManager().RegisterMaterialRef(*this);
}

CMaterialRef::CMaterialRef(const CMaterialRef& other) :
	m_pMaterial(other.m_pMaterial)
{
	g_Renderer.GetMaterialManager().RegisterMaterialRef(*this);
}

CMaterialRef& CMaterialRef::operator=(const CMaterialRef& other)
{
	if (m_pMaterial != nullptr)
		g_Renderer.GetMaterialManager().UnRegisterMaterialRef(*this);
	m_pMaterial = other.m_pMaterial;
	g_Renderer.GetMaterialManager().RegisterMaterialRef(*this);
	return *this;
}

CMaterialRef::~CMaterialRef()
{
	if (m_pMaterial != nullptr)
		g_Renderer.GetMaterialManager().UnRegisterMaterialRef(*this);
}

CMaterialTemplate::CMaterialTemplate() :
	m_AlphaBlending(false),
	m_Id(-1)
{
}

void CMaterialTemplate::AddRenderQuery(const char* key)
{
	m_RenderQueries.Add(key);
}

void CMaterialTemplate::AddShaderDefine(CStrIntern key, CStrIntern value)
{
	m_ShaderDefines.Add(key, value);
}

void CMaterialTemplate::AddRequiredSampler(const CStr& samplerName)
{
	CStrIntern string(samplerName);
	m_RequiredSamplers.push_back(string);
}

void CMaterialTemplate::AddConditionalDefine(const char* defname, const char* defvalue, int type, std::vector<float> &args)
{
	m_ConditionalDefines.Add(defname, defvalue, type, args);
}

void CMaterialTemplate::AddBlockValueAssignment(CStrIntern blockName, CStrIntern name, const CVector4D& value)
{
	m_BlockValueAssignments.emplace_back(BlockValueAssignment{ blockName, name, value });
}

void CMaterialTemplate::AddStaticUniform(const char* key, const CVector4D& value)
{
	m_StaticUniforms.Add(key, value);
}

void CMaterialTemplate::SetShaderEffect(const CStr& effect)
{
	m_ShaderEffect = CStrIntern(effect);
}

CMaterial::CMaterial() :
	m_pTemplate(nullptr),
	m_Id(-1)
{
}

void CMaterial::Init(CMaterialTemplate* materialTempl)
{
	m_pTemplate = materialTempl;
	m_StaticUniforms.SetMany(m_pTemplate->m_StaticUniforms);
	RecomputeCombinedShaderDefines();
}

void CMaterial::ComputeHash()
{
	m_Hash = 0;
	boost::hash_combine(m_Hash, m_pTemplate);
	boost::hash_combine(m_Hash, m_ShaderDefines.GetHash());
	for (BlockValueAssignment& blkVal : m_BlockValueAssignments)
		boost::hash_combine(m_Hash, blkVal.GetHash());
	boost::hash_combine(m_Hash, m_ConditionalDefines.GetHash());
	boost::hash_combine(m_Hash, m_StaticUniforms.GetHash());
	for (TextureSampler samp : m_Samplers)
		boost::hash_combine(m_Hash, samp.GetHash());
}

void CMaterial::AddShaderDefine(CStrIntern key, CStrIntern value)
{
	m_ShaderDefines.Add(key, value);
	m_CombinedShaderDefinesLookup.clear();
}

void CMaterial::AddConditionalDefine(const char* defname, const char* defvalue, int type, std::vector<float> &args)
{
	m_ConditionalDefines.Add(defname, defvalue, type, args);
	m_CombinedShaderDefinesLookup.clear();
}

void CMaterial::AddStaticUniform(const char* key, const CVector4D& value)
{
	m_StaticUniforms.Add(key, value);
}

void CMaterial::AddBlockValueAssignment(CStrIntern blockName, CStrIntern name, const CVector4D& value)
{
	m_BlockValueAssignments.emplace_back(BlockValueAssignment{ blockName, name, value });
}

void CMaterial::AddSampler(const TextureSampler& texture)
{
	m_Samplers.push_back(texture);
	if (texture.Name == str_baseTex)
		m_DiffuseTexture = texture.Sampler;
}

// First, update m_CombinedConditionalDefines to contain both the conditional defines
// from the MaterialTemplate and those from the Material.
//
// Set up m_CombinedShaderDefinesLookup so that index i contains m_ShaderDefines, plus
// the extra defines from m_ConditionalDefines[j] for all j where bit j is set in i.
// This lets GetShaderDefines() cheaply return the defines for any combination of conditions.
//
// (This might scale badly if we had a large number of conditional defines per material,
// but currently we don't expect to have many.)
void CMaterial::RecomputeCombinedShaderDefines()
{
	m_CombinedShaderDefinesLookup.clear();
	m_CombinedConditionalDefines = CShaderConditionalDefines();
	m_CombinedConditionalDefines.AddMany(m_pTemplate->m_ConditionalDefines);
	m_CombinedConditionalDefines.AddMany(m_ConditionalDefines);

	int size = m_CombinedConditionalDefines.GetSize();

	CShaderDefines combinedShaderDefines = m_pTemplate->m_ShaderDefines;
	combinedShaderDefines.SetMany(m_ShaderDefines);
	
	// Loop over all 2^n combinations of flags
	for (int i = 0; i < (1 << size); i++)
	{
		CShaderDefines defs = combinedShaderDefines;
		for (int j = 0; j < size; j++)
		{
			if (i & (1 << j))
			{
				const CShaderConditionalDefines::CondDefine& def = m_CombinedConditionalDefines.GetItem(j);
				defs.Add(def.m_DefName, def.m_DefValue);
			}
		}
		m_CombinedShaderDefinesLookup.push_back(defs);
	}
}
