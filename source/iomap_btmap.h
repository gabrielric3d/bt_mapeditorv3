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

#ifndef RME_IOMAP_BTMAP_H_
#define RME_IOMAP_BTMAP_H_

#include "main.h"
#include "iomap.h"
#include "iomap_otbm.h"
#include "chunk_tracker.h"

#include <set>
#include <string>

// IOMapBTMap handles the .btmap chunk-based format.
//
// Directory structure:
//   mymap.btmap/
//     manifest.json     - map metadata (version, width, height, descriptions)
//     chunks/           - tile data split into 256x256 per-floor chunk files
//       0000_0000_07.chunk
//       0100_0000_07.chunk
//       ...
//     towns.xml         - town definitions (id, name, temple position)
//     waypoints.xml     - waypoint definitions (name, position)
//     spawns.xml        - spawn data
//     houses.xml        - house data
//
// Each .chunk file is a self-contained OTBM-style binary blob containing
// a single TILE_AREA node with all tiles in that 256x256 region on one floor.
class IOMapBTMap : public IOMap
{
public:
	IOMapBTMap(MapVersion ver) { version = ver; }
	~IOMapBTMap() {}

	// Load entire .btmap directory
	virtual bool loadMap(Map& map, const FileName& btmapDir);

	// Full save: writes all tiles to .btmap directory
	virtual bool saveMap(Map& map, const FileName& btmapDir);

	// Incremental save: only writes dirty chunks
	bool saveIncremental(Map& map, const FileName& btmapDir, ChunkTracker& tracker);

	// Export: convert .btmap back to a single .otbm file
	static bool exportToOTBM(Map& map, const FileName& otbmFile);

	// Check if a path is a .btmap directory
	static bool isBTMapDirectory(const FileName& path);

	// Collect all chunk keys that exist on the map
	static std::set<ChunkKey, std::less<ChunkKey>> collectAllChunkKeys(Map& map);

	// Load manifest.json (public: needed by Editor to get version info before full load)
	bool loadManifest(Map& map, const std::string& dirPath);

protected:
	// Save manifest.json
	bool saveManifest(Map& map, const std::string& dirPath);

	// Save/load individual chunk files
	bool saveChunk(Map& map, const ChunkKey& key, const std::string& chunksDir);
	bool loadChunk(Map& map, const std::string& chunkFilePath);

	// Save/load spawns and houses inside .btmap dir
	bool saveSpawns(Map& map, const std::string& dirPath);
	bool loadSpawns(Map& map, const std::string& dirPath);
	bool saveHouses(Map& map, const std::string& dirPath);
	bool loadHouses(Map& map, const std::string& dirPath);

	// Save/load towns and waypoints inside .btmap dir
	bool saveTowns(Map& map, const std::string& dirPath);
	bool loadTowns(Map& map, const std::string& dirPath);
	bool saveWaypoints(Map& map, const std::string& dirPath);
	bool loadWaypoints(Map& map, const std::string& dirPath);
};

// Comparison operator for ChunkKey (used by std::set)
inline bool operator<(const ChunkKey& a, const ChunkKey& b) {
	if(a.z != b.z) return a.z < b.z;
	if(a.x != b.x) return a.x < b.x;
	return a.y < b.y;
}

#endif
