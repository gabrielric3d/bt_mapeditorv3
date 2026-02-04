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
#include "replace_items_window.h"
#include "find_item_window.h"
#include "graphics.h"
#include "gui.h"
#include "artprovider.h"
#include "items.h"
#include "theme.h"
#include "selection.h"
#include "map_tab.h"
#include "ground_brush.h"
#include "common_windows.h"
#include <wx/spinctrl.h>

namespace {

template <typename ForeachType>
void foreach_ItemOnPositions(Map& map, ForeachType& foreach, const std::vector<Position>& positions)
{
	long long done = 0;
	for(const Position& pos : positions) {
		Tile* tile = map.getTile(pos);
		if(!tile) {
			continue;
		}

		++done;

		if(tile->ground) {
			foreach(map, tile, tile->ground, done);
		}

		std::queue<Container*> containers;
		for(ItemVector::iterator itemiter = tile->items.begin(); itemiter != tile->items.end(); ++itemiter) {
			Item* item = *itemiter;
			Container* container = dynamic_cast<Container*>(item);
			foreach(map, tile, item, done);
			if(container) {
				containers.push(container);

				do {
					container = containers.front();
					ItemVector& v = container->getVector();
					for(ItemVector::iterator containeriter = v.begin(); containeriter != v.end(); ++containeriter) {
						Item* i = *containeriter;
						Container* c = dynamic_cast<Container*>(i);
						foreach(map, tile, i, done);
						if(c) {
							containers.push(c);
						}
					}
					containers.pop();
				} while(containers.size());
			}
		}
	}
}

} // namespace

// ----------------------------------------------------------------------------
// Range replace helper dialog

