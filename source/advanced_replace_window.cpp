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
#include "advanced_replace_window.h"
#include "graphics.h"
#include "gui.h"
#include "items.h"
#include "item.h"
#include "theme.h"
#include "selection.h"
#include "map_tab.h"
#include "map_window.h"
#include "map_display.h"
#include "map.h"
#include "ground_brush.h"
#include "carpet_brush.h"
#include "wall_brush.h"
#include "brush.h"
#include "editor.h"
#include "artprovider.h"
#include "dcbutton.h"
#include "action.h"
#include "tile.h"

#include <wx/splitter.h>
#include <wx/wfstream.h>
#include <wx/txtstrm.h>
#include <algorithm>
#include <fstream>

// ============================================================================
// ReplaceRule implementation

bool ReplaceRule::isValid() const
{
	if(kind == Kind::ItemId) {
		return originalId != 0 && !replacementIds.empty();
	} else {
		return !originalBrushName.empty() && !replacementBrushNames.empty();
	}
}

wxString ReplaceRule::getDisplayName() const
{
	if(kind == Kind::ItemId) {
		wxString repls;
		for(size_t i = 0; i < replacementIds.size() && i < 3; ++i) {
			if(i > 0) repls += ", ";
			repls += wxString::Format("%d", replacementIds[i]);
		}
		if(replacementIds.size() > 3) repls += "...";
		return wxString::Format("Item %d -> %s", originalId, repls);
	} else {
		wxString repls;
		for(size_t i = 0; i < replacementBrushNames.size() && i < 2; ++i) {
			if(i > 0) repls += ", ";
			repls += wxstr(replacementBrushNames[i]);
		}
		if(replacementBrushNames.size() > 2) repls += "...";
		return wxString::Format("%s -> %s", wxstr(originalBrushName), repls);
	}
}

// ============================================================================
// ItemLibraryPanel implementation

ItemLibraryPanel::ItemLibraryPanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY)
{
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.surface);

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	// Search control
	searchCtrl = new wxSearchCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	searchCtrl->SetDescriptiveText("Search...");
	mainSizer->Add(searchCtrl, 0, wxALL | wxEXPAND, 5);

	// Notebook for Items/Brushes tabs
	notebook = new wxNotebook(this, wxID_ANY);

	// Items panel
	itemsPanel = new wxScrolledWindow(notebook, wxID_ANY);
	itemsPanel->SetScrollRate(0, 10);
	itemsPanel->SetBackgroundColour(theme.surface);
	notebook->AddPage(itemsPanel, "Items");

	// Brushes panel
	brushesPanel = new wxScrolledWindow(notebook, wxID_ANY);
	brushesPanel->SetScrollRate(0, 10);
	brushesPanel->SetBackgroundColour(theme.surface);
	notebook->AddPage(brushesPanel, "Brushes");

	mainSizer->Add(notebook, 1, wxALL | wxEXPAND, 5);

	// Pagination
	wxBoxSizer* pageSizer = new wxBoxSizer(wxHORIZONTAL);
	prevPageBtn = new wxButton(this, wxID_ANY, "<", wxDefaultPosition, wxSize(30, -1));
	nextPageBtn = new wxButton(this, wxID_ANY, ">", wxDefaultPosition, wxSize(30, -1));
	pageLabel = new wxStaticText(this, wxID_ANY, "1 / 1", wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER);
	pageLabel->SetForegroundColour(theme.text);

	pageSizer->Add(prevPageBtn, 0);
	pageSizer->Add(pageLabel, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
	pageSizer->Add(nextPageBtn, 0);
	mainSizer->Add(pageSizer, 0, wxALL | wxEXPAND, 5);

	SetSizer(mainSizer);

	// Bind events
	searchCtrl->Bind(wxEVT_TEXT, &ItemLibraryPanel::OnSearchChanged, this);
	notebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &ItemLibraryPanel::OnTabChanged, this);
	prevPageBtn->Bind(wxEVT_BUTTON, &ItemLibraryPanel::OnPageChanged, this);
	nextPageBtn->Bind(wxEVT_BUTTON, &ItemLibraryPanel::OnPageChanged, this);

	RefreshItems();
}

void ItemLibraryPanel::SetSearchFilter(const wxString& filter)
{
	currentFilter = filter.Lower();
	currentPage = 0;
	RefreshItems();
}

void ItemLibraryPanel::RefreshItems()
{
	if(IsItemMode()) {
		PopulateItems();
	} else {
		PopulateBrushes();
	}
	UpdatePagination();
}

uint16_t ItemLibraryPanel::GetSelectedItemId() const
{
	return selectedItemId;
}

Brush* ItemLibraryPanel::GetSelectedBrush() const
{
	return selectedBrush;
}

bool ItemLibraryPanel::IsItemMode() const
{
	return notebook->GetSelection() == 0;
}

void ItemLibraryPanel::OnTabChanged(wxBookCtrlEvent& event)
{
	currentPage = 0;
	RefreshItems();
	event.Skip();
}

void ItemLibraryPanel::OnSearchChanged(wxCommandEvent& event)
{
	SetSearchFilter(searchCtrl->GetValue());
}

void ItemLibraryPanel::SetItemSelectedCallback(std::function<void(uint16_t)> callback)
{
	itemSelectedCallback = callback;
}

void ItemLibraryPanel::SetBrushSelectedCallback(std::function<void(Brush*)> callback)
{
	brushSelectedCallback = callback;
}

void ItemLibraryPanel::OnItemSelected(wxMouseEvent& event)
{
	DCButton* btn = dynamic_cast<DCButton*>(event.GetEventObject());
	if(!btn) return;

	if(IsItemMode()) {
		selectedItemId = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(btn->GetClientData()));
		selectedBrush = nullptr;
	} else {
		selectedBrush = reinterpret_cast<Brush*>(btn->GetClientData());
		selectedItemId = 0;
	}

	event.Skip();
}

void ItemLibraryPanel::OnItemDoubleClicked(wxMouseEvent& event)
{
	DCButton* btn = dynamic_cast<DCButton*>(event.GetEventObject());
	if(!btn) return;

	if(IsItemMode()) {
		uint16_t itemId = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(btn->GetClientData()));
		if(itemSelectedCallback) {
			itemSelectedCallback(itemId);
		}
	} else {
		Brush* brush = reinterpret_cast<Brush*>(btn->GetClientData());
		if(brush && brushSelectedCallback) {
			brushSelectedCallback(brush);
		}
	}
}

void ItemLibraryPanel::OnItemDragStart(wxMouseEvent& event)
{
	if(!event.Dragging()) {
		event.Skip();
		return;
	}

	DCButton* btn = dynamic_cast<DCButton*>(event.GetEventObject());
	if(!btn) return;

	wxString dragData;
	if(IsItemMode()) {
		uint16_t itemId = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(btn->GetClientData()));
		dragData = wxString::Format("ITEM_ID:%d", itemId);
	} else {
		Brush* brush = reinterpret_cast<Brush*>(btn->GetClientData());
		if(brush) {
			dragData = wxString::Format("BRUSH:%s", wxstr(brush->getName()));
		}
	}

	if(!dragData.IsEmpty()) {
		wxTextDataObject data(dragData);
		wxDropSource dragSource(this);
		dragSource.SetData(data);
		dragSource.DoDragDrop(wxDrag_CopyOnly);
	}
}

void ItemLibraryPanel::OnPageChanged(wxCommandEvent& event)
{
	if(event.GetEventObject() == prevPageBtn && currentPage > 0) {
		currentPage--;
		RefreshItems();
	} else if(event.GetEventObject() == nextPageBtn) {
		int totalItems = IsItemMode() ? filteredItems.size() : filteredBrushes.size();
		int totalPages = (totalItems + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE;
		if(currentPage < totalPages - 1) {
			currentPage++;
			RefreshItems();
		}
	}
}

void ItemLibraryPanel::PopulateItems()
{
	// Clear existing
	itemsPanel->DestroyChildren();

	// Filter items
	filteredItems.clear();
	for(uint16_t id = 100; id < g_items.getMaxID(); ++id) {
		const ItemType& type = g_items.getItemType(id);
		if(type.id == 0 || type.clientID == 0) continue;

		if(!currentFilter.IsEmpty()) {
			wxString name = wxstr(type.name).Lower();
			wxString idStr = wxString::Format("%d", id);
			if(!name.Contains(currentFilter) && !idStr.Contains(currentFilter)) {
				continue;
			}
		}

		filteredItems.push_back(id);
	}

	// Create grid sizer
	wxGridSizer* grid = new wxGridSizer(4, 5, 5);

	// Populate current page
	int startIdx = currentPage * ITEMS_PER_PAGE;
	int endIdx = std::min(startIdx + ITEMS_PER_PAGE, static_cast<int>(filteredItems.size()));

	for(int i = startIdx; i < endIdx; ++i) {
		uint16_t id = filteredItems[i];
		const ItemType& type = g_items.getItemType(id);

		DCButton* btn = new DCButton(itemsPanel, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, type.clientID);
		btn->SetClientData(reinterpret_cast<void*>(static_cast<uintptr_t>(id)));
		btn->SetToolTip(wxString::Format("%s\nS: %d\nC: %d", wxstr(type.name), id, type.clientID));

		btn->Bind(wxEVT_LEFT_DOWN, &ItemLibraryPanel::OnItemSelected, this);
		btn->Bind(wxEVT_LEFT_DCLICK, &ItemLibraryPanel::OnItemDoubleClicked, this);
		btn->Bind(wxEVT_MOTION, &ItemLibraryPanel::OnItemDragStart, this);

		grid->Add(btn, 0, wxALL, 2);
	}

	itemsPanel->SetSizer(grid);
	itemsPanel->FitInside();
	itemsPanel->Layout();
}

void ItemLibraryPanel::PopulateBrushes()
{
	// Clear existing
	brushesPanel->DestroyChildren();

	// Filter brushes - include Ground and Carpet brushes
	filteredBrushes.clear();
	const BrushMap& brushMap = g_brushes.getMap();
	for(const auto& pair : brushMap) {
		Brush* brush = pair.second;
		if(!brush) continue;

		// Include ground and carpet brushes
		if(!brush->isGround() && !brush->isCarpet()) continue;

		if(!currentFilter.IsEmpty()) {
			wxString name = wxstr(brush->getName()).Lower();
			if(!name.Contains(currentFilter)) {
				continue;
			}
		}

		filteredBrushes.push_back(brush);
	}

	// Sort by name
	std::sort(filteredBrushes.begin(), filteredBrushes.end(), [](Brush* a, Brush* b) {
		return a->getName() < b->getName();
	});

	// Create grid sizer
	wxGridSizer* grid = new wxGridSizer(4, 5, 5);

	// Populate current page
	int startIdx = currentPage * ITEMS_PER_PAGE;
	int endIdx = std::min(startIdx + ITEMS_PER_PAGE, static_cast<int>(filteredBrushes.size()));

	for(int i = startIdx; i < endIdx; ++i) {
		Brush* brush = filteredBrushes[i];
		int lookId = brush->getLookID();

		DCButton* btn = new DCButton(brushesPanel, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, lookId);
		btn->SetClientData(brush);
		btn->SetToolTip(wxstr(brush->getName()));

		btn->Bind(wxEVT_LEFT_DOWN, &ItemLibraryPanel::OnItemSelected, this);
		btn->Bind(wxEVT_LEFT_DCLICK, &ItemLibraryPanel::OnItemDoubleClicked, this);
		btn->Bind(wxEVT_MOTION, &ItemLibraryPanel::OnItemDragStart, this);

		grid->Add(btn, 0, wxALL, 2);
	}

	brushesPanel->SetSizer(grid);
	brushesPanel->FitInside();
	brushesPanel->Layout();
}

void ItemLibraryPanel::UpdatePagination()
{
	int totalItems = IsItemMode() ? filteredItems.size() : filteredBrushes.size();
	int totalPages = std::max(1, (totalItems + ITEMS_PER_PAGE - 1) / ITEMS_PER_PAGE);

	pageLabel->SetLabel(wxString::Format("%d / %d", currentPage + 1, totalPages));
	prevPageBtn->Enable(currentPage > 0);
	nextPageBtn->Enable(currentPage < totalPages - 1);
}

// ============================================================================
// RuleBuilderPanel implementation

RuleBuilderPanel::RuleBuilderPanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY)
{
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.surface);

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	// Title row with Clear button
	wxBoxSizer* titleSizer = new wxBoxSizer(wxHORIZONTAL);
	wxStaticText* title = new wxStaticText(this, wxID_ANY, "RULE BUILDER");
	title->SetForegroundColour(theme.text);
	title->SetFont(title->GetFont().Bold());
	titleSizer->Add(title, 0, wxALIGN_CENTER_VERTICAL);
	titleSizer->AddStretchSpacer();
	wxStaticText* hint = new wxStaticText(this, wxID_ANY, "Double-click item in library to add");
	hint->SetForegroundColour(wxColour(100, 150, 200));
	titleSizer->Add(hint, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
	clearBtn = new wxButton(this, wxID_ANY, "Clear", wxDefaultPosition, wxSize(60, -1));
	titleSizer->Add(clearBtn, 0, wxALIGN_CENTER_VERTICAL);
	mainSizer->Add(titleSizer, 0, wxALL | wxEXPAND, 5);

	// Horizontal layout: Original | Replacements side by side
	wxBoxSizer* dropZonesSizer = new wxBoxSizer(wxHORIZONTAL);

	// Original section (left)
	wxBoxSizer* origSection = new wxBoxSizer(wxVERTICAL);
	origLabel = new wxStaticText(this, wxID_ANY, "Original (Click)");
	origLabel->SetForegroundColour(wxColour(100, 200, 100));
	origLabel->SetFont(origLabel->GetFont().Bold());
	origSection->Add(origLabel, 0, wxBOTTOM, 3);

	originalDropZone = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxSize(60, 60));
	originalDropZone->SetBackgroundColour(wxColour(40, 60, 40));
	origSection->Add(originalDropZone, 0, wxEXPAND);

	dropZonesSizer->Add(origSection, 0, wxRIGHT | wxEXPAND, 5);

	// Arrow
	wxStaticText* arrowLabel = new wxStaticText(this, wxID_ANY, wxString::FromUTF8("\xe2\x96\xb6"));
	arrowLabel->SetForegroundColour(theme.text);
	arrowLabel->SetFont(arrowLabel->GetFont().MakeLarger());
	dropZonesSizer->Add(arrowLabel, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

	// Replacements section (right, takes remaining space)
	wxBoxSizer* replSection = new wxBoxSizer(wxVERTICAL);
	replLabel = new wxStaticText(this, wxID_ANY, "Replacements (Click)");
	replLabel->SetForegroundColour(theme.text);
	replSection->Add(replLabel, 0, wxBOTTOM, 3);

	replacementsScroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 60));
	replacementsScroll->SetScrollRate(10, 0);
	replacementsScroll->SetBackgroundColour(theme.background);
	replSection->Add(replacementsScroll, 1, wxEXPAND);

	dropZonesSizer->Add(replSection, 1, wxEXPAND);

	mainSizer->Add(dropZonesSizer, 0, wxALL | wxEXPAND, 5);

	SetSizer(mainSizer);

	clearBtn->Bind(wxEVT_BUTTON, &RuleBuilderPanel::OnClearClicked, this);
	originalDropZone->Bind(wxEVT_LEFT_DOWN, &RuleBuilderPanel::OnOriginalClicked, this);
	replacementsScroll->Bind(wxEVT_LEFT_DOWN, &RuleBuilderPanel::OnReplacementClicked, this);
}

