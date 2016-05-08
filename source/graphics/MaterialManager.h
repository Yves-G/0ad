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

#ifndef INCLUDED_MATERIALMANAGER
#define INCLUDED_MATERIALMANAGER

#include <map>
#include <boost/bimap/bimap.hpp>
#include <boost/bimap/multiset_of.hpp>
#include <set>
#include <list>
#include <memory>
#include "Material.h"
#include "lib/file/vfs/vfs_path.h"

typedef std::map<VfsPath, std::map<size_t, CMaterial> > MaterialStoreT;

class CMaterialManager
{
public:
	CMaterialManager();
	CTemporaryMaterialRef CheckoutMaterial(CMaterialRef matRef);
	CMaterialRef CommitMaterial(const VfsPath& path, CTemporaryMaterialRef materialRef);
	CTemporaryMaterialRef CreateMaterialFromTemplate(const VfsPath& path);
	const MaterialStoreT& GetAllMaterials() { return m_Materials; }
	const std::map<VfsPath, CMaterialTemplate>& GetAllMaterialTemplates() { return m_MaterialTemplates; }
	
	void RegisterMaterialRef(const CMaterialRef& matRef);
	void UnRegisterMaterialRef(const CMaterialRef& matRef);
	
	// This function finds all materials using the texture specified by tex and calls the
	// MaterialTextureChange function/event on them.
	//
	// TODO: when this works, think if it would be better to put this into UniformBlockManager.
	void OnTextureUpdated(CTexture* tex);

private:
	
	CMaterialTemplate* LoadMaterialTemplate(const VfsPath& path);
	
	std::map<VfsPath, CMaterialTemplate> m_MaterialTemplates;
	MaterialStoreT m_Materials;
	// Materials still being modified by some external class and not committed yet
	std::list<CMaterial> m_TemporaryMaterials;
	
	// Textures aren't necessarily loaded when the material is committed. This stores the
	// mapping between materials and textures for two purposes.
	// 1. When a texture gets loaded, trigger an event for all materials using that texture
	// to update the texture handles in the storage block (data for shaders).
	// 2. When a material gets deleted, efficiently find and delete all associated entries
	// in m_MatTexLookup.
	typedef boost::bimaps::bimap<boost::bimaps::multiset_of<CMaterial*>, 
		boost::bimaps::multiset_of<CTexture*> > MatTexLookupT;
	MatTexLookupT m_MatTexLookup;

	std::map<size_t, int> m_MatRefCount;
	
	float qualityLevel;
	
	// TODO: We might want to reuse IDs in cases when we reload materials or stop using them
	static int m_NextFreeMaterialID;
	static int m_NextFreeTemplateID;
};

#endif // INCLUDED_MATERIALMANAGER