class RangeReplaceDialog : public wxDialog
{
public:
	RangeReplaceDialog(wxWindow* parent) :
		wxDialog(parent, wxID_ANY, "Add Range Replace", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE)
	{
		wxBoxSizer* top = new wxBoxSizer(wxVERTICAL);

		wxStaticBoxSizer* replaceBox = new wxStaticBoxSizer(wxVERTICAL, this, "Replace IDs");
		wxBoxSizer* replaceRow = new wxBoxSizer(wxHORIZONTAL);
		replace_from = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535, 0);
		replace_to = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535, 0);
		replaceRow->Add(new wxStaticText(this, wxID_ANY, "From"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
		replaceRow->Add(replace_from, 0, wxRIGHT, 8);
		replaceRow->Add(new wxStaticText(this, wxID_ANY, "To"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
		replaceRow->Add(replace_to, 0, wxRIGHT, 4);
		replaceBox->Add(replaceRow, 0, wxALL, 5);

		wxStaticBoxSizer* withBox = new wxStaticBoxSizer(wxVERTICAL, this, "With IDs");
		wxBoxSizer* withRow = new wxBoxSizer(wxHORIZONTAL);
		with_from = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535, 0);
		with_to = new wxSpinCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535, 0);
		withRow->Add(new wxStaticText(this, wxID_ANY, "From"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
		withRow->Add(with_from, 0, wxRIGHT, 8);
		withRow->Add(new wxStaticText(this, wxID_ANY, "To"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
		withRow->Add(with_to, 0, wxRIGHT, 4);
		withBox->Add(withRow, 0, wxALL, 5);

		top->Add(replaceBox, 0, wxALL | wxEXPAND, 5);
		top->Add(withBox, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

		wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
		buttons->AddButton(new wxButton(this, wxID_OK));
		buttons->AddButton(new wxButton(this, wxID_CANCEL));
		buttons->Realize();
		top->Add(buttons, 0, wxALL | wxALIGN_RIGHT, 5);

		SetSizerAndFit(top);
		CentreOnParent();
	}

	bool BuildItems(std::vector<ReplacingItem>& out, wxString& error) const
	{
		int fromStart = replace_from->GetValue();
		int fromEnd = replace_to->GetValue();
		int toStart = with_from->GetValue();
		int toEnd = with_to->GetValue();

		if(fromEnd < fromStart) std::swap(fromStart, fromEnd);
		if(toEnd < toStart) std::swap(toStart, toEnd);

		const int replaceCount = fromEnd - fromStart + 1;
		const int withCount = toEnd - toStart + 1;

		if(replaceCount <= 0) {
			error = "Invalid replace range.";
			return false;
		}

		if(withCount != 1 && withCount != replaceCount) {
			error = "Target range must have the same size as source range or be a single ID.";
			return false;
		}

		out.clear();
		out.reserve(static_cast<size_t>(replaceCount));
		for(int i = 0; i < replaceCount; ++i) {
			uint16_t replaceId = static_cast<uint16_t>(fromStart + i);
			uint16_t withId = static_cast<uint16_t>(withCount == 1 ? toStart : (toStart + i));
			out.push_back(ReplacingItem::FromIds(replaceId, withId));
		}
		return true;
	}

private:
	wxSpinCtrl* replace_from;
	wxSpinCtrl* replace_to;
	wxSpinCtrl* with_from;
	wxSpinCtrl* with_to;
};

// ----------------------------------------------------------------------------
// Ground brush replace helper dialog

class GroundBrushReplaceDialog : public wxDialog
{
public:
	GroundBrushReplaceDialog(wxWindow* parent) :
		wxDialog(parent, wxID_ANY, "Replace Ground Brush", wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE),
		fromBrush(nullptr), toBrush(nullptr)
	{
		wxBoxSizer* top = new wxBoxSizer(wxVERTICAL);

		from_label = new wxStaticText(this, wxID_ANY, "From: (none)");
		to_label = new wxStaticText(this, wxID_ANY, "To: (none)");

		wxButton* pickFrom = new wxButton(this, wxID_ANY, "Choose From");
		wxButton* pickTo = new wxButton(this, wxID_ANY, "Choose To");

		pickFrom->Bind(wxEVT_BUTTON, &GroundBrushReplaceDialog::OnPickFrom, this);
		pickTo->Bind(wxEVT_BUTTON, &GroundBrushReplaceDialog::OnPickTo, this);

		top->Add(from_label, 0, wxALL, 5);
		top->Add(pickFrom, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
		top->Add(to_label, 0, wxALL, 5);
		top->Add(pickTo, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

		wxStdDialogButtonSizer* buttons = new wxStdDialogButtonSizer();
		ok_button = new wxButton(this, wxID_OK);
		ok_button->Enable(false);
		buttons->AddButton(ok_button);
		buttons->AddButton(new wxButton(this, wxID_CANCEL));
		buttons->Realize();
		top->Add(buttons, 0, wxALL | wxALIGN_RIGHT, 5);

		SetSizerAndFit(top);
		CentreOnParent();
	}

	GroundBrush* GetFrom() const { return fromBrush; }
	GroundBrush* GetTo() const { return toBrush; }

private:
	void OnPickFrom(wxCommandEvent&)
	{
		fromBrush = PickBrush("Select source ground brush");
		UpdateLabels();
	}

	void OnPickTo(wxCommandEvent&)
	{
		toBrush = PickBrush("Select target ground brush");
		UpdateLabels();
	}

	GroundBrush* PickBrush(const wxString& title)
	{
		FindBrushDialog dlg(this, title);
		if(dlg.ShowModal() == wxID_OK) {
			const Brush* brush = dlg.getResult();
			if(brush && brush->isGround()) {
				return const_cast<GroundBrush*>(dynamic_cast<const GroundBrush*>(brush));
			}
			wxMessageBox("Please pick a ground brush.", "Replace Ground Brush", wxOK | wxICON_WARNING, this);
		}
		return nullptr;
	}

	void UpdateLabels()
	{
		from_label->SetLabel(wxString::Format("From: %s", fromBrush ? wxstr(fromBrush->getName()) : "none"));
		to_label->SetLabel(wxString::Format("To: %s", toBrush ? wxstr(toBrush->getName()) : "none"));
		ok_button->Enable(fromBrush && toBrush);
		Layout();
		Fit();
	}

	GroundBrush* fromBrush;
	GroundBrush* toBrush;
	wxStaticText* from_label;
	wxStaticText* to_label;
	wxButton* ok_button;
};

// ============================================================================
// ReplaceItemsDropTarget

ReplaceItemsDropTarget::ReplaceItemsDropTarget(ReplaceItemsButton* button) :
	m_button(button)
{
	////
}

bool ReplaceItemsDropTarget::OnDropText(wxCoord x, wxCoord y, const wxString& data)
{
	if(data.StartsWith("ITEM_ID:")) {
		wxString idStr = data.Mid(8);
		long itemId = 0;
		if(idStr.ToLong(&itemId) && itemId > 0 && itemId <= 0xFFFF) {
			m_button->OnItemDropped(static_cast<uint16_t>(itemId));
			return true;
		}
	}
	return false;
}

// ============================================================================
// ReplaceItemsButton

ReplaceItemsButton::ReplaceItemsButton(wxWindow* parent) :
	DCButton(parent, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, 0),
	m_id(0)
{
	SetDropTarget(new ReplaceItemsDropTarget(this));
}

ItemGroup_t ReplaceItemsButton::GetGroup() const
{
	if(m_id != 0) {
		const ItemType& it = g_items.getItemType(m_id);
		if(it.id != 0)
			return it.group;
	}
	return ITEM_GROUP_NONE;
}

void ReplaceItemsButton::SetItemId(uint16_t id)
{
	if(m_id == id)
		return;

	m_id = id;

	if(m_id != 0) {
		const ItemType& it = g_items.getItemType(m_id);
		if(it.id != 0) {
			SetSprite(it.clientID);
			return;
		}
	}

	SetSprite(0);
}

void ReplaceItemsButton::OnItemDropped(uint16_t itemId)
{
	SetItemId(itemId);

	// Notify parent dialog to update widgets
	ReplaceItemsDialog* dialog = dynamic_cast<ReplaceItemsDialog*>(GetParent());
	if(dialog) {
		wxCommandEvent evt(wxEVT_COMMAND_BUTTON_CLICKED);
		evt.SetEventObject(this);
		dialog->GetEventHandler()->ProcessEvent(evt);
	}
}

// ============================================================================
// ReplaceItemsListBox

ReplaceItemsListBox::ReplaceItemsListBox(wxWindow* parent) :
	wxVListBox(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLB_SINGLE)
{
	m_arrow_bitmap = wxArtProvider::GetBitmap(ART_POSITION_GO, wxART_TOOLBAR, wxSize(16, 16));
	m_flag_bitmap = wxArtProvider::GetBitmap(ART_PZ_BRUSH, wxART_TOOLBAR, wxSize(16, 16));

	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.surface);
	SetForegroundColour(theme.text);
}

bool ReplaceItemsListBox::AddItem(const ReplacingItem& item)
{
	if(item.kind == ReplacingItem::Kind::ItemId) {
		if(item.replaceId == 0 || item.withId == 0 || item.replaceId == item.withId)
			return false;
	}
	if(item.kind == ReplacingItem::Kind::GroundBrush) {
		if(!item.fromBrush || !item.toBrush || item.fromBrush == item.toBrush)
			return false;
	}

	SetItemCount(GetItemCount() + 1);
	m_items.push_back(item);
	Refresh();

	return true;
}

void ReplaceItemsListBox::MarkAsComplete(const ReplacingItem& item, uint32_t total)
{
	auto it = std::find(m_items.begin(), m_items.end(), item);
	if(it != m_items.end()) {
		it->total = total;
		it->complete = true;
		Refresh();
	}
}

void ReplaceItemsListBox::RemoveSelected()
{
	if(m_items.empty())
		return;

	const int index = GetSelection();
	if(index == wxNOT_FOUND)
		return;

	m_items.erase(m_items.begin() + index);
	SetItemCount(GetItemCount() - 1);
	Refresh();
}

bool ReplaceItemsListBox::CanAdd(uint16_t replaceId, uint16_t withId) const
{
	if(replaceId == 0 || withId == 0 || replaceId == withId)
		return false;

	for(const ReplacingItem& item : m_items) {
		if(item.kind == ReplacingItem::Kind::ItemId && replaceId == item.replaceId)
			return false;
	}
	return true;
}

void ReplaceItemsListBox::OnDrawItem(wxDC& dc, const wxRect& rect, size_t index) const
{
	ASSERT(index < m_items.size());

	const ReplacingItem& item = m_items.at(index);
	Sprite* sprite1 = nullptr;
	Sprite* sprite2 = nullptr;
	wxString label;

	if(item.kind == ReplacingItem::Kind::ItemId) {
		const ItemType& type1 = g_items.getItemType(item.replaceId);
		const ItemType& type2 = g_items.getItemType(item.withId);
		sprite1 = g_gui.gfx.getSprite(type1.clientID);
		sprite2 = g_gui.gfx.getSprite(type2.clientID);
		label = wxString::Format("Replace: %d With: %d", item.replaceId, item.withId);
	} else if(item.kind == ReplacingItem::Kind::GroundBrush) {
		if(item.fromBrush) {
			sprite1 = g_gui.gfx.getSprite(item.fromBrush->getLookID());
		}
		if(item.toBrush) {
			sprite2 = g_gui.gfx.getSprite(item.toBrush->getLookID());
		}
		label = wxString::Format("Replace brush: %s With: %s",
			item.fromBrush ? wxstr(item.fromBrush->getName()) : "?", 
			item.toBrush ? wxstr(item.toBrush->getName()) : "?");
	}

	const ThemeColors& theme = Theme::Dark();
	wxColour text_colour = theme.text;
	if(IsSelected(index)) {
		text_colour = HasFocus() ? theme.text : theme.accent;
	}
	dc.SetTextForeground(text_colour);

	int x = rect.GetX();
	int y = rect.GetY();
	constexpr int icon_size = 32;
	if(sprite1 && sprite2) {
		sprite1->DrawTo(&dc, SPRITE_SIZE_32x32, x + 4, y + 4, icon_size, icon_size);
		dc.DrawBitmap(m_arrow_bitmap, x + 38, y + 10, true);
		sprite2->DrawTo(&dc, SPRITE_SIZE_32x32, x + 56, y + 4, icon_size, icon_size);
		x += 104;
	} else {
		x += 10;
	}
	dc.DrawText(label, x, y + 10);

	if(item.complete) {
		int tx = rect.GetWidth() - 100;
		dc.DrawBitmap(m_flag_bitmap, tx + 70, y + 10, true);
		dc.DrawText(wxString::Format("Total: %d", item.total), tx, y + 10);
	}
}

wxCoord ReplaceItemsListBox::OnMeasureItem(size_t WXUNUSED(index)) const
{
	return 40;
}

// ============================================================================
// ReplaceItemsDialog

ReplaceItemsDialog::ReplaceItemsDialog(wxWindow* parent, bool selectionOnly) :
	wxDialog(parent, wxID_ANY, (selectionOnly ? "Replace Items on Selection" : "Replace Items"),
			 wxDefaultPosition, wxSize(500, 480), wxDEFAULT_DIALOG_STYLE),
	selectionOnly(selectionOnly),
	lock_selection_checkbox(nullptr)
{
	SetSizeHints(wxDefaultSize, wxDefaultSize);

	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

	wxFlexGridSizer* list_sizer = new wxFlexGridSizer(0, 2, 0, 0);
	list_sizer->SetFlexibleDirection(wxBOTH);
	list_sizer->SetNonFlexibleGrowMode(wxFLEX_GROWMODE_SPECIFIED);
	list_sizer->SetMinSize(wxSize(-1, 300));

	list = new ReplaceItemsListBox(this);
	list->SetMinSize(wxSize(480, 320));

	list_sizer->Add(list, 0, wxALL | wxEXPAND, 5);
	sizer->Add(list_sizer, 1, wxALL | wxEXPAND, 5);

	wxBoxSizer* items_sizer = new wxBoxSizer(wxHORIZONTAL);
	items_sizer->SetMinSize(wxSize(-1, 40));

	replace_button = new ReplaceItemsButton(this);
	items_sizer->Add(replace_button, 0, wxALL, 5);

	wxBitmap bitmap = wxArtProvider::GetBitmap(ART_POSITION_GO, wxART_TOOLBAR, wxSize(16, 16));
	arrow_bitmap = new wxStaticBitmap(this, wxID_ANY, bitmap);
	items_sizer->Add(arrow_bitmap, 0, wxTOP, 15);

	with_button = new ReplaceItemsButton(this);
	items_sizer->Add(with_button, 0, wxALL, 5);

	auto_add_checkbox = new wxCheckBox(this, wxID_ANY, wxT("Auto Add"));
	auto_add_checkbox->SetToolTip("Automatically add to list when both boxes are filled");
	items_sizer->Add(auto_add_checkbox, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

	if(selectionOnly) {
		lock_selection_checkbox = new wxCheckBox(this, wxID_ANY, wxT("Lock Selection"));
		lock_selection_checkbox->SetToolTip("Use the selection captured now even if it is cleared or changed later");
		items_sizer->Add(lock_selection_checkbox, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
	}

	items_sizer->Add(0, 0, 1, wxEXPAND, 5);

	progress = new wxGauge(this, wxID_ANY, 100);
	progress->SetValue(0);
	items_sizer->Add(progress, 0, wxALL, 5);

	sizer->Add(items_sizer, 1, wxALL | wxEXPAND, 5);

	wxBoxSizer* buttons_sizer = new wxBoxSizer(wxHORIZONTAL);

	add_button = new wxButton(this, wxID_ANY, wxT("Add"));
	add_button->Enable(false);
	buttons_sizer->Add(add_button, 0, wxALL, 5);

	add_range_button = new wxButton(this, wxID_ANY, wxT("Add Range"));
	buttons_sizer->Add(add_range_button, 0, wxALL, 5);

	add_ground_brush_button = new wxButton(this, wxID_ANY, wxT("Add Ground Brush"));
	buttons_sizer->Add(add_ground_brush_button, 0, wxALL, 5);

	remove_button = new wxButton(this, wxID_ANY, wxT("Remove"));
	remove_button->Enable(false);
	buttons_sizer->Add(remove_button, 0, wxALL, 5);

	buttons_sizer->Add(0, 0, 1, wxEXPAND, 5);

	execute_button = new wxButton(this, wxID_ANY, wxT("Execute"));
	execute_button->Enable(false);
	buttons_sizer->Add(execute_button, 0, wxALL, 5);

	close_button = new wxButton(this, wxID_ANY, wxT("Close"));
	buttons_sizer->Add(close_button, 0, wxALL, 5);

	sizer->Add(buttons_sizer, 1, wxALL | wxLEFT | wxRIGHT | wxSHAPED, 5);

	SetSizer(sizer);
	Layout();
	Centre(wxBOTH);

	// Connect Events
	list->Connect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(ReplaceItemsDialog::OnListSelected), NULL, this);
	replace_button->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(ReplaceItemsDialog::OnReplaceItemClicked), NULL, this);
	replace_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnItemDropped), NULL, this);
	with_button->Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(ReplaceItemsDialog::OnWithItemClicked), NULL, this);
	with_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnItemDropped), NULL, this);
	add_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnAddButtonClicked), NULL, this);
	add_range_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnAddRangeButtonClicked), NULL, this);
	add_ground_brush_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnAddGroundBrushButtonClicked), NULL, this);
	remove_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnRemoveButtonClicked), NULL, this);
	execute_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnExecuteButtonClicked), NULL, this);
	close_button->Connect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnCancelButtonClicked), NULL, this);
	if(lock_selection_checkbox) {
		lock_selection_checkbox->Connect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnLockSelectionToggled), NULL, this);
	}

	if(selectionOnly) {
		CaptureSelectionSnapshot(GetParentEditor());
	}
}

