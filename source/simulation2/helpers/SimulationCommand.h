/* Copyright (C) 2010 Wildfire Games.
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

#ifndef INCLUDED_SIMULATIONCOMMAND
#define INCLUDED_SIMULATIONCOMMAND

#include "scriptinterface/ScriptInterface.h"
#include "simulation2/helpers/Player.h"

/**
 * Simulation command, typically received over the network in multiplayer games.
 */
struct SimulationCommand
{
	player_id_t player;
	
	JS::PersistentRootedValue& GetData() const { return *data; }
	void SetData(JSContext* cx, JS::HandleValue val) 
	{
		data.reset(new JS::PersistentRootedValue(cx, val));
	};
	
private:
	// Manage the root as a pointer to avoid having to:
	//  - Manually define the copy constructor and copy assignment operator.
	//  - Store a reference to a ScriptInterface, JSContext or JSRuntime sowhere in this class
	std::shared_ptr<JS::PersistentRootedValue> data;
};

#endif // INCLUDED_SIMULATIONCOMMAND
