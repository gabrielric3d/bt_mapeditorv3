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
	load_selection_button(nullptr),
	auto_load_checkbox(nullptr),
	auto_apply_checkbox(nullptr),
	working_tile(nullptr),
	source_editor(nullptr),
	working_position_valid(false),
	auto_load_timer(newd wxTimer(this)),
	auto_load_enabled(false),
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

	auto* actionSizer = newd wxBoxSizer(wxHORIZONTAL);
	load_selection_button = newd wxButton(this, wxID_ANY, "Load Selection");
	apply_button = newd wxButton(this, wxID_ANY, "Apply Changes");
	actionSizer->Add(load_selection_button, 1, wxRIGHT, 5);
	actionSizer->Add(apply_button, 1, wxLEFT, 5);
	root_sizer->Add(actionSizer, 0, wxEXPAND | wxALL, 5);

	auto_load_checkbox = newd wxCheckBox(this, wxID_ANY, "Auto load from selection");
	auto_load_checkbox->SetForegroundColour(kBrowseTileTextColour);
	root_sizer->Add(auto_load_checkbox, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

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
	load_selection_button->Bind(wxEVT_BUTTON, &BrowseTilePanel::OnClickLoadSelection, this);
	auto_load_checkbox->Bind(wxEVT_CHECKBOX, &BrowseTilePanel::OnToggleAutoLoad, this);
	auto_apply_checkbox->Bind(wxEVT_CHECKBOX, &BrowseTilePanel::OnToggleAutoApply, this);
	Bind(wxEVT_TIMER, &BrowseTilePanel::OnAutoLoadTimer, this, auto_load_timer->GetId());

	UpdateControlStates();
	UpdateTileInfo();
}

BrowseTilePanel::~BrowseTilePanel()
{
	if(auto_load_timer->IsRunning()) {
		auto_load_timer->Stop();
	}
	delete auto_load_timer;
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

void BrowseTilePanel::LoadSelectionFromEditor()
{
	Editor* editor = g_gui.GetCurrentEditor();
	if(!editor) {
		ClearSelection();
		return;
	}

	if(editor->getSelection().size() != 1) {
		g_gui.SetStatusText("Select exactly one tile to browse.");
		ClearSelection();
		return;
	}

	Tile* tile = editor->getSelection().getSelectedTile();
	if(!tile) {
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

void BrowseTilePanel::OnClickLoadSelection(wxCommandEvent& WXUNUSED(event))
{
	LoadSelectionFromEditor();
}

void BrowseTilePanel::OnToggleAutoLoad(wxCommandEvent& event)
{
	auto_load_enabled = event.IsChecked();
	if(auto_load_enabled) {
		LoadSelectionFromEditor();
		if(!auto_load_timer->IsRunning()) {
			auto_load_timer->Start(300);
		}
	} else if(auto_load_timer->IsRunning()) {
		auto_load_timer->Stop();
	}
}

void BrowseTilePanel::OnAutoLoadTimer(wxTimerEvent& WXUNUSED(event))
{
	if(!auto_load_enabled)
		return;

	Editor* editor = g_gui.GetCurrentEditor();
	if(!editor)
		return;

	if(editor->getSelection().size() != 1) {
		return;
	}

	Tile* tile = editor->getSelection().getSelectedTile();
	if(!tile)
		return;

	if(!working_position_valid || editor != source_editor ||
		tile->getX() != working_position.x ||
		tile->getY() != working_position.y ||
		tile->getZ() != working_position.z)
	{
		LoadSelectionFromEditor();
	}
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