ReplaceItemsDialog::~ReplaceItemsDialog()
{
	// Disconnect Events
	list->Disconnect(wxEVT_COMMAND_LISTBOX_SELECTED, wxCommandEventHandler(ReplaceItemsDialog::OnListSelected), NULL, this);
	replace_button->Disconnect(wxEVT_LEFT_DOWN, wxMouseEventHandler(ReplaceItemsDialog::OnReplaceItemClicked), NULL, this);
	replace_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnItemDropped), NULL, this);
	with_button->Disconnect(wxEVT_LEFT_DOWN, wxMouseEventHandler(ReplaceItemsDialog::OnWithItemClicked), NULL, this);
	with_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnItemDropped), NULL, this);
	add_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnAddButtonClicked), NULL, this);
	add_range_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnAddRangeButtonClicked), NULL, this);
	add_ground_brush_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnAddGroundBrushButtonClicked), NULL, this);
	remove_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnRemoveButtonClicked), NULL, this);
	execute_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnExecuteButtonClicked), NULL, this);
	close_button->Disconnect(wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnCancelButtonClicked), NULL, this);
	if(lock_selection_checkbox) {
		lock_selection_checkbox->Disconnect(wxEVT_COMMAND_CHECKBOX_CLICKED, wxCommandEventHandler(ReplaceItemsDialog::OnLockSelectionToggled), NULL, this);
	}
}

