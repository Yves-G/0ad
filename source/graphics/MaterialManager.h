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

private:
	
	CMaterialTemplate* LoadMaterialTemplate(const VfsPath& path);
	
	std::map<VfsPath, CMaterialTemplate> m_MaterialTemplates;
	MaterialStoreT m_Materials;
	// Materials still being modified by some external class and not committed yet
	std::list<CMaterial> m_TemporaryMaterials;
	
	std::map<size_t, int> m_MatRefCount;
	
	float qualityLevel;
	
	// TODO: We might want to reuse IDs in cases when we reload materials or stop using them
	static int m_NextFreeMaterialID;
	static int m_NextFreeTemplateID;
};

#endif // INCLUDED_MATERIALMANAGER
