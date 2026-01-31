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
#include "area_decoration_dialog.h"
#include "editor.h"
#include "gui.h"
#include "map.h"
#include "tile.h"
#include "selection.h"
#include "find_item_window.h"
#include "graphics.h"
#include "items.h"
#include "brush.h"
#include "doodad_brush.h"
#include "raw_brush.h"
#include "materials.h"
#include "tileset.h"
#include <wx/dcbuffer.h>
#include <algorithm>
#include <climits>

namespace {
	enum {
		ID_AREA_TYPE_CHOICE = wxID_HIGHEST + 5000,
		ID_SELECT_FROM_MAP,
		ID_USE_SELECTION,

		ID_ADD_RULE,
		ID_EDIT_RULE,
		ID_REMOVE_RULE,
		ID_RULES_LIST,

		ID_DISTRIBUTION_CHOICE,

		ID_PREVIEW,
		ID_REROLL,
		ID_APPLY,
		ID_REVERT,
		ID_REMOVE_LAST_APPLY,

		ID_FLOOR_TYPE_SINGLE,
		ID_FLOOR_TYPE_RANGE,
		ID_ADD_ITEM,
		ID_EDIT_ITEM,
		ID_REMOVE_ITEM,
		ID_CLEAR_ITEMS,
		ID_BROWSE_ITEM,
		ID_ADD_CLUSTER,
		ID_REPLACE_CLUSTER,
		ID_ITEMS_LIST,
		ID_ADD_DOODAD,
		ID_DOODAD_LIST,
		ID_DOODAD_SEARCH,
		ID_DOODAD_PREV_PAGE,
		ID_DOODAD_NEXT_PAGE,
		ID_FLOOR_PREVIEW,
		ID_SINGLE_FLOOR_SPIN,
		ID_FROM_FLOOR_SPIN,
		ID_TO_FLOOR_SPIN,
		ID_RULE_OK,
		ID_RULE_CANCEL,

		// Preset management IDs
		ID_PRESET_CHOICE,
		ID_SAVE_PRESET,
		ID_DELETE_PRESET,
		ID_EXPORT_PRESET,
		ID_IMPORT_PRESET
	};

	// Constants for icon sizes
	const int ITEM_ICON_SIZE = 32;
}

//=============================================================================
// ItemListDropTarget Implementation
//=============================================================================

ItemListDropTarget::ItemListDropTarget(FloorRuleEditDialog* dialog)
	: m_dialog(dialog)
{
}

bool ItemListDropTarget::OnDropText(wxCoord x, wxCoord y, const wxString& data) {
	if (!m_dialog) return false;

	// Handle ITEM_ID:xxx format (from palette drag)
	if (data.StartsWith("ITEM_ID:")) {
		wxString idStr = data.Mid(8);
		long itemId = 0;
		if (idStr.ToLong(&itemId) && itemId > 0 && itemId <= 0xFFFF) {
			m_dialog->AddItemById(static_cast<uint16_t>(itemId));
			return true;
		}
	}

	// Handle BRUSH:xxx format (brush name)
	if (data.StartsWith("BRUSH:")) {
		wxString brushName = data.Mid(6);
		Brush* brush = g_brushes.getBrush(brushName.ToStdString());
		if (brush) {
			if (brush->isDoodad()) {
				m_dialog->AddItemsFromDoodad(brush->asDoodad());
				return true;
			} else if (brush->isRaw()) {
				RAWBrush* raw = brush->asRaw();
				if (raw) {
					m_dialog->AddItemById(raw->getItemID());
					return true;
				}
			}
		}
	}

	// Try to parse as plain number
	long itemId = 0;
	if (data.ToLong(&itemId) && itemId > 0 && itemId <= 0xFFFF) {
		m_dialog->AddItemById(static_cast<uint16_t>(itemId));
		return true;
	}

	return false;
}

//=============================================================================
// FloorRuleEditDialog
//=============================================================================

wxBEGIN_EVENT_TABLE(FloorRuleEditDialog, wxDialog)
	EVT_RADIOBUTTON(ID_FLOOR_TYPE_SINGLE, FloorRuleEditDialog::OnFloorTypeChanged)
	EVT_RADIOBUTTON(ID_FLOOR_TYPE_RANGE, FloorRuleEditDialog::OnFloorTypeChanged)
	EVT_BUTTON(ID_ADD_ITEM, FloorRuleEditDialog::OnAddItem)
	EVT_BUTTON(ID_EDIT_ITEM, FloorRuleEditDialog::OnEditItem)
	EVT_BUTTON(ID_REPLACE_CLUSTER, FloorRuleEditDialog::OnReplaceClusterFromSelection)
	EVT_BUTTON(ID_REMOVE_ITEM, FloorRuleEditDialog::OnRemoveItem)
	EVT_BUTTON(ID_CLEAR_ITEMS, FloorRuleEditDialog::OnClearItems)
	EVT_BUTTON(ID_BROWSE_ITEM, FloorRuleEditDialog::OnBrowseItem)
	EVT_BUTTON(ID_ADD_DOODAD, FloorRuleEditDialog::OnAddDoodad)
	EVT_BUTTON(ID_ADD_CLUSTER, FloorRuleEditDialog::OnAddClusterFromSelection)
	EVT_LIST_ITEM_SELECTED(ID_ITEMS_LIST, FloorRuleEditDialog::OnItemsListSelected)
	EVT_LIST_ITEM_ACTIVATED(ID_ITEMS_LIST, FloorRuleEditDialog::OnItemsListActivated)
	EVT_LIST_ITEM_ACTIVATED(ID_DOODAD_LIST, FloorRuleEditDialog::OnDoodadDoubleClick)
	EVT_TEXT(ID_DOODAD_SEARCH, FloorRuleEditDialog::OnDoodadSearch)
	EVT_BUTTON(ID_DOODAD_PREV_PAGE, FloorRuleEditDialog::OnPrevPage)
	EVT_BUTTON(ID_DOODAD_NEXT_PAGE, FloorRuleEditDialog::OnNextPage)
	EVT_SPINCTRL(ID_SINGLE_FLOOR_SPIN, FloorRuleEditDialog::OnFloorIdChanged)
	EVT_SPINCTRL(ID_FROM_FLOOR_SPIN, FloorRuleEditDialog::OnFloorIdChanged)
	EVT_SPINCTRL(ID_TO_FLOOR_SPIN, FloorRuleEditDialog::OnFloorIdChanged)
	EVT_BUTTON(ID_RULE_OK, FloorRuleEditDialog::OnOK)
	EVT_BUTTON(ID_RULE_CANCEL, FloorRuleEditDialog::OnCancel)
	EVT_CLOSE(FloorRuleEditDialog::OnClose)
wxEND_EVENT_TABLE()

FloorRuleEditDialog::FloorRuleEditDialog(wxWindow* parent, AreaDecoration::FloorRule& rule,
                                         std::function<void(bool)> onCloseCallback)
	: wxDialog(parent, wxID_ANY, "Edit Floor Rule", wxDefaultPosition, wxSize(900, 550),
	           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)  // Non-modal friendly style
	, m_rule(rule)
	, m_onCloseCallback(onCloseCallback)
	, m_floorPreviewPanel(nullptr)
	, m_itemsImageList(nullptr)
	, m_doodadSearchCtrl(nullptr)
	, m_doodadListCtrl(nullptr)
	, m_doodadImageList(nullptr)
	, m_prevPageBtn(nullptr)
	, m_nextPageBtn(nullptr)
	, m_pageInfoText(nullptr)
	, m_currentPage(0)
{
	CreateControls();
	LoadDoodadList();
	LoadRuleData();
	Centre();

	// Set drop target for items list
	m_itemsListCtrl->SetDropTarget(new ItemListDropTarget(this));
}

FloorRuleEditDialog::~FloorRuleEditDialog() {
	if (m_doodadImageList) {
		delete m_doodadImageList;
		m_doodadImageList = nullptr;
	}
}

