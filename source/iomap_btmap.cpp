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

#include "settings.h"
#include "gui.h"

#include "creatures.h"
#include "creature.h"
#include "map.h"
#include "tile.h"
#include "item.h"
#include "complexitem.h"
#include "town.h"
#include "waypoints.h"

#include "iomap_btmap.h"

#include <wx/dir.h>
#include <wx/filename.h>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>

// ============================================================================
// Manifest JSON (simple hand-written JSON — no external dependency)
// ============================================================================

bool IOMapBTMap::saveManifest(Map& map, const std::string& dirPath)
{
	std::string filepath = dirPath + "manifest.json";
	std::string tmppath = filepath + ".tmp";

	std::ofstream f(tmppath);
	if(!f.is_open()) {
		error("Could not open manifest for writing: %s", tmppath.c_str());
		return false;
	}

	f << "{\n";
	f << "  \"format\": \"btmap\",\n";
	f << "  \"format_version\": 1,\n";
	f << "  \"otbm_version\": " << map.getVersion().otbm << ",\n";

	// Write client version as integer
	f << "  \"client_version\": " << static_cast<int>(map.getVersion().client) << ",\n";

	f << "  \"width\": " << map.getWidth() << ",\n";
	f << "  \"height\": " << map.getHeight() << ",\n";

	// Escape strings for JSON
	auto escapeJson = [](const std::string& s) -> std::string {
		std::string result;
		for(char c : s) {
			switch(c) {
				case '"': result += "\\\""; break;
				case '\\': result += "\\\\"; break;
				case '\n': result += "\\n"; break;
				case '\r': result += "\\r"; break;
				case '\t': result += "\\t"; break;
				default: result += c; break;
			}
		}
		return result;
	};

	f << "  \"description\": \"" << escapeJson(map.getMapDescription()) << "\",\n";
	f << "  \"spawn_file\": \"" << escapeJson(map.getSpawnFilename()) << "\",\n";
	f << "  \"house_file\": \"" << escapeJson(map.getHouseFilename()) << "\",\n";
	f << "  \"items_major_version\": " << g_items.MajorVersion << ",\n";
	f << "  \"items_minor_version\": " << g_items.MinorVersion << "\n";
	f << "}\n";

	f.close();

	// Atomic rename
	std::remove(filepath.c_str());
	if(std::rename(tmppath.c_str(), filepath.c_str()) != 0) {
		error("Could not rename manifest temp file");
		return false;
	}

	return true;
}

// Simple JSON value parser (minimal, for our manifest only)
static std::string getJsonStringValue(const std::string& json, const std::string& key)
{
	std::string searchKey = "\"" + key + "\"";
	size_t pos = json.find(searchKey);
	if(pos == std::string::npos) return "";

	pos = json.find(':', pos + searchKey.size());
	if(pos == std::string::npos) return "";

	pos = json.find('"', pos + 1);
	if(pos == std::string::npos) return "";
	pos++; // skip opening quote

	std::string result;
	while(pos < json.size() && json[pos] != '"') {
		if(json[pos] == '\\' && pos + 1 < json.size()) {
			pos++;
			switch(json[pos]) {
				case '"': result += '"'; break;
				case '\\': result += '\\'; break;
				case 'n': result += '\n'; break;
				case 'r': result += '\r'; break;
				case 't': result += '\t'; break;
				default: result += json[pos]; break;
			}
		} else {
			result += json[pos];
		}
		pos++;
	}
	return result;
}

static int getJsonIntValue(const std::string& json, const std::string& key, int defaultVal = 0)
{
	std::string searchKey = "\"" + key + "\"";
	size_t pos = json.find(searchKey);
	if(pos == std::string::npos) return defaultVal;

	pos = json.find(':', pos + searchKey.size());
	if(pos == std::string::npos) return defaultVal;
	pos++;

	// Skip whitespace
	while(pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) pos++;

	try {
		return std::stoi(json.substr(pos));
	} catch(...) {
		return defaultVal;
	}
}

