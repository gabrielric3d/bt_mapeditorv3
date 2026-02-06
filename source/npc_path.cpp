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

#include "npc_path.h"

#include <fstream>

namespace {

const NPCPathColor kDefaultColors[] = {
	{80, 200, 255},   // Cyan
	{255, 160, 80},   // Orange
	{160, 255, 100},  // Light green
	{255, 100, 180},  // Pink
	{180, 140, 255},  // Purple
	{255, 220, 80},   // Yellow
	{100, 255, 200},  // Teal
};

NPCPathColor PickColor(size_t index)
{
	return kDefaultColors[index % (sizeof(kDefaultColors) / sizeof(kDefaultColors[0]))];
}

std::string NPCActionTypeToString(NPCActionType type)
{
	switch(type) {
		case NPCActionType::Speak: return "speak";
		case NPCActionType::Wait: return "wait";
		case NPCActionType::FaceDirection: return "face_direction";
		case NPCActionType::Emote: return "emote";
		default: return "none";
	}
}

NPCActionType StringToNPCActionType(const std::string& str)
{
	if(str == "speak") return NPCActionType::Speak;
	if(str == "wait") return NPCActionType::Wait;
	if(str == "face_direction") return NPCActionType::FaceDirection;
	if(str == "emote") return NPCActionType::Emote;
	return NPCActionType::None;
}

} // namespace

NPCPaths::NPCPaths() :
	active_waypoint(-1)
{
}

NPCPath* NPCPaths::addPath(const std::string& name)
{
	NPCPath path;
	path.name = generateUniquePathName(name.empty() ? "NPC Path" : name);
	path.npc_name = "";
	path.loop = true;
	path.active = true;
	path.color = PickColor(paths.size());
	paths.push_back(path);
	setActivePath(path.name);
	active_waypoint = -1;
	return &paths.back();
}

bool NPCPaths::removePath(const std::string& name)
{
	for(size_t i = 0; i < paths.size(); ++i) {
		if(paths[i].name == name) {
			paths.erase(paths.begin() + static_cast<long>(i));
			if(active_path == name) {
				if(paths.empty()) {
					active_path.clear();
					active_waypoint = -1;
				} else {
					active_path = paths.front().name;
					active_waypoint = -1;
				}
			}
			return true;
		}
	}
	return false;
}

void NPCPaths::clear()
{
	paths.clear();
	active_path.clear();
	active_waypoint = -1;
}

NPCPath* NPCPaths::getPath(const std::string& name)
{
	for(NPCPath& path : paths) {
		if(path.name == name) {
			return &path;
		}
	}
	return nullptr;
}

const NPCPath* NPCPaths::getPath(const std::string& name) const
{
	for(const NPCPath& path : paths) {
		if(path.name == name) {
			return &path;
		}
	}
	return nullptr;
}

NPCPath* NPCPaths::getActivePath()
{
	if(active_path.empty()) {
		return paths.empty() ? nullptr : &paths.front();
	}
	NPCPath* path = getPath(active_path);
	if(!path && !paths.empty()) {
		active_path = paths.front().name;
		return &paths.front();
	}
	return path;
}

const NPCPath* NPCPaths::getActivePath() const
{
	if(active_path.empty()) {
		return paths.empty() ? nullptr : &paths.front();
	}
	const NPCPath* path = getPath(active_path);
	return path ? path : (paths.empty() ? nullptr : &paths.front());
}

void NPCPaths::setActivePath(const std::string& name)
{
	active_path = name;
}

void NPCPaths::setActiveWaypoint(int index)
{
	active_waypoint = index;
}

NPCPathsSnapshot NPCPaths::snapshot() const
{
	NPCPathsSnapshot snap;
	snap.paths = paths;
	snap.active_path = active_path;
	snap.active_waypoint = active_waypoint;
	return snap;
}

void NPCPaths::applySnapshot(const NPCPathsSnapshot& snapshot)
{
	paths = snapshot.paths;
	active_path = snapshot.active_path;
	active_waypoint = snapshot.active_waypoint;
}

void NPCPaths::swapSnapshot(NPCPathsSnapshot& snapshot)
{
	std::swap(paths, snapshot.paths);
	std::swap(active_path, snapshot.active_path);
	std::swap(active_waypoint, snapshot.active_waypoint);
}

std::string NPCPaths::generateUniquePathName(const std::string& base) const
{
	if(getPath(base) == nullptr) {
		return base;
	}

	for(int i = 2; i < 10000; ++i) {
		std::string name = base + " " + i2s(i);
		if(getPath(name) == nullptr) {
			return name;
		}
	}
	return base + " 10000";
}

FileName NPCPaths::BuildSidecarPath(const FileName& mapFile)
{
	FileName sidecar(mapFile);
	if(sidecar.GetFullPath().empty()) {
		return sidecar;
	}
	sidecar.SetExt("npcpaths.json");
	return sidecar;
}

