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

#ifndef RME_ADVANCED_REPLACE_WINDOW_H_
#define RME_ADVANCED_REPLACE_WINDOW_H_

#include <wx/dialog.h>
#include <wx/panel.h>
#include <wx/string.h>
#include <wx/srchctrl.h>
#include <wx/notebook.h>
#include <wx/listctrl.h>
#include <wx/dnd.h>
#include <wx/gauge.h>
#include <wx/stattext.h>
#include <wx/radiobut.h>
#include <wx/checkbox.h>
#include <wx/listbox.h>
#include <wx/textctrl.h>
#include <wx/scrolwin.h>
#include <wx/button.h>
#include <vector>
#include <string>
#include <functional>
#include <cstdint>

#include "position.h"

class DCButton;
class GroundBrush;
class Brush;
class ItemType;
class Editor;

// ============================================================================
// Replace Rule - represents a single replacement rule
struct ReplaceRule
{
	enum class Kind {
		ItemId,
		Brush,
	};

	Kind kind = Kind::ItemId;

	// For ItemId kind
	uint16_t originalId = 0;
	std::vector<uint16_t> replacementIds;

	// For Brush kind
	std::string originalBrushName;
	std::vector<std::string> replacementBrushNames;

	bool isValid() const;
	wxString getDisplayName() const;
};

// ============================================================================
// Saved Rule Set - a named collection of rules
struct SavedRuleSet
{
	wxString name;
	std::vector<ReplaceRule> rules;
};

// ============================================================================
// Item Library Panel - shows items/brushes with search
class ItemLibraryPanel : public wxPanel
{
public:
	ItemLibraryPanel(wxWindow* parent);

	void SetSearchFilter(const wxString& filter);
	void RefreshItems();

	// Get currently selected item/brush
	uint16_t GetSelectedItemId() const;
	Brush* GetSelectedBrush() const;
	bool IsItemMode() const;

	// Callbacks for item selection
	void SetItemSelectedCallback(std::function<void(uint16_t)> callback);
	void SetBrushSelectedCallback(std::function<void(Brush*)> callback);

private:
	void OnTabChanged(wxBookCtrlEvent& event);
	void OnSearchChanged(wxCommandEvent& event);
	void OnItemSelected(wxMouseEvent& event);
	void OnItemDoubleClicked(wxMouseEvent& event);
	void OnItemDragStart(wxMouseEvent& event);
	void OnPageChanged(wxCommandEvent& event);

	void PopulateItems();
	void PopulateBrushes();
	void UpdatePagination();

	wxNotebook* notebook;
	wxSearchCtrl* searchCtrl;
	wxScrolledWindow* itemsPanel;
	wxScrolledWindow* brushesPanel;
	wxButton* prevPageBtn;
	wxButton* nextPageBtn;
	wxStaticText* pageLabel;

	wxString currentFilter;
	int currentPage = 0;
	static constexpr int ITEMS_PER_PAGE = 32;

	std::vector<uint16_t> filteredItems;
	std::vector<Brush*> filteredBrushes;

	uint16_t selectedItemId = 0;
	Brush* selectedBrush = nullptr;

	std::function<void(uint16_t)> itemSelectedCallback;
	std::function<void(Brush*)> brushSelectedCallback;

	wxPoint dragStartPos;
	bool isDragging = false;
};

// ============================================================================
// Rule Builder Panel - shows current rule being built
class RuleBuilderPanel : public wxPanel
{
public:
	RuleBuilderPanel(wxWindow* parent);

	void SetOriginal(uint16_t itemId);
	void SetOriginal(Brush* brush);
	void AddReplacement(uint16_t itemId);
	void AddReplacement(Brush* brush);
	void ClearRule();

	ReplaceRule GetCurrentRule() const;
	bool HasValidRule() const;

	void SetDropCallback(std::function<void(bool isOriginal)> callback);
	void SetTargetOriginal(bool isOriginal);
	bool IsTargetOriginal() const { return isTargetOriginal; }

private:
	void OnOriginalClicked(wxMouseEvent& event);
	void OnReplacementClicked(wxMouseEvent& event);
	void OnClearClicked(wxCommandEvent& event);
	void UpdateDisplay();
	void UpdateSelectionHighlight();

	wxPanel* originalDropZone;
	wxPanel* replacementsDropZone;
	wxScrolledWindow* replacementsScroll;
	wxButton* clearBtn;
	wxStaticText* origLabel;
	wxStaticText* replLabel;
	bool isTargetOriginal = true;

	ReplaceRule currentRule;
	std::function<void(bool isOriginal)> dropCallback;

	std::vector<DCButton*> replacementButtons;
	DCButton* originalButton = nullptr;
};

// ============================================================================
// Suggestions Panel - shows items with similar names
class SuggestionsPanel : public wxPanel
{
public:
	SuggestionsPanel(wxWindow* parent);

