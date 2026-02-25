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

#ifndef RME_CHUNK_TRACKER_H_
#define RME_CHUNK_TRACKER_H_

#include <unordered_set>
#include <cstdint>

// A ChunkKey identifies a 256x256 tile region on a specific floor.
// This matches the OTBM TILE_AREA grouping (pos & 0xFF00).
struct ChunkKey {
	uint16_t x; // Base X (aligned to 256: pos.x & 0xFF00)
	uint16_t y; // Base Y (aligned to 256: pos.y & 0xFF00)
	uint8_t z;  // Floor level (0-15)

	ChunkKey() : x(0), y(0), z(0) {}
	ChunkKey(uint16_t _x, uint16_t _y, uint8_t _z) : x(_x), y(_y), z(_z) {}

	// Create a ChunkKey from any tile position
	static ChunkKey fromPosition(int pos_x, int pos_y, int pos_z) {
		return ChunkKey(
			static_cast<uint16_t>(pos_x & 0xFF00),
			static_cast<uint16_t>(pos_y & 0xFF00),
			static_cast<uint8_t>(pos_z)
		);
	}

	bool operator==(const ChunkKey& other) const {
		return x == other.x && y == other.y && z == other.z;
	}

	bool operator!=(const ChunkKey& other) const {
		return !(*this == other);
	}

	// Generate a filename like "0100_0200_07.chunk"
	std::string toFilename() const;
};

struct ChunkKeyHash {
	size_t operator()(const ChunkKey& key) const {
		// Combine x, y, z into a single hash
		size_t h = std::hash<uint16_t>()(key.x);
		h ^= std::hash<uint16_t>()(key.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
		h ^= std::hash<uint8_t>()(key.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
		return h;
	}
};

// Tracks which 256x256 tile regions (chunks) have been modified since
// the last save. Used for incremental saving in the .btmap format.
class ChunkTracker {
public:
	ChunkTracker();

	// Mark a chunk as dirty (modified) given a tile position
	void markDirty(int pos_x, int pos_y, int pos_z);

	// Mark a specific chunk as dirty
	void markDirty(const ChunkKey& key);

	// Mark all chunks as dirty (for full re-save)
	void markAllDirty();

	// Check if a chunk is dirty
	bool isDirty(const ChunkKey& key) const;

	// Get all dirty chunks
	const std::unordered_set<ChunkKey, ChunkKeyHash>& getDirtyChunks() const;

	// Clear all dirty flags (after successful save)
	void clearDirty();

	// Get number of dirty chunks
	size_t dirtyCount() const;

	// Whether towns/waypoints/spawns/houses metadata changed
	bool isMetadataDirty() const { return metadata_dirty; }
	void markMetadataDirty() { metadata_dirty = true; }
	void clearMetadataDirty() { metadata_dirty = false; }

	// Whether we need a full save (all chunks)
	bool needsFullSave() const { return full_save_needed; }
	void setFullSaveNeeded() { full_save_needed = true; }
	void clearFullSaveNeeded() { full_save_needed = false; }

private:
	std::unordered_set<ChunkKey, ChunkKeyHash> dirty_chunks;
	bool metadata_dirty;
	bool full_save_needed;
};

#endif
