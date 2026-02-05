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

#include "map.h"
#include "gui.h"
#include "raw_brush.h"
#include "tile.h"
#include "graphics.h"
#include "gui.h"
#include "editor.h"
#include "items.h"
#include "action.h"
#include "selection.h"
#include "browse_tile_window.h"

namespace {

const wxColour kBrowseTileBackgroundColour(0x0C, 0x14, 0x2A);
const wxColour kBrowseTileSelectionColour(0x16, 0x24, 0x43);
const wxColour kBrowseTileTextColour(0xE0, 0xE6, 0xFF);

} // namespace

// ============================================================================
//

class BrowseTileListBox : public wxVListBox
{
public:
	BrowseTileListBox(wxWindow* parent, wxWindowID id, Tile* tile);
	~BrowseTileListBox();

	void OnDrawItem(wxDC& dc, const wxRect& rect, size_t index) const;
	wxCoord OnMeasureItem(size_t index) const;
	Item* GetSelectedItem();
	void RemoveSelected();
	void changeIndex(Item* item, bool direction);
	void SetTile(Tile* tile);

protected:
	void UpdateItems();

	typedef std::map<int, Item*> ItemsMap;
	ItemsMap items;
	Tile* edit_tile;
};

BrowseTileListBox::BrowseTileListBox(wxWindow* parent, wxWindowID id, Tile* tile) :
wxVListBox(parent, id, wxDefaultPosition, wxSize(200, 180), wxLB_MULTIPLE), edit_tile(tile)
{
	SetBackgroundColour(kBrowseTileBackgroundColour);
	SetOwnForegroundColour(kBrowseTileTextColour);
	UpdateItems();
}

BrowseTileListBox::~BrowseTileListBox()
{
	////
}

void BrowseTileListBox::OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const
{
	ItemsMap::const_iterator item_iterator = items.find(int(n));
	if(item_iterator == items.end())
		return;

	Item* item = item_iterator->second;

	dc.SetPen(*wxTRANSPARENT_PEN);
	if(IsSelected(n)) {
		dc.SetBrush(wxBrush(kBrowseTileSelectionColour));
	} else {
		dc.SetBrush(wxBrush(kBrowseTileBackgroundColour));
	}
	dc.DrawRectangle(rect);

	Sprite* sprite = g_gui.gfx.getSprite(item->getClientID());
	if(sprite) {
		// Draw thumbnails using the native sprite size so they do not stretch across the entire row.
		sprite->DrawTo(&dc, SPRITE_SIZE_32x32, rect.GetX(), rect.GetY(), 32, 32);
	}

	if(IsSelected(n)) {
		item->select();
		dc.SetTextForeground(wxColor(0xFF, 0xFF, 0xFF));
	} else {
		item->deselect();
		dc.SetTextForeground(kBrowseTileTextColour);
	}

	wxString label;
	label << item->getID() << " - " << item->getName();
	dc.DrawText(label, rect.GetX() + 40, rect.GetY() + 6);
}

wxCoord BrowseTileListBox::OnMeasureItem(size_t n) const
{
	return 32;
}

Item* BrowseTileListBox::GetSelectedItem()
{
	if(edit_tile == nullptr || GetItemCount() == 0 || GetSelectedCount() == 0)
		return nullptr;

	return edit_tile->getTopSelectedItem();
}

void BrowseTileListBox::changeIndex(Item* item, bool direction)
{
	if(!edit_tile || !item)
		return;

	std::vector<Item*> selected_items;
	selected_items.reserve(GetSelectedCount());
	for(size_t i = 0; i < GetItemCount(); ++i) {
		if(!IsSelected(i)) {
			continue;
		}
		ItemsMap::const_iterator item_iterator = items.find(int(i));
		if(item_iterator != items.end()) {
			selected_items.push_back(item_iterator->second);
		}
	}

	if (direction == false) {
		edit_tile->moveItemToIndex(item, edit_tile->getIndexOf(item) - 1);
	} else {
		edit_tile->moveItemToIndex(item, edit_tile->getIndexOf(item) + 1);
	}

	UpdateItems();
	for(Item* selected_item : selected_items) {
		for(const auto& entry : items) {
			if(entry.second == selected_item) {
				Select(entry.first, true);
				break;
			}
		}
	}
	Refresh();
}

void BrowseTileListBox::RemoveSelected()
{
	if(!edit_tile || GetItemCount() == 0 || GetSelectedCount() == 0) return;

	Clear();
	items.clear();

	// Delete the items from the tile
	ItemVector tile_selection = edit_tile->popSelectedItems(true);
	for(ItemVector::iterator iit = tile_selection.begin(); iit != tile_selection.end(); ++iit) {
		delete *iit;
	}

	UpdateItems();
	Refresh();
}

void BrowseTileListBox::UpdateItems()
{
	items.clear();
	if(!edit_tile) {
		SetItemCount(0);
		return;
	}

	int n = 0;
	for(ItemVector::reverse_iterator it = edit_tile->items.rbegin(); it != edit_tile->items.rend(); ++it) {
		items[n] = (*it);
		++n;
	}

	if(edit_tile->ground) {
		items[n] = edit_tile->ground;
		++n;
	}

	SetItemCount(n);
}

void BrowseTileListBox::SetTile(Tile* tile)
{
	edit_tile = tile;
	UpdateItems();
	Refresh();
}

// ============================================================================
//

BEGIN_EVENT_TABLE(BrowseTileWindow, wxDialog)
	EVT_BUTTON(wxID_REMOVE, BrowseTileWindow::OnClickDelete)
	EVT_BUTTON(wxID_FIND, BrowseTileWindow::OnClickSelectRaw)
	EVT_BUTTON(wxID_OK, BrowseTileWindow::OnClickOK)
	EVT_BUTTON(wxID_CANCEL, BrowseTileWindow::OnClickCancel)
	EVT_BUTTON(BROWSE_TILE_MOVE_UP, BrowseTileWindow::OnClickMoveUp)
	EVT_BUTTON(BROWSE_TILE_MOVE_DOWN, BrowseTileWindow::OnClickMoveDown)
