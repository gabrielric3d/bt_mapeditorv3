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

#ifndef RME_SPRITE_CACHE_H_
#define RME_SPRITE_CACHE_H_

#include <string>
#include <vector>
#include <cstdint>

// Cache file format version - increment when format changes
#define SPRITE_CACHE_VERSION 1

// Magic number to identify cache files: "SPRC"
#define SPRITE_CACHE_MAGIC 0x43525053

#pragma pack(push, 1)
struct SpriteCacheHeader {
	uint32_t magic;              // Magic number "SPRC"
	uint32_t version;            // Cache format version
	uint64_t spr_modified_time;  // Modification time of Tibia.spr
	uint64_t dat_modified_time;  // Modification time of Tibia.dat
	uint64_t spr_file_size;      // Size of Tibia.spr
	uint64_t dat_file_size;      // Size of Tibia.dat
	uint32_t spr_signature;      // Signature from .spr file
	uint32_t dat_signature;      // Signature from .dat file
	uint32_t sprite_count;       // Total number of sprites in cache
	uint32_t is_extended;        // Whether extended sprite format is used
	uint32_t has_transparency;   // Whether sprites have alpha channel
	uint32_t reserved[4];        // Reserved for future use
};

struct SpriteCacheEntry {
	uint32_t id;                 // Sprite ID
	uint32_t offset;             // Offset in cache file where data starts
	uint16_t size;               // Size of sprite data
};
#pragma pack(pop)

class SpriteCache {
public:
	SpriteCache();
	~SpriteCache();

	// Check if a valid cache exists for the given spr/dat files
	bool isValidCache(const std::string& cache_path,
	                  const std::string& spr_path,
	                  const std::string& dat_path,
	                  uint32_t spr_signature,
	                  uint32_t dat_signature,
	                  bool is_extended,
	                  bool has_transparency);

	// Load sprite data from cache
	// Returns true if cache was loaded successfully
	bool loadFromCache(const std::string& cache_path,
	                   std::vector<uint32_t>& sprite_offsets,
	                   std::vector<uint16_t>& sprite_sizes,
	                   std::vector<uint8_t*>& sprite_dumps);

	// Save sprite data to cache
	bool saveToCache(const std::string& cache_path,
	                 const std::string& spr_path,
	                 const std::string& dat_path,
	                 uint32_t spr_signature,
	                 uint32_t dat_signature,
	                 bool is_extended,
	                 bool has_transparency,
	                 const std::vector<uint32_t>& sprite_ids,
	                 const std::vector<uint16_t>& sprite_sizes,
	                 const std::vector<uint8_t*>& sprite_dumps);

	// Get cache file path for a given client version
	static std::string getCachePath(const std::string& local_data_path);

	// Delete cache file
	static bool deleteCache(const std::string& cache_path);

private:
	// Get file modification time
	static uint64_t getFileModTime(const std::string& path);

	// Get file size
	static uint64_t getFileSize(const std::string& path);

	SpriteCacheHeader header;
};

#endif // RME_SPRITE_CACHE_H_