	void UpdateSuggestions(uint16_t itemId);
	void UpdateSuggestions(Brush* brush);
	void Clear();

	void SetItemClickCallback(std::function<void(uint16_t)> callback);
	void SetBrushClickCallback(std::function<void(Brush*)> callback);

private:
	void OnItemClicked(wxMouseEvent& event);
	void PopulateSuggestions();

	wxScrolledWindow* scrollPanel;
	wxStaticText* titleLabel;

	std::vector<uint16_t> suggestedItems;
	std::vector<Brush*> suggestedBrushes;
	bool isItemMode = true;

	std::function<void(uint16_t)> itemClickCallback;
	std::function<void(Brush*)> brushClickCallback;
};

// ============================================================================
// Saved Rules Panel - shows saved rule sets
class SavedRulesPanel : public wxPanel
{
public:
	SavedRulesPanel(wxWindow* parent);

	void LoadRules();
	void SaveRules();
	void AddRuleSet(const SavedRuleSet& ruleSet);
	void RemoveSelectedRuleSet();

	SavedRuleSet* GetSelectedRuleSet();
	void SetRuleSelectedCallback(std::function<void(SavedRuleSet*)> callback);

private:
	void OnRuleSelected(wxCommandEvent& event);
	void OnDeleteClicked(wxCommandEvent& event);
	void OnRenameClicked(wxCommandEvent& event);

	wxListBox* rulesList;
	wxButton* deleteBtn;
	wxButton* renameBtn;
	wxTextCtrl* nameInput;
	wxButton* saveBtn;

	std::vector<SavedRuleSet> savedRules;
	std::function<void(SavedRuleSet*)> ruleSelectedCallback;

	static wxString GetRulesFilePath();
};

// ============================================================================
// Forward declaration
class ActiveRuleRow;

// ============================================================================
// Drop target for active rule row item slots
class ActiveRuleDropTarget : public wxTextDropTarget
{
public:
	ActiveRuleDropTarget(ActiveRuleRow* row, bool isOriginalSlot);
	virtual bool OnDropText(wxCoord x, wxCoord y, const wxString& data) override;

private:
	ActiveRuleRow* m_row;
	bool m_isOriginalSlot;
};

// ============================================================================
// Active Rule Row - displays a single rule with visual icons
class ActiveRuleRow : public wxPanel
{
public:
	ActiveRuleRow(wxWindow* parent, int index, const ReplaceRule& rule);

	void SetRule(const ReplaceRule& rule);
	const ReplaceRule& GetRule() const { return rule; }
	int GetIndex() const { return rowIndex; }

	void SetSelected(bool selected);
	bool IsSelected() const { return isSelected; }

	void SetClickCallback(std::function<void(int)> callback);
	void SetOriginalSlotClickCallback(std::function<void(int)> callback);
	void SetReplacementSlotClickCallback(std::function<void(int, int)> callback);
	void SetItemDropCallback(std::function<void(int, uint16_t, bool)> callback);
	void SetBrushDropCallback(std::function<void(int, const std::string&, bool)> callback);

	// Called by drop target
	void OnItemDropped(uint16_t itemId, bool isOriginalSlot);
	void OnBrushDropped(const std::string& brushName, bool isOriginalSlot);

private:
	void OnClick(wxMouseEvent& event);
	void OnOriginalSlotClick(wxMouseEvent& event);
	void OnReplacementSlotClick(wxMouseEvent& event, int replIndex);
	void UpdateDisplay();

	int rowIndex;
	ReplaceRule rule;
	bool isSelected = false;

	DCButton* originalBtn;
	wxStaticText* originalLabel;
	wxStaticText* arrowLabel;
	std::vector<DCButton*> replacementBtns;
	std::vector<wxStaticText*> replacementLabels;
	wxBoxSizer* replacementsSizer;

	std::function<void(int)> clickCallback;
	std::function<void(int)> originalSlotClickCallback;
	std::function<void(int, int)> replacementSlotClickCallback;
	std::function<void(int, uint16_t, bool)> itemDropCallback;
	std::function<void(int, const std::string&, bool)> brushDropCallback;
};

// ============================================================================
// Active Rules Panel - displays active rules with visual icons
class ActiveRulesPanel : public wxPanel
{
public:
	ActiveRulesPanel(wxWindow* parent);

	void AddRule(const ReplaceRule& rule);
	void RemoveRule(int index);
	void UpdateRule(int index, const ReplaceRule& rule);
	void Clear();
	void SetRules(const std::vector<ReplaceRule>& rules);

	int GetRuleCount() const { return static_cast<int>(rules.size()); }
	const std::vector<ReplaceRule>& GetRules() const { return rules; }
	int GetSelectedIndex() const { return selectedIndex; }
	ReplaceRule* GetSelectedRule();