void RuleBuilderPanel::SetOriginal(uint16_t itemId)
{
	currentRule.kind = ReplaceRule::Kind::ItemId;
	currentRule.originalId = itemId;
	currentRule.originalBrushName.clear();
	UpdateDisplay();
}

void RuleBuilderPanel::SetOriginal(Brush* brush)
{
	if(!brush) return;
	currentRule.kind = ReplaceRule::Kind::Brush;
	currentRule.originalBrushName = brush->getName();
	currentRule.originalId = 0;
	UpdateDisplay();
}

void RuleBuilderPanel::AddReplacement(uint16_t itemId)
{
	if(currentRule.kind != ReplaceRule::Kind::ItemId) return;
	if(std::find(currentRule.replacementIds.begin(), currentRule.replacementIds.end(), itemId) == currentRule.replacementIds.end()) {
		currentRule.replacementIds.push_back(itemId);
		UpdateDisplay();
	}
}

void RuleBuilderPanel::AddReplacement(Brush* brush)
{
	if(!brush || currentRule.kind != ReplaceRule::Kind::Brush) return;
	const std::string& name = brush->getName();
	if(std::find(currentRule.replacementBrushNames.begin(), currentRule.replacementBrushNames.end(), name) == currentRule.replacementBrushNames.end()) {
		currentRule.replacementBrushNames.push_back(name);
		UpdateDisplay();
	}
}

void RuleBuilderPanel::ClearRule()
{
	currentRule = ReplaceRule();
	UpdateDisplay();
}

ReplaceRule RuleBuilderPanel::GetCurrentRule() const
{
	return currentRule;
}

bool RuleBuilderPanel::HasValidRule() const
{
	return currentRule.isValid();
}

void RuleBuilderPanel::SetDropCallback(std::function<void(bool isOriginal)> callback)
{
	dropCallback = callback;
}

void RuleBuilderPanel::SetTargetOriginal(bool isOriginal)
{
	isTargetOriginal = isOriginal;
	UpdateSelectionHighlight();
	if(dropCallback) dropCallback(isOriginal);
}

void RuleBuilderPanel::UpdateSelectionHighlight()
{
	const ThemeColors& theme = Theme::Dark();

	wxFont normalFont = origLabel->GetFont();
	normalFont.SetWeight(wxFONTWEIGHT_NORMAL);
	wxFont boldFont = origLabel->GetFont();
	boldFont.SetWeight(wxFONTWEIGHT_BOLD);

	if(isTargetOriginal) {
		// Original is selected - green highlight
		origLabel->SetForegroundColour(wxColour(100, 200, 100));
		origLabel->SetFont(boldFont);
		originalDropZone->SetBackgroundColour(wxColour(40, 60, 40));

		replLabel->SetForegroundColour(theme.text);
		replLabel->SetFont(normalFont);
		replacementsScroll->SetBackgroundColour(theme.background);
	} else {
		// Replacements is selected - green highlight
		origLabel->SetForegroundColour(theme.text);
		origLabel->SetFont(normalFont);
		originalDropZone->SetBackgroundColour(theme.background);

		replLabel->SetForegroundColour(wxColour(100, 200, 100));
		replLabel->SetFont(boldFont);
		replacementsScroll->SetBackgroundColour(wxColour(40, 60, 40));
	}

	origLabel->Refresh();
	replLabel->Refresh();
	originalDropZone->Refresh();
	replacementsScroll->Refresh();
}

void RuleBuilderPanel::OnOriginalClicked(wxMouseEvent& event)
{
	SetTargetOriginal(true);
	event.Skip();
}

void RuleBuilderPanel::OnReplacementClicked(wxMouseEvent& event)
{
	SetTargetOriginal(false);
	event.Skip();
}

void RuleBuilderPanel::OnClearClicked(wxCommandEvent& event)
{
	ClearRule();
}

void RuleBuilderPanel::UpdateDisplay()
{
	// Clear original zone
	originalDropZone->DestroyChildren();

	wxBoxSizer* origSizer = new wxBoxSizer(wxHORIZONTAL);

	if(currentRule.kind == ReplaceRule::Kind::ItemId && currentRule.originalId != 0) {
		const ItemType& type = g_items.getItemType(currentRule.originalId);
		originalButton = new DCButton(originalDropZone, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, type.clientID);
		originalButton->SetToolTip(wxString::Format("%s (%d)", wxstr(type.name), currentRule.originalId));
		origSizer->Add(originalButton, 0, wxALL, 5);
	} else if(currentRule.kind == ReplaceRule::Kind::Brush && !currentRule.originalBrushName.empty()) {
		Brush* brush = g_brushes.getBrush(currentRule.originalBrushName);
		if(brush) {
			originalButton = new DCButton(originalDropZone, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, brush->getLookID());
			originalButton->SetToolTip(wxstr(brush->getName()));
			origSizer->Add(originalButton, 0, wxALL, 5);
		}
	}

	originalDropZone->SetSizer(origSizer);
	originalDropZone->Layout();

	// Clear replacements zone
	replacementsScroll->DestroyChildren();
	replacementButtons.clear();

	wxBoxSizer* replSizer = new wxBoxSizer(wxHORIZONTAL);

	if(currentRule.kind == ReplaceRule::Kind::ItemId) {
		for(uint16_t id : currentRule.replacementIds) {
			const ItemType& type = g_items.getItemType(id);
			DCButton* btn = new DCButton(replacementsScroll, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, type.clientID);
			btn->SetToolTip(wxString::Format("%s (%d)", wxstr(type.name), id));
			replSizer->Add(btn, 0, wxALL, 5);
			replacementButtons.push_back(btn);
		}
	} else if(currentRule.kind == ReplaceRule::Kind::Brush) {
		for(const std::string& name : currentRule.replacementBrushNames) {
			Brush* brush = g_brushes.getBrush(name);
			if(brush) {
				DCButton* btn = new DCButton(replacementsScroll, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, brush->getLookID());
				btn->SetToolTip(wxstr(name));
				replSizer->Add(btn, 0, wxALL, 5);
				replacementButtons.push_back(btn);
			}
		}
	}

	replacementsScroll->SetSizer(replSizer);
	replacementsScroll->FitInside();
	replacementsScroll->Layout();
}

// ============================================================================
// SuggestionsPanel implementation

SuggestionsPanel::SuggestionsPanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY)
{
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.surface);

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	titleLabel = new wxStaticText(this, wxID_ANY, "SUGGESTIONS");
	titleLabel->SetForegroundColour(theme.text);
	titleLabel->SetFont(titleLabel->GetFont().Bold());
	mainSizer->Add(titleLabel, 0, wxALL, 5);

	scrollPanel = new wxScrolledWindow(this, wxID_ANY);
	scrollPanel->SetScrollRate(0, 10);
	scrollPanel->SetBackgroundColour(theme.surface);
	mainSizer->Add(scrollPanel, 1, wxALL | wxEXPAND, 5);

	SetSizer(mainSizer);
}

void SuggestionsPanel::UpdateSuggestions(uint16_t itemId)
{
	isItemMode = true;
	suggestedItems.clear();
	suggestedBrushes.clear();

	if(itemId == 0) {
		PopulateSuggestions();
		return;
	}

	const ItemType& sourceType = g_items.getItemType(itemId);
	wxString sourceName = wxstr(sourceType.name).Lower();

	// Extract base name (remove numbers and common suffixes)
	wxString baseName = sourceName;
	baseName.Replace("0", "");
	baseName.Replace("1", "");
	baseName.Replace("2", "");
	baseName.Replace("3", "");
	baseName.Replace("4", "");
	baseName.Replace("5", "");
	baseName.Replace("6", "");
	baseName.Replace("7", "");
	baseName.Replace("8", "");
	baseName.Replace("9", "");
	baseName.Trim();

	// Find items with similar names
	for(uint16_t id = 100; id < g_items.getMaxID(); ++id) {
		if(id == itemId) continue;

		const ItemType& type = g_items.getItemType(id);
		if(type.id == 0 || type.clientID == 0) continue;

		wxString name = wxstr(type.name).Lower();

		// Check if names are similar
		if(!baseName.IsEmpty() && name.Contains(baseName)) {
			suggestedItems.push_back(id);
			if(suggestedItems.size() >= 20) break;
		}
	}

	PopulateSuggestions();
}

void SuggestionsPanel::UpdateSuggestions(Brush* brush)
{
	isItemMode = false;
	suggestedItems.clear();
	suggestedBrushes.clear();

	if(!brush) {
		PopulateSuggestions();
		return;
	}

	wxString sourceName = wxstr(brush->getName()).Lower();

	// Extract base name
	wxString baseName = sourceName;
	baseName.Replace("0", "");
	baseName.Replace("1", "");
	baseName.Replace("2", "");
	baseName.Replace("3", "");
	baseName.Replace("4", "");
	baseName.Replace("5", "");
	baseName.Replace("6", "");
	baseName.Replace("7", "");
	baseName.Replace("8", "");
	baseName.Replace("9", "");
	baseName.Trim();

	// Find brushes with similar names (same type: ground or carpet)
	bool isCarpetBrush = brush->isCarpet();
	const BrushMap& brushMap = g_brushes.getMap();
	for(const auto& pair : brushMap) {
		Brush* b = pair.second;
		if(!b || b == brush) continue;

		// Match same brush type (ground with ground, carpet with carpet)
		if(isCarpetBrush) {
			if(!b->isCarpet()) continue;
		} else {
			if(!b->isGround()) continue;
		}

		wxString name = wxstr(b->getName()).Lower();

		if(!baseName.IsEmpty() && name.Contains(baseName)) {
			suggestedBrushes.push_back(b);
			if(suggestedBrushes.size() >= 20) break;
		}
	}

	PopulateSuggestions();
}

void SuggestionsPanel::Clear()
{
	suggestedItems.clear();
	suggestedBrushes.clear();
	PopulateSuggestions();
}

void SuggestionsPanel::SetItemClickCallback(std::function<void(uint16_t)> callback)
{
	itemClickCallback = callback;
}

void SuggestionsPanel::SetBrushClickCallback(std::function<void(Brush*)> callback)
{
	brushClickCallback = callback;
}

void SuggestionsPanel::OnItemClicked(wxMouseEvent& event)
{
	DCButton* btn = dynamic_cast<DCButton*>(event.GetEventObject());
	if(!btn) return;

	if(isItemMode) {
		uint16_t id = static_cast<uint16_t>(btn->GetValue());
		if(itemClickCallback) itemClickCallback(id);
	} else {
		Brush* brush = reinterpret_cast<Brush*>(btn->GetClientData());
		if(brush && brushClickCallback) brushClickCallback(brush);
	}

	event.Skip();
}

void SuggestionsPanel::PopulateSuggestions()
{
	scrollPanel->DestroyChildren();

	wxGridSizer* grid = new wxGridSizer(2, 5, 5);

	if(isItemMode) {
		for(uint16_t id : suggestedItems) {
			const ItemType& type = g_items.getItemType(id);
			DCButton* btn = new DCButton(scrollPanel, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, type.clientID);
			btn->SetValue(id);
			btn->SetToolTip(wxString::Format("%s\nS: %d\nC: %d", wxstr(type.name), id, type.clientID));
			btn->Bind(wxEVT_LEFT_DOWN, &SuggestionsPanel::OnItemClicked, this);
			grid->Add(btn, 0, wxALL, 2);
		}
	} else {
		for(Brush* brush : suggestedBrushes) {
			DCButton* btn = new DCButton(scrollPanel, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, brush->getLookID());
			btn->SetClientData(brush);
			btn->SetToolTip(wxstr(brush->getName()));
			btn->Bind(wxEVT_LEFT_DOWN, &SuggestionsPanel::OnItemClicked, this);
			grid->Add(btn, 0, wxALL, 2);
		}
	}

	scrollPanel->SetSizer(grid);
	scrollPanel->FitInside();
	scrollPanel->Layout();
}

// ============================================================================
// SavedRulesPanel implementation

SavedRulesPanel::SavedRulesPanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY)
{
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.surface);

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText* title = new wxStaticText(this, wxID_ANY, "SAVED RULES");
	title->SetForegroundColour(theme.text);
	title->SetFont(title->GetFont().Bold());
	mainSizer->Add(title, 0, wxALL, 5);

	rulesList = new wxListBox(this, wxID_ANY);
	rulesList->SetBackgroundColour(theme.background);
	rulesList->SetForegroundColour(theme.text);
	mainSizer->Add(rulesList, 1, wxALL | wxEXPAND, 5);

	// Name input for saving
	nameInput = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
	nameInput->SetHint("Rule set name...");
	mainSizer->Add(nameInput, 0, wxLEFT | wxRIGHT | wxEXPAND, 5);

	// Buttons
	wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
	deleteBtn = new wxButton(this, wxID_ANY, "Delete", wxDefaultPosition, wxSize(60, -1));
	renameBtn = new wxButton(this, wxID_ANY, "Rename", wxDefaultPosition, wxSize(60, -1));
	deleteBtn->Enable(false);
	renameBtn->Enable(false);

	btnSizer->Add(deleteBtn, 0, wxRIGHT, 5);
	btnSizer->Add(renameBtn, 0);
	mainSizer->Add(btnSizer, 0, wxALL, 5);

	SetSizer(mainSizer);

	rulesList->Bind(wxEVT_LISTBOX, &SavedRulesPanel::OnRuleSelected, this);
	deleteBtn->Bind(wxEVT_BUTTON, &SavedRulesPanel::OnDeleteClicked, this);
	renameBtn->Bind(wxEVT_BUTTON, &SavedRulesPanel::OnRenameClicked, this);

	LoadRules();
}

void SavedRulesPanel::LoadRules()
{
	savedRules.clear();
	rulesList->Clear();

	wxString path = GetRulesFilePath();
	if(!wxFileExists(path)) return;

	pugi::xml_document doc;
	if(!doc.load_file(path.ToStdString().c_str())) return;

	pugi::xml_node root = doc.child("saved_rules");
	for(pugi::xml_node ruleSetNode = root.child("ruleset"); ruleSetNode; ruleSetNode = ruleSetNode.next_sibling("ruleset")) {
		SavedRuleSet ruleSet;
		ruleSet.name = wxString::FromUTF8(ruleSetNode.attribute("name").as_string());

		for(pugi::xml_node ruleNode = ruleSetNode.child("rule"); ruleNode; ruleNode = ruleNode.next_sibling("rule")) {
			ReplaceRule rule;
			std::string kindStr = ruleNode.attribute("kind").as_string();
			rule.kind = (kindStr == "brush") ? ReplaceRule::Kind::Brush : ReplaceRule::Kind::ItemId;

			if(rule.kind == ReplaceRule::Kind::ItemId) {
				rule.originalId = ruleNode.attribute("original").as_uint();
				for(pugi::xml_node replNode = ruleNode.child("replacement"); replNode; replNode = replNode.next_sibling("replacement")) {
					rule.replacementIds.push_back(replNode.attribute("id").as_uint());
				}
			} else {
				rule.originalBrushName = ruleNode.attribute("original").as_string();
				for(pugi::xml_node replNode = ruleNode.child("replacement"); replNode; replNode = replNode.next_sibling("replacement")) {
					rule.replacementBrushNames.push_back(replNode.attribute("name").as_string());
				}
			}

			if(rule.isValid()) {
				ruleSet.rules.push_back(rule);
			}
		}

		if(!ruleSet.rules.empty()) {
			savedRules.push_back(ruleSet);
			rulesList->Append(ruleSet.name);
		}
	}
}

