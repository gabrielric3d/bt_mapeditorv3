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

#include "copybuffer.h"
#include "editor.h"
#include "gui.h"
#include "creature.h"
#include "brush.h"
#include "wall_brush.h"

#include <algorithm>
#include <unordered_map>

CopyBuffer::CopyBuffer() :
	tiles(newd BaseMap())
{
	;
}

size_t CopyBuffer::GetTileCount()
{
	return tiles ? (size_t)tiles->size() : 0;
}

BaseMap& CopyBuffer::getBufferMap()
{
	ASSERT(tiles);
	return *tiles;
}

void CopyBuffer::setBuffer(BaseMap* newTiles, const Position& newCopyPos)
{
	clear();

	if(newTiles) {
		tiles = newTiles;
	} else {
		tiles = newd BaseMap();
	}
	copyPos = newCopyPos;
}

CopyBuffer::~CopyBuffer()
{
	clear();
}

Position CopyBuffer::getPosition() const
{
	ASSERT(tiles);
	return copyPos;
}

void CopyBuffer::clear()
{
	delete tiles;
	tiles = nullptr;
}

void CopyBuffer::copy(Editor& editor, int floor, bool silent)
{
	if(!editor.hasSelection()) {
		if(!silent) {
			g_gui.SetStatusText("No tiles to copy.");
		}
		return;
	}

	clear();
	tiles = newd BaseMap();

	int tile_count = 0;
	int item_count = 0;
	copyPos = Position(0xFFFF, 0xFFFF, floor);

	for(Tile* tile : editor.getSelection()) {
		++tile_count;

		TileLocation* newlocation = tiles->createTileL(tile->getPosition());
		Tile* copied_tile = tiles->allocator(newlocation);

		if(tile->ground && tile->ground->isSelected()) {
			copied_tile->house_id = tile->house_id;
			copied_tile->setMapFlags(tile->getMapFlags());
		}

		ItemVector tile_selection = tile->getSelectedItems();
		for(ItemVector::iterator iit = tile_selection.begin(); iit != tile_selection.end(); ++iit) {
			++item_count;
			// Copy items to copybuffer
			copied_tile->addItem((*iit)->deepCopy());
		}

		if(tile->creature && tile->creature->isSelected()) {
			copied_tile->creature = tile->creature->deepCopy();
		}
		if(tile->spawn && tile->spawn->isSelected()) {
			copied_tile->spawn = tile->spawn->deepCopy();
		}

		tiles->setTile(copied_tile);

		if(copied_tile->getX() < copyPos.x)
			copyPos.x = copied_tile->getX();

		if(copied_tile->getY() < copyPos.y)
			copyPos.y = copied_tile->getY();
	}

	if(!silent) {
		std::ostringstream ss;
		ss << "Copied " << tile_count << " tile" << (tile_count > 1 ? "s" : "") <<  " (" << item_count << " item" << (item_count > 1? "s" : "") << ")";
		g_gui.SetStatusText(wxstr(ss.str()));
	}
}

