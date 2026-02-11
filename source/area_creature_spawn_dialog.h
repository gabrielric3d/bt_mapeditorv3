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

#ifndef RME_AREA_CREATURE_SPAWN_DIALOG_H
#define RME_AREA_CREATURE_SPAWN_DIALOG_H

#include <memory>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>

#include "area_creature_spawn.h"

class AreaCreatureSpawnDialog : public wxDialog {
public:
	explicit AreaCreatureSpawnDialog(wxWindow* parent);
	virtual ~AreaCreatureSpawnDialog() = default;

	void UpdateEngine();

private:
	void CreateControls();
	void CreatePresetControls(wxBoxSizer* mainSizer);
	void CreateAreaTab(wxNotebook* notebook);
	void CreateGroupsTab(wxNotebook* notebook);
	void CreateSettingsTab(wxNotebook* notebook);
	void CreateSeedTab(wxNotebook* notebook);
	void CreateActionControls(wxBoxSizer* mainSizer);

	void BuildAreaFromUI();
	void BuildPresetFromUI();
	void LoadPresetToUI(const AreaCreatureSpawn::SpawnPreset& preset);
	void UpdatePresetList();
	void UpdateGroupsList();
	void UpdateAreaInfoText();
	void UpdateStatsText();

	void OnPickArea(wxCommandEvent& event);
	void OnUseSelection(wxCommandEvent& event);
	void OnAreaCoordsChanged(wxSpinEvent& event);

	void OnAddGroup(wxCommandEvent& event);
	void OnEditGroup(wxCommandEvent& event);
	void OnRemoveGroup(wxCommandEvent& event);
	void OnGroupDoubleClick(wxListEvent& event);
	void OnGroupCheckChanged(wxListEvent& event);

	void OnPreview(wxCommandEvent& event);
	void OnReroll(wxCommandEvent& event);
	void OnApply(wxCommandEvent& event);
	void OnRerollApply(wxCommandEvent& event);
	void OnRemoveLastApply(wxCommandEvent& event);

	void OnPresetSelected(wxCommandEvent& event);
	void OnSavePreset(wxCommandEvent& event);
	void OnRefreshPresets(wxCommandEvent& event);
	void OnDeletePreset(wxCommandEvent& event);
	void OnExportPreset(wxCommandEvent& event);
	void OnImportPreset(wxCommandEvent& event);

	void OnClose(wxCloseEvent& event);

private:
	wxChoice* m_presetChoice = nullptr;
	wxTextCtrl* m_presetNameInput = nullptr;

	wxStaticText* m_areaInfoText = nullptr;
	wxStaticText* m_pickStatusText = nullptr;
	wxSpinCtrl* m_fromXSpin = nullptr;
	wxSpinCtrl* m_fromYSpin = nullptr;
	wxSpinCtrl* m_fromZSpin = nullptr;
	wxSpinCtrl* m_toXSpin = nullptr;
	wxSpinCtrl* m_toYSpin = nullptr;
	wxSpinCtrl* m_toZSpin = nullptr;

	wxListCtrl* m_groupsListCtrl = nullptr;

	wxSpinCtrl* m_minCreatureDistanceSpin = nullptr;
	wxSpinCtrl* m_availabilityRangeSpin = nullptr;
	wxSpinCtrl* m_minWalkableSpin = nullptr;
	wxSpinCtrl* m_escapeDistanceSpin = nullptr;
	wxSpinCtrl* m_centerAttemptsSpin = nullptr;
	wxChoice* m_processingProfileChoice = nullptr;
	wxCheckBox* m_autoCreateSpawnCheck = nullptr;
	wxSpinCtrl* m_defaultSpawnTimeSpin = nullptr;

	wxCheckBox* m_useSeedCheck = nullptr;
	wxTextCtrl* m_seedInput = nullptr;

	wxStaticText* m_statsText = nullptr;
	wxButton* m_applyBtn = nullptr;
	wxButton* m_removeLastApplyBtn = nullptr;

	std::unique_ptr<AreaCreatureSpawn::SpawnEngine> m_engine;
	AreaCreatureSpawn::AreaDefinition m_area;
	AreaCreatureSpawn::SpawnPreset m_preset;

	wxDECLARE_EVENT_TABLE();
};

#endif // RME_AREA_CREATURE_SPAWN_DIALOG_H
