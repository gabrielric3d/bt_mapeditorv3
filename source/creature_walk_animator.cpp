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

#include "creature_walk_animator.h"
#include "const.h"

#include <cmath>
#include <algorithm>

// Direction offsets: NORTH=0, EAST=1, SOUTH=2, WEST=3
static const int dx[] = {0, 1, 0, -1};
static const int dy[] = {-1, 0, 1, 0};

CreatureWalkAnimator& CreatureWalkAnimator::getInstance()
{
	static CreatureWalkAnimator instance;
	return instance;
}

CreatureWalkAnimator& g_creature_walk_animator = CreatureWalkAnimator::getInstance();

int CreatureWalkAnimator::randomInt(int min, int max) {
	std::uniform_int_distribution<int> dist(min, max);
	return dist(rng);
}

void CreatureWalkAnimator::update(uint32_t current_tick_ms)
{
	if(!enabled)
		return;

	current_tick = current_tick_ms;

	// Creature state updates happen in ensureRegistered() which is called
	// per-creature during DrawTile, since we need the Creature pointer
	// for walk speed/steps/radius parameters.
	// This method stores the frame tick and handles garbage collection.
	if(current_tick_ms - last_gc_tick >= 5000) {
		garbageCollect(current_tick_ms);
	}
}

void CreatureWalkAnimator::updateCreature(CreatureWalkState& state,
										 const Creature* creature,
										 uint32_t current_tick_ms)
{
	if(!creature)
		return;

	// Calculate delta time
	uint32_t delta_ms = 0;
	if(state.last_update_tick > 0) {
		delta_ms = current_tick_ms - state.last_update_tick; // Correct even on wrap-around
	}
	// Cap delta to prevent large jumps (e.g. when window was minimized)
	if(delta_ms > 100)
		delta_ms = 100;

	state.last_update_tick = current_tick_ms;

	// If delta is 0, nothing to update
	if(delta_ms == 0)
		return;

	int wander_radius = creature->getWanderRadius();

	switch(state.phase) {
		case CreatureWalkState::IDLE: {
			// Approximate frames from delta_ms (assuming ~16ms per frame at 60fps)
			int frame_ticks = std::max(1, (int)(delta_ms / 16));
			state.idle_countdown -= frame_ticks;
			if(state.idle_countdown <= 0) {
				state.phase = CreatureWalkState::WALKING;
				state.walk_dir = pickNextDirection(state, wander_radius);
				state.pixel_progress = 0.f;
				state.steps_taken = 0;
			}
			break;
		}

		case CreatureWalkState::WALKING: {
			// Calculate walk duration based on creature speed
			int walk_speed = creature->getWalkSpeed();
			float walk_duration_ms = (float)std::max(200, 1000 - walk_speed * 8);

			// Advance interpolation progress
			state.pixel_progress += (float)delta_ms / walk_duration_ms;

			if(state.pixel_progress >= 1.0f) {
				// Step complete - commit the tile offset
				state.offset_x += dx[state.walk_dir];
				state.offset_y += dy[state.walk_dir];
				state.pixel_progress = 0.f;
				state.steps_taken++;

				int max_steps = creature->getWalkSteps();
				if(state.steps_taken >= max_steps) {
					// Done with this burst, transition to resting
					state.phase = CreatureWalkState::RESTING;
					state.rest_countdown = creature->getRestTicks();
					state.steps_taken = 0;
				} else {
					// Continue walking, pick next direction
					state.walk_dir = pickNextDirection(state, wander_radius);
				}
			}
			break;
		}

		case CreatureWalkState::RESTING: {
			int frame_ticks = std::max(1, (int)(delta_ms / 16));
			state.rest_countdown -= frame_ticks;
			if(state.rest_countdown <= 0) {
				state.steps_taken = 0;
				state.walk_dir = pickNextDirection(state, wander_radius);
				state.phase = CreatureWalkState::WALKING;
				state.pixel_progress = 0.f;
			}
			break;
		}
	}
}

CreatureWalkAnimator::WalkOffset CreatureWalkAnimator::getOffset(
	int map_x, int map_y, int map_z, const Creature* creature) const
{
	WalkOffset result;

	if(!enabled || !creature)
		return result;

	PositionKey key{map_x, map_y, map_z};
	auto it = states.find(key);
	if(it == states.end())
		return result;

	const CreatureWalkState& state = it->second;

	// Base offset from completed tile steps
	result.pixel_x = state.offset_x * rme::TileSize;
	result.pixel_y = state.offset_y * rme::TileSize;

	// Add sub-tile interpolation during walking phase
	if(state.phase == CreatureWalkState::WALKING) {
		result.pixel_x += (int)(dx[state.walk_dir] * state.pixel_progress * rme::TileSize);
		result.pixel_y += (int)(dy[state.walk_dir] * state.pixel_progress * rme::TileSize);
	}

	result.direction = state.walk_dir;
	result.is_walking = (state.phase == CreatureWalkState::WALKING);

	return result;
}

