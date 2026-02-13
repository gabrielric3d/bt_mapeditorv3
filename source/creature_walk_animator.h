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

#ifndef RME_CREATURE_WALK_ANIMATOR_H_
#define RME_CREATURE_WALK_ANIMATOR_H_

#include <unordered_map>
#include <cstdint>
#include <random>
#include "creature.h"

struct CreatureWalkState {
	enum Phase {
		IDLE,      // Waiting to start (random initial delay)
		WALKING,   // Moving between tiles (pixel interpolation)
		RESTING    // Paused between walk bursts
	};

	Phase phase = IDLE;
	int offset_x = 0;          // Current visual offset in tiles from origin
	int offset_y = 0;
	float pixel_progress = 0.f; // 0.0 to 1.0 interpolation within current step
	Direction walk_dir = SOUTH; // Current walking direction
	int steps_taken = 0;        // Steps completed in current burst
	int rest_countdown = 0;     // Frames remaining in rest phase
	int idle_countdown = 0;     // Frames remaining in initial idle delay
	uint32_t last_update_tick = 0; // For delta-time calculation
};

class CreatureWalkAnimator {
public:
	static CreatureWalkAnimator& getInstance();

	// Called once per frame from Draw()
	void update(uint32_t current_tick_ms);

	// Get pixel offset for a creature at given map position
	struct WalkOffset {
		int pixel_x = 0;
		int pixel_y = 0;
		Direction direction = SOUTH;
		bool is_walking = false;
	};
	WalkOffset getOffset(int map_x, int map_y, int map_z,
						const Creature* creature) const;

	// Register a creature for animation (called during DrawTile)
	void ensureRegistered(int map_x, int map_y, int map_z,
						 const Creature* creature);

	// Clear all states
	void clear();

	// Remove stale entries not seen in last N frames
	void garbageCollect(uint32_t current_tick_ms);

	bool isEnabled() const { return enabled; }
	void setEnabled(bool v) { enabled = v; if(!v) clear(); }

private:
	CreatureWalkAnimator() = default;

	struct PositionKey {
		int x, y, z;
		bool operator==(const PositionKey& o) const {
			return x == o.x && y == o.y && z == o.z;
		}
	};

	struct PositionHash {
		size_t operator()(const PositionKey& k) const {
			size_t h = std::hash<int>()(k.x);
			h ^= std::hash<int>()(k.y) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<int>()(k.z) + 0x9e3779b9 + (h << 6) + (h >> 2);
			return h;
		}
	};

	std::unordered_map<PositionKey, CreatureWalkState, PositionHash> states;
	bool enabled = false;
	uint32_t last_gc_tick = 0;
	uint32_t current_tick = 0; // Set each frame by update()

	void updateCreature(CreatureWalkState& state,
					   const Creature* creature,
					   uint32_t current_tick_ms);
	Direction pickNextDirection(const CreatureWalkState& state,
							   int wander_radius);
	int randomInt(int min, int max);

	std::mt19937 rng{std::random_device{}()};
};

extern CreatureWalkAnimator& g_creature_walk_animator;

#endif
