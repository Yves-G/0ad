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

#include "ICmpBattalion.h"

#include "simulation2/system/InterfaceScripted.h"
#include "simulation2/scripting/ScriptComponent.h"

BEGIN_INTERFACE_WRAPPER(Battalion)
END_INTERFACE_WRAPPER(Battalion)


class CCmpBattalionScripted : public ICmpBattalion
{
public:
	DEFAULT_SCRIPT_WRAPPER(BattalionScripted)

	virtual std::vector<entity_id_t> GetMembers()
	{
		return m_Script.Call<std::vector<entity_id_t>>("GetMembers");
	}

	virtual void SetMembers(std::vector<entity_id_t> entities)
	{
		m_Script.CallVoid("SetMembers", entities);
	}

	virtual entity_id_t GetFormationEntity()
	{
		return m_Script.Call<entity_id_t>("GetFormationEntity");
	}

	virtual void SetFormationEntity(entity_id_t formationEnt)
	{
		m_Script.CallVoid("SetFormationEntity", formationEnt);
	}

	virtual void CreateFormation()
	{
		m_Script.CallVoid("CreateFormation");
	}
};

REGISTER_COMPONENT_SCRIPT_WRAPPER(BattalionScripted)
