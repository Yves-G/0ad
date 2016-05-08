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

#include "precompiled.h"

#include "MaterialManager.h"

#include "lib/ogl.h"
#include "maths/MathUtil.h"
#include "maths/Vector4D.h"
#include "ps/CLogger.h"
#include "ps/ConfigDB.h"
#include "ps/Filesystem.h"
#include "ps/PreprocessorWrapper.h"
#include "ps/XML/Xeromyces.h"
#include "renderer/Renderer.h"
#include "graphics/UniformBlockManager.h"

#include <sstream>

int CMaterialManager::m_NextFreeMaterialID = 0;
int CMaterialManager::m_NextFreeTemplateID = 0;

CMaterialManager::CMaterialManager()
{
	qualityLevel = 5.0;
	CFG_GET_VAL("materialmgr.quality", qualityLevel);
	qualityLevel = clamp(qualityLevel, 0.0f, 10.0f);

	if (VfsDirectoryExists(L"art/materials/") && !CXeromyces::AddValidator(g_VFS, "material", "art/materials/material.rng"))
		LOGERROR("CMaterialManager: failed to load grammar file 'art/materials/material.rng'");
}

CTemporaryMaterialRef CMaterialManager::CreateMaterialFromTemplate(const VfsPath& path)
{
	CMaterialTemplate* matTempl = LoadMaterialTemplate(path);
	CMaterial material;
	material.Init(matTempl);
	m_TemporaryMaterials.push_back(material);
	auto itr = m_TemporaryMaterials.end();
	itr--;
	return CTemporaryMaterialRef(itr);
}

CTemporaryMaterialRef CMaterialManager::CheckoutMaterial(CMaterialRef matRef)
{
	m_TemporaryMaterials.push_back(*matRef.m_pMaterial);
	auto itr = m_TemporaryMaterials.end();
	itr--;
	return CTemporaryMaterialRef(itr);
}

CMaterialRef CMaterialManager::CommitMaterial(const VfsPath& path, CTemporaryMaterialRef materialRef)
{
	std::map<size_t, CMaterial>& matMap = m_Materials[path];
	materialRef->ComputeHash();
	std::map<size_t, CMaterial>::iterator itr = matMap.find(materialRef->GetHash());
	if (itr == matMap.end())
	{
		itr = matMap.insert(std::make_pair(materialRef->GetHash(), materialRef.Get())).first;
		itr->second.m_Id = m_NextFreeMaterialID++;
		UniformBlockManager& uniformBlockManager = g_Renderer.GetUniformBlockManager();
		uniformBlockManager.MaterialCommitted(itr->second);
	}
	m_TemporaryMaterials.erase(materialRef.m_Itr);
	
	#if 0 // Just temporarily for debugging to get an idea
	// how many templates and materials are used.
	struct MaterialDebugInformaton
	{
		int nbrTempls;
		int nbrMats;
	} matDbgInfo;
	
	matDbgInfo.nbrTempls = m_MaterialTemplates.size();
	for (auto& itr  : m_Materials)
		matDbgInfo.nbrMats += itr.second.size();
	
	#endif // 1

	return CMaterialRef(&itr->second);
}

