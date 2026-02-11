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

#ifndef RME_AREA_CREATURE_SPAWN_H
#define RME_AREA_CREATURE_SPAWN_H

#include "position.h"

#include <map>
#include <random>
#include <string>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class Editor;
class Map;
class Tile;
class CreatureType;

namespace AreaCreatureSpawn {

using ProgressCallback = std::function<bool(const std::string& message)>;

enum class InstancePlacementMode {
	Manual = 0,
	AutoBySpacing = 1,
};

enum class ProcessingProfile {
	LowEnd = 0,
	Balanced = 1,
	Quality = 2,
};

struct CreatureEntry {
	std::string name;
	int count = 1;
	int spawnTime = -1;
};

struct CreatureGroup {
	std::string name;
	std::vector<CreatureEntry> creatures;
	int instances = 1;
	InstancePlacementMode instanceMode = InstancePlacementMode::Manual;
	int spreadRadius = 6;
	int minGroupDistance = 8;
	bool enabled = true;

	int getTotalCreatures() const;
	bool validate(std::string& errorOut) const;
};

struct SpawnSettings {
	int minCreatureDistance = 1;
	int availabilityRange = 6;
	int minWalkableTilesInRange = 12;
	int bfsEscapeDistance = 4;
	int centerAttempts = 80;
	bool autoCreateSpawn = true;
	int defaultSpawnTime = 60;
	ProcessingProfile processingProfile = ProcessingProfile::Balanced;
};

struct AreaDefinition {
	Position fromPos;
	Position toPos;

	std::vector<Position> getAllPositions() const;
	void normalize();
};

struct SpawnPreset {
	std::string name;
	AreaDefinition area;
	bool hasArea = false;
	SpawnSettings settings;
	uint64_t defaultSeed = 0;
	std::vector<CreatureGroup> groups;

	bool validate(std::string& errorOut) const;

	bool saveToFile(const std::string& filepath) const;
	bool loadFromFile(const std::string& filepath);
	std::string toXmlString() const;
	bool fromXmlString(const std::string& xml);
};

class PresetManager {
public:
	static PresetManager& getInstance();

	bool loadPresets();
	bool savePresets();

	std::vector<std::string> getPresetNames() const;
	const SpawnPreset* getPreset(const std::string& name) const;
	bool addPreset(const SpawnPreset& preset);
	bool removePreset(const std::string& name);
	bool renamePreset(const std::string& oldName, const std::string& newName);

	std::string getPresetsDirectory() const;

private:
	PresetManager() = default;
	std::map<std::string, SpawnPreset> m_presets;
};

struct PreviewCreature {
	Position position;
	std::string creatureName;
	std::string sourceGroupName;
	int spawnTime = -1;
	uint64_t instanceId = 0;
	Position spawnCenter;
	int spawnRadius = 1;
};

struct PreviewSpawnInstance {
	uint64_t instanceId = 0;
	Position center;
	int radius = 1;
	std::string sourceGroupName;
};

struct AppliedCreature {
	Position position;
	std::string creatureName;
	int spawnTime = -1;
	uint64_t instanceId = 0;
};

struct AppliedSpawn {
	Position center;
	int radius = 1;
};

struct PreviewState {
	std::vector<PreviewCreature> creatures;
	std::vector<PreviewSpawnInstance> spawnInstances;
	std::unordered_map<std::string, int> creatureCounts;
	uint64_t seed = 0;
	bool isValid = false;
	std::string errorMessage;

	void clear();
};

class SpawnEngine {
public:
	explicit SpawnEngine(Editor* editor);
	~SpawnEngine() = default;

	void setArea(const AreaDefinition& area);
	void setPreset(const SpawnPreset& preset);

	bool generatePreview(uint64_t seed = 0, const ProgressCallback& progress = ProgressCallback());
	bool rerollPreview(const ProgressCallback& progress = ProgressCallback());
	const PreviewState& getPreviewState() const { return m_preview; }

	bool applyPreview(const ProgressCallback& progress = ProgressCallback());
	bool removeLastApplied();
	void clearPreview();
	bool hasLastApplied() const { return !m_lastApplied.empty(); }
	size_t getLastAppliedCount() const { return m_lastApplied.size(); }

	const std::string& getLastError() const { return m_lastError; }

private:
	Editor* m_editor;
	AreaDefinition m_area;
	SpawnPreset m_preset;
	PreviewState m_preview;
	std::vector<AppliedCreature> m_lastApplied;
	std::vector<AppliedSpawn> m_lastCreatedSpawns;
	std::string m_lastError;
	std::mt19937 m_rng;
	uint64_t m_currentSeed = 0;
	uint64_t m_nextPreviewInstanceId = 0;
	mutable bool m_operationCancelled = false;

	bool collectCandidateTiles(std::vector<Position>& outCandidates, const ProgressCallback& progress) const;
	int estimateAutoInstances(const CreatureGroup& group, const std::vector<Position>& candidates) const;
	bool placeGroup(const CreatureGroup& group, const std::vector<Position>& candidates,
	                std::vector<Position>& plannedPositions,
	                std::vector<Position>& groupCenters,
	                const ProgressCallback& progress);
	bool tryPlaceGroupInstance(const CreatureGroup& group, const Position& center,
	                           const std::vector<Position>& candidates,
	                           const std::vector<Position>& currentPlanned,
	                           std::vector<PreviewCreature>& outPlaced,
	                           const ProgressCallback& progress);
	bool tryPlaceCreature(const CreatureType* type, int spawnTime, const Position& center,
	                      int spreadRadius, const std::vector<Position>& candidates,
	                      const std::vector<Position>& currentPlanned,
	                      PreviewCreature& outCreature,
	                      const ProgressCallback& progress);

	bool canPlaceCreatureAt(const Position& pos, const CreatureType* type,
	                        const std::vector<Position>& currentPlanned) const;
	bool passesMinCreatureDistance(const Position& pos,
	                               const std::vector<Position>& currentPlanned) const;
	bool passesAvailabilityCheck(const Position& pos, const CreatureType* type,
	                             const std::vector<Position>& currentPlanned) const;
	bool isWalkableForType(const Position& pos, const CreatureType* type,
	                       const std::vector<Position>& currentPlanned) const;
	bool reportProgress(const ProgressCallback& progress, const char* message) const;
	bool tickProgress(const ProgressCallback& progress, const char* message,
	                  size_t& counter, size_t interval) const;
	void buildCandidateIndex(const std::vector<Position>& candidates);
	void clearCandidateIndex();
	void getNearbyCandidateIndices(const Position& center, int spreadRadius, std::vector<size_t>& outIndices) const;
	static uint64_t positionHash(const Position& pos);
	static uint64_t chunkHash(int chunkX, int chunkY, int z);

private:
	std::unordered_map<uint64_t, std::vector<size_t>> m_candidateChunkIndices;
	std::unordered_map<int, std::vector<size_t>> m_candidateFloorIndices;
	int m_candidateChunkSize = 32;
	SpawnSettings m_runtimeSettings;
};

} // namespace AreaCreatureSpawn

#endif // RME_AREA_CREATURE_SPAWN_H