void ReplaceItemsDialog::UpdateWidgets()
{
	const uint16_t replaceId = replace_button->GetItemId();
	const uint16_t withId = with_button->GetItemId();
	add_button->Enable(list->CanAdd(replaceId, withId));
	remove_button->Enable(list->GetCount() != 0 && list->GetSelection() != wxNOT_FOUND);
	execute_button->Enable(list->GetCount() != 0);
}

Editor* ReplaceItemsDialog::GetParentEditor() const
{
	if(MapTab* tab = dynamic_cast<MapTab*>(GetParent())) {
		return tab->GetEditor();
	}
	return nullptr;
}

void ReplaceItemsDialog::CaptureSelectionSnapshot(Editor* editor)
{
	selectionSnapshot.clear();
	if(!selectionOnly || !editor) {
		return;
	}

	const Selection& selection = editor->getSelection();
	selectionSnapshot.reserve(selection.size());
	for(Tile* tile : selection.getTiles()) {
		if(tile) {
			selectionSnapshot.push_back(tile->getPosition());
		}
	}
}

void ReplaceItemsDialog::OnLockSelectionToggled(wxCommandEvent& event)
{
	if(!selectionOnly) {
		return;
	}

	if(event.IsChecked()) {
		CaptureSelectionSnapshot(GetParentEditor());
	} else {
		selectionSnapshot.clear();
	}
}