//CMaterial CMaterialManager::LoadMaterial(const VfsPath& pathname)
CMaterialTemplate* CMaterialManager::LoadMaterialTemplate(const VfsPath& pathname)
{
	// TODO: ...
	if (pathname.empty())
		debug_warn("Empty pathname!");
		//return CMaterial();

	std::map<VfsPath, CMaterialTemplate>::iterator iter = m_MaterialTemplates.find(pathname);
	if (iter != m_MaterialTemplates.end())
		return &iter->second;

	CXeromyces xeroFile;
	if (xeroFile.Load(g_VFS, pathname, "material") != PSRETURN_OK)
		debug_warn("Error loading material template file!"); // TODO: ...
		//return CMaterial();

	#define EL(x) int el_##x = xeroFile.GetElementID(#x)
	#define AT(x) int at_##x = xeroFile.GetAttributeID(#x)
	EL(alpha_blending);
	EL(alternative);
	EL(define);
	EL(shader);
	EL(uniform);
	EL(blockuniform);
	EL(renderquery);
	EL(required_texture);
	EL(conditional_define);
	AT(effect);
	AT(if);
	AT(define);
	AT(quality);
	AT(material);
	AT(name);
	AT(blockname);
	AT(instanced);
	AT(value);
	AT(type);
	AT(min);
	AT(max);
	AT(conf);
	#undef AT
	#undef EL

	CMaterialTemplate matTempl;
	matTempl.SetPath(pathname);
	matTempl.m_Id = m_NextFreeTemplateID++;

	XMBElement root = xeroFile.GetRoot();
	
	CPreprocessorWrapper preprocessor;
	preprocessor.AddDefine("CFG_FORCE_ALPHATEST", g_Renderer.m_Options.m_ForceAlphaTest ? "1" : "0");

	XERO_ITER_EL(root, node)
	{
		int token = node.GetNodeName();
		XMBAttributeList attrs = node.GetAttributes();
		if (token == el_alternative)
		{
			CStr cond = attrs.GetNamedItem(at_if);
			if (cond.empty() || !preprocessor.TestConditional(cond))
			{
				cond = attrs.GetNamedItem(at_quality);
				if (cond.empty())
					continue;
				else
				{	
					if (cond.ToFloat() <= qualityLevel)
						continue;
				}
			}
			
			m_NextFreeTemplateID--; // We ditch the matTempl we already initialized and can free its ID
			return LoadMaterialTemplate(VfsPath("art/materials") / attrs.GetNamedItem(at_material).FromUTF8());
		}
		else if (token == el_alpha_blending)
		{
			matTempl.SetUsesAlphaBlending(true);
		}
		else if (token == el_shader)
		{
			matTempl.SetShaderEffect(attrs.GetNamedItem(at_effect));
		}
		else if (token == el_define)
		{
			matTempl.AddShaderDefine(CStrIntern(attrs.GetNamedItem(at_name)), CStrIntern(attrs.GetNamedItem(at_value)));
		}
		else if (token == el_conditional_define)
		{
			std::vector<float> args;
			
			CStr type = attrs.GetNamedItem(at_type).c_str();
			int typeID = -1;
			
			if (type == CStr("draw_range"))
			{
				typeID = DCOND_DISTANCE;
				
				float valmin = -1.0f; 
				float valmax = -1.0f;
				
				CStr conf = attrs.GetNamedItem(at_conf);
				if (!conf.empty())
				{
					CFG_GET_VAL("materialmgr." + conf + ".min", valmin);
					CFG_GET_VAL("materialmgr." + conf + ".max", valmax);
				}
				else
				{
					CStr dmin = attrs.GetNamedItem(at_min);
					if (!dmin.empty())
						valmin = attrs.GetNamedItem(at_min).ToFloat();
					
					CStr dmax = attrs.GetNamedItem(at_max);
					if (!dmax.empty())
						valmax = attrs.GetNamedItem(at_max).ToFloat();
				}
				
				args.push_back(valmin);
				args.push_back(valmax);
				
				if (valmin >= 0.0f)
				{
					std::stringstream sstr;
					sstr << valmin;
					matTempl.AddShaderDefine(CStrIntern(conf + "_MIN"), CStrIntern(sstr.str()));
				}
				
				if (valmax >= 0.0f)
				{	
					std::stringstream sstr;
					sstr << valmax;
					matTempl.AddShaderDefine(CStrIntern(conf + "_MAX"), CStrIntern(sstr.str()));
				}
			}
			
			matTempl.AddConditionalDefine(attrs.GetNamedItem(at_name).c_str(), 
						      attrs.GetNamedItem(at_value).c_str(), 
						      typeID, args);
		}		
		else if (token == el_uniform)
		{
			std::stringstream str(attrs.GetNamedItem(at_value));
			CVector4D vec;
			str >> vec.X >> vec.Y >> vec.Z >> vec.W;
			matTempl.AddStaticUniform(attrs.GetNamedItem(at_name).c_str(), vec);
			
			CStr blockName = attrs.GetNamedItem(at_blockname);
			if (!blockName.empty())
			{
				bool isInstanced = attrs.GetNamedItem(at_instanced).ToInt();

				// For block uniforms, we automatically convert to array type
				// TODO: Remove "isInstanced?"
				matTempl.AddBlockValueAssignment(CStrIntern(blockName),
				  CStrIntern(attrs.GetNamedItem(at_name).append("[0]").c_str()),
				  vec);
			}			                  
		}
		/*
		else if (token == el_blockuniform)
		{
			std::stringstream str(attrs.GetNamedItem(at_value));
			CVector4D vec;
			str >> vec.X >> vec.Y >> vec.Z >> vec.W;
			bool isInstanced = attrs.GetNamedItem(at_instanced).ToInt();
			
			material.AddStaticBlockUniform(CStrIntern(attrs.GetNamedItem(at_blockname)),
			                  CStrIntern(attrs.GetNamedItem(at_name).c_str()),
							  isInstanced,
			                  vec);
		}*/
		else if (token == el_renderquery)
		{
			matTempl.AddRenderQuery(attrs.GetNamedItem(at_name).c_str());
		}
		else if (token == el_required_texture)
		{
			matTempl.AddRequiredSampler(attrs.GetNamedItem(at_name));
			if (!attrs.GetNamedItem(at_define).empty())
				matTempl.AddShaderDefine(CStrIntern(attrs.GetNamedItem(at_define)), str_1);
		}
	}

	std::cout << "MaterialTemplate loaded: " << pathname.string8().c_str() << "   ID: " << matTempl.GetId() << std::endl;
	m_MaterialTemplates[pathname] = matTempl;
	
	UniformBlockManager& uniformBlockManager = g_Renderer.GetUniformBlockManager();
	uniformBlockManager.MaterialTemplateAdded(m_MaterialTemplates[pathname]);
	return &m_MaterialTemplates[pathname];
}

void CMaterialManager::RegisterMaterialRef(const CMaterialRef& matRef)
{
	const size_t h = matRef->GetHash();
	auto itr = m_MatRefCount.find(h);
	if (itr != m_MatRefCount.end())
		itr->second++;
	else
		m_MatRefCount[h] = 1;
}

void CMaterialManager::UnRegisterMaterialRef(const CMaterialRef& matRef)
{
	const size_t h = matRef->GetHash();
	auto itr = m_MatRefCount.find(h);
	ENSURE(itr != m_MatRefCount.end());
	if (itr->second == 1)
	{
		m_MatRefCount.erase(itr);
		// TODO: Check if it might be worth to keep the material for a while even though the reference
		// count reached 0.
		m_Materials[matRef->GetPath()].erase(h);
	}
	else
		itr->second--;
}
