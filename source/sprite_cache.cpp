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
#include "sprite_cache.h"

#include <fstream>
#include <sys/stat.h>

#ifdef _WIN32
#include <sys/types.h>
#endif

SpriteCache::SpriteCache() {
	memset(&header, 0, sizeof(header));
}

SpriteCache::~SpriteCache() {
}

uint64_t SpriteCache::getFileModTime(const std::string& path) {
	struct stat file_stat;
	if (stat(path.c_str(), &file_stat) != 0) {
		return 0;
	}
	return static_cast<uint64_t>(file_stat.st_mtime);
}

uint64_t SpriteCache::getFileSize(const std::string& path) {
	struct stat file_stat;
	if (stat(path.c_str(), &file_stat) != 0) {
		return 0;
	}
	return static_cast<uint64_t>(file_stat.st_size);
}

std::string SpriteCache::getCachePath(const std::string& local_data_path) {
	return local_data_path + "sprites.cache";
}

bool SpriteCache::deleteCache(const std::string& cache_path) {
	return std::remove(cache_path.c_str()) == 0;
}

bool SpriteCache::isValidCache(const std::string& cache_path,
                               const std::string& spr_path,
                               const std::string& dat_path,
                               uint32_t spr_signature,
                               uint32_t dat_signature,
                               bool is_extended,
                               bool has_transparency) {
	std::ifstream file(cache_path, std::ios::binary);
	if (!file.is_open()) {
		return false;
	}

	SpriteCacheHeader cached_header;
	file.read(reinterpret_cast<char*>(&cached_header), sizeof(cached_header));
	if (!file.good()) {
		return false;
	}

	// Check magic number
	if (cached_header.magic != SPRITE_CACHE_MAGIC) {
		return false;
	}

	// Check version
	if (cached_header.version != SPRITE_CACHE_VERSION) {
		return false;
	}

	// Check signatures
	if (cached_header.spr_signature != spr_signature ||
	    cached_header.dat_signature != dat_signature) {
		return false;
	}

	// Check format flags
	if (cached_header.is_extended != (is_extended ? 1 : 0) ||
	    cached_header.has_transparency != (has_transparency ? 1 : 0)) {
		return false;
	}

	// Check file timestamps and sizes
	uint64_t spr_mod_time = getFileModTime(spr_path);
	uint64_t dat_mod_time = getFileModTime(dat_path);
	uint64_t spr_size = getFileSize(spr_path);
	uint64_t dat_size = getFileSize(dat_path);

	if (cached_header.spr_modified_time != spr_mod_time ||
	    cached_header.dat_modified_time != dat_mod_time ||
	    cached_header.spr_file_size != spr_size ||
	    cached_header.dat_file_size != dat_size) {
		return false;
	}

	return true;
}

bool SpriteCache::loadFromCache(const std::string& cache_path,
                                std::vector<uint32_t>& sprite_offsets,
                                std::vector<uint16_t>& sprite_sizes,
                                std::vector<uint8_t*>& sprite_dumps) {
	std::ifstream file(cache_path, std::ios::binary);
	if (!file.is_open()) {
		return false;
	}

	// Read header
	file.read(reinterpret_cast<char*>(&header), sizeof(header));
	if (!file.good() || header.magic != SPRITE_CACHE_MAGIC) {
		return false;
	}

	uint32_t sprite_count = header.sprite_count;

	// Read sprite index table
	std::vector<SpriteCacheEntry> entries(sprite_count);
	file.read(reinterpret_cast<char*>(entries.data()),
	          sprite_count * sizeof(SpriteCacheEntry));
	if (!file.good()) {
		return false;
	}

	// Prepare output vectors
	// Find max ID to size the vectors appropriately
	uint32_t max_id = 0;
	for (const auto& entry : entries) {
		if (entry.id > max_id) {
			max_id = entry.id;
		}
	}

	sprite_offsets.resize(max_id + 1, 0);
	sprite_sizes.resize(max_id + 1, 0);
	sprite_dumps.resize(max_id + 1, nullptr);

	// Read sprite data
	for (const auto& entry : entries) {
		if (entry.size > 0) {
			sprite_offsets[entry.id] = entry.offset;
			sprite_sizes[entry.id] = entry.size;

			// Read the actual sprite data
			uint8_t* dump = new uint8_t[entry.size];
			file.seekg(entry.offset);
			file.read(reinterpret_cast<char*>(dump), entry.size);
			if (!file.good()) {
				delete[] dump;
				// Clean up already allocated dumps
				for (auto& d : sprite_dumps) {
					delete[] d;
					d = nullptr;
				}
				return false;
			}
			sprite_dumps[entry.id] = dump;
		}
	}

	return true;
}