void CreatureWalkAnimator::ensureRegistered(int map_x, int map_y, int map_z,
										   const Creature* creature)
{
	if(!enabled || !creature || !creature->hasWanderBehavior())
		return;

	PositionKey key{map_x, map_y, map_z};
	auto it = states.find(key);

	if(it != states.end()) {
		// If creature was off-screen for too long, reset its animation
		if(current_tick - it->second.last_update_tick > 2000) {
			it->second.phase = CreatureWalkState::IDLE;
			it->second.idle_countdown = randomInt(0, 59);  // Short delay before restart
			it->second.offset_x = 0;
			it->second.offset_y = 0;
			it->second.pixel_progress = 0.f;
			it->second.steps_taken = 0;
			it->second.last_update_tick = current_tick;
			return;
		}
		// Normal update
		it->second.last_update_tick = current_tick;
		updateCreature(it->second, creature, current_tick);
		return;
	}

	// Don't exceed capacity limit
	if(states.size() >= 200)
		return;

	// Create new state with random initial delay
	CreatureWalkState state;
	state.phase = CreatureWalkState::IDLE;
	state.idle_countdown = randomInt(0, 179); // 0-3 seconds random start delay
	state.offset_x = 0;
	state.offset_y = 0;
	state.pixel_progress = 0.f;
	state.walk_dir = SOUTH;
	state.steps_taken = 0;
	state.rest_countdown = 0;
	state.last_update_tick = current_tick;

	states[key] = state;
}

Direction CreatureWalkAnimator::pickNextDirection(const CreatureWalkState& state,
												 int wander_radius)
{
	// If wander_radius is 0 or negative, just pick a random direction
	if(wander_radius <= 0) {
		return static_cast<Direction>(randomInt(0, 3));
	}

	// Try a random direction first
	Direction dir = static_cast<Direction>(randomInt(0, 3));
	int new_x = state.offset_x + dx[dir];
	int new_y = state.offset_y + dy[dir];

	// Check if the new position would be within wander radius
	if(std::abs(new_x) <= wander_radius && std::abs(new_y) <= wander_radius) {
		return dir;
	}

	// Out of bounds - pick a direction that moves toward center
	// with some randomness to avoid predictable patterns
	Direction preferred = SOUTH; // default fallback

	// Determine which axis is further from center
	bool prefer_x = (std::abs(state.offset_x) > std::abs(state.offset_y));

	// Add randomness: 30% chance to prefer the other axis
	if(randomInt(0, 99) < 30)
		prefer_x = !prefer_x;

	if(prefer_x) {
		if(state.offset_x > 0)
			preferred = WEST;
		else if(state.offset_x < 0)
			preferred = EAST;
		else
			preferred = (randomInt(0, 1) == 0) ? WEST : EAST;
	} else {
		if(state.offset_y > 0)
			preferred = NORTH;
		else if(state.offset_y < 0)
			preferred = SOUTH;
		else
			preferred = (randomInt(0, 1) == 0) ? NORTH : SOUTH;
	}

	// Verify the preferred direction is valid
	new_x = state.offset_x + dx[preferred];
	new_y = state.offset_y + dy[preferred];
	if(std::abs(new_x) <= wander_radius && std::abs(new_y) <= wander_radius) {
		return preferred;
	}

	// If even the preferred direction is out of bounds, try all four
	for(int i = 0; i < 4; i++) {
		Direction try_dir = static_cast<Direction>(i);
		new_x = state.offset_x + dx[try_dir];
		new_y = state.offset_y + dy[try_dir];
		if(std::abs(new_x) <= wander_radius && std::abs(new_y) <= wander_radius) {
			return try_dir;
		}
	}

	// Unreachable fallback -- all directions should have been checked above
	return SOUTH;
}

void CreatureWalkAnimator::garbageCollect(uint32_t current_tick_ms)
{
	last_gc_tick = current_tick_ms;

	for(auto it = states.begin(); it != states.end(); ) {
		if(current_tick_ms - it->second.last_update_tick > 5000) {
			it = states.erase(it);
		} else {
			++it;
		}
	}
}

void CreatureWalkAnimator::clear()
{
	states.clear();
}