END_EVENT_TABLE()

BrowseTileWindow::BrowseTileWindow(wxWindow* parent, Tile* tile, wxPoint position /* = wxDefaultPosition */) :
wxDialog(parent, wxID_ANY, "Browse Field", position, wxSize(600, 400), wxCAPTION | wxCLOSE_BOX | wxRESIZE_BORDER),
edit_tile(tile),
auto_apply_enabled(false)
{
	SetBackgroundColour(kBrowseTileBackgroundColour);

	wxSizer* sizer = newd wxBoxSizer(wxVERTICAL);
	item_list = newd BrowseTileListBox(this, wxID_ANY, tile);
	sizer->Add(item_list, wxSizerFlags(1).Expand());

	wxString pos;
	pos << "x=" << tile->getX() << ",  y=" << tile->getY() << ",  z=" << tile->getZ();

	wxSizer* infoSizer = newd wxBoxSizer(wxVERTICAL);
    wxBoxSizer* buttons = newd wxBoxSizer(wxHORIZONTAL);

	moveup_button = newd wxButton(this, BROWSE_TILE_MOVE_UP, "Move Up");
	moveup_button->Enable(false);
	buttons->Add(moveup_button);
	buttons->AddSpacer(5);
	movedown_button = newd wxButton(this, BROWSE_TILE_MOVE_DOWN, "Move Down");
	movedown_button->Enable(false);
	buttons->Add(movedown_button);
	buttons->AddSpacer(5);

	delete_button = newd wxButton(this, wxID_REMOVE, "Delete");
	delete_button->Enable(false);
	buttons->Add(delete_button);
	buttons->AddSpacer(5);
	select_raw_button = newd wxButton(this, wxID_FIND, "Select RAW");
	select_raw_button->SetToolTip("Uses the selected item as a RAW brush");
	select_raw_button->Enable(false);
	buttons->Add(select_raw_button);
	infoSizer->Add(buttons);
	infoSizer->AddSpacer(5);
	auto add_info_label = [&](const wxString& text) -> wxStaticText* {
		wxStaticText* label = newd wxStaticText(this, wxID_ANY, text);
		label->SetForegroundColour(kBrowseTileTextColour);
		return label;
	};

	infoSizer->Add(add_info_label("Position:  " + pos), wxSizerFlags(0).Left());
	item_count_txt = add_info_label("Item count:  " + i2ws(item_list->GetItemCount()));
	infoSizer->Add(item_count_txt, wxSizerFlags(0).Left());
	infoSizer->Add(add_info_label("Protection zone:  " + b2yn(tile->isPZ())), wxSizerFlags(0).Left());
	infoSizer->Add(add_info_label("No PvP:  " + b2yn(tile->getMapFlags() & TILESTATE_NOPVP)), wxSizerFlags(0).Left());
	infoSizer->Add(add_info_label("No logout:  " + b2yn(tile->getMapFlags() & TILESTATE_NOLOGOUT)), wxSizerFlags(0).Left());
	infoSizer->Add(add_info_label("PvP zone:  " + b2yn(tile->getMapFlags() & TILESTATE_PVPZONE)), wxSizerFlags(0).Left());
	infoSizer->Add(add_info_label("World Boss zone:  " + b2yn(tile->getMapFlags() & TILESTATE_WORLDBOSS)), wxSizerFlags(0).Left());
	infoSizer->Add(add_info_label("House:  " + b2yn(tile->isHouseTile())), wxSizerFlags(0).Left());

	sizer->Add(infoSizer, wxSizerFlags(0).Left().DoubleBorder());

	// Auto-apply checkbox
	auto_apply_checkbox = newd wxCheckBox(this, wxID_ANY, "Auto Apply");
	auto_apply_checkbox->SetForegroundColour(kBrowseTileTextColour);
	auto_apply_checkbox->SetToolTip("Automatically apply changes after delete or move operations");
	sizer->Add(auto_apply_checkbox, wxSizerFlags(0).Left().Border(wxLEFT | wxBOTTOM, 10));

	// OK/Cancel buttons
	wxSizer* btnSizer = newd wxBoxSizer(wxHORIZONTAL);
	btnSizer->Add(newd wxButton(this, wxID_OK, "OK"), wxSizerFlags(0).Center());
	btnSizer->Add(newd wxButton(this, wxID_CANCEL, "Cancel"), wxSizerFlags(0).Center());
	sizer->Add(btnSizer, wxSizerFlags(0).Center().DoubleBorder());

	SetSizerAndFit(sizer);

	// Connect Events
	item_list->Connect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(BrowseTileWindow::OnItemSelected), NULL, this);
	auto_apply_checkbox->Bind(wxEVT_CHECKBOX, &BrowseTileWindow::OnToggleAutoApply, this);
}

BrowseTileWindow::~BrowseTileWindow()
{
	// Disconnect Events
	item_list->Disconnect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(BrowseTileWindow::OnItemSelected), NULL, this);
}

void BrowseTileWindow::OnItemSelected(wxCommandEvent& WXUNUSED(event))
{
	const size_t count = item_list->GetSelectedCount();
	delete_button->Enable(count != 0);
	select_raw_button->Enable(count == 1);

	moveup_button->Enable(count != 0);
	movedown_button->Enable(count != 0);

	if(count != 0) {
		g_gui.SelectPalettePage(TILESET_RAW);

		Item* item = item_list->GetSelectedItem();
		if(item && item->getRAWBrush()) {
			g_gui.SelectBrush(item->getRAWBrush(), TILESET_RAW);
		}
	}
}

void BrowseTileWindow::OnClickDelete(wxCommandEvent& WXUNUSED(event))
{
	item_list->RemoveSelected();
	item_count_txt->SetLabelText("Item count:  " + i2ws(item_list->GetItemCount()));
	TriggerAutoApply();
}

void BrowseTileWindow::OnClickSelectRaw(wxCommandEvent& WXUNUSED(event))
{
	Item* item = item_list->GetSelectedItem();
	if(item && item->getRAWBrush())
		g_gui.SelectBrush(item->getRAWBrush(), TILESET_RAW);

	EndModal(1);
}