void SavedRulesPanel::SaveRules()
{
	pugi::xml_document doc;
	pugi::xml_node root = doc.append_child("saved_rules");

	for(const SavedRuleSet& ruleSet : savedRules) {
		pugi::xml_node ruleSetNode = root.append_child("ruleset");
		ruleSetNode.append_attribute("name") = ruleSet.name.ToUTF8().data();

		for(const ReplaceRule& rule : ruleSet.rules) {
			pugi::xml_node ruleNode = ruleSetNode.append_child("rule");
			ruleNode.append_attribute("kind") = (rule.kind == ReplaceRule::Kind::Brush) ? "brush" : "item";

			if(rule.kind == ReplaceRule::Kind::ItemId) {
				ruleNode.append_attribute("original") = rule.originalId;
				for(uint16_t id : rule.replacementIds) {
					pugi::xml_node replNode = ruleNode.append_child("replacement");
					replNode.append_attribute("id") = id;
				}
			} else {
				ruleNode.append_attribute("original") = rule.originalBrushName.c_str();
				for(const std::string& name : rule.replacementBrushNames) {
					pugi::xml_node replNode = ruleNode.append_child("replacement");
					replNode.append_attribute("name") = name.c_str();
				}
			}
		}
	}

	doc.save_file(GetRulesFilePath().ToStdString().c_str());
}

void SavedRulesPanel::AddRuleSet(const SavedRuleSet& ruleSet)
{
	// Check if name exists and update
	for(size_t i = 0; i < savedRules.size(); ++i) {
		if(savedRules[i].name == ruleSet.name) {
			savedRules[i] = ruleSet;
			SaveRules();
			return;
		}
	}

	savedRules.push_back(ruleSet);
	rulesList->Append(ruleSet.name);
	SaveRules();
}

void SavedRulesPanel::RemoveSelectedRuleSet()
{
	int sel = rulesList->GetSelection();
	if(sel == wxNOT_FOUND || sel >= static_cast<int>(savedRules.size())) return;

	savedRules.erase(savedRules.begin() + sel);
	rulesList->Delete(sel);
	SaveRules();

	deleteBtn->Enable(false);
	renameBtn->Enable(false);
}

SavedRuleSet* SavedRulesPanel::GetSelectedRuleSet()
{
	int sel = rulesList->GetSelection();
	if(sel == wxNOT_FOUND || sel >= static_cast<int>(savedRules.size())) return nullptr;
	return &savedRules[sel];
}

void SavedRulesPanel::SetRuleSelectedCallback(std::function<void(SavedRuleSet*)> callback)
{
	ruleSelectedCallback = callback;
}

void SavedRulesPanel::OnRuleSelected(wxCommandEvent& event)
{
	int sel = rulesList->GetSelection();
	bool hasSelection = sel != wxNOT_FOUND;
	deleteBtn->Enable(hasSelection);
	renameBtn->Enable(hasSelection);

	if(hasSelection && ruleSelectedCallback) {
		ruleSelectedCallback(&savedRules[sel]);
	}
}

void SavedRulesPanel::OnDeleteClicked(wxCommandEvent& event)
{
	RemoveSelectedRuleSet();
}

void SavedRulesPanel::OnRenameClicked(wxCommandEvent& event)
{
	int sel = rulesList->GetSelection();
	if(sel == wxNOT_FOUND) return;

	wxString newName = wxGetTextFromUser("Enter new name:", "Rename Rule Set", savedRules[sel].name, this);
	if(!newName.IsEmpty()) {
		savedRules[sel].name = newName;
		rulesList->SetString(sel, newName);
		SaveRules();
	}
}

wxString SavedRulesPanel::GetRulesFilePath()
{
	return g_gui.GetDataDirectory() + "/replace_rules.xml";
}

// ============================================================================
// ActiveRuleDropTarget implementation

ActiveRuleDropTarget::ActiveRuleDropTarget(ActiveRuleRow* row, bool isOriginalSlot) :
	m_row(row),
	m_isOriginalSlot(isOriginalSlot)
{
}

bool ActiveRuleDropTarget::OnDropText(wxCoord x, wxCoord y, const wxString& data)
{
	if(data.StartsWith("ITEM_ID:")) {
		wxString idStr = data.Mid(8);
		long itemId = 0;
		if(idStr.ToLong(&itemId) && itemId > 0 && itemId <= 0xFFFF) {
			m_row->OnItemDropped(static_cast<uint16_t>(itemId), m_isOriginalSlot);
			return true;
		}
	} else if(data.StartsWith("BRUSH:")) {
		wxString brushName = data.Mid(6);
		if(!brushName.IsEmpty()) {
			m_row->OnBrushDropped(nstr(brushName), m_isOriginalSlot);
			return true;
		}
	}
	return false;
}

// ============================================================================
// ActiveRuleRow implementation

ActiveRuleRow::ActiveRuleRow(wxWindow* parent, int index, const ReplaceRule& ruleData) :
	wxPanel(parent, wxID_ANY),
	rowIndex(index),
	rule(ruleData)
{
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.background);

	wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);

	// Original item/brush button
	wxBoxSizer* origSizer = new wxBoxSizer(wxVERTICAL);
	originalBtn = new DCButton(this, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, 0);
	originalBtn->SetMinSize(wxSize(36, 36));
	originalBtn->SetDropTarget(new ActiveRuleDropTarget(this, true));
	origSizer->Add(originalBtn, 0, wxALIGN_CENTER);
	originalLabel = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxSize(80, -1), wxALIGN_CENTER | wxST_ELLIPSIZE_END);
	originalLabel->SetForegroundColour(theme.text);
	originalLabel->SetFont(originalLabel->GetFont().Smaller());
	origSizer->Add(originalLabel, 0, wxALIGN_CENTER | wxTOP, 2);
	mainSizer->Add(origSizer, 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);

	// Arrow
	arrowLabel = new wxStaticText(this, wxID_ANY, " -> ");
	arrowLabel->SetForegroundColour(wxColour(150, 150, 150));
	mainSizer->Add(arrowLabel, 0, wxALIGN_CENTER_VERTICAL);

	// Replacements sizer (can hold multiple replacement items)
	replacementsSizer = new wxBoxSizer(wxHORIZONTAL);
	mainSizer->Add(replacementsSizer, 1, wxALIGN_CENTER_VERTICAL);

	SetSizer(mainSizer);

	// Bind events
	Bind(wxEVT_LEFT_DOWN, &ActiveRuleRow::OnClick, this);
	originalBtn->Bind(wxEVT_LEFT_DOWN, &ActiveRuleRow::OnOriginalSlotClick, this);
	originalLabel->Bind(wxEVT_LEFT_DOWN, &ActiveRuleRow::OnOriginalSlotClick, this);
	arrowLabel->Bind(wxEVT_LEFT_DOWN, &ActiveRuleRow::OnClick, this);

	UpdateDisplay();
}

void ActiveRuleRow::SetRule(const ReplaceRule& ruleData)
{
	rule = ruleData;
	UpdateDisplay();
}

void ActiveRuleRow::SetSelected(bool selected)
{
	isSelected = selected;
	const ThemeColors& theme = Theme::Dark();

	if(isSelected) {
		SetBackgroundColour(wxColour(60, 80, 100));
	} else {
		SetBackgroundColour(theme.background);
	}
	Refresh();
}

void ActiveRuleRow::SetClickCallback(std::function<void(int)> callback)
{
	clickCallback = callback;
}

void ActiveRuleRow::SetOriginalSlotClickCallback(std::function<void(int)> callback)
{
	originalSlotClickCallback = callback;
}

void ActiveRuleRow::SetReplacementSlotClickCallback(std::function<void(int, int)> callback)
{
	replacementSlotClickCallback = callback;
}

void ActiveRuleRow::SetItemDropCallback(std::function<void(int, uint16_t, bool)> callback)
{
	itemDropCallback = callback;
}

void ActiveRuleRow::SetBrushDropCallback(std::function<void(int, const std::string&, bool)> callback)
{
	brushDropCallback = callback;
}

void ActiveRuleRow::OnItemDropped(uint16_t itemId, bool isOriginalSlot)
{
	if(itemDropCallback) {
		itemDropCallback(rowIndex, itemId, isOriginalSlot);
	}
}

void ActiveRuleRow::OnBrushDropped(const std::string& brushName, bool isOriginalSlot)
{
	if(brushDropCallback) {
		brushDropCallback(rowIndex, brushName, isOriginalSlot);
	}
}

void ActiveRuleRow::OnClick(wxMouseEvent& event)
{
	if(clickCallback) {
		clickCallback(rowIndex);
	}
}

void ActiveRuleRow::OnOriginalSlotClick(wxMouseEvent& event)
{
	if(clickCallback) {
		clickCallback(rowIndex);
	}
	if(originalSlotClickCallback) {
		originalSlotClickCallback(rowIndex);
	}
}

void ActiveRuleRow::OnReplacementSlotClick(wxMouseEvent& event, int replIndex)
{
	if(clickCallback) {
		clickCallback(rowIndex);
	}
	if(replacementSlotClickCallback) {
		replacementSlotClickCallback(rowIndex, replIndex);
	}
}

void ActiveRuleRow::UpdateDisplay()
{
	const ThemeColors& theme = Theme::Dark();

	// Clear existing replacement buttons
	for(auto btn : replacementBtns) {
		btn->Destroy();
	}
	replacementBtns.clear();
	for(auto lbl : replacementLabels) {
		lbl->Destroy();
	}
	replacementLabels.clear();
	replacementsSizer->Clear();

	// Update original slot
	if(rule.kind == ReplaceRule::Kind::ItemId) {
		const ItemType& origType = g_items.getItemType(rule.originalId);
		originalBtn->SetSprite(origType.clientID);
		originalLabel->SetLabel(wxString::Format("%d", rule.originalId));
		originalBtn->SetToolTip(wxString::Format("%s\nID: %d", wxstr(origType.name), rule.originalId));
	} else {
		Brush* brush = g_brushes.getBrush(rule.originalBrushName);
		if(brush) {
			originalBtn->SetSprite(brush->getLookID());
			originalLabel->SetLabel(wxstr(rule.originalBrushName));
			originalBtn->SetToolTip(wxstr(rule.originalBrushName));
		}
	}

	// Create replacement buttons
	if(rule.kind == ReplaceRule::Kind::ItemId) {
		for(size_t i = 0; i < rule.replacementIds.size(); ++i) {
			wxBoxSizer* replSizer = new wxBoxSizer(wxVERTICAL);

			DCButton* btn = new DCButton(this, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, 0);
			btn->SetMinSize(wxSize(36, 36));
			btn->SetDropTarget(new ActiveRuleDropTarget(this, false));

			const ItemType& replType = g_items.getItemType(rule.replacementIds[i]);
			btn->SetSprite(replType.clientID);
			btn->SetToolTip(wxString::Format("%s\nID: %d", wxstr(replType.name), rule.replacementIds[i]));

			wxStaticText* lbl = new wxStaticText(this, wxID_ANY, wxString::Format("%d", rule.replacementIds[i]),
				wxDefaultPosition, wxSize(80, -1), wxALIGN_CENTER | wxST_ELLIPSIZE_END);
			lbl->SetForegroundColour(theme.text);
			lbl->SetFont(lbl->GetFont().Smaller());

			replSizer->Add(btn, 0, wxALIGN_CENTER);
			replSizer->Add(lbl, 0, wxALIGN_CENTER | wxTOP, 2);
			replacementsSizer->Add(replSizer, 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);

			// Bind click events
			int replIndex = static_cast<int>(i);
			btn->Bind(wxEVT_LEFT_DOWN, [this, replIndex](wxMouseEvent& evt) { OnReplacementSlotClick(evt, replIndex); });
			lbl->Bind(wxEVT_LEFT_DOWN, [this, replIndex](wxMouseEvent& evt) { OnReplacementSlotClick(evt, replIndex); });

			replacementBtns.push_back(btn);
			replacementLabels.push_back(lbl);
		}
	} else {
		for(size_t i = 0; i < rule.replacementBrushNames.size(); ++i) {
			wxBoxSizer* replSizer = new wxBoxSizer(wxVERTICAL);

			DCButton* btn = new DCButton(this, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, 0);
			btn->SetMinSize(wxSize(36, 36));
			btn->SetDropTarget(new ActiveRuleDropTarget(this, false));

			Brush* brush = g_brushes.getBrush(rule.replacementBrushNames[i]);
			if(brush) {
				btn->SetSprite(brush->getLookID());
				btn->SetToolTip(wxstr(rule.replacementBrushNames[i]));
			}

			wxStaticText* lbl = new wxStaticText(this, wxID_ANY, wxstr(rule.replacementBrushNames[i]),
				wxDefaultPosition, wxSize(80, -1), wxALIGN_CENTER | wxST_ELLIPSIZE_END);
			lbl->SetForegroundColour(theme.text);
			lbl->SetFont(lbl->GetFont().Smaller());

			replSizer->Add(btn, 0, wxALIGN_CENTER);
			replSizer->Add(lbl, 0, wxALIGN_CENTER | wxTOP, 2);
			replacementsSizer->Add(replSizer, 0, wxALL | wxALIGN_CENTER_VERTICAL, 3);

			// Bind click events
			int replIndex = static_cast<int>(i);
			btn->Bind(wxEVT_LEFT_DOWN, [this, replIndex](wxMouseEvent& evt) { OnReplacementSlotClick(evt, replIndex); });
			lbl->Bind(wxEVT_LEFT_DOWN, [this, replIndex](wxMouseEvent& evt) { OnReplacementSlotClick(evt, replIndex); });

			replacementBtns.push_back(btn);
			replacementLabels.push_back(lbl);
		}
	}

	Layout();
	Refresh();
}

// ============================================================================
// ActiveRulesPanel implementation

ActiveRulesPanel::ActiveRulesPanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY)
{
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.surface);

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	// Scrolled window for rules
	scrollPanel = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
									   wxVSCROLL | wxBORDER_NONE);
	scrollPanel->SetBackgroundColour(theme.background);
	scrollPanel->SetScrollRate(0, 10);

	rowsSizer = new wxBoxSizer(wxVERTICAL);
	scrollPanel->SetSizer(rowsSizer);

	mainSizer->Add(scrollPanel, 1, wxEXPAND);
	SetSizer(mainSizer);
}