void CopyBuffer::cut(Editor& editor, int floor)
{
	if(!editor.hasSelection()) {
		g_gui.SetStatusText("No tiles to cut.");
		return;
	}

	clear();
	tiles = newd BaseMap();

	Map& map = editor.getMap();
	int tile_count = 0;
	int item_count = 0;
	copyPos = Position(0xFFFF, 0xFFFF, floor);

	BatchAction* batch = editor.createBatch(ACTION_CUT_TILES);
	Action* action = editor.createAction(batch);

	PositionList tilestoborder;

	for(Tile* tile : editor.getSelection()) {
		tile_count++;

		Tile* newtile = tile->deepCopy(map);
		Tile* copied_tile = tiles->allocator(tile->getLocation());

		if(tile->ground && tile->ground->isSelected()) {
			copied_tile->house_id = newtile->house_id;
			newtile->house_id = 0;
			copied_tile->setMapFlags(tile->getMapFlags());
			newtile->setMapFlags(TILESTATE_NONE);
		}

		ItemVector tile_selection = newtile->popSelectedItems();
		for(ItemVector::iterator iit = tile_selection.begin(); iit != tile_selection.end(); ++iit) {
			item_count++;
			// Add items to copybuffer
			copied_tile->addItem(*iit);
		}

		if(newtile->creature && newtile->creature->isSelected()) {
			copied_tile->creature = newtile->creature;
			newtile->creature = nullptr;
		}

		if(newtile->spawn && newtile->spawn->isSelected()) {
			copied_tile->spawn = newtile->spawn;
			newtile->spawn = nullptr;
		}

		tiles->setTile(copied_tile->getPosition(), copied_tile);

		if(copied_tile->getX() < copyPos.x) {
			copyPos.x = copied_tile->getX();
		}

		if(copied_tile->getY() < copyPos.y) {
			copyPos.y = copied_tile->getY();
		}

		if(g_settings.getInteger(Config::USE_AUTOMAGIC)) {
			for(int y = -1; y <= 1; y++)
				for(int x = -1; x <= 1; x++)
					tilestoborder.push_back(Position(tile->getX() + x, tile->getY() + y, tile->getZ()));
		}
		action->addChange(newd Change(newtile));
	}

	batch->addAndCommitAction(action);

	// Remove duplicates
	tilestoborder.sort();
	tilestoborder.unique();

	if(g_settings.getInteger(Config::USE_AUTOMAGIC)) {
		action = editor.createAction(batch);
		for(PositionList::iterator it = tilestoborder.begin(); it != tilestoborder.end(); ++it) {
			TileLocation* location = map.createTileL(*it);
			if(location->get()) {
				Tile* new_tile = location->get()->deepCopy(map);
				new_tile->borderize(&map);
				new_tile->wallize(&map);
				action->addChange(newd Change(new_tile));
			} else {
				Tile* new_tile = map.allocator(location);
				new_tile->borderize(&map);
				if(new_tile->size()) {
					action->addChange(newd Change(new_tile));
				} else {
					delete new_tile;
				}
			}
		}

		batch->addAndCommitAction(action);
	}

	editor.addBatch(batch);
	editor.updateActions();

	std::stringstream ss;
	ss << "Cut out " << tile_count << " tile" << (tile_count > 1 ? "s" : "") <<  " (" << item_count << " item" << (item_count > 1? "s" : "") << ")";
	g_gui.SetStatusText(wxstr(ss.str()));
}