void BrowseTileWindow::OnClickOK(wxCommandEvent& WXUNUSED(event))
{
	EndModal(1);
}

void BrowseTileWindow::OnClickCancel(wxCommandEvent& WXUNUSED(event))
{
	EndModal(0);
}

void BrowseTileWindow::OnClickMoveUp(wxCommandEvent& WXUNUSED(event))
{
	Item* item = item_list->GetSelectedItem();
	if(item) {
		item_list->changeIndex(item, true);
		TriggerAutoApply();
	}
}

void BrowseTileWindow::OnClickMoveDown(wxCommandEvent& WXUNUSED(event))
{
	Item* item = item_list->GetSelectedItem();
	if(item) {
		item_list->changeIndex(item, false);
		TriggerAutoApply();
	}
}

void BrowseTileWindow::OnToggleAutoApply(wxCommandEvent& event)
{
	auto_apply_enabled = event.IsChecked();
}

void BrowseTileWindow::TriggerAutoApply()
{
	if(!auto_apply_enabled || !edit_tile)
		return;

	Editor* editor = g_gui.GetCurrentEditor();
	if(!editor)
		return;

	Tile* commit_tile = edit_tile->deepCopy(editor->getMap());
	Action* action = editor->createAction(ACTION_DELETE_TILES);
	action->addChange(newd Change(commit_tile));
	editor->addAction(action);
	editor->updateActions();
}

// ============================================================================
// BrowseTilePanel implementation

namespace {

wxString FormatPosition(const Tile* tile)
{
	if(!tile) {
		return "-";
	}

	wxString pos;
	pos << "x=" << tile->getX() << ",  y=" << tile->getY() << ",  z=" << tile->getZ();
	return pos;
}

} // namespace

