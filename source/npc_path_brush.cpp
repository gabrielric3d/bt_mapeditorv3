//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////
// Remere's Map Editor is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Remere's Map Editor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <http://www.gnu.org/licenses/>.
//////////////////////////////////////////////////////////////////////

#include "main.h"

#include "npc_path_brush.h"

NPCPathBrush::NPCPathBrush() :
	Brush()
{
}

NPCPathBrush::~NPCPathBrush()
{
}

bool NPCPathBrush::canDraw(BaseMap* map, const Position& position) const
{
	(void)map;
	return position.isValid();
}

void NPCPathBrush::draw(BaseMap* map, Tile* tile, void* parameter)
{
	(void)map;
	(void)tile;
	(void)parameter;
	ASSERT(_MSG("NPCPathBrush::draw should not be called directly."));
}

void NPCPathBrush::undraw(BaseMap* map, Tile* tile)
{
	(void)map;
	(void)tile;
	ASSERT(_MSG("NPCPathBrush::undraw should not be called directly."));
}
