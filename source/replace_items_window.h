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

#ifndef RME_REPLACE_ITEMS_WINDOW_H_
#define RME_REPLACE_ITEMS_WINDOW_H_

#include "main.h"
#include "common_windows.h"
#include "editor.h"
#include <wx/dnd.h>
#include "ground_brush.h"

struct ReplacingItem
{
	enum class Kind {
		ItemId,
		GroundBrush,
	};

	ReplacingItem() :
		kind(Kind::ItemId), replaceId(0), withId(0), fromBrush(nullptr), toBrush(nullptr), total(0), complete(false) { }

	static ReplacingItem FromIds(uint16_t replaceId, uint16_t withId)
	{
		ReplacingItem item;
		item.kind = Kind::ItemId;
		item.replaceId = replaceId;
		item.withId = withId;
		return item;
	}

	static ReplacingItem FromBrushes(GroundBrush* from, GroundBrush* to)
	{
		ReplacingItem item;
		item.kind = Kind::GroundBrush;
		item.fromBrush = from;
		item.toBrush = to;
		return item;
	}

	bool operator==(const ReplacingItem& other) const
	{
		if(kind != other.kind)
			return false;
		if(kind == Kind::GroundBrush) {
			return fromBrush == other.fromBrush && toBrush == other.toBrush;
		}
		return replaceId == other.replaceId && withId == other.withId;
	}

	Kind kind;
	uint16_t replaceId;
	uint16_t withId;
	GroundBrush* fromBrush;
	GroundBrush* toBrush;
	uint32_t total;
	bool complete;
};

// ============================================================================
// ReplaceItemsButton

class ReplaceItemsButton;

class ReplaceItemsDropTarget : public wxTextDropTarget
{
public:
	ReplaceItemsDropTarget(ReplaceItemsButton* button);
	virtual bool OnDropText(wxCoord x, wxCoord y, const wxString& data) override;

private:
	ReplaceItemsButton* m_button;
};

class ReplaceItemsButton : public DCButton
{
public:
	ReplaceItemsButton(wxWindow* parent);
	~ReplaceItemsButton() { }

	ItemGroup_t GetGroup() const;
	uint16_t GetItemId() const { return m_id; }
	void SetItemId(uint16_t id);

	// Callback for when item is dropped
	void OnItemDropped(uint16_t itemId);

private:
	uint16_t m_id;
};

// ============================================================================
// ReplaceItemsListBox

class ReplaceItemsListBox : public wxVListBox
{
public:
	ReplaceItemsListBox(wxWindow* parent);

	bool AddItem(const ReplacingItem& item);
	void MarkAsComplete(const ReplacingItem& item, uint32_t total);
	void RemoveSelected();
	bool CanAdd(uint16_t replaceId, uint16_t withId) const;

	void OnDrawItem(wxDC& dc, const wxRect& rect, size_t index) const;
	wxCoord OnMeasureItem(size_t index) const;

	const std::vector<ReplacingItem>& GetItems() const { return m_items; }
	size_t GetCount() const { return m_items.size(); }

private:
	std::vector<ReplacingItem> m_items;
	wxBitmap m_arrow_bitmap;
	wxBitmap m_flag_bitmap;
};

// ============================================================================
// ReplaceItemsDialog

struct ItemFinder
{
	ItemFinder(uint16_t itemid, int32_t limit = -1) : itemid(itemid), limit(limit), exceeded(false) {}

	void operator()(Map& map, Tile* tile, Item* item, long long done) {
		if(exceeded)
			return;

		if(item->getID() == itemid) {
			result.push_back(std::make_pair(tile, item));
			if(limit > 0 && result.size() >= size_t(limit))
				exceeded = true;
		}
	}

	std::vector<std::pair<Tile*, Item*>> result;

private:
	uint16_t itemid;
	int32_t limit;
	bool exceeded;
};

class ReplaceItemsDialog : public wxDialog
{
public:
	ReplaceItemsDialog(wxWindow* parent, bool selectionOnly);
	~ReplaceItemsDialog();

	void OnListSelected(wxCommandEvent& event);
	void OnReplaceItemClicked(wxMouseEvent& event);
	void OnWithItemClicked(wxMouseEvent& event);
	void OnAddButtonClicked(wxCommandEvent& event);
	void OnAddRangeButtonClicked(wxCommandEvent& event);
	void OnAddGroundBrushButtonClicked(wxCommandEvent& event);
	void OnRemoveButtonClicked(wxCommandEvent& event);
	void OnExecuteButtonClicked(wxCommandEvent& event);
	void OnCancelButtonClicked(wxCommandEvent& event);
	void OnItemDropped(wxCommandEvent& event);
	void OnLockSelectionToggled(wxCommandEvent& event);

	void UpdateWidgets();

	// Apply item to replace boxes (1 = Replace, 2 = With)
	void ApplyItemToBox(uint16_t itemId, int boxNumber);

private:
	ReplaceItemsListBox* list;
	ReplaceItemsButton* replace_button;
	ReplaceItemsButton* with_button;
	wxGauge* progress;
	wxStaticBitmap* arrow_bitmap;
	wxButton* add_button;
	wxButton* add_range_button;
	wxButton* add_ground_brush_button;
	wxButton* remove_button;
	wxButton* execute_button;
	wxButton* close_button;
	wxCheckBox* auto_add_checkbox;
	wxCheckBox* lock_selection_checkbox;
	bool selectionOnly;
	std::vector<Position> selectionSnapshot;

	// Try to auto-add when both boxes are filled
	void TryAutoAdd();
	Editor* GetParentEditor() const;
	void CaptureSelectionSnapshot(Editor* editor);
};

#endif
