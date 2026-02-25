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
#include "spawn_density_dialog.h"
#include "gui.h"
#include "editor.h"
#include "map.h"
#include "tile.h"
#include "creature.h"
#include "creatures.h"
#include "spawn.h"
#include "action.h"
#include "positionctrl.h"
#include "theme.h"
#include "settings.h"

#include <wx/spinctrl.h>
#include <wx/checklst.h>
#include <random>
#include <algorithm>
#include <sstream>
#include <set>
#include <cmath>

namespace {

inline int ChebyshevDistance(const Position& a, const Position& b) {
	return std::max(std::abs(a.x - b.x), std::abs(a.y - b.y));
}

} // namespace

// ============================================================================
// SpawnDensityDialog
// ============================================================================

SpawnDensityDialog::SpawnDensityDialog(wxWindow* parent) :
	wxDialog(parent, wxID_ANY, "Spawn Density Adjuster", wxDefaultPosition, wxSize(560, 700),
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	const ThemeColors& theme = Theme::Dark();

	wxBoxSizer* topSizer = newd wxBoxSizer(wxVERTICAL);

	// --- Position controls ---
	wxBoxSizer* posSizer = newd wxBoxSizer(wxHORIZONTAL);
	m_fromPos = newd PositionCtrl(this, "From Position", 0, 0, 7);
	m_toPos = newd PositionCtrl(this, "To Position", 0, 0, 7);
	posSizer->Add(m_fromPos, 1, wxALL | wxEXPAND, 5);
	posSizer->Add(m_toPos, 1, wxALL | wxEXPAND, 5);
	topSizer->Add(posSizer, 0, wxEXPAND);

	m_scanButton = newd wxButton(this, wxID_ANY, "Scan Region");
	topSizer->Add(m_scanButton, 0, wxALL | wxALIGN_CENTER_HORIZONTAL, 5);

	// --- Creatures found ---
	wxStaticBoxSizer* creatureBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Creatures Found");

	m_creatureList = newd wxCheckListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 160));
	m_creatureList->SetBackgroundColour(theme.surface);
	m_creatureList->SetForegroundColour(theme.text);
	creatureBox->Add(m_creatureList, 1, wxALL | wxEXPAND, 5);

	wxBoxSizer* selBtnSizer = newd wxBoxSizer(wxHORIZONTAL);
	m_selectAllButton = newd wxButton(this, wxID_ANY, "Select All");
	m_deselectAllButton = newd wxButton(this, wxID_ANY, "Deselect All");
	selBtnSizer->Add(m_selectAllButton, 0, wxRIGHT, 5);
	selBtnSizer->Add(m_deselectAllButton, 0);
	creatureBox->Add(selBtnSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

	topSizer->Add(creatureBox, 1, wxALL | wxEXPAND, 5);

	// --- Adjustment settings ---
	wxStaticBoxSizer* adjustBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Adjustment");

	wxBoxSizer* dirSizer = newd wxBoxSizer(wxHORIZONTAL);
	m_radioReduce = newd wxRadioButton(this, wxID_ANY, "Reduce", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	m_radioIncrease = newd wxRadioButton(this, wxID_ANY, "Increase");
	m_radioReduce->SetValue(true);
	dirSizer->Add(m_radioReduce, 0, wxRIGHT, 15);
	dirSizer->Add(m_radioIncrease, 0);
	adjustBox->Add(dirSizer, 0, wxALL, 5);

	wxBoxSizer* modeSizer = newd wxBoxSizer(wxHORIZONTAL);
	m_radioPercentage = newd wxRadioButton(this, wxID_ANY, "By Percentage", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	m_radioFixedNumber = newd wxRadioButton(this, wxID_ANY, "By Fixed Number");
	m_radioPercentage->SetValue(true);
	modeSizer->Add(m_radioPercentage, 0, wxRIGHT, 15);
	modeSizer->Add(m_radioFixedNumber, 0);
	adjustBox->Add(modeSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

	wxBoxSizer* valueSizer = newd wxBoxSizer(wxHORIZONTAL);
	valueSizer->Add(newd wxStaticText(this, wxID_ANY, "Value:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	m_valueSpin = newd wxSpinCtrl(this, wxID_ANY, "50", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 1, 1000, 50);
	valueSizer->Add(m_valueSpin, 0, wxRIGHT, 4);
	m_percentLabel = newd wxStaticText(this, wxID_ANY, "%");
	valueSizer->Add(m_percentLabel, 0, wxALIGN_CENTER_VERTICAL);
	adjustBox->Add(valueSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

	topSizer->Add(adjustBox, 0, wxLEFT | wxRIGHT | wxEXPAND, 5);

	// --- Action buttons ---
	wxBoxSizer* actionSizer = newd wxBoxSizer(wxHORIZONTAL);
	m_previewButton = newd wxButton(this, wxID_ANY, "Preview");
	m_executeButton = newd wxButton(this, wxID_ANY, "Execute");
	m_closeButton = newd wxButton(this, wxID_ANY, "Close");
	m_previewButton->Enable(false);
	m_executeButton->Enable(false);
	actionSizer->Add(m_previewButton, 0, wxRIGHT, 5);
	actionSizer->Add(m_executeButton, 0, wxRIGHT, 5);
	actionSizer->AddStretchSpacer();
	actionSizer->Add(m_closeButton, 0);
	topSizer->Add(actionSizer, 0, wxALL | wxEXPAND, 10);

	// --- Preview text ---
	m_previewText = newd wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(-1, 150),
		wxTE_MULTILINE | wxTE_READONLY);
	m_previewText->SetBackgroundColour(theme.surface);
	m_previewText->SetForegroundColour(theme.text);
	topSizer->Add(m_previewText, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

	// --- Progress bar ---
	m_progress = newd wxGauge(this, wxID_ANY, 100);
	m_progress->SetValue(0);
	topSizer->Add(m_progress, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

	SetSizer(topSizer);
	Layout();
	Centre(wxBOTH);

	// Bind events
	m_scanButton->Bind(wxEVT_BUTTON, &SpawnDensityDialog::OnScan, this);
	m_selectAllButton->Bind(wxEVT_BUTTON, &SpawnDensityDialog::OnSelectAll, this);
	m_deselectAllButton->Bind(wxEVT_BUTTON, &SpawnDensityDialog::OnDeselectAll, this);
	m_previewButton->Bind(wxEVT_BUTTON, &SpawnDensityDialog::OnPreview, this);
	m_executeButton->Bind(wxEVT_BUTTON, &SpawnDensityDialog::OnExecute, this);
	m_closeButton->Bind(wxEVT_BUTTON, &SpawnDensityDialog::OnClose, this);
	m_radioPercentage->Bind(wxEVT_RADIOBUTTON, &SpawnDensityDialog::OnModeChanged, this);
	m_radioFixedNumber->Bind(wxEVT_RADIOBUTTON, &SpawnDensityDialog::OnModeChanged, this);
}

SpawnDensityDialog::~SpawnDensityDialog()
{
	// Events are cleaned up automatically by Bind()
}

Editor* SpawnDensityDialog::GetEditor() const
{
	return g_gui.GetCurrentEditor();
}

void SpawnDensityDialog::UpdateWidgets()
{
	bool hasGroups = !m_groups.empty();
	m_previewButton->Enable(hasGroups);
	m_executeButton->Enable(!m_preview.empty());

	// Show/hide percent label based on mode
	m_percentLabel->Show(m_radioPercentage->GetValue());

	// Adjust spin range based on mode
	if(m_radioPercentage->GetValue()) {
		m_valueSpin->SetRange(1, 1000);
	} else {
		m_valueSpin->SetRange(1, 9999);
	}

	Layout();
}

void SpawnDensityDialog::OnModeChanged(wxCommandEvent& WXUNUSED(event))
{
	UpdateWidgets();
}

void SpawnDensityDialog::OnScan(wxCommandEvent& WXUNUSED(event))
{
	ScanRegion();
}

void SpawnDensityDialog::OnSelectAll(wxCommandEvent& WXUNUSED(event))
{
	for(unsigned int i = 0; i < m_creatureList->GetCount(); ++i) {
		m_creatureList->Check(i, true);
	}
}

void SpawnDensityDialog::OnDeselectAll(wxCommandEvent& WXUNUSED(event))
{
	for(unsigned int i = 0; i < m_creatureList->GetCount(); ++i) {
		m_creatureList->Check(i, false);
	}
}

void SpawnDensityDialog::OnPreview(wxCommandEvent& WXUNUSED(event))
{
	ComputePreview();
}

void SpawnDensityDialog::OnExecute(wxCommandEvent& WXUNUSED(event))
{
	ExecuteAdjustment();
}

void SpawnDensityDialog::OnClose(wxCommandEvent& WXUNUSED(event))
{
	Close();
}

// ============================================================================
// FindOwningSpawn - finds the spawn tile that covers a creature position
// ============================================================================

Position SpawnDensityDialog::FindOwningSpawn(Map& map, const Position& creaturePos) const
{
	// Check if the creature tile itself has a spawn
	Tile* tile = map.getTile(creaturePos);
	if(tile && tile->spawn) {
		return creaturePos;
	}

	// Search outward for spawn tiles that cover this position
	const int maxRadius = std::max(1, g_settings.getInteger(Config::MAX_SPAWN_RADIUS));
	for(int y = creaturePos.y - maxRadius; y <= creaturePos.y + maxRadius; ++y) {
		for(int x = creaturePos.x - maxRadius; x <= creaturePos.x + maxRadius; ++x) {
			const Tile* spawnTile = map.getTile(x, y, creaturePos.z);
			if(!spawnTile || !spawnTile->spawn) {
				continue;
			}
			const int radius = std::max(1, spawnTile->spawn->getSize());
			if(ChebyshevDistance(creaturePos, spawnTile->getPosition()) <= radius) {
				return spawnTile->getPosition();
			}
		}
	}

	// No spawn found - orphan creature
	return creaturePos;
}

// ============================================================================
// ScanRegion
// ============================================================================

void SpawnDensityDialog::ScanRegion()
{
	Editor* editor = GetEditor();
	if(!editor) {
		wxMessageBox("No map is open.", "Spawn Density Adjuster", wxOK | wxICON_WARNING);
		return;
	}

	Map& map = editor->getMap();

	// Get and normalize bounds
	int minX = std::min((int)m_fromPos->GetX(), (int)m_toPos->GetX());
	int maxX = std::max((int)m_fromPos->GetX(), (int)m_toPos->GetX());
	int minY = std::min((int)m_fromPos->GetY(), (int)m_toPos->GetY());
	int maxY = std::max((int)m_fromPos->GetY(), (int)m_toPos->GetY());
	int minZ = std::min((int)m_fromPos->GetZ(), (int)m_toPos->GetZ());
	int maxZ = std::max((int)m_fromPos->GetZ(), (int)m_toPos->GetZ());

	if(minX == 0 && maxX == 0 && minY == 0 && maxY == 0) {
		wxMessageBox("Please set the From and To positions.", "Spawn Density Adjuster", wxOK | wxICON_INFORMATION);
		return;
	}

	m_groups.clear();
	m_preview.clear();
	m_creatureList->Clear();
	m_previewText->Clear();
	m_progress->SetValue(0);

	// Scan all tiles using MapIterator
	std::map<std::string, CreatureTypeGroup> groupMap;

	MapIterator tileIter = map.begin();
	MapIterator end = map.end();
	long long done = 0;
	long long total = map.getTileCount();

	while(tileIter != end) {
		Tile* tile = (*tileIter)->get();
		++tileIter;
		++done;

		if(!tile || !tile->creature) {
			continue;
		}

		int tx = tile->getX();
		int ty = tile->getY();
		int tz = tile->getZ();

		if(tx < minX || tx > maxX || ty < minY || ty > maxY || tz < minZ || tz > maxZ) {
			continue;
		}

		const Position& pos = tile->getPosition();
		Position spawnPos = FindOwningSpawn(map, pos);

		SpawnCreatureInfo info;
		info.creaturePos = pos;
		info.spawnPos = spawnPos;
		info.creatureName = tile->creature->getName();
		info.spawnTime = tile->creature->getSpawnTime();

		Tile* spawnTile = map.getTile(spawnPos);
		info.spawnRadius = (spawnTile && spawnTile->spawn) ? spawnTile->spawn->getSize() : 0;

		auto& group = groupMap[info.creatureName];
		if(group.typeName.empty()) {
			group.typeName = info.creatureName;
			group.count = 0;
		}
		group.count++;
		group.creatures.push_back(info);

		// Update progress periodically
		if(total > 0 && (done % 4096) == 0) {
			int pct = static_cast<int>((done * 100) / total);
			m_progress->SetValue(std::clamp(pct, 0, 100));
			wxYield();
		}
	}

	m_progress->SetValue(100);

	// Convert to sorted vector
	m_groups.clear();
	m_groups.reserve(groupMap.size());
	for(auto& pair : groupMap) {
		m_groups.push_back(std::move(pair.second));
	}
	std::sort(m_groups.begin(), m_groups.end(),
		[](const CreatureTypeGroup& a, const CreatureTypeGroup& b) {
			return a.typeName < b.typeName;
		});

	// Populate checklist
	m_creatureList->Clear();
	for(const auto& group : m_groups) {
		wxString label = wxString::Format("%s (x%d)", wxstr(group.typeName), group.count);
		int idx = m_creatureList->Append(label);
		m_creatureList->Check(idx, true);
	}

	if(m_groups.empty()) {
		m_previewText->SetValue("No creatures found in the specified region.");
	} else {
		int totalCreatures = 0;
		for(const auto& g : m_groups) totalCreatures += g.count;
		m_previewText->SetValue(wxString::Format("Found %d creature(s) of %zu type(s). Configure adjustment and click Preview.",
			totalCreatures, m_groups.size()));
	}

	UpdateWidgets();
}

// ============================================================================
// ComputePreview
// ============================================================================

void SpawnDensityDialog::ComputePreview()
{
	if(m_groups.empty()) {
		return;
	}

	Editor* editor = GetEditor();
	if(!editor) return;

	Map& map = editor->getMap();

	bool isReduce = m_radioReduce->GetValue();
	bool isPercentage = m_radioPercentage->GetValue();
	int value = m_valueSpin->GetValue();

	m_preview.clear();

	std::ostringstream ss;
	ss << "=== Spawn Density Adjustment Preview ===" << std::endl;

	int minX = std::min((int)m_fromPos->GetX(), (int)m_toPos->GetX());
	int maxX = std::max((int)m_fromPos->GetX(), (int)m_toPos->GetX());
	int minY = std::min((int)m_fromPos->GetY(), (int)m_toPos->GetY());
	int maxY = std::max((int)m_fromPos->GetY(), (int)m_toPos->GetY());
	int minZ = std::min((int)m_fromPos->GetZ(), (int)m_toPos->GetZ());
	int maxZ = std::max((int)m_fromPos->GetZ(), (int)m_toPos->GetZ());

	ss << "Region: (" << minX << ", " << minY << ", " << minZ
	   << ") to (" << maxX << ", " << maxY << ", " << maxZ << ")" << std::endl;
	ss << "Mode: " << (isReduce ? "Reduce" : "Increase")
	   << " by " << value << (isPercentage ? "%" : " creatures") << std::endl;
	ss << std::endl;

	int totalDelta = 0;

	for(size_t i = 0; i < m_groups.size(); ++i) {
		if(!m_creatureList->IsChecked(static_cast<unsigned int>(i))) {
			continue;
		}

		const CreatureTypeGroup& group = m_groups[i];
		AdjustmentPreviewEntry entry;
		entry.typeName = group.typeName;
		entry.currentCount = group.count;

		int deltaAbs = 0;
		if(isPercentage) {
			deltaAbs = static_cast<int>(std::round(group.count * value / 100.0));
		} else {
			deltaAbs = value;
		}

		if(isReduce) {
			deltaAbs = std::min(deltaAbs, group.count);
			entry.delta = -deltaAbs;
			entry.targetCount = group.count - deltaAbs;
		} else {
			// For increase, check available empty tiles
			std::set<Position> spawnPositions;
			for(const auto& info : group.creatures) {
				spawnPositions.insert(info.spawnPos);
			}

			int availableEmpty = 0;
			std::set<uint64_t> occupied;

			// Collect all existing creature positions to avoid double-counting
			for(const auto& g : m_groups) {
				for(const auto& c : g.creatures) {
					uint64_t hash = (static_cast<uint64_t>(c.creaturePos.x & 0xFFFF) << 32) |
					                (static_cast<uint64_t>(c.creaturePos.y & 0xFFFF) << 16) |
					                static_cast<uint64_t>(c.creaturePos.z & 0xFFFF);
					occupied.insert(hash);
				}
			}

			for(const Position& spawnPos : spawnPositions) {
				Tile* spawnTile = map.getTile(spawnPos);
				if(!spawnTile || !spawnTile->spawn) continue;
				int radius = spawnTile->spawn->getSize();

				for(int y = spawnPos.y - radius; y <= spawnPos.y + radius; ++y) {
					for(int x = spawnPos.x - radius; x <= spawnPos.x + radius; ++x) {
						Position pos(x, y, spawnPos.z);
						if(ChebyshevDistance(pos, spawnPos) > radius) continue;

						uint64_t hash = (static_cast<uint64_t>(pos.x & 0xFFFF) << 32) |
						                (static_cast<uint64_t>(pos.y & 0xFFFF) << 16) |
						                static_cast<uint64_t>(pos.z & 0xFFFF);
						if(occupied.count(hash)) continue;

						Tile* t = map.getTile(pos);
						if(!t || !t->ground || t->isBlocking() || t->creature) continue;
						availableEmpty++;
					}
				}
			}

			int actualAdd = std::min(deltaAbs, availableEmpty);
			entry.delta = actualAdd;
			entry.targetCount = group.count + actualAdd;

			if(actualAdd < deltaAbs) {
				ss << group.typeName << ": " << group.count << " -> " << entry.targetCount
				   << " (requested +" << deltaAbs << ", only " << availableEmpty << " empty tiles available)" << std::endl;
			} else {
				ss << group.typeName << ": " << group.count << " -> " << entry.targetCount
				   << " (+" << actualAdd << ")" << std::endl;
			}

			m_preview.push_back(entry);
			totalDelta += entry.delta;
			continue;
		}

		ss << group.typeName << ": " << group.count << " -> " << entry.targetCount
		   << " (" << entry.delta << ")" << std::endl;

		m_preview.push_back(entry);
		totalDelta += std::abs(entry.delta);
	}

	ss << std::endl;
	ss << "Total creatures to " << (isReduce ? "remove" : "add") << ": " << totalDelta << std::endl;

	m_previewText->SetValue(wxstr(ss.str()));
	UpdateWidgets();
}

// ============================================================================
// ExecuteAdjustment
// ============================================================================

void SpawnDensityDialog::ExecuteAdjustment()
{
	if(m_preview.empty()) {
		wxMessageBox("Please click Preview first.", "Spawn Density Adjuster", wxOK | wxICON_INFORMATION);
		return;
	}

	Editor* editor = GetEditor();
	if(!editor) return;

	Map& map = editor->getMap();
	bool isReduce = m_radioReduce->GetValue();

	// Disable UI during execution
	m_scanButton->Enable(false);
	m_previewButton->Enable(false);
	m_executeButton->Enable(false);
	m_closeButton->Enable(false);
	m_progress->SetValue(0);

	std::mt19937 rng(std::random_device{}());

	BatchAction* batch = editor->createBatch(ACTION_CHANGE_PROPERTIES);
	Action* action = editor->createAction(batch);

	int totalChanges = 0;
	int doneEntries = 0;
	int totalEntries = static_cast<int>(m_preview.size());

	// Track positions already used for placement (increase mode)
	std::set<uint64_t> usedPositions;

	// Pre-populate used positions with all existing creature tiles
	if(!isReduce) {
		for(const auto& group : m_groups) {
			for(const auto& c : group.creatures) {
				uint64_t hash = (static_cast<uint64_t>(c.creaturePos.x & 0xFFFF) << 32) |
				                (static_cast<uint64_t>(c.creaturePos.y & 0xFFFF) << 16) |
				                static_cast<uint64_t>(c.creaturePos.z & 0xFFFF);
				usedPositions.insert(hash);
			}
		}
	}

	for(const AdjustmentPreviewEntry& entry : m_preview) {
		// Find the matching group
		const CreatureTypeGroup* group = nullptr;
		for(const auto& g : m_groups) {
			if(g.typeName == entry.typeName) {
				group = &g;
				break;
			}
		}
		if(!group) continue;

		if(isReduce && entry.delta < 0) {
			int removeCount = std::abs(entry.delta);

			// Shuffle creatures and pick which to remove
			std::vector<SpawnCreatureInfo> candidates = group->creatures;
			std::shuffle(candidates.begin(), candidates.end(), rng);

			int removed = 0;
			for(const auto& info : candidates) {
				if(removed >= removeCount) break;

				Tile* tile = map.getTile(info.creaturePos);
				if(!tile || !tile->creature) continue;

				Tile* newTile = tile->deepCopy(map);
				delete newTile->creature;
				newTile->creature = nullptr;
				action->addChange(newd Change(newTile));
				removed++;
				totalChanges++;
			}
		} else if(!isReduce && entry.delta > 0) {
			int addCount = entry.delta;

			// Collect unique spawn positions for this creature type
			std::set<Position> spawnPositions;
			for(const auto& info : group->creatures) {
				spawnPositions.insert(info.spawnPos);
			}

			// Find empty tiles within spawn radii
			std::vector<Position> emptyTiles;
			for(const Position& spawnPos : spawnPositions) {
				Tile* spawnTile = map.getTile(spawnPos);
				if(!spawnTile || !spawnTile->spawn) continue;
				int radius = spawnTile->spawn->getSize();

				for(int y = spawnPos.y - radius; y <= spawnPos.y + radius; ++y) {
					for(int x = spawnPos.x - radius; x <= spawnPos.x + radius; ++x) {
						Position pos(x, y, spawnPos.z);
						if(ChebyshevDistance(pos, spawnPos) > radius) continue;

						uint64_t hash = (static_cast<uint64_t>(pos.x & 0xFFFF) << 32) |
						                (static_cast<uint64_t>(pos.y & 0xFFFF) << 16) |
						                static_cast<uint64_t>(pos.z & 0xFFFF);
						if(usedPositions.count(hash)) continue;

						Tile* t = map.getTile(pos);
						if(!t || !t->ground || t->isBlocking() || t->creature) continue;

						emptyTiles.push_back(pos);
					}
				}
			}

			std::shuffle(emptyTiles.begin(), emptyTiles.end(), rng);

			// Use spawntime from existing creatures of the same type
			int spawnTime = group->creatures.empty() ? 60 : group->creatures[0].spawnTime;

			int placed = 0;
			for(const Position& pos : emptyTiles) {
				if(placed >= addCount) break;

				uint64_t hash = (static_cast<uint64_t>(pos.x & 0xFFFF) << 32) |
				                (static_cast<uint64_t>(pos.y & 0xFFFF) << 16) |
				                static_cast<uint64_t>(pos.z & 0xFFFF);
				if(usedPositions.count(hash)) continue;

				Tile* tile = map.getTile(pos);
				if(!tile || tile->creature) continue;

				CreatureType* ctype = g_creatures[group->typeName];
				if(!ctype) continue;

				Tile* newTile = tile->deepCopy(map);
				newTile->creature = newd Creature(ctype);
				newTile->creature->setSpawnTime(spawnTime);
				action->addChange(newd Change(newTile));
				usedPositions.insert(hash);
				placed++;
				totalChanges++;
			}
		}

		doneEntries++;
		int pct = totalEntries > 0 ? static_cast<int>((doneEntries * 100) / totalEntries) : 100;
		m_progress->SetValue(std::clamp(pct, 0, 100));
	}

	if(totalChanges > 0) {
		batch->addAndCommitAction(action);
		editor->addBatch(batch);
		editor->updateActions();

		// Refresh the map view
		g_gui.RefreshView();
	} else {
		delete action;
		delete batch;
	}

	m_progress->SetValue(100);

	// Append result to preview text
	wxString result = m_previewText->GetValue();
	result += wxString::Format("\n\n--- Execution Complete ---\nTotal tiles modified: %d\n", totalChanges);
	m_previewText->SetValue(result);

	// Re-enable UI
	m_scanButton->Enable(true);
	m_previewButton->Enable(!m_groups.empty());
	m_executeButton->Enable(false); // Require new preview after execution
	m_closeButton->Enable(true);
	m_preview.clear();
}