void ReplaceItemsDialog::OnListSelected(wxCommandEvent& WXUNUSED(event))
{
	remove_button->Enable(list->GetCount() != 0 && list->GetSelection() != wxNOT_FOUND);
}

void ReplaceItemsDialog::OnItemDropped(wxCommandEvent& WXUNUSED(event))
{
	UpdateWidgets();
	TryAutoAdd();
}

void ReplaceItemsDialog::OnReplaceItemClicked(wxMouseEvent& WXUNUSED(event))
{
	FindItemDialog dialog(this, "Replace Item");
	if(dialog.ShowModal() == wxID_OK) {
		uint16_t id = dialog.getResultID();
		if(id != with_button->GetItemId()) {
			replace_button->SetItemId(id);
			UpdateWidgets();
			TryAutoAdd();
		}
	}
	dialog.Destroy();
}

void ReplaceItemsDialog::OnWithItemClicked(wxMouseEvent& WXUNUSED(event))
{
	if(replace_button->GetItemId() == 0)
		return;

	FindItemDialog dialog(this, "With Item");
	if(dialog.ShowModal() == wxID_OK) {
		uint16_t id = dialog.getResultID();
		if(id != replace_button->GetItemId()) {
			with_button->SetItemId(id);
			UpdateWidgets();
			TryAutoAdd();
		}
	}
	dialog.Destroy();
}