void ActiveRulesPanel::AddRule(const ReplaceRule& rule)
{
	rules.push_back(rule);

	ActiveRuleRow* row = new ActiveRuleRow(scrollPanel, static_cast<int>(rules.size() - 1), rule);
	row->SetClickCallback([this](int idx) { OnRowSelected(idx); });
	row->SetOriginalSlotClickCallback(originalSlotClickCallback);
	row->SetReplacementSlotClickCallback(replacementSlotClickCallback);
	row->SetItemDropCallback(itemDropCallback);
	row->SetBrushDropCallback(brushDropCallback);
	rowsSizer->Add(row, 0, wxEXPAND | wxBOTTOM, 2);
	rows.push_back(row);

	scrollPanel->FitInside();
	scrollPanel->Layout();
}

void ActiveRulesPanel::RemoveRule(int index)
{
	if(index < 0 || index >= static_cast<int>(rules.size())) return;

	rules.erase(rules.begin() + index);

	if(selectedIndex == index) {
		selectedIndex = -1;
	} else if(selectedIndex > index) {
		selectedIndex--;
	}

	RebuildRows();
}

void ActiveRulesPanel::UpdateRule(int index, const ReplaceRule& rule)
{
	if(index < 0 || index >= static_cast<int>(rules.size())) return;

	rules[index] = rule;

	if(index < static_cast<int>(rows.size())) {
		rows[index]->SetRule(rule);
	}
}

void ActiveRulesPanel::Clear()
{
	rules.clear();
	selectedIndex = -1;
	RebuildRows();
}

void ActiveRulesPanel::SetRules(const std::vector<ReplaceRule>& newRules)
{
	rules = newRules;
	selectedIndex = -1;
	RebuildRows();
}

ReplaceRule* ActiveRulesPanel::GetSelectedRule()
{
	if(selectedIndex < 0 || selectedIndex >= static_cast<int>(rules.size())) return nullptr;
	return &rules[selectedIndex];
}

void ActiveRulesPanel::SetRuleClickCallback(std::function<void(int)> callback)
{
	ruleClickCallback = callback;
}

void ActiveRulesPanel::SetOriginalSlotClickCallback(std::function<void(int)> callback)
{
	originalSlotClickCallback = callback;
}

void ActiveRulesPanel::SetReplacementSlotClickCallback(std::function<void(int, int)> callback)
{
	replacementSlotClickCallback = callback;
}

void ActiveRulesPanel::SetItemDropCallback(std::function<void(int, uint16_t, bool)> callback)
{
	itemDropCallback = callback;
}

void ActiveRulesPanel::SetBrushDropCallback(std::function<void(int, const std::string&, bool)> callback)
{
	brushDropCallback = callback;
}

void ActiveRulesPanel::OnRowSelected(int index)
{
	// Deselect previous
	if(selectedIndex >= 0 && selectedIndex < static_cast<int>(rows.size())) {
		rows[selectedIndex]->SetSelected(false);
	}

	selectedIndex = index;

	// Select new
	if(selectedIndex >= 0 && selectedIndex < static_cast<int>(rows.size())) {
		rows[selectedIndex]->SetSelected(true);
	}

	if(ruleClickCallback) {
		ruleClickCallback(index);
	}
}

void ActiveRulesPanel::RebuildRows()
{
	// Clear existing rows
	for(auto row : rows) {
		row->Destroy();
	}
	rows.clear();
	rowsSizer->Clear();

	// Create new rows
	for(size_t i = 0; i < rules.size(); ++i) {
		ActiveRuleRow* row = new ActiveRuleRow(scrollPanel, static_cast<int>(i), rules[i]);
		row->SetClickCallback([this](int idx) { OnRowSelected(idx); });
		row->SetOriginalSlotClickCallback(originalSlotClickCallback);
		row->SetReplacementSlotClickCallback(replacementSlotClickCallback);
		row->SetItemDropCallback(itemDropCallback);
		row->SetBrushDropCallback(brushDropCallback);
		rowsSizer->Add(row, 0, wxEXPAND | wxBOTTOM, 2);
		rows.push_back(row);
	}

	scrollPanel->FitInside();
	scrollPanel->Layout();
}

// ============================================================================
// BatchReplaceDropTarget implementation

BatchReplaceDropTarget::BatchReplaceDropTarget(BatchReplaceRow* row) :
	m_row(row)
{
}

bool BatchReplaceDropTarget::OnDropText(wxCoord x, wxCoord y, const wxString& data)
{
	if(data.StartsWith("ITEM_ID:")) {
		wxString idStr = data.Mid(8);
		long itemId = 0;
		if(idStr.ToLong(&itemId) && itemId > 0 && itemId <= 0xFFFF) {
			m_row->OnItemDropped(static_cast<uint16_t>(itemId));
			return true;
		}
	} else if(data.StartsWith("BRUSH:")) {
		wxString brushName = data.Mid(6);
		if(!brushName.IsEmpty()) {
			m_row->OnBrushDropped(nstr(brushName));
			return true;
		}
	}
	return false;
}

// ============================================================================
// BatchReplaceRow implementation

BatchReplaceRow::BatchReplaceRow(wxWindow* parent, int index, bool itemMode) :
	wxPanel(parent, wxID_ANY),
	rowIndex(index),
	isItemMode(itemMode)
{
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.background);

	wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);

	// Original item button and label
	wxBoxSizer* origSizer = new wxBoxSizer(wxVERTICAL);
	originalBtn = new DCButton(this, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, 0);
	originalBtn->SetMinSize(wxSize(36, 36));
	origSizer->Add(originalBtn, 0, wxALIGN_CENTER);
	originalLabel = new wxStaticText(this, wxID_ANY, "", wxDefaultPosition, wxSize(100, -1), wxALIGN_CENTER | wxST_ELLIPSIZE_END);
	originalLabel->SetForegroundColour(theme.text);
	originalLabel->SetFont(originalLabel->GetFont().Smaller());
	origSizer->Add(originalLabel, 0, wxALIGN_CENTER | wxTOP, 2);
	mainSizer->Add(origSizer, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

	// Arrow
	arrowLabel = new wxStaticText(this, wxID_ANY, "  ->  ");
	arrowLabel->SetForegroundColour(wxColour(150, 150, 150));
	mainSizer->Add(arrowLabel, 0, wxALIGN_CENTER_VERTICAL);

	// Replacement item button and label
	wxBoxSizer* replSizer = new wxBoxSizer(wxVERTICAL);
	replacementBtn = new DCButton(this, wxID_ANY, wxDefaultPosition, DC_BTN_TOGGLE, RENDER_SIZE_32x32, 0);
	replacementBtn->SetMinSize(wxSize(36, 36));
	replacementBtn->SetDropTarget(new BatchReplaceDropTarget(this));
	replSizer->Add(replacementBtn, 0, wxALIGN_CENTER);
	replacementLabel = new wxStaticText(this, wxID_ANY, "Click to set", wxDefaultPosition, wxSize(100, -1), wxALIGN_CENTER | wxST_ELLIPSIZE_END);
	replacementLabel->SetForegroundColour(wxColour(100, 100, 100));
	replacementLabel->SetFont(replacementLabel->GetFont().Smaller());
	replSizer->Add(replacementLabel, 0, wxALIGN_CENTER | wxTOP, 2);
	mainSizer->Add(replSizer, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

	// Count label
	countLabel = new wxStaticText(this, wxID_ANY, "x0", wxDefaultPosition, wxSize(50, -1), wxALIGN_RIGHT);
	countLabel->SetForegroundColour(wxColour(150, 150, 150));
	mainSizer->Add(countLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

	SetSizer(mainSizer);

	// Bind click events to the entire row
	Bind(wxEVT_LEFT_DOWN, &BatchReplaceRow::OnClick, this);
	originalBtn->Bind(wxEVT_LEFT_DOWN, &BatchReplaceRow::OnClick, this);
	originalLabel->Bind(wxEVT_LEFT_DOWN, &BatchReplaceRow::OnClick, this);
	replacementBtn->Bind(wxEVT_LEFT_DOWN, &BatchReplaceRow::OnReplacementClick, this);
	replacementLabel->Bind(wxEVT_LEFT_DOWN, &BatchReplaceRow::OnReplacementClick, this);
	arrowLabel->Bind(wxEVT_LEFT_DOWN, &BatchReplaceRow::OnClick, this);
	countLabel->Bind(wxEVT_LEFT_DOWN, &BatchReplaceRow::OnClick, this);
}

void BatchReplaceRow::SetItemData(uint16_t originalId, uint16_t replacementId, int count, bool hasRepl)
{
	originalItemId = originalId;
	replacementItemId = replacementId;
	itemCount = count;
	hasReplacement = hasRepl;
	UpdateDisplay();
}

void BatchReplaceRow::SetBrushData(const std::string& originalBrush, const std::string& replacementBrush, int count, bool hasRepl)
{
	originalBrushName = originalBrush;
	replacementBrushName = replacementBrush;
	itemCount = count;
	hasReplacement = hasRepl;
	UpdateDisplay();
}

void BatchReplaceRow::SetSelected(bool selected)
{
	isSelected = selected;
	const ThemeColors& theme = Theme::Dark();

	if(isSelected) {
		SetBackgroundColour(wxColour(60, 80, 100));
	} else if(hasReplacement) {
		SetBackgroundColour(wxColour(40, 60, 40));
	} else {
		SetBackgroundColour(theme.background);
	}
	Refresh();
}

void BatchReplaceRow::SetClickCallback(std::function<void(int)> callback)
{
	clickCallback = callback;
}

void BatchReplaceRow::SetReplacementClickCallback(std::function<void(int)> callback)
{
	replacementClickCallback = callback;
}

void BatchReplaceRow::SetItemDropCallback(std::function<void(int, uint16_t)> callback)
{
	itemDropCallback = callback;
}

void BatchReplaceRow::SetBrushDropCallback(std::function<void(int, const std::string&)> callback)
{
	brushDropCallback = callback;
}

void BatchReplaceRow::OnItemDropped(uint16_t itemId)
{
	if(itemDropCallback) {
		itemDropCallback(rowIndex, itemId);
	}
}

void BatchReplaceRow::OnBrushDropped(const std::string& brushName)
{
	if(brushDropCallback) {
		brushDropCallback(rowIndex, brushName);
	}
}

void BatchReplaceRow::OnClick(wxMouseEvent& event)
{
	if(clickCallback) {
		clickCallback(rowIndex);
	}
}

void BatchReplaceRow::OnReplacementClick(wxMouseEvent& event)
{
	// Select the row first
	if(clickCallback) {
		clickCallback(rowIndex);
	}
	// Then notify about replacement area click
	if(replacementClickCallback) {
		replacementClickCallback(rowIndex);
	}
}

void BatchReplaceRow::UpdateDisplay()
{
	const ThemeColors& theme = Theme::Dark();

	if(isItemMode) {
		// Original item
		const ItemType& origType = g_items.getItemType(originalItemId);
		originalBtn->SetSprite(origType.clientID);
		originalLabel->SetLabel(wxstr(origType.name));
		originalBtn->SetToolTip(wxString::Format("%s\nID: %d", wxstr(origType.name), originalItemId));

		// Replacement item
		if(hasReplacement && replacementItemId != 0) {
			const ItemType& replType = g_items.getItemType(replacementItemId);
			replacementBtn->SetSprite(replType.clientID);
			replacementLabel->SetLabel(wxstr(replType.name));
			replacementLabel->SetForegroundColour(theme.text);
			replacementBtn->SetToolTip(wxString::Format("%s\nID: %d", wxstr(replType.name), replacementItemId));
		} else {
			replacementBtn->SetSprite(0);
			replacementLabel->SetLabel("Click to set");
			replacementLabel->SetForegroundColour(wxColour(100, 100, 100));
			replacementBtn->SetToolTip("Double-click an item in the library");
		}
	} else {
		// Original brush
		Brush* origBrush = g_brushes.getBrush(originalBrushName);
		if(origBrush) {
			originalBtn->SetSprite(origBrush->getLookID());
			originalLabel->SetLabel(wxstr(originalBrushName));
			originalBtn->SetToolTip(wxstr(originalBrushName));
		}

		// Replacement brush
		if(hasReplacement && !replacementBrushName.empty()) {
			Brush* replBrush = g_brushes.getBrush(replacementBrushName);
			if(replBrush) {
				replacementBtn->SetSprite(replBrush->getLookID());
				replacementLabel->SetLabel(wxstr(replacementBrushName));
				replacementLabel->SetForegroundColour(theme.text);
				replacementBtn->SetToolTip(wxstr(replacementBrushName));
			}
		} else {
			replacementBtn->SetSprite(0);
			replacementLabel->SetLabel("Click to set");
			replacementLabel->SetForegroundColour(wxColour(100, 100, 100));
			replacementBtn->SetToolTip("Double-click a brush in the library");
		}
	}

	countLabel->SetLabel(wxString::Format("x%d", itemCount));

	// Update background based on state
	if(isSelected) {
		SetBackgroundColour(wxColour(60, 80, 100));
	} else if(hasReplacement) {
		SetBackgroundColour(wxColour(40, 60, 40));
	} else {
		SetBackgroundColour(theme.background);
	}

	Layout();
	Refresh();
}

// ============================================================================
// BatchReplacePanel implementation

BatchReplacePanel::BatchReplacePanel(wxWindow* parent) :
	wxPanel(parent, wxID_ANY),
	loadMoreTimer(nullptr)
{
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.surface);

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	// Title and status
	wxBoxSizer* headerSizer = new wxBoxSizer(wxHORIZONTAL);
	wxStaticText* title = new wxStaticText(this, wxID_ANY, "BATCH REPLACE");
	title->SetForegroundColour(theme.text);
	title->SetFont(title->GetFont().Bold());
	headerSizer->Add(title, 0, wxALIGN_CENTER_VERTICAL);
	headerSizer->AddStretchSpacer();
	statusLabel = new wxStaticText(this, wxID_ANY, "No items identified");
	statusLabel->SetForegroundColour(wxColour(150, 150, 150));
	headerSizer->Add(statusLabel, 0, wxALIGN_CENTER_VERTICAL);
	mainSizer->Add(headerSizer, 0, wxALL | wxEXPAND, 5);

	// Options row
	wxBoxSizer* optionsSizer = new wxBoxSizer(wxHORIZONTAL);
	groupSimilarCheck = new wxCheckBox(this, wxID_ANY, "Group by brush (hide variations)");
	groupSimilarCheck->SetValue(true);
	groupSimilarCheck->SetForegroundColour(theme.text);
	groupSimilarCheck->SetToolTip("When enabled, items from the same brush are grouped together");
	optionsSizer->Add(groupSimilarCheck, 0, wxALIGN_CENTER_VERTICAL);
	mainSizer->Add(optionsSizer, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 5);

	// Column headers
	wxBoxSizer* colHeaderSizer = new wxBoxSizer(wxHORIZONTAL);
	wxStaticText* origHeader = new wxStaticText(this, wxID_ANY, "Original", wxDefaultPosition, wxSize(120, -1));
	origHeader->SetForegroundColour(wxColour(150, 150, 150));
	colHeaderSizer->Add(origHeader, 0, wxLEFT, 10);
	colHeaderSizer->AddSpacer(40);
	wxStaticText* replHeader = new wxStaticText(this, wxID_ANY, "Replacement", wxDefaultPosition, wxSize(120, -1));
	replHeader->SetForegroundColour(wxColour(150, 150, 150));
	colHeaderSizer->Add(replHeader, 0);
	colHeaderSizer->AddStretchSpacer();
	wxStaticText* countHeader = new wxStaticText(this, wxID_ANY, "Count", wxDefaultPosition, wxSize(50, -1), wxALIGN_RIGHT);
	countHeader->SetForegroundColour(wxColour(150, 150, 150));
	colHeaderSizer->Add(countHeader, 0, wxRIGHT, 10);
	mainSizer->Add(colHeaderSizer, 0, wxEXPAND | wxBOTTOM, 2);

	// Scrolled panel for rows
	scrollPanel = new wxScrolledWindow(this, wxID_ANY);
	scrollPanel->SetScrollRate(0, 10);
	scrollPanel->SetBackgroundColour(theme.background);
	rowsSizer = new wxBoxSizer(wxVERTICAL);
	scrollPanel->SetSizer(rowsSizer);
	mainSizer->Add(scrollPanel, 1, wxALL | wxEXPAND, 5);

	// Hint
	wxStaticText* hint = new wxStaticText(this, wxID_ANY, "Select a row, then double-click in library to set replacement");
	hint->SetForegroundColour(wxColour(100, 150, 200));
	mainSizer->Add(hint, 0, wxALL | wxALIGN_CENTER, 5);

	// Buttons
	wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
	clearReplacementBtn = new wxButton(this, wxID_ANY, "Clear Selected");
	clearAllBtn = new wxButton(this, wxID_ANY, "Clear All");
	clearReplacementBtn->Enable(false);
	btnSizer->Add(clearReplacementBtn, 0, wxRIGHT, 5);
	btnSizer->Add(clearAllBtn, 0);
	mainSizer->Add(btnSizer, 0, wxALL | wxALIGN_RIGHT, 5);

	SetSizer(mainSizer);

	// Create timer for lazy loading
	loadMoreTimer = new wxTimer(this);

	// Bind events
	clearReplacementBtn->Bind(wxEVT_BUTTON, &BatchReplacePanel::OnClearReplacement, this);
	clearAllBtn->Bind(wxEVT_BUTTON, &BatchReplacePanel::OnClearAllReplacements, this);
	scrollPanel->Bind(wxEVT_SCROLLWIN_THUMBTRACK, &BatchReplacePanel::OnScrolled, this);
	scrollPanel->Bind(wxEVT_SCROLLWIN_THUMBRELEASE, &BatchReplacePanel::OnScrolled, this);
	scrollPanel->Bind(wxEVT_SCROLLWIN_LINEDOWN, &BatchReplacePanel::OnScrolled, this);
	scrollPanel->Bind(wxEVT_SCROLLWIN_PAGEDOWN, &BatchReplacePanel::OnScrolled, this);
	Bind(wxEVT_TIMER, &BatchReplacePanel::OnLoadMoreTimer, this);
	groupSimilarCheck->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
		groupSimilarItems = groupSimilarCheck->IsChecked();
		// Re-identify to apply filter - need to store editor reference or just rebuild
		RebuildRows();
	});
}

