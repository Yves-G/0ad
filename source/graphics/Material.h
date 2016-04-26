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

#ifndef INCLUDED_MATERIAL
#define INCLUDED_MATERIAL

#include "graphics/ShaderDefines.h"
#include "graphics/Texture.h"
#include "maths/Vector4D.h"
#include "ps/CStr.h"
#include "ps/CStrIntern.h"
#include "ps/Shapes.h"
#include "renderer/Renderer.h"
#include "simulation2/helpers/Player.h"

/**
 
 Background: 
 ------------
 The MaterialManager keeps track of all materials. It manages template data
 separately from the additions that were made on top of these templates. A material 
 is a combination of one template and a set of additions to this template.
 Storing the template data separately is an optimization because duplication of the
 common data is avoided in RAM and also in VRAM on the GPU (uniforms, texture 
 handles etc.). In addition, it could help with hotloading of material templates.
 
 As a second optimization, the MaterialManager also detects identical materials 
 (same template, identical additions) and ensures that this data is only stored
 once. 
 
 
 Steps to create a Material:
 ---------------------------
 
 1. Load the material from a template.
 The xml files in art/materials are considered templates because you usually use 
 them together with actors or terrain definitions to define the final material with 
 the data used for rendering.

 Pass the path of the template file and get a temporary reference to a new material 
 created from this template.
 
     CTemporaryMaterialRef tmpMatref = CreateMaterialFromTemplate(path);

 2. Use CTemporaryMaterialRef to access the material and modify it as needed. You 
 can only add things, removing or modifying is not possible. Add uniforms, defines,
 conditional defines, samplers etc.

     tmpMatref->AddConditionalDefine(...); 
	 // and possibly more...

 3. Commit the material
 Before you can use the material in the renderer, you must commit it. This gives you
 a new material reference of the type CMaterialRef and the CTemporaryMaterialRef
 object you passed to the commit function gets invalidated (you must not use it
 after committing!).
 
     CMaterialRef matRef = g_Renderer.GetMaterialManager().CommitMaterial(tmpMatRef->GetPath(), tmpMatRef));
 
 NOTE: If an identical material is already stored in the MaterialManager, commit 
 returns a reference to this material, otherwise it uses tmpMatRef to create
 a new one. Either way, tmpMatRef gets invalidated.

Steps to modify a material:
----------------------------

Because the same material can possibly be referenced by multiple references you can't
really modify a material after it's committed, but you have two options to achieve 
something similar.
 
 1. Create a new material
 Just create a new material from the template. The destructor of CMaterialRef will
 delete the existing material if this was the last reference.
 
 2. Extend material
 This is very similar to creating a new material, but you can use an existing material
 as a starting point and extend it. Call CheckoutMaterial to get a temporary reference
 to an existing material, modify it and then commit it.

**/

// TODO: Maybe some more check should be added to ensure correct use of these classes as described above.

class CMaterial;

class CTemporaryMaterialRef
{
	friend class CMaterialManager;
public:
	
	CTemporaryMaterialRef() {}
	CTemporaryMaterialRef(std::list<CMaterial>::iterator itr) :
		m_Itr(itr) {}
	
	std::list<CMaterial>::iterator operator->()
	{
		return m_Itr;
	}
	
	CMaterial& Get() { return *m_Itr; }
	
private:
	std::list<CMaterial>::iterator m_Itr;
};

/**
 * Material reference with use count.
 * 
 */
class CMaterialRef
{
	friend class CMaterialManager;
public:
	CMaterialRef() :
		m_pMaterial(nullptr)
	{
	}
	
	CMaterialRef(CMaterial* material);
	CMaterialRef(const CMaterialRef& other);
	CMaterialRef& operator=(const CMaterialRef& other);
	~CMaterialRef();
	
	// Const because the material must not be changed this way.
	// See CMagerialManager::CheckoutMaterial for information how
	// to modify the material.
	const CMaterial* operator->() const
	{
		ENSURE(m_pMaterial);
		return m_pMaterial;
	}
	
	bool isNull() const { return m_pMaterial == 0; }

private:
	CMaterial* m_pMaterial;
};

struct BlockValueAssignment
{
	size_t GetHash()
	{
		size_t h = 0;
		boost::hash_combine(h, BlockName.GetHash());
		boost::hash_combine(h, UniformName.GetHash());
		boost::hash_combine(h, Value.X);
		boost::hash_combine(h, Value.Y);
		boost::hash_combine(h, Value.Z);
		boost::hash_combine(h, Value.W);
		return h;
	}
	
	CStrIntern BlockName;
	CStrIntern UniformName;
	CVector4D Value;
};

class CMaterialTemplate
{
	friend class CMaterial;
	friend class CMaterialManager;
public:
	
	CMaterialTemplate();

	// Whether this material's shaders use alpha blending, in which case
	// models using this material need to be rendered in a special order
	// relative to the alpha-blended water plane
	void SetUsesAlphaBlending(bool flag) { m_AlphaBlending = flag; }
 	bool UsesAlphaBlending() { return m_AlphaBlending; }

	void SetShaderEffect(const CStr& effect);
	CStrIntern GetShaderEffect() const { return m_ShaderEffect; }

	// single uniforms in the default block (temporary for backwards compatibility with older code and shaders)
	const CShaderUniforms& GetStaticUniforms() const { return m_StaticUniforms; }

	// uniforms in blocks
	const std::vector<BlockValueAssignment>& GetBlockValueAssignments() const { return m_BlockValueAssignments; }
	
	const CShaderRenderQueries& GetRenderQueries() const { return m_RenderQueries; }

	const std::vector<CStrIntern>& GetRequiredSampler() const { return m_RequiredSamplers; }
	
	int GetId() const { return m_Id; }
	
