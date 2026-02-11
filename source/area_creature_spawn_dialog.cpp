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
#include "area_creature_spawn_dialog.h"

#include "creatures.h"
#include "editor.h"
#include "gui.h"
#include "selection.h"
#include "tile.h"

#include <algorithm>
#include <climits>
#include <functional>
#include <wx/progdlg.h>

namespace {

enum {
	ID_PICK_AREA = wxID_HIGHEST + 7400,
	ID_USE_SELECTION,
	ID_FROM_X,
	ID_FROM_Y,
	ID_FROM_Z,
	ID_TO_X,
	ID_TO_Y,
	ID_TO_Z,

	ID_GROUPS_LIST,
	ID_ADD_GROUP,
	ID_EDIT_GROUP,
	ID_REMOVE_GROUP,

	ID_PRESET_CHOICE,
	ID_SAVE_PRESET,
	ID_REFRESH_PRESET,
	ID_DELETE_PRESET,
	ID_EXPORT_PRESET,
	ID_IMPORT_PRESET,

	ID_PREVIEW,
	ID_REROLL,
	ID_APPLY,
	ID_REROLL_APPLY,
	ID_REMOVE_LAST_APPLY,
};

const std::vector<std::string>& GetCachedCreatureNames() {
	static std::vector<std::string> names;
	static bool loaded = false;
	if (!loaded) {
		loaded = true;
		for (auto it = g_creatures.begin(); it != g_creatures.end(); ++it) {
			if (it->second && !it->second->name.empty()) {
				names.push_back(it->second->name);
			}
		}
		std::sort(names.begin(), names.end());
		names.erase(std::unique(names.begin(), names.end()), names.end());
	}
	return names;
}

bool IsUserCancelledError(const std::string& error) {
	return as_lower_str(error).find("cancelled") != std::string::npos;
}

class CreatureGroupEditDialog : public wxDialog {
public:
	using SaveCallback = std::function<void(const AreaCreatureSpawn::CreatureGroup&)>;

	CreatureGroupEditDialog(wxWindow* parent, const AreaCreatureSpawn::CreatureGroup& group,
	                        const AreaCreatureSpawn::AreaDefinition& areaContext,
	                        SaveCallback onSave)
		: wxDialog(parent, wxID_ANY, "Edit Creature Group", wxDefaultPosition, wxSize(700, 560),
		           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
		  m_working(group),
		  m_areaContext(areaContext),
		  m_onSave(std::move(onSave))
	{
		CreateControls();
		LoadData();
		CentreOnParent();
	}

private:
	void CreateControls() {
		wxBoxSizer* mainSizer = newd wxBoxSizer(wxVERTICAL);

		wxFlexGridSizer* topGrid = newd wxFlexGridSizer(2, 5, 8);
		topGrid->AddGrowableCol(1);

		topGrid->Add(newd wxStaticText(this, wxID_ANY, "Name:"), 0, wxALIGN_CENTER_VERTICAL);
		m_nameInput = newd wxTextCtrl(this, wxID_ANY, "");
		topGrid->Add(m_nameInput, 1, wxEXPAND);

		topGrid->Add(newd wxStaticText(this, wxID_ANY, "Enabled:"), 0, wxALIGN_CENTER_VERTICAL);
		m_enabledCheck = newd wxCheckBox(this, wxID_ANY, "Use this group");
		topGrid->Add(m_enabledCheck, 0, wxALIGN_CENTER_VERTICAL);

		topGrid->Add(newd wxStaticText(this, wxID_ANY, "Instance Mode:"), 0, wxALIGN_CENTER_VERTICAL);
		m_instanceModeChoice = newd wxChoice(this, wxID_ANY);
		m_instanceModeChoice->Append("Manual count");
		m_instanceModeChoice->Append("Auto by spacing");
		topGrid->Add(m_instanceModeChoice, 0, wxALIGN_CENTER_VERTICAL);

		topGrid->Add(newd wxStaticText(this, wxID_ANY, "Instances:"), 0, wxALIGN_CENTER_VERTICAL);
		wxBoxSizer* instancesRow = newd wxBoxSizer(wxHORIZONTAL);
		m_instancesSpin = newd wxSpinCtrl(this, wxID_ANY, "1", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 1, 999, 1);
		instancesRow->Add(m_instancesSpin, 0, wxRIGHT, 5);
		m_calculateInstancesBtn = newd wxButton(this, wxID_ANY, "Calculate");
		instancesRow->Add(m_calculateInstancesBtn, 0);
		topGrid->Add(instancesRow, 0, wxALIGN_CENTER_VERTICAL);

		topGrid->Add(newd wxStaticText(this, wxID_ANY, "Spread Radius:"), 0, wxALIGN_CENTER_VERTICAL);
		m_spreadSpin = newd wxSpinCtrl(this, wxID_ANY, "6", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 0, 100, 6);
		topGrid->Add(m_spreadSpin, 0, wxALIGN_CENTER_VERTICAL);

		topGrid->Add(newd wxStaticText(this, wxID_ANY, "Min Group Distance:"), 0, wxALIGN_CENTER_VERTICAL);
		m_minGroupDistanceSpin = newd wxSpinCtrl(this, wxID_ANY, "8", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 0, 100, 8);
		topGrid->Add(m_minGroupDistanceSpin, 0, wxALIGN_CENTER_VERTICAL);

		mainSizer->Add(topGrid, 0, wxALL | wxEXPAND, 10);

		wxStaticBoxSizer* entriesBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Creatures");
		m_entriesList = newd wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 220),
		                                wxLC_REPORT | wxLC_SINGLE_SEL);
		m_entriesList->InsertColumn(0, "Creature", wxLIST_FORMAT_LEFT, 260);
		m_entriesList->InsertColumn(1, "Count", wxLIST_FORMAT_LEFT, 70);
		m_entriesList->InsertColumn(2, "Spawn Time", wxLIST_FORMAT_LEFT, 90);
		entriesBox->Add(m_entriesList, 1, wxALL | wxEXPAND, 5);

		wxFlexGridSizer* entryGrid = newd wxFlexGridSizer(2, 6, 8);
		entryGrid->AddGrowableCol(1);
		entryGrid->AddGrowableCol(3);

		entryGrid->Add(newd wxStaticText(this, wxID_ANY, "Creature:"), 0, wxALIGN_CENTER_VERTICAL);
		m_creatureNameInput = newd wxTextCtrl(this, wxID_ANY, "");
		m_creatureNameInput->SetHint("Type or pick from list");
		entryGrid->Add(m_creatureNameInput, 1, wxEXPAND);

		entryGrid->Add(newd wxStaticText(this, wxID_ANY, "Filter:"), 0, wxALIGN_CENTER_VERTICAL);
		m_creatureFilterInput = newd wxTextCtrl(this, wxID_ANY, "");
		m_creatureFilterInput->SetHint("Filter creatures (prefix first, then contains)");
		entryGrid->Add(m_creatureFilterInput, 1, wxEXPAND);

		entryGrid->Add(newd wxStaticText(this, wxID_ANY, "Count:"), 0, wxALIGN_CENTER_VERTICAL);
		m_countSpin = newd wxSpinCtrl(this, wxID_ANY, "1", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 1, 999, 1);
		entryGrid->Add(m_countSpin, 0, wxALIGN_CENTER_VERTICAL);

		entryGrid->Add(newd wxStaticText(this, wxID_ANY, "Spawn Time (-1 = default):"), 0, wxALIGN_CENTER_VERTICAL);
		m_spawnTimeSpin = newd wxSpinCtrl(this, wxID_ANY, "-1", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, -1, 3600, -1);
		entryGrid->Add(m_spawnTimeSpin, 0, wxALIGN_CENTER_VERTICAL);

		entriesBox->Add(entryGrid, 0, wxALL | wxEXPAND, 5);

