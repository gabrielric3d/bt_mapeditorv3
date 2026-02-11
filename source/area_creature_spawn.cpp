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
#include "area_creature_spawn.h"

#include "action.h"
#include "creature.h"
#include "creatures.h"
#include "editor.h"
#include "gui.h"
#include "map.h"
#include "settings.h"
#include "spawn.h"
#include "tile.h"

#include <algorithm>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <wx/dir.h>
#include <wx/utils.h>

namespace AreaCreatureSpawn {

namespace {

inline int ChebyshevDistance(const Position& a, const Position& b) {
	return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

inline bool HasSpawnCoverage(const Map& map, const Position& position, const Tile* tileHint = nullptr) {
	const Tile* tile = tileHint ? tileHint : map.getTile(position);
	if (!tile) {
		return false;
	}
	const TileLocation* location = tile->getLocation();
	if (!location || location->getSpawnCount() == 0) {
		return false;
	}
	if (tile->spawn != nullptr) {
		return true;
	}

	const int maxSpawnRadius = std::max(1, g_settings.getInteger(Config::MAX_SPAWN_RADIUS));
	for (int y = position.y - maxSpawnRadius; y <= position.y + maxSpawnRadius; ++y) {
		for (int x = position.x - maxSpawnRadius; x <= position.x + maxSpawnRadius; ++x) {
			const Tile* spawnTile = map.getTile(x, y, position.z);
			if (!spawnTile || !spawnTile->spawn) {
				continue;
			}
			const int radius = std::max(1, spawnTile->spawn->getSize());
			if (ChebyshevDistance(position, spawnTile->getPosition()) <= radius) {
				return true;
			}
		}
	}
	return false;
}

std::string SanitizeFilename(const std::string& name) {
	std::string out = name;
	for (char& c : out) {
		if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
			c = '_';
		}
	}
	return out;
}

const char* InstanceModeToString(InstancePlacementMode mode) {
	switch (mode) {
		case InstancePlacementMode::AutoBySpacing:
			return "auto_spacing";
		case InstancePlacementMode::Manual:
		default:
			return "manual";
	}
}

InstancePlacementMode InstanceModeFromString(const std::string& value) {
	if (value == "auto_spacing") {
		return InstancePlacementMode::AutoBySpacing;
	}
	return InstancePlacementMode::Manual;
}

const char* ProcessingProfileToString(ProcessingProfile profile) {
	switch (profile) {
		case ProcessingProfile::LowEnd:
			return "low_end";
		case ProcessingProfile::Quality:
			return "quality";
		case ProcessingProfile::Balanced:
		default:
			return "balanced";
	}
}

ProcessingProfile ProcessingProfileFromString(const std::string& value) {
	if (value == "low_end") {
		return ProcessingProfile::LowEnd;
	}
	if (value == "quality") {
		return ProcessingProfile::Quality;
	}
	return ProcessingProfile::Balanced;
}

int CandidateChunkSizeForProfile(ProcessingProfile profile) {
	switch (profile) {
		case ProcessingProfile::LowEnd:
			return 64;
		case ProcessingProfile::Quality:
			return 24;
		case ProcessingProfile::Balanced:
		default:
			return 32;
	}
}

SpawnSettings BuildEffectiveSettings(const SpawnSettings& base) {
	SpawnSettings effective = base;

	switch (base.processingProfile) {
		case ProcessingProfile::LowEnd:
			effective.availabilityRange = std::min(effective.availabilityRange, 3);
			effective.minWalkableTilesInRange = std::min(effective.minWalkableTilesInRange, 4);
			effective.bfsEscapeDistance = std::min(effective.bfsEscapeDistance, 2);
			effective.centerAttempts = std::min(effective.centerAttempts, 24);
			break;
		case ProcessingProfile::Quality:
		case ProcessingProfile::Balanced:
		default:
			break;
	}

	effective.minCreatureDistance = std::max(0, effective.minCreatureDistance);
	effective.availabilityRange = std::max(0, effective.availabilityRange);
	effective.minWalkableTilesInRange = std::max(0, effective.minWalkableTilesInRange);
	effective.bfsEscapeDistance = std::max(0, effective.bfsEscapeDistance);
	effective.centerAttempts = std::max(1, effective.centerAttempts);
	effective.defaultSpawnTime = std::max(0, effective.defaultSpawnTime);
	return effective;
}

} // namespace

int CreatureGroup::getTotalCreatures() const {
	int total = 0;
	for (const auto& entry : creatures) {
		total += std::max(0, entry.count);
	}
	return total;
}

bool CreatureGroup::validate(std::string& errorOut) const {
	if (name.empty()) {
		errorOut = "Creature group with empty name";
		return false;
	}
	if (instanceMode == InstancePlacementMode::Manual && instances <= 0) {
		errorOut = "Group '" + name + "' has invalid instance count";
		return false;
	}
	if (spreadRadius < 0) {
		errorOut = "Group '" + name + "' has invalid spread radius";
		return false;
	}
	if (minGroupDistance < 0) {
		errorOut = "Group '" + name + "' has invalid minimum group distance";
		return false;
	}
	if (creatures.empty()) {
		errorOut = "Group '" + name + "' has no creatures";
		return false;
	}

	for (const auto& entry : creatures) {
		if (entry.name.empty()) {
			errorOut = "Group '" + name + "' has creature entry without name";
			return false;
		}
		if (entry.count <= 0) {
			errorOut = "Group '" + name + "' has invalid creature count";
			return false;
		}
		if (entry.spawnTime < -1) {
			errorOut = "Group '" + name + "' has invalid creature spawn time";
			return false;
		}
	}
	return true;
}

void AreaDefinition::normalize() {
	Position a = fromPos;
	Position b = toPos;
	fromPos.x = std::min(a.x, b.x);
	fromPos.y = std::min(a.y, b.y);
	fromPos.z = std::min(a.z, b.z);
	toPos.x = std::max(a.x, b.x);
	toPos.y = std::max(a.y, b.y);
	toPos.z = std::max(a.z, b.z);
}

std::vector<Position> AreaDefinition::getAllPositions() const {
	std::vector<Position> out;

	int minX = std::min(fromPos.x, toPos.x);
	int minY = std::min(fromPos.y, toPos.y);
	int minZ = std::min(fromPos.z, toPos.z);
	int maxX = std::max(fromPos.x, toPos.x);
	int maxY = std::max(fromPos.y, toPos.y);
	int maxZ = std::max(fromPos.z, toPos.z);

	if (minX < 0 || minY < 0 || minZ < 0) {
		return out;
	}

	const size_t width = static_cast<size_t>(maxX - minX + 1);
	const size_t height = static_cast<size_t>(maxY - minY + 1);
	const size_t floors = static_cast<size_t>(maxZ - minZ + 1);
	out.reserve(width * height * floors);

	for (int z = minZ; z <= maxZ; ++z) {
		for (int y = minY; y <= maxY; ++y) {
			for (int x = minX; x <= maxX; ++x) {
				out.push_back(Position(x, y, z));
			}
		}
	}
	return out;
}

bool SpawnPreset::validate(std::string& errorOut) const {
	if (groups.empty()) {
		errorOut = "No creature groups defined";
		return false;
	}

	bool hasEnabled = false;
	for (const auto& group : groups) {
		if (group.enabled) {
			hasEnabled = true;
		}
		if (!group.validate(errorOut)) {
			return false;
		}
	}

	if (!hasEnabled) {
		errorOut = "All creature groups are disabled";
		return false;
	}

	if (settings.minCreatureDistance < 0) {
		errorOut = "minCreatureDistance cannot be negative";
		return false;
	}
	if (settings.availabilityRange < 0) {
		errorOut = "availabilityRange cannot be negative";
		return false;
	}
	if (settings.minWalkableTilesInRange < 0) {
		errorOut = "minWalkableTilesInRange cannot be negative";
		return false;
	}
	if (settings.bfsEscapeDistance < 0) {
		errorOut = "bfsEscapeDistance cannot be negative";
		return false;
	}
	if (settings.centerAttempts <= 0) {
		errorOut = "centerAttempts must be positive";
		return false;
	}
	if (settings.defaultSpawnTime < 0) {
		errorOut = "defaultSpawnTime cannot be negative";
		return false;
	}

	return true;
}

bool SpawnPreset::saveToFile(const std::string& filepath) const {
	pugi::xml_document doc;
	pugi::xml_node root = doc.append_child("creature_spawn_preset");
	root.append_attribute("name") = name.c_str();
	root.append_attribute("version") = "1.0";

	pugi::xml_node settingsNode = root.append_child("settings");
	settingsNode.append_attribute("min_creature_distance") = settings.minCreatureDistance;
	settingsNode.append_attribute("availability_range") = settings.availabilityRange;
	settingsNode.append_attribute("min_walkable_tiles_in_range") = settings.minWalkableTilesInRange;
	settingsNode.append_attribute("bfs_escape_distance") = settings.bfsEscapeDistance;
	settingsNode.append_attribute("center_attempts") = settings.centerAttempts;
	settingsNode.append_attribute("auto_create_spawn") = settings.autoCreateSpawn;
	settingsNode.append_attribute("default_spawn_time") = settings.defaultSpawnTime;
	settingsNode.append_attribute("processing_profile") = ProcessingProfileToString(settings.processingProfile);
	settingsNode.append_attribute("default_seed") = std::to_string(defaultSeed).c_str();

	if (hasArea) {
		pugi::xml_node areaNode = root.append_child("area");
		areaNode.append_attribute("from_x") = area.fromPos.x;
		areaNode.append_attribute("from_y") = area.fromPos.y;
		areaNode.append_attribute("from_z") = area.fromPos.z;
		areaNode.append_attribute("to_x") = area.toPos.x;
		areaNode.append_attribute("to_y") = area.toPos.y;
		areaNode.append_attribute("to_z") = area.toPos.z;
	}

	pugi::xml_node groupsNode = root.append_child("groups");
	for (const auto& group : groups) {
		pugi::xml_node groupNode = groupsNode.append_child("group");
		groupNode.append_attribute("name") = group.name.c_str();
		groupNode.append_attribute("enabled") = group.enabled;
		groupNode.append_attribute("instances") = group.instances;
		groupNode.append_attribute("instance_mode") = InstanceModeToString(group.instanceMode);
		groupNode.append_attribute("spread_radius") = group.spreadRadius;
		groupNode.append_attribute("min_group_distance") = group.minGroupDistance;

		for (const auto& creature : group.creatures) {
			pugi::xml_node creatureNode = groupNode.append_child("creature");
			creatureNode.append_attribute("name") = creature.name.c_str();
			creatureNode.append_attribute("count") = creature.count;
			creatureNode.append_attribute("spawn_time") = creature.spawnTime;
		}
	}

	return doc.save_file(filepath.c_str(), "\t", pugi::format_default, pugi::encoding_utf8);
}

bool SpawnPreset::loadFromFile(const std::string& filepath) {
	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(filepath.c_str());
	if (!result) {
		return false;
	}
	std::ostringstream stream;
	doc.save(stream);
	return fromXmlString(stream.str());
}

std::string SpawnPreset::toXmlString() const {
	pugi::xml_document doc;
	pugi::xml_node root = doc.append_child("creature_spawn_preset");
	root.append_attribute("name") = name.c_str();
	root.append_attribute("version") = "1.0";

	pugi::xml_node settingsNode = root.append_child("settings");
	settingsNode.append_attribute("min_creature_distance") = settings.minCreatureDistance;
	settingsNode.append_attribute("availability_range") = settings.availabilityRange;
	settingsNode.append_attribute("min_walkable_tiles_in_range") = settings.minWalkableTilesInRange;
	settingsNode.append_attribute("bfs_escape_distance") = settings.bfsEscapeDistance;
	settingsNode.append_attribute("center_attempts") = settings.centerAttempts;
	settingsNode.append_attribute("auto_create_spawn") = settings.autoCreateSpawn;
	settingsNode.append_attribute("default_spawn_time") = settings.defaultSpawnTime;
	settingsNode.append_attribute("processing_profile") = ProcessingProfileToString(settings.processingProfile);
	settingsNode.append_attribute("default_seed") = std::to_string(defaultSeed).c_str();

	if (hasArea) {
		pugi::xml_node areaNode = root.append_child("area");
		areaNode.append_attribute("from_x") = area.fromPos.x;
		areaNode.append_attribute("from_y") = area.fromPos.y;
		areaNode.append_attribute("from_z") = area.fromPos.z;
		areaNode.append_attribute("to_x") = area.toPos.x;
		areaNode.append_attribute("to_y") = area.toPos.y;
		areaNode.append_attribute("to_z") = area.toPos.z;
	}

	pugi::xml_node groupsNode = root.append_child("groups");
	for (const auto& group : groups) {
		pugi::xml_node groupNode = groupsNode.append_child("group");
		groupNode.append_attribute("name") = group.name.c_str();
		groupNode.append_attribute("enabled") = group.enabled;
		groupNode.append_attribute("instances") = group.instances;
		groupNode.append_attribute("instance_mode") = InstanceModeToString(group.instanceMode);
		groupNode.append_attribute("spread_radius") = group.spreadRadius;
		groupNode.append_attribute("min_group_distance") = group.minGroupDistance;

		for (const auto& creature : group.creatures) {
			pugi::xml_node creatureNode = groupNode.append_child("creature");
			creatureNode.append_attribute("name") = creature.name.c_str();
			creatureNode.append_attribute("count") = creature.count;
			creatureNode.append_attribute("spawn_time") = creature.spawnTime;
		}
	}

	std::ostringstream stream;
	doc.save(stream);
	return stream.str();
}

bool SpawnPreset::fromXmlString(const std::string& xml) {
	pugi::xml_document doc;
	pugi::xml_parse_result parseResult = doc.load_buffer(xml.c_str(), xml.size());
	if (!parseResult) {
		return false;
	}

	pugi::xml_node root = doc.child("creature_spawn_preset");
	if (!root) {
		return false;
	}

	name = root.attribute("name").as_string("Unnamed Creature Preset");
	groups.clear();
	settings = SpawnSettings();

	pugi::xml_node settingsNode = root.child("settings");
	if (settingsNode) {
		settings.minCreatureDistance = settingsNode.attribute("min_creature_distance").as_int(1);
		settings.availabilityRange = settingsNode.attribute("availability_range").as_int(6);
		settings.minWalkableTilesInRange = settingsNode.attribute("min_walkable_tiles_in_range").as_int(12);
		settings.bfsEscapeDistance = settingsNode.attribute("bfs_escape_distance").as_int(4);
		settings.centerAttempts = settingsNode.attribute("center_attempts").as_int(80);
		settings.autoCreateSpawn = settingsNode.attribute("auto_create_spawn").as_bool(true);
		settings.defaultSpawnTime = settingsNode.attribute("default_spawn_time").as_int(60);
		settings.processingProfile = ProcessingProfileFromString(settingsNode.attribute("processing_profile").as_string("balanced"));
		const char* seedStr = settingsNode.attribute("default_seed").as_string("0");
		defaultSeed = std::strtoull(seedStr, nullptr, 10);
	} else {
		settings.processingProfile = ProcessingProfile::Balanced;
		defaultSeed = 0;
	}

	hasArea = false;
	pugi::xml_node areaNode = root.child("area");
	if (areaNode) {
		hasArea = true;
		area.fromPos = Position(
			areaNode.attribute("from_x").as_int(0),
			areaNode.attribute("from_y").as_int(0),
			areaNode.attribute("from_z").as_int(7));
		area.toPos = Position(
			areaNode.attribute("to_x").as_int(0),
			areaNode.attribute("to_y").as_int(0),
			areaNode.attribute("to_z").as_int(7));
	}

	pugi::xml_node groupsNode = root.child("groups");
	for (pugi::xml_node groupNode = groupsNode.child("group"); groupNode; groupNode = groupNode.next_sibling("group")) {
		CreatureGroup group;
		group.name = groupNode.attribute("name").as_string("Group");
		group.enabled = groupNode.attribute("enabled").as_bool(true);
		group.instances = groupNode.attribute("instances").as_int(1);
		group.instanceMode = InstanceModeFromString(groupNode.attribute("instance_mode").as_string("manual"));
		group.spreadRadius = groupNode.attribute("spread_radius").as_int(6);
		group.minGroupDistance = groupNode.attribute("min_group_distance").as_int(8);

		for (pugi::xml_node creatureNode = groupNode.child("creature"); creatureNode; creatureNode = creatureNode.next_sibling("creature")) {
			CreatureEntry entry;
			entry.name = creatureNode.attribute("name").as_string("");
			entry.count = creatureNode.attribute("count").as_int(1);
			entry.spawnTime = creatureNode.attribute("spawn_time").as_int(-1);
			if (!entry.name.empty() && entry.count > 0) {
				group.creatures.push_back(entry);
			}
		}

		if (!group.creatures.empty()) {
			groups.push_back(group);
		}
	}

	return true;
}

PresetManager& PresetManager::getInstance() {
	static PresetManager instance;
	return instance;
}

std::string PresetManager::getPresetsDirectory() const {
	wxString dataDir = g_gui.GetDataDirectory();
	wxString presetsBaseDir = dataDir + "/presets";
	wxString presetsDir = presetsBaseDir + "/creature_spawn";

	if (!wxDirExists(presetsBaseDir)) {
		wxMkdir(presetsBaseDir);
	}
	if (!wxDirExists(presetsDir)) {
		wxMkdir(presetsDir);
	}
	return presetsDir.ToStdString();
}

bool PresetManager::loadPresets() {
	m_presets.clear();

	wxDir dir(getPresetsDirectory());
	if (!dir.IsOpened()) {
		return false;
	}

	wxString filename;
	bool cont = dir.GetFirst(&filename, "*.xml", wxDIR_FILES);
	while (cont) {
		SpawnPreset preset;
		const std::string filepath = getPresetsDirectory() + "/" + filename.ToStdString();
		if (preset.loadFromFile(filepath)) {
			m_presets[preset.name] = preset;
		}
		cont = dir.GetNext(&filename);
	}
	return true;
}

bool PresetManager::savePresets() {
	std::string dir = getPresetsDirectory();
	for (const auto& pair : m_presets) {
		std::string filepath = dir + "/" + SanitizeFilename(pair.first) + ".xml";
		pair.second.saveToFile(filepath);
	}
	return true;
}

std::vector<std::string> PresetManager::getPresetNames() const {
	std::vector<std::string> names;
	names.reserve(m_presets.size());
	for (const auto& pair : m_presets) {
		names.push_back(pair.first);
	}
	std::sort(names.begin(), names.end());
	return names;
}

const SpawnPreset* PresetManager::getPreset(const std::string& name) const {
	auto it = m_presets.find(name);
	if (it != m_presets.end()) {
		return &it->second;
	}
	return nullptr;
}

bool PresetManager::addPreset(const SpawnPreset& preset) {
	if (preset.name.empty()) {
		return false;
	}
	m_presets[preset.name] = preset;
	std::string filepath = getPresetsDirectory() + "/" + SanitizeFilename(preset.name) + ".xml";
	return preset.saveToFile(filepath);
}

bool PresetManager::removePreset(const std::string& name) {
	auto it = m_presets.find(name);
	if (it == m_presets.end()) {
		return false;
	}
	wxRemoveFile(getPresetsDirectory() + "/" + SanitizeFilename(name) + ".xml");
	m_presets.erase(it);
	return true;
}

bool PresetManager::renamePreset(const std::string& oldName, const std::string& newName) {
	if (oldName == newName) {
		return true;
	}
	if (newName.empty()) {
		return false;
	}

	auto it = m_presets.find(oldName);
	if (it == m_presets.end()) {
		return false;
	}
	if (m_presets.find(newName) != m_presets.end()) {
		return false;
	}

	SpawnPreset preset = it->second;
	preset.name = newName;
	removePreset(oldName);
	return addPreset(preset);
}

void PreviewState::clear() {
	creatures.clear();
	spawnInstances.clear();
	creatureCounts.clear();
	seed = 0;
	isValid = false;
	errorMessage.clear();
}

SpawnEngine::SpawnEngine(Editor* editor) : m_editor(editor) {
	m_runtimeSettings = m_preset.settings;
}

void SpawnEngine::setArea(const AreaDefinition& area) {
	m_area = area;
	clearPreview();
}

void SpawnEngine::setPreset(const SpawnPreset& preset) {
	clearPreview();
	m_preset = preset;
	m_runtimeSettings = m_preset.settings;
}

bool SpawnEngine::reportProgress(const ProgressCallback& progress, const char* message) const {
	if (progress) {
		if (!progress(message ? std::string(message) : std::string())) {
			m_operationCancelled = true;
			return false;
		}
	} else {
		wxYieldIfNeeded();
	}
	return true;
}

bool SpawnEngine::tickProgress(const ProgressCallback& progress, const char* message,
                               size_t& counter, size_t interval) const {
	++counter;
	if (counter % std::max<size_t>(1, interval) != 0) {
		return true;
	}
	return reportProgress(progress, message);
}

uint64_t SpawnEngine::positionHash(const Position& pos) {
	return (static_cast<uint64_t>(pos.x & 0xFFFF) << 32) |
	       (static_cast<uint64_t>(pos.y & 0xFFFF) << 16) |
	       static_cast<uint64_t>(pos.z & 0xFFFF);
}

uint64_t SpawnEngine::chunkHash(int chunkX, int chunkY, int z) {
	return (static_cast<uint64_t>(chunkX & 0x1FFFFF) << 32) |
	       (static_cast<uint64_t>(chunkY & 0x1FFFFF) << 8) |
	       static_cast<uint64_t>(z & 0xFF);
}

void SpawnEngine::clearCandidateIndex() {
	m_candidateChunkIndices.clear();
	m_candidateFloorIndices.clear();
}

void SpawnEngine::buildCandidateIndex(const std::vector<Position>& candidates) {
	clearCandidateIndex();
	if (candidates.empty()) {
		return;
	}

	m_candidateChunkIndices.reserve(candidates.size() / std::max(8, m_candidateChunkSize));
	m_candidateFloorIndices.reserve(16);

	for (size_t i = 0; i < candidates.size(); ++i) {
		const Position& pos = candidates[i];
		const int chunkX = pos.x / m_candidateChunkSize;
		const int chunkY = pos.y / m_candidateChunkSize;
		m_candidateChunkIndices[chunkHash(chunkX, chunkY, pos.z)].push_back(i);
		m_candidateFloorIndices[pos.z].push_back(i);
	}
}

void SpawnEngine::getNearbyCandidateIndices(const Position& center, int spreadRadius, std::vector<size_t>& outIndices) const {
	outIndices.clear();
	const int chunkRadius = std::max(0, spreadRadius + m_candidateChunkSize - 1) / std::max(1, m_candidateChunkSize);
	const int centerChunkX = center.x / m_candidateChunkSize;
	const int centerChunkY = center.y / m_candidateChunkSize;

	for (int cy = centerChunkY - chunkRadius; cy <= centerChunkY + chunkRadius; ++cy) {
		for (int cx = centerChunkX - chunkRadius; cx <= centerChunkX + chunkRadius; ++cx) {
			auto it = m_candidateChunkIndices.find(chunkHash(cx, cy, center.z));
			if (it == m_candidateChunkIndices.end()) {
				continue;
			}
			const std::vector<size_t>& chunkList = it->second;
			outIndices.insert(outIndices.end(), chunkList.begin(), chunkList.end());
		}
	}
}

bool SpawnEngine::collectCandidateTiles(std::vector<Position>& outCandidates, const ProgressCallback& progress) const {
	outCandidates.clear();
	if (!m_editor) {
		return false;
	}

	Map& map = m_editor->getMap();
	const std::vector<Position> positions = m_area.getAllPositions();
	outCandidates.reserve(positions.size());

	size_t progressTick = 0;
	size_t index = 0;
	for (const Position& pos : positions) {
		++index;
		if (!tickProgress(progress, "Scanning area tiles...", progressTick, 4096)) {
			return false;
		}
		Tile* tile = map.getTile(pos);
		if (!tile) {
			continue;
		}
		if (!tile->ground) {
			continue;
		}
		if (tile->isBlocking()) {
			continue;
		}
		if (tile->creature) {
			continue;
		}
		if (!m_runtimeSettings.autoCreateSpawn && !HasSpawnCoverage(map, pos, tile)) {
			continue;
		}
		outCandidates.push_back(pos);
	}
	(void)index;
	return true;
}

bool SpawnEngine::passesMinCreatureDistance(const Position& pos,
                                            const std::vector<Position>& currentPlanned) const {
	if (m_runtimeSettings.minCreatureDistance <= 0) {
		return true;
	}

	for (const Position& other : currentPlanned) {
		if (other.z != pos.z) {
			continue;
		}
		if (ChebyshevDistance(other, pos) < m_runtimeSettings.minCreatureDistance) {
			return false;
		}
	}
	return true;
}

bool SpawnEngine::isWalkableForType(const Position& pos, const CreatureType* type,
                                    const std::vector<Position>& currentPlanned) const {
	if (!m_editor || !type) {
		return false;
	}

	Map& map = m_editor->getMap();
	Tile* tile = map.getTile(pos);
	if (!tile || !tile->ground) {
		return false;
	}
	if (tile->isBlocking()) {
		return false;
	}
	if (tile->creature) {
		return false;
	}

	for (const Position& planned : currentPlanned) {
		if (planned == pos) {
			return false;
		}
	}

	if (tile->isPZ() && !type->isNpc) {
		return false;
	}
	if (!m_runtimeSettings.autoCreateSpawn && !HasSpawnCoverage(map, pos, tile)) {
		return false;
	}
	return true;
}

bool SpawnEngine::passesAvailabilityCheck(const Position& pos, const CreatureType* type,
                                          const std::vector<Position>& currentPlanned) const {
	const int range = std::max(0, m_runtimeSettings.availabilityRange);
	if (range <= 0) {
		return true;
	}

	int walkable = 0;
	for (int y = -range; y <= range; ++y) {
		for (int x = -range; x <= range; ++x) {
			Position test(pos.x + x, pos.y + y, pos.z);
			if (ChebyshevDistance(pos, test) > range) {
				continue;
			}
			if (isWalkableForType(test, type, currentPlanned)) {
				walkable++;
			}
		}
	}
	if (walkable < m_runtimeSettings.minWalkableTilesInRange) {
		return false;
	}

	const int requiredEscape = std::max(0, m_runtimeSettings.bfsEscapeDistance);
	if (requiredEscape == 0) {
		return true;
	}

	if (!isWalkableForType(pos, type, currentPlanned)) {
		return false;
	}

	std::queue<Position> q;
	std::unordered_set<uint64_t> visited;
	q.push(pos);
	visited.insert(positionHash(pos));
	int maxDistance = 0;

	while (!q.empty()) {
		Position current = q.front();
		q.pop();

		const int distance = ChebyshevDistance(pos, current);
		maxDistance = std::max(maxDistance, distance);
		if (maxDistance >= requiredEscape) {
			return true;
		}

		if (distance >= range) {
			continue;
		}

		const Position neighbors[4] = {
			Position(current.x + 1, current.y, current.z),
			Position(current.x - 1, current.y, current.z),
			Position(current.x, current.y + 1, current.z),
			Position(current.x, current.y - 1, current.z),
		};

		for (const Position& next : neighbors) {
			if (ChebyshevDistance(pos, next) > range) {
				continue;
			}

			const uint64_t hash = positionHash(next);
			if (visited.find(hash) != visited.end()) {
				continue;
			}
			if (!isWalkableForType(next, type, currentPlanned)) {
				continue;
			}

			visited.insert(hash);
			q.push(next);
		}
	}

	return maxDistance >= requiredEscape;
}

bool SpawnEngine::canPlaceCreatureAt(const Position& pos, const CreatureType* type,
                                     const std::vector<Position>& currentPlanned) const {
	if (!isWalkableForType(pos, type, currentPlanned)) {
		return false;
	}
	if (!passesMinCreatureDistance(pos, currentPlanned)) {
		return false;
	}
	if (!passesAvailabilityCheck(pos, type, currentPlanned)) {
		return false;
	}
	return true;
}

bool SpawnEngine::tryPlaceCreature(const CreatureType* type, int spawnTime, const Position& center,
                                   int spreadRadius, const std::vector<Position>& candidates,
                                   const std::vector<Position>& currentPlanned,
                                   PreviewCreature& outCreature,
                                   const ProgressCallback& progress) {
	if (!type || candidates.empty()) {
		return false;
	}

	std::vector<const Position*> nearby;
	std::vector<size_t> nearbyIndices;
	getNearbyCandidateIndices(center, std::max(0, spreadRadius), nearbyIndices);
	nearby.reserve(nearbyIndices.size());
	size_t progressTick = 0;
	for (size_t idx : nearbyIndices) {
		if (!tickProgress(progress, "Searching valid tiles for creatures...", progressTick, 4096)) {
			return false;
		}
		if (idx >= candidates.size()) {
			continue;
		}
		const Position& candidate = candidates[idx];
		if (!canPlaceCreatureAt(candidate, type, currentPlanned)) {
			continue;
		}
		if (ChebyshevDistance(candidate, center) <= spreadRadius) {
			nearby.push_back(&candidate);
		}
	}

	if (!nearby.empty()) {
		std::uniform_int_distribution<size_t> dist(0, nearby.size() - 1);
		const Position& selected = *nearby[dist(m_rng)];
		outCreature.position = selected;
		outCreature.creatureName = type->name;
		outCreature.spawnTime = spawnTime;
		return true;
	}

	auto floorIt = m_candidateFloorIndices.find(center.z);
	if (floorIt == m_candidateFloorIndices.end() || floorIt->second.empty()) {
		return false;
	}

	const std::vector<size_t>& floorIndices = floorIt->second;
	std::uniform_int_distribution<size_t> randomIndex(0, floorIndices.size() - 1);
	const size_t randomAttempts = std::min<size_t>(512, floorIndices.size());
	for (size_t attempt = 0; attempt < randomAttempts; ++attempt) {
		if (!tickProgress(progress, "Searching fallback tiles...", progressTick, 1024)) {
			return false;
		}
		size_t idx = floorIndices[randomIndex(m_rng)];
		if (idx >= candidates.size()) {
			continue;
		}
		const Position& candidate = candidates[idx];
		if (!canPlaceCreatureAt(candidate, type, currentPlanned)) {
			continue;
		}
		outCreature.position = candidate;
		outCreature.creatureName = type->name;
		outCreature.spawnTime = spawnTime;
		return true;
	}

	for (size_t idx : floorIndices) {
		if (!tickProgress(progress, "Searching fallback tiles...", progressTick, 2048)) {
			return false;
		}
		if (idx >= candidates.size()) {
			continue;
		}
		const Position& candidate = candidates[idx];
		if (!canPlaceCreatureAt(candidate, type, currentPlanned)) {
			continue;
		}
		outCreature.position = candidate;
		outCreature.creatureName = type->name;
		outCreature.spawnTime = spawnTime;
		return true;
	}

	return false;
}

bool SpawnEngine::tryPlaceGroupInstance(const CreatureGroup& group, const Position& center,
                                        const std::vector<Position>& candidates,
                                        const std::vector<Position>& currentPlanned,
                                        std::vector<PreviewCreature>& outPlaced,
                                        const ProgressCallback& progress) {
	outPlaced.clear();
	std::vector<Position> localPlanned = currentPlanned;

	for (const CreatureEntry& creature : group.creatures) {
		CreatureType* type = g_creatures[creature.name];
		if (!type) {
			return false;
		}

		const int spawnTime = creature.spawnTime >= 0 ? creature.spawnTime : m_runtimeSettings.defaultSpawnTime;
		for (int i = 0; i < creature.count; ++i) {
			PreviewCreature placed;
			if (!tryPlaceCreature(type, spawnTime, center, group.spreadRadius, candidates, localPlanned, placed, progress)) {
				return false;
			}
			placed.sourceGroupName = group.name;
			outPlaced.push_back(placed);
			localPlanned.push_back(placed.position);
		}
	}

	return !outPlaced.empty();
}

int SpawnEngine::estimateAutoInstances(const CreatureGroup& group, const std::vector<Position>& candidates) const {
	if (candidates.empty()) {
		return 0;
	}

	const int creaturesPerInstance = std::max(1, group.getTotalCreatures());
	long long byTiles = static_cast<long long>(candidates.size()) / creaturesPerInstance;
	if (byTiles <= 0) {
		return 0;
	}

	AreaDefinition normalizedArea = m_area;
	normalizedArea.normalize();
	const long long width = static_cast<long long>(std::abs(normalizedArea.toPos.x - normalizedArea.fromPos.x)) + 1;
	const long long height = static_cast<long long>(std::abs(normalizedArea.toPos.y - normalizedArea.fromPos.y)) + 1;
	const long long floors = static_cast<long long>(std::abs(normalizedArea.toPos.z - normalizedArea.fromPos.z)) + 1;

	long long bySpacing = std::numeric_limits<long long>::max();
	const int spacing = std::max(0, group.minGroupDistance);
	if (spacing > 0) {
		const long long cellsX = (width + spacing - 1) / spacing;
		const long long cellsY = (height + spacing - 1) / spacing;
		bySpacing = cellsX * cellsY * floors;
	}

	long long result = std::min(byTiles, bySpacing);
	result = std::max<long long>(1, result);
	result = std::min<long long>(result, std::numeric_limits<int>::max());
	return static_cast<int>(result);
}

bool SpawnEngine::placeGroup(const CreatureGroup& group, const std::vector<Position>& candidates,
                             std::vector<Position>& plannedPositions,
                             std::vector<Position>& groupCenters,
                             const ProgressCallback& progress) {
	if (!group.enabled || group.creatures.empty() || candidates.empty()) {
		return false;
	}

	bool anyPlaced = false;
	std::uniform_int_distribution<size_t> centerDist(0, candidates.size() - 1);
	const int maxAttempts = std::max(1, m_runtimeSettings.centerAttempts);
	const int targetInstances =
		group.instanceMode == InstancePlacementMode::AutoBySpacing ?
			estimateAutoInstances(group, candidates) :
			std::max(1, group.instances);
	if (targetInstances <= 0) {
		return false;
	}

	size_t progressTick = 0;
	for (int instance = 0; instance < targetInstances; ++instance) {
		if (!tickProgress(progress, "Placing group instances...", progressTick, 8)) {
			return false;
		}
		bool instancePlaced = false;

		for (int attempt = 0; attempt < maxAttempts; ++attempt) {
			if (!tickProgress(progress, "Trying candidate centers...", progressTick, 64)) {
				return false;
			}
			const Position& center = candidates[centerDist(m_rng)];

			bool tooClose = false;
			for (const Position& existingCenter : groupCenters) {
				if (existingCenter.z != center.z) {
					continue;
				}
				if (ChebyshevDistance(existingCenter, center) < group.minGroupDistance) {
					tooClose = true;
					break;
				}
			}
			if (tooClose) {
				continue;
			}

			std::vector<PreviewCreature> placedInInstance;
			if (!tryPlaceGroupInstance(group, center, candidates, plannedPositions, placedInInstance, progress)) {
				continue;
			}

			const uint64_t instanceId = ++m_nextPreviewInstanceId;
			PreviewSpawnInstance spawnInstance;
			spawnInstance.instanceId = instanceId;
			spawnInstance.center = center;
			spawnInstance.radius = std::max(1, group.spreadRadius);
			spawnInstance.sourceGroupName = group.name;
			m_preview.spawnInstances.push_back(spawnInstance);

			for (auto& placed : placedInInstance) {
				placed.instanceId = instanceId;
				placed.spawnCenter = center;
				placed.spawnRadius = spawnInstance.radius;
				m_preview.creatures.push_back(placed);
				m_preview.creatureCounts[as_lower_str(placed.creatureName)]++;
				plannedPositions.push_back(placed.position);
			}

			groupCenters.push_back(center);
			instancePlaced = true;
			anyPlaced = true;
			break;
		}

		if (!instancePlaced) {
			break;
		}
	}

	return anyPlaced;
}

bool SpawnEngine::generatePreview(uint64_t seed, const ProgressCallback& progress) {
	clearPreview();
	m_nextPreviewInstanceId = 0;
	m_operationCancelled = false;

	if (!reportProgress(progress, "Starting preview generation...")) {
		m_lastError = "Operation cancelled by user";
		m_preview.errorMessage = m_lastError;
		return false;
	}

	if (!m_editor) {
		m_lastError = "No editor available";
		m_preview.errorMessage = m_lastError;
		return false;
	}

	if (seed == 0) {
		if (m_preset.defaultSeed != 0) {
			seed = m_preset.defaultSeed;
		} else {
			std::random_device rd;
			seed = rd();
		}
	}

	m_currentSeed = seed;
	m_rng.seed(static_cast<unsigned int>(seed));
	m_preview.seed = seed;

	std::string validationError;
	if (!m_preset.validate(validationError)) {
		m_lastError = "Invalid preset: " + validationError;
		m_preview.errorMessage = m_lastError;
		return false;
	}
	m_runtimeSettings = BuildEffectiveSettings(m_preset.settings);
	m_candidateChunkSize = CandidateChunkSizeForProfile(m_runtimeSettings.processingProfile);

	std::vector<Position> candidates;
	if (!collectCandidateTiles(candidates, progress)) {
		m_lastError = m_operationCancelled ? "Operation cancelled by user" : "Failed to collect candidate tiles";
		m_preview.errorMessage = m_lastError;
		return false;
	}
	if (candidates.empty()) {
		m_lastError = "No candidate tiles available in selected area";
		m_preview.errorMessage = m_lastError;
		return false;
	}

	std::shuffle(candidates.begin(), candidates.end(), m_rng);
	buildCandidateIndex(candidates);

	std::vector<Position> plannedPositions;
	std::vector<Position> groupCenters;
	bool anyPlaced = false;
	size_t groupCounter = 0;
	for (const CreatureGroup& group : m_preset.groups) {
		if (!group.enabled) {
			continue;
		}
		++groupCounter;
		if (!reportProgress(progress, ("Placing group " + std::to_string(groupCounter) + "...").c_str())) {
			m_lastError = "Operation cancelled by user";
			m_preview.errorMessage = m_lastError;
			return false;
		}
		if (placeGroup(group, candidates, plannedPositions, groupCenters, progress)) {
			anyPlaced = true;
		} else if (m_operationCancelled) {
			m_lastError = "Operation cancelled by user";
			m_preview.errorMessage = m_lastError;
			return false;
		}
	}

	if (!anyPlaced) {
		m_lastError = "No creatures could be placed with current constraints";
		m_preview.errorMessage = m_lastError;
		clearCandidateIndex();
		return false;
	}

	m_preview.isValid = true;
	clearCandidateIndex();
	return true;
}

bool SpawnEngine::rerollPreview(const ProgressCallback& progress) {
	return generatePreview(0, progress);
}

void SpawnEngine::clearPreview() {
	m_preview.clear();
	clearCandidateIndex();
}

bool SpawnEngine::applyPreview(const ProgressCallback& progress) {
	if (!m_preview.isValid || m_preview.creatures.empty()) {
		m_lastError = "No valid preview to apply";
		return false;
	}
	if (!m_editor) {
		m_lastError = "No editor available";
		return false;
	}
	m_operationCancelled = false;
	if (!reportProgress(progress, "Applying creatures to map...")) {
		m_lastError = "Operation cancelled by user";
		return false;
	}

	BatchAction* batch = m_editor->createBatch(ACTION_DRAW);
	Action* action = m_editor->createAction(batch);
	Map& map = m_editor->getMap();

	std::unordered_set<uint64_t> touched;
	std::unordered_map<uint64_t, int> instanceCoverageRadius;
	std::unordered_map<uint64_t, PreviewSpawnInstance> instanceById;
	struct PendingSpawnCreation {
		uint64_t instanceId = 0;
		Tile* tile = nullptr;
		AppliedSpawn spawn;
	};
	std::vector<PendingSpawnCreation> pendingSpawnCreations;
	std::unordered_set<uint64_t> appliedInstanceIds;
	std::vector<AppliedCreature> applied;
	std::vector<AppliedSpawn> createdSpawns;
	applied.reserve(m_preview.creatures.size());
	pendingSpawnCreations.reserve(m_preview.spawnInstances.size());
	appliedInstanceIds.reserve(m_preview.spawnInstances.size());
	createdSpawns.reserve(m_preview.spawnInstances.size());
	instanceById.reserve(m_preview.spawnInstances.size());
	instanceCoverageRadius.reserve(m_preview.spawnInstances.size());
	auto freePendingSpawnTiles = [&map, &pendingSpawnCreations]() {
		for (PendingSpawnCreation& pending : pendingSpawnCreations) {
			if (pending.tile) {
				map.allocator.freeTile(pending.tile);
				pending.tile = nullptr;
			}
		}
	};

	size_t progressTick = 0;
	for (const PreviewSpawnInstance& instance : m_preview.spawnInstances) {
		if (!tickProgress(progress, "Creating spawn centers...", progressTick, 128)) {
			m_lastError = "Operation cancelled by user";
			delete action;
			delete batch;
			return false;
		}
		instanceById[instance.instanceId] = instance;
	}

	for (const PreviewSpawnInstance& instance : m_preview.spawnInstances) {
		if (!tickProgress(progress, "Preparing spawn coverage...", progressTick, 128)) {
			m_lastError = "Operation cancelled by user";
			freePendingSpawnTiles();
			delete action;
			delete batch;
			return false;
		}
		Tile* centerTile = map.getTile(instance.center);
		if (!centerTile || !centerTile->ground || centerTile->isBlocking()) {
			continue;
		}

		if (centerTile->spawn) {
			instanceCoverageRadius[instance.instanceId] = std::max(1, centerTile->spawn->getSize());
			continue;
		}
		if (!m_preset.settings.autoCreateSpawn) {
			continue;
		}

		Tile* newCenterTile = centerTile->deepCopy(map);
		if (!newCenterTile || newCenterTile->spawn) {
			if (newCenterTile) {
				map.allocator.freeTile(newCenterTile);
			}
			continue;
		}

		newCenterTile->spawn = newd Spawn(std::max(1, instance.radius));
		instanceCoverageRadius[instance.instanceId] = std::max(1, instance.radius);

		PendingSpawnCreation pending;
		pending.instanceId = instance.instanceId;
		pending.tile = newCenterTile;
		pending.spawn.center = instance.center;
		pending.spawn.radius = std::max(1, instance.radius);
		pendingSpawnCreations.push_back(pending);
	}

	for (const PreviewCreature& previewCreature : m_preview.creatures) {
		if (!tickProgress(progress, "Placing creatures...", progressTick, 256)) {
			m_lastError = "Operation cancelled by user";
			freePendingSpawnTiles();
			delete action;
			delete batch;
			return false;
		}
		const uint64_t hash = positionHash(previewCreature.position);
		if (touched.find(hash) != touched.end()) {
			continue;
		}
		touched.insert(hash);

		Tile* tile = map.getTile(previewCreature.position);
		if (!tile || !tile->ground) {
			continue;
		}

		CreatureType* type = g_creatures[previewCreature.creatureName];
		if (!type) {
			continue;
		}

		Tile* newTile = tile->deepCopy(map);
		if (!newTile || newTile->isBlocking() || newTile->creature) {
			if (newTile) {
				map.allocator.freeTile(newTile);
			}
			continue;
		}
		if (newTile->isPZ() && !type->isNpc) {
			map.allocator.freeTile(newTile);
			continue;
		}

		bool hasSpawnCoverage = HasSpawnCoverage(map, previewCreature.position, tile);
		if (!hasSpawnCoverage) {
			auto instanceIt = instanceById.find(previewCreature.instanceId);
			auto radiusIt = instanceCoverageRadius.find(previewCreature.instanceId);
			if (instanceIt != instanceById.end() && radiusIt != instanceCoverageRadius.end()) {
				const PreviewSpawnInstance& instance = instanceIt->second;
				if (instance.center.z == previewCreature.position.z &&
				    ChebyshevDistance(instance.center, previewCreature.position) <= radiusIt->second) {
					hasSpawnCoverage = true;
				}
			}
		}

		if (!hasSpawnCoverage) {
			map.allocator.freeTile(newTile);
			continue;
		}

		newTile->creature = newd Creature(type);
		newTile->creature->setSpawnTime(std::max(0, previewCreature.spawnTime));
		action->addChange(newd Change(newTile));

		AppliedCreature appliedCreature;
		appliedCreature.position = previewCreature.position;
		appliedCreature.creatureName = previewCreature.creatureName;
		appliedCreature.spawnTime = previewCreature.spawnTime;
		appliedCreature.instanceId = previewCreature.instanceId;
		applied.push_back(appliedCreature);
		appliedInstanceIds.insert(previewCreature.instanceId);
	}

	for (PendingSpawnCreation& pending : pendingSpawnCreations) {
		if (!pending.tile) {
			continue;
		}
		if (appliedInstanceIds.find(pending.instanceId) != appliedInstanceIds.end()) {
			action->addChange(newd Change(pending.tile));
			createdSpawns.push_back(pending.spawn);
			pending.tile = nullptr;
		}
	}

	if (applied.empty()) {
		freePendingSpawnTiles();
		delete action;
		delete batch;
		m_lastError = "No creatures were applied";
		return false;
	}

	freePendingSpawnTiles();

	batch->addAndCommitAction(action);
	m_editor->addBatch(batch);
	m_editor->updateActions();

	m_lastApplied = applied;
	m_lastCreatedSpawns = createdSpawns;
	clearPreview();
	return true;
}

bool SpawnEngine::removeLastApplied() {
	if (!m_editor) {
		m_lastError = "No editor available";
		return false;
	}
	if (m_lastApplied.empty() && m_lastCreatedSpawns.empty()) {
		m_lastError = "No previous apply to remove";
		return false;
	}

	Map& map = m_editor->getMap();
	BatchAction* batch = m_editor->createBatch(ACTION_DRAW);
	Action* action = m_editor->createAction(batch);
	bool anyChanged = false;
	std::unordered_map<uint64_t, std::string> pendingCreatureRemovals;
	pendingCreatureRemovals.reserve(m_lastApplied.size());
	for (const AppliedCreature& applied : m_lastApplied) {
		pendingCreatureRemovals[positionHash(applied.position)] = as_lower_str(applied.creatureName);
	}

	for (const AppliedCreature& applied : m_lastApplied) {
		Tile* tile = map.getTile(applied.position);
		if (!tile) {
			continue;
		}

		Tile* newTile = tile->deepCopy(map);
		bool changed = false;

		if (newTile->creature && as_lower_str(newTile->creature->getName()) == as_lower_str(applied.creatureName)) {
			delete newTile->creature;
			newTile->creature = nullptr;
			changed = true;
		}

		if (changed) {
			action->addChange(newd Change(newTile));
			anyChanged = true;
		} else {
			map.allocator.freeTile(newTile);
		}
	}

	std::unordered_set<uint64_t> processedSpawns;
	for (const AppliedSpawn& appliedSpawn : m_lastCreatedSpawns) {
		const uint64_t spawnHash = positionHash(appliedSpawn.center);
		if (processedSpawns.find(spawnHash) != processedSpawns.end()) {
			continue;
		}
		processedSpawns.insert(spawnHash);

		Tile* tile = map.getTile(appliedSpawn.center);
		if (!tile || !tile->spawn) {
			continue;
		}

		Tile* newTile = tile->deepCopy(map);
		if (!newTile || !newTile->spawn) {
			if (newTile) {
				map.allocator.freeTile(newTile);
			}
			continue;
		}

		bool hasOtherCreaturesInSpawn = false;
		const int radius = newTile->spawn->getSize();
		for (int y = appliedSpawn.center.y - radius; y <= appliedSpawn.center.y + radius && !hasOtherCreaturesInSpawn; ++y) {
			for (int x = appliedSpawn.center.x - radius; x <= appliedSpawn.center.x + radius; ++x) {
				Tile* checkTile = map.getTile(x, y, appliedSpawn.center.z);
				if (!checkTile || !checkTile->creature) {
					continue;
				}
				const Position& checkPos = checkTile->getPosition();
				const uint64_t checkHash = positionHash(checkPos);
				auto pendingIt = pendingCreatureRemovals.find(checkHash);
				if (pendingIt != pendingCreatureRemovals.end() &&
				    as_lower_str(checkTile->creature->getName()) == pendingIt->second) {
					// This creature is being removed in the same batch.
					continue;
				}
				hasOtherCreaturesInSpawn = true;
				break;
			}
		}

		if (!hasOtherCreaturesInSpawn) {
			delete newTile->spawn;
			newTile->spawn = nullptr;
			action->addChange(newd Change(newTile));
			anyChanged = true;
		} else {
			map.allocator.freeTile(newTile);
		}
	}

	if (!anyChanged) {
		delete action;
		delete batch;
		m_lastError = "No applied creatures were found to remove";
		return false;
	}

	batch->addAndCommitAction(action);
	m_editor->addBatch(batch);
	m_editor->updateActions();
	m_lastApplied.clear();
	m_lastCreatedSpawns.clear();
	return true;
}

} // namespace AreaCreatureSpawn
