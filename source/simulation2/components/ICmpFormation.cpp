/* Copyright (C) 2019 Wildfire Games.
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

#include "ICmpFormation.h"

#include "simulation2/system/InterfaceScripted.h"
#include "simulation2/scripting/ScriptComponent.h"

BEGIN_INTERFACE_WRAPPER(Formation)
END_INTERFACE_WRAPPER(Formation)


class CCmpFormationScripted : public ICmpFormation
{
public:
	DEFAULT_SCRIPT_WRAPPER(FormationScripted)

	virtual std::vector<CFixedVector2D> ComputeFormationOffsets(std::vector<entity_id_t> active, const std::vector<CFixedVector2D>& positions, fixed angle)
	{
		return m_Script.Call<std::vector<CFixedVector2D>>("ComputeFormationOffsets", active, positions, angle);
	}

	virtual std::vector<CFixedVector2D> GetRealOffsetPositions(const std::vector<CFixedVector2D>& offsets, const CFixedVector2D& pos, fixed angle)
	{
		return m_Script.Call<std::vector<CFixedVector2D>>("GetRealOffsetPositions", offsets, pos, angle);
	}
};

REGISTER_COMPONENT_SCRIPT_WRAPPER(FormationScripted)
