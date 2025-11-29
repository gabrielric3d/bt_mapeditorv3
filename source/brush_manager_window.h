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

#ifndef RME_BRUSH_MANAGER_WINDOW_H_
#define RME_BRUSH_MANAGER_WINDOW_H_

#include "main.h"

#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/dataview.h>
#include <wx/variant.h>
#include <wx/combobox.h>

class BrushSpritePanel;

class BrushManagerDialog : public wxDialog
{
public:
	explicit BrushManagerDialog(wxWindow* parent);

private:
	struct BrushDescriptor
	{
		wxString name;
		uint32_t serverId;
		uint32_t spriteId;
		wxString type;
		wxString fileLabel;
		bool enabled;
		pugi::xml_node node;
	};

	struct BrushFileEntry
	{
		wxString displayName;
		FileName path;
		bool exists;
		std::shared_ptr<pugi::xml_document> document;
		std::vector<BrushDescriptor> brushes;
	};

	void Reload();
	void PopulateList();
	void UpdateSelectionUi();
	long GetSelection() const;
	void OpenSelectedFile();
	void LoadSelectedFileContent();
	void SetStatus(const wxString& text);
	void UpdateSaveButton();
	void ShowBrushDetails(const BrushDescriptor* descriptor);
	void SaveSelectedFile();
	void ReloadBrushData();
	void PopulateBrushList(const BrushFileEntry& entry);
	void ParseBrushes(BrushFileEntry& entry);
	void OnBrushSelection(wxDataViewEvent& event);
	void OnBrushValueChanged(wxDataViewEvent& event);
	void OnBrushFilterChanged(wxCommandEvent& event);
	void SetBrushEnabled(BrushFileEntry& entry, size_t brushIndex, bool enabled);
	void RefreshBrushPreview(const BrushDescriptor* descriptor);

	void OnOpenFile(wxCommandEvent& event);
	void OnSaveFile(wxCommandEvent& event);
	void OnRefresh(wxCommandEvent& event);
	void OnListActivated(wxListEvent& event);
	void OnSelectionChanged(wxListEvent& event);
	void OnEditorText(wxCommandEvent& event);

	std::vector<BrushFileEntry> entries;
	wxListCtrl* listCtrl;
	wxDataViewListCtrl* brushListCtrl;
	wxComboBox* brushFilterCtrl;
	BrushSpritePanel* spritePreview;
	wxStaticText* statusLabel;
	wxButton* openButton;
	wxButton* saveButton;
	wxTextCtrl* editorCtrl;
	wxPanel* xmlContentPanel;
	wxStaticText* xmlContentsLabel;
	wxString brushFilter;
	std::vector<size_t> displayedBrushes;
	BrushFileEntry* currentEntry;
	long currentSelection;
};

#endif