		m_creatureListBox = newd wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 170), 0, nullptr, wxLB_SINGLE);
		entriesBox->Add(m_creatureListBox, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

		wxBoxSizer* entryButtons = newd wxBoxSizer(wxHORIZONTAL);
		m_addOrUpdateBtn = newd wxButton(this, wxID_ANY, "Add / Update");
		wxButton* removeBtn = newd wxButton(this, wxID_ANY, "Remove");
		wxButton* clearBtn = newd wxButton(this, wxID_ANY, "Clear");
		entryButtons->Add(m_addOrUpdateBtn, 0, wxRIGHT, 5);
		entryButtons->Add(removeBtn, 0, wxRIGHT, 5);
		entryButtons->Add(clearBtn, 0);
		entriesBox->Add(entryButtons, 0, wxALL, 5);

		mainSizer->Add(entriesBox, 1, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

		wxBoxSizer* bottom = newd wxBoxSizer(wxHORIZONTAL);
		bottom->AddStretchSpacer(1);
		bottom->Add(newd wxButton(this, wxID_OK, "OK"), 0, wxRIGHT, 5);
		bottom->Add(newd wxButton(this, wxID_CANCEL, "Cancel"), 0);
		mainSizer->Add(bottom, 0, wxALL | wxEXPAND, 10);

		SetSizerAndFit(mainSizer);

		m_entriesList->Bind(wxEVT_LIST_ITEM_SELECTED, &CreatureGroupEditDialog::OnEntrySelected, this);
		m_entriesList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &CreatureGroupEditDialog::OnEntryActivated, this);
		m_addOrUpdateBtn->Bind(wxEVT_BUTTON, &CreatureGroupEditDialog::OnAddOrUpdateEntry, this);
		removeBtn->Bind(wxEVT_BUTTON, &CreatureGroupEditDialog::OnRemoveEntry, this);
		clearBtn->Bind(wxEVT_BUTTON, &CreatureGroupEditDialog::OnClearEntries, this);
		m_creatureFilterInput->Bind(wxEVT_TEXT, &CreatureGroupEditDialog::OnCreatureFilterChanged, this);
		m_creatureListBox->Bind(wxEVT_LISTBOX, &CreatureGroupEditDialog::OnCreatureListSelected, this);
		m_creatureListBox->Bind(wxEVT_LISTBOX_DCLICK, &CreatureGroupEditDialog::OnCreatureListActivated, this);
		m_instanceModeChoice->Bind(wxEVT_CHOICE, &CreatureGroupEditDialog::OnInstanceModeChanged, this);
		m_calculateInstancesBtn->Bind(wxEVT_BUTTON, &CreatureGroupEditDialog::OnCalculateInstances, this);
		Bind(wxEVT_BUTTON, &CreatureGroupEditDialog::OnOk, this, wxID_OK);
		Bind(wxEVT_BUTTON, &CreatureGroupEditDialog::OnCancel, this, wxID_CANCEL);
		Bind(wxEVT_CLOSE_WINDOW, &CreatureGroupEditDialog::OnCloseWindow, this);
	}

	void LoadData() {
		m_nameInput->SetValue(wxstr(m_working.name));
		m_enabledCheck->SetValue(m_working.enabled);
		m_instancesSpin->SetValue(std::max(1, m_working.instances));
		m_instanceModeChoice->SetSelection(
			m_working.instanceMode == AreaCreatureSpawn::InstancePlacementMode::AutoBySpacing ? 1 : 0);
		m_spreadSpin->SetValue(std::max(0, m_working.spreadRadius));
		m_minGroupDistanceSpin->SetValue(std::max(0, m_working.minGroupDistance));
		UpdateInstanceModeControls();
		RefreshEntriesList();
		RefreshCreatureFilterList();
	}

	void RefreshEntriesList() {
		m_entriesList->DeleteAllItems();
		for (size_t i = 0; i < m_working.creatures.size(); ++i) {
			const auto& entry = m_working.creatures[i];
			long row = m_entriesList->InsertItem(static_cast<long>(i), wxstr(entry.name));
			m_entriesList->SetItem(row, 1, wxString::Format("%d", entry.count));
			m_entriesList->SetItem(row, 2, wxString::Format("%d", entry.spawnTime));
		}
	}

	void OnEntrySelected(wxListEvent& event) {
		long idx = event.GetIndex();
		if (idx < 0 || idx >= static_cast<long>(m_working.creatures.size())) {
			return;
		}
		const auto& entry = m_working.creatures[static_cast<size_t>(idx)];
		m_creatureNameInput->SetValue(wxstr(entry.name));
		m_countSpin->SetValue(std::max(1, entry.count));
		m_spawnTimeSpin->SetValue(entry.spawnTime);
	}

	void OnEntryActivated(wxListEvent& event) {
		OnEntrySelected(event);
	}

	void OnAddOrUpdateEntry(wxCommandEvent&) {
		wxString creatureName = m_creatureNameInput->GetValue().Trim().Trim(false);
		if (creatureName.IsEmpty()) {
			wxMessageBox("Choose a creature name.", "Creature Group", wxOK | wxICON_WARNING, this);
			return;
		}

		if (!g_creatures[creatureName.ToStdString()]) {
			const int ret = wxMessageBox(
				wxString::Format("Creature '%s' was not found in the loaded creature database.\n"
				                 "Keep this name anyway?", creatureName),
				"Unknown Creature", wxYES_NO | wxICON_WARNING, this);
			if (ret != wxYES) {
				return;
			}
		}

		AreaCreatureSpawn::CreatureEntry entry;
		entry.name = creatureName.ToStdString();
		entry.count = std::max(1, m_countSpin->GetValue());
		entry.spawnTime = m_spawnTimeSpin->GetValue();

		long selected = m_entriesList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (selected >= 0 && selected < static_cast<long>(m_working.creatures.size())) {
			m_working.creatures[static_cast<size_t>(selected)] = entry;
		} else {
			m_working.creatures.push_back(entry);
		}

		RefreshEntriesList();
	}

	void OnRemoveEntry(wxCommandEvent&) {
		long selected = m_entriesList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
		if (selected < 0 || selected >= static_cast<long>(m_working.creatures.size())) {
			return;
		}
		m_working.creatures.erase(m_working.creatures.begin() + selected);
		RefreshEntriesList();
	}

	void OnClearEntries(wxCommandEvent&) {
		m_working.creatures.clear();
		RefreshEntriesList();
	}

	void OnInstanceModeChanged(wxCommandEvent&) {
		UpdateInstanceModeControls();
	}

	void UpdateInstanceModeControls() {
		const bool manual = (m_instanceModeChoice->GetSelection() != 1);
		m_instancesSpin->Enable(manual);
		m_calculateInstancesBtn->Enable(true);
		if (manual) {
			m_instancesSpin->SetToolTip("Number of group instances to try.");
		} else {
			m_instancesSpin->SetToolTip("Ignored in auto mode.");
		}
	}

	void OnCalculateInstances(wxCommandEvent&) {
		m_working.spreadRadius = std::max(0, m_spreadSpin->GetValue());
		m_working.minGroupDistance = std::max(0, m_minGroupDistanceSpin->GetValue());

		const int creaturesPerInstance = std::max(1, m_working.getTotalCreatures());
		if (m_working.creatures.empty()) {
			wxMessageBox("Add at least one creature entry before calculating instances.",
			             "Instance Calculator", wxOK | wxICON_INFORMATION, this);
			return;
		}

		AreaCreatureSpawn::AreaDefinition area = m_areaContext;
		area.normalize();
		const long long width = static_cast<long long>(std::abs(area.toPos.x - area.fromPos.x)) + 1;
		const long long height = static_cast<long long>(std::abs(area.toPos.y - area.fromPos.y)) + 1;
		const long long floors = static_cast<long long>(std::abs(area.toPos.z - area.fromPos.z)) + 1;
		const long long areaTiles = width * height * floors;

		if (areaTiles <= 0) {
			wxMessageBox("Invalid area for calculation.", "Instance Calculator", wxOK | wxICON_ERROR, this);
			return;
		}

		long long byTiles = areaTiles / creaturesPerInstance;
		byTiles = std::max<long long>(1, byTiles);

		const int spacing = std::max(0, m_working.minGroupDistance);
		long long bySpacing = LLONG_MAX;
		if (spacing > 0) {
			const long long cellsX = (width + spacing - 1) / spacing;
			const long long cellsY = (height + spacing - 1) / spacing;
			bySpacing = cellsX * cellsY * floors;
		}

		long long suggested = std::min(byTiles, bySpacing);
		suggested = std::max<long long>(1, suggested);
		const int clamped = static_cast<int>(std::min<long long>(999, suggested));
		m_instancesSpin->SetValue(clamped);

		wxString spacingText = (spacing > 0) ?
			wxString::Format("%lld", bySpacing) :
			wxString("N/A (spacing = 0)");
		wxMessageBox(
			wxString::Format(
				"Area: %lldx%lldx%lld (%lld tiles)\n"
				"Creatures per instance: %d\n"
				"Limit by tiles: %lld\n"
				"Limit by spacing: %s\n"
				"Suggested instances: %lld%s",
				width, height, floors, areaTiles,
				creaturesPerInstance,
				byTiles,
				spacingText,
				suggested,
				suggested > 999 ? " (clamped to 999 in UI)" : ""),
			"Instance Calculator", wxOK | wxICON_INFORMATION, this);
	}

	void RefreshCreatureFilterList() {
		if (!m_creatureListBox) {
			return;
		}

		const wxString filter = m_creatureFilterInput ? m_creatureFilterInput->GetValue().Lower() : wxString();
		const auto& allNames = GetCachedCreatureNames();
		wxArrayString prefixMatches;
		wxArrayString containsMatches;

		for (const std::string& name : allNames) {
			wxString wxName = wxstr(name);
			wxString lowerName = wxName.Lower();
			if (filter.IsEmpty()) {
				prefixMatches.Add(wxName);
				continue;
			}

			if (lowerName.StartsWith(filter)) {
				prefixMatches.Add(wxName);
			} else if (lowerName.Find(filter) != wxNOT_FOUND) {
				containsMatches.Add(wxName);
			}
		}

		const wxString selectedName = m_creatureNameInput ? m_creatureNameInput->GetValue() : wxString();
		m_creatureListBox->Freeze();
		m_creatureListBox->Clear();
		for (size_t i = 0; i < prefixMatches.size(); ++i) {
			m_creatureListBox->Append(prefixMatches[i]);
		}
		for (size_t i = 0; i < containsMatches.size(); ++i) {
			m_creatureListBox->Append(containsMatches[i]);
		}

		if (!selectedName.IsEmpty()) {
			const int idx = m_creatureListBox->FindString(selectedName, true);
			if (idx != wxNOT_FOUND) {
				m_creatureListBox->SetSelection(idx);
			}
		}
		m_creatureListBox->Thaw();
	}

	void OnCreatureFilterChanged(wxCommandEvent&) {
		RefreshCreatureFilterList();
	}

	void OnCreatureListSelected(wxCommandEvent&) {
		const int selected = m_creatureListBox ? m_creatureListBox->GetSelection() : wxNOT_FOUND;
		if (selected == wxNOT_FOUND) {
			return;
		}
		m_creatureNameInput->SetValue(m_creatureListBox->GetString(selected));
	}

	void OnCreatureListActivated(wxCommandEvent&) {
		wxCommandEvent dummy;
		OnCreatureListSelected(dummy);
	}

	void OnOk(wxCommandEvent&) {
		m_working.name = m_nameInput->GetValue().Trim().Trim(false).ToStdString();
		m_working.enabled = m_enabledCheck->GetValue();
		m_working.instances = std::max(1, m_instancesSpin->GetValue());
		m_working.instanceMode =
			(m_instanceModeChoice->GetSelection() == 1) ?
				AreaCreatureSpawn::InstancePlacementMode::AutoBySpacing :
				AreaCreatureSpawn::InstancePlacementMode::Manual;
		m_working.spreadRadius = std::max(0, m_spreadSpin->GetValue());
		m_working.minGroupDistance = std::max(0, m_minGroupDistanceSpin->GetValue());

		std::string error;
		if (!m_working.validate(error)) {
			wxMessageBox(wxstr(error), "Invalid Group", wxOK | wxICON_ERROR, this);
			return;
		}

		if (m_onSave) {
			m_onSave(m_working);
		}
		Destroy();
	}

	void OnCancel(wxCommandEvent&) {
		Destroy();
	}

	void OnCloseWindow(wxCloseEvent&) {
		Destroy();
	}