void BatchReplacePanel::IdentifyItemsFromSelection(Editor* editor)
{
	isItemMode = true;
	identifiedItems.clear();
	identifiedBrushes.clear();
	selectedIndex = -1;

	if(!editor) {
		RebuildRows();
		return;
	}

	const Selection& selection = editor->getSelection();

	if(groupSimilarItems) {
		// Group by brush - only show one representative item per brush
		std::map<std::string, std::pair<uint16_t, int>> brushGroups; // brush name -> (first item id, total count)

		for(Tile* tile : selection.getTiles()) {
			if(!tile) continue;

			// Check ground
			if(tile->ground) {
				GroundBrush* gb = tile->ground->getGroundBrush();
				if(gb) {
					std::string brushName = gb->getName();
					auto it = brushGroups.find(brushName);
					if(it == brushGroups.end()) {
						brushGroups[brushName] = {tile->ground->getID(), 1};
					} else {
						it->second.second++;
					}
				} else {
					// No brush, use item ID as key
					std::string key = "item_" + std::to_string(tile->ground->getID());
					auto it = brushGroups.find(key);
					if(it == brushGroups.end()) {
						brushGroups[key] = {tile->ground->getID(), 1};
					} else {
						it->second.second++;
					}
				}
			}

			// Check items
			for(Item* item : tile->items) {
				if(!item) continue;

				// Check for carpet brush
				if(item->isCarpet()) {
					CarpetBrush* cb = item->getCarpetBrush();
					if(cb) {
						std::string brushName = cb->getName();
						auto it = brushGroups.find(brushName);
						if(it == brushGroups.end()) {
							brushGroups[brushName] = {item->getID(), 1};
						} else {
							it->second.second++;
						}
						continue;
					}
				}

				// Check for wall brush
				if(item->isWall()) {
					WallBrush* wb = item->getWallBrush();
					if(wb) {
						std::string brushName = wb->getName();
						auto it = brushGroups.find(brushName);
						if(it == brushGroups.end()) {
							brushGroups[brushName] = {item->getID(), 1};
						} else {
							it->second.second++;
						}
						continue;
					}
				}

				// Check for border
				if(item->isBorder()) {
					GroundBrush* borderBrush = item->getGroundBrush();
					if(borderBrush) {
						std::string brushName = "border_" + borderBrush->getName();
						auto it = brushGroups.find(brushName);
						if(it == brushGroups.end()) {
							brushGroups[brushName] = {item->getID(), 1};
						} else {
							it->second.second++;
						}
						continue;
					}
				}

				// Regular item - use item ID as key
				std::string key = "item_" + std::to_string(item->getID());
				auto it = brushGroups.find(key);
				if(it == brushGroups.end()) {
					brushGroups[key] = {item->getID(), 1};
				} else {
					it->second.second++;
				}
			}
		}

		// Convert to entries
		for(const auto& pair : brushGroups) {
			IdentifiedItemEntry entry;
			entry.originalId = pair.second.first;
			entry.count = pair.second.second;
			entry.hasReplacement = false;
			identifiedItems.push_back(entry);
		}
	} else {
		// Original behavior - list all unique item IDs
		std::map<uint16_t, int> itemCounts;

		for(Tile* tile : selection.getTiles()) {
			if(!tile) continue;

			// Check ground
			if(tile->ground) {
				itemCounts[tile->ground->getID()]++;
			}

			// Check items
			for(Item* item : tile->items) {
				if(item) {
					itemCounts[item->getID()]++;
				}
			}
		}

		// Convert to entries
		for(const auto& pair : itemCounts) {
			IdentifiedItemEntry entry;
			entry.originalId = pair.first;
			entry.count = pair.second;
			entry.hasReplacement = false;
			identifiedItems.push_back(entry);
		}
	}

	// Sort by count descending
	std::sort(identifiedItems.begin(), identifiedItems.end(), [](const IdentifiedItemEntry& a, const IdentifiedItemEntry& b) {
		return a.count > b.count;
	});

	RebuildRows();
}

void BatchReplacePanel::IdentifyBrushesFromSelection(Editor* editor)
{
	isItemMode = false;
	identifiedItems.clear();
	identifiedBrushes.clear();
	selectedIndex = -1;

	if(!editor) {
		RebuildRows();
		return;
	}

	// Map to count occurrences
	std::map<std::string, int> brushCounts;

	const Selection& selection = editor->getSelection();
	for(Tile* tile : selection.getTiles()) {
		if(!tile) continue;

		// Check ground brush
		if(tile->ground) {
			GroundBrush* gb = tile->ground->getGroundBrush();
			if(gb) {
				brushCounts[gb->getName()]++;
			}
		}

		// Check carpet brushes
		for(Item* item : tile->items) {
			if(item && item->isCarpet()) {
				CarpetBrush* cb = item->getCarpetBrush();
				if(cb) {
					brushCounts[cb->getName()]++;
				}
			}
		}
	}

	// Convert to entries
	for(const auto& pair : brushCounts) {
		IdentifiedBrushEntry entry;
		entry.originalBrushName = pair.first;
		entry.count = pair.second;
		entry.hasReplacement = false;
		identifiedBrushes.push_back(entry);
	}

	// Sort by count descending
	std::sort(identifiedBrushes.begin(), identifiedBrushes.end(), [](const IdentifiedBrushEntry& a, const IdentifiedBrushEntry& b) {
		return a.count > b.count;
	});

	RebuildRows();
}

void BatchReplacePanel::Clear()
{
	identifiedItems.clear();
	identifiedBrushes.clear();
	selectedIndex = -1;
	RebuildRows();
}

void BatchReplacePanel::SetReplacementForSelected(uint16_t itemId)
{
	if(!isItemMode || selectedIndex < 0 || selectedIndex >= (int)identifiedItems.size()) return;

	identifiedItems[selectedIndex].replacementId = itemId;
	identifiedItems[selectedIndex].hasReplacement = true;

	// Update the row display
	if(selectedIndex < (int)rows.size()) {
		rows[selectedIndex]->SetItemData(
			identifiedItems[selectedIndex].originalId,
			identifiedItems[selectedIndex].replacementId,
			identifiedItems[selectedIndex].count,
			identifiedItems[selectedIndex].hasReplacement
		);
	}

	UpdateStatus();

	// Auto-advance to next item without replacement
	for(int i = selectedIndex + 1; i < (int)identifiedItems.size(); ++i) {
		if(!identifiedItems[i].hasReplacement) {
			OnRowSelected(i);
			if(i < (int)rows.size()) {
				int scrollY = i * 70; // Approximate row height
				scrollPanel->Scroll(-1, scrollY / 10);
			}
			break;
		}
	}
}

void BatchReplacePanel::SetReplacementForSelected(Brush* brush)
{
	if(isItemMode || !brush || selectedIndex < 0 || selectedIndex >= (int)identifiedBrushes.size()) return;

	identifiedBrushes[selectedIndex].replacementBrushName = brush->getName();
	identifiedBrushes[selectedIndex].hasReplacement = true;

	// Update the row display
	if(selectedIndex < (int)rows.size()) {
		rows[selectedIndex]->SetBrushData(
			identifiedBrushes[selectedIndex].originalBrushName,
			identifiedBrushes[selectedIndex].replacementBrushName,
			identifiedBrushes[selectedIndex].count,
			identifiedBrushes[selectedIndex].hasReplacement
		);
	}

	UpdateStatus();

	// Auto-advance to next brush without replacement
	for(int i = selectedIndex + 1; i < (int)identifiedBrushes.size(); ++i) {
		if(!identifiedBrushes[i].hasReplacement) {
			OnRowSelected(i);
			if(i < (int)rows.size()) {
				int scrollY = i * 70;
				scrollPanel->Scroll(-1, scrollY / 10);
			}
			break;
		}
	}
}

void BatchReplacePanel::SetReplacementAtIndex(int index, uint16_t itemId)
{
	if(!isItemMode || index < 0 || index >= (int)identifiedItems.size()) return;

	identifiedItems[index].replacementId = itemId;
	identifiedItems[index].hasReplacement = true;

	// Update the row display
	if(index < (int)rows.size()) {
		rows[index]->SetItemData(
			identifiedItems[index].originalId,
			identifiedItems[index].replacementId,
			identifiedItems[index].count,
			identifiedItems[index].hasReplacement
		);
	}

	UpdateStatus();
}

void BatchReplacePanel::SetReplacementAtIndex(int index, const std::string& brushName)
{
	if(isItemMode || index < 0 || index >= (int)identifiedBrushes.size()) return;

	identifiedBrushes[index].replacementBrushName = brushName;
	identifiedBrushes[index].hasReplacement = true;

	// Update the row display
	if(index < (int)rows.size()) {
		rows[index]->SetBrushData(
			identifiedBrushes[index].originalBrushName,
			identifiedBrushes[index].replacementBrushName,
			identifiedBrushes[index].count,
			identifiedBrushes[index].hasReplacement
		);
	}

	UpdateStatus();
}

std::vector<ReplaceRule> BatchReplacePanel::GetRulesFromMappings() const
{
	std::vector<ReplaceRule> rules;

	if(isItemMode) {
		for(const IdentifiedItemEntry& entry : identifiedItems) {
			if(entry.hasReplacement && entry.replacementId != 0) {
				ReplaceRule rule;
				rule.kind = ReplaceRule::Kind::ItemId;
				rule.originalId = entry.originalId;
				rule.replacementIds.push_back(entry.replacementId);
				rules.push_back(rule);
			}
		}
	} else {
		for(const IdentifiedBrushEntry& entry : identifiedBrushes) {
			if(entry.hasReplacement && !entry.replacementBrushName.empty()) {
				ReplaceRule rule;
				rule.kind = ReplaceRule::Kind::Brush;
				rule.originalBrushName = entry.originalBrushName;
				rule.replacementBrushNames.push_back(entry.replacementBrushName);
				rules.push_back(rule);
			}
		}
	}

	return rules;
}

bool BatchReplacePanel::HasAnyMappings() const
{
	if(isItemMode) {
		for(const IdentifiedItemEntry& entry : identifiedItems) {
			if(entry.hasReplacement) return true;
		}
	} else {
		for(const IdentifiedBrushEntry& entry : identifiedBrushes) {
			if(entry.hasReplacement) return true;
		}
	}
	return false;
}

void BatchReplacePanel::SetItemClickCallback(std::function<void(uint16_t)> callback)
{
	itemClickCallback = callback;
}

void BatchReplacePanel::SetBrushClickCallback(std::function<void(Brush*)> callback)
{
	brushClickCallback = callback;
}

void BatchReplacePanel::SetReplacementClickCallback(std::function<void(int)> callback)
{
	replacementClickCallback = callback;
}

void BatchReplacePanel::SetGroupSimilarItems(bool group)
{
	groupSimilarItems = group;
	if(groupSimilarCheck) {
		groupSimilarCheck->SetValue(group);
	}
}

void BatchReplacePanel::OnRowSelected(int index)
{
	// Deselect previous
	if(selectedIndex >= 0 && selectedIndex < (int)rows.size()) {
		rows[selectedIndex]->SetSelected(false);
	}

	selectedIndex = index;
	clearReplacementBtn->Enable(true);

	// Select new
	if(selectedIndex >= 0 && selectedIndex < (int)rows.size()) {
		rows[selectedIndex]->SetSelected(true);
	}

	// Notify about selection for suggestions
	if(isItemMode && selectedIndex < (int)identifiedItems.size()) {
		if(itemClickCallback) {
			itemClickCallback(identifiedItems[selectedIndex].originalId);
		}
	} else if(!isItemMode && selectedIndex < (int)identifiedBrushes.size()) {
		if(brushClickCallback) {
			Brush* brush = g_brushes.getBrush(identifiedBrushes[selectedIndex].originalBrushName);
			if(brush && brushClickCallback) {
				brushClickCallback(brush);
			}
		}
	}
}