bool IOMapBTMap::loadManifest(Map& map, const std::string& dirPath)
{
	std::string filepath = dirPath + "manifest.json";
	std::ifstream f(filepath);
	if(!f.is_open()) {
		error("Could not open manifest: %s", filepath.c_str());
		return false;
	}

	std::string json((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
	f.close();

	std::string format = getJsonStringValue(json, "format");
	if(format != "btmap") {
		error("Invalid btmap manifest format: %s", format.c_str());
		return false;
	}

	int format_version = getJsonIntValue(json, "format_version");
	if(format_version < 1) {
		error("Unsupported btmap format version: %d", format_version);
		return false;
	}

	MapVersion ver;
	ver.otbm = static_cast<MapVersionID>(getJsonIntValue(json, "otbm_version"));
	ver.client = static_cast<ClientVersionID>(getJsonIntValue(json, "client_version"));
	version = ver;

	map.setWidth(getJsonIntValue(json, "width", 512));
	map.setHeight(getJsonIntValue(json, "height", 512));
	map.setMapDescription(getJsonStringValue(json, "description"));
	map.setSpawnFilename(getJsonStringValue(json, "spawn_file"));
	map.setHouseFilename(getJsonStringValue(json, "house_file"));

	return true;
}

// ============================================================================
// Chunk I/O — each chunk is an OTBM-compatible binary with a single TILE_AREA
// ============================================================================

bool IOMapBTMap::saveChunk(Map& map, const ChunkKey& key, const std::string& chunksDir)
{
	// Collect tiles in this 256x256 region on floor key.z
	int base_x = key.x;
	int base_y = key.y;
	int z = key.z;

	bool has_tiles = false;

	// First pass: check if any tiles exist in this chunk
	for(int x = base_x; x < base_x + 256 && !has_tiles; ++x) {
		for(int y = base_y; y < base_y + 256 && !has_tiles; ++y) {
			Tile* tile = map.getTile(x, y, z);
			if(tile && tile->size() > 0) {
				has_tiles = true;
			}
		}
	}

	std::string chunkFile = chunksDir + key.toFilename();
	std::string tmpFile = chunkFile + ".tmp";

	if(!has_tiles) {
		// Remove chunk file if it exists (chunk is now empty)
		std::remove(chunkFile.c_str());
		return true;
	}

	// Write chunk using OTBM node format
	DiskNodeFileWriteHandle f(tmpFile,
		(g_settings.getInteger(Config::SAVE_WITH_OTB_MAGIC_NUMBER) ? "BTCK" : std::string(4, '\0'))
	);

	if(!f.isOk()) {
		error("Can not open chunk file for writing: %s", tmpFile.c_str());
		return false;
	}

	const IOMap& self = *this;

	// Root node
	f.addNode(0);
	{
		// TILE_AREA node
		f.addNode(OTBM_TILE_AREA);
		f.addU16(static_cast<uint16_t>(base_x));
		f.addU16(static_cast<uint16_t>(base_y));
		f.addU8(static_cast<uint8_t>(z));

		for(int x = base_x; x < base_x + 256; ++x) {
			for(int y = base_y; y < base_y + 256; ++y) {
				Tile* tile = map.getTile(x, y, z);
				if(!tile || tile->size() == 0)
					continue;

				f.addNode(tile->isHouseTile() ? OTBM_HOUSETILE : OTBM_TILE);

				f.addU8(static_cast<uint8_t>(x & 0xFF));
				f.addU8(static_cast<uint8_t>(y & 0xFF));

				if(tile->isHouseTile()) {
					f.addU32(tile->getHouseID());
				}

				if(tile->getMapFlags()) {
					f.addByte(OTBM_ATTR_TILE_FLAGS);
					f.addU32(tile->getMapFlags());
				}

				if(tile->ground) {
					Item* ground = tile->ground;
					if(ground->isMetaItem()) {
						// Don't save meta items
					} else if(ground->hasBorderEquivalent()) {
						bool found = false;
						for(Item* item : tile->items) {
							if(item->getGroundEquivalent() == ground->getID()) {
								found = true;
								break;
							}
						}
						if(!found) {
							ground->serializeItemNode_OTBM(self, f);
						}
					} else if(ground->isComplex()) {
						ground->serializeItemNode_OTBM(self, f);
					} else {
						f.addByte(OTBM_ATTR_ITEM);
						ground->serializeItemCompact_OTBM(self, f);
					}
				}

				for(Item* item : tile->items) {
					if(!item->isMetaItem()) {
						item->serializeItemNode_OTBM(self, f);
					}
				}

				f.endNode(); // end tile
			}
		}

		f.endNode(); // end TILE_AREA
	}
	f.endNode(); // end root

	f.close();

	// Atomic rename
	std::remove(chunkFile.c_str());
	if(std::rename(tmpFile.c_str(), chunkFile.c_str()) != 0) {
		error("Could not rename chunk temp file: %s", tmpFile.c_str());
		return false;
	}

	return true;
}

bool IOMapBTMap::loadChunk(Map& map, const std::string& chunkFilePath)
{
	DiskNodeFileReadHandle f(chunkFilePath, std::vector<std::string>{"BTCK", std::string(4, '\0')});

	if(!f.isOk()) {
		error("Could not open chunk file: %s", chunkFilePath.c_str());
		return false;
	}

	BinaryNode* root = f.getRootNode();
	if(!root) {
		error("Could not read root node from chunk: %s", chunkFilePath.c_str());
		return false;
	}

	BinaryNode* tileAreaNode = root->getChild();
	while(tileAreaNode) {
		uint8_t node_type;
		if(!tileAreaNode->getByte(node_type))
			break;

		if(node_type != OTBM_TILE_AREA) {
			tileAreaNode = tileAreaNode->advance();
			continue;
		}

		uint16_t base_x, base_y;
		uint8_t base_z;
		if(!tileAreaNode->getU16(base_x) || !tileAreaNode->getU16(base_y) || !tileAreaNode->getU8(base_z)) {
			warning("Could not read tile area coords from chunk");
			tileAreaNode = tileAreaNode->advance();
			continue;
		}

		BinaryNode* tileNode = tileAreaNode->getChild();
		while(tileNode) {
			uint8_t tile_type;
			if(!tileNode->getByte(tile_type)) {
				tileNode = tileNode->advance();
				continue;
			}

			if(tile_type != OTBM_TILE && tile_type != OTBM_HOUSETILE) {
				tileNode = tileNode->advance();
				continue;
			}

			uint8_t x_offset, y_offset;
			if(!tileNode->getU8(x_offset) || !tileNode->getU8(y_offset)) {
				tileNode = tileNode->advance();
				continue;
			}

			int tile_x = base_x + x_offset;
			int tile_y = base_y + y_offset;
			int tile_z = base_z;

			Tile* tile = map.createTile(tile_x, tile_y, tile_z);

			if(tile_type == OTBM_HOUSETILE) {
				uint32_t house_id;
				if(tileNode->getU32(house_id)) {
					House* house = map.houses.getHouse(house_id);
					if(!house) {
						// Create house on the fly (like OTBM loader does).
						// loadHouses will fill in details (name, exit, townid) later.
						house = newd House(map);
						house->id = house_id;
						map.houses.addHouse(house);
					}
					tile->setHouse(house);
				}
			}

			// Read tile attributes
			uint8_t attr;
			while(tileNode->getU8(attr)) {
				switch(attr) {
					case OTBM_ATTR_TILE_FLAGS: {
						uint32_t flags;
						if(tileNode->getU32(flags)) {
							tile->setMapFlags(flags);
						}
						break;
					}
					case OTBM_ATTR_ITEM: {
						Item* item = Item::Create_OTBM(*this, tileNode);
						if(item) {
							if(item->isGroundTile()) {
								tile->addItem(item);
							} else {
								tile->addItem(item);
							}
						}
						break;
					}
					default: {
						// Unknown attribute, skip
						break;
					}
				}
			}

			// Read child item nodes
			BinaryNode* itemNode = tileNode->getChild();
			while(itemNode) {
				uint8_t item_type;
				if(itemNode->getByte(item_type) && item_type == OTBM_ITEM) {
					Item* item = Item::Create_OTBM(*this, itemNode);
					if(item) {
						if(!item->unserializeItemNode_OTBM(*this, itemNode))
							warning("Could not unserialize item in chunk");
						if(item->isGroundTile()) {
							tile->addItem(item);
						} else {
							tile->addItem(item);
						}
					}
				}
				itemNode = itemNode->advance();
			}

			tileNode = tileNode->advance();
		}

		tileAreaNode = tileAreaNode->advance();
	}

	return true;
}

// ============================================================================
// Spawns / Houses I/O (delegate to IOMapOTBM's XML format)
// ============================================================================

bool IOMapBTMap::saveSpawns(Map& map, const std::string& dirPath)
{
	IOMapOTBM otbmSaver(version);
	FileName dir(wxstr(dirPath));

	std::string savedSpawnFile = map.getSpawnFilename();
	map.setSpawnFilename("spawns.xml");
	bool result = otbmSaver.saveSpawns(map, dir);
	map.setSpawnFilename(savedSpawnFile);
	return result;
}

bool IOMapBTMap::loadSpawns(Map& map, const std::string& dirPath)
{
	IOMapOTBM otbmLoader(version);
	FileName dir(wxstr(dirPath));

	std::string savedSpawnFile = map.getSpawnFilename();
	map.setSpawnFilename("spawns.xml");
	bool result = otbmLoader.loadSpawns(map, dir);
	map.setSpawnFilename(savedSpawnFile);

	// Collect any warnings
	wxArrayString& otbmWarnings = otbmLoader.getWarnings();
	for(size_t i = 0; i < otbmWarnings.size(); ++i)
		warning("%s", (const char*)otbmWarnings[i].mb_str(wxConvUTF8));

	return result;
}

bool IOMapBTMap::saveHouses(Map& map, const std::string& dirPath)
{
	IOMapOTBM otbmSaver(version);
	FileName dir(wxstr(dirPath));

	std::string savedHouseFile = map.getHouseFilename();
	map.setHouseFilename("houses.xml");
	bool result = otbmSaver.saveHouses(map, dir);
	map.setHouseFilename(savedHouseFile);
	return result;
}

bool IOMapBTMap::loadHouses(Map& map, const std::string& dirPath)
{
	IOMapOTBM otbmLoader(version);
	FileName dir(wxstr(dirPath));

	std::string savedHouseFile = map.getHouseFilename();
	map.setHouseFilename("houses.xml");
	bool result = otbmLoader.loadHouses(map, dir);
	map.setHouseFilename(savedHouseFile);

	wxArrayString& otbmWarnings = otbmLoader.getWarnings();
	for(size_t i = 0; i < otbmWarnings.size(); ++i)
		warning("%s", (const char*)otbmWarnings[i].mb_str(wxConvUTF8));

	return result;
}

bool IOMapBTMap::saveTowns(Map& map, const std::string& dirPath)
{
	std::string filepath = dirPath + "towns.xml";
	std::string tmppath = filepath + ".tmp";

	pugi::xml_document doc;
	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";

	pugi::xml_node townNodes = doc.append_child("towns");
	for(const auto& townEntry : map.towns) {
		Town* town = townEntry.second;
		const Position& templePos = town->getTemplePosition();

		pugi::xml_node townNode = townNodes.append_child("town");
		townNode.append_attribute("townid") = town->getID();
		townNode.append_attribute("name") = town->getName().c_str();
		townNode.append_attribute("templex") = templePos.x;
		townNode.append_attribute("templey") = templePos.y;
		townNode.append_attribute("templez") = templePos.z;
	}

	if(!doc.save_file(tmppath.c_str(), "\t", pugi::format_default, pugi::encoding_utf8)) {
		error("Could not save towns file: %s", tmppath.c_str());
		return false;
	}

	std::remove(filepath.c_str());
	if(std::rename(tmppath.c_str(), filepath.c_str()) != 0) {
		error("Could not rename towns temp file");
		return false;
	}

	return true;
}

bool IOMapBTMap::loadTowns(Map& map, const std::string& dirPath)
{
	std::string filepath = dirPath + "towns.xml";

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filepath.c_str());
	if(!result) {
		// towns.xml might not exist for older btmap files
		return true;
	}

	pugi::xml_node node = doc.child("towns");
	if(!node) {
		warning("Invalid towns.xml root element");
		return false;
	}

	for(pugi::xml_node townNode = node.first_child(); townNode; townNode = townNode.next_sibling()) {
		if(std::string(townNode.name()) != "town")
			continue;

		uint32_t town_id = townNode.attribute("townid").as_uint();
		if(town_id == 0)
			continue;

		Town* town = map.towns.getTown(town_id);
		if(!town) {
			town = newd Town(town_id);
			if(!map.towns.addTown(town)) {
				delete town;
				continue;
			}
		}

		pugi::xml_attribute attr;
		if((attr = townNode.attribute("name")))
			town->setName(attr.as_string());

		Position templePos;
		templePos.x = townNode.attribute("templex").as_int();
		templePos.y = townNode.attribute("templey").as_int();
		templePos.z = townNode.attribute("templez").as_int();
		town->setTemplePosition(templePos);
	}

	return true;
}

bool IOMapBTMap::saveWaypoints(Map& map, const std::string& dirPath)
{
	std::string filepath = dirPath + "waypoints.xml";
	std::string tmppath = filepath + ".tmp";

	pugi::xml_document doc;
	pugi::xml_node decl = doc.prepend_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";

	pugi::xml_node waypointNodes = doc.append_child("waypoints");
	for(const auto& waypointEntry : map.waypoints) {
		Waypoint* waypoint = waypointEntry.second;

		pugi::xml_node wpNode = waypointNodes.append_child("waypoint");
		wpNode.append_attribute("name") = waypoint->name.c_str();
		wpNode.append_attribute("x") = waypoint->pos.x;
		wpNode.append_attribute("y") = waypoint->pos.y;
		wpNode.append_attribute("z") = waypoint->pos.z;
	}

	if(!doc.save_file(tmppath.c_str(), "\t", pugi::format_default, pugi::encoding_utf8)) {
		error("Could not save waypoints file: %s", tmppath.c_str());
		return false;
	}

	std::remove(filepath.c_str());
	if(std::rename(tmppath.c_str(), filepath.c_str()) != 0) {
		error("Could not rename waypoints temp file");
		return false;
	}

	return true;
}

bool IOMapBTMap::loadWaypoints(Map& map, const std::string& dirPath)
{
	std::string filepath = dirPath + "waypoints.xml";

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filepath.c_str());
	if(!result) {
		// waypoints.xml might not exist for older btmap files
		return true;
	}

	pugi::xml_node node = doc.child("waypoints");
	if(!node) {
		warning("Invalid waypoints.xml root element");
		return false;
	}

	for(pugi::xml_node wpNode = node.first_child(); wpNode; wpNode = wpNode.next_sibling()) {
		if(std::string(wpNode.name()) != "waypoint")
			continue;

		Waypoint* wp = newd Waypoint();
		wp->name = wpNode.attribute("name").as_string();
		wp->pos.x = wpNode.attribute("x").as_int();
		wp->pos.y = wpNode.attribute("y").as_int();
		wp->pos.z = wpNode.attribute("z").as_int();

		map.waypoints.addWaypoint(wp);
	}

	return true;
}

