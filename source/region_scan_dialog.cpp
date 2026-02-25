//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "main.h"

#include "region_scan_dialog.h"
#include "gui.h"
#include "positionctrl.h"
#include "result_window.h"
#include "map.h"
#include "tile.h"
#include "creature.h"
#include "spawn.h"
#include "editor.h"

#include <wx/wx.h>
#include <wx/clipbrd.h>
#include <sstream>
#include <algorithm>

extern GUI g_gui;

namespace {

// Parse integers from a clipboard string.
// Returns the extracted integer values (up to maxValues).
std::vector<int> extractIntegers(const std::string& text, size_t maxValues = 6) {
	std::vector<int> values;
	std::string num;
	for(size_t i = 0; i < text.size() && values.size() < maxValues; ++i) {
		if(text[i] >= '0' && text[i] <= '9') {
			num += text[i];
		} else if(!num.empty()) {
			try { values.push_back(std::stoi(num)); } catch(...) {}
			num.clear();
		}
	}
	if(!num.empty() && values.size() < maxValues) {
		try { values.push_back(std::stoi(num)); } catch(...) {}
	}
	return values;
}

} // anonymous namespace

void RegionScanDialog::Show(wxWindow* parent) {
	if(!g_gui.IsEditorOpen())
		return;

	Map& map = g_gui.GetCurrentMap();

	wxDialog dlg(parent, wxID_ANY, "Scan Region", wxDefaultPosition, wxDefaultSize, wxRESIZE_BORDER | wxCAPTION | wxCLOSE_BOX);

	wxBoxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);

	// Position controls
	wxBoxSizer* pos_sizer = newd wxBoxSizer(wxHORIZONTAL);
	PositionCtrl* from_pos = newd PositionCtrl(&dlg, "From Position", 0, 0, 0);
	PositionCtrl* to_pos = newd PositionCtrl(&dlg, "To Position", 0, 0, 0);
	pos_sizer->Add(from_pos, 1, wxEXPAND | wxRIGHT, 5);
	pos_sizer->Add(to_pos, 1, wxEXPAND | wxLEFT, 5);
	topsizer->Add(pos_sizer, 0, wxEXPAND | wxALL, 10);

	// Paste region button
	wxButton* paste_button = newd wxButton(&dlg, wxID_PASTE, "Paste Region");
	topsizer->Add(paste_button, 0, wxALIGN_CENTER | wxBOTTOM, 5);

	// Buttons row
	wxBoxSizer* btn_sizer = newd wxBoxSizer(wxHORIZONTAL);
	wxButton* scan_button = newd wxButton(&dlg, wxID_APPLY, "Scan");
	wxButton* remove_button = newd wxButton(&dlg, wxID_DELETE, "Clear Everything");
	wxButton* list_button = newd wxButton(&dlg, wxID_MORE, "List Items");
	btn_sizer->Add(scan_button, 0, wxRIGHT, 10);
	btn_sizer->Add(remove_button, 0, wxRIGHT, 10);
	btn_sizer->Add(list_button, 0);
	topsizer->Add(btn_sizer, 0, wxALIGN_CENTER | wxBOTTOM, 10);

	// Results text
	wxTextCtrl* results = newd wxTextCtrl(&dlg, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
	results->SetMinSize(wxSize(500, 350));
	topsizer->Add(results, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

	// Close button
	wxButton* close_button = newd wxButton(&dlg, wxID_CANCEL, "Close");
	topsizer->Add(close_button, 0, wxALIGN_CENTER | wxBOTTOM, 10);

	dlg.SetSizerAndFit(topsizer);
	dlg.Centre(wxBOTH);

	// Bind paste region button
	paste_button->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
		if(!wxTheClipboard->Open())
			return;

		if(!wxTheClipboard->IsSupported(wxDF_TEXT)) {
			wxTheClipboard->Close();
			return;
		}

		wxTextDataObject data;
		wxTheClipboard->GetData(data);
		wxTheClipboard->Close();

		std::string text = data.GetText().ToStdString();
		if(text.empty())
			return;

		std::vector<int> values = extractIntegers(text);

		if(values.size() == 6) {
			// Range: {fromx, tox, fromy, toy, fromz, toz}
			from_pos->SetPosition(Position(values[0], values[2], values[4]));
			to_pos->SetPosition(Position(values[1], values[3], values[5]));
		} else if(values.size() == 5) {
			// Range with shared z: {fromx, tox, fromy, toy, z}
			from_pos->SetPosition(Position(values[0], values[2], values[4]));
			to_pos->SetPosition(Position(values[1], values[3], values[4]));
		} else if(values.size() >= 3) {
			// Single position: both from and to
			Position pos(values[0], values[1], values[2]);
			from_pos->SetPosition(pos);
			to_pos->SetPosition(pos);
		}
	});

	// Bind scan button
	scan_button->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
		int min_x = std::min((int)from_pos->GetX(), (int)to_pos->GetX());
		int max_x = std::max((int)from_pos->GetX(), (int)to_pos->GetX());
		int min_y = std::min((int)from_pos->GetY(), (int)to_pos->GetY());
		int max_y = std::max((int)from_pos->GetY(), (int)to_pos->GetY());
		int min_z = std::min((int)from_pos->GetZ(), (int)to_pos->GetZ());
		int max_z = std::max((int)from_pos->GetZ(), (int)to_pos->GetZ());

		g_gui.CreateLoadBar("Scanning region...");

		struct FloorStats {
			int tiles_with_content = 0;
			int tiles_with_ground = 0;
			int tiles_with_borders = 0;
			int tiles_with_walls = 0;
			int tiles_with_items = 0;
			int tiles_with_creatures = 0;
			int tiles_with_spawns = 0;
			int total_item_count = 0;
		};

		FloorStats floor_stats[rme::MapMaxLayer + 1];
		FloorStats totals;

		uint64_t total_tiles = map.getTileCount();
		uint64_t done = 0;

		for(MapIterator mit = map.begin(); mit != map.end(); ++mit) {
			Tile* tile = (*mit)->get();
			++done;

			if(done % 8192 == 0)
				g_gui.SetLoadDone((int)(done * 100 / std::max<uint64_t>(total_tiles, 1)));

			if(!tile || tile->empty())
				continue;

			int tx = tile->getX();
			int ty = tile->getY();
			int tz = tile->getZ();

			if(tx < min_x || tx > max_x || ty < min_y || ty > max_y || tz < min_z || tz > max_z)
				continue;

			FloorStats& fs = floor_stats[tz];
			fs.tiles_with_content++;

			if(tile->hasGround())
				fs.tiles_with_ground++;
			if(tile->hasBorders())
				fs.tiles_with_borders++;
			if(tile->hasWall())
				fs.tiles_with_walls++;
			if(tile->creature)
				fs.tiles_with_creatures++;
			if(tile->spawn)
				fs.tiles_with_spawns++;

			int other_items = 0;
			for(Item* item : tile->items) {
				if(!item->isBorder() && !item->isWall())
					other_items++;
			}
			if(other_items > 0)
				fs.tiles_with_items++;

			fs.total_item_count += (tile->ground ? 1 : 0) + (int)tile->items.size();
		}

		g_gui.DestroyLoadBar();

		for(int z = min_z; z <= max_z; ++z) {
			FloorStats& fs = floor_stats[z];
			totals.tiles_with_content += fs.tiles_with_content;
			totals.tiles_with_ground += fs.tiles_with_ground;
			totals.tiles_with_borders += fs.tiles_with_borders;
			totals.tiles_with_walls += fs.tiles_with_walls;
			totals.tiles_with_items += fs.tiles_with_items;
			totals.tiles_with_creatures += fs.tiles_with_creatures;
			totals.tiles_with_spawns += fs.tiles_with_spawns;
			totals.total_item_count += fs.total_item_count;
		}

		std::ostringstream os;
		os << "Region Scan Results\n";
		os << "From: (" << min_x << ", " << min_y << ", " << min_z << ") ";
		os << "To: (" << max_x << ", " << max_y << ", " << max_z << ")\n";
		os << "\n";

		os << "Region has content: " << (totals.tiles_with_content > 0 ? "YES" : "NO") << "\n";
		os << "\n";

		for(int z = min_z; z <= max_z; ++z) {
			FloorStats& fs = floor_stats[z];
			if(fs.tiles_with_content == 0)
				continue;
			os << "--- Floor " << z << " ---\n";
			os << "  Tiles with content: " << fs.tiles_with_content << "\n";
			os << "  Tiles with ground: " << fs.tiles_with_ground << "\n";
			os << "  Tiles with borders: " << fs.tiles_with_borders << "\n";
			os << "  Tiles with walls: " << fs.tiles_with_walls << "\n";
			os << "  Tiles with items: " << fs.tiles_with_items << "\n";
			os << "  Tiles with creatures: " << fs.tiles_with_creatures << "\n";
			os << "  Tiles with spawns: " << fs.tiles_with_spawns << "\n";
			os << "  Total item count: " << fs.total_item_count << "\n";
			os << "\n";
		}

		os << "=== TOTALS ===\n";
		os << "  Tiles with content: " << totals.tiles_with_content << "\n";
		os << "  Tiles with ground: " << totals.tiles_with_ground << "\n";
		os << "  Tiles with borders: " << totals.tiles_with_borders << "\n";
		os << "  Tiles with walls: " << totals.tiles_with_walls << "\n";
		os << "  Tiles with items: " << totals.tiles_with_items << "\n";
		os << "  Tiles with creatures: " << totals.tiles_with_creatures << "\n";
		os << "  Tiles with spawns: " << totals.tiles_with_spawns << "\n";
		os << "  Total item count: " << totals.total_item_count << "\n";

		results->SetValue(wxstr(os.str()));
	});

	// Bind clear button
	remove_button->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
		int min_x = std::min((int)from_pos->GetX(), (int)to_pos->GetX());
		int max_x = std::max((int)from_pos->GetX(), (int)to_pos->GetX());
		int min_y = std::min((int)from_pos->GetY(), (int)to_pos->GetY());
		int max_y = std::max((int)from_pos->GetY(), (int)to_pos->GetY());
		int min_z = std::min((int)from_pos->GetZ(), (int)to_pos->GetZ());
		int max_z = std::max((int)from_pos->GetZ(), (int)to_pos->GetZ());

		wxString confirm_msg;
		confirm_msg << "Clear EVERYTHING from region ("
			<< min_x << "," << min_y << "," << min_z << ") to ("
			<< max_x << "," << max_y << "," << max_z << ")?\n\n"
			<< "This will remove tiles, items, creatures, spawns, waypoints and house links.\n"
			<< "This action cannot be undone!";

		int answer = wxMessageBox(confirm_msg, "Confirm Clear", wxYES_NO | wxICON_WARNING, &dlg);
		if(answer != wxYES)
			return;

		Editor* editor = g_gui.GetCurrentEditor();
		if(!editor)
			return;

		editor->getSelection().clear();
		editor->clearActions();

		g_gui.CreateLoadBar("Clearing region...");

		long long removed_tiles = 0;
		long long removed_items = 0;
		long long removed_creatures = 0;
		long long removed_spawns = 0;
		long long removed_waypoints = 0;
		long long removed_house_tiles = 0;
		long long removed_house_exits = 0;

		std::vector<std::string> waypoint_names_to_remove;
		for(WaypointMap::const_iterator it = map.waypoints.begin(); it != map.waypoints.end(); ++it) {
			Waypoint* waypoint = it->second;
			if(!waypoint || !waypoint->pos.isValid()) {
				continue;
			}

			const Position& waypoint_pos = waypoint->pos;
			if(waypoint_pos.x < min_x || waypoint_pos.x > max_x ||
				waypoint_pos.y < min_y || waypoint_pos.y > max_y ||
				waypoint_pos.z < min_z || waypoint_pos.z > max_z) {
				continue;
			}
			waypoint_names_to_remove.push_back(waypoint->name);
		}

		for(const std::string& waypoint_name : waypoint_names_to_remove) {
			Waypoint* waypoint = map.waypoints.getWaypoint(waypoint_name);
			if(!waypoint) {
				continue;
			}

			const Position waypoint_pos = waypoint->pos;
			TileLocation* location = map.getTileL(waypoint_pos);
			if(location && location->getWaypointCount() > 0) {
				location->decreaseWaypointCount();
			}

			map.waypoints.removeWaypoint(waypoint->name);
			++removed_waypoints;
		}

		uint64_t total_tiles = map.getTileCount();
		uint64_t done = 0;

		for(MapIterator mit = map.begin(); mit != map.end(); ++mit) {
			Tile* tile = (*mit)->get();
			++done;

			if(done % 8192 == 0)
				g_gui.SetLoadDone((int)(done * 100 / std::max<uint64_t>(total_tiles, 1)));

			if(!tile)
				continue;

			const Position tile_pos = tile->getPosition();
			int tx = tile_pos.x;
			int ty = tile_pos.y;
			int tz = tile_pos.z;

			if(tx < min_x || tx > max_x || ty < min_y || ty > max_y || tz < min_z || tz > max_z)
				continue;

			removed_items += (tile->ground ? 1 : 0) + (long long)tile->items.size();
			if(tile->creature) {
				++removed_creatures;
			}

			if(tile->isHouseTile()) {
				House* house = map.houses.getHouse(tile->getHouseID());
				if(house) {
					house->removeTile(tile);
				}
				++removed_house_tiles;
			}

			if(const HouseExitList* exits = tile->getHouseExits()) {
				HouseExitList exit_ids = *exits;
				for(uint32_t house_id : exit_ids) {
					House* house = map.houses.getHouse(house_id);
					if(!house) {
						continue;
					}

					if(house->getExit() == tile_pos) {
						house->setExit(Position());
					} else {
						tile->removeHouseExit(house);
					}
					++removed_house_exits;
				}
			}

			if(tile->spawn) {
				map.removeSpawn(tile);
				++removed_spawns;
			}

			map.setTile(tile_pos, nullptr, true);
			++removed_tiles;
		}

		g_gui.DestroyLoadBar();

		map.doChange();
		g_gui.RefreshPalettes(&map);
		g_gui.RefreshView();

		wxString status_msg;
		status_msg << "Cleared region: "
			<< removed_tiles << " tiles, "
			<< removed_items << " items, "
			<< removed_creatures << " creatures, "
			<< removed_spawns << " spawns, "
			<< removed_waypoints << " waypoints, "
			<< removed_house_tiles << " house tiles, "
			<< removed_house_exits << " house exits.";
		g_gui.SetStatusText(status_msg);

		results->SetValue(status_msg);
	});

	// Shared state for List Items
	std::vector<std::pair<wxString, Position>> found_items;
	bool list_items_triggered = false;

	// Bind list items button
	list_button->Bind(wxEVT_BUTTON, [&](wxCommandEvent&) {
		int min_x = std::min((int)from_pos->GetX(), (int)to_pos->GetX());
		int max_x = std::max((int)from_pos->GetX(), (int)to_pos->GetX());
		int min_y = std::min((int)from_pos->GetY(), (int)to_pos->GetY());
		int max_y = std::max((int)from_pos->GetY(), (int)to_pos->GetY());
		int min_z = std::min((int)from_pos->GetZ(), (int)to_pos->GetZ());
		int max_z = std::max((int)from_pos->GetZ(), (int)to_pos->GetZ());

		g_gui.CreateLoadBar("Listing items in region...");

		uint64_t total_tiles = map.getTileCount();
		uint64_t done = 0;

		for(MapIterator mit = map.begin(); mit != map.end(); ++mit) {
			Tile* tile = (*mit)->get();
			++done;

			if(done % 8192 == 0)
				g_gui.SetLoadDone((int)(done * 100 / std::max<uint64_t>(total_tiles, 1)));

			if(!tile || tile->empty())
				continue;

			int tx = tile->getX();
			int ty = tile->getY();
			int tz = tile->getZ();

			if(tx < min_x || tx > max_x || ty < min_y || ty > max_y || tz < min_z || tz > max_z)
				continue;

			Position pos(tx, ty, tz);

			if(tile->ground) {
				wxString desc = "Ground: " + wxstr(tile->ground->getName());
				desc << " [" << (int)tile->ground->getID() << "]";
				found_items.push_back(std::make_pair(desc, pos));
			}

			for(Item* item : tile->items) {
				if(!item)
					continue;
				wxString desc;
				if(item->isBorder())
					desc = "[Border] ";
				else if(item->isWall())
					desc = "[Wall] ";
				desc += wxstr(item->getName());
				desc << " [" << (int)item->getID() << "]";
				found_items.push_back(std::make_pair(desc, pos));
			}

			if(tile->creature) {
				wxString desc = "Creature: " + wxstr(tile->creature->getName());
				found_items.push_back(std::make_pair(desc, pos));
			}

			if(tile->spawn) {
				wxString desc;
				desc << "Spawn (size: " << tile->spawn->getSize() << ")";
				found_items.push_back(std::make_pair(desc, pos));
			}

			// Location-based content
			if(tile->location) {
				const HouseExitList* exits = tile->location->getHouseExits();
				if(exits && !exits->empty()) {
					for(uint32_t houseId : *exits) {
						wxString desc;
						desc << "House Exit (id: " << (int)houseId << ")";
						found_items.push_back(std::make_pair(desc, pos));
					}
				}
				if(tile->location->getWaypointCount() > 0) {
					wxString desc = "Waypoint";
					found_items.push_back(std::make_pair(desc, pos));
				}
			}
		}

		g_gui.DestroyLoadBar();

		if(found_items.empty()) {
			results->SetValue("No items found in the selected region.");
			return;
		}

		list_items_triggered = true;
		dlg.EndModal(wxID_OK);
	});

	dlg.ShowModal();

	if(list_items_triggered && !found_items.empty()) {
		SearchResultWindow* window = g_gui.ShowSearchWindow();
		window->Clear(true);
		for(const auto& entry : found_items) {
			window->AddPosition(entry.first, entry.second);
		}
	}
}