	const VfsPath& GetPath() const { return m_SourceFilePath; }

private:
	
	// The following functions are private. They are meant to be used by the MaterialManager only
	void AddShaderDefine(CStrIntern key, CStrIntern value);
	void AddConditionalDefine(const char* defname, const char* defvalue, int type, std::vector<float> &args);
	void AddRequiredSampler(const CStr& samplerName);
	void AddRenderQuery(const char* key);
	void AddBlockValueAssignment(CStrIntern blockName, CStrIntern name, const CVector4D& value);
	void AddStaticUniform(const char* key, const CVector4D& value);
	
	void SetPath(const VfsPath& sourceFilePath) { m_SourceFilePath = sourceFilePath; };
	
	std::vector<CStrIntern> m_RequiredSamplers;
	
	CStrIntern m_ShaderEffect;
	CShaderDefines m_ShaderDefines;
	CShaderConditionalDefines m_ConditionalDefines;
	
	// The material might have additional block value assignments
	std::vector<BlockValueAssignment> m_BlockValueAssignments;
	
	// Storing uniforms for the template. These will be copied to each material which uses this template
	CShaderUniforms m_StaticUniforms;
	CShaderRenderQueries m_RenderQueries;
	
	VfsPath m_SourceFilePath;

	bool m_AlphaBlending;
	
	int m_Id;
};

class CMaterial
{
	friend class CMaterialManager;
public:
	
	struct TextureSampler
	{
		TextureSampler(const CStr &n, CTexturePtr t) : Name(n), Sampler(t) {}
		TextureSampler(const CStrIntern &n, CTexturePtr t) : Name(n), Sampler(t) {}
		
		CStrIntern Name;
		CTexturePtr Sampler;
		size_t GetHash()
		{
			size_t h = 0;
			boost::hash_combine(h, Name.GetHash());
			boost::hash_combine(h, Sampler);
			return h;
		}
	};
	
	typedef std::vector<TextureSampler> SamplersVector;
	
	CMaterial();
	
	void Init(CMaterialTemplate* materialTempl);
	
	void AddSampler(const TextureSampler& texture);
		
	void AddBlockValueAssignment(CStrIntern blockName, CStrIntern name, const CVector4D& value);
	const std::vector<BlockValueAssignment>& GetBlockValueAssignments() const { return m_BlockValueAssignments; }
	void AddStaticUniform(const char* key, const CVector4D& value);
	
	// Must call RecomputeCombinedShaderDefines after this, before rendering with this material
	void AddShaderDefine(CStrIntern key, CStrIntern value);
	
	// Must call RecomputeCombinedShaderDefines after this, before rendering with this material
	void AddConditionalDefine(const char* defname, const char* defvalue, int type, std::vector<float> &args);
	
	// Must be called after all AddShaderDefine and AddConditionalDefine
	void RecomputeCombinedShaderDefines();
	
	const CShaderUniforms& GetStaticUniforms() const { return m_StaticUniforms; }
	
	const CShaderConditionalDefines& GetConditionalDefines() const { return m_CombinedConditionalDefines; }

	// conditionFlags is a bitmask representing which indexes of the
	// GetConditionalDefines() list are currently matching.
	// Use 0 if you don't care about conditional defines.
	const CShaderDefines& GetShaderDefines(uint32_t conditionFlags) const { return m_CombinedShaderDefinesLookup.at(conditionFlags); }
	
	const CTexturePtr& GetDiffuseTexture() const { return m_DiffuseTexture; }
	const SamplersVector& GetSamplers() const { return m_Samplers; }
	
	int GetId() const { return m_Id; }
	int GetTemplateId() const
	{
		ENSURE(m_pTemplate);
		return m_pTemplate->GetId();
	}
	
	/// Getters forwarded from the template
	// TODO: avoid these forwarders with smarter inheritance?
	const std::vector<CStrIntern>& GetRequiredSampler() const
	{
		ENSURE(m_pTemplate);
		return m_pTemplate->GetRequiredSampler();
	}
	
	const VfsPath& GetPath() const 
	{
		ENSURE(m_pTemplate);
		return m_pTemplate->GetPath();
	}
	
	CStrIntern GetShaderEffect() const
	{
		ENSURE(m_pTemplate);
		return m_pTemplate->GetShaderEffect();
	}
	
	const CShaderRenderQueries& GetRenderQueries() const
	{
		ENSURE(m_pTemplate);
		return m_pTemplate->GetRenderQueries();
	}
	
	bool UsesAlphaBlending() const
	{
		ENSURE(m_pTemplate);
		return m_pTemplate->UsesAlphaBlending();
	}
	
	size_t GetHash() const { return m_Hash; }
	void ComputeHash();
	
private:
	
	// This pointer is kept to make it easier for the fixed pipeline to 
	// access the only texture it's interested in.
	CTexturePtr m_DiffuseTexture;
	
	SamplersVector m_Samplers;
	
	// Defines and conditional defines, excluding those from CMaterialTemplate
	CShaderDefines m_ShaderDefines;
	CShaderConditionalDefines m_ConditionalDefines;
	
	// Conditional defines, including those from CMaterialTemplate
	CShaderConditionalDefines m_CombinedConditionalDefines;
	
	// Including all (template + material and define + conditional define)
	// for each combination of flag bits (flag bits are the vector index)
	std::vector<CShaderDefines> m_CombinedShaderDefinesLookup;
	
	// The template might have additional block value assignments
	std::vector<BlockValueAssignment> m_BlockValueAssignments;
	// Initialized with the uniforms from the template and then possibly more are added
	CShaderUniforms m_StaticUniforms;
	
	CMaterialTemplate* m_pTemplate;
	
	int m_Id;
	size_t m_Hash;
};

#endif