void BatchReplacePanel::OnClearReplacement(wxCommandEvent& event)
{
	if(selectedIndex < 0) return;

	if(isItemMode && selectedIndex < (int)identifiedItems.size()) {
		identifiedItems[selectedIndex].replacementId = 0;
		identifiedItems[selectedIndex].hasReplacement = false;
		if(selectedIndex < (int)rows.size()) {
			rows[selectedIndex]->SetItemData(
				identifiedItems[selectedIndex].originalId,
				0,
				identifiedItems[selectedIndex].count,
				false
			);
		}
	} else if(!isItemMode && selectedIndex < (int)identifiedBrushes.size()) {
		identifiedBrushes[selectedIndex].replacementBrushName.clear();
		identifiedBrushes[selectedIndex].hasReplacement = false;
		if(selectedIndex < (int)rows.size()) {
			rows[selectedIndex]->SetBrushData(
				identifiedBrushes[selectedIndex].originalBrushName,
				"",
				identifiedBrushes[selectedIndex].count,
				false
			);
		}
	}
	UpdateStatus();
}

void BatchReplacePanel::OnClearAllReplacements(wxCommandEvent& event)
{
	for(auto& entry : identifiedItems) {
		entry.replacementId = 0;
		entry.hasReplacement = false;
	}
	for(auto& entry : identifiedBrushes) {
		entry.replacementBrushName.clear();
		entry.hasReplacement = false;
	}

	// Update all rows
	for(size_t i = 0; i < rows.size(); ++i) {
		if(isItemMode && i < identifiedItems.size()) {
			rows[i]->SetItemData(
				identifiedItems[i].originalId,
				0,
				identifiedItems[i].count,
				false
			);
		} else if(!isItemMode && i < identifiedBrushes.size()) {
			rows[i]->SetBrushData(
				identifiedBrushes[i].originalBrushName,
				"",
				identifiedBrushes[i].count,
				false
			);
		}
	}
	UpdateStatus();
}

void BatchReplacePanel::OnReplacementAreaClicked(int index)
{
	// This is called when user clicks on the "Click to set" replacement area
	// Notify parent dialog to apply selected library item
	if(replacementClickCallback) {
		replacementClickCallback(index);
	}
}

void BatchReplacePanel::OnScrolled(wxScrollWinEvent& event)
{
	// Check if we need to load more rows when scrolling near bottom
	if(loadMoreTimer && !loadMoreTimer->IsRunning()) {
		loadMoreTimer->StartOnce(100); // Debounce
	}
	event.Skip();
}

void BatchReplacePanel::OnLoadMoreTimer(wxTimerEvent& event)
{
	LoadMoreRows();
}

void BatchReplacePanel::LoadMoreRows()
{
	// Lazy loading - load more rows as user scrolls
	int totalItems = isItemMode ? identifiedItems.size() : identifiedBrushes.size();
	if(loadedRowCount >= totalItems) return;

	int toLoad = std::min(loadedRowCount + ROWS_PER_BATCH, totalItems);

	for(int i = loadedRowCount; i < toLoad; ++i) {
		if(isItemMode) {
			const IdentifiedItemEntry& entry = identifiedItems[i];
			BatchReplaceRow* row = new BatchReplaceRow(scrollPanel, i, true);
			row->SetItemData(entry.originalId, entry.replacementId, entry.count, entry.hasReplacement);
			row->SetClickCallback([this](int idx) { OnRowSelected(idx); });
			row->SetReplacementClickCallback([this](int idx) { OnReplacementAreaClicked(idx); });
			row->SetItemDropCallback([this](int idx, uint16_t itemId) { SetReplacementAtIndex(idx, itemId); });
			rowsSizer->Add(row, 0, wxEXPAND | wxBOTTOM, 2);
			rows.push_back(row);
		} else {
			const IdentifiedBrushEntry& entry = identifiedBrushes[i];
			BatchReplaceRow* row = new BatchReplaceRow(scrollPanel, i, false);
			row->SetBrushData(entry.originalBrushName, entry.replacementBrushName, entry.count, entry.hasReplacement);
			row->SetClickCallback([this](int idx) { OnRowSelected(idx); });
			row->SetReplacementClickCallback([this](int idx) { OnReplacementAreaClicked(idx); });
			row->SetBrushDropCallback([this](int idx, const std::string& brushName) { SetReplacementAtIndex(idx, brushName); });
			rowsSizer->Add(row, 0, wxEXPAND | wxBOTTOM, 2);
			rows.push_back(row);
		}
	}

	loadedRowCount = toLoad;
	scrollPanel->FitInside();
	scrollPanel->Layout();
}

void BatchReplacePanel::RebuildRows()
{
	// Clear existing rows
	for(BatchReplaceRow* row : rows) {
		row->Destroy();
	}
	rows.clear();
	rowsSizer->Clear();
	loadedRowCount = 0;

	// Load first batch of rows
	int totalItems = isItemMode ? identifiedItems.size() : identifiedBrushes.size();
	int toLoad = std::min(ROWS_PER_BATCH, totalItems);

	for(int i = 0; i < toLoad; ++i) {
		if(isItemMode) {
			const IdentifiedItemEntry& entry = identifiedItems[i];
			BatchReplaceRow* row = new BatchReplaceRow(scrollPanel, i, true);
			row->SetItemData(entry.originalId, entry.replacementId, entry.count, entry.hasReplacement);
			row->SetClickCallback([this](int idx) { OnRowSelected(idx); });
			row->SetReplacementClickCallback([this](int idx) { OnReplacementAreaClicked(idx); });
			row->SetItemDropCallback([this](int idx, uint16_t itemId) { SetReplacementAtIndex(idx, itemId); });
			rowsSizer->Add(row, 0, wxEXPAND | wxBOTTOM, 2);
			rows.push_back(row);
		} else {
			const IdentifiedBrushEntry& entry = identifiedBrushes[i];
			BatchReplaceRow* row = new BatchReplaceRow(scrollPanel, i, false);
			row->SetBrushData(entry.originalBrushName, entry.replacementBrushName, entry.count, entry.hasReplacement);
			row->SetClickCallback([this](int idx) { OnRowSelected(idx); });
			row->SetReplacementClickCallback([this](int idx) { OnReplacementAreaClicked(idx); });
			row->SetBrushDropCallback([this](int idx, const std::string& brushName) { SetReplacementAtIndex(idx, brushName); });
			rowsSizer->Add(row, 0, wxEXPAND | wxBOTTOM, 2);
			rows.push_back(row);
		}
	}

	loadedRowCount = toLoad;
	scrollPanel->FitInside();
	scrollPanel->Layout();
	UpdateStatus();
}

void BatchReplacePanel::UpdateStatus()
{
	int totalItems = isItemMode ? identifiedItems.size() : identifiedBrushes.size();
	int mappedItems = 0;

	if(isItemMode) {
		for(const auto& entry : identifiedItems) {
			if(entry.hasReplacement) mappedItems++;
		}
	} else {
		for(const auto& entry : identifiedBrushes) {
			if(entry.hasReplacement) mappedItems++;
		}
	}

	if(totalItems == 0) {
		statusLabel->SetLabel("No items identified");
	} else {
		statusLabel->SetLabel(wxString::Format("%d/%d mapped", mappedItems, totalItems));
	}

	clearReplacementBtn->Enable(selectedIndex >= 0);
	clearAllBtn->Enable(totalItems > 0);
}

// ============================================================================
// AdvancedReplaceDialog implementation

AdvancedReplaceDialog::AdvancedReplaceDialog(wxWindow* parent) :
	wxDialog(parent, wxID_ANY, "Advanced Replace Tool", wxDefaultPosition, wxSize(1150, 750),
			 wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX)
{
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.background);

	wxBoxSizer* mainSizer = new wxBoxSizer(wxHORIZONTAL);

	// Left panel - Item Library
	libraryPanel = new ItemLibraryPanel(this);
	libraryPanel->SetMinSize(wxSize(280, -1));
	mainSizer->Add(libraryPanel, 0, wxALL | wxEXPAND, 5);

	// Center panel - Mode Notebook (Manual / Batch)
	wxBoxSizer* centerSizer = new wxBoxSizer(wxVERTICAL);

	modeNotebook = new wxNotebook(this, wxID_ANY);

	// === Tab 1: Manual Mode ===
	wxPanel* manualPanel = new wxPanel(modeNotebook, wxID_ANY);
	manualPanel->SetBackgroundColour(theme.surface);
	wxBoxSizer* manualSizer = new wxBoxSizer(wxVERTICAL);

	ruleBuilderPanel = new RuleBuilderPanel(manualPanel);
	manualSizer->Add(ruleBuilderPanel, 0, wxALL | wxEXPAND, 5);

	// Active rules list
	wxStaticText* rulesTitle = new wxStaticText(manualPanel, wxID_ANY, "Active Rules");
	rulesTitle->SetForegroundColour(theme.text);
	manualSizer->Add(rulesTitle, 0, wxLEFT | wxTOP, 10);

	activeRulesPanel = new ActiveRulesPanel(manualPanel);
	activeRulesPanel->SetMinSize(wxSize(-1, 150));
	manualSizer->Add(activeRulesPanel, 1, wxALL | wxEXPAND, 5);

	// Set up callbacks for editing rules
	activeRulesPanel->SetRuleClickCallback([this](int index) {
		// Optionally load the rule into the builder for editing
	});

	activeRulesPanel->SetOriginalSlotClickCallback([this](int ruleIndex) {
		// When clicking original slot, could allow editing
	});

	activeRulesPanel->SetReplacementSlotClickCallback([this](int ruleIndex, int replIndex) {
		// When clicking replacement slot, could allow editing
	});

	activeRulesPanel->SetItemDropCallback([this](int ruleIndex, uint16_t itemId, bool isOriginal) {
		if(ruleIndex < 0 || ruleIndex >= static_cast<int>(activeRules.size())) return;
		ReplaceRule& rule = activeRules[ruleIndex];

		if(rule.kind != ReplaceRule::Kind::ItemId) return;

		if(isOriginal) {
			rule.originalId = itemId;
		} else {
			// Replace first replacement item or add if empty
			if(rule.replacementIds.empty()) {
				rule.replacementIds.push_back(itemId);
			} else {
				rule.replacementIds[0] = itemId;
			}
		}
		activeRulesPanel->UpdateRule(ruleIndex, rule);
	});

	activeRulesPanel->SetBrushDropCallback([this](int ruleIndex, const std::string& brushName, bool isOriginal) {
		if(ruleIndex < 0 || ruleIndex >= static_cast<int>(activeRules.size())) return;
		ReplaceRule& rule = activeRules[ruleIndex];

		if(rule.kind != ReplaceRule::Kind::Brush) return;

		if(isOriginal) {
			rule.originalBrushName = brushName;
		} else {
			// Replace first replacement brush or add if empty
			if(rule.replacementBrushNames.empty()) {
				rule.replacementBrushNames.push_back(brushName);
			} else {
				rule.replacementBrushNames[0] = brushName;
			}
		}
		activeRulesPanel->UpdateRule(ruleIndex, rule);
	});

	// Manual mode buttons
	wxBoxSizer* manualBtnSizer = new wxBoxSizer(wxHORIZONTAL);
	addRuleBtn = new wxButton(manualPanel, wxID_ANY, "Add Rule");
	autoAddRuleCheck = new wxCheckBox(manualPanel, wxID_ANY, "Auto Add");
	autoAddRuleCheck->SetToolTip("Automatically add rule when Original and Replacement are set");
	clearRulesBtn = new wxButton(manualPanel, wxID_ANY, "Clear All");
	saveRuleSetBtn = new wxButton(manualPanel, wxID_ANY, "Save Rule Set");

	manualBtnSizer->Add(addRuleBtn, 0, wxRIGHT, 5);
	manualBtnSizer->Add(autoAddRuleCheck, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
	manualBtnSizer->Add(clearRulesBtn, 0, wxRIGHT, 5);
	manualBtnSizer->Add(saveRuleSetBtn, 0);
	manualSizer->Add(manualBtnSizer, 0, wxALL, 5);

	manualPanel->SetSizer(manualSizer);
	modeNotebook->AddPage(manualPanel, "Manual Rules");

	// === Tab 2: Batch Mode ===
	wxPanel* batchPanel = new wxPanel(modeNotebook, wxID_ANY);
	batchPanel->SetBackgroundColour(theme.surface);
	wxBoxSizer* batchSizer = new wxBoxSizer(wxVERTICAL);

	// Identify buttons
	wxBoxSizer* identifySizer = new wxBoxSizer(wxHORIZONTAL);
	identifyAllBtn = new wxButton(batchPanel, wxID_ANY, "Identify Items from Selection");
	identifyAllBtn->SetToolTip("Scan selected area and list all unique items");
	wxButton* identifyBrushesBtn = new wxButton(batchPanel, wxID_ANY, "Identify Brushes");
	identifyBrushesBtn->SetToolTip("Scan selected area and list all unique ground/carpet brushes");
	identifySizer->Add(identifyAllBtn, 0, wxRIGHT, 5);
	identifySizer->Add(identifyBrushesBtn, 0);
	batchSizer->Add(identifySizer, 0, wxALL, 5);

	batchReplacePanel = new BatchReplacePanel(batchPanel);
	batchSizer->Add(batchReplacePanel, 1, wxALL | wxEXPAND, 5);

	batchPanel->SetSizer(batchSizer);
	modeNotebook->AddPage(batchPanel, "Batch Replace");

	centerSizer->Add(modeNotebook, 1, wxALL | wxEXPAND, 5);

	// Scope options (shared between modes)
	wxStaticBoxSizer* scopeBox = new wxStaticBoxSizer(wxHORIZONTAL, this, "Scope");
	scopeEntireMap = new wxRadioButton(this, wxID_ANY, "Entire Map", wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
	scopeSelection = new wxRadioButton(this, wxID_ANY, "Selection");
	scopeScreen = new wxRadioButton(this, wxID_ANY, "Screen Area");
	lockSelectionCheck = new wxCheckBox(this, wxID_ANY, "Lock");
	lockSelectionCheck->Enable(false);

	scopeBox->Add(scopeEntireMap, 0, wxALL, 5);
	scopeBox->Add(scopeSelection, 0, wxALL, 5);
	scopeBox->Add(scopeScreen, 0, wxALL, 5);
	scopeBox->Add(lockSelectionCheck, 0, wxALL, 5);
	centerSizer->Add(scopeBox, 0, wxALL | wxEXPAND, 5);

	// Execute and Close buttons
	wxBoxSizer* actionSizer = new wxBoxSizer(wxHORIZONTAL);
	actionSizer->AddStretchSpacer();
	executeBtn = new wxButton(this, wxID_ANY, "Execute");
	closeBtn = new wxButton(this, wxID_ANY, "Close");
	actionSizer->Add(executeBtn, 0, wxRIGHT, 5);
	actionSizer->Add(closeBtn, 0);
	centerSizer->Add(actionSizer, 0, wxALL | wxEXPAND, 5);

	// Progress bar
	progressBar = new wxGauge(this, wxID_ANY, 100);
	centerSizer->Add(progressBar, 0, wxALL | wxEXPAND, 5);

	mainSizer->Add(centerSizer, 1, wxEXPAND);

	// Right panel - Suggestions + Saved Rules
	wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);

	suggestionsPanel = new SuggestionsPanel(this);
	suggestionsPanel->SetMinSize(wxSize(200, 250));
	rightSizer->Add(suggestionsPanel, 1, wxALL | wxEXPAND, 5);

	savedRulesPanel = new SavedRulesPanel(this);
	savedRulesPanel->SetMinSize(wxSize(200, 200));
	rightSizer->Add(savedRulesPanel, 1, wxALL | wxEXPAND, 5);

	mainSizer->Add(rightSizer, 0, wxEXPAND);

	SetSizer(mainSizer);
	Layout();
	Centre();

	// Bind events
	modeNotebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &AdvancedReplaceDialog::OnModeChanged, this);
	scopeEntireMap->Bind(wxEVT_RADIOBUTTON, &AdvancedReplaceDialog::OnScopeChanged, this);
	scopeSelection->Bind(wxEVT_RADIOBUTTON, &AdvancedReplaceDialog::OnScopeChanged, this);
	scopeScreen->Bind(wxEVT_RADIOBUTTON, &AdvancedReplaceDialog::OnScopeChanged, this);
	lockSelectionCheck->Bind(wxEVT_CHECKBOX, &AdvancedReplaceDialog::OnLockSelectionChanged, this);

	addRuleBtn->Bind(wxEVT_BUTTON, &AdvancedReplaceDialog::OnAddRuleClicked, this);
	clearRulesBtn->Bind(wxEVT_BUTTON, &AdvancedReplaceDialog::OnClearRulesClicked, this);
	saveRuleSetBtn->Bind(wxEVT_BUTTON, &AdvancedReplaceDialog::OnSaveRuleSetClicked, this);
	executeBtn->Bind(wxEVT_BUTTON, &AdvancedReplaceDialog::OnExecuteClicked, this);
	closeBtn->Bind(wxEVT_BUTTON, &AdvancedReplaceDialog::OnCloseClicked, this);
	identifyAllBtn->Bind(wxEVT_BUTTON, &AdvancedReplaceDialog::OnIdentifyAllClicked, this);
	identifyBrushesBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		Editor* editor = GetParentEditor();
		if(editor) {
			batchReplacePanel->IdentifyBrushesFromSelection(editor);
		} else {
			wxMessageBox("No editor available.", "Identify Brushes", wxOK | wxICON_WARNING);
		}
	});

	// Set up callbacks
	libraryPanel->SetItemSelectedCallback([this](uint16_t id) { OnLibraryItemSelected(id); });
	libraryPanel->SetBrushSelectedCallback([this](Brush* b) { OnLibraryBrushSelected(b); });
	suggestionsPanel->SetItemClickCallback([this](uint16_t id) { OnSuggestionItemClicked(id); });
	suggestionsPanel->SetBrushClickCallback([this](Brush* b) { OnSuggestionBrushClicked(b); });
	savedRulesPanel->SetRuleSelectedCallback([this](SavedRuleSet* rs) { OnSavedRuleSelected(rs); });
	batchReplacePanel->SetItemClickCallback([this](uint16_t id) { OnBatchItemSelected(id); });
	batchReplacePanel->SetBrushClickCallback([this](Brush* b) { OnBatchBrushSelected(b); });
	batchReplacePanel->SetReplacementClickCallback([this](int index) {
		// When user clicks "Click to set", apply selected library item
		if(libraryPanel->IsItemMode()) {
			uint16_t selectedId = libraryPanel->GetSelectedItemId();
			if(selectedId != 0) {
				batchReplacePanel->SetReplacementForSelected(selectedId);
				executeBtn->Enable(batchReplacePanel->HasAnyMappings());
			}
		} else {
			Brush* selectedBrush = libraryPanel->GetSelectedBrush();
			if(selectedBrush) {
				batchReplacePanel->SetReplacementForSelected(selectedBrush);
				executeBtn->Enable(batchReplacePanel->HasAnyMappings());
			}
		}
	});

	// Set up drop target callback for Rule Builder
	ruleBuilderPanel->SetDropCallback([this](bool isOriginal) { isDropTargetOriginal = isOriginal; });

	UpdateWidgets();
}