void CopyBuffer::paste(Editor& editor, const Position& toPosition)
{
	if(!tiles) {
		return;
	}

	Map& map = editor.getMap();

	BatchAction* batchAction = editor.createBatch(ACTION_PASTE_TILES);
	Action* action = editor.createAction(batchAction);
	for(MapIterator it = tiles->begin(); it != tiles->end(); ++it) {
		Tile* buffer_tile = (*it)->get();
		Position pos = buffer_tile->getPosition() - copyPos + toPosition;

		if(!pos.isValid())
			continue;

		TileLocation* location = map.createTileL(pos);
		Tile* copy_tile = buffer_tile->deepCopy(map);
		Tile* old_dest_tile = location->get();
		Tile* new_dest_tile = nullptr;
		copy_tile->setLocation(location);

		if(g_settings.getInteger(Config::MERGE_PASTE) || !copy_tile->ground) {
			if(old_dest_tile)
				new_dest_tile = old_dest_tile->deepCopy(map);
			else
				new_dest_tile = map.allocator(location);
			new_dest_tile->merge(copy_tile);
			delete copy_tile;
		} else {
			// If the copied tile has ground, replace target tile
			new_dest_tile = copy_tile;
		}

		// Add all surrounding tiles to the map, so they get borders
		map.createTile(pos.x-1, pos.y-1, pos.z);
		map.createTile(pos.x  , pos.y-1, pos.z);
		map.createTile(pos.x+1, pos.y-1, pos.z);
		map.createTile(pos.x-1, pos.y  , pos.z);
		map.createTile(pos.x+1, pos.y  , pos.z);
		map.createTile(pos.x-1, pos.y+1, pos.z);
		map.createTile(pos.x  , pos.y+1, pos.z);
		map.createTile(pos.x+1, pos.y+1, pos.z);

		action->addChange(newd Change(new_dest_tile));
	}
	batchAction->addAndCommitAction(action);

	if(g_settings.getInteger(Config::USE_AUTOMAGIC) && g_settings.getInteger(Config::BORDERIZE_PASTE)) {
		action = editor.createAction(batchAction);
		TileList borderize_tiles;

		// Go through all modified (selected) tiles (might be slow)
		for(MapIterator it = tiles->begin(); it != tiles->end(); ++it) {
			bool add_me = false; // If this tile is touched
			Position pos = (*it)->getPosition() - copyPos + toPosition;
			if(pos.z < rme::MapMinLayer || pos.z > rme::MapMaxLayer) {
				continue;
			}
			// Go through all neighbours
			Tile* t;
			t = map.getTile(pos.x-1, pos.y-1, pos.z); if(t && !t->isSelected()) { borderize_tiles.push_back(t); add_me = true; }
			t = map.getTile(pos.x  , pos.y-1, pos.z); if(t && !t->isSelected()) { borderize_tiles.push_back(t); add_me = true; }
			t = map.getTile(pos.x+1, pos.y-1, pos.z); if(t && !t->isSelected()) { borderize_tiles.push_back(t); add_me = true; }
			t = map.getTile(pos.x-1, pos.y  , pos.z); if(t && !t->isSelected()) { borderize_tiles.push_back(t); add_me = true; }
			t = map.getTile(pos.x+1, pos.y  , pos.z); if(t && !t->isSelected()) { borderize_tiles.push_back(t); add_me = true; }
			t = map.getTile(pos.x-1, pos.y+1, pos.z); if(t && !t->isSelected()) { borderize_tiles.push_back(t); add_me = true; }
			t = map.getTile(pos.x  , pos.y+1, pos.z); if(t && !t->isSelected()) { borderize_tiles.push_back(t); add_me = true; }
			t = map.getTile(pos.x+1, pos.y+1, pos.z); if(t && !t->isSelected()) { borderize_tiles.push_back(t); add_me = true; }
			if(add_me) borderize_tiles.push_back(map.getTile(pos));
		}
		// Remove duplicates
		borderize_tiles.sort();
		borderize_tiles.unique();

		for(Tile* tile : borderize_tiles) {
			if(tile) {
				Tile* newTile = tile->deepCopy(map);
				newTile->borderize(&map);

				if(tile->ground && tile->ground->isSelected()) {
					newTile->selectGround();
				}

				newTile->wallize(&map);
				action->addChange(newd Change(newTile));
			}
		}

		// Commit changes to map
		batchAction->addAndCommitAction(action);
	}

	editor.addBatch(batchAction);
	editor.updateActions();
}