private:
	AreaCreatureSpawn::CreatureGroup m_working;
	AreaCreatureSpawn::AreaDefinition m_areaContext;
	SaveCallback m_onSave;

	wxTextCtrl* m_nameInput = nullptr;
	wxCheckBox* m_enabledCheck = nullptr;
	wxChoice* m_instanceModeChoice = nullptr;
	wxSpinCtrl* m_instancesSpin = nullptr;
	wxButton* m_calculateInstancesBtn = nullptr;
	wxSpinCtrl* m_spreadSpin = nullptr;
	wxSpinCtrl* m_minGroupDistanceSpin = nullptr;

	wxListCtrl* m_entriesList = nullptr;
	wxTextCtrl* m_creatureNameInput = nullptr;
	wxTextCtrl* m_creatureFilterInput = nullptr;
	wxListBox* m_creatureListBox = nullptr;
	wxSpinCtrl* m_countSpin = nullptr;
	wxSpinCtrl* m_spawnTimeSpin = nullptr;
	wxButton* m_addOrUpdateBtn = nullptr;
};

} // namespace

wxBEGIN_EVENT_TABLE(AreaCreatureSpawnDialog, wxDialog)
	EVT_BUTTON(ID_PICK_AREA, AreaCreatureSpawnDialog::OnPickArea)
	EVT_BUTTON(ID_USE_SELECTION, AreaCreatureSpawnDialog::OnUseSelection)
	EVT_SPINCTRL(ID_FROM_X, AreaCreatureSpawnDialog::OnAreaCoordsChanged)
	EVT_SPINCTRL(ID_FROM_Y, AreaCreatureSpawnDialog::OnAreaCoordsChanged)
	EVT_SPINCTRL(ID_FROM_Z, AreaCreatureSpawnDialog::OnAreaCoordsChanged)
	EVT_SPINCTRL(ID_TO_X, AreaCreatureSpawnDialog::OnAreaCoordsChanged)
	EVT_SPINCTRL(ID_TO_Y, AreaCreatureSpawnDialog::OnAreaCoordsChanged)
	EVT_SPINCTRL(ID_TO_Z, AreaCreatureSpawnDialog::OnAreaCoordsChanged)

	EVT_BUTTON(ID_ADD_GROUP, AreaCreatureSpawnDialog::OnAddGroup)
	EVT_BUTTON(ID_EDIT_GROUP, AreaCreatureSpawnDialog::OnEditGroup)
	EVT_BUTTON(ID_REMOVE_GROUP, AreaCreatureSpawnDialog::OnRemoveGroup)
	EVT_LIST_ITEM_ACTIVATED(ID_GROUPS_LIST, AreaCreatureSpawnDialog::OnGroupDoubleClick)
	EVT_LIST_ITEM_CHECKED(ID_GROUPS_LIST, AreaCreatureSpawnDialog::OnGroupCheckChanged)
	EVT_LIST_ITEM_UNCHECKED(ID_GROUPS_LIST, AreaCreatureSpawnDialog::OnGroupCheckChanged)

	EVT_BUTTON(ID_PREVIEW, AreaCreatureSpawnDialog::OnPreview)
	EVT_BUTTON(ID_REROLL, AreaCreatureSpawnDialog::OnReroll)
	EVT_BUTTON(ID_APPLY, AreaCreatureSpawnDialog::OnApply)
	EVT_BUTTON(ID_REROLL_APPLY, AreaCreatureSpawnDialog::OnRerollApply)
	EVT_BUTTON(ID_REMOVE_LAST_APPLY, AreaCreatureSpawnDialog::OnRemoveLastApply)

	EVT_CHOICE(ID_PRESET_CHOICE, AreaCreatureSpawnDialog::OnPresetSelected)
	EVT_BUTTON(ID_SAVE_PRESET, AreaCreatureSpawnDialog::OnSavePreset)
	EVT_BUTTON(ID_REFRESH_PRESET, AreaCreatureSpawnDialog::OnRefreshPresets)
	EVT_BUTTON(ID_DELETE_PRESET, AreaCreatureSpawnDialog::OnDeletePreset)
	EVT_BUTTON(ID_EXPORT_PRESET, AreaCreatureSpawnDialog::OnExportPreset)
	EVT_BUTTON(ID_IMPORT_PRESET, AreaCreatureSpawnDialog::OnImportPreset)

	EVT_CLOSE(AreaCreatureSpawnDialog::OnClose)