AdvancedReplaceDialog::~AdvancedReplaceDialog()
{
}

void AdvancedReplaceDialog::ApplyItemFromPalette(uint16_t itemId, bool toOriginal)
{
	if(toOriginal) {
		ruleBuilderPanel->SetOriginal(itemId);
		suggestionsPanel->UpdateSuggestions(itemId);
		// Auto-switch to Replacements after setting Original
		isDropTargetOriginal = false;
		ruleBuilderPanel->SetTargetOriginal(false);
	} else {
		ruleBuilderPanel->AddReplacement(itemId);
	}
	UpdateWidgets();
	TryAutoAddRule();
}

void AdvancedReplaceDialog::ApplyItemToOriginal(uint16_t itemId)
{
	ApplyItemFromPalette(itemId, true);
}

void AdvancedReplaceDialog::ApplyItemToReplacement(uint16_t itemId)
{
	ApplyItemFromPalette(itemId, false);
}

void AdvancedReplaceDialog::ApplyBrushToOriginal(Brush* brush)
{
	if(!brush) return;
	ruleBuilderPanel->SetOriginal(brush);
	suggestionsPanel->UpdateSuggestions(brush);
	// Auto-switch to Replacements after setting Original
	isDropTargetOriginal = false;
	ruleBuilderPanel->SetTargetOriginal(false);
	UpdateWidgets();
	TryAutoAddRule();
}

void AdvancedReplaceDialog::ApplyBrushToReplacement(Brush* brush)
{
	if(!brush) return;
	ruleBuilderPanel->AddReplacement(brush);
	UpdateWidgets();
	TryAutoAddRule();
}

void AdvancedReplaceDialog::OnScopeChanged(wxCommandEvent& event)
{
	if(scopeEntireMap->GetValue()) {
		currentScope = Scope::EntireMap;
		lockSelectionCheck->Enable(false);
		lockSelectionCheck->SetValue(false);
	} else if(scopeSelection->GetValue()) {
		currentScope = Scope::SelectionOnly;
		lockSelectionCheck->Enable(true);
		// Automatically capture selection when switching to Selection mode
		Editor* editor = GetParentEditor();
		if(editor) {
			CaptureSelectionSnapshot(editor);
			if(!selectionSnapshot.empty()) {
				lockSelectionCheck->SetLabel(wxString::Format("Lock (%d tiles)", (int)selectionSnapshot.size()));
			} else {
				lockSelectionCheck->SetLabel("Lock (no selection)");
			}
		}
	} else {
		currentScope = Scope::ScreenArea;
		lockSelectionCheck->Enable(false);
		lockSelectionCheck->SetValue(false);
	}
}

void AdvancedReplaceDialog::OnLockSelectionChanged(wxCommandEvent& event)
{
	if(lockSelectionCheck->IsChecked()) {
		// Capture current selection when lock is enabled
		Editor* editor = GetParentEditor();
		if(editor) {
			CaptureSelectionSnapshot(editor);
			if(!selectionSnapshot.empty()) {
				lockSelectionCheck->SetLabel(wxString::Format("Lock (%d tiles)", (int)selectionSnapshot.size()));
			} else {
				lockSelectionCheck->SetLabel("Lock (no selection)");
				lockSelectionCheck->SetValue(false);
				wxMessageBox("No tiles selected. Please select an area first.", "Lock Selection", wxOK | wxICON_WARNING);
			}
		}
	} else {
		lockSelectionCheck->SetLabel("Lock");
		selectionSnapshot.clear();
	}
}

void AdvancedReplaceDialog::OnAddRuleClicked(wxCommandEvent& event)
{
	if(!ruleBuilderPanel->HasValidRule()) return;

	ReplaceRule rule = ruleBuilderPanel->GetCurrentRule();
	activeRules.push_back(rule);
	activeRulesPanel->AddRule(rule);

	ruleBuilderPanel->ClearRule();
	suggestionsPanel->Clear();
	UpdateWidgets();
}

void AdvancedReplaceDialog::OnExecuteClicked(wxCommandEvent& event)
{
	if(isBatchMode) {
		ExecuteBatchReplace();
	} else {
		ExecuteRules();
	}
}

void AdvancedReplaceDialog::OnCloseClicked(wxCommandEvent& event)
{
	Close();
}

void AdvancedReplaceDialog::OnSaveRuleSetClicked(wxCommandEvent& event)
{
	if(activeRules.empty()) {
		wxMessageBox("No active rules to save.", "Save Rule Set", wxOK | wxICON_INFORMATION);
		return;
	}

	wxString name = wxGetTextFromUser("Enter a name for this rule set:", "Save Rule Set", "", this);
	if(name.IsEmpty()) return;

	SavedRuleSet ruleSet;
	ruleSet.name = name;
	ruleSet.rules = activeRules;
	savedRulesPanel->AddRuleSet(ruleSet);
}

void AdvancedReplaceDialog::OnClearRulesClicked(wxCommandEvent& event)
{
	activeRules.clear();
	activeRulesPanel->Clear();
	UpdateWidgets();
}

void AdvancedReplaceDialog::OnLibraryItemSelected(uint16_t itemId)
{
	if(isBatchMode) {
		// In batch mode, set replacement for selected item in batch list
		batchReplacePanel->SetReplacementForSelected(itemId);
		executeBtn->Enable(batchReplacePanel->HasAnyMappings());
		return;
	}

	if(isDropTargetOriginal) {
		ruleBuilderPanel->SetOriginal(itemId);
		suggestionsPanel->UpdateSuggestions(itemId);
		// Auto-switch to Replacements after setting Original
		isDropTargetOriginal = false;
		ruleBuilderPanel->SetTargetOriginal(false);
	} else {
		ruleBuilderPanel->AddReplacement(itemId);
	}
	UpdateWidgets();
	TryAutoAddRule();
}

void AdvancedReplaceDialog::OnLibraryBrushSelected(Brush* brush)
{
	if(isBatchMode) {
		// In batch mode, set replacement for selected brush in batch list
		batchReplacePanel->SetReplacementForSelected(brush);
		executeBtn->Enable(batchReplacePanel->HasAnyMappings());
		return;
	}

	if(isDropTargetOriginal) {
		ruleBuilderPanel->SetOriginal(brush);
		suggestionsPanel->UpdateSuggestions(brush);
		// Auto-switch to Replacements after setting Original
		isDropTargetOriginal = false;
		ruleBuilderPanel->SetTargetOriginal(false);
	} else {
		ruleBuilderPanel->AddReplacement(brush);
	}
	UpdateWidgets();
	TryAutoAddRule();
}

void AdvancedReplaceDialog::OnSuggestionItemClicked(uint16_t itemId)
{
	if(isBatchMode) {
		batchReplacePanel->SetReplacementForSelected(itemId);
		executeBtn->Enable(batchReplacePanel->HasAnyMappings());
		return;
	}

	ruleBuilderPanel->AddReplacement(itemId);
	UpdateWidgets();
	TryAutoAddRule();
}

void AdvancedReplaceDialog::OnSuggestionBrushClicked(Brush* brush)
{
	if(isBatchMode) {
		batchReplacePanel->SetReplacementForSelected(brush);
		executeBtn->Enable(batchReplacePanel->HasAnyMappings());
		return;
	}

	ruleBuilderPanel->AddReplacement(brush);
	UpdateWidgets();
	TryAutoAddRule();
}

void AdvancedReplaceDialog::OnSavedRuleSelected(SavedRuleSet* ruleSet)
{
	if(!ruleSet) return;

	// Load the rules into active rules
	activeRules = ruleSet->rules;
	activeRulesPanel->SetRules(activeRules);

	UpdateWidgets();
}

void AdvancedReplaceDialog::ExecuteRules()
{
	if(activeRules.empty()) return;

	Editor* editor = GetParentEditor();
	if(!editor) return;

	// Prepare positions based on scope
	std::vector<Position> positions;

	if(currentScope == Scope::SelectionOnly) {
		if(lockSelectionCheck->IsChecked() && !selectionSnapshot.empty()) {
			positions = selectionSnapshot;
		} else {
			CaptureSelectionSnapshot(editor);
			positions = selectionSnapshot;
		}
	} else if(currentScope == Scope::ScreenArea) {
		CaptureScreenArea(editor);
		positions = screenAreaPositions;
	}

	// Disable UI during execution
	addRuleBtn->Enable(false);
	executeBtn->Enable(false);
	closeBtn->Enable(false);
	progressBar->SetValue(0);

	int totalRules = activeRules.size();
	int completedRules = 0;

	for(const ReplaceRule& rule : activeRules) {
		if(rule.kind == ReplaceRule::Kind::ItemId) {
			// Find and replace items
			std::vector<std::pair<Tile*, Item*>> found;

			auto finder = [&](Map& map, Tile* tile, Item* item, long long) {
				if(item->getID() == rule.originalId) {
					found.push_back({tile, item});
				}
			};

			if(currentScope == Scope::EntireMap) {
				foreach_ItemOnMap(editor->getMap(), finder, false);
			} else {
				// Use positions
				for(const Position& pos : positions) {
					Tile* tile = editor->getMap().getTile(pos);
					if(!tile) continue;

					if(tile->ground && tile->ground->getID() == rule.originalId) {
						found.push_back({tile, tile->ground});
					}
					for(Item* item : tile->items) {
						if(item->getID() == rule.originalId) {
							found.push_back({tile, item});
						}
					}
				}
			}

			if(!found.empty()) {
				BatchAction* batch = editor->createBatch(ACTION_REPLACE_ITEMS);
				Action* action = editor->createAction(batch);

				for(auto& pair : found) {
					Tile* newTile = pair.first->deepCopy(editor->getMap());
					int index = pair.first->getIndexOf(pair.second);
					if(index != wxNOT_FOUND) {
						Item* item = newTile->getItemAt(index);
						if(item) {
							// Pick random replacement
							uint16_t newId = rule.replacementIds[rand() % rule.replacementIds.size()];
							transformItem(item, newId, newTile);
						}
					}
					action->addChange(new Change(newTile));
				}

				batch->addAndCommitAction(action);
				editor->addBatch(batch);
			}
		} else if(rule.kind == ReplaceRule::Kind::Brush) {
			// Find and replace ground or carpet brushes
			Brush* originalBrush = g_brushes.getBrush(rule.originalBrushName);
			if(!originalBrush) continue;

			bool isCarpetBrush = originalBrush->isCarpet();
			bool isGroundBrush = originalBrush->isGround();
			if(!isCarpetBrush && !isGroundBrush) continue;

			std::vector<Tile*> foundTiles;
			std::vector<Item*> foundCarpetItems; // For carpet brushes, store the carpet items

			if(isGroundBrush) {
				GroundBrush* groundBrush = static_cast<GroundBrush*>(originalBrush);

				auto finder = [&](Map& map, Tile* tile, Item* item, long long) {
					if(item && tile && item == tile->ground && item->getGroundBrush() == groundBrush) {
						foundTiles.push_back(tile);
					}
				};

				if(currentScope == Scope::EntireMap) {
					foreach_ItemOnMap(editor->getMap(), finder, false);
				} else {
					for(const Position& pos : positions) {
						Tile* tile = editor->getMap().getTile(pos);
						if(tile && tile->ground && tile->ground->getGroundBrush() == groundBrush) {
							foundTiles.push_back(tile);
						}
					}
				}
			} else if(isCarpetBrush) {
				// For carpet brushes, we need to find items that belong to this carpet brush
				CarpetBrush* carpetBrush = static_cast<CarpetBrush*>(originalBrush);

				auto finder = [&](Map& map, Tile* tile, Item* item, long long) {
					if(item && tile && item->isCarpet()) {
						CarpetBrush* itemCarpetBrush = item->getCarpetBrush();
						if(itemCarpetBrush == carpetBrush) {
							foundTiles.push_back(tile);
						}
					}
				};

				if(currentScope == Scope::EntireMap) {
					foreach_ItemOnMap(editor->getMap(), finder, false);
				} else {
					for(const Position& pos : positions) {
						Tile* tile = editor->getMap().getTile(pos);
						if(!tile) continue;
						for(Item* item : tile->items) {
							if(item && item->isCarpet()) {
								CarpetBrush* itemCarpetBrush = item->getCarpetBrush();
								if(itemCarpetBrush == carpetBrush) {
									foundTiles.push_back(tile);
									break;
								}
							}
						}
					}
				}
			}

			if(!foundTiles.empty()) {
				BatchAction* batch = editor->createBatch(ACTION_REPLACE_ITEMS);
				Action* action = editor->createAction(batch);

				for(Tile* tile : foundTiles) {
					Tile* newTile = tile->deepCopy(editor->getMap());

					// Pick random replacement brush
					const std::string& newBrushName = rule.replacementBrushNames[rand() % rule.replacementBrushNames.size()];
					Brush* newBrush = g_brushes.getBrush(newBrushName);

					if(isGroundBrush && newBrush && newBrush->isGround()) {
						if(newTile->ground) {
							delete newTile->ground;
							newTile->ground = nullptr;
						}
						GroundBrush* newGroundBrush = static_cast<GroundBrush*>(newBrush);
						GroundBrush::DrawParams params;
						params.paintSingleTile = true;
						newGroundBrush->draw(&editor->getMap(), newTile, &params);
						GroundBrush::doBorders(&editor->getMap(), newTile);
					} else if(isCarpetBrush && newBrush && newBrush->isCarpet()) {
						// Remove old carpet items from this brush
						CarpetBrush* oldCarpetBrush = static_cast<CarpetBrush*>(originalBrush);
						CarpetBrush* newCarpetBrush = static_cast<CarpetBrush*>(newBrush);

						// Find and remove carpet items belonging to the old brush
						for(auto it = newTile->items.begin(); it != newTile->items.end(); ) {
							Item* item = *it;
							if(item && item->isCarpet() && item->getCarpetBrush() == oldCarpetBrush) {
								delete item;
								it = newTile->items.erase(it);
							} else {
								++it;
							}
						}

						// Draw the new carpet
						newCarpetBrush->draw(&editor->getMap(), newTile, nullptr);
					}

					action->addChange(new Change(newTile));
				}

				batch->addAndCommitAction(action);
				editor->addBatch(batch);
			}
		}

		completedRules++;
		progressBar->SetValue((completedRules * 100) / totalRules);
	}

	editor->updateActions();

	// Re-enable UI
	addRuleBtn->Enable(true);
	executeBtn->Enable(true);
	closeBtn->Enable(true);

	// Refresh view
	if(MapTab* tab = dynamic_cast<MapTab*>(GetParent())) {
		tab->Refresh();
	}
}

