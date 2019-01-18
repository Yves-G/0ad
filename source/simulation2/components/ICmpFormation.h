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
#ifndef INCLUDED_ICMPFORMATION
#define INCLUDED_ICMPFORMATION

#include "maths/FixedVector2D.h"
#include "simulation2/system/Interface.h"

/**
 * Documentation to describe what this interface and its associated component types are
 * for, and roughly how they should be used.
 */
class ICmpFormation : public IComponent
{
public:
	/**
	* Documentation for each method.
	*/
	virtual std::vector<CFixedVector2D> ComputeFormationOffsets(std::vector<entity_id_t> active, const std::vector<CFixedVector2D>& positions, fixed angle) = 0;
	virtual std::vector<CFixedVector2D> GetRealOffsetPositions(const std::vector<CFixedVector2D>& offsets, const CFixedVector2D& pos, fixed angle) = 0;

	DECLARE_INTERFACE_TYPE(Formation)
};
#endif // INCLUDED_ICMPFORMATION