wxEND_EVENT_TABLE()

AreaCreatureSpawnDialog::AreaCreatureSpawnDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, "Auto Creature Spawn", wxDefaultPosition, wxSize(920, 760),
	           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	UpdateEngine();
	CreateControls();

	m_preset.name = "Creature Preset";
	m_preset.hasArea = true;
	BuildAreaFromUI();
	m_preset.area = m_area;

	AreaCreatureSpawn::PresetManager::getInstance().loadPresets();
	UpdatePresetList();
	UpdateGroupsList();
	UpdateAreaInfoText();
	UpdateStatsText();
	Centre();
}

void AreaCreatureSpawnDialog::UpdateEngine() {
	Editor* editor = g_gui.GetCurrentEditor();
	if (editor) {
		m_engine = std::make_unique<AreaCreatureSpawn::SpawnEngine>(editor);
	} else {
		m_engine.reset();
	}
}

void AreaCreatureSpawnDialog::CreateControls() {
	wxBoxSizer* mainSizer = newd wxBoxSizer(wxVERTICAL);

	CreatePresetControls(mainSizer);

	wxNotebook* notebook = newd wxNotebook(this, wxID_ANY);
	CreateAreaTab(notebook);
	CreateGroupsTab(notebook);
	CreateSettingsTab(notebook);
	CreateSeedTab(notebook);
	mainSizer->Add(notebook, 1, wxALL | wxEXPAND, 8);

	CreateActionControls(mainSizer);

	SetSizerAndFit(mainSizer);
}

