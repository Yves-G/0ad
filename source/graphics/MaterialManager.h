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
#include "Material.h"
#include "lib/file/vfs/vfs_path.h"

class CMaterialManager
{
public:
	CMaterialManager();
	CMaterial LoadMaterial(const VfsPath& pathname);
	const std::map<VfsPath, CMaterial>& GetAllMaterials() { return m_Materials; }

private:
	std::map<VfsPath, CMaterial> m_Materials;
	float qualityLevel;
	
	// TODO: We might want to reuse IDs in cases when we reload materials or stop using them
	static int m_NextFreeMaterialID;
};

#endif // INCLUDED_MATERIALMANAGER