// ============================================================================
// Full save / load
// ============================================================================

bool IOMapBTMap::isBTMapDirectory(const FileName& path)
{
	wxString fullPath = path.GetFullPath();

	// Check if it's a directory ending with .btmap
	if(wxDirExists(fullPath)) {
		return fullPath.EndsWith(".btmap");
	}

	// Check extension
	return path.GetExt() == "btmap";
}

std::set<ChunkKey, std::less<ChunkKey>> IOMapBTMap::collectAllChunkKeys(Map& map)
{
	std::set<ChunkKey, std::less<ChunkKey>> keys;

	MapIterator it = map.begin();
	MapIterator end = map.end();

	while(it != end) {
		Tile* tile = (*it)->get();
		if(tile && tile->size() > 0) {
			const Position& pos = tile->getPosition();
			keys.insert(ChunkKey::fromPosition(pos.x, pos.y, pos.z));
		}
		++it;
	}

	return keys;
}

bool IOMapBTMap::saveMap(Map& map, const FileName& btmapDir)
{
	std::string dirPath = nstr(btmapDir.GetFullPath());

	// Ensure trailing separator
	if(!dirPath.empty() && dirPath.back() != '/' && dirPath.back() != '\\')
		dirPath += '/';

	std::string chunksDir = dirPath + "chunks/";

	// Create directories
	if(!wxDirExists(wxstr(dirPath))) {
		wxFileName::Mkdir(wxstr(dirPath), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
	}
	if(!wxDirExists(wxstr(chunksDir))) {
		wxFileName::Mkdir(wxstr(chunksDir), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
	}

	g_gui.SetLoadDone(0, "Saving manifest...");

	if(!saveManifest(map, dirPath)) {
		return false;
	}

	// Collect all chunk keys from the map
	g_gui.SetLoadDone(5, "Collecting chunks...");
	auto allKeys = collectAllChunkKeys(map);

	if(allKeys.empty()) {
		// Empty map, save manifest only
		g_gui.SetLoadDone(85, "Saving towns...");
		saveTowns(map, dirPath);
		g_gui.SetLoadDone(88, "Saving waypoints...");
		saveWaypoints(map, dirPath);
		g_gui.SetLoadDone(90, "Saving spawns...");
		saveSpawns(map, dirPath);
		g_gui.SetLoadDone(95, "Saving houses...");
		saveHouses(map, dirPath);
		return true;
	}

	// Save all chunks
	size_t done = 0;
	size_t total = allKeys.size();

	for(const auto& key : allKeys) {
		if(!saveChunk(map, key, chunksDir)) {
			return false;
		}

		done++;
		if(done % 10 == 0 || done == total) {
			int progress = 5 + static_cast<int>((done * 85.0) / total);
			g_gui.SetLoadDone(progress, "Saving chunks...");
		}
	}

	g_gui.SetLoadDone(88, "Saving towns...");
	saveTowns(map, dirPath);

	g_gui.SetLoadDone(90, "Saving waypoints...");
	saveWaypoints(map, dirPath);

	g_gui.SetLoadDone(93, "Saving spawns...");
	saveSpawns(map, dirPath);

	g_gui.SetLoadDone(96, "Saving houses...");
	saveHouses(map, dirPath);

	return true;
}

bool IOMapBTMap::saveIncremental(Map& map, const FileName& btmapDir, ChunkTracker& tracker)
{
	std::string dirPath = nstr(btmapDir.GetFullPath());
	if(!dirPath.empty() && dirPath.back() != '/' && dirPath.back() != '\\')
		dirPath += '/';

	std::string chunksDir = dirPath + "chunks/";

	// Create directories if they don't exist
	if(!wxDirExists(wxstr(dirPath))) {
		wxFileName::Mkdir(wxstr(dirPath), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
	}
	if(!wxDirExists(wxstr(chunksDir))) {
		wxFileName::Mkdir(wxstr(chunksDir), wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
	}

	// If full save needed, do a full save
	if(tracker.needsFullSave()) {
		g_gui.SetLoadDone(0, "Full save needed...");
		if(!saveMap(map, btmapDir))
			return false;

		tracker.clearDirty();
		tracker.clearMetadataDirty();
		return true;
	}

	// Incremental save: only dirty chunks
	const auto& dirtyChunks = tracker.getDirtyChunks();
	size_t total = dirtyChunks.size();

	// Always update manifest (lightweight)
	g_gui.SetLoadDone(0, "Saving manifest...");
	if(!saveManifest(map, dirPath))
		return false;

	if(total > 0) {
		size_t done = 0;
		for(const auto& key : dirtyChunks) {
			if(!saveChunk(map, key, chunksDir))
				return false;

			done++;
			if(done % 5 == 0 || done == total) {
				int progress = 5 + static_cast<int>((done * 80.0) / total);
				std::ostringstream msg;
				msg << "Saving chunk " << done << "/" << total << "...";
				g_gui.SetLoadDone(progress, wxstr(msg.str()));
			}
		}
	}

	// Always save metadata XML files (small/fast) since there's no reliable
	// way to track all spawn/creature/house/town/waypoint changes.
	g_gui.SetLoadDone(85, "Saving towns...");
	saveTowns(map, dirPath);

	g_gui.SetLoadDone(88, "Saving waypoints...");
	saveWaypoints(map, dirPath);

	g_gui.SetLoadDone(92, "Saving spawns...");
	saveSpawns(map, dirPath);

	g_gui.SetLoadDone(96, "Saving houses...");
	saveHouses(map, dirPath);

	tracker.clearMetadataDirty();

	tracker.clearDirty();
	return true;
}

bool IOMapBTMap::loadMap(Map& map, const FileName& btmapDir)
{
	std::string dirPath = nstr(btmapDir.GetFullPath());
	if(!dirPath.empty() && dirPath.back() != '/' && dirPath.back() != '\\')
		dirPath += '/';

	std::string chunksDir = dirPath + "chunks/";

	g_gui.SetLoadDone(0, "Loading manifest...");

	if(!loadManifest(map, dirPath)) {
		return false;
	}

	// Find all .chunk files
	wxArrayString chunkFiles;
	if(wxDirExists(wxstr(chunksDir))) {
		wxDir::GetAllFiles(wxstr(chunksDir), &chunkFiles, "*.chunk", wxDIR_FILES);
	}

	size_t total = chunkFiles.size();
	if(total == 0) {
		warning("No chunk files found in %s", chunksDir.c_str());
	}

	g_gui.SetLoadDone(5, "Loading chunks...");

	for(size_t i = 0; i < total; ++i) {
		std::string chunkPath = nstr(chunkFiles[i]);
		if(!loadChunk(map, chunkPath)) {
			warning("Failed to load chunk: %s", chunkPath.c_str());
		}

		if(i % 10 == 0 || i + 1 == total) {
			int progress = 5 + static_cast<int>(((i + 1) * 80.0) / total);
			g_gui.SetLoadDone(progress, "Loading chunks...");
		}
	}

	g_gui.SetLoadDone(86, "Loading towns...");
	loadTowns(map, dirPath);

	g_gui.SetLoadDone(89, "Loading waypoints...");
	loadWaypoints(map, dirPath);

	g_gui.SetLoadDone(92, "Loading spawns...");
	loadSpawns(map, dirPath);

	g_gui.SetLoadDone(96, "Loading houses...");
	loadHouses(map, dirPath);

	return true;
}

// ============================================================================
// Export to OTBM
// ============================================================================

bool IOMapBTMap::exportToOTBM(Map& map, const FileName& otbmFile)
{
	IOMapOTBM otbmSaver(map.getVersion());
	return otbmSaver.saveMap(map, otbmFile);
}
