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

#ifndef RME_STRUCTURE_MANAGER_WINDOW_H_
#define RME_STRUCTURE_MANAGER_WINDOW_H_

#include <wx/dialog.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/listbox.h>
#include <wx/panel.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/treectrl.h>

#include <vector>

class StructureManagerDialog : public wxDialog
{
public:
	explicit StructureManagerDialog(wxWindow* parent);
	~StructureManagerDialog() override;
	static bool HandleGlobalHotkey(wxKeyEvent& event);

private:
	struct StructureEntry {
		wxString name;
		wxString path;
		wxString category;
	};

	void CreateControls();
	void LoadStructures();
	void BuildCategoryTree();
	void RefreshItemList();
	void UpdateSelectionUi();
	void SetStatusText(const wxString& text);
	const StructureEntry* GetSelectedEntry(int* outIndex) const;
	void SelectCategoryByPath(const wxString& path);
	void SelectEntryByName(const wxString& name);
	void StartPasteFromEntry(const StructureEntry& entry);
	void RenameSelectedCategory();
	void SelectAdjacentEntry(int delta);
	bool HandleGlobalHotkeyInternal(wxKeyEvent& event);
	void AddCategory(bool asChild);

	void OnSaveSelection(wxCommandEvent& event);
	void OnPaste(wxCommandEvent& event);
	void OnDelete(wxCommandEvent& event);
	void OnSelectionChanged(wxCommandEvent& event);
	void OnListKeyDown(wxKeyEvent& event);
	void OnRenameCategory(wxCommandEvent& event);
	void OnAddCategory(wxCommandEvent& event);
	void OnAddSubcategory(wxCommandEvent& event);
	void OnCategoryChanged(wxTreeEvent& event);
	void OnSearchChanged(wxCommandEvent& event);
	void OnCharHook(wxKeyEvent& event);
	void OnClose(wxCloseEvent& event);

	std::vector<StructureEntry> m_entries;
	std::vector<const StructureEntry*> m_listEntries;
	std::vector<wxString> m_categoryPaths;
	wxString m_baseDir;
	wxString m_currentCategoryPath;
	wxTreeCtrl* m_categoryTree = nullptr;
	wxTextCtrl* m_searchCtrl = nullptr;
	wxListBox* m_list = nullptr;
	wxCheckBox* m_keepPasteCheck = nullptr;
	wxButton* m_renameCategoryButton = nullptr;
	wxButton* m_addCategoryButton = nullptr;
	wxButton* m_addSubcategoryButton = nullptr;
	wxButton* m_saveButton = nullptr;
	wxButton* m_pasteButton = nullptr;
	wxButton* m_deleteButton = nullptr;
	wxStaticText* m_detailsText = nullptr;
	wxStaticText* m_statusText = nullptr;
	bool m_suppressAutoPaste = false;
	static StructureManagerDialog* s_active;

	DECLARE_EVENT_TABLE()
};

#endif