void ReplaceItemsDialog::OnAddButtonClicked(wxCommandEvent& WXUNUSED(event))
{
	const uint16_t replaceId = replace_button->GetItemId();
	const uint16_t withId = with_button->GetItemId();
	if(list->CanAdd(replaceId, withId)) {
		ReplacingItem item = ReplacingItem::FromIds(replaceId, withId);
		if(list->AddItem(item)) {
			replace_button->SetItemId(0);
			with_button->SetItemId(0);
			UpdateWidgets();
		}
	}
}

void ReplaceItemsDialog::OnAddRangeButtonClicked(wxCommandEvent& WXUNUSED(event))
{
	RangeReplaceDialog dlg(this);
	if(dlg.ShowModal() == wxID_OK) {
		std::vector<ReplacingItem> items;
		wxString error;
		if(!dlg.BuildItems(items, error)) {
			wxMessageBox(error, "Add Range", wxOK | wxICON_WARNING, this);
			return;
		}

		for(const ReplacingItem& item : items) {
			if(list->CanAdd(item.replaceId, item.withId)) {
				list->AddItem(item);
			}
		}
		UpdateWidgets();
	}
}

void ReplaceItemsDialog::OnAddGroundBrushButtonClicked(wxCommandEvent& WXUNUSED(event))
{
	GroundBrushReplaceDialog dlg(this);
	if(dlg.ShowModal() == wxID_OK) {
		GroundBrush* from = dlg.GetFrom();
		GroundBrush* to = dlg.GetTo();
		if(from && to && from != to) {
			ReplacingItem item = ReplacingItem::FromBrushes(from, to);
			list->AddItem(item);
			UpdateWidgets();
		}
	}
}