void CopyBuffer::rotate(int quarterTurns)
{
	if(!tiles || tiles->size() == 0) {
		return;
	}

	int turns = quarterTurns % 4;
	if(turns < 0) {
		turns += 4;
	}
	if(turns == 0) {
		return;
	}

	bool hasPos = false;
	Position minPos;
	Position maxPos;

	for(MapIterator it = tiles->begin(); it != tiles->end(); ++it) {
		Tile* tile = (*it)->get();
		if(!tile) {
			continue;
		}

		const Position& pos = tile->getPosition();
		if(!hasPos) {
			minPos = pos;
			maxPos = pos;
			hasPos = true;
		} else {
			minPos.x = std::min(minPos.x, pos.x);
			minPos.y = std::min(minPos.y, pos.y);
			maxPos.x = std::max(maxPos.x, pos.x);
			maxPos.y = std::max(maxPos.y, pos.y);
		}
	}

	if(!hasPos) {
		return;
	}

	const int width = maxPos.x - minPos.x + 1;
	const int height = maxPos.y - minPos.y + 1;

	// Border items are position-dependent within their border group. They do not necessarily
	// follow rotateTo chains, so we remap the border alignment instead.
	const auto rotate_border_type_cw_once = [](BorderType type) -> BorderType {
		switch(type) {
			case NORTH_HORIZONTAL: return EAST_HORIZONTAL;
			case EAST_HORIZONTAL: return SOUTH_HORIZONTAL;
			case SOUTH_HORIZONTAL: return WEST_HORIZONTAL;
			case WEST_HORIZONTAL: return NORTH_HORIZONTAL;

			// NOTE: enum order is NW(5), NE(6), SW(7), SE(8) (not clockwise)
			case NORTHWEST_CORNER: return NORTHEAST_CORNER;
			case NORTHEAST_CORNER: return SOUTHEAST_CORNER;
			case SOUTHEAST_CORNER: return SOUTHWEST_CORNER;
			case SOUTHWEST_CORNER: return NORTHWEST_CORNER;

			case NORTHWEST_DIAGONAL: return NORTHEAST_DIAGONAL;
			case NORTHEAST_DIAGONAL: return SOUTHEAST_DIAGONAL;
			case SOUTHEAST_DIAGONAL: return SOUTHWEST_DIAGONAL;
			case SOUTHWEST_DIAGONAL: return NORTHWEST_DIAGONAL;

			default:
				return type;
		}
	};

	const auto rotate_border_type = [&](BorderType type, int cwTurns) -> BorderType {
		int t = cwTurns % 4;
		if(t < 0) {
			t += 4;
		}
		BorderType out = type;
		for(int i = 0; i < t; ++i) {
			out = rotate_border_type_cw_once(out);
		}
		return out;
	};

	// Cache: itemId -> AutoBorder* (found by scanning loaded borders once per unique itemId).
	std::unordered_map<uint16_t, const AutoBorder*> border_for_item_id;
	border_for_item_id.reserve(128);

	const auto get_border_for_item = [&](uint16_t itemId, BorderType alignmentHint) -> const AutoBorder* {
		auto it = border_for_item_id.find(itemId);
		if(it != border_for_item_id.end()) {
			return it->second;
		}

		const AutoBorder* border = g_brushes.findAutoBorderByBorderItem(itemId, alignmentHint);
		border_for_item_id.emplace(itemId, border);
		return border;
	};

	// Wall items are defined by wall brushes and use a different alignment enum section.
	// They don't have rotateTo chains; rotate the wall alignment and swap the ID within the same wall brush.
	const auto rotate_wall_alignment_cw_once = [](BorderType type) -> BorderType {
		switch(type) {
			case WALL_POLE: return WALL_POLE;

			case WALL_VERTICAL: return WALL_HORIZONTAL;
			case WALL_HORIZONTAL: return WALL_VERTICAL;

			case WALL_NORTH_END: return WALL_EAST_END;
			case WALL_EAST_END: return WALL_SOUTH_END;
			case WALL_SOUTH_END: return WALL_WEST_END;
			case WALL_WEST_END: return WALL_NORTH_END;

			case WALL_NORTH_T: return WALL_EAST_T;
			case WALL_EAST_T: return WALL_SOUTH_T;
			case WALL_SOUTH_T: return WALL_WEST_T;
			case WALL_WEST_T: return WALL_NORTH_T;

			case WALL_NORTHWEST_DIAGONAL: return WALL_NORTHEAST_DIAGONAL;
			case WALL_NORTHEAST_DIAGONAL: return WALL_SOUTHEAST_DIAGONAL;
			case WALL_SOUTHEAST_DIAGONAL: return WALL_SOUTHWEST_DIAGONAL;
			case WALL_SOUTHWEST_DIAGONAL: return WALL_NORTHWEST_DIAGONAL;

			case WALL_INTERSECTION: return WALL_INTERSECTION;
			case WALL_UNTOUCHABLE: return WALL_UNTOUCHABLE;

			default:
				return type;
		}
	};

	const auto rotate_wall_alignment = [&](BorderType type, int cwTurns) -> BorderType {
		int t = cwTurns % 4;
		if(t < 0) {
			t += 4;
		}
		BorderType out = type;
		for(int i = 0; i < t; ++i) {
			out = rotate_wall_alignment_cw_once(out);
		}
		return out;
	};

	struct WallBrushCatalog {
		std::vector<uint16_t> byAlignment[17];
		std::unordered_map<uint32_t, std::vector<uint16_t>> doorsByKey;
	};

	bool wall_catalog_built = false;
	std::unordered_map<const WallBrush*, WallBrushCatalog> wall_catalog_by_brush;

	const auto build_door_key = [](BorderType alignment, ::DoorType doorType, bool open) -> uint32_t {
		return (static_cast<uint32_t>(static_cast<uint8_t>(alignment)) & 0xFFu) |
			((static_cast<uint32_t>(static_cast<uint8_t>(doorType)) & 0xFFu) << 8) |
			((open ? 1u : 0u) << 16);
	};

	const auto ensure_wall_catalogs = [&]() {
		if(wall_catalog_built) {
			return;
		}

		for(uint16_t id = g_items.getMinID(); id <= g_items.getMaxID(); ++id) {
			if(!g_items.isValidID(id)) {
				continue;
			}
			const ItemType& type = g_items.getItemType(id);
			if(!type.isWall || !type.brush || !type.brush->isWall()) {
				continue;
			}

			const WallBrush* brush = type.brush->asWall();
			if(!brush) {
				continue;
			}

			const int alignmentIndex = static_cast<int>(type.border_alignment);
			if(alignmentIndex < 0 || alignmentIndex >= 17) {
				continue;
			}

			WallBrushCatalog& catalog = wall_catalog_by_brush[brush];
			if(type.isBrushDoor) {
				const ::DoorType doorType = const_cast<WallBrush*>(brush)->getDoorTypeFromID(id);
				const uint32_t key = build_door_key(type.border_alignment, doorType, type.isOpen);
				catalog.doorsByKey[key].push_back(id);
			} else {
				catalog.byAlignment[alignmentIndex].push_back(id);
			}
		}

		for(auto& pair : wall_catalog_by_brush) {
			WallBrushCatalog& catalog = pair.second;
			for(int i = 0; i < 17; ++i) {
				std::sort(catalog.byAlignment[i].begin(), catalog.byAlignment[i].end());
			}
			for(auto& doorPair : catalog.doorsByKey) {
				std::vector<uint16_t>& ids = doorPair.second;
				std::sort(ids.begin(), ids.end());
			}
		}

		wall_catalog_built = true;
	};

	auto rotate_position = [&](const Position& pos) -> Position {
		const int rx = pos.x - minPos.x;
		const int ry = pos.y - minPos.y;
		switch(turns) {
			case 1: // 90 CW
				return Position(minPos.x + (height - 1 - ry), minPos.y + rx, pos.z);
			case 2: // 180
				return Position(minPos.x + (width - 1 - rx), minPos.y + (height - 1 - ry), pos.z);
			case 3: // 90 CCW
				return Position(minPos.x + ry, minPos.y + (width - 1 - rx), pos.z);
			default:
				return pos;
		}
	};

	auto rotate_item = [&](Item* item) {
		if(!item) {
			return;
		}

		// Rotate borders via border group/alignment remap.
		// This is required so internal borders stay correct when rotating mixed ground types.
		if(item->isBorder() || item->isOptionalBorder()) {
			const BorderType current = item->getBorderAlignment();
			const BorderType rotated = rotate_border_type(current, turns);

			if(rotated != BORDER_NONE) {
				const AutoBorder* border = get_border_for_item(item->getID(), current);
				if(border && border->tiles[rotated] != 0) {
					const uint16_t newId = static_cast<uint16_t>(border->tiles[rotated]);
					if(newId != item->getID()) {
						item->setID(newId);
					}
					return;
				}
			}
			// Fall back to rotateTo (some border items may still use it, or borders not found in borders.xml)
		}

		// Rotate wall items by remapping their wall alignment within the same wall brush.
		// This ensures rotated wall clusters preserve structure even when automagic is disabled.
		if(item->isWall()) {
			WallBrush* brush = item->getWallBrush();
			if(brush) {
				ensure_wall_catalogs();

				const BorderType current = item->getWallAlignment();
				const BorderType rotated = rotate_wall_alignment(current, turns);
				if(rotated != current) {
					auto catalogIt = wall_catalog_by_brush.find(brush);
					if(catalogIt != wall_catalog_by_brush.end()) {
						WallBrushCatalog& catalog = catalogIt->second;
						const int currentIndex = static_cast<int>(current);
						const int rotatedIndex = static_cast<int>(rotated);

						if(item->isBrushDoor()) {
							const ::DoorType doorType = brush->getDoorTypeFromID(item->getID());
							const bool open = item->isOpen();

							const uint32_t oldKey = build_door_key(current, doorType, open);
							const uint32_t newKey = build_door_key(rotated, doorType, open);
							auto oldIt = catalog.doorsByKey.find(oldKey);
							auto newIt = catalog.doorsByKey.find(newKey);

							if(newIt != catalog.doorsByKey.end() && !newIt->second.empty()) {
								size_t idx = 0;
								if(oldIt != catalog.doorsByKey.end()) {
									const auto& oldIds = oldIt->second;
									auto findIt = std::find(oldIds.begin(), oldIds.end(), item->getID());
									if(findIt != oldIds.end()) {
										idx = static_cast<size_t>(findIt - oldIds.begin());
									}
								}
								const auto& newIds = newIt->second;
								const uint16_t newId = newIds[idx % newIds.size()];
								if(newId != 0 && newId != item->getID()) {
									item->setID(newId);
								}
								return;
							}
						} else if(currentIndex >= 0 && currentIndex < 17 && rotatedIndex >= 0 && rotatedIndex < 17) {
							const auto& oldIds = catalog.byAlignment[currentIndex];
							const auto& newIds = catalog.byAlignment[rotatedIndex];
							if(!newIds.empty()) {
								size_t idx = 0;
								auto findIt = std::find(oldIds.begin(), oldIds.end(), item->getID());
								if(findIt != oldIds.end()) {
									idx = static_cast<size_t>(findIt - oldIds.begin());
								}
								const uint16_t newId = newIds[idx % newIds.size()];
								if(newId != 0 && newId != item->getID()) {
									item->setID(newId);
								}
								return;
							}
						}
					}
				}
			}
		}

		for(int i = 0; i < turns; ++i) {
			item->doRotate();
		}
	};

	auto rotate_tile_items = [&](Tile* tile) {
		if(!tile) {
			return;
		}
		rotate_item(tile->ground);
		for(Item* item : tile->items) {
			rotate_item(item);
		}
	};

	struct PendingTile {
		Tile* tile = nullptr;
		Position pos;
	};

	std::vector<PendingTile> rotatedTiles;
	rotatedTiles.reserve(static_cast<size_t>(tiles->size()));

	Position newMinPos = Position(0xFFFF, 0xFFFF, copyPos.z);

	for(MapIterator it = tiles->begin(); it != tiles->end(); ++it) {
		Tile* oldTile = (*it)->get();
		if(!oldTile) {
			continue;
		}

		Tile* rotatedTile = oldTile->deepCopy(*tiles);
		const Position newPos = rotate_position(oldTile->getPosition());

		rotate_tile_items(rotatedTile);

		newMinPos.x = std::min(newMinPos.x, newPos.x);
		newMinPos.y = std::min(newMinPos.y, newPos.y);

		rotatedTiles.push_back(PendingTile{rotatedTile, newPos});
	}

	tiles->clear(true);

	for(PendingTile& entry : rotatedTiles) {
		if(!entry.tile) {
			continue;
		}
		TileLocation* location = tiles->createTileL(entry.pos);
		entry.tile->setLocation(location);
		tiles->setTile(entry.pos, entry.tile);
	}

	// Ensure walls are consistent with their new neighbors.
	// This also fixes wall decorations that depend on the wall alignment.
	for(MapIterator it = tiles->begin(); it != tiles->end(); ++it) {
		Tile* tile = (*it)->get();
		if(tile && tile->hasWall()) {
			tile->wallize(tiles);
		}
	}

	copyPos.x = newMinPos.x;
	copyPos.y = newMinPos.y;
}

bool CopyBuffer::canPaste() const
{
	return tiles && tiles->size() != 0;
}