void AdvancedReplaceDialog::UpdateWidgets()
{
	addRuleBtn->Enable(ruleBuilderPanel->HasValidRule());
	executeBtn->Enable(!activeRules.empty());
	clearRulesBtn->Enable(!activeRules.empty());
	saveRuleSetBtn->Enable(!activeRules.empty());
}

void AdvancedReplaceDialog::TryAutoAddRule()
{
	// Only auto-add if checkbox is checked
	if(!autoAddRuleCheck || !autoAddRuleCheck->IsChecked()) return;

	// Check if we have a valid rule (1 original and at least 1 replacement)
	if(!ruleBuilderPanel->HasValidRule()) return;

	ReplaceRule rule = ruleBuilderPanel->GetCurrentRule();

	// For item rules: need exactly 1 original and 1 replacement
	// For brush rules: need exactly 1 original and 1 replacement
	bool shouldAdd = false;
	if(rule.kind == ReplaceRule::Kind::ItemId) {
		shouldAdd = (rule.originalId != 0 && rule.replacementIds.size() == 1);
	} else {
		shouldAdd = (!rule.originalBrushName.empty() && rule.replacementBrushNames.size() == 1);
	}

	if(shouldAdd) {
		// Add the rule automatically
		activeRules.push_back(rule);
		activeRulesPanel->AddRule(rule);

		// Clear and reset for next rule
		ruleBuilderPanel->ClearRule();
		suggestionsPanel->Clear();

		// Reset target to Original for next rule
		isDropTargetOriginal = true;
		ruleBuilderPanel->SetTargetOriginal(true);

		UpdateWidgets();
	}
}

Editor* AdvancedReplaceDialog::GetParentEditor() const
{
	if(MapTab* tab = dynamic_cast<MapTab*>(GetParent())) {
		return tab->GetEditor();
	}
	return nullptr;
}

void AdvancedReplaceDialog::CaptureSelectionSnapshot(Editor* editor)
{
	selectionSnapshot.clear();
	if(!editor) return;

	const Selection& selection = editor->getSelection();
	selectionSnapshot.reserve(selection.size());
	for(Tile* tile : selection.getTiles()) {
		if(tile) {
			selectionSnapshot.push_back(tile->getPosition());
		}
	}
}

void AdvancedReplaceDialog::CaptureScreenArea(Editor* editor)
{
	screenAreaPositions.clear();
	if(!editor) return;

	MapTab* tab = dynamic_cast<MapTab*>(GetParent());
	if(!tab) return;

	MapCanvas* canvas = tab->GetCanvas();
	if(!canvas) return;

	// Get visible area (scroll positions and screen size)
	int viewScrollX, viewScrollY, screenSizeX, screenSizeY;
	canvas->GetViewBox(&viewScrollX, &viewScrollY, &screenSizeX, &screenSizeY);

	int floor = canvas->GetFloor();
	int zoom = canvas->GetZoom();
	int tileSize = rme::TileSize;
	if(zoom != 100) {
		tileSize = static_cast<int>(tileSize * (zoom / 100.0));
	}

	// Calculate map coordinates from screen coordinates
	int startX = viewScrollX / tileSize;
	int startY = viewScrollY / tileSize;
	int endX = (viewScrollX + screenSizeX) / tileSize + 1;
	int endY = (viewScrollY + screenSizeY) / tileSize + 1;

	for(int x = startX; x <= endX; ++x) {
		for(int y = startY; y <= endY; ++y) {
			screenAreaPositions.push_back(Position(x, y, floor));
		}
	}
}

void AdvancedReplaceDialog::OnIdentifyAllClicked(wxCommandEvent& event)
{
	Editor* editor = GetParentEditor();
	if(!editor) {
		wxMessageBox("No editor available.", "Identify Items", wxOK | wxICON_WARNING);
		return;
	}

	// Check if there's a selection
	const Selection& selection = editor->getSelection();
	if(selection.empty()) {
		wxMessageBox("Please select an area first.", "Identify Items", wxOK | wxICON_WARNING);
		return;
	}

	batchReplacePanel->IdentifyItemsFromSelection(editor);
}

void AdvancedReplaceDialog::OnModeChanged(wxBookCtrlEvent& event)
{
	isBatchMode = (modeNotebook->GetSelection() == 1);

	// Update execute button text based on mode
	if(isBatchMode) {
		executeBtn->SetLabel("Execute Batch");
		executeBtn->Enable(batchReplacePanel->HasAnyMappings());
	} else {
		executeBtn->SetLabel("Execute");
		executeBtn->Enable(!activeRules.empty());
	}

	event.Skip();
}

void AdvancedReplaceDialog::OnBatchItemSelected(uint16_t itemId)
{
	// Update suggestions panel when an item is selected in batch list
	suggestionsPanel->UpdateSuggestions(itemId);
}

void AdvancedReplaceDialog::OnBatchBrushSelected(Brush* brush)
{
	// Update suggestions panel when a brush is selected in batch list
	suggestionsPanel->UpdateSuggestions(brush);
}

void AdvancedReplaceDialog::ExecuteBatchReplace()
{
	if(!batchReplacePanel->HasAnyMappings()) return;

	Editor* editor = GetParentEditor();
	if(!editor) return;

	// Get rules from batch mappings
	std::vector<ReplaceRule> batchRules = batchReplacePanel->GetRulesFromMappings();
	if(batchRules.empty()) return;

	// Prepare positions based on scope
	std::vector<Position> positions;

	if(currentScope == Scope::SelectionOnly) {
		if(lockSelectionCheck->IsChecked() && !selectionSnapshot.empty()) {
			positions = selectionSnapshot;
		} else {
			CaptureSelectionSnapshot(editor);
			positions = selectionSnapshot;
		}
	} else if(currentScope == Scope::ScreenArea) {
		CaptureScreenArea(editor);
		positions = screenAreaPositions;
	}

	// Disable UI during execution
	executeBtn->Enable(false);
	closeBtn->Enable(false);
	progressBar->SetValue(0);

	int totalRules = batchRules.size();
	int completedRules = 0;

	for(const ReplaceRule& rule : batchRules) {
		if(rule.kind == ReplaceRule::Kind::ItemId) {
			// Find and replace items
			std::vector<std::pair<Tile*, Item*>> found;

			auto finder = [&](Map& map, Tile* tile, Item* item, long long) {
				if(item->getID() == rule.originalId) {
					found.push_back({tile, item});
				}
			};

			if(currentScope == Scope::EntireMap) {
				foreach_ItemOnMap(editor->getMap(), finder, false);
			} else {
				// Use positions
				for(const Position& pos : positions) {
					Tile* tile = editor->getMap().getTile(pos);
					if(!tile) continue;

					if(tile->ground && tile->ground->getID() == rule.originalId) {
						found.push_back({tile, tile->ground});
					}
					for(Item* item : tile->items) {
						if(item->getID() == rule.originalId) {
							found.push_back({tile, item});
						}
					}
				}
			}

			if(!found.empty()) {
				BatchAction* batch = editor->createBatch(ACTION_REPLACE_ITEMS);
				Action* action = editor->createAction(batch);

				for(auto& pair : found) {
					Tile* newTile = pair.first->deepCopy(editor->getMap());
					int index = pair.first->getIndexOf(pair.second);
					if(index != wxNOT_FOUND) {
						Item* item = newTile->getItemAt(index);
						if(item) {
							// Pick random replacement
							uint16_t newId = rule.replacementIds[rand() % rule.replacementIds.size()];
							transformItem(item, newId, newTile);
						}
					}
					action->addChange(new Change(newTile));
				}

				batch->addAndCommitAction(action);
				editor->addBatch(batch);
			}
		} else if(rule.kind == ReplaceRule::Kind::Brush) {
			// Find and replace ground or carpet brushes
			Brush* originalBrush = g_brushes.getBrush(rule.originalBrushName);
			if(!originalBrush) continue;

			bool isCarpetBrush = originalBrush->isCarpet();
			bool isGroundBrush = originalBrush->isGround();
			if(!isCarpetBrush && !isGroundBrush) continue;

			std::vector<Tile*> foundTiles;

			if(isGroundBrush) {
				GroundBrush* groundBrush = static_cast<GroundBrush*>(originalBrush);

				auto finder = [&](Map& map, Tile* tile, Item* item, long long) {
					if(item && tile && item == tile->ground && item->getGroundBrush() == groundBrush) {
						foundTiles.push_back(tile);
					}
				};

				if(currentScope == Scope::EntireMap) {
					foreach_ItemOnMap(editor->getMap(), finder, false);
				} else {
					for(const Position& pos : positions) {
						Tile* tile = editor->getMap().getTile(pos);
						if(tile && tile->ground && tile->ground->getGroundBrush() == groundBrush) {
							foundTiles.push_back(tile);
						}
					}
				}
			} else if(isCarpetBrush) {
				CarpetBrush* carpetBrush = static_cast<CarpetBrush*>(originalBrush);

				auto finder = [&](Map& map, Tile* tile, Item* item, long long) {
					if(item && tile && item->isCarpet()) {
						CarpetBrush* itemCarpetBrush = item->getCarpetBrush();
						if(itemCarpetBrush == carpetBrush) {
							foundTiles.push_back(tile);
						}
					}
				};

				if(currentScope == Scope::EntireMap) {
					foreach_ItemOnMap(editor->getMap(), finder, false);
				} else {
					for(const Position& pos : positions) {
						Tile* tile = editor->getMap().getTile(pos);
						if(!tile) continue;
						for(Item* item : tile->items) {
							if(item && item->isCarpet()) {
								CarpetBrush* itemCarpetBrush = item->getCarpetBrush();
								if(itemCarpetBrush == carpetBrush) {
									foundTiles.push_back(tile);
									break;
								}
							}
						}
					}
				}
			}

			if(!foundTiles.empty()) {
				BatchAction* batch = editor->createBatch(ACTION_REPLACE_ITEMS);
				Action* action = editor->createAction(batch);

				for(Tile* tile : foundTiles) {
					Tile* newTile = tile->deepCopy(editor->getMap());

					// Pick random replacement brush
					const std::string& newBrushName = rule.replacementBrushNames[rand() % rule.replacementBrushNames.size()];
					Brush* newBrush = g_brushes.getBrush(newBrushName);

					if(isGroundBrush && newBrush && newBrush->isGround()) {
						if(newTile->ground) {
							delete newTile->ground;
							newTile->ground = nullptr;
						}
						GroundBrush* newGroundBrush = static_cast<GroundBrush*>(newBrush);
						GroundBrush::DrawParams params;
						params.paintSingleTile = true;
						newGroundBrush->draw(&editor->getMap(), newTile, &params);
						GroundBrush::doBorders(&editor->getMap(), newTile);
					} else if(isCarpetBrush && newBrush && newBrush->isCarpet()) {
						CarpetBrush* oldCarpetBrush = static_cast<CarpetBrush*>(originalBrush);
						CarpetBrush* newCarpetBrush = static_cast<CarpetBrush*>(newBrush);

						for(auto it = newTile->items.begin(); it != newTile->items.end(); ) {
							Item* item = *it;
							if(item && item->isCarpet() && item->getCarpetBrush() == oldCarpetBrush) {
								delete item;
								it = newTile->items.erase(it);
							} else {
								++it;
							}
						}

						newCarpetBrush->draw(&editor->getMap(), newTile, nullptr);
					}

					action->addChange(new Change(newTile));
				}

				batch->addAndCommitAction(action);
				editor->addBatch(batch);
			}
		}

		completedRules++;
		progressBar->SetValue((completedRules * 100) / totalRules);
	}

	editor->updateActions();

	// Re-enable UI
	executeBtn->Enable(true);
	closeBtn->Enable(true);

	// Refresh view
	if(MapTab* tab = dynamic_cast<MapTab*>(GetParent())) {
		tab->Refresh();
	}

	wxMessageBox(wxString::Format("Replaced %d rule(s) successfully.", totalRules), "Batch Replace", wxOK | wxICON_INFORMATION);
}
