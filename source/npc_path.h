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

#ifndef RME_NPC_PATH_H_
#define RME_NPC_PATH_H_

#include "main.h"
#include "position.h"

#include <cstdint>
#include <string>
#include <vector>

// NPC Actions that can occur at waypoints
enum class NPCActionType {
	None = 0,       // Just walk through
	Speak,          // Say a message
	Wait,           // Pause for duration
	FaceDirection,  // Turn to face a direction
	Emote           // Play an emote animation
};

struct NPCAction {
	NPCActionType type = NPCActionType::None;
	std::string message;           // For Speak action
	double duration = 0.0;         // For Wait action (seconds)
	int direction = 0;             // For FaceDirection (0-3: N/E/S/W)
	int emote_id = 0;              // For Emote action
};

struct NPCWaypoint {
	Position pos;
	double walk_speed = 1.0;       // Tiles per second
	double wait_before = 0.0;      // Wait before moving to this point
	double wait_after = 0.0;       // Wait after reaching this point
	std::vector<NPCAction> actions; // Actions to perform at this waypoint
};

struct NPCPathColor {
	uint8_t r = 80;
	uint8_t g = 200;
	uint8_t b = 255;
};

struct NPCPath {
	std::string name;
	std::string npc_name;          // Which NPC follows this path
	bool loop = true;              // Loop path or stop at end
	bool active = true;            // Is this path currently active
	NPCPathColor color;            // Visualization color
	std::vector<NPCWaypoint> waypoints;
};

struct NPCPathsSnapshot {
	std::vector<NPCPath> paths;
	std::string active_path;
	int active_waypoint = -1;
};

class NPCPaths {
public:
	NPCPaths();

	NPCPath* addPath(const std::string& name);
	bool removePath(const std::string& name);
	void clear();

	NPCPath* getPath(const std::string& name);
	const NPCPath* getPath(const std::string& name) const;
	NPCPath* getActivePath();
	const NPCPath* getActivePath() const;

	const std::vector<NPCPath>& getPaths() const { return paths; }
	std::vector<NPCPath>& getPaths() { return paths; }

	void setActivePath(const std::string& name);
	const std::string& getActivePathName() const { return active_path; }
	void setActiveWaypoint(int index);
	int getActiveWaypoint() const { return active_waypoint; }

	NPCPathsSnapshot snapshot() const;
	void applySnapshot(const NPCPathsSnapshot& snapshot);
	void swapSnapshot(NPCPathsSnapshot& snapshot);

	std::string generateUniquePathName(const std::string& base) const;

	bool loadFromFile(const FileName& mapFile, wxString* outError = nullptr);
	bool saveToFile(const FileName& mapFile, wxString* outError = nullptr) const;
	static FileName BuildSidecarPath(const FileName& mapFile);

private:
	std::vector<NPCPath> paths;
	std::string active_path;
	int active_waypoint;
};

#endif
