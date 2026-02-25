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

#ifndef RME_SPAWN_DENSITY_DIALOG_H_
#define RME_SPAWN_DENSITY_DIALOG_H_

#include "main.h"
#include "position.h"
#include <vector>
#include <string>

class Editor;
class Map;
class PositionCtrl;

struct SpawnCreatureInfo {
	Position creaturePos;
	Position spawnPos;
	std::string creatureName;
	int spawnTime;
	int spawnRadius;
};

struct CreatureTypeGroup {
	std::string typeName;
	int count;
	std::vector<SpawnCreatureInfo> creatures;
};

struct AdjustmentPreviewEntry {
	std::string typeName;
	int currentCount;
	int targetCount;
	int delta; // positive=add, negative=remove
};

class SpawnDensityDialog : public wxDialog
{
public:
	SpawnDensityDialog(wxWindow* parent);
	~SpawnDensityDialog();

private:
	// UI widgets
	PositionCtrl* m_fromPos;
	PositionCtrl* m_toPos;
	wxButton* m_scanButton;
	wxCheckListBox* m_creatureList;
	wxButton* m_selectAllButton;
	wxButton* m_deselectAllButton;
	wxRadioButton* m_radioReduce;
	wxRadioButton* m_radioIncrease;
	wxRadioButton* m_radioPercentage;
	wxRadioButton* m_radioFixedNumber;
	wxSpinCtrl* m_valueSpin;
	wxStaticText* m_percentLabel;
	wxButton* m_previewButton;
	wxTextCtrl* m_previewText;
	wxButton* m_executeButton;
	wxButton* m_closeButton;
	wxGauge* m_progress;

	// Internal data
	std::vector<CreatureTypeGroup> m_groups;
	std::vector<AdjustmentPreviewEntry> m_preview;

	// Event handlers
	void OnScan(wxCommandEvent& event);
	void OnSelectAll(wxCommandEvent& event);
	void OnDeselectAll(wxCommandEvent& event);
	void OnPreview(wxCommandEvent& event);
	void OnExecute(wxCommandEvent& event);
	void OnClose(wxCommandEvent& event);
	void OnModeChanged(wxCommandEvent& event);

	// Core algorithms
	void ScanRegion();
	void ComputePreview();
	void ExecuteAdjustment();

	// Helpers
	Editor* GetEditor() const;
	void UpdateWidgets();
	Position FindOwningSpawn(Map& map, const Position& creaturePos) const;
};

#endif