void ReplaceItemsDialog::OnRemoveButtonClicked(wxCommandEvent& WXUNUSED(event))
{
	list->RemoveSelected();
	UpdateWidgets();
}

void ReplaceItemsDialog::OnExecuteButtonClicked(wxCommandEvent& WXUNUSED(event))
{
	if(!g_gui.IsEditorOpen())
		return;

	const auto& items = list->GetItems();
	if(items.empty())
		return;

	MapTab* tab = dynamic_cast<MapTab*>(GetParent());
	Editor* editor = tab ? tab->GetEditor() : nullptr;
	if(!editor)
		return;

	const bool useLockedSelection = selectionOnly && lock_selection_checkbox && lock_selection_checkbox->IsChecked();
	if(useLockedSelection && selectionSnapshot.empty()) {
		CaptureSelectionSnapshot(editor);
		if(selectionSnapshot.empty()) {
			wxMessageBox("Lock Selection is enabled, but no selection was captured. Select an area and toggle the checkbox to store it.", "Replace Items", wxOK | wxICON_INFORMATION);
			return;
		}
	}

	replace_button->Enable(false);
	with_button->Enable(false);
	add_button->Enable(false);
	remove_button->Enable(false);
	execute_button->Enable(false);
	close_button->Enable(false);
	progress->SetValue(0);

	int done = 0;
	for(const ReplacingItem& info : items) {
		uint32_t total = 0;

		if(info.kind == ReplacingItem::Kind::ItemId) {
			ItemFinder finder(info.replaceId, (uint32_t)g_settings.getInteger(Config::REPLACE_SIZE));

			if(useLockedSelection) {
				foreach_ItemOnPositions(editor->getMap(), finder, selectionSnapshot);
			} else {
				foreach_ItemOnMap(editor->getMap(), finder, selectionOnly);
			}

			const auto& result = finder.result;

			if(!result.empty()) {
				BatchAction* batch = editor->createBatch(ACTION_REPLACE_ITEMS);
				Action* action = editor->createAction(batch);
				for(const auto& pair : result) {
					Tile* new_tile = pair.first->deepCopy(editor->getMap());
					int index = pair.first->getIndexOf(pair.second);
					ASSERT(index != wxNOT_FOUND);
					Item* item = new_tile->getItemAt(index);
					ASSERT(item && item->getID() == pair.second->getID());
					transformItem(item, info.withId, new_tile);
					action->addChange(new Change(new_tile));
					total++;
				}
				batch->addAndCommitAction(action);
				editor->addBatch(batch);
				editor->updateActions();
			}
		} else if(info.kind == ReplacingItem::Kind::GroundBrush) {
			struct GroundBrushFinder {
				GroundBrush* brush;
				std::vector<Tile*> tiles;
				void operator()(Map& map, Tile* tile, Item* item, long long done) {
					if(item && tile && item == tile->ground && item->getGroundBrush() == brush) {
						tiles.push_back(tile);
					}
				}
			} finder;
			finder.brush = info.fromBrush;

			if(useLockedSelection) {
				foreach_ItemOnPositions(editor->getMap(), finder, selectionSnapshot);
			} else {
				foreach_ItemOnMap(editor->getMap(), finder, selectionOnly);
			}

			if(!finder.tiles.empty()) {
				BatchAction* batch = editor->createBatch(ACTION_REPLACE_ITEMS);
				Action* action = editor->createAction(batch);
				GroundBrush::DrawParams params;
				params.paintSingleTile = true;

				for(Tile* tile : finder.tiles) {
					Tile* new_tile = tile->deepCopy(editor->getMap());
					if(new_tile->ground) {
						delete new_tile->ground;
						new_tile->ground = nullptr;
					}
					info.toBrush->draw(&editor->getMap(), new_tile, &params);
					GroundBrush::doBorders(&editor->getMap(), new_tile);
					action->addChange(new Change(new_tile));
					++total;
				}

				batch->addAndCommitAction(action);
				editor->addBatch(batch);
				editor->updateActions();
			}
		}

		done++;
		const int value = static_cast<int>((done * 100) / std::max<size_t>(1, items.size()));
		progress->SetValue(std::clamp<int>(value, 0, 100));
		list->MarkAsComplete(info, total);
	}

	tab->Refresh();
	close_button->Enable(true);
	replace_button->Enable(true);
	with_button->Enable(true);
	UpdateWidgets();
}

void ReplaceItemsDialog::OnCancelButtonClicked(wxCommandEvent& WXUNUSED(event))
{
	Close();
}

void ReplaceItemsDialog::ApplyItemToBox(uint16_t itemId, int boxNumber)
{
	if(boxNumber == 1) {
		replace_button->SetItemId(itemId);
	} else if(boxNumber == 2) {
		with_button->SetItemId(itemId);
	}
	UpdateWidgets();
	TryAutoAdd();
}

void ReplaceItemsDialog::TryAutoAdd()
{
	if(!auto_add_checkbox->IsChecked())
		return;

	const uint16_t replaceId = replace_button->GetItemId();
	const uint16_t withId = with_button->GetItemId();

	if(list->CanAdd(replaceId, withId)) {
		ReplacingItem item;
		item.replaceId = replaceId;
		item.withId = withId;
		if(list->AddItem(item)) {
			replace_button->SetItemId(0);
			with_button->SetItemId(0);
			UpdateWidgets();
		}
	}
}