void FloorRuleEditDialog::CreateControls() {
	wxBoxSizer* mainSizer = newd wxBoxSizer(wxVERTICAL);

	// Two-column layout
	wxBoxSizer* columnsSizer = newd wxBoxSizer(wxHORIZONTAL);

	// ==================== LEFT COLUMN ====================
	wxBoxSizer* leftColumn = newd wxBoxSizer(wxVERTICAL);

	// Name
	wxStaticBoxSizer* nameBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Rule Name");
	m_nameInput = newd wxTextCtrl(this, wxID_ANY, "");
	nameBox->Add(m_nameInput, 0, wxALL | wxEXPAND, 5);
	leftColumn->Add(nameBox, 0, wxALL | wxEXPAND, 5);

	// Floor Selection with Preview
	wxStaticBoxSizer* floorBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Floor Selection");

	// Floor preview at top
	wxBoxSizer* previewSizer = newd wxBoxSizer(wxHORIZONTAL);
	previewSizer->Add(newd wxStaticText(this, wxID_ANY, "Preview:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
	m_floorPreviewPanel = newd wxPanel(this, ID_FLOOR_PREVIEW, wxDefaultPosition, wxSize(64, 64));
	m_floorPreviewPanel->SetBackgroundStyle(wxBG_STYLE_PAINT);
	m_floorPreviewPanel->Bind(wxEVT_PAINT, &FloorRuleEditDialog::OnPaintFloorPreview, this);
	previewSizer->Add(m_floorPreviewPanel, 0);
	floorBox->Add(previewSizer, 0, wxALL, 5);

	// Single floor option
	m_singleFloorRadio = newd wxRadioButton(this, ID_FLOOR_TYPE_SINGLE, "Single Floor ID", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	wxBoxSizer* singleSizer = newd wxBoxSizer(wxHORIZONTAL);
	singleSizer->Add(m_singleFloorRadio, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
	m_singleFloorSpin = newd wxSpinCtrl(this, ID_SINGLE_FLOOR_SPIN, "0", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535, 0);
	singleSizer->Add(m_singleFloorSpin, 0, wxALIGN_CENTER_VERTICAL);
	floorBox->Add(singleSizer, 0, wxALL, 5);

	// Floor range option
	m_floorRangeRadio = newd wxRadioButton(this, ID_FLOOR_TYPE_RANGE, "Floor Range");
	wxBoxSizer* rangeSizer = newd wxBoxSizer(wxHORIZONTAL);
	rangeSizer->Add(m_floorRangeRadio, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
	rangeSizer->Add(newd wxStaticText(this, wxID_ANY, "From:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 3);
	m_fromFloorSpin = newd wxSpinCtrl(this, ID_FROM_FLOOR_SPIN, "0", wxDefaultPosition, wxSize(70, -1), wxSP_ARROW_KEYS, 0, 65535, 0);
	rangeSizer->Add(m_fromFloorSpin, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	rangeSizer->Add(newd wxStaticText(this, wxID_ANY, "To:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 3);
	m_toFloorSpin = newd wxSpinCtrl(this, ID_TO_FLOOR_SPIN, "0", wxDefaultPosition, wxSize(70, -1), wxSP_ARROW_KEYS, 0, 65535, 0);
	rangeSizer->Add(m_toFloorSpin, 0, wxALIGN_CENTER_VERTICAL);
	floorBox->Add(rangeSizer, 0, wxALL, 5);

	leftColumn->Add(floorBox, 0, wxALL | wxEXPAND, 5);

	// Settings
	wxStaticBoxSizer* settingsBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Settings");
	wxFlexGridSizer* settingsGrid = newd wxFlexGridSizer(2, 5, 10);

	settingsGrid->Add(newd wxStaticText(this, wxID_ANY, "Density (%):"), 0, wxALIGN_CENTER_VERTICAL);
	m_densitySpin = newd wxSpinCtrl(this, wxID_ANY, "30", wxDefaultPosition, wxSize(70, -1), wxSP_ARROW_KEYS, 1, 100, 30);
	settingsGrid->Add(m_densitySpin, 0);

	settingsGrid->Add(newd wxStaticText(this, wxID_ANY, "Max Placements:"), 0, wxALIGN_CENTER_VERTICAL);
	m_maxPlacementsSpin = newd wxSpinCtrl(this, wxID_ANY, "-1", wxDefaultPosition, wxSize(70, -1), wxSP_ARROW_KEYS, -1, 10000, -1);
	settingsGrid->Add(m_maxPlacementsSpin, 0);

	settingsGrid->Add(newd wxStaticText(this, wxID_ANY, "Priority:"), 0, wxALIGN_CENTER_VERTICAL);
	m_prioritySpin = newd wxSpinCtrl(this, wxID_ANY, "0", wxDefaultPosition, wxSize(70, -1), wxSP_ARROW_KEYS, 0, 100, 0);
	settingsGrid->Add(m_prioritySpin, 0);

	settingsBox->Add(settingsGrid, 0, wxALL, 5);
	settingsBox->Add(newd wxStaticText(this, wxID_ANY, "(-1 = unlimited placements)"), 0, wxLEFT | wxBOTTOM, 5);
	leftColumn->Add(settingsBox, 0, wxALL | wxEXPAND, 5);

	// Add doodad browser to left column
	wxStaticBoxSizer* doodadBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Doodad Browser (double-click to add)");

	// Search row
	wxBoxSizer* searchSizer = newd wxBoxSizer(wxHORIZONTAL);
	m_doodadSearchCtrl = newd wxTextCtrl(this, ID_DOODAD_SEARCH, "", wxDefaultPosition, wxDefaultSize);
	m_doodadSearchCtrl->SetHint("Search doodads...");
	searchSizer->Add(m_doodadSearchCtrl, 1, wxRIGHT, 5);
	wxButton* addDoodadBtn = newd wxButton(this, ID_ADD_DOODAD, "Add", wxDefaultPosition, wxSize(50, -1));
	addDoodadBtn->SetToolTip("Add doodad entries (singles + composites)");
	searchSizer->Add(addDoodadBtn, 0);
	doodadBox->Add(searchSizer, 0, wxALL | wxEXPAND, 5);

	// Doodad list
	m_doodadImageList = newd wxImageList(ITEM_ICON_SIZE, ITEM_ICON_SIZE, true);
	m_doodadListCtrl = newd wxListCtrl(this, ID_DOODAD_LIST, wxDefaultPosition, wxSize(-1, 100),
	                                    wxLC_REPORT | wxLC_SINGLE_SEL);
	m_doodadListCtrl->SetImageList(m_doodadImageList, wxIMAGE_LIST_SMALL);
	m_doodadListCtrl->InsertColumn(0, "", wxLIST_FORMAT_LEFT, 40);
	m_doodadListCtrl->InsertColumn(1, "Name", wxLIST_FORMAT_LEFT, 150);
	m_doodadListCtrl->InsertColumn(2, "#", wxLIST_FORMAT_LEFT, 35);
	doodadBox->Add(m_doodadListCtrl, 1, wxLEFT | wxRIGHT | wxEXPAND, 5);

	// Pagination
	wxBoxSizer* pageSizer = newd wxBoxSizer(wxHORIZONTAL);
	m_prevPageBtn = newd wxButton(this, ID_DOODAD_PREV_PAGE, "<", wxDefaultPosition, wxSize(30, -1));
	m_pageInfoText = newd wxStaticText(this, wxID_ANY, "1/1", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
	m_nextPageBtn = newd wxButton(this, ID_DOODAD_NEXT_PAGE, ">", wxDefaultPosition, wxSize(30, -1));
	pageSizer->Add(m_prevPageBtn, 0);
	pageSizer->Add(m_pageInfoText, 1, wxALIGN_CENTER_VERTICAL);
	pageSizer->Add(m_nextPageBtn, 0);
	doodadBox->Add(pageSizer, 0, wxALL | wxEXPAND, 5);

	leftColumn->Add(doodadBox, 1, wxALL | wxEXPAND, 5);

	columnsSizer->Add(leftColumn, 0, wxEXPAND);

	// ==================== RIGHT COLUMN ====================
	wxBoxSizer* rightColumn = newd wxBoxSizer(wxVERTICAL);

	// Items list (main focus - larger area)
	wxStaticBoxSizer* itemsBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Items (drag & drop supported)");

	// Create image list for icons
	m_itemsImageList = newd wxImageList(ITEM_ICON_SIZE, ITEM_ICON_SIZE, true);
	m_itemsListCtrl = newd wxListCtrl(this, ID_ITEMS_LIST, wxDefaultPosition, wxSize(350, -1), wxLC_REPORT | wxLC_SINGLE_SEL);
	m_itemsListCtrl->SetImageList(m_itemsImageList, wxIMAGE_LIST_SMALL);
	m_itemsListCtrl->InsertColumn(0, "", wxLIST_FORMAT_LEFT, 40);
	m_itemsListCtrl->InsertColumn(1, "Item ID", wxLIST_FORMAT_LEFT, 80);
	m_itemsListCtrl->InsertColumn(2, "Weight", wxLIST_FORMAT_LEFT, 60);
	m_itemsListCtrl->InsertColumn(3, "Name", wxLIST_FORMAT_LEFT, 150);
	itemsBox->Add(m_itemsListCtrl, 1, wxALL | wxEXPAND, 5);

	// Item controls row
	wxBoxSizer* itemControlsSizer = newd wxBoxSizer(wxHORIZONTAL);
	itemControlsSizer->Add(newd wxStaticText(this, wxID_ANY, "ID:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 3);
	m_newItemIdSpin = newd wxSpinCtrl(this, wxID_ANY, "0", wxDefaultPosition, wxSize(70, -1), wxSP_ARROW_KEYS, 0, 65535, 0);
	itemControlsSizer->Add(m_newItemIdSpin, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

	wxButton* browseBtn = newd wxButton(this, ID_BROWSE_ITEM, "...", wxDefaultPosition, wxSize(25, -1));
	browseBtn->SetToolTip("Browse for item");
	itemControlsSizer->Add(browseBtn, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

	itemControlsSizer->Add(newd wxStaticText(this, wxID_ANY, "Weight:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 3);
	m_newItemWeightSpin = newd wxSpinCtrl(this, wxID_ANY, "100", wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 1, 1000, 100);
	itemControlsSizer->Add(m_newItemWeightSpin, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

	wxButton* addBtn = newd wxButton(this, ID_ADD_ITEM, "Add", wxDefaultPosition, wxSize(50, -1));
	m_editItemBtn = newd wxButton(this, ID_EDIT_ITEM, "Edit", wxDefaultPosition, wxSize(50, -1));
	wxButton* removeBtn = newd wxButton(this, ID_REMOVE_ITEM, "Remove", wxDefaultPosition, wxSize(60, -1));
	wxButton* clearBtn = newd wxButton(this, ID_CLEAR_ITEMS, "Clear All", wxDefaultPosition, wxSize(70, -1));
	itemControlsSizer->Add(addBtn, 0, wxRIGHT, 5);
	itemControlsSizer->Add(m_editItemBtn, 0, wxRIGHT, 5);
	itemControlsSizer->Add(removeBtn, 0);
	itemControlsSizer->Add(clearBtn, 0, wxLEFT, 5);

	m_editItemBtn->Enable(false);
	m_editItemBtn->SetToolTip("Apply the fields above to the selected item/cluster");

	itemsBox->Add(itemControlsSizer, 0, wxALL, 5);
	rightColumn->Add(itemsBox, 1, wxALL | wxEXPAND, 5);

	wxStaticBoxSizer* clusterBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Cluster From Selection");
	wxFlexGridSizer* clusterGrid = newd wxFlexGridSizer(3, 5, 10);

	clusterGrid->Add(newd wxStaticText(this, wxID_ANY, "Count:"), 0, wxALIGN_CENTER_VERTICAL);
	m_clusterCountSpin = newd wxSpinCtrl(this, wxID_ANY, "3", wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 1, 100, 3);
	clusterGrid->Add(m_clusterCountSpin, 0);

	clusterGrid->Add(newd wxStaticText(this, wxID_ANY, "Radius:"), 0, wxALIGN_CENTER_VERTICAL);
	m_clusterRadiusSpin = newd wxSpinCtrl(this, wxID_ANY, "3", wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 50, 3);
	clusterGrid->Add(m_clusterRadiusSpin, 0);

	clusterGrid->Add(newd wxStaticText(this, wxID_ANY, "Min Dist:"), 0, wxALIGN_CENTER_VERTICAL);
	m_clusterMinDistanceSpin = newd wxSpinCtrl(this, wxID_ANY, "2", wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 0, 50, 2);
	clusterGrid->Add(m_clusterMinDistanceSpin, 0);

	clusterBox->Add(clusterGrid, 0, wxALL, 5);
	wxBoxSizer* clusterBtnSizer = newd wxBoxSizer(wxHORIZONTAL);
	wxButton* addClusterBtn = newd wxButton(this, ID_ADD_CLUSTER, "Add Cluster From Selection");
	m_replaceClusterBtn = newd wxButton(this, ID_REPLACE_CLUSTER, "Replace Selected Cluster");
	m_replaceClusterBtn->Enable(false);
	clusterBtnSizer->Add(addClusterBtn, 0, wxRIGHT, 5);
	clusterBtnSizer->Add(m_replaceClusterBtn, 0);
	clusterBox->Add(clusterBtnSizer, 0, wxALL, 5);
	rightColumn->Add(clusterBox, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	columnsSizer->Add(rightColumn, 1, wxEXPAND);

	mainSizer->Add(columnsSizer, 1, wxEXPAND);

	// Buttons at bottom
	wxBoxSizer* btnSizer = newd wxBoxSizer(wxHORIZONTAL);
	btnSizer->AddStretchSpacer();
	wxButton* okBtn = newd wxButton(this, ID_RULE_OK, "OK");
	wxButton* cancelBtn = newd wxButton(this, ID_RULE_CANCEL, "Cancel");
	btnSizer->Add(okBtn, 0, wxRIGHT, 5);
	btnSizer->Add(cancelBtn, 0);
	mainSizer->Add(btnSizer, 0, wxALL | wxEXPAND, 10);

	SetSizer(mainSizer);
}

void FloorRuleEditDialog::LoadRuleData() {
	m_nameInput->SetValue(m_rule.name);

	if (m_rule.isRangeRule()) {
		m_floorRangeRadio->SetValue(true);
		m_fromFloorSpin->SetValue(m_rule.fromFloorId);
		m_toFloorSpin->SetValue(m_rule.toFloorId);
		m_singleFloorSpin->Enable(false);
	} else {
		m_singleFloorRadio->SetValue(true);
		m_singleFloorSpin->SetValue(m_rule.floorId);
		m_fromFloorSpin->Enable(false);
		m_toFloorSpin->Enable(false);
	}

	m_densitySpin->SetValue(static_cast<int>(m_rule.density * 100));
	m_maxPlacementsSpin->SetValue(m_rule.maxPlacements);
	m_prioritySpin->SetValue(m_rule.priority);

	UpdateItemsList();
	UpdateFloorPreview();
}

void FloorRuleEditDialog::UpdateItemsList() {
	m_itemsListCtrl->DeleteAllItems();
	m_itemsImageList->RemoveAll();

	for (size_t i = 0; i < m_rule.items.size(); ++i) {
		const auto& item = m_rule.items[i];

		uint16_t iconItemId = item.isCompositeEntry() ? item.getRepresentativeItemId() : item.itemId;

		// Get item bitmap and add to image list
		wxBitmap bmp = GetItemBitmap(iconItemId, ITEM_ICON_SIZE);
		int imgIdx = m_itemsImageList->Add(bmp);

		// Get item name and display ID
		wxString itemName = "Unknown";
		wxString itemIdText;
		if (item.isCompositeEntry()) {
			if (item.isClusterEntry()) {
				itemIdText = "Cluster";
				itemName = wxString::Format("Cluster (%zu tiles, %zu items) x%d",
					item.getCompositeTileCount(),
					item.getCompositeItemCount(),
					item.clusterCount);
			} else {
				itemIdText = "Composite";
				itemName = wxString::Format("Composite (%zu tiles, %zu items)",
					item.getCompositeTileCount(),
					item.getCompositeItemCount());
			}
		} else {
			itemIdText = wxString::Format("%d", item.itemId);
			const ItemType& iType = g_items.getItemType(item.itemId);
			if (iType.id != 0) {
				itemName = wxstr(iType.name);
				if (itemName.IsEmpty()) {
					itemName = wxString::Format("Item #%d", item.itemId);
				}
			}
		}

		// Insert item with icon
		long idx = m_itemsListCtrl->InsertItem(i, "", imgIdx);
		m_itemsListCtrl->SetItem(idx, 1, itemIdText);
		m_itemsListCtrl->SetItem(idx, 2, wxString::Format("%d", item.weight));
		m_itemsListCtrl->SetItem(idx, 3, itemName);
	}

	if (m_editItemBtn) {
		m_editItemBtn->Enable(false);
	}
	if (m_replaceClusterBtn) {
		m_replaceClusterBtn->Enable(false);
	}
}

wxBitmap FloorRuleEditDialog::GetItemBitmap(uint16_t itemId, int size) {
	wxBitmap bmp(size, size, 32);
	wxMemoryDC dc(bmp);

	// Fill with dark background
	dc.SetBackground(wxBrush(wxColour(0x0C, 0x14, 0x2A)));
	dc.Clear();

	// Get the ItemType to find the client sprite ID
	const ItemType& itemType = g_items.getItemType(itemId);
	Sprite* spr = nullptr;
	if (itemType.id != 0) {
		spr = g_gui.gfx.getSprite(itemType.clientID);
	}

	if (spr) {
		spr->DrawTo(&dc, SPRITE_SIZE_32x32, 0, 0, size, size);
	} else {
		// Draw placeholder
		dc.SetBrush(wxBrush(wxColour(100, 100, 100)));
		dc.SetPen(*wxTRANSPARENT_PEN);
		dc.DrawRectangle(2, 2, size - 4, size - 4);
		dc.SetTextForeground(*wxWHITE);
		dc.DrawText("?", size / 2 - 4, size / 2 - 8);
	}

	dc.SelectObject(wxNullBitmap);
	return bmp;
}

void FloorRuleEditDialog::UpdateFloorPreview() {
	if (m_floorPreviewPanel) {
		m_floorPreviewPanel->Refresh();
	}
}

void FloorRuleEditDialog::OnPaintFloorPreview(wxPaintEvent& event) {
	wxBufferedPaintDC dc(m_floorPreviewPanel);

	// Get panel size
	wxSize size = m_floorPreviewPanel->GetSize();

	// Fill with dark background
	dc.SetBackground(wxBrush(wxColour(0x0C, 0x14, 0x2A)));
	dc.Clear();

	// Determine which floor ID to preview
	uint16_t floorId = 0;
	if (m_singleFloorRadio->GetValue()) {
		floorId = m_singleFloorSpin->GetValue();
	} else {
		floorId = m_fromFloorSpin->GetValue();
	}

	if (floorId > 0) {
		// Get the ItemType to find the client sprite ID
		const ItemType& itemType = g_items.getItemType(floorId);
		Sprite* spr = nullptr;
		if (itemType.id != 0) {
			spr = g_gui.gfx.getSprite(itemType.clientID);
		}
		if (spr) {
			// Draw centered
			int drawSize = std::min(size.GetWidth(), size.GetHeight()) - 4;
			int x = (size.GetWidth() - drawSize) / 2;
			int y = (size.GetHeight() - drawSize) / 2;
			spr->DrawTo(&dc, SPRITE_SIZE_32x32, x, y, drawSize, drawSize);
		}
	}

	// Draw border
	dc.SetPen(wxPen(wxColour(80, 80, 120), 1));
	dc.SetBrush(*wxTRANSPARENT_BRUSH);
	dc.DrawRectangle(0, 0, size.GetWidth(), size.GetHeight());
}

void FloorRuleEditDialog::LoadDoodadList() {
	m_allDoodads.clear();

	// Get all doodad brushes from materials
	for (auto& pair : g_materials.tilesets) {
		Tileset* tileset = pair.second;
		if (!tileset) continue;

		TilesetCategory* category = tileset->getCategory(TILESET_DOODAD);
		if (!category) continue;

		for (Brush* brush : category->brushlist) {
			if (brush && brush->isDoodad()) {
				DoodadBrush* doodad = brush->asDoodad();
				if (doodad) {
					m_allDoodads.push_back(doodad);
				}
			}
		}
	}

	// Sort by name for easier browsing
	std::sort(m_allDoodads.begin(), m_allDoodads.end(),
		[](DoodadBrush* a, DoodadBrush* b) {
			return a->getName() < b->getName();
		});

	// Initialize filtered list with all doodads
	m_filteredDoodads = m_allDoodads;
	m_currentPage = 0;

	UpdateDoodadListDisplay();
}

void FloorRuleEditDialog::FilterDoodads(const wxString& filter) {
	m_filteredDoodads.clear();
	wxString lowerFilter = filter.Lower();

	if (lowerFilter.IsEmpty()) {
		m_filteredDoodads = m_allDoodads;
	} else {
		for (DoodadBrush* doodad : m_allDoodads) {
			wxString name = wxstr(doodad->getName()).Lower();
			if (name.Contains(lowerFilter)) {
				m_filteredDoodads.push_back(doodad);
			}
		}
	}

	m_currentPage = 0;
	UpdateDoodadListDisplay();
}

void FloorRuleEditDialog::UpdateDoodadListDisplay() {
	if (!m_doodadListCtrl || !m_doodadImageList) return;

	m_doodadListCtrl->DeleteAllItems();
	m_doodadImageList->RemoveAll();

	int totalDoodads = static_cast<int>(m_filteredDoodads.size());
	int totalPages = (totalDoodads + DOODADS_PER_PAGE - 1) / DOODADS_PER_PAGE;
	if (totalPages == 0) totalPages = 1;

	// Clamp current page
	if (m_currentPage >= totalPages) m_currentPage = totalPages - 1;
	if (m_currentPage < 0) m_currentPage = 0;

	// Calculate range for this page
	int startIdx = m_currentPage * DOODADS_PER_PAGE;
	int endIdx = std::min(startIdx + DOODADS_PER_PAGE, totalDoodads);

	// Populate list for current page
	for (int i = startIdx; i < endIdx; ++i) {
		DoodadBrush* doodad = m_filteredDoodads[i];

		// Get first item ID for icon and count total entries (singles + composites)
		uint16_t iconItemId = 0;
		int itemCount = 0;
		int maxVariation = doodad->getMaxVariation();
		for (int v = 0; v < maxVariation; ++v) {
			// Count single items
			int singleCount = doodad->getSingleCount(v);
			itemCount += singleCount;
			if (iconItemId == 0 && singleCount > 0) {
				iconItemId = doodad->getSingleItemId(v, 0);
			}

			// Count composite items
			int compositeCount = doodad->getCompositeCount(v);
			itemCount += compositeCount;
			if (iconItemId == 0 && compositeCount > 0) {
				const CompositeTileList& composite = doodad->getCompositeAt(v, 0);
				for (const auto& tilePair : composite) {
					if (!tilePair.second.empty()) {
						Item* firstItem = tilePair.second.front();
						if (firstItem) {
							iconItemId = firstItem->getID();
							break;
						}
					}
				}
			}
		}

		// Add icon to image list
		wxBitmap bmp = GetItemBitmap(iconItemId, ITEM_ICON_SIZE);
		int imgIdx = m_doodadImageList->Add(bmp);

		// Insert item
		long idx = m_doodadListCtrl->InsertItem(i - startIdx, "", imgIdx);
		m_doodadListCtrl->SetItem(idx, 1, wxstr(doodad->getName()));
		m_doodadListCtrl->SetItem(idx, 2, wxString::Format("%d", itemCount));
		m_doodadListCtrl->SetItemData(idx, i);  // Store index in filtered list
	}

	// Update pagination controls
	if (m_pageInfoText) {
		m_pageInfoText->SetLabel(wxString::Format("%d/%d (%d)",
			m_currentPage + 1, totalPages, totalDoodads));
	}
	if (m_prevPageBtn) {
		m_prevPageBtn->Enable(m_currentPage > 0);
	}
	if (m_nextPageBtn) {
		m_nextPageBtn->Enable(m_currentPage < totalPages - 1);
	}
}

void FloorRuleEditDialog::AddItemById(uint16_t itemId, int weight) {
	if (itemId == 0) return;

	// Check if item already exists
	for (const auto& item : m_rule.items) {
		if (!item.isCompositeEntry() && item.itemId == itemId) {
			wxMessageBox(wxString::Format("Item %d is already in the list", itemId), "Info", wxOK | wxICON_INFORMATION);
			return;
		}
	}

	m_rule.items.push_back(AreaDecoration::ItemEntry(itemId, weight));
	UpdateItemsList();
}

void FloorRuleEditDialog::AddItemsFromDoodad(DoodadBrush* doodad) {
	if (!doodad) return;

	int addedSingles = 0;
	int addedComposites = 0;
	int defaultWeight = m_newItemWeightSpin->GetValue();

	// Helper lambda to add single item if not already in list
	auto tryAddSingle = [&](uint16_t itemId, int itemWeight) {
		if (itemId == 0) return false;

		for (const auto& item : m_rule.items) {
			if (!item.isCompositeEntry() && item.itemId == itemId) {
				return false;
			}
		}

		if (itemWeight <= 0) itemWeight = defaultWeight;
		m_rule.items.push_back(AreaDecoration::ItemEntry(itemId, itemWeight));
		return true;
	};

	// Add single items from all variations
	int maxVariation = doodad->getMaxVariation();
	for (int v = 0; v < maxVariation; ++v) {
		// Add single items
		int singleCount = doodad->getSingleCount(v);
		for (int i = 0; i < singleCount; ++i) {
			uint16_t itemId = doodad->getSingleItemId(v, i);
			int itemWeight = doodad->getSingleItemChance(v, i);
			if (tryAddSingle(itemId, itemWeight)) {
				addedSingles++;
			}
		}

		// Add composites as grouped entries
		int compositeCount = doodad->getCompositeCount(v);
		for (int c = 0; c < compositeCount; ++c) {
			const CompositeTileList& composite = doodad->getCompositeAt(v, c);
			if (composite.empty()) continue;

			int compositeChance = doodad->getCompositeChanceAt(v, c);
			int prevChance = (c > 0) ? doodad->getCompositeChanceAt(v, c - 1) : 0;
			int compositeWeight = compositeChance - prevChance;
			if (compositeWeight <= 0) compositeWeight = defaultWeight;

			std::vector<AreaDecoration::CompositeTile> tiles;
			for (const auto& tilePair : composite) {
				AreaDecoration::CompositeTile tile;
				tile.offset = tilePair.first;
				const ItemVector& items = tilePair.second;
				for (Item* item : items) {
					if (item) {
						tile.itemIds.push_back(item->getID());
					}
				}
				if (!tile.itemIds.empty()) {
					tiles.push_back(tile);
				}
			}

			if (!tiles.empty()) {
				m_rule.items.push_back(AreaDecoration::ItemEntry::MakeComposite(tiles, compositeWeight));
				addedComposites++;
			}
		}
	}

	int addedCount = addedSingles + addedComposites;
	if (addedCount > 0) {
		UpdateItemsList();
		wxMessageBox(wxString::Format("Added %d entries (%d singles, %d composites) from doodad '%s'",
			addedCount, addedSingles, addedComposites, wxstr(doodad->getName())),
			"Success", wxOK | wxICON_INFORMATION);
	} else {
		wxMessageBox("No new entries to add from this doodad (items may already exist in the list)", "Info", wxOK | wxICON_INFORMATION);
	}
}

bool FloorRuleEditDialog::TransferDataFromWindow() {
	m_rule.name = m_nameInput->GetValue().ToStdString();

	if (m_floorRangeRadio->GetValue()) {
		m_rule.floorId = 0;
		m_rule.fromFloorId = m_fromFloorSpin->GetValue();
		m_rule.toFloorId = m_toFloorSpin->GetValue();

		if (m_rule.fromFloorId > m_rule.toFloorId) {
			wxMessageBox("From floor ID must be <= To floor ID", "Error", wxOK | wxICON_ERROR);
			return false;
		}
	} else {
		m_rule.floorId = m_singleFloorSpin->GetValue();
		m_rule.fromFloorId = 0;
		m_rule.toFloorId = 0;

		if (m_rule.floorId == 0) {
			wxMessageBox("Floor ID cannot be 0", "Error", wxOK | wxICON_ERROR);
			return false;
		}
	}

	if (m_rule.items.empty()) {
		wxMessageBox("Rule must have at least one item", "Error", wxOK | wxICON_ERROR);
		return false;
	}

	m_rule.density = m_densitySpin->GetValue() / 100.0f;
	m_rule.maxPlacements = m_maxPlacementsSpin->GetValue();
	m_rule.priority = m_prioritySpin->GetValue();

	return true;
}

void FloorRuleEditDialog::OnFloorTypeChanged(wxCommandEvent& event) {
	bool isSingle = m_singleFloorRadio->GetValue();
	m_singleFloorSpin->Enable(isSingle);
	m_fromFloorSpin->Enable(!isSingle);
	m_toFloorSpin->Enable(!isSingle);
	UpdateFloorPreview();
}

void FloorRuleEditDialog::OnFloorIdChanged(wxSpinEvent& event) {
	UpdateFloorPreview();
}

void FloorRuleEditDialog::OnAddItem(wxCommandEvent& event) {
	uint16_t itemId = m_newItemIdSpin->GetValue();
	int weight = m_newItemWeightSpin->GetValue();

	if (itemId == 0) {
		wxMessageBox("Item ID cannot be 0", "Error", wxOK | wxICON_ERROR);
		return;
	}

	m_rule.items.push_back(AreaDecoration::ItemEntry(itemId, weight));
	UpdateItemsList();
}

void FloorRuleEditDialog::OnEditItem(wxCommandEvent& event) {
	long selected = m_itemsListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected < 0 || selected >= static_cast<long>(m_rule.items.size())) {
		wxMessageBox("Select an item to edit", "Edit Item", wxOK | wxICON_INFORMATION);
		return;
	}

	AreaDecoration::ItemEntry& entry = m_rule.items[selected];
	if (entry.isCompositeEntry()) {
		EditItemDialog(static_cast<size_t>(selected));
		return;
	}
	int weight = m_newItemWeightSpin->GetValue();

	uint16_t itemId = m_newItemIdSpin->GetValue();
	if (itemId == 0) {
		wxMessageBox("Item ID cannot be 0", "Error", wxOK | wxICON_ERROR);
		return;
	}
	entry.itemId = itemId;
	entry.weight = weight;

	UpdateItemsList();

	if (selected >= 0 && selected < m_itemsListCtrl->GetItemCount()) {
		m_itemsListCtrl->SetItemState(selected, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		m_itemsListCtrl->EnsureVisible(selected);
		if (m_editItemBtn) {
			m_editItemBtn->Enable(true);
		}
	}
}

void FloorRuleEditDialog::OnReplaceClusterFromSelection(wxCommandEvent& event) {
	long selected = m_itemsListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected < 0 || selected >= static_cast<long>(m_rule.items.size())) {
		wxMessageBox("Select a cluster to replace", "Replace Cluster", wxOK | wxICON_INFORMATION);
		return;
	}

	AreaDecoration::ItemEntry& entry = m_rule.items[selected];
	if (!entry.isClusterEntry()) {
		wxMessageBox("Selected item is not a cluster entry.", "Replace Cluster", wxOK | wxICON_INFORMATION);
		return;
	}

	std::vector<AreaDecoration::CompositeTile> clusterTiles;
	if (!BuildClusterTilesFromSelection(clusterTiles)) {
		return;
	}

	entry.compositeTiles = std::move(clusterTiles);
	entry.weight = m_newItemWeightSpin ? m_newItemWeightSpin->GetValue() : entry.weight;
	entry.clusterCount = m_clusterCountSpin ? m_clusterCountSpin->GetValue() : entry.clusterCount;
	entry.clusterRadius = m_clusterRadiusSpin ? m_clusterRadiusSpin->GetValue() : entry.clusterRadius;
	entry.clusterMinDistance = m_clusterMinDistanceSpin ? m_clusterMinDistanceSpin->GetValue() : entry.clusterMinDistance;

	UpdateItemsList();
	if (selected >= 0 && selected < m_itemsListCtrl->GetItemCount()) {
		m_itemsListCtrl->SetItemState(selected, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		m_itemsListCtrl->EnsureVisible(selected);
		if (m_editItemBtn) {
			m_editItemBtn->Enable(true);
		}
		if (m_replaceClusterBtn) {
			m_replaceClusterBtn->Enable(true);
		}
	}
}

void FloorRuleEditDialog::OnRemoveItem(wxCommandEvent& event) {
	long selected = m_itemsListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected >= 0 && selected < static_cast<long>(m_rule.items.size())) {
		m_rule.items.erase(m_rule.items.begin() + selected);
		UpdateItemsList();
	}
}

void FloorRuleEditDialog::OnClearItems(wxCommandEvent& event) {
	if (m_rule.items.empty()) return;
	int result = wxMessageBox("Remove all items from this rule?", "Confirm", wxYES_NO | wxICON_WARNING);
	if (result != wxYES) return;
	m_rule.items.clear();
	UpdateItemsList();
}

void FloorRuleEditDialog::OnBrowseItem(wxCommandEvent& event) {
	FindItemDialog dialog(this, "Select Item");
	if (dialog.ShowModal() == wxID_OK) {
		uint16_t itemId = dialog.getResultID();
		if (itemId > 0) {
			m_newItemIdSpin->SetValue(itemId);
		}
	}
}

bool FloorRuleEditDialog::BuildClusterTilesFromSelection(std::vector<AreaDecoration::CompositeTile>& outTiles) {
	outTiles.clear();

	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		wxMessageBox("No editor available", "Error", wxOK | wxICON_ERROR);
		return false;
	}

	const Selection& selection = editor->getSelection();
	if (selection.size() == 0) {
		wxMessageBox("No tiles selected. Select some tiles first.", "Error", wxOK | wxICON_ERROR);
		return false;
	}

	Position minPos(INT_MAX, INT_MAX, INT_MAX);
	Position maxPos(INT_MIN, INT_MIN, INT_MIN);

	const TileSet& tiles = selection.getTiles();
	for (Tile* tile : tiles) {
		const Position& pos = tile->getPosition();
		minPos.x = std::min(minPos.x, pos.x);
		minPos.y = std::min(minPos.y, pos.y);
		minPos.z = std::min(minPos.z, pos.z);
		maxPos.x = std::max(maxPos.x, pos.x);
		maxPos.y = std::max(maxPos.y, pos.y);
		maxPos.z = std::max(maxPos.z, pos.z);
	}

	Position center((minPos.x + maxPos.x) / 2,
	                (minPos.y + maxPos.y) / 2,
	                (minPos.z + maxPos.z) / 2);

	outTiles.reserve(tiles.size());

	for (Tile* tile : tiles) {
		const Position& pos = tile->getPosition();
		AreaDecoration::CompositeTile compTile;
		compTile.offset = Position(pos.x - center.x, pos.y - center.y, pos.z - center.z);

		if (tile->ground) {
			compTile.itemIds.push_back(tile->ground->getID());
		}
		for (Item* item : tile->items) {
			if (item) {
				compTile.itemIds.push_back(item->getID());
			}
		}

		if (!compTile.itemIds.empty()) {
			outTiles.push_back(compTile);
		}
	}

	if (outTiles.empty()) {
		wxMessageBox("Selected tiles contain no items to add.", "Error", wxOK | wxICON_ERROR);
		return false;
	}

	return true;
}

void FloorRuleEditDialog::PrepareClusterPaste(const AreaDecoration::ItemEntry& entry) {
	if (!entry.isCompositeEntry() || entry.compositeTiles.empty()) {
		wxMessageBox("Cluster has no structure to paste.", "Error", wxOK | wxICON_ERROR);
		return;
	}

	int minX = INT_MAX;
	int minY = INT_MAX;
	int minZ = INT_MAX;
	for (const auto& tile : entry.compositeTiles) {
		minX = std::min(minX, tile.offset.x);
		minY = std::min(minY, tile.offset.y);
		minZ = std::min(minZ, tile.offset.z);
	}

	BaseMap* buffer = newd BaseMap();

	for (const auto& tile : entry.compositeTiles) {
		Position pos(tile.offset.x - minX, tile.offset.y - minY, tile.offset.z - minZ);
		Tile* newTile = buffer->createTile(pos.x, pos.y, pos.z);
		if (!newTile) {
			continue;
		}
		for (uint16_t id : tile.itemIds) {
			if (id == 0) continue;
			Item* newItem = Item::Create(id);
			if (newItem) {
				newTile->addItem(newItem);
			}
		}
	}

	g_gui.copybuffer.setBuffer(buffer, Position(0, 0, 0));
	g_gui.PreparePaste(false);
	g_gui.SetStatusText("Paste the cluster on the map, edit it, then select and use 'Replace Selected Cluster'.");
}

void FloorRuleEditDialog::OnAddClusterFromSelection(wxCommandEvent& event) {
	std::vector<AreaDecoration::CompositeTile> clusterTiles;
	if (!BuildClusterTilesFromSelection(clusterTiles)) {
		return;
	}

	int weight = m_newItemWeightSpin ? m_newItemWeightSpin->GetValue() : 100;
	int count = m_clusterCountSpin ? m_clusterCountSpin->GetValue() : 3;
	int radius = m_clusterRadiusSpin ? m_clusterRadiusSpin->GetValue() : 3;
	int minDist = m_clusterMinDistanceSpin ? m_clusterMinDistanceSpin->GetValue() : 2;

	m_rule.items.push_back(AreaDecoration::ItemEntry::MakeCluster(clusterTiles, weight, count, radius, minDist));
	UpdateItemsList();
}

bool FloorRuleEditDialog::EditItemDialog(size_t index) {
	if (index >= m_rule.items.size()) {
		return false;
	}

	AreaDecoration::ItemEntry& entry = m_rule.items[index];

	wxDialog dialog(this, wxID_ANY, "Edit Item", wxDefaultPosition, wxDefaultSize,
		wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER);

	wxBoxSizer* topSizer = newd wxBoxSizer(wxVERTICAL);

	wxFlexGridSizer* grid = newd wxFlexGridSizer(2, 6, 6);
	grid->AddGrowableCol(1, 1);

	wxSpinCtrl* idSpin = nullptr;
	wxButton* browseBtn = nullptr;

	if (!entry.isCompositeEntry()) {
		grid->Add(newd wxStaticText(&dialog, wxID_ANY, "Item ID:"), 0, wxALIGN_CENTER_VERTICAL);
		idSpin = newd wxSpinCtrl(&dialog, wxID_ANY, i2ws(entry.itemId), wxDefaultPosition, wxSize(90, -1), wxSP_ARROW_KEYS, 0, 65535, entry.itemId);
		grid->Add(idSpin, 0, wxALIGN_CENTER_VERTICAL);

		grid->Add(newd wxStaticText(&dialog, wxID_ANY, "Browse:"), 0, wxALIGN_CENTER_VERTICAL);
		browseBtn = newd wxButton(&dialog, wxID_ANY, "...", wxDefaultPosition, wxSize(30, -1));
		grid->Add(browseBtn, 0, wxALIGN_CENTER_VERTICAL);
	} else {
		wxString typeLabel = entry.isClusterEntry() ? "Cluster" : "Composite";
		grid->Add(newd wxStaticText(&dialog, wxID_ANY, "Type:"), 0, wxALIGN_CENTER_VERTICAL);
		grid->Add(newd wxStaticText(&dialog, wxID_ANY, typeLabel), 0, wxALIGN_CENTER_VERTICAL);
	}

	grid->Add(newd wxStaticText(&dialog, wxID_ANY, "Weight:"), 0, wxALIGN_CENTER_VERTICAL);
	wxSpinCtrl* weightSpin = newd wxSpinCtrl(&dialog, wxID_ANY, i2ws(entry.weight), wxDefaultPosition, wxSize(80, -1),
		wxSP_ARROW_KEYS, 1, 1000, entry.weight);
	grid->Add(weightSpin, 0, wxALIGN_CENTER_VERTICAL);

	wxSpinCtrl* countSpin = nullptr;
	wxSpinCtrl* radiusSpin = nullptr;
	wxSpinCtrl* minDistSpin = nullptr;
	wxButton* changeStructureBtn = nullptr;

	if (entry.isClusterEntry()) {
		grid->Add(newd wxStaticText(&dialog, wxID_ANY, "Count:"), 0, wxALIGN_CENTER_VERTICAL);
		countSpin = newd wxSpinCtrl(&dialog, wxID_ANY, i2ws(entry.clusterCount), wxDefaultPosition, wxSize(80, -1),
			wxSP_ARROW_KEYS, 1, 100, entry.clusterCount);
		grid->Add(countSpin, 0, wxALIGN_CENTER_VERTICAL);

		grid->Add(newd wxStaticText(&dialog, wxID_ANY, "Radius:"), 0, wxALIGN_CENTER_VERTICAL);
		radiusSpin = newd wxSpinCtrl(&dialog, wxID_ANY, i2ws(entry.clusterRadius), wxDefaultPosition, wxSize(80, -1),
			wxSP_ARROW_KEYS, 0, 50, entry.clusterRadius);
		grid->Add(radiusSpin, 0, wxALIGN_CENTER_VERTICAL);

		grid->Add(newd wxStaticText(&dialog, wxID_ANY, "Min Dist:"), 0, wxALIGN_CENTER_VERTICAL);
		minDistSpin = newd wxSpinCtrl(&dialog, wxID_ANY, i2ws(entry.clusterMinDistance), wxDefaultPosition, wxSize(80, -1),
			wxSP_ARROW_KEYS, 0, 50, entry.clusterMinDistance);
		grid->Add(minDistSpin, 0, wxALIGN_CENTER_VERTICAL);
	}

	topSizer->Add(grid, 0, wxALL | wxEXPAND, 10);

	if (entry.isClusterEntry()) {
		wxBoxSizer* clusterBtnSizer = newd wxBoxSizer(wxHORIZONTAL);
		changeStructureBtn = newd wxButton(&dialog, wxID_ANY, "Change Structure");
		changeStructureBtn->SetToolTip("Paste this cluster on the map to edit its structure");
		clusterBtnSizer->Add(changeStructureBtn, 0);
		topSizer->Add(clusterBtnSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
	}

	wxBoxSizer* btnSizer = newd wxBoxSizer(wxHORIZONTAL);
	btnSizer->AddStretchSpacer();
	btnSizer->Add(newd wxButton(&dialog, wxID_OK, "OK"), 0, wxRIGHT, 5);
	btnSizer->Add(newd wxButton(&dialog, wxID_CANCEL, "Cancel"), 0);
	topSizer->Add(btnSizer, 0, wxALL | wxEXPAND, 10);

	dialog.SetSizerAndFit(topSizer);
	dialog.CentreOnParent();

	if (browseBtn && idSpin) {
		browseBtn->Bind(wxEVT_BUTTON, [this, &dialog, idSpin](wxCommandEvent&) {
			FindItemDialog findDialog(&dialog, "Select Item");
			if (findDialog.ShowModal() == wxID_OK) {
				uint16_t itemId = findDialog.getResultID();
				if (itemId > 0) {
					idSpin->SetValue(itemId);
				}
			}
		});
	}

	if (changeStructureBtn) {
		changeStructureBtn->Bind(wxEVT_BUTTON, [this, &dialog, &entry](wxCommandEvent&) {
			PrepareClusterPaste(entry);
			dialog.EndModal(wxID_CANCEL);
		});
	}

	if (dialog.ShowModal() != wxID_OK) {
		return false;
	}

	if (!entry.isCompositeEntry()) {
		uint16_t newId = idSpin ? static_cast<uint16_t>(idSpin->GetValue()) : 0;
		if (newId == 0) {
			wxMessageBox("Item ID cannot be 0", "Error", wxOK | wxICON_ERROR, this);
			return false;
		}
		entry.itemId = newId;
	}

	entry.weight = weightSpin->GetValue();

	if (entry.isClusterEntry()) {
		entry.clusterCount = countSpin ? countSpin->GetValue() : entry.clusterCount;
		entry.clusterRadius = radiusSpin ? radiusSpin->GetValue() : entry.clusterRadius;
		entry.clusterMinDistance = minDistSpin ? minDistSpin->GetValue() : entry.clusterMinDistance;
	}

	UpdateItemsList();
	if (index < static_cast<size_t>(m_itemsListCtrl->GetItemCount())) {
		long idx = static_cast<long>(index);
		m_itemsListCtrl->SetItemState(idx, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		m_itemsListCtrl->EnsureVisible(idx);
		if (m_editItemBtn) {
			m_editItemBtn->Enable(true);
		}
	}

	return true;
}

void FloorRuleEditDialog::OnItemsListSelected(wxListEvent& event) {
	long selected = event.GetIndex();
	if (selected < 0 || selected >= static_cast<long>(m_rule.items.size())) {
		return;
	}

	const AreaDecoration::ItemEntry& entry = m_rule.items[selected];
	if (m_newItemWeightSpin) {
		m_newItemWeightSpin->SetValue(entry.weight);
	}

	if (!entry.isCompositeEntry()) {
		if (m_newItemIdSpin) {
			m_newItemIdSpin->SetValue(entry.itemId);
		}
	} else {
		if (m_newItemIdSpin) {
			m_newItemIdSpin->SetValue(0);
		}
		if (entry.isClusterEntry()) {
			if (m_clusterCountSpin) m_clusterCountSpin->SetValue(entry.clusterCount);
			if (m_clusterRadiusSpin) m_clusterRadiusSpin->SetValue(entry.clusterRadius);
			if (m_clusterMinDistanceSpin) m_clusterMinDistanceSpin->SetValue(entry.clusterMinDistance);
		}
	}

	if (m_editItemBtn) {
		m_editItemBtn->Enable(true);
	}
	if (m_replaceClusterBtn) {
		m_replaceClusterBtn->Enable(entry.isClusterEntry());
	}
}

void FloorRuleEditDialog::OnItemsListActivated(wxListEvent& event) {
	const long selected = event.GetIndex();
	if (selected >= 0 && selected < static_cast<long>(m_rule.items.size())) {
		EditItemDialog(static_cast<size_t>(selected));
	} else {
		OnItemsListSelected(event);
	}
}

void FloorRuleEditDialog::OnAddDoodad(wxCommandEvent& event) {
	long sel = m_doodadListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (sel < 0) {
		wxMessageBox("Please select a doodad brush first", "Error", wxOK | wxICON_ERROR);
		return;
	}

	int filteredIdx = static_cast<int>(m_doodadListCtrl->GetItemData(sel));
	if (filteredIdx >= 0 && filteredIdx < static_cast<int>(m_filteredDoodads.size())) {
		DoodadBrush* doodad = m_filteredDoodads[filteredIdx];
		if (doodad) {
			AddItemsFromDoodad(doodad);
		}
	}
}

void FloorRuleEditDialog::OnDoodadDoubleClick(wxListEvent& event) {
	int filteredIdx = static_cast<int>(event.GetData());
	if (filteredIdx >= 0 && filteredIdx < static_cast<int>(m_filteredDoodads.size())) {
		DoodadBrush* doodad = m_filteredDoodads[filteredIdx];
		if (doodad) {
			AddItemsFromDoodad(doodad);
		}
	}
}

void FloorRuleEditDialog::OnDoodadSearch(wxCommandEvent& event) {
	FilterDoodads(m_doodadSearchCtrl->GetValue());
}

void FloorRuleEditDialog::OnPrevPage(wxCommandEvent& event) {
	if (m_currentPage > 0) {
		m_currentPage--;
		UpdateDoodadListDisplay();
	}
}

void FloorRuleEditDialog::OnNextPage(wxCommandEvent& event) {
	int totalPages = (static_cast<int>(m_filteredDoodads.size()) + DOODADS_PER_PAGE - 1) / DOODADS_PER_PAGE;
	if (m_currentPage < totalPages - 1) {
		m_currentPage++;
		UpdateDoodadListDisplay();
	}
}

void FloorRuleEditDialog::OnOK(wxCommandEvent& event) {
	if (TransferDataFromWindow()) {
		if (m_onCloseCallback) {
			m_onCloseCallback(true);
		}
		Destroy();
	}
}

void FloorRuleEditDialog::OnCancel(wxCommandEvent& event) {
	if (m_onCloseCallback) {
		m_onCloseCallback(false);
	}
	Destroy();
}

void FloorRuleEditDialog::OnClose(wxCloseEvent& event) {
	if (m_onCloseCallback) {
		m_onCloseCallback(false);
	}
	Destroy();
}

//=============================================================================
// AreaDecorationDialog
//=============================================================================

wxBEGIN_EVENT_TABLE(AreaDecorationDialog, wxDialog)
	EVT_CHOICE(ID_AREA_TYPE_CHOICE, AreaDecorationDialog::OnAreaTypeChanged)
	EVT_BUTTON(ID_SELECT_FROM_MAP, AreaDecorationDialog::OnSelectFromMap)
	EVT_BUTTON(ID_USE_SELECTION, AreaDecorationDialog::OnUseSelection)

	EVT_BUTTON(ID_ADD_RULE, AreaDecorationDialog::OnAddRule)
	EVT_BUTTON(ID_EDIT_RULE, AreaDecorationDialog::OnEditRule)
	EVT_BUTTON(ID_REMOVE_RULE, AreaDecorationDialog::OnRemoveRule)
	EVT_LIST_ITEM_ACTIVATED(ID_RULES_LIST, AreaDecorationDialog::OnRuleDoubleClick)

	EVT_CHOICE(ID_DISTRIBUTION_CHOICE, AreaDecorationDialog::OnDistributionChanged)

	EVT_BUTTON(ID_PREVIEW, AreaDecorationDialog::OnPreview)
	EVT_BUTTON(ID_REROLL, AreaDecorationDialog::OnReroll)
	EVT_BUTTON(ID_APPLY, AreaDecorationDialog::OnApply)
	EVT_BUTTON(ID_REVERT, AreaDecorationDialog::OnRevert)
	EVT_BUTTON(ID_REMOVE_LAST_APPLY, AreaDecorationDialog::OnRemoveLastApply)

	// Preset events
	EVT_CHOICE(ID_PRESET_CHOICE, AreaDecorationDialog::OnPresetSelected)
	EVT_BUTTON(ID_SAVE_PRESET, AreaDecorationDialog::OnSavePreset)
	EVT_BUTTON(ID_DELETE_PRESET, AreaDecorationDialog::OnDeletePreset)
	EVT_BUTTON(ID_EXPORT_PRESET, AreaDecorationDialog::OnExportPreset)
	EVT_BUTTON(ID_IMPORT_PRESET, AreaDecorationDialog::OnImportPreset)

	EVT_CLOSE(AreaDecorationDialog::OnClose)
wxEND_EVENT_TABLE()

AreaDecorationDialog::AreaDecorationDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, "Area Decoration", wxDefaultPosition, wxSize(620, 800),
	           wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
	, m_presetChoice(nullptr)
	, m_presetNameInput(nullptr)
{
	Editor* editor = g_gui.GetCurrentEditor();
	if (editor) {
		m_engine = std::make_unique<AreaDecoration::DecorationEngine>(editor);
	}

	// Load presets
	AreaDecoration::PresetManager::getInstance().loadPresets();

	CreateControls();
	UpdatePresetList();
	UpdateUI();
	Centre();
}

AreaDecorationDialog::~AreaDecorationDialog() {
	if (m_engine) {
		m_engine->clearPreview();
	}
}

void AreaDecorationDialog::SetSeedInputValue(uint64_t seed) {
	if (m_seedInput) {
		m_seedInput->SetValue(wxString::Format("%llu", seed));
	}
}

void AreaDecorationDialog::CreateControls() {
	wxBoxSizer* mainSizer = newd wxBoxSizer(wxVERTICAL);

	// Preset management section at the top
	CreatePresetControls(mainSizer);

	wxNotebook* notebook = newd wxNotebook(this, wxID_ANY);

	CreateAreaTab(notebook);
	CreateRulesTab(notebook);
	CreateSettingsTab(notebook);
	CreateSeedTab(notebook);

	mainSizer->Add(notebook, 1, wxALL | wxEXPAND, 5);

	CreatePreviewControls(mainSizer);

	mainSizer->Add(CreateButtonSizer(wxCLOSE), 0, wxALL | wxALIGN_RIGHT, 5);

	SetSizer(mainSizer);
}

void AreaDecorationDialog::CreatePresetControls(wxBoxSizer* mainSizer) {
	wxStaticBoxSizer* presetBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Preset Configuration");

	// Row 1: Load preset
	wxBoxSizer* loadRow = newd wxBoxSizer(wxHORIZONTAL);
	loadRow->Add(newd wxStaticText(this, wxID_ANY, "Load Preset:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	m_presetChoice = newd wxChoice(this, ID_PRESET_CHOICE, wxDefaultPosition, wxSize(200, -1));
	loadRow->Add(m_presetChoice, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

	wxButton* deleteBtn = newd wxButton(this, ID_DELETE_PRESET, "Delete", wxDefaultPosition, wxSize(60, -1));
	deleteBtn->SetToolTip("Delete the selected preset");
	loadRow->Add(deleteBtn, 0, wxRIGHT, 5);

	wxButton* importBtn = newd wxButton(this, ID_IMPORT_PRESET, "Import...", wxDefaultPosition, wxSize(70, -1));
	importBtn->SetToolTip("Import preset from file");
	loadRow->Add(importBtn, 0);

	presetBox->Add(loadRow, 0, wxALL | wxEXPAND, 5);

	// Row 2: Save preset
	wxBoxSizer* saveRow = newd wxBoxSizer(wxHORIZONTAL);
	saveRow->Add(newd wxStaticText(this, wxID_ANY, "Save As:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	m_presetNameInput = newd wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(200, -1));
	m_presetNameInput->SetHint("Enter preset name...");
	saveRow->Add(m_presetNameInput, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

	wxButton* saveBtn = newd wxButton(this, ID_SAVE_PRESET, "Save", wxDefaultPosition, wxSize(60, -1));
	saveBtn->SetToolTip("Save current configuration as a preset");
	saveRow->Add(saveBtn, 0, wxRIGHT, 5);

	wxButton* exportBtn = newd wxButton(this, ID_EXPORT_PRESET, "Export...", wxDefaultPosition, wxSize(70, -1));
	exportBtn->SetToolTip("Export current configuration to file");
	saveRow->Add(exportBtn, 0);

	presetBox->Add(saveRow, 0, wxALL | wxEXPAND, 5);

	mainSizer->Add(presetBox, 0, wxALL | wxEXPAND, 5);
}

void AreaDecorationDialog::CreateAreaTab(wxNotebook* notebook) {
	wxPanel* panel = newd wxPanel(notebook);
	wxBoxSizer* sizer = newd wxBoxSizer(wxVERTICAL);

	// Area type selection
	wxStaticBoxSizer* typeBox = newd wxStaticBoxSizer(wxVERTICAL, panel, "Area Type");

	wxArrayString areaTypes;
	areaTypes.Add("Rectangle");
	areaTypes.Add("Flood Fill");
	areaTypes.Add("Current Selection");

	m_areaTypeChoice = newd wxChoice(panel, ID_AREA_TYPE_CHOICE, wxDefaultPosition, wxDefaultSize, areaTypes);
	m_areaTypeChoice->SetSelection(0);
	typeBox->Add(m_areaTypeChoice, 0, wxALL | wxEXPAND, 5);

	sizer->Add(typeBox, 0, wxALL | wxEXPAND, 5);

	// Rectangle coordinates
	wxStaticBoxSizer* rectBox = newd wxStaticBoxSizer(wxVERTICAL, panel, "Rectangle Coordinates");

	wxFlexGridSizer* coordGrid = newd wxFlexGridSizer(4, 5, 5);

	coordGrid->Add(newd wxStaticText(panel, wxID_ANY, "X1:"), 0, wxALIGN_CENTER_VERTICAL);
	m_rectX1Spin = newd wxSpinCtrl(panel, wxID_ANY, "0", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535, 0);
	coordGrid->Add(m_rectX1Spin, 0);

	coordGrid->Add(newd wxStaticText(panel, wxID_ANY, "Y1:"), 0, wxALIGN_CENTER_VERTICAL);
	m_rectY1Spin = newd wxSpinCtrl(panel, wxID_ANY, "0", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535, 0);
	coordGrid->Add(m_rectY1Spin, 0);

	coordGrid->Add(newd wxStaticText(panel, wxID_ANY, "X2:"), 0, wxALIGN_CENTER_VERTICAL);
	m_rectX2Spin = newd wxSpinCtrl(panel, wxID_ANY, "0", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535, 0);
	coordGrid->Add(m_rectX2Spin, 0);

	coordGrid->Add(newd wxStaticText(panel, wxID_ANY, "Y2:"), 0, wxALIGN_CENTER_VERTICAL);
	m_rectY2Spin = newd wxSpinCtrl(panel, wxID_ANY, "0", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535, 0);
	coordGrid->Add(m_rectY2Spin, 0);

	coordGrid->Add(newd wxStaticText(panel, wxID_ANY, "Z:"), 0, wxALIGN_CENTER_VERTICAL);
	m_rectZSpin = newd wxSpinCtrl(panel, wxID_ANY, "7", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 15, 7);
	coordGrid->Add(m_rectZSpin, 0);

	m_rectX1Spin->Bind(wxEVT_SPINCTRL, &AreaDecorationDialog::OnRectangleCoordsChanged, this);
	m_rectY1Spin->Bind(wxEVT_SPINCTRL, &AreaDecorationDialog::OnRectangleCoordsChanged, this);
	m_rectX2Spin->Bind(wxEVT_SPINCTRL, &AreaDecorationDialog::OnRectangleCoordsChanged, this);
	m_rectY2Spin->Bind(wxEVT_SPINCTRL, &AreaDecorationDialog::OnRectangleCoordsChanged, this);
	m_rectZSpin->Bind(wxEVT_SPINCTRL, &AreaDecorationDialog::OnRectangleCoordsChanged, this);

	m_rectX1Spin->Bind(wxEVT_TEXT, &AreaDecorationDialog::OnRectangleCoordsChanged, this);
	m_rectY1Spin->Bind(wxEVT_TEXT, &AreaDecorationDialog::OnRectangleCoordsChanged, this);
	m_rectX2Spin->Bind(wxEVT_TEXT, &AreaDecorationDialog::OnRectangleCoordsChanged, this);
	m_rectY2Spin->Bind(wxEVT_TEXT, &AreaDecorationDialog::OnRectangleCoordsChanged, this);
	m_rectZSpin->Bind(wxEVT_TEXT, &AreaDecorationDialog::OnRectangleCoordsChanged, this);

	rectBox->Add(coordGrid, 0, wxALL, 5);

	wxBoxSizer* selectBtnSizer = newd wxBoxSizer(wxHORIZONTAL);
	m_selectAreaButton = newd wxButton(panel, ID_SELECT_FROM_MAP, "Select from Map...");
	wxButton* useSelectionBtn = newd wxButton(panel, ID_USE_SELECTION, "Use Current Selection");
	selectBtnSizer->Add(m_selectAreaButton, 0, wxRIGHT, 5);
	selectBtnSizer->Add(useSelectionBtn, 0);
	rectBox->Add(selectBtnSizer, 0, wxALL, 5);

	sizer->Add(rectBox, 0, wxALL | wxEXPAND, 5);

	// Area info
	m_areaInfoText = newd wxStaticText(panel, wxID_ANY, "No area defined");
	sizer->Add(m_areaInfoText, 0, wxALL, 10);

	panel->SetSizer(sizer);
	notebook->AddPage(panel, "Area");
}

void AreaDecorationDialog::CreateRulesTab(wxNotebook* notebook) {
	wxPanel* panel = newd wxPanel(notebook);
	wxBoxSizer* sizer = newd wxBoxSizer(wxVERTICAL);

	// Rules list
	m_rulesListCtrl = newd wxListCtrl(panel, ID_RULES_LIST, wxDefaultPosition, wxSize(-1, 250),
	                                   wxLC_REPORT | wxLC_SINGLE_SEL);
	m_rulesListCtrl->InsertColumn(0, "Name", wxLIST_FORMAT_LEFT, 120);
	m_rulesListCtrl->InsertColumn(1, "Floor(s)", wxLIST_FORMAT_LEFT, 100);
	m_rulesListCtrl->InsertColumn(2, "Items", wxLIST_FORMAT_LEFT, 60);
	m_rulesListCtrl->InsertColumn(3, "Density", wxLIST_FORMAT_LEFT, 70);
	m_rulesListCtrl->InsertColumn(4, "Priority", wxLIST_FORMAT_LEFT, 60);

	sizer->Add(m_rulesListCtrl, 1, wxALL | wxEXPAND, 5);

	// Buttons
	wxBoxSizer* btnSizer = newd wxBoxSizer(wxHORIZONTAL);
	wxButton* addBtn = newd wxButton(panel, ID_ADD_RULE, "Add Rule");
	wxButton* editBtn = newd wxButton(panel, ID_EDIT_RULE, "Edit");
	wxButton* removeBtn = newd wxButton(panel, ID_REMOVE_RULE, "Remove");

	btnSizer->Add(addBtn, 0, wxRIGHT, 5);
	btnSizer->Add(editBtn, 0, wxRIGHT, 5);
	btnSizer->Add(removeBtn, 0);

	sizer->Add(btnSizer, 0, wxALL, 5);

	panel->SetSizer(sizer);
	notebook->AddPage(panel, "Floor Rules");
}

void AreaDecorationDialog::CreateSettingsTab(wxNotebook* notebook) {
	wxPanel* panel = newd wxPanel(notebook);
	wxBoxSizer* sizer = newd wxBoxSizer(wxVERTICAL);

	// Spacing
	wxStaticBoxSizer* spacingBox = newd wxStaticBoxSizer(wxVERTICAL, panel, "Spacing");

	wxFlexGridSizer* spacingGrid = newd wxFlexGridSizer(2, 5, 5);

	spacingGrid->Add(newd wxStaticText(panel, wxID_ANY, "Min Distance:"), 0, wxALIGN_CENTER_VERTICAL);
	m_minDistanceSpin = newd wxSpinCtrl(panel, wxID_ANY, "1", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 20, 1);
	spacingGrid->Add(m_minDistanceSpin, 0);

	spacingGrid->Add(newd wxStaticText(panel, wxID_ANY, "Same Item Distance:"), 0, wxALIGN_CENTER_VERTICAL);
	m_sameItemDistanceSpin = newd wxSpinCtrl(panel, wxID_ANY, "2", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 20, 2);
	spacingGrid->Add(m_sameItemDistanceSpin, 0);

	spacingBox->Add(spacingGrid, 0, wxALL, 5);

	m_checkDiagonalsCheck = newd wxCheckBox(panel, wxID_ANY, "Check Diagonals");
	m_checkDiagonalsCheck->SetValue(true);
	spacingBox->Add(m_checkDiagonalsCheck, 0, wxALL, 5);

	sizer->Add(spacingBox, 0, wxALL | wxEXPAND, 5);

	// Distribution
	wxStaticBoxSizer* distBox = newd wxStaticBoxSizer(wxVERTICAL, panel, "Distribution");

	wxArrayString distModes;
	distModes.Add("Pure Random");
	distModes.Add("Clustered");
	distModes.Add("Grid Based");

	wxBoxSizer* modeSizer = newd wxBoxSizer(wxHORIZONTAL);
	modeSizer->Add(newd wxStaticText(panel, wxID_ANY, "Mode:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	m_distributionChoice = newd wxChoice(panel, ID_DISTRIBUTION_CHOICE, wxDefaultPosition, wxDefaultSize, distModes);
	m_distributionChoice->SetSelection(0);
	modeSizer->Add(m_distributionChoice, 1);
	distBox->Add(modeSizer, 0, wxALL | wxEXPAND, 5);

	// Cluster settings
	wxFlexGridSizer* clusterGrid = newd wxFlexGridSizer(2, 5, 5);

	clusterGrid->Add(newd wxStaticText(panel, wxID_ANY, "Cluster Strength:"), 0, wxALIGN_CENTER_VERTICAL);
	m_clusterStrengthSlider = newd wxSlider(panel, wxID_ANY, 50, 0, 100, wxDefaultPosition, wxSize(150, -1));
	clusterGrid->Add(m_clusterStrengthSlider, 0);

	clusterGrid->Add(newd wxStaticText(panel, wxID_ANY, "Cluster Count:"), 0, wxALIGN_CENTER_VERTICAL);
	m_clusterCountSpin = newd wxSpinCtrl(panel, wxID_ANY, "3", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 1, 20, 3);
	clusterGrid->Add(m_clusterCountSpin, 0);

	distBox->Add(clusterGrid, 0, wxALL, 5);

	// Grid settings
	wxFlexGridSizer* gridGrid = newd wxFlexGridSizer(2, 5, 5);

	gridGrid->Add(newd wxStaticText(panel, wxID_ANY, "Grid Spacing X:"), 0, wxALIGN_CENTER_VERTICAL);
	m_gridSpacingXSpin = newd wxSpinCtrl(panel, wxID_ANY, "3", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 1, 20, 3);
	gridGrid->Add(m_gridSpacingXSpin, 0);

	gridGrid->Add(newd wxStaticText(panel, wxID_ANY, "Grid Spacing Y:"), 0, wxALIGN_CENTER_VERTICAL);
	m_gridSpacingYSpin = newd wxSpinCtrl(panel, wxID_ANY, "3", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 1, 20, 3);
	gridGrid->Add(m_gridSpacingYSpin, 0);

	gridGrid->Add(newd wxStaticText(panel, wxID_ANY, "Grid Jitter:"), 0, wxALIGN_CENTER_VERTICAL);
	m_gridJitterSpin = newd wxSpinCtrl(panel, wxID_ANY, "1", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 5, 1);
	gridGrid->Add(m_gridJitterSpin, 0);

	distBox->Add(gridGrid, 0, wxALL, 5);

	sizer->Add(distBox, 0, wxALL | wxEXPAND, 5);

	// Limits
	wxStaticBoxSizer* limitsBox = newd wxStaticBoxSizer(wxVERTICAL, panel, "Limits");

	wxFlexGridSizer* limitsGrid = newd wxFlexGridSizer(2, 5, 5);

	limitsGrid->Add(newd wxStaticText(panel, wxID_ANY, "Max Items Total:"), 0, wxALIGN_CENTER_VERTICAL);
	m_maxItemsSpin = newd wxSpinCtrl(panel, wxID_ANY, "-1", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, -1, 10000, -1);
	limitsGrid->Add(m_maxItemsSpin, 0);

	limitsBox->Add(limitsGrid, 0, wxALL, 5);

	m_skipBlockedCheck = newd wxCheckBox(panel, wxID_ANY, "Skip Blocked Tiles");
	m_skipBlockedCheck->SetValue(true);
	limitsBox->Add(m_skipBlockedCheck, 0, wxALL, 5);

	sizer->Add(limitsBox, 0, wxALL | wxEXPAND, 5);

	panel->SetSizer(sizer);
	notebook->AddPage(panel, "Settings");
}

void AreaDecorationDialog::CreateSeedTab(wxNotebook* notebook) {
	wxPanel* panel = newd wxPanel(notebook);
	wxBoxSizer* sizer = newd wxBoxSizer(wxVERTICAL);

	wxStaticBoxSizer* seedBox = newd wxStaticBoxSizer(wxVERTICAL, panel, "Random Seed");

	m_useSeedCheck = newd wxCheckBox(panel, wxID_ANY, "Use Specific Seed");
	seedBox->Add(m_useSeedCheck, 0, wxALL, 5);

	wxBoxSizer* seedInputSizer = newd wxBoxSizer(wxHORIZONTAL);
	seedInputSizer->Add(newd wxStaticText(panel, wxID_ANY, "Seed:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	m_seedInput = newd wxTextCtrl(panel, wxID_ANY, "0", wxDefaultPosition, wxSize(150, -1));
	seedInputSizer->Add(m_seedInput, 0);
	seedBox->Add(seedInputSizer, 0, wxALL, 5);

	seedBox->Add(newd wxStaticText(panel, wxID_ANY,
		"Using the same seed will produce identical results.\n"
		"Leave unchecked for random seed each preview."),
		0, wxALL, 5);

	sizer->Add(seedBox, 0, wxALL | wxEXPAND, 5);

	panel->SetSizer(sizer);
	notebook->AddPage(panel, "Seed");
}

void AreaDecorationDialog::CreatePreviewControls(wxBoxSizer* mainSizer) {
	wxStaticBoxSizer* previewBox = newd wxStaticBoxSizer(wxVERTICAL, this, "Preview");

	wxBoxSizer* btnSizer = newd wxBoxSizer(wxHORIZONTAL);

	wxButton* previewBtn = newd wxButton(this, ID_PREVIEW, "Apply Changes");
	wxButton* rerollBtn = newd wxButton(this, ID_REROLL, "Reroll");
	m_applyBtn = newd wxButton(this, ID_APPLY, "Apply to Map");
	wxButton* revertBtn = newd wxButton(this, ID_REVERT, "Clear Preview");
	m_removeLastApplyBtn = newd wxButton(this, ID_REMOVE_LAST_APPLY, "Remove Last Apply");
	m_removeLastApplyBtn->Enable(false);
	m_applyBtn->Enable(false);

	btnSizer->Add(previewBtn, 0, wxRIGHT, 5);
	btnSizer->Add(rerollBtn, 0, wxRIGHT, 5);
	btnSizer->Add(m_applyBtn, 0, wxRIGHT, 5);
	btnSizer->Add(revertBtn, 0, wxRIGHT, 5);
	btnSizer->Add(m_removeLastApplyBtn, 0);

	previewBox->Add(btnSizer, 0, wxALL, 5);

	m_statsText = newd wxStaticText(this, wxID_ANY, "No preview generated");
	previewBox->Add(m_statsText, 0, wxALL | wxEXPAND, 5);

	mainSizer->Add(previewBox, 0, wxALL | wxEXPAND, 5);
}

void AreaDecorationDialog::UpdateUI() {
	int areaType = m_areaTypeChoice->GetSelection();

	bool isRect = (areaType == 0);
	m_rectX1Spin->Enable(isRect);
	m_rectY1Spin->Enable(isRect);
	m_rectX2Spin->Enable(isRect);
	m_rectY2Spin->Enable(isRect);
	m_rectZSpin->Enable(isRect);
	m_selectAreaButton->Enable(isRect);

	int distMode = m_distributionChoice->GetSelection();
	bool isClustered = (distMode == 1);
	bool isGrid = (distMode == 2);

	m_clusterStrengthSlider->Enable(isClustered);
	m_clusterCountSpin->Enable(isClustered);
	m_gridSpacingXSpin->Enable(isGrid);
	m_gridSpacingYSpin->Enable(isGrid);
	m_gridJitterSpin->Enable(isGrid);

	UpdateRulesList();
}

void AreaDecorationDialog::UpdateRulesList() {
	m_rulesListCtrl->DeleteAllItems();

	for (size_t i = 0; i < m_preset.floorRules.size(); ++i) {
		const auto& rule = m_preset.floorRules[i];

		wxString floorStr;
		if (rule.isRangeRule()) {
			floorStr = wxString::Format("%d - %d", rule.fromFloorId, rule.toFloorId);
		} else {
			floorStr = wxString::Format("%d", rule.floorId);
		}

		long idx = m_rulesListCtrl->InsertItem(i, rule.name);
		m_rulesListCtrl->SetItem(idx, 1, floorStr);
		m_rulesListCtrl->SetItem(idx, 2, wxString::Format("%zu", rule.items.size()));
		m_rulesListCtrl->SetItem(idx, 3, wxString::Format("%.0f%%", rule.density * 100));
		m_rulesListCtrl->SetItem(idx, 4, wxString::Format("%d", rule.priority));
	}
}

void AreaDecorationDialog::UpdateStats() {
	if (!m_engine) {
		m_statsText->SetLabel("No editor available");
		if (m_removeLastApplyBtn) {
			m_removeLastApplyBtn->Enable(false);
		}
		if (m_applyBtn) {
			m_applyBtn->Enable(false);
		}
		return;
	}

	const auto& state = m_engine->getPreviewState();

	if (!state.isValid) {
		m_statsText->SetLabel("No preview generated");
		if (m_removeLastApplyBtn) {
			m_removeLastApplyBtn->Enable(m_engine->hasLastApplied());
		}
		if (m_applyBtn) {
			m_applyBtn->Enable(false);
		}
		return;
	}

	wxString stats;
	stats << "Items placed: " << state.totalItemsPlaced << "\n";
	stats << "Seed: " << state.seed;

	m_statsText->SetLabel(stats);
	if (m_removeLastApplyBtn) {
		m_removeLastApplyBtn->Enable(m_engine->hasLastApplied());
	}
	if (m_applyBtn) {
		m_applyBtn->Enable(true);
	}
}

void AreaDecorationDialog::BuildPresetFromUI() {
	m_preset.spacing.minDistance = m_minDistanceSpin->GetValue();
	m_preset.spacing.minSameItemDistance = m_sameItemDistanceSpin->GetValue();
	m_preset.spacing.checkDiagonals = m_checkDiagonalsCheck->GetValue();

	int distMode = m_distributionChoice->GetSelection();
	m_preset.distribution.mode = static_cast<AreaDecoration::DistributionMode>(distMode);
	m_preset.distribution.clusterStrength = m_clusterStrengthSlider->GetValue() / 100.0f;
	m_preset.distribution.clusterCount = m_clusterCountSpin->GetValue();
	m_preset.distribution.gridSpacingX = m_gridSpacingXSpin->GetValue();
	m_preset.distribution.gridSpacingY = m_gridSpacingYSpin->GetValue();
	m_preset.distribution.gridJitter = m_gridJitterSpin->GetValue();

	m_preset.maxItemsTotal = m_maxItemsSpin->GetValue();
	m_preset.skipBlockedTiles = m_skipBlockedCheck->GetValue();

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

void AreaDecorationDialog::BuildAreaFromUI() {
	int areaType = m_areaTypeChoice->GetSelection();

	switch (areaType) {
		case 0: // Rectangle
			m_area.type = AreaDecoration::AreaDefinition::Type::Rectangle;
			m_area.rectMin = Position(m_rectX1Spin->GetValue(), m_rectY1Spin->GetValue(), m_rectZSpin->GetValue());
			m_area.rectMax = Position(m_rectX2Spin->GetValue(), m_rectY2Spin->GetValue(), m_rectZSpin->GetValue());
			break;
		case 1: // Flood Fill - need to implement selection
			m_area.type = AreaDecoration::AreaDefinition::Type::FloodFill;
			break;
		case 2: // Selection
			m_area.type = AreaDecoration::AreaDefinition::Type::Selection;
			break;
	}
}

void AreaDecorationDialog::OnAreaTypeChanged(wxCommandEvent& event) {
	UpdateUI();
}

void AreaDecorationDialog::OnRectangleCoordsChanged(wxCommandEvent& event) {
	if (m_areaTypeChoice && m_areaTypeChoice->GetSelection() != 0) {
		m_areaTypeChoice->SetSelection(0);
		UpdateUI();
	}

	if (m_areaInfoText) {
		m_areaInfoText->SetLabel(wxString::Format("Rectangle: (%d,%d) to (%d,%d) Z:%d",
			m_rectX1Spin->GetValue(),
			m_rectY1Spin->GetValue(),
			m_rectX2Spin->GetValue(),
			m_rectY2Spin->GetValue(),
			m_rectZSpin->GetValue()));
	}
}

void AreaDecorationDialog::OnSelectFromMap(wxCommandEvent& event) {
	wxMessageBox("Click two corners on the map to define the rectangle area.\n\n"
	             "Feature coming soon - for now, enter coordinates manually or use current selection.",
	             "Select Area", wxOK | wxICON_INFORMATION);
}

void AreaDecorationDialog::OnUseSelection(wxCommandEvent& event) {
	Editor* editor = g_gui.GetCurrentEditor();
	if (!editor) {
		wxMessageBox("No editor available", "Error", wxOK | wxICON_ERROR);
		return;
	}

	const Selection& selection = editor->getSelection();
	if (selection.size() == 0) {
		wxMessageBox("No tiles selected. Select some tiles first.", "Error", wxOK | wxICON_ERROR);
		return;
	}

	Position minPos(INT_MAX, INT_MAX, INT_MAX);
	Position maxPos(INT_MIN, INT_MIN, INT_MIN);

	const TileSet& tiles = selection.getTiles();
	for (Tile* tile : tiles) {
		const Position& pos = tile->getPosition();
		minPos.x = std::min(minPos.x, pos.x);
		minPos.y = std::min(minPos.y, pos.y);
		minPos.z = std::min(minPos.z, pos.z);
		maxPos.x = std::max(maxPos.x, pos.x);
		maxPos.y = std::max(maxPos.y, pos.y);
		maxPos.z = std::max(maxPos.z, pos.z);
	}

	m_rectX1Spin->SetValue(minPos.x);
	m_rectY1Spin->SetValue(minPos.y);
	m_rectX2Spin->SetValue(maxPos.x);
	m_rectY2Spin->SetValue(maxPos.y);
	m_rectZSpin->SetValue(minPos.z);

	m_areaTypeChoice->SetSelection(2); // Selection type

	m_areaInfoText->SetLabel(wxString::Format("Selection: %zu tiles (%d,%d) to (%d,%d) Z:%d",
		selection.size(), minPos.x, minPos.y, maxPos.x, maxPos.y, minPos.z));

	UpdateUI();
}

void AreaDecorationDialog::OnAddRule(wxCommandEvent& event) {
	// Create a new rule and add it to the list first
	AreaDecoration::FloorRule newRule;
	newRule.name = "New Rule";
	newRule.floorId = 0;
	newRule.density = 0.3f;

	m_preset.floorRules.push_back(newRule);
	size_t ruleIndex = m_preset.floorRules.size() - 1;
	UpdateRulesList();

	// Open non-modal dialog for editing
	FloorRuleEditDialog* dialog = newd FloorRuleEditDialog(
		g_gui.root,  // Use root window to allow palette interaction
		m_preset.floorRules[ruleIndex],
		[this, ruleIndex](bool accepted) {
			if (!accepted) {
				// User cancelled, remove the rule
				if (ruleIndex < m_preset.floorRules.size()) {
					m_preset.floorRules.erase(m_preset.floorRules.begin() + ruleIndex);
				}
			}
			UpdateRulesList();
		}
	);
	dialog->Show();
}

void AreaDecorationDialog::OnEditRule(wxCommandEvent& event) {
	long selected = m_rulesListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected < 0 || selected >= static_cast<long>(m_preset.floorRules.size())) {
		wxMessageBox("Select a rule to edit", "Error", wxOK | wxICON_ERROR);
		return;
	}

	size_t ruleIndex = static_cast<size_t>(selected);

	// Store a backup copy in case user cancels
	AreaDecoration::FloorRule backupRule = m_preset.floorRules[ruleIndex];

	FloorRuleEditDialog* dialog = newd FloorRuleEditDialog(
		g_gui.root,  // Use root window to allow palette interaction
		m_preset.floorRules[ruleIndex],
		[this, ruleIndex, backupRule](bool accepted) {
			if (!accepted) {
				// User cancelled, restore backup
				if (ruleIndex < m_preset.floorRules.size()) {
					m_preset.floorRules[ruleIndex] = backupRule;
				}
			}
			UpdateRulesList();
		}
	);
	dialog->Show();
}

void AreaDecorationDialog::OnRemoveRule(wxCommandEvent& event) {
	long selected = m_rulesListCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selected >= 0 && selected < static_cast<long>(m_preset.floorRules.size())) {
		m_preset.floorRules.erase(m_preset.floorRules.begin() + selected);
		UpdateRulesList();
	}
}

void AreaDecorationDialog::OnRuleDoubleClick(wxListEvent& event) {
	long idx = event.GetIndex();
	if (idx >= 0 && idx < static_cast<long>(m_preset.floorRules.size())) {
		size_t ruleIndex = static_cast<size_t>(idx);

		// Store a backup copy in case user cancels
		AreaDecoration::FloorRule backupRule = m_preset.floorRules[ruleIndex];

		FloorRuleEditDialog* dialog = newd FloorRuleEditDialog(
			g_gui.root,  // Use root window to allow palette interaction
			m_preset.floorRules[ruleIndex],
			[this, ruleIndex, backupRule](bool accepted) {
				if (!accepted) {
					// User cancelled, restore backup
					if (ruleIndex < m_preset.floorRules.size()) {
						m_preset.floorRules[ruleIndex] = backupRule;
					}
				}
				UpdateRulesList();
			}
		);
		dialog->Show();
	}
}

void AreaDecorationDialog::OnDistributionChanged(wxCommandEvent& event) {
	UpdateUI();
}

void AreaDecorationDialog::OnPreview(wxCommandEvent& event) {
	if (!m_engine) {
		wxMessageBox("No editor available", "Error", wxOK | wxICON_ERROR);
		return;
	}

	if (m_preset.floorRules.empty()) {
		wxMessageBox("Add at least one floor rule first", "Error", wxOK | wxICON_ERROR);
		return;
	}

	BuildPresetFromUI();
	BuildAreaFromUI();

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

	if (!m_engine->generatePreview(seed)) {
		wxMessageBox("Failed to generate preview:\n" + m_engine->getLastError(),
		             "Error", wxOK | wxICON_ERROR);
		UpdateStats();
		return;
	}

	UpdateStats();
	g_gui.RefreshView();
}

void AreaDecorationDialog::OnReroll(wxCommandEvent& event) {
	if (!m_engine) return;

	if (!m_engine->getPreviewState().isValid) {
		BuildPresetFromUI();
		BuildAreaFromUI();
		m_engine->setArea(m_area);
		m_engine->setPreset(m_preset);
	}

	if (m_engine->generatePreview(0)) {
		m_seedInput->SetValue(wxString::Format("%llu", m_engine->getPreviewState().seed));
		UpdateStats();
		g_gui.RefreshView();
	} else {
		wxMessageBox("Failed to reroll preview:\n" + m_engine->getLastError(),
		             "Error", wxOK | wxICON_ERROR);
	}
}

void AreaDecorationDialog::OnApply(wxCommandEvent& event) {
	if (!m_engine) return;

	if (!m_engine->getPreviewState().isValid) {
		wxMessageBox("Apply changes first to generate a preview.", "Area Decoration",
		             wxOK | wxICON_INFORMATION);
		return;
	}

	const int kMaxAutoBatches = 20;
	int batchesApplied = 0;
	int totalItemsApplied = 0;

	while (true) {
		const bool wasCapped = m_engine->wasPreviewCapped();
		const int batchCount = static_cast<int>(m_engine->getPreviewState().items.size());

		if (m_engine->applyPreview()) {
			totalItemsApplied += batchCount;
			batchesApplied++;
		} else {
			wxMessageBox("Failed to apply preview:\n" + m_engine->getLastError(),
			             "Error", wxOK | wxICON_ERROR);
			return;
		}

		if (!wasCapped || batchesApplied >= kMaxAutoBatches) {
			break;
		}

		if (!m_engine->generatePreview(0)) {
			break;
		}
	}

	if (batchesApplied > 1) {
		m_statsText->SetLabel(wxString::Format("Applied in %d batches (%d items).", batchesApplied, totalItemsApplied));
	} else {
		m_statsText->SetLabel("Preview applied successfully!");
	}
	if (m_removeLastApplyBtn) {
		m_removeLastApplyBtn->Enable(m_engine->hasLastApplied());
	}
	g_gui.RefreshView();
}

void AreaDecorationDialog::OnRevert(wxCommandEvent& event) {
	if (!m_engine) return;

	m_engine->clearPreview();
	m_statsText->SetLabel("Preview reverted");
	g_gui.RefreshView();
	if (m_applyBtn) {
		m_applyBtn->Enable(false);
	}
}

void AreaDecorationDialog::OnRemoveLastApply(wxCommandEvent& event) {
	if (!m_engine) return;

	if (m_engine->removeLastApplied()) {
		m_statsText->SetLabel("Last apply removed");
		if (m_removeLastApplyBtn) {
			m_removeLastApplyBtn->Enable(m_engine->hasLastApplied());
		}
		g_gui.RefreshView();
	} else {
		wxMessageBox("Failed to remove last apply:\n" + m_engine->getLastError(),
		             "Error", wxOK | wxICON_ERROR);
	}
}

void AreaDecorationDialog::OnClose(wxCloseEvent& event) {
	if (m_engine) {
		m_engine->clearPreview();
		g_gui.RefreshView();
	}
	event.Skip();
}

void AreaDecorationDialog::UpdatePresetList() {
	if (!m_presetChoice) return;

	m_presetChoice->Clear();
	m_presetChoice->Append("(None - Custom)");

	auto& manager = AreaDecoration::PresetManager::getInstance();
	std::vector<std::string> names = manager.getPresetNames();

	for (const auto& name : names) {
		m_presetChoice->Append(wxstr(name));
	}

	m_presetChoice->SetSelection(0);
}

void AreaDecorationDialog::LoadPresetToUI(const AreaDecoration::DecorationPreset& preset) {
	// Clear existing rules and load from preset
	m_preset = preset;

	// Load spacing settings
	m_minDistanceSpin->SetValue(preset.spacing.minDistance);
	m_sameItemDistanceSpin->SetValue(preset.spacing.minSameItemDistance);
	m_checkDiagonalsCheck->SetValue(preset.spacing.checkDiagonals);

	// Load distribution settings
	m_distributionChoice->SetSelection(static_cast<int>(preset.distribution.mode));
	m_clusterStrengthSlider->SetValue(static_cast<int>(preset.distribution.clusterStrength * 100));
	m_clusterCountSpin->SetValue(preset.distribution.clusterCount);
	m_gridSpacingXSpin->SetValue(preset.distribution.gridSpacingX);
	m_gridSpacingYSpin->SetValue(preset.distribution.gridSpacingY);
	m_gridJitterSpin->SetValue(preset.distribution.gridJitter);

	// Load limits
	m_maxItemsSpin->SetValue(preset.maxItemsTotal);
	m_skipBlockedCheck->SetValue(preset.skipBlockedTiles);

	// Load seed settings
	if (preset.defaultSeed != 0) {
		m_useSeedCheck->SetValue(true);
		m_seedInput->SetValue(wxString::Format("%llu", preset.defaultSeed));
	} else {
		m_useSeedCheck->SetValue(false);
		m_seedInput->SetValue("0");
	}

	// Update preset name input
	m_presetNameInput->SetValue(wxstr(preset.name));

	// Update UI
	UpdateRulesList();
	UpdateUI();
}

void AreaDecorationDialog::OnPresetSelected(wxCommandEvent& event) {
	int sel = m_presetChoice->GetSelection();
	if (sel <= 0) {
		// "(None - Custom)" selected, don't load anything
		return;
	}

	wxString presetName = m_presetChoice->GetString(sel);
	auto& manager = AreaDecoration::PresetManager::getInstance();
	const AreaDecoration::DecorationPreset* preset = manager.getPreset(presetName.ToStdString());

	if (preset) {
		LoadPresetToUI(*preset);
	}
}

void AreaDecorationDialog::OnSavePreset(wxCommandEvent& event) {
	wxString name = m_presetNameInput->GetValue().Trim().Trim(false);
	if (name.IsEmpty()) {
		wxMessageBox("Please enter a preset name", "Error", wxOK | wxICON_ERROR);
		return;
	}

	// Build current settings into preset
	BuildPresetFromUI();
	m_preset.name = name.ToStdString();

	auto& manager = AreaDecoration::PresetManager::getInstance();

	// Check if preset already exists
	if (manager.getPreset(name.ToStdString()) != nullptr) {
		int result = wxMessageBox(
			wxString::Format("Preset '%s' already exists. Overwrite?", name),
			"Confirm Overwrite",
			wxYES_NO | wxICON_QUESTION
		);
		if (result != wxYES) {
			return;
		}
		// Remove old preset first
		manager.removePreset(name.ToStdString());
	}

	if (manager.addPreset(m_preset)) {
		if (manager.savePresets()) {
			wxMessageBox(wxString::Format("Preset '%s' saved successfully", name), "Success", wxOK | wxICON_INFORMATION);
			UpdatePresetList();

			// Select the newly saved preset
			int idx = m_presetChoice->FindString(name);
			if (idx != wxNOT_FOUND) {
				m_presetChoice->SetSelection(idx);
			}
		} else {
			wxMessageBox("Failed to save presets to disk", "Error", wxOK | wxICON_ERROR);
		}
	} else {
		wxMessageBox("Failed to add preset", "Error", wxOK | wxICON_ERROR);
	}
}

void AreaDecorationDialog::OnDeletePreset(wxCommandEvent& event) {
	int sel = m_presetChoice->GetSelection();
	if (sel <= 0) {
		wxMessageBox("Select a preset to delete", "Error", wxOK | wxICON_ERROR);
		return;
	}

	wxString presetName = m_presetChoice->GetString(sel);

	int result = wxMessageBox(
		wxString::Format("Are you sure you want to delete preset '%s'?", presetName),
		"Confirm Delete",
		wxYES_NO | wxICON_WARNING
	);

	if (result != wxYES) {
		return;
	}

	auto& manager = AreaDecoration::PresetManager::getInstance();
	if (manager.removePreset(presetName.ToStdString())) {
		if (manager.savePresets()) {
			wxMessageBox(wxString::Format("Preset '%s' deleted", presetName), "Success", wxOK | wxICON_INFORMATION);
			UpdatePresetList();
		} else {
			wxMessageBox("Failed to save changes to disk", "Error", wxOK | wxICON_ERROR);
		}
	} else {
		wxMessageBox("Failed to delete preset", "Error", wxOK | wxICON_ERROR);
	}
}

void AreaDecorationDialog::OnExportPreset(wxCommandEvent& event) {
	wxString name = m_presetNameInput->GetValue().Trim().Trim(false);
	if (name.IsEmpty()) {
		name = "decoration_preset";
	}

	wxFileDialog saveDialog(
		this,
		"Export Preset",
		"",
		name + ".xml",
		"XML files (*.xml)|*.xml|All files (*.*)|*.*",
		wxFD_SAVE | wxFD_OVERWRITE_PROMPT
	);

	if (saveDialog.ShowModal() == wxID_CANCEL) {
		return;
	}

	// Build current settings
	BuildPresetFromUI();
	m_preset.name = name.ToStdString();

	if (m_preset.saveToFile(saveDialog.GetPath().ToStdString())) {
		wxMessageBox("Preset exported successfully", "Success", wxOK | wxICON_INFORMATION);
	} else {
		wxMessageBox("Failed to export preset", "Error", wxOK | wxICON_ERROR);
	}
}

void AreaDecorationDialog::OnImportPreset(wxCommandEvent& event) {
	wxFileDialog openDialog(
		this,
		"Import Preset",
		"",
		"",
		"XML files (*.xml)|*.xml|All files (*.*)|*.*",
		wxFD_OPEN | wxFD_FILE_MUST_EXIST
	);

	if (openDialog.ShowModal() == wxID_CANCEL) {
		return;
	}

	AreaDecoration::DecorationPreset importedPreset;
	if (importedPreset.loadFromFile(openDialog.GetPath().ToStdString())) {
		// Load into UI
		LoadPresetToUI(importedPreset);

		// Ask if user wants to save to presets list
		int result = wxMessageBox(
			wxString::Format("Preset '%s' imported. Would you like to save it to your presets list?", wxstr(importedPreset.name)),
			"Save Imported Preset?",
			wxYES_NO | wxICON_QUESTION
		);

		if (result == wxYES) {
			auto& manager = AreaDecoration::PresetManager::getInstance();

			// Check if already exists
			if (manager.getPreset(importedPreset.name) != nullptr) {
				int overwrite = wxMessageBox(
					wxString::Format("Preset '%s' already exists. Overwrite?", wxstr(importedPreset.name)),
					"Confirm Overwrite",
					wxYES_NO | wxICON_QUESTION
				);
				if (overwrite == wxYES) {
					manager.removePreset(importedPreset.name);
				} else {
					return;
				}
			}

			if (manager.addPreset(importedPreset) && manager.savePresets()) {
				wxMessageBox("Preset saved to your presets list", "Success", wxOK | wxICON_INFORMATION);
				UpdatePresetList();

				int idx = m_presetChoice->FindString(wxstr(importedPreset.name));
				if (idx != wxNOT_FOUND) {
					m_presetChoice->SetSelection(idx);
				}
			}
		}
	} else {
		wxMessageBox("Failed to import preset. The file may be invalid or corrupted.", "Error", wxOK | wxICON_ERROR);
	}
}