	void SetRuleClickCallback(std::function<void(int)> callback);
	void SetOriginalSlotClickCallback(std::function<void(int)> callback);
	void SetReplacementSlotClickCallback(std::function<void(int, int)> callback);
	void SetItemDropCallback(std::function<void(int, uint16_t, bool)> callback);
	void SetBrushDropCallback(std::function<void(int, const std::string&, bool)> callback);

private:
	void OnRowSelected(int index);
	void RebuildRows();

	wxScrolledWindow* scrollPanel;
	wxBoxSizer* rowsSizer;

	std::vector<ActiveRuleRow*> rows;
	std::vector<ReplaceRule> rules;
	int selectedIndex = -1;

	std::function<void(int)> ruleClickCallback;
	std::function<void(int)> originalSlotClickCallback;
	std::function<void(int, int)> replacementSlotClickCallback;
	std::function<void(int, uint16_t, bool)> itemDropCallback;
	std::function<void(int, const std::string&, bool)> brushDropCallback;
};

// ============================================================================
// Identified Item Entry - represents an item found in selection with its replacement
struct IdentifiedItemEntry
{
	uint16_t originalId = 0;
	uint16_t replacementId = 0;
	int count = 0; // How many times this item appears in selection
	bool hasReplacement = false;
};

// ============================================================================
// Identified Brush Entry - represents a brush found in selection with its replacement
struct IdentifiedBrushEntry
{
	std::string originalBrushName;
	std::string replacementBrushName;
	int count = 0;
	bool hasReplacement = false;
};

// ============================================================================
// Forward declaration
class BatchReplaceRow;

// ============================================================================
// Drop target for batch replace row replacement button
class BatchReplaceDropTarget : public wxTextDropTarget
{
public:
	BatchReplaceDropTarget(BatchReplaceRow* row);
	virtual bool OnDropText(wxCoord x, wxCoord y, const wxString& data) override;

private:
	BatchReplaceRow* m_row;
};

// ============================================================================
// Batch Replace Row - a single row in the batch replace panel with visual preview
class BatchReplaceRow : public wxPanel
{
public:
	BatchReplaceRow(wxWindow* parent, int index, bool isItemMode);

	void SetItemData(uint16_t originalId, uint16_t replacementId, int count, bool hasReplacement);
	void SetBrushData(const std::string& originalBrush, const std::string& replacementBrush, int count, bool hasReplacement);
	void SetSelected(bool selected);
	bool IsSelected() const { return isSelected; }
	int GetIndex() const { return rowIndex; }
	bool IsItemMode() const { return isItemMode; }

	void SetClickCallback(std::function<void(int)> callback);
	void SetReplacementClickCallback(std::function<void(int)> callback);
	void SetItemDropCallback(std::function<void(int, uint16_t)> callback);
	void SetBrushDropCallback(std::function<void(int, const std::string&)> callback);

	// Called by drop target
	void OnItemDropped(uint16_t itemId);
	void OnBrushDropped(const std::string& brushName);

private:
	void OnClick(wxMouseEvent& event);
	void OnReplacementClick(wxMouseEvent& event);
	void UpdateDisplay();

	int rowIndex;
	bool isItemMode;
	bool isSelected = false;
	bool hasReplacement = false;

	uint16_t originalItemId = 0;
	uint16_t replacementItemId = 0;
	std::string originalBrushName;
	std::string replacementBrushName;
	int itemCount = 0;

	wxPanel* rowPanel;
	DCButton* originalBtn;
	DCButton* replacementBtn;
	wxStaticText* originalLabel;
	wxStaticText* replacementLabel;
	wxStaticText* countLabel;
	wxStaticText* arrowLabel;

	std::function<void(int)> clickCallback;
	std::function<void(int)> replacementClickCallback;
	std::function<void(int, uint16_t)> itemDropCallback;
	std::function<void(int, const std::string&)> brushDropCallback;
};

// ============================================================================
// Batch Replace Panel - shows identified items in two columns (Original | Replacement)
class BatchReplacePanel : public wxPanel
{
public:
	BatchReplacePanel(wxWindow* parent);

	void IdentifyItemsFromSelection(Editor* editor);
	void IdentifyBrushesFromSelection(Editor* editor);
	void Clear();

	void SetReplacementForSelected(uint16_t itemId);
	void SetReplacementForSelected(Brush* brush);
	void SetReplacementAtIndex(int index, uint16_t itemId);
	void SetReplacementAtIndex(int index, const std::string& brushName);

	std::vector<ReplaceRule> GetRulesFromMappings() const;
	bool HasAnyMappings() const;

	void SetItemClickCallback(std::function<void(uint16_t)> callback);
	void SetBrushClickCallback(std::function<void(Brush*)> callback);

