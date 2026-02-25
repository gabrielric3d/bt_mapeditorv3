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

#include "chunk_tracker.h"

#include <sstream>
#include <iomanip>

std::string ChunkKey::toFilename() const {
	std::ostringstream oss;
	oss << std::setfill('0') << std::setw(4) << std::hex << x
		<< "_"
		<< std::setfill('0') << std::setw(4) << std::hex << y
		<< "_"
		<< std::setfill('0') << std::setw(2) << static_cast<int>(z)
		<< ".chunk";
	return oss.str();
}

ChunkTracker::ChunkTracker() :
	metadata_dirty(false),
	full_save_needed(true) // First save is always full
{
}

void ChunkTracker::markDirty(int pos_x, int pos_y, int pos_z) {
	dirty_chunks.insert(ChunkKey::fromPosition(pos_x, pos_y, pos_z));
}

void ChunkTracker::markDirty(const ChunkKey& key) {
	dirty_chunks.insert(key);
}

void ChunkTracker::markAllDirty() {
	full_save_needed = true;
}

bool ChunkTracker::isDirty(const ChunkKey& key) const {
	return dirty_chunks.find(key) != dirty_chunks.end();
}

const std::unordered_set<ChunkKey, ChunkKeyHash>& ChunkTracker::getDirtyChunks() const {
	return dirty_chunks;
}

void ChunkTracker::clearDirty() {
	dirty_chunks.clear();
	full_save_needed = false;
}

size_t ChunkTracker::dirtyCount() const {
	return dirty_chunks.size();
}