bool SpriteCache::saveToCache(const std::string& cache_path,
                              const std::string& spr_path,
                              const std::string& dat_path,
                              uint32_t spr_signature,
                              uint32_t dat_signature,
                              bool is_extended,
                              bool has_transparency,
                              const std::vector<uint32_t>& sprite_ids,
                              const std::vector<uint16_t>& sprite_sizes,
                              const std::vector<uint8_t*>& sprite_dumps) {
	std::ofstream file(cache_path, std::ios::binary | std::ios::trunc);
	if (!file.is_open()) {
		return false;
	}

	// Prepare header
	SpriteCacheHeader cache_header;
	memset(&cache_header, 0, sizeof(cache_header));
	cache_header.magic = SPRITE_CACHE_MAGIC;
	cache_header.version = SPRITE_CACHE_VERSION;
	cache_header.spr_modified_time = getFileModTime(spr_path);
	cache_header.dat_modified_time = getFileModTime(dat_path);
	cache_header.spr_file_size = getFileSize(spr_path);
	cache_header.dat_file_size = getFileSize(dat_path);
	cache_header.spr_signature = spr_signature;
	cache_header.dat_signature = dat_signature;
	cache_header.is_extended = is_extended ? 1 : 0;
	cache_header.has_transparency = has_transparency ? 1 : 0;

	// Count valid sprites
	uint32_t valid_sprite_count = 0;
	for (size_t i = 0; i < sprite_ids.size(); ++i) {
		if (sprite_sizes[i] > 0 && sprite_dumps[i] != nullptr) {
			valid_sprite_count++;
		}
	}
	cache_header.sprite_count = valid_sprite_count;

	// Write header
	file.write(reinterpret_cast<const char*>(&cache_header), sizeof(cache_header));
	if (!file.good()) {
		return false;
	}

	// Calculate data offset (after header and index table)
	uint32_t data_offset = sizeof(SpriteCacheHeader) +
	                       valid_sprite_count * sizeof(SpriteCacheEntry);

	// Build index entries and write them
	std::vector<SpriteCacheEntry> entries;
	entries.reserve(valid_sprite_count);

	uint32_t current_offset = data_offset;
	for (size_t i = 0; i < sprite_ids.size(); ++i) {
		if (sprite_sizes[i] > 0 && sprite_dumps[i] != nullptr) {
			SpriteCacheEntry entry;
			entry.id = sprite_ids[i];
			entry.offset = current_offset;
			entry.size = sprite_sizes[i];
			entries.push_back(entry);
			current_offset += sprite_sizes[i];
		}
	}

	// Write index table
	file.write(reinterpret_cast<const char*>(entries.data()),
	           entries.size() * sizeof(SpriteCacheEntry));
	if (!file.good()) {
		return false;
	}

	// Write sprite data
	for (size_t i = 0; i < sprite_ids.size(); ++i) {
		if (sprite_sizes[i] > 0 && sprite_dumps[i] != nullptr) {
			file.write(reinterpret_cast<const char*>(sprite_dumps[i]), sprite_sizes[i]);
			if (!file.good()) {
				return false;
			}
		}
	}

	file.flush();
	return file.good();
}