	void SetGroupSimilarItems(bool group);
	bool IsGroupingSimilarItems() const { return groupSimilarItems; }

	void SetReplacementClickCallback(std::function<void(int)> callback);

private:
	void OnRowSelected(int index);
	void OnReplacementAreaClicked(int index);
	void OnClearReplacement(wxCommandEvent& event);
	void OnClearAllReplacements(wxCommandEvent& event);
	void OnScrolled(wxScrollWinEvent& event);
	void OnLoadMoreTimer(wxTimerEvent& event);
	void RebuildRows();
	void LoadMoreRows();
	void UpdateStatus();

	wxScrolledWindow* scrollPanel;
	wxBoxSizer* rowsSizer;
	wxButton* clearReplacementBtn;
	wxButton* clearAllBtn;
	wxStaticText* statusLabel;
	wxCheckBox* groupSimilarCheck;
	wxTimer* loadMoreTimer;

	std::vector<BatchReplaceRow*> rows;
	std::vector<IdentifiedItemEntry> identifiedItems;
	std::vector<IdentifiedBrushEntry> identifiedBrushes;
	bool isItemMode = true;
	int selectedIndex = -1;
	bool groupSimilarItems = true;
	int loadedRowCount = 0;
	static constexpr int ROWS_PER_BATCH = 20;

	std::function<void(uint16_t)> itemClickCallback;
	std::function<void(Brush*)> brushClickCallback;
	std::function<void(int)> replacementClickCallback;
};

// ============================================================================
// Advanced Replace Dialog - main dialog
class AdvancedReplaceDialog : public wxDialog
{
public:
	enum class Scope {
		EntireMap,
		SelectionOnly,
		ScreenArea,
	};

	AdvancedReplaceDialog(wxWindow* parent);
	~AdvancedReplaceDialog();

	void ApplyItemFromPalette(uint16_t itemId, bool toOriginal);
	void ApplyItemToOriginal(uint16_t itemId);
	void ApplyItemToReplacement(uint16_t itemId);
	void ApplyBrushToOriginal(Brush* brush);
	void ApplyBrushToReplacement(Brush* brush);

private:
	void OnScopeChanged(wxCommandEvent& event);
	void OnLockSelectionChanged(wxCommandEvent& event);
	void OnAddRuleClicked(wxCommandEvent& event);
	void OnExecuteClicked(wxCommandEvent& event);
	void OnCloseClicked(wxCommandEvent& event);
	void OnSaveRuleSetClicked(wxCommandEvent& event);
	void OnClearRulesClicked(wxCommandEvent& event);
	void OnIdentifyAllClicked(wxCommandEvent& event);
	void OnModeChanged(wxBookCtrlEvent& event);

	void OnLibraryItemSelected(uint16_t itemId);
	void OnLibraryBrushSelected(Brush* brush);
	void OnSuggestionItemClicked(uint16_t itemId);
	void OnSuggestionBrushClicked(Brush* brush);
	void OnSavedRuleSelected(SavedRuleSet* ruleSet);
	void OnBatchItemSelected(uint16_t itemId);
	void OnBatchBrushSelected(Brush* brush);

	void ExecuteRules();
	void ExecuteBatchReplace();
	void UpdateWidgets();
	void TryAutoAddRule();

	Editor* GetParentEditor() const;
	void CaptureSelectionSnapshot(Editor* editor);
	void CaptureScreenArea(Editor* editor);

	// Mode notebook (Manual / Batch)
	wxNotebook* modeNotebook;

	// Panels
	ItemLibraryPanel* libraryPanel;
	RuleBuilderPanel* ruleBuilderPanel;
	SuggestionsPanel* suggestionsPanel;
	SavedRulesPanel* savedRulesPanel;
	BatchReplacePanel* batchReplacePanel;

	// Rules list
	ActiveRulesPanel* activeRulesPanel;
	std::vector<ReplaceRule> activeRules;

	// Scope controls
	wxRadioButton* scopeEntireMap;
	wxRadioButton* scopeSelection;
	wxRadioButton* scopeScreen;
	wxCheckBox* lockSelectionCheck;

	// Action buttons
	wxButton* addRuleBtn;
	wxButton* executeBtn;
	wxButton* clearRulesBtn;
	wxButton* saveRuleSetBtn;
	wxButton* closeBtn;
	wxCheckBox* autoAddRuleCheck;
	wxButton* identifyAllBtn;

	// Progress
	wxGauge* progressBar;

	// State
	Scope currentScope = Scope::EntireMap;
	std::vector<Position> selectionSnapshot;
	std::vector<Position> screenAreaPositions;

	bool isDropTargetOriginal = true;
	bool isBatchMode = false;
};

#endif // RME_ADVANCED_REPLACE_WINDOW_H_
