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

#ifndef RME_BRUSH_MANAGER_PANEL_H_
#define RME_BRUSH_MANAGER_PANEL_H_

#include "main.h"
#include "brush_enums.h"

class Brush;

// Filter status for brush list
enum BrushStatusFilter {
	BRUSH_STATUS_ALL,
	BRUSH_STATUS_ACTIVE,
	BRUSH_STATUS_INACTIVE,
};

// Virtual list box for efficient display of many brushes
class BrushManagerListBox : public wxVListBox {
public:
	BrushManagerListBox(wxWindow* parent, wxWindowID id = wxID_ANY);
	virtual ~BrushManagerListBox();

	// Set the brushes to display
	void SetBrushes(const std::vector<Brush*>& brushes);

	// Get the brushes being displayed
	const std::vector<Brush*>& GetBrushes() const { return brushes; }

	// Get the brush at a specific index
	Brush* GetBrush(size_t index) const;

	// Get all selected brushes
	std::vector<Brush*> GetSelectedBrushes() const;

	// Toggle selection state of all visible items
	void SelectAll();
	void DeselectAll();

	// Refresh the list display
	void RefreshList();

protected:
	// wxVListBox overrides
	virtual void OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const override;
	virtual wxCoord OnMeasureItem(size_t n) const override;
	virtual void OnDrawBackground(wxDC& dc, const wxRect& rect, size_t n) const override;

	// Event handlers
	void OnMouseDown(wxMouseEvent& event);
	void OnMouseDoubleClick(wxMouseEvent& event);
	void OnKeyDown(wxKeyEvent& event);

private:
	std::vector<Brush*> brushes;
	int item_height;
	int icon_size;

	DECLARE_EVENT_TABLE()
};

// Main panel for managing brushes
class BrushManagerPanel : public wxPanel {
public:
	BrushManagerPanel(wxWindow* parent, wxWindowID id = wxID_ANY);
	virtual ~BrushManagerPanel();

	// Refresh the brush list based on current filters
	void RefreshBrushList();

	// Reload all brushes from XML files
	void ReloadBrushes();

	// Get/Set auto-reload state
	bool IsAutoReloadEnabled() const;
	void SetAutoReloadEnabled(bool enabled);

	// Filter accessors
	BrushType GetTypeFilter() const;
	BrushStatusFilter GetStatusFilter() const;
	wxString GetSearchFilter() const;

	// Selection operations
	void ToggleSelectedBrushes();
	void SelectAllBrushes();
	void DeselectAllBrushes();

	// Called when a brush activation is toggled (from listbox or buttons)
	// Handles auto-reload if enabled
	void OnBrushActivationToggled();

protected:
	// Initialize UI components
	void CreateControls();
	void CreateFilterBar();
	void CreateBrushList();
	void CreateButtonBar();

	// Apply current filters to brush list
	void ApplyFilters();

	// Get brush type name for display
	static wxString GetBrushTypeName(BrushType type);

	// Event handlers
	void OnTypeFilterChanged(wxCommandEvent& event);
	void OnStatusFilterChanged(wxCommandEvent& event);
	void OnSearchChanged(wxCommandEvent& event);
	void OnToggleSelected(wxCommandEvent& event);
	void OnReloadBrushes(wxCommandEvent& event);
	void OnAutoReloadCheckbox(wxCommandEvent& event);

private:
	// Filter controls
	wxComboBox* type_filter;
	wxComboBox* status_filter;
	wxTextCtrl* search_box;

	// Brush list
	BrushManagerListBox* brush_list;

	// Action buttons
	wxButton* toggle_button;
	wxButton* reload_button;
	wxCheckBox* auto_reload_checkbox;

	// Filtered brush data
	std::vector<Brush*> filtered_brushes;
	std::vector<Brush*> all_brushes;

	DECLARE_EVENT_TABLE()
};

#endif // RME_BRUSH_MANAGER_PANEL_H_