BrowseTilePanel::BrowseTilePanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY),
	item_list(newd BrowseTileListBox(this, wxID_ANY, nullptr)),
	item_count_txt(nullptr),
	position_txt(nullptr),
	protection_txt(nullptr),
	nopvp_txt(nullptr),
	nologout_txt(nullptr),
	pvpzone_txt(nullptr),
	worldboss_txt(nullptr),
	house_txt(nullptr),
	delete_button(nullptr),
	select_raw_button(nullptr),
	moveup_button(nullptr),
	movedown_button(nullptr),
	apply_button(nullptr),
	auto_apply_checkbox(nullptr),
	working_tile(nullptr),
	source_editor(nullptr),
	working_position_valid(false),
	auto_apply_enabled(false)
{
	SetBackgroundColour(kBrowseTileBackgroundColour);

	auto* root_sizer = newd wxBoxSizer(wxVERTICAL);
	root_sizer->Add(item_list, 1, wxEXPAND | wxALL, 5);

	auto* buttons = newd wxBoxSizer(wxHORIZONTAL);
	moveup_button = newd wxButton(this, wxID_ANY, "Move Up");
	movedown_button = newd wxButton(this, wxID_ANY, "Move Down");
	delete_button = newd wxButton(this, wxID_ANY, "Delete");
	select_raw_button = newd wxButton(this, wxID_FIND, "Select RAW");
	select_raw_button->SetToolTip("Uses the selected item as a RAW brush");
	select_raw_button->Enable(false);

	buttons->Add(moveup_button, 0, wxRIGHT, 5);
	buttons->Add(movedown_button, 0, wxRIGHT, 5);
	buttons->Add(delete_button, 0, wxRIGHT, 5);
	buttons->Add(select_raw_button, 0);
	root_sizer->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT, 5);

	auto* infoSizer = newd wxBoxSizer(wxVERTICAL);
	position_txt = newd wxStaticText(this, wxID_ANY, "Position:  -");
	item_count_txt = newd wxStaticText(this, wxID_ANY, "Item count:  0");
	protection_txt = newd wxStaticText(this, wxID_ANY, "Protection zone:  -");
	nopvp_txt = newd wxStaticText(this, wxID_ANY, "No PvP:  -");
	nologout_txt = newd wxStaticText(this, wxID_ANY, "No logout:  -");
	pvpzone_txt = newd wxStaticText(this, wxID_ANY, "PvP zone:  -");
	worldboss_txt = newd wxStaticText(this, wxID_ANY, "World Boss zone:  -");
	house_txt = newd wxStaticText(this, wxID_ANY, "House:  -");

	auto apply_info_colour = [](wxStaticText* text) {
		text->SetForegroundColour(kBrowseTileTextColour);
	};
	apply_info_colour(position_txt);
	apply_info_colour(item_count_txt);
	apply_info_colour(protection_txt);
	apply_info_colour(nopvp_txt);
	apply_info_colour(nologout_txt);
	apply_info_colour(pvpzone_txt);
	apply_info_colour(worldboss_txt);
	apply_info_colour(house_txt);

	infoSizer->Add(position_txt, 0, wxBOTTOM, 1);
	infoSizer->Add(item_count_txt, 0, wxBOTTOM, 1);
	infoSizer->Add(protection_txt, 0, wxBOTTOM, 1);
	infoSizer->Add(nopvp_txt, 0, wxBOTTOM, 1);
	infoSizer->Add(nologout_txt, 0, wxBOTTOM, 1);
	infoSizer->Add(pvpzone_txt, 0, wxBOTTOM, 1);
	infoSizer->Add(worldboss_txt, 0, wxBOTTOM, 1);
	infoSizer->Add(house_txt, 0);

	root_sizer->Add(infoSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	apply_button = newd wxButton(this, wxID_ANY, "Apply Changes");
	root_sizer->Add(apply_button, 0, wxEXPAND | wxALL, 5);

	auto_apply_checkbox = newd wxCheckBox(this, wxID_ANY, "Auto Apply");
	auto_apply_checkbox->SetForegroundColour(kBrowseTileTextColour);
	auto_apply_checkbox->SetToolTip("Automatically apply changes after delete or move operations");
	root_sizer->Add(auto_apply_checkbox, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

	SetSizer(root_sizer);

	item_list->Bind(wxEVT_COMMAND_LISTBOX_SELECTED, &BrowseTilePanel::OnItemSelected, this);
	delete_button->Bind(wxEVT_BUTTON, &BrowseTilePanel::OnClickDelete, this);
	select_raw_button->Bind(wxEVT_BUTTON, &BrowseTilePanel::OnClickSelectRaw, this);
	moveup_button->Bind(wxEVT_BUTTON, &BrowseTilePanel::OnClickMoveUp, this);
	movedown_button->Bind(wxEVT_BUTTON, &BrowseTilePanel::OnClickMoveDown, this);
	apply_button->Bind(wxEVT_BUTTON, &BrowseTilePanel::OnClickApply, this);
	auto_apply_checkbox->Bind(wxEVT_CHECKBOX, &BrowseTilePanel::OnToggleAutoApply, this);

	UpdateControlStates();
	UpdateTileInfo();
}

BrowseTilePanel::~BrowseTilePanel()
{
	delete working_tile;
}

void BrowseTilePanel::ClearSelection()
{
	source_editor = nullptr;
	working_position = Position();
	working_position_valid = false;
	ResetTile(nullptr);
	UpdateTileInfo();
	UpdateControlStates();
}

void BrowseTilePanel::SetTile(Tile* tile, Editor* editor)
{
	if(!tile || !editor) {
		ClearSelection();
		return;
	}

	Tile* copy = tile->deepCopy(editor->getMap());
	working_position = Position(tile->getX(), tile->getY(), tile->getZ());
	working_position_valid = true;
	source_editor = editor;
	ResetTile(copy);
	UpdateTileInfo();
	UpdateControlStates();
}

void BrowseTilePanel::OnItemSelected(wxCommandEvent& WXUNUSED(event))
{
	if(!HasTile()) {
		delete_button->Enable(false);
		select_raw_button->Enable(false);
		moveup_button->Enable(false);
		movedown_button->Enable(false);
		return;
	}

	const size_t count = item_list->GetSelectedCount();
	delete_button->Enable(count != 0);
	select_raw_button->Enable(count == 1);
	moveup_button->Enable(count != 0);
	movedown_button->Enable(count != 0);

	if(count != 0) {
		g_gui.SelectPalettePage(TILESET_RAW);

		Item* item = item_list->GetSelectedItem();
		if(item && item->getRAWBrush()) {
			g_gui.SelectBrush(item->getRAWBrush(), TILESET_RAW);
		}
	}
}

void BrowseTilePanel::OnClickDelete(wxCommandEvent& WXUNUSED(event))
{
	if(!HasTile())
		return;

	item_list->RemoveSelected();
	item_count_txt->SetLabel("Item count:  " + i2ws(item_list->GetItemCount()));
	UpdateTileInfo();
	UpdateControlStates();
	TriggerAutoApply();
}

void BrowseTilePanel::OnClickSelectRaw(wxCommandEvent& WXUNUSED(event))
{
	if(!HasTile())
		return;

	Item* item = item_list->GetSelectedItem();
	if(item && item->getRAWBrush())
		g_gui.SelectBrush(item->getRAWBrush(), TILESET_RAW);
}

void BrowseTilePanel::OnClickMoveUp(wxCommandEvent& WXUNUSED(event))
{
	if(!HasTile())
		return;

	Item* item = item_list->GetSelectedItem();
	if(item) {
		item_list->changeIndex(item, true);
		TriggerAutoApply();
	}
}

void BrowseTilePanel::OnClickMoveDown(wxCommandEvent& WXUNUSED(event))
{
	if(!HasTile())
		return;

	Item* item = item_list->GetSelectedItem();
	if(item) {
		item_list->changeIndex(item, false);
		TriggerAutoApply();
	}
}

void BrowseTilePanel::OnClickApply(wxCommandEvent& WXUNUSED(event))
{
	if(!HasTile() || !source_editor)
		return;

	Tile* commit_tile = working_tile->deepCopy(source_editor->getMap());
	Action* action = source_editor->createAction(ACTION_DELETE_TILES);
	action->addChange(newd Change(commit_tile));
	source_editor->addAction(action);
	source_editor->updateActions();

	RefreshFromSourceTile();
}

void BrowseTilePanel::UpdateTileInfo()
{
	const Tile* tile = working_tile;
	position_txt->SetLabel("Position:  " + FormatPosition(tile));
	item_count_txt->SetLabel("Item count:  " + i2ws(tile ? item_list->GetItemCount() : 0));

	const wxString protection = tile ? b2yn(tile->isPZ()) : wxString("-");
	const wxString nopvp = tile ? b2yn(tile->getMapFlags() & TILESTATE_NOPVP) : wxString("-");
	const wxString nologout = tile ? b2yn(tile->getMapFlags() & TILESTATE_NOLOGOUT) : wxString("-");
	const wxString pvpzone = tile ? b2yn(tile->getMapFlags() & TILESTATE_PVPZONE) : wxString("-");
	const wxString worldboss = tile ? b2yn(tile->getMapFlags() & TILESTATE_WORLDBOSS) : wxString("-");
	const wxString house = tile ? b2yn(tile->isHouseTile()) : wxString("-");

	protection_txt->SetLabel("Protection zone:  " + protection);
	nopvp_txt->SetLabel("No PvP:  " + nopvp);
	nologout_txt->SetLabel("No logout:  " + nologout);
	pvpzone_txt->SetLabel("PvP zone:  " + pvpzone);
	worldboss_txt->SetLabel("World Boss zone:  " + worldboss);
	house_txt->SetLabel("House:  " + house);
}

void BrowseTilePanel::UpdateControlStates()
{
	const bool has_tile = HasTile();
	item_list->Enable(has_tile);
	apply_button->Enable(has_tile && source_editor != nullptr);
	delete_button->Enable(false);
	select_raw_button->Enable(false);
	moveup_button->Enable(false);
	movedown_button->Enable(false);
}

void BrowseTilePanel::ResetTile(Tile* new_tile)
{
	if(working_tile == new_tile)
		return;

	delete working_tile;
	working_tile = new_tile;
	item_list->SetTile(working_tile);
}

void BrowseTilePanel::RefreshFromSourceTile()
{
	if(!source_editor || !working_position_valid)
		return;

	Tile* tile = source_editor->getMap().getTile(working_position);
	if(!tile) {
		ClearSelection();
		return;
	}

	Tile* tile_copy = tile->deepCopy(source_editor->getMap());
	ResetTile(tile_copy);
	UpdateTileInfo();
	UpdateControlStates();
}

void BrowseTilePanel::OnToggleAutoApply(wxCommandEvent& event)
{
	auto_apply_enabled = event.IsChecked();
}

void BrowseTilePanel::TriggerAutoApply()
{
	if(!auto_apply_enabled || !HasTile() || !source_editor)
		return;

	Tile* commit_tile = working_tile->deepCopy(source_editor->getMap());
	Action* action = source_editor->createAction(ACTION_DELETE_TILES);
	action->addChange(newd Change(commit_tile));
	source_editor->addAction(action);
	source_editor->updateActions();

	RefreshFromSourceTile();
}

// ============================================================================
// SelectionItemListBox implementation

SelectionItemListBox::SelectionItemListBox(wxWindow* parent, wxWindowID id) :
	wxVListBox(parent, id, wxDefaultPosition, wxSize(200, 150), wxLB_SINGLE)
{
	SetBackgroundColour(kBrowseTileBackgroundColour);
	SetItemCount(0);
}

SelectionItemListBox::~SelectionItemListBox()
{
}

void SelectionItemListBox::OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const
{
	if(n >= item_ids.size())
		return;

	uint16_t itemId = item_ids[n];
	const ItemType& type = g_items.getItemType(itemId);

	dc.SetPen(*wxTRANSPARENT_PEN);
	if(IsSelected(n)) {
		dc.SetBrush(wxBrush(kBrowseTileSelectionColour));
	} else {
		dc.SetBrush(wxBrush(kBrowseTileBackgroundColour));
	}
	dc.DrawRectangle(rect);

	Sprite* sprite = g_gui.gfx.getSprite(type.clientID);
	if(sprite) {
		sprite->DrawTo(&dc, SPRITE_SIZE_32x32, rect.GetX(), rect.GetY(), 32, 32);
	}

	if(IsSelected(n)) {
		dc.SetTextForeground(wxColor(0xFF, 0xFF, 0xFF));
	} else {
		dc.SetTextForeground(kBrowseTileTextColour);
	}

	wxString label;
	label << itemId << " - " << wxstr(type.name);
	dc.DrawText(label, rect.GetX() + 40, rect.GetY() + 6);
}

wxCoord SelectionItemListBox::OnMeasureItem(size_t n) const
{
	return 32;
}

void SelectionItemListBox::AddItem(uint16_t itemId)
{
	// Check if item already exists
	for(uint16_t id : item_ids) {
		if(id == itemId) return;
	}
	item_ids.push_back(itemId);
	UpdateList();
}

void SelectionItemListBox::RemoveSelected()
{
	int sel = GetSelection();
	if(sel == wxNOT_FOUND || sel >= static_cast<int>(item_ids.size()))
		return;

	item_ids.erase(item_ids.begin() + sel);
	UpdateList();

	if(!item_ids.empty()) {
		if(sel >= static_cast<int>(item_ids.size())) {
			sel = item_ids.size() - 1;
		}
		SetSelection(sel);
	}
}

void SelectionItemListBox::MoveSelectedUp()
{
	int sel = GetSelection();
	if(sel == wxNOT_FOUND || sel <= 0)
		return;

	std::swap(item_ids[sel], item_ids[sel - 1]);
	UpdateList();
	SetSelection(sel - 1);
}

void SelectionItemListBox::MoveSelectedDown()
{
	int sel = GetSelection();
	if(sel == wxNOT_FOUND || sel >= static_cast<int>(item_ids.size()) - 1)
		return;

	std::swap(item_ids[sel], item_ids[sel + 1]);
	UpdateList();
	SetSelection(sel + 1);
}

void SelectionItemListBox::Clear()
{
	item_ids.clear();
	UpdateList();
}

uint16_t SelectionItemListBox::GetSelectedItemId() const
{
	int sel = GetSelection();
	if(sel == wxNOT_FOUND || sel >= static_cast<int>(item_ids.size()))
		return 0;
	return item_ids[sel];
}

void SelectionItemListBox::UpdateList()
{
	SetItemCount(item_ids.size());
	Refresh();
}

// ============================================================================
// ApplySelectionPanel implementation

ApplySelectionPanel::ApplySelectionPanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY),
	search_ctrl(nullptr),
	search_results(nullptr),
	add_button(nullptr),
	from_id_ctrl(nullptr),
	to_id_ctrl(nullptr),
	add_range_button(nullptr),
	item_list(nullptr),
	remove_button(nullptr),
	moveup_button(nullptr),
	movedown_button(nullptr),
	clear_button(nullptr),
	load_from_selection_checkbox(nullptr),
	load_selection_button(nullptr),
	status_label(nullptr),
	selection_info_label(nullptr)
{
	SetBackgroundColour(kBrowseTileBackgroundColour);

	auto* root_sizer = newd wxBoxSizer(wxVERTICAL);

	// Title/Description
	auto* title_label = newd wxStaticText(this, wxID_ANY, "Apply to Selection");
	title_label->SetForegroundColour(kBrowseTileTextColour);
	title_label->SetFont(title_label->GetFont().Bold());
	root_sizer->Add(title_label, 0, wxALL, 5);

	auto* desc_label = newd wxStaticText(this, wxID_ANY,
		"Add items to the list, then use Up/Down to move them one layer in all selected tiles.");
	desc_label->SetForegroundColour(kBrowseTileTextColour);
	desc_label->Wrap(280);
	root_sizer->Add(desc_label, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Selection info
	selection_info_label = newd wxStaticText(this, wxID_ANY, "Selection: 0 tiles");
	selection_info_label->SetForegroundColour(kBrowseTileTextColour);
	root_sizer->Add(selection_info_label, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Search section
	auto* search_box = newd wxStaticBox(this, wxID_ANY, "Search Items");
	search_box->SetForegroundColour(kBrowseTileTextColour);
	auto* search_sizer = newd wxStaticBoxSizer(search_box, wxVERTICAL);

	search_ctrl = newd wxSearchCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	search_ctrl->SetDescriptiveText("Enter item ID or name...");
	search_sizer->Add(search_ctrl, 0, wxEXPAND | wxALL, 3);

	search_results = newd wxListBox(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 80));
	search_results->SetBackgroundColour(kBrowseTileBackgroundColour);
	search_results->SetForegroundColour(kBrowseTileTextColour);
	search_sizer->Add(search_results, 1, wxEXPAND | wxALL, 3);

	add_button = newd wxButton(this, wxID_ANY, "Add to List");
	add_button->Enable(false);
	search_sizer->Add(add_button, 0, wxEXPAND | wxALL, 3);

	root_sizer->Add(search_sizer, 0, wxEXPAND | wxALL, 5);

	// Range input section
	auto* range_box = newd wxStaticBox(this, wxID_ANY, "Add by ID Range");
	range_box->SetForegroundColour(kBrowseTileTextColour);
	auto* range_sizer = newd wxStaticBoxSizer(range_box, wxVERTICAL);

	auto* range_inputs = newd wxBoxSizer(wxHORIZONTAL);
	auto* from_label = newd wxStaticText(this, wxID_ANY, "From:");
	from_label->SetForegroundColour(kBrowseTileTextColour);
	from_id_ctrl = newd wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(60, -1));
	auto* to_label = newd wxStaticText(this, wxID_ANY, "To:");
	to_label->SetForegroundColour(kBrowseTileTextColour);
	to_id_ctrl = newd wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(60, -1));

	range_inputs->Add(from_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	range_inputs->Add(from_id_ctrl, 1, wxRIGHT, 10);
	range_inputs->Add(to_label, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	range_inputs->Add(to_id_ctrl, 1);
	range_sizer->Add(range_inputs, 0, wxEXPAND | wxALL, 3);

	add_range_button = newd wxButton(this, wxID_ANY, "Add Range");
	range_sizer->Add(add_range_button, 0, wxEXPAND | wxALL, 3);

	root_sizer->Add(range_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Item manipulation list section
	auto* list_box = newd wxStaticBox(this, wxID_ANY, "Items to Move");
	list_box->SetForegroundColour(kBrowseTileTextColour);
	auto* list_sizer = newd wxStaticBoxSizer(list_box, wxVERTICAL);

	item_list = newd SelectionItemListBox(this, wxID_ANY);
	list_sizer->Add(item_list, 1, wxEXPAND | wxALL, 3);

	remove_button = newd wxButton(this, wxID_ANY, "Remove from List");
	list_sizer->Add(remove_button, 0, wxEXPAND | wxALL, 3);

	root_sizer->Add(list_sizer, 1, wxEXPAND | wxALL, 5);

	// Move buttons - these apply to the map directly
	auto* move_box = newd wxStaticBox(this, wxID_ANY, "Move Layer in Selection");
	move_box->SetForegroundColour(kBrowseTileTextColour);
	auto* move_sizer = newd wxStaticBoxSizer(move_box, wxVERTICAL);

	auto* move_buttons = newd wxBoxSizer(wxHORIZONTAL);
	moveup_button = newd wxButton(this, wxID_ANY, "Move Up");
	moveup_button->SetToolTip("Move all listed items one layer UP (on top) in all selected tiles");
	movedown_button = newd wxButton(this, wxID_ANY, "Move Down");
	movedown_button->SetToolTip("Move all listed items one layer DOWN (below) in all selected tiles");

	move_buttons->Add(moveup_button, 1, wxRIGHT, 5);
	move_buttons->Add(movedown_button, 1);
	move_sizer->Add(move_buttons, 0, wxEXPAND | wxALL, 3);

	root_sizer->Add(move_sizer, 0, wxEXPAND | wxALL, 5);

	// Action buttons
	auto* action_sizer = newd wxBoxSizer(wxHORIZONTAL);
	clear_button = newd wxButton(this, wxID_ANY, "Clear List");
	action_sizer->Add(clear_button, 1);
	root_sizer->Add(action_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Load from selection option
	auto* load_sizer = newd wxBoxSizer(wxHORIZONTAL);
	load_from_selection_checkbox = newd wxCheckBox(this, wxID_ANY, "Load items from selection");
	load_from_selection_checkbox->SetForegroundColour(kBrowseTileTextColour);
	load_from_selection_checkbox->SetToolTip("When enabled, clicking 'Load Selection' will add all unique items from selected tiles to the list");
	load_selection_button = newd wxButton(this, wxID_ANY, "Load Selection");
	load_selection_button->SetToolTip("Load all unique items from selected tiles into the list");
	load_sizer->Add(load_from_selection_checkbox, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	load_sizer->Add(load_selection_button, 0);
	root_sizer->Add(load_sizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

	// Status
	status_label = newd wxStaticText(this, wxID_ANY, "");
	status_label->SetForegroundColour(kBrowseTileTextColour);
	root_sizer->Add(status_label, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

	SetSizer(root_sizer);

	// Bind events
	search_ctrl->Bind(wxEVT_TEXT, &ApplySelectionPanel::OnSearchChanged, this);
	search_ctrl->Bind(wxEVT_SEARCHCTRL_SEARCH_BTN, &ApplySelectionPanel::OnSearchChanged, this);
	search_results->Bind(wxEVT_LISTBOX, &ApplySelectionPanel::OnSearchResultSelected, this);
	search_results->Bind(wxEVT_LISTBOX_DCLICK, &ApplySelectionPanel::OnAddItem, this);
	add_button->Bind(wxEVT_BUTTON, &ApplySelectionPanel::OnAddItem, this);
	remove_button->Bind(wxEVT_BUTTON, &ApplySelectionPanel::OnRemoveItem, this);
	moveup_button->Bind(wxEVT_BUTTON, &ApplySelectionPanel::OnMoveUp, this);
	movedown_button->Bind(wxEVT_BUTTON, &ApplySelectionPanel::OnMoveDown, this);
	load_selection_button->Bind(wxEVT_BUTTON, &ApplySelectionPanel::OnLoadSelection, this);
	clear_button->Bind(wxEVT_BUTTON, &ApplySelectionPanel::OnClear, this);
	add_range_button->Bind(wxEVT_BUTTON, &ApplySelectionPanel::OnAddRange, this);
	item_list->Bind(wxEVT_LISTBOX, &ApplySelectionPanel::OnItemListSelected, this);

	UpdateButtonStates();
}

ApplySelectionPanel::~ApplySelectionPanel()
{
}

void ApplySelectionPanel::OnSearchChanged(wxCommandEvent& WXUNUSED(event))
{
	PopulateSearchResults();
}

void ApplySelectionPanel::OnSearchResultSelected(wxCommandEvent& WXUNUSED(event))
{
	add_button->Enable(search_results->GetSelection() != wxNOT_FOUND);
}

void ApplySelectionPanel::OnAddItem(wxCommandEvent& WXUNUSED(event))
{
	int sel = search_results->GetSelection();
	if(sel == wxNOT_FOUND || sel >= static_cast<int>(filtered_items.size()))
		return;

	uint16_t itemId = filtered_items[sel];
	item_list->AddItem(itemId);
	UpdateButtonStates();
}

void ApplySelectionPanel::OnAddRange(wxCommandEvent& WXUNUSED(event))
{
	long fromId = 0, toId = 0;

	if(!from_id_ctrl->GetValue().ToLong(&fromId) || !to_id_ctrl->GetValue().ToLong(&toId)) {
		status_label->SetLabel("Error: Invalid ID values.");
		return;
	}

	if(fromId <= 0 || toId <= 0) {
		status_label->SetLabel("Error: IDs must be positive.");
		return;
	}

	if(fromId > toId) {
		std::swap(fromId, toId);
	}

	// Limit range to prevent too many items
	if(toId - fromId > 1000) {
		status_label->SetLabel("Error: Range too large (max 1000 items).");
		return;
	}

	int added = 0;
	for(long id = fromId; id <= toId; ++id) {
		const ItemType& type = g_items.getItemType(static_cast<uint16_t>(id));
		if(type.id != 0 && type.clientID != 0) {
			item_list->AddItem(static_cast<uint16_t>(id));
			added++;
		}
	}

	UpdateButtonStates();
	status_label->SetLabel(wxString::Format("Added %d item(s) from range %ld-%ld.", added, fromId, toId));
}

void ApplySelectionPanel::OnRemoveItem(wxCommandEvent& WXUNUSED(event))
{
	item_list->RemoveSelected();
	UpdateButtonStates();
}

void ApplySelectionPanel::OnMoveUp(wxCommandEvent& WXUNUSED(event))
{
	// Move all items in the list UP one layer (higher z-order) in all selected tiles
	MoveItemsInSelection(true);
}

void ApplySelectionPanel::OnMoveDown(wxCommandEvent& WXUNUSED(event))
{
	// Move all items in the list DOWN one layer (lower z-order) in all selected tiles
	MoveItemsInSelection(false);
}

void ApplySelectionPanel::OnClear(wxCommandEvent& WXUNUSED(event))
{
	item_list->Clear();
	UpdateButtonStates();
}

void ApplySelectionPanel::OnLoadSelection(wxCommandEvent& WXUNUSED(event))
{
	if(load_from_selection_checkbox->IsChecked()) {
		LoadItemsFromSelection();
	} else {
		// Just refresh the selection info
		UpdateButtonStates();
		status_label->SetLabel("Enable 'Load items from selection' to populate the list.");
	}
}

void ApplySelectionPanel::LoadItemsFromSelection()
{
	Editor* editor = g_gui.GetCurrentEditor();
	if(!editor) {
		status_label->SetLabel("Error: No editor available.");
		return;
	}

	Selection& selection = editor->getSelection();
	if(selection.empty()) {
		status_label->SetLabel("Error: No tiles selected.");
		return;
	}

	// Collect all unique item IDs from the selection
	std::set<uint16_t> uniqueIds;
	for(Tile* tile : selection.getTiles()) {
		if(!tile) continue;
		for(Item* item : tile->items) {
			if(item) {
				uniqueIds.insert(item->getID());
			}
		}
	}

	// Clear current list and add all found items
	item_list->Clear();
	for(uint16_t id : uniqueIds) {
		item_list->AddItem(id);
	}

	UpdateButtonStates();
	status_label->SetLabel(wxString::Format("Loaded %zu unique item(s) from selection.", uniqueIds.size()));
}

void ApplySelectionPanel::OnItemListSelected(wxCommandEvent& WXUNUSED(event))
{
	UpdateButtonStates();
}

void ApplySelectionPanel::PopulateSearchResults()
{
	wxString filter = search_ctrl->GetValue().Lower();
	filtered_items.clear();
	search_results->Clear();

	if(filter.IsEmpty()) {
		add_button->Enable(false);
		return;
	}

	// Check if it's a numeric search (item ID)
	long itemIdSearch = 0;
	bool isNumericSearch = filter.ToLong(&itemIdSearch);

	for(uint16_t id = 100; id < g_items.getMaxID() && filtered_items.size() < 50; ++id) {
		const ItemType& type = g_items.getItemType(id);
		if(type.id == 0 || type.clientID == 0) continue;

		bool match = false;
		if(isNumericSearch) {
			// Match by ID
			wxString idStr = wxString::Format("%d", id);
			if(idStr.Contains(filter)) {
				match = true;
			}
		}

		// Match by name
		wxString name = wxstr(type.name).Lower();
		if(name.Contains(filter)) {
			match = true;
		}

		if(match) {
			filtered_items.push_back(id);
			wxString label = wxString::Format("%d - %s", id, wxstr(type.name));
			search_results->Append(label);
		}
	}

	add_button->Enable(false);
}

void ApplySelectionPanel::UpdateButtonStates()
{
	Editor* editor = g_gui.GetCurrentEditor();
	size_t selectionSize = 0;
	if(editor) {
		selectionSize = editor->getSelection().size();
	}

	selection_info_label->SetLabel(wxString::Format("Selection: %zu tile(s)", selectionSize));

	bool hasItems = !item_list->GetItemIds().empty();
	bool hasSelection = item_list->GetSelection() != wxNOT_FOUND;

	remove_button->Enable(hasSelection);
	// Move buttons are enabled when we have items AND a selection in the map
	moveup_button->Enable(hasItems && selectionSize > 0);
	movedown_button->Enable(hasItems && selectionSize > 0);
	clear_button->Enable(hasItems);
}

void ApplySelectionPanel::MoveItemsInSelection(bool moveUp)
{
	Editor* editor = g_gui.GetCurrentEditor();
	if(!editor) {
		status_label->SetLabel("Error: No editor available.");
		return;
	}

	Selection& selection = editor->getSelection();
	if(selection.empty()) {
		status_label->SetLabel("Error: No tiles selected.");
		return;
	}

	const std::vector<uint16_t>& itemIds = item_list->GetItemIds();
	if(itemIds.empty()) {
		status_label->SetLabel("Error: No items in the list.");
		return;
	}

	// Create a set for fast lookup
	std::set<uint16_t> itemIdSet(itemIds.begin(), itemIds.end());

	// Create action for undo/redo
	Action* action = editor->createAction(ACTION_DELETE_TILES);

	int tilesModified = 0;
	int itemsMoved = 0;

	// Process each tile in the selection
	for(Tile* tile : selection.getTiles()) {
		if(!tile) continue;

		// Create a deep copy of the tile for modification
		Tile* newTile = tile->deepCopy(editor->getMap());
		bool tileModified = false;

		// Get indices of items to move
		std::vector<size_t> indicesToMove;
		for(size_t i = 0; i < newTile->items.size(); ++i) {
			Item* item = newTile->items[i];
			if(item && itemIdSet.count(item->getID()) > 0) {
				indicesToMove.push_back(i);
			}
		}

		if(indicesToMove.empty()) {
			delete newTile;
			continue;
		}

		// For moveUp (higher index = on top): process from highest to lowest
		// For moveDown (lower index = below): process from lowest to highest
		if(moveUp) {
			// Process from end to start (highest indices first)
			for(int i = static_cast<int>(indicesToMove.size()) - 1; i >= 0; --i) {
				size_t idx = indicesToMove[i];
				if(idx + 1 < newTile->items.size()) {
					// Check if the item above is NOT in our set (don't swap with each other)
					Item* itemAbove = newTile->items[idx + 1];
					if(itemIdSet.count(itemAbove->getID()) == 0) {
						// Swap with item above
						std::swap(newTile->items[idx], newTile->items[idx + 1]);
						tileModified = true;
						itemsMoved++;
					}
				}
			}
		} else {
			// Process from start to end (lowest indices first)
			for(size_t i = 0; i < indicesToMove.size(); ++i) {
				size_t idx = indicesToMove[i];
				if(idx > 0) {
					// Check if the item below is NOT in our set (don't swap with each other)
					Item* itemBelow = newTile->items[idx - 1];
					if(itemIdSet.count(itemBelow->getID()) == 0) {
						// Swap with item below
						std::swap(newTile->items[idx], newTile->items[idx - 1]);
						tileModified = true;
						itemsMoved++;
						// Update indices for subsequent items since we moved this one down
						for(size_t j = i + 1; j < indicesToMove.size(); ++j) {
							if(indicesToMove[j] == idx - 1) {
								indicesToMove[j] = idx; // The swapped item is now at idx
							}
						}
					}
				}
			}
		}

		if(tileModified) {
			action->addChange(newd Change(newTile));
			tilesModified++;
		} else {
			delete newTile;
		}
	}

	if(tilesModified > 0) {
		editor->addAction(action);
		editor->updateActions();
		status_label->SetLabel(wxString::Format("Moved %s: %d tile(s), %d item(s).",
			moveUp ? "Up" : "Down", tilesModified, itemsMoved));
	} else {
		delete action;
		status_label->SetLabel("No changes made (items not found or already at limit).");
	}

	// Refresh the view
	g_gui.RefreshView();
}

void ApplySelectionPanel::RefreshSelectionInfo()
{
	UpdateButtonStates();
}

// ============================================================================
// BrowseFieldNotebook implementation

BrowseFieldNotebook::BrowseFieldNotebook(wxWindow* parent) :
	wxPanel(parent, wxID_ANY),
	notebook(nullptr),
	browse_tile_panel(nullptr),
	apply_selection_panel(nullptr)
{
	SetBackgroundColour(kBrowseTileBackgroundColour);

	auto* sizer = newd wxBoxSizer(wxVERTICAL);

	notebook = newd wxNotebook(this, wxID_ANY);
	notebook->SetBackgroundColour(kBrowseTileBackgroundColour);

	// Create and add the Browse Tile panel
	browse_tile_panel = newd BrowseTilePanel(notebook);
	notebook->AddPage(browse_tile_panel, "Browse Tile");

	// Create and add the Apply Selection panel
	apply_selection_panel = newd ApplySelectionPanel(notebook);
	notebook->AddPage(apply_selection_panel, "Apply Selection");

	sizer->Add(notebook, 1, wxEXPAND | wxALL, 0);
	SetSizer(sizer);

	// Bind page change event
	notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &BrowseFieldNotebook::OnPageChanged, this);
}

BrowseFieldNotebook::~BrowseFieldNotebook()
{
}

void BrowseFieldNotebook::OnPageChanged(wxBookCtrlEvent& event)
{
	// Refresh selection info when switching to Apply Selection tab
	if(event.GetSelection() == 1 && apply_selection_panel) {
		apply_selection_panel->RefreshSelectionInfo();
	}
	event.Skip();
}