void AreaCreatureSpawnDialog::CreatePresetControls(wxBoxSizer* mainSizer) {
	wxStaticBoxSizer* presetBox = newd wxStaticBoxSizer(wxHORIZONTAL, this, "Preset");
	presetBox->Add(newd wxStaticText(this, wxID_ANY, "Preset:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	m_presetChoice = newd wxChoice(this, ID_PRESET_CHOICE);
	presetBox->Add(m_presetChoice, 1, wxRIGHT | wxEXPAND, 8);

	presetBox->Add(newd wxStaticText(this, wxID_ANY, "Name:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	m_presetNameInput = newd wxTextCtrl(this, wxID_ANY, "Creature Preset", wxDefaultPosition, wxSize(200, -1));
	presetBox->Add(m_presetNameInput, 0, wxRIGHT, 8);

	presetBox->Add(newd wxButton(this, ID_SAVE_PRESET, "Save"), 0, wxRIGHT, 4);
	presetBox->Add(newd wxButton(this, ID_REFRESH_PRESET, "Refresh"), 0, wxRIGHT, 4);
	presetBox->Add(newd wxButton(this, ID_DELETE_PRESET, "Delete"), 0, wxRIGHT, 4);
	presetBox->Add(newd wxButton(this, ID_EXPORT_PRESET, "Export"), 0, wxRIGHT, 4);
	presetBox->Add(newd wxButton(this, ID_IMPORT_PRESET, "Import"), 0);

	mainSizer->Add(presetBox, 0, wxLEFT | wxRIGHT | wxTOP | wxEXPAND, 8);
}

void AreaCreatureSpawnDialog::CreateAreaTab(wxNotebook* notebook) {
	wxPanel* panel = newd wxPanel(notebook);
	wxBoxSizer* sizer = newd wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer* coordsBox = newd wxStaticBoxSizer(wxVERTICAL, panel, "Area (fromPos -> toPos)");
	wxFlexGridSizer* grid = newd wxFlexGridSizer(2, 6, 8);
	grid->AddGrowableCol(1);
	grid->AddGrowableCol(3);
	grid->AddGrowableCol(5);

	grid->Add(newd wxStaticText(panel, wxID_ANY, "From X:"), 0, wxALIGN_CENTER_VERTICAL);
	m_fromXSpin = newd wxSpinCtrl(panel, ID_FROM_X, "0", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 65535, 1000);
	grid->Add(m_fromXSpin, 1, wxEXPAND);
	grid->Add(newd wxStaticText(panel, wxID_ANY, "From Y:"), 0, wxALIGN_CENTER_VERTICAL);
	m_fromYSpin = newd wxSpinCtrl(panel, ID_FROM_Y, "0", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 65535, 1000);
	grid->Add(m_fromYSpin, 1, wxEXPAND);
	grid->Add(newd wxStaticText(panel, wxID_ANY, "From Z:"), 0, wxALIGN_CENTER_VERTICAL);
	m_fromZSpin = newd wxSpinCtrl(panel, ID_FROM_Z, "7", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 15, 7);
	grid->Add(m_fromZSpin, 1, wxEXPAND);

	grid->Add(newd wxStaticText(panel, wxID_ANY, "To X:"), 0, wxALIGN_CENTER_VERTICAL);
	m_toXSpin = newd wxSpinCtrl(panel, ID_TO_X, "0", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 65535, 1020);
	grid->Add(m_toXSpin, 1, wxEXPAND);
	grid->Add(newd wxStaticText(panel, wxID_ANY, "To Y:"), 0, wxALIGN_CENTER_VERTICAL);
	m_toYSpin = newd wxSpinCtrl(panel, ID_TO_Y, "0", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 65535, 1020);
	grid->Add(m_toYSpin, 1, wxEXPAND);
	grid->Add(newd wxStaticText(panel, wxID_ANY, "To Z:"), 0, wxALIGN_CENTER_VERTICAL);
	m_toZSpin = newd wxSpinCtrl(panel, ID_TO_Z, "7", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 15, 7);
	grid->Add(m_toZSpin, 1, wxEXPAND);

	coordsBox->Add(grid, 0, wxALL | wxEXPAND, 8);

	wxBoxSizer* btns = newd wxBoxSizer(wxHORIZONTAL);
	btns->Add(newd wxButton(panel, ID_PICK_AREA, "Pick on Map"), 0, wxRIGHT, 5);
	btns->Add(newd wxButton(panel, ID_USE_SELECTION, "Use Selection"), 0, wxRIGHT, 5);
	m_pickStatusText = newd wxStaticText(panel, wxID_ANY, "Tip: use Pick on Map for fast from/to selection.");
	btns->Add(m_pickStatusText, 1, wxALIGN_CENTER_VERTICAL);
	coordsBox->Add(btns, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);

	m_areaInfoText = newd wxStaticText(panel, wxID_ANY, "Area: (0,0,0) -> (0,0,0)");
	coordsBox->Add(m_areaInfoText, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

	sizer->Add(coordsBox, 0, wxALL | wxEXPAND, 8);
	sizer->AddStretchSpacer(1);

	panel->SetSizer(sizer);
	notebook->AddPage(panel, "Area");
}

void AreaCreatureSpawnDialog::CreateGroupsTab(wxNotebook* notebook) {
	wxPanel* panel = newd wxPanel(notebook);
	wxBoxSizer* sizer = newd wxBoxSizer(wxVERTICAL);

	m_groupsListCtrl = newd wxListCtrl(panel, ID_GROUPS_LIST, wxDefaultPosition, wxDefaultSize,
	                                   wxLC_REPORT | wxLC_SINGLE_SEL);
	m_groupsListCtrl->EnableCheckBoxes(true);
	m_groupsListCtrl->InsertColumn(0, "Name", wxLIST_FORMAT_LEFT, 230);
	m_groupsListCtrl->InsertColumn(1, "Instances", wxLIST_FORMAT_LEFT, 80);
	m_groupsListCtrl->InsertColumn(2, "Spread", wxLIST_FORMAT_LEFT, 70);
	m_groupsListCtrl->InsertColumn(3, "Min Dist", wxLIST_FORMAT_LEFT, 80);
	m_groupsListCtrl->InsertColumn(4, "Creatures", wxLIST_FORMAT_LEFT, 90);
	sizer->Add(m_groupsListCtrl, 1, wxALL | wxEXPAND, 8);

	wxBoxSizer* buttons = newd wxBoxSizer(wxHORIZONTAL);
	buttons->Add(newd wxButton(panel, ID_ADD_GROUP, "Add Group"), 0, wxRIGHT, 5);
	buttons->Add(newd wxButton(panel, ID_EDIT_GROUP, "Edit Group"), 0, wxRIGHT, 5);
	buttons->Add(newd wxButton(panel, ID_REMOVE_GROUP, "Remove Group"), 0);
	sizer->Add(buttons, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

	panel->SetSizer(sizer);
	notebook->AddPage(panel, "Groups");
}

void AreaCreatureSpawnDialog::CreateSettingsTab(wxNotebook* notebook) {
	wxPanel* panel = newd wxPanel(notebook);
	wxBoxSizer* sizer = newd wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer* settings = newd wxStaticBoxSizer(wxVERTICAL, panel, "Spawn Settings");
	wxFlexGridSizer* grid = newd wxFlexGridSizer(2, 6, 10);
	grid->AddGrowableCol(1);
	grid->AddGrowableCol(3);

	grid->Add(newd wxStaticText(panel, wxID_ANY, "Min Creature Distance:"), 0, wxALIGN_CENTER_VERTICAL);
	m_minCreatureDistanceSpin = newd wxSpinCtrl(panel, wxID_ANY, "1", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 0, 50, 1);
	grid->Add(m_minCreatureDistanceSpin, 0, wxALIGN_CENTER_VERTICAL);

	grid->Add(newd wxStaticText(panel, wxID_ANY, "Availability Check Range:"), 0, wxALIGN_CENTER_VERTICAL);
	m_availabilityRangeSpin = newd wxSpinCtrl(panel, wxID_ANY, "6", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 0, 30, 6);
	grid->Add(m_availabilityRangeSpin, 0, wxALIGN_CENTER_VERTICAL);

	grid->Add(newd wxStaticText(panel, wxID_ANY, "Min Walkable Tiles in Range:"), 0, wxALIGN_CENTER_VERTICAL);
	m_minWalkableSpin = newd wxSpinCtrl(panel, wxID_ANY, "12", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 0, 400, 12);
	grid->Add(m_minWalkableSpin, 0, wxALIGN_CENTER_VERTICAL);

	grid->Add(newd wxStaticText(panel, wxID_ANY, "Escape Distance (BFS):"), 0, wxALIGN_CENTER_VERTICAL);
	m_escapeDistanceSpin = newd wxSpinCtrl(panel, wxID_ANY, "4", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 0, 30, 4);
	grid->Add(m_escapeDistanceSpin, 0, wxALIGN_CENTER_VERTICAL);

	grid->Add(newd wxStaticText(panel, wxID_ANY, "Center Attempts per Instance:"), 0, wxALIGN_CENTER_VERTICAL);
	m_centerAttemptsSpin = newd wxSpinCtrl(panel, wxID_ANY, "80", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 1, 1000, 80);
	grid->Add(m_centerAttemptsSpin, 0, wxALIGN_CENTER_VERTICAL);

	grid->Add(newd wxStaticText(panel, wxID_ANY, "Processing Profile:"), 0, wxALIGN_CENTER_VERTICAL);
	m_processingProfileChoice = newd wxChoice(panel, wxID_ANY);
	m_processingProfileChoice->Append("Low-End (Fast)");
	m_processingProfileChoice->Append("Balanced");
	m_processingProfileChoice->Append("Quality (Slower)");
	m_processingProfileChoice->SetSelection(1);
	grid->Add(m_processingProfileChoice, 0, wxALIGN_CENTER_VERTICAL);

	grid->Add(newd wxStaticText(panel, wxID_ANY, "Default Spawn Time:"), 0, wxALIGN_CENTER_VERTICAL);
	m_defaultSpawnTimeSpin = newd wxSpinCtrl(panel, wxID_ANY, "60", wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 0, 3600, 60);
	grid->Add(m_defaultSpawnTimeSpin, 0, wxALIGN_CENTER_VERTICAL);

	settings->Add(grid, 0, wxALL | wxEXPAND, 8);
	m_autoCreateSpawnCheck = newd wxCheckBox(panel, wxID_ANY, "Auto-create spawn when needed");
	m_autoCreateSpawnCheck->SetValue(true);
	settings->Add(m_autoCreateSpawnCheck, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);

	sizer->Add(settings, 0, wxALL | wxEXPAND, 8);
	sizer->AddStretchSpacer(1);

	panel->SetSizer(sizer);
	notebook->AddPage(panel, "Settings");
}

void AreaCreatureSpawnDialog::CreateSeedTab(wxNotebook* notebook) {
	wxPanel* panel = newd wxPanel(notebook);
	wxBoxSizer* sizer = newd wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer* seedBox = newd wxStaticBoxSizer(wxVERTICAL, panel, "Seed");
	m_useSeedCheck = newd wxCheckBox(panel, wxID_ANY, "Use fixed seed");
	seedBox->Add(m_useSeedCheck, 0, wxALL, 8);

	wxBoxSizer* row = newd wxBoxSizer(wxHORIZONTAL);
	row->Add(newd wxStaticText(panel, wxID_ANY, "Seed:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	m_seedInput = newd wxTextCtrl(panel, wxID_ANY, "0");
	row->Add(m_seedInput, 1, wxEXPAND);
	seedBox->Add(row, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);

	sizer->Add(seedBox, 0, wxALL | wxEXPAND, 8);
	sizer->AddStretchSpacer(1);
	panel->SetSizer(sizer);
	notebook->AddPage(panel, "Seed");
}

void AreaCreatureSpawnDialog::CreateActionControls(wxBoxSizer* mainSizer) {
	wxBoxSizer* row = newd wxBoxSizer(wxHORIZONTAL);
	m_statsText = newd wxStaticText(this, wxID_ANY, "No preview generated.");
	row->Add(m_statsText, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
	row->Add(newd wxButton(this, ID_PREVIEW, "Preview"), 0, wxRIGHT, 5);
	row->Add(newd wxButton(this, ID_REROLL, "Reroll"), 0, wxRIGHT, 5);
	row->Add(newd wxButton(this, ID_REROLL_APPLY, "Reroll + Apply"), 0, wxRIGHT, 5);
	m_applyBtn = newd wxButton(this, ID_APPLY, "Apply");
	row->Add(m_applyBtn, 0, wxRIGHT, 5);
	m_removeLastApplyBtn = newd wxButton(this, ID_REMOVE_LAST_APPLY, "Remove Last Apply");
	row->Add(m_removeLastApplyBtn, 0);

	mainSizer->Add(row, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 8);
}

void AreaCreatureSpawnDialog::BuildAreaFromUI() {
	m_area.fromPos = Position(m_fromXSpin->GetValue(), m_fromYSpin->GetValue(), m_fromZSpin->GetValue());
	m_area.toPos = Position(m_toXSpin->GetValue(), m_toYSpin->GetValue(), m_toZSpin->GetValue());
	m_area.normalize();
}

void AreaCreatureSpawnDialog::BuildPresetFromUI() {
	BuildAreaFromUI();
	m_preset.area = m_area;
	m_preset.hasArea = true;
	m_preset.settings.minCreatureDistance = std::max(0, m_minCreatureDistanceSpin->GetValue());
	m_preset.settings.availabilityRange = std::max(0, m_availabilityRangeSpin->GetValue());
	m_preset.settings.minWalkableTilesInRange = std::max(0, m_minWalkableSpin->GetValue());
	m_preset.settings.bfsEscapeDistance = std::max(0, m_escapeDistanceSpin->GetValue());
	m_preset.settings.centerAttempts = std::max(1, m_centerAttemptsSpin->GetValue());
	m_preset.settings.autoCreateSpawn = m_autoCreateSpawnCheck->GetValue();
	m_preset.settings.defaultSpawnTime = std::max(0, m_defaultSpawnTimeSpin->GetValue());
	switch (m_processingProfileChoice ? m_processingProfileChoice->GetSelection() : 1) {
		case 0:
			m_preset.settings.processingProfile = AreaCreatureSpawn::ProcessingProfile::LowEnd;
			break;
		case 2:
			m_preset.settings.processingProfile = AreaCreatureSpawn::ProcessingProfile::Quality;
			break;
		case 1:
		default:
			m_preset.settings.processingProfile = AreaCreatureSpawn::ProcessingProfile::Balanced;
			break;
	}

	if (m_useSeedCheck->GetValue()) {
		try {
			m_preset.defaultSeed = std::stoull(m_seedInput->GetValue().ToStdString());
		} catch (...) {
			m_preset.defaultSeed = 0;
		}
	} else {
		m_preset.defaultSeed = 0;
	}
}

void AreaCreatureSpawnDialog::LoadPresetToUI(const AreaCreatureSpawn::SpawnPreset& preset) {
	m_preset = preset;

	if (preset.hasArea) {
		m_fromXSpin->SetValue(preset.area.fromPos.x);
		m_fromYSpin->SetValue(preset.area.fromPos.y);
		m_fromZSpin->SetValue(preset.area.fromPos.z);
		m_toXSpin->SetValue(preset.area.toPos.x);
		m_toYSpin->SetValue(preset.area.toPos.y);
		m_toZSpin->SetValue(preset.area.toPos.z);
	}

	m_minCreatureDistanceSpin->SetValue(preset.settings.minCreatureDistance);
	m_availabilityRangeSpin->SetValue(preset.settings.availabilityRange);
	m_minWalkableSpin->SetValue(preset.settings.minWalkableTilesInRange);
	m_escapeDistanceSpin->SetValue(preset.settings.bfsEscapeDistance);
	m_centerAttemptsSpin->SetValue(preset.settings.centerAttempts);
	m_autoCreateSpawnCheck->SetValue(preset.settings.autoCreateSpawn);
	m_defaultSpawnTimeSpin->SetValue(preset.settings.defaultSpawnTime);
	if (m_processingProfileChoice) {
		switch (preset.settings.processingProfile) {
			case AreaCreatureSpawn::ProcessingProfile::LowEnd:
				m_processingProfileChoice->SetSelection(0);
				break;
			case AreaCreatureSpawn::ProcessingProfile::Quality:
				m_processingProfileChoice->SetSelection(2);
				break;
			case AreaCreatureSpawn::ProcessingProfile::Balanced:
			default:
				m_processingProfileChoice->SetSelection(1);
				break;
		}
	}

	if (preset.defaultSeed != 0) {
		m_useSeedCheck->SetValue(true);
		m_seedInput->SetValue(wxString::Format("%llu", preset.defaultSeed));
	} else {
		m_useSeedCheck->SetValue(false);
		m_seedInput->SetValue("0");
	}

	m_presetNameInput->SetValue(wxstr(preset.name));
	UpdateAreaInfoText();
	UpdateGroupsList();
	UpdateStatsText();
}

void AreaCreatureSpawnDialog::UpdatePresetList() {
	if (!m_presetChoice) {
		return;
	}
	m_presetChoice->Clear();
	m_presetChoice->Append("(None - Custom)");
	auto names = AreaCreatureSpawn::PresetManager::getInstance().getPresetNames();
	for (const auto& name : names) {
		m_presetChoice->Append(wxstr(name));
	}
	m_presetChoice->SetSelection(0);
}

void AreaCreatureSpawnDialog::UpdateGroupsList() {
	if (!m_groupsListCtrl) {
		return;
	}
	m_groupsListCtrl->DeleteAllItems();
	for (size_t i = 0; i < m_preset.groups.size(); ++i) {
		const auto& group = m_preset.groups[i];
		long row = m_groupsListCtrl->InsertItem(static_cast<long>(i), wxstr(group.name));
		const wxString instancesText =
			(group.instanceMode == AreaCreatureSpawn::InstancePlacementMode::AutoBySpacing) ?
				wxString("Auto") :
				wxString::Format("%d", group.instances);
		m_groupsListCtrl->SetItem(row, 1, instancesText);
		m_groupsListCtrl->SetItem(row, 2, wxString::Format("%d", group.spreadRadius));
		m_groupsListCtrl->SetItem(row, 3, wxString::Format("%d", group.minGroupDistance));
		m_groupsListCtrl->SetItem(row, 4, wxString::Format("%d", group.getTotalCreatures()));
		m_groupsListCtrl->CheckItem(row, group.enabled);
	}
}

void AreaCreatureSpawnDialog::UpdateAreaInfoText() {
	BuildAreaFromUI();
	int width = std::abs(m_area.toPos.x - m_area.fromPos.x) + 1;
	int height = std::abs(m_area.toPos.y - m_area.fromPos.y) + 1;
	int floors = std::abs(m_area.toPos.z - m_area.fromPos.z) + 1;
	long long total = static_cast<long long>(width) * static_cast<long long>(height) * static_cast<long long>(floors);
	m_areaInfoText->SetLabel(wxString::Format("Area: (%d,%d,%d) -> (%d,%d,%d) | %dx%d | %d floor(s) | %lld tiles",
		m_area.fromPos.x, m_area.fromPos.y, m_area.fromPos.z,
		m_area.toPos.x, m_area.toPos.y, m_area.toPos.z,
		width, height, floors, total));
}

void AreaCreatureSpawnDialog::UpdateStatsText() {
	if (!m_engine) {
		m_statsText->SetLabel("No editor available.");
		if (m_applyBtn) m_applyBtn->Enable(false);
		if (m_removeLastApplyBtn) m_removeLastApplyBtn->Enable(false);
		return;
	}

	const auto& preview = m_engine->getPreviewState();
	if (!preview.isValid) {
		m_statsText->SetLabel("No preview generated.");
		if (m_applyBtn) m_applyBtn->Enable(false);
	} else {
		m_statsText->SetLabel(wxString::Format("Preview: %d creatures | Seed: %llu",
			static_cast<int>(preview.creatures.size()), preview.seed));
		if (m_applyBtn) m_applyBtn->Enable(!preview.creatures.empty());
	}
	if (m_removeLastApplyBtn) {
		m_removeLastApplyBtn->Enable(m_engine->hasLastApplied());
	}
}

void AreaCreatureSpawnDialog::OnPickArea(wxCommandEvent&) {
	if (!g_gui.IsEditorOpen()) {
		return;
	}

	if (m_pickStatusText) {
		m_pickStatusText->SetForegroundColour(wxColour(0, 110, 220));
		m_pickStatusText->SetLabel("Click first corner on map...");
	}

	g_gui.BeginRectanglePick(
		[this](const Position& first, const Position& second) {
			m_fromXSpin->SetValue(first.x);
			m_fromYSpin->SetValue(first.y);
			m_fromZSpin->SetValue(first.z);
			m_toXSpin->SetValue(second.x);
			m_toYSpin->SetValue(second.y);
			m_toZSpin->SetValue(second.z);
			if (m_pickStatusText) {
				m_pickStatusText->SetForegroundColour(wxColour(0, 140, 0));
				m_pickStatusText->SetLabel(wxString::Format("Area selected: (%d,%d,%d) -> (%d,%d,%d)",
					first.x, first.y, first.z, second.x, second.y, second.z));
			}
			UpdateAreaInfoText();
		},
		[this]() {
			if (m_pickStatusText) {
				m_pickStatusText->SetForegroundColour(wxColour(170, 80, 0));
				m_pickStatusText->SetLabel("Area pick cancelled.");
			}
		},
		[this](const Position& first) {
			if (m_pickStatusText) {
				m_pickStatusText->SetForegroundColour(wxColour(200, 130, 0));
				m_pickStatusText->SetLabel(wxString::Format("First corner: (%d,%d,%d). Click second corner...",
					first.x, first.y, first.z));
			}
		}
	);
}

void AreaCreatureSpawnDialog::OnUseSelection(wxCommandEvent&) {
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		return;
	}
	const Selection& selection = editor->getSelection();
	if (selection.empty()) {
		wxMessageBox("No tiles selected. Select an area first.", "Auto Creature Spawn", wxOK | wxICON_INFORMATION, this);
		return;
	}

	Position minPos(INT_MAX, INT_MAX, INT_MAX);
	Position maxPos(INT_MIN, INT_MIN, INT_MIN);
	for (Tile* tile : selection.getTiles()) {
		if (!tile) {
			continue;
		}
		const Position& pos = tile->getPosition();
		minPos.x = std::min(minPos.x, pos.x);
		minPos.y = std::min(minPos.y, pos.y);
		minPos.z = std::min(minPos.z, pos.z);
		maxPos.x = std::max(maxPos.x, pos.x);
		maxPos.y = std::max(maxPos.y, pos.y);
		maxPos.z = std::max(maxPos.z, pos.z);
	}

	m_fromXSpin->SetValue(minPos.x);
	m_fromYSpin->SetValue(minPos.y);
	m_fromZSpin->SetValue(minPos.z);
	m_toXSpin->SetValue(maxPos.x);
	m_toYSpin->SetValue(maxPos.y);
	m_toZSpin->SetValue(maxPos.z);

	UpdateAreaInfoText();
}

void AreaCreatureSpawnDialog::OnAreaCoordsChanged(wxSpinEvent&) {
	UpdateAreaInfoText();
}

void AreaCreatureSpawnDialog::OnAddGroup(wxCommandEvent&) {
	BuildAreaFromUI();
	AreaCreatureSpawn::CreatureGroup group;
	group.name = wxString::Format("Group %d", static_cast<int>(m_preset.groups.size()) + 1).ToStdString();
	CreatureGroupEditDialog* dialog = newd CreatureGroupEditDialog(
		this, group, m_area,
		[this](const AreaCreatureSpawn::CreatureGroup& savedGroup) {
			m_preset.groups.push_back(savedGroup);
			UpdateGroupsList();
		});
	dialog->Show();
	dialog->Raise();
}

void AreaCreatureSpawnDialog::OnEditGroup(wxCommandEvent&) {
	long selected = m_groupsListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected < 0 || selected >= static_cast<long>(m_preset.groups.size())) {
		wxMessageBox("Select a group first.", "Auto Creature Spawn", wxOK | wxICON_INFORMATION, this);
		return;
	}
	BuildAreaFromUI();
	AreaCreatureSpawn::CreatureGroup copy = m_preset.groups[static_cast<size_t>(selected)];
	const size_t selectedIndex = static_cast<size_t>(selected);
	CreatureGroupEditDialog* dialog = newd CreatureGroupEditDialog(
		this, copy, m_area,
		[this, selectedIndex](const AreaCreatureSpawn::CreatureGroup& savedGroup) {
			if (selectedIndex < m_preset.groups.size()) {
				m_preset.groups[selectedIndex] = savedGroup;
			} else {
				m_preset.groups.push_back(savedGroup);
			}
			UpdateGroupsList();
		});
	dialog->Show();
	dialog->Raise();
}

void AreaCreatureSpawnDialog::OnRemoveGroup(wxCommandEvent&) {
	long selected = m_groupsListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected < 0 || selected >= static_cast<long>(m_preset.groups.size())) {
		return;
	}
	m_preset.groups.erase(m_preset.groups.begin() + selected);
	UpdateGroupsList();
}

void AreaCreatureSpawnDialog::OnGroupDoubleClick(wxListEvent&) {
	wxCommandEvent dummy;
	OnEditGroup(dummy);
}

void AreaCreatureSpawnDialog::OnGroupCheckChanged(wxListEvent& event) {
	long idx = event.GetIndex();
	if (idx < 0 || idx >= static_cast<long>(m_preset.groups.size())) {
		return;
	}
	m_preset.groups[static_cast<size_t>(idx)].enabled = m_groupsListCtrl->IsItemChecked(idx);
}

void AreaCreatureSpawnDialog::OnPreview(wxCommandEvent&) {
	if (!m_engine) {
		wxMessageBox("No editor available.", "Auto Creature Spawn", wxOK | wxICON_ERROR, this);
		return;
	}
	BuildPresetFromUI();
	std::string error;
	if (!m_preset.validate(error)) {
		wxMessageBox(wxstr(error), "Invalid Preset", wxOK | wxICON_ERROR, this);
		return;
	}

	m_engine->setArea(m_area);
	m_engine->setPreset(m_preset);

	uint64_t seed = 0;
	if (m_useSeedCheck->GetValue()) {
		try {
			seed = std::stoull(m_seedInput->GetValue().ToStdString());
		} catch (...) {
			seed = 0;
		}
	}

	wxProgressDialog progress(
		"Auto Creature Spawn",
		"Generating preview...",
		100,
		this,
		wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME | wxPD_AUTO_HIDE);
	auto progressCb = [&progress](const std::string& message) -> bool {
		bool keepGoing = true;
		progress.Pulse(wxstr(message), &keepGoing);
		return keepGoing;
	};

	if (!m_engine->generatePreview(seed, progressCb)) {
		const std::string errorText = m_engine->getLastError();
		if (!IsUserCancelledError(errorText)) {
			wxMessageBox(wxstr(errorText), "Preview Error", wxOK | wxICON_ERROR, this);
		}
		UpdateStatsText();
		return;
	}

	m_seedInput->SetValue(wxString::Format("%llu", m_engine->getPreviewState().seed));
	UpdateStatsText();
	g_gui.RefreshView();
}

void AreaCreatureSpawnDialog::OnReroll(wxCommandEvent&) {
	if (!m_engine) {
		return;
	}
	if (!m_engine->getPreviewState().isValid) {
		wxCommandEvent dummy;
		OnPreview(dummy);
		return;
	}
	wxProgressDialog progress(
		"Auto Creature Spawn",
		"Rerolling preview...",
		100,
		this,
		wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME | wxPD_AUTO_HIDE);
	auto progressCb = [&progress](const std::string& message) -> bool {
		bool keepGoing = true;
		progress.Pulse(wxstr(message), &keepGoing);
		return keepGoing;
	};

	if (!m_engine->rerollPreview(progressCb)) {
		const std::string errorText = m_engine->getLastError();
		if (!IsUserCancelledError(errorText)) {
			wxMessageBox(wxstr(errorText), "Reroll Error", wxOK | wxICON_ERROR, this);
		}
		UpdateStatsText();
		return;
	}
	m_seedInput->SetValue(wxString::Format("%llu", m_engine->getPreviewState().seed));
	UpdateStatsText();
	g_gui.RefreshView();
}

void AreaCreatureSpawnDialog::OnApply(wxCommandEvent&) {
	if (!m_engine) {
		return;
	}
	wxProgressDialog progress(
		"Auto Creature Spawn",
		"Applying creatures to map...",
		100,
		this,
		wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME | wxPD_AUTO_HIDE);
	auto progressCb = [&progress](const std::string& message) -> bool {
		bool keepGoing = true;
		progress.Pulse(wxstr(message), &keepGoing);
		return keepGoing;
	};

	if (!m_engine->applyPreview(progressCb)) {
		const std::string errorText = m_engine->getLastError();
		if (!IsUserCancelledError(errorText)) {
			wxMessageBox(wxstr(errorText), "Apply Error", wxOK | wxICON_ERROR, this);
		}
		UpdateStatsText();
		return;
	}
	UpdateStatsText();
	g_gui.RefreshView();
}

void AreaCreatureSpawnDialog::OnRerollApply(wxCommandEvent&) {
	if (!m_engine) {
		return;
	}

	if (m_engine->hasLastApplied()) {
		m_engine->removeLastApplied();
	}

	BuildPresetFromUI();
	m_engine->setArea(m_area);
	m_engine->setPreset(m_preset);
	wxProgressDialog previewProgress(
		"Auto Creature Spawn",
		"Rerolling preview...",
		100,
		this,
		wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME | wxPD_AUTO_HIDE);
	auto previewProgressCb = [&previewProgress](const std::string& message) -> bool {
		bool keepGoing = true;
		previewProgress.Pulse(wxstr(message), &keepGoing);
		return keepGoing;
	};
	if (!m_engine->generatePreview(0, previewProgressCb)) {
		const std::string errorText = m_engine->getLastError();
		if (!IsUserCancelledError(errorText)) {
			wxMessageBox(wxstr(errorText), "Reroll Error", wxOK | wxICON_ERROR, this);
		}
		UpdateStatsText();
		return;
	}

	m_seedInput->SetValue(wxString::Format("%llu", m_engine->getPreviewState().seed));
	wxProgressDialog applyProgress(
		"Auto Creature Spawn",
		"Applying creatures to map...",
		100,
		this,
		wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_REMAINING_TIME | wxPD_AUTO_HIDE);
	auto applyProgressCb = [&applyProgress](const std::string& message) -> bool {
		bool keepGoing = true;
		applyProgress.Pulse(wxstr(message), &keepGoing);
		return keepGoing;
	};
	if (!m_engine->applyPreview(applyProgressCb)) {
		const std::string errorText = m_engine->getLastError();
		if (!IsUserCancelledError(errorText)) {
			wxMessageBox(wxstr(errorText), "Apply Error", wxOK | wxICON_ERROR, this);
		}
	}
	UpdateStatsText();
	g_gui.RefreshView();
}

void AreaCreatureSpawnDialog::OnRemoveLastApply(wxCommandEvent&) {
	if (!m_engine) {
		return;
	}
	if (!m_engine->removeLastApplied()) {
		wxMessageBox(wxstr(m_engine->getLastError()), "Remove Error", wxOK | wxICON_ERROR, this);
	}
	UpdateStatsText();
	g_gui.RefreshView();
}

void AreaCreatureSpawnDialog::OnPresetSelected(wxCommandEvent&) {
	int sel = m_presetChoice->GetSelection();
	if (sel <= 0) {
		return;
	}
	wxString name = m_presetChoice->GetString(sel);
	const auto* preset = AreaCreatureSpawn::PresetManager::getInstance().getPreset(name.ToStdString());
	if (preset) {
		LoadPresetToUI(*preset);
	}
}

void AreaCreatureSpawnDialog::OnSavePreset(wxCommandEvent&) {
	wxString name = m_presetNameInput->GetValue().Trim().Trim(false);
	if (name.IsEmpty()) {
		wxMessageBox("Enter a preset name.", "Auto Creature Spawn", wxOK | wxICON_WARNING, this);
		return;
	}

	BuildPresetFromUI();
	m_preset.name = name.ToStdString();

	std::string error;
	if (!m_preset.validate(error)) {
		wxMessageBox(wxstr(error), "Invalid Preset", wxOK | wxICON_ERROR, this);
		return;
	}

	auto& manager = AreaCreatureSpawn::PresetManager::getInstance();
	if (manager.getPreset(m_preset.name)) {
		int ret = wxMessageBox(wxString::Format("Preset '%s' exists. Overwrite?", name),
		                       "Confirm Overwrite", wxYES_NO | wxICON_QUESTION, this);
		if (ret != wxYES) {
			return;
		}
		manager.removePreset(m_preset.name);
	}

	if (!manager.addPreset(m_preset) || !manager.savePresets()) {
		wxMessageBox("Failed to save preset.", "Auto Creature Spawn", wxOK | wxICON_ERROR, this);
		return;
	}

	UpdatePresetList();
	int idx = m_presetChoice->FindString(name);
	if (idx != wxNOT_FOUND) {
		m_presetChoice->SetSelection(idx);
	}
}

void AreaCreatureSpawnDialog::OnRefreshPresets(wxCommandEvent&) {
	AreaCreatureSpawn::PresetManager::getInstance().loadPresets();
	UpdatePresetList();
}

void AreaCreatureSpawnDialog::OnDeletePreset(wxCommandEvent&) {
	int sel = m_presetChoice->GetSelection();
	if (sel <= 0) {
		wxMessageBox("Select a preset first.", "Auto Creature Spawn", wxOK | wxICON_INFORMATION, this);
		return;
	}
	wxString name = m_presetChoice->GetString(sel);
	int ret = wxMessageBox(wxString::Format("Delete preset '%s'?", name),
	                       "Confirm Delete", wxYES_NO | wxICON_WARNING, this);
	if (ret != wxYES) {
		return;
	}

	auto& manager = AreaCreatureSpawn::PresetManager::getInstance();
	if (!manager.removePreset(name.ToStdString()) || !manager.savePresets()) {
		wxMessageBox("Failed to delete preset.", "Auto Creature Spawn", wxOK | wxICON_ERROR, this);
		return;
	}
	UpdatePresetList();
}

void AreaCreatureSpawnDialog::OnExportPreset(wxCommandEvent&) {
	wxString name = m_presetNameInput->GetValue().Trim().Trim(false);
	if (name.IsEmpty()) {
		name = "creature_spawn_preset";
	}

	wxFileDialog saveDialog(this, "Export Creature Spawn Preset", "", name + ".xml",
	                        "XML files (*.xml)|*.xml|All files (*.*)|*.*",
	                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
	if (saveDialog.ShowModal() == wxID_CANCEL) {
		return;
	}

	BuildPresetFromUI();
	m_preset.name = name.ToStdString();
	if (!m_preset.saveToFile(saveDialog.GetPath().ToStdString())) {
		wxMessageBox("Failed to export preset.", "Auto Creature Spawn", wxOK | wxICON_ERROR, this);
		return;
	}
}

void AreaCreatureSpawnDialog::OnImportPreset(wxCommandEvent&) {
	wxFileDialog openDialog(this, "Import Creature Spawn Preset", "", "",
	                        "XML files (*.xml)|*.xml|All files (*.*)|*.*",
	                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
	if (openDialog.ShowModal() == wxID_CANCEL) {
		return;
	}

	AreaCreatureSpawn::SpawnPreset imported;
	if (!imported.loadFromFile(openDialog.GetPath().ToStdString())) {
		wxMessageBox("Invalid preset file.", "Auto Creature Spawn", wxOK | wxICON_ERROR, this);
		return;
	}

	LoadPresetToUI(imported);
	int ret = wxMessageBox(wxString::Format("Preset '%s' imported. Save to preset list?", wxstr(imported.name)),
	                       "Save Imported Preset", wxYES_NO | wxICON_QUESTION, this);
	if (ret != wxYES) {
		return;
	}

	auto& manager = AreaCreatureSpawn::PresetManager::getInstance();
	if (manager.getPreset(imported.name) != nullptr) {
		int overwrite = wxMessageBox(wxString::Format("Preset '%s' already exists. Overwrite?", wxstr(imported.name)),
		                             "Confirm Overwrite", wxYES_NO | wxICON_QUESTION, this);
		if (overwrite != wxYES) {
			return;
		}
		manager.removePreset(imported.name);
	}
	if (!manager.addPreset(imported) || !manager.savePresets()) {
		wxMessageBox("Failed to save imported preset.", "Auto Creature Spawn", wxOK | wxICON_ERROR, this);
		return;
	}
	UpdatePresetList();
	int idx = m_presetChoice->FindString(wxstr(imported.name));
	if (idx != wxNOT_FOUND) {
		m_presetChoice->SetSelection(idx);
	}
}

void AreaCreatureSpawnDialog::OnClose(wxCloseEvent& event) {
	g_gui.CancelRectanglePick();
	Hide();
	event.Veto();
}