bool NPCPaths::loadFromFile(const FileName& mapFile, wxString* outError)
{
	clear();
	FileName sidecar = BuildSidecarPath(mapFile);
	if(sidecar.GetFullPath().empty() || !sidecar.FileExists()) {
		return false;
	}

	std::ifstream file(nstr(sidecar.GetFullPath()).c_str(), std::ios::in);
	if(!file.is_open()) {
		if(outError) {
			*outError = "Could not open NPC path file.";
		}
		return false;
	}

	try {
		nlohmann::json root;
		file >> root;
		if(!root.is_object()) {
			if(outError) {
				*outError = "NPC path file has invalid format.";
			}
			return false;
		}

		const nlohmann::json& pathsNode = root.value("paths", nlohmann::json::array());
		if(!pathsNode.is_array()) {
			if(outError) {
				*outError = "NPC path file has invalid paths list.";
			}
			return false;
		}

		for(const auto& pathNode : pathsNode) {
			if(!pathNode.is_object()) {
				continue;
			}

			NPCPath path;
			path.name = pathNode.value("name", std::string());
			if(path.name.empty()) {
				path.name = generateUniquePathName("NPC Path");
			} else {
				path.name = generateUniquePathName(path.name);
			}

			path.npc_name = pathNode.value("npc_name", std::string());
			path.loop = pathNode.value("loop", true);
			path.active = pathNode.value("active", true);

			if(pathNode.contains("color") && pathNode["color"].is_array() && pathNode["color"].size() >= 3) {
				path.color.r = static_cast<uint8_t>(pathNode["color"][0].get<int>());
				path.color.g = static_cast<uint8_t>(pathNode["color"][1].get<int>());
				path.color.b = static_cast<uint8_t>(pathNode["color"][2].get<int>());
			} else {
				path.color = PickColor(paths.size());
			}

			const nlohmann::json& waypointsNode = pathNode.value("waypoints", nlohmann::json::array());
			if(waypointsNode.is_array()) {
				for(const auto& wpNode : waypointsNode) {
					if(!wpNode.is_object()) {
						continue;
					}

					NPCWaypoint waypoint;
					waypoint.pos.x = wpNode.value("x", 0);
					waypoint.pos.y = wpNode.value("y", 0);
					waypoint.pos.z = wpNode.value("z", rme::MapGroundLayer);
					waypoint.walk_speed = wpNode.value("walk_speed", 1.0);
					waypoint.wait_before = wpNode.value("wait_before", 0.0);
					waypoint.wait_after = wpNode.value("wait_after", 0.0);

					// Load actions
					const nlohmann::json& actionsNode = wpNode.value("actions", nlohmann::json::array());
					if(actionsNode.is_array()) {
						for(const auto& actionNode : actionsNode) {
							if(!actionNode.is_object()) {
								continue;
							}

							NPCAction action;
							std::string typeStr = actionNode.value("type", std::string("none"));
							action.type = StringToNPCActionType(typeStr);
							action.message = actionNode.value("message", std::string());
							action.duration = actionNode.value("duration", 0.0);
							action.direction = actionNode.value("direction", 0);
							action.emote_id = actionNode.value("emote_id", 0);
							waypoint.actions.push_back(action);
						}
					}

					path.waypoints.push_back(waypoint);
				}
			}

			paths.push_back(path);
		}

		if(!paths.empty()) {
			active_path = paths.front().name;
			active_waypoint = -1;
		}
	} catch(const std::exception& e) {
		if(outError) {
			*outError = "NPC path file parse error: " + wxString(e.what());
		}
		return false;
	}

	return true;
}

bool NPCPaths::saveToFile(const FileName& mapFile, wxString* outError) const
{
	FileName sidecar = BuildSidecarPath(mapFile);
	if(sidecar.GetFullPath().empty()) {
		return false;
	}

	nlohmann::json root;
	root["version"] = 1;
	root["paths"] = nlohmann::json::array();

	for(const NPCPath& path : paths) {
		nlohmann::json pathNode;
		pathNode["name"] = path.name;
		pathNode["npc_name"] = path.npc_name;
		pathNode["loop"] = path.loop;
		pathNode["active"] = path.active;
		pathNode["color"] = { path.color.r, path.color.g, path.color.b };

		nlohmann::json waypoints = nlohmann::json::array();
		for(const NPCWaypoint& wp : path.waypoints) {
			nlohmann::json wpNode;
			wpNode["x"] = wp.pos.x;
			wpNode["y"] = wp.pos.y;
			wpNode["z"] = wp.pos.z;
			wpNode["walk_speed"] = wp.walk_speed;
			wpNode["wait_before"] = wp.wait_before;
			wpNode["wait_after"] = wp.wait_after;

			nlohmann::json actions = nlohmann::json::array();
			for(const NPCAction& action : wp.actions) {
				nlohmann::json actionNode;
				actionNode["type"] = NPCActionTypeToString(action.type);

				// Only include relevant fields based on action type
				switch(action.type) {
					case NPCActionType::Speak:
						actionNode["message"] = action.message;
						break;
					case NPCActionType::Wait:
						actionNode["duration"] = action.duration;
						break;
					case NPCActionType::FaceDirection:
						actionNode["direction"] = action.direction;
						break;
					case NPCActionType::Emote:
						actionNode["emote_id"] = action.emote_id;
						break;
					default:
						break;
				}
				actions.push_back(actionNode);
			}
			wpNode["actions"] = actions;
			waypoints.push_back(wpNode);
		}
		pathNode["waypoints"] = waypoints;
		root["paths"].push_back(pathNode);
	}

	std::ofstream file(nstr(sidecar.GetFullPath()).c_str(), std::ios::out | std::ios::trunc);
	if(!file.is_open()) {
		if(outError) {
			*outError = "Could not write NPC path file.";
		}
		return false;
	}

	try {
		file << root.dump(2);
	} catch(const std::exception& e) {
		if(outError) {
			*outError = "NPC path write error: " + wxString(e.what());
		}
		return false;
	}

	return true;
}
