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

#include "structure_manager_window.h"

#include <wx/dir.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/textdlg.h>
#include <wx/treectrl.h>

#include <algorithm>
#include <functional>
#include <unordered_map>

#include "copybuffer.h"
#include "editor.h"
#include "filehandle.h"
#include "gui.h"
#include "iomap_otbm.h"
#include "map.h"

namespace
{
	enum {
		ID_STRUCTURE_LIST = wxID_HIGHEST + 370,
		ID_SAVE_STRUCTURE,
		ID_PASTE_STRUCTURE,
		ID_DELETE_STRUCTURE,
		ID_RENAME_CATEGORY,
		ID_ADD_CATEGORY,
		ID_ADD_SUBCATEGORY,
		ID_SEARCH_FILTER,
		ID_CATEGORY_TREE
	};

	class StructureIOMapOTBM : public IOMapOTBM
	{
	public:
		explicit StructureIOMapOTBM(MapVersion ver) : IOMapOTBM(ver) {}

		bool Save(Map& map, NodeFileWriteHandle& handle) {
			return IOMapOTBM::saveMap(map, handle);
		}

		bool Load(Map& map, NodeFileReadHandle& handle) {
			return IOMapOTBM::loadMap(map, handle);
		}
	};

	class CategoryItemData : public wxTreeItemData
	{
	public:
		explicit CategoryItemData(const wxString& path) : categoryPath(path) {}
		wxString categoryPath;
	};

	wxString GetStructuresDirectory()
	{
		wxString dataDir = g_gui.GetDataDirectory();
		wxString presetsDir = dataDir + "/presets";
		wxString structuresDir = presetsDir + "/structures";

		if(!wxDirExists(presetsDir)) {
			wxMkdir(presetsDir);
		}
		if(!wxDirExists(structuresDir)) {
			wxMkdir(structuresDir);
		}

		return structuresDir;
	}

	std::string SanitizeFilename(const std::string& name)
	{
		std::string sanitized = name;
		for(char& c : sanitized) {
			if(c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|') {
				c = '_';
			}
		}
		return sanitized;
	}

	bool BuildSelectionBuffer(Editor& editor, BaseMap& outMap, Position& outMinPos, Position& outMaxPos, int& outTiles, int& outItems)
	{
		if(!editor.hasSelection()) {
			return false;
		}

		outMap.clear(true);
		outTiles = 0;
		outItems = 0;
		bool hasPos = false;

		for(Tile* tile : editor.getSelection()) {
			if(!tile) {
				continue;
			}

			++outTiles;

			TileLocation* location = outMap.createTileL(tile->getPosition());
			Tile* copiedTile = outMap.allocator(location);

			if(tile->ground && tile->ground->isSelected()) {
				copiedTile->house_id = tile->house_id;
				copiedTile->setMapFlags(tile->getMapFlags());
			}

			ItemVector selectedItems = tile->getSelectedItems();
			for(Item* item : selectedItems) {
				++outItems;
				copiedTile->addItem(item->deepCopy());
			}

			if(tile->creature && tile->creature->isSelected()) {
				copiedTile->creature = tile->creature->deepCopy();
			}
			if(tile->spawn && tile->spawn->isSelected()) {
				copiedTile->spawn = tile->spawn->deepCopy();
			}

			outMap.setTile(copiedTile);

			const Position& pos = tile->getPosition();
			if(!hasPos) {
				outMinPos = pos;
				outMaxPos = pos;
				hasPos = true;
			} else {
				outMinPos.x = std::min(outMinPos.x, pos.x);
				outMinPos.y = std::min(outMinPos.y, pos.y);
				outMinPos.z = std::min(outMinPos.z, pos.z);
				outMaxPos.x = std::max(outMaxPos.x, pos.x);
				outMaxPos.y = std::max(outMaxPos.y, pos.y);
				outMaxPos.z = std::max(outMaxPos.z, pos.z);
			}
		}

		return hasPos;
	}

	bool SaveStructureFile(BaseMap& source, const Position& minPos, const Position& maxPos, const MapVersion& version, const wxString& path, wxString& errorOut)
	{
		Map structureMap;
		structureMap.convert(version, false);

		int width = std::max(1, maxPos.x - minPos.x + 1);
		int height = std::max(1, maxPos.y - minPos.y + 1);
		structureMap.setWidth(width);
		structureMap.setHeight(height);

		for(MapIterator it = source.begin(); it != source.end(); ++it) {
			Tile* tile = (*it)->get();
			if(!tile || tile->size() == 0) {
				continue;
			}

			Position relPos = tile->getPosition() - minPos;
			TileLocation* location = structureMap.createTileL(relPos);
			Tile* copiedTile = tile->deepCopy(structureMap);
			copiedTile->setLocation(location);
			structureMap.setTile(relPos, copiedTile);
		}

		StructureIOMapOTBM saver(structureMap.getVersion());
		DiskNodeFileWriteHandle handle(nstr(path), "OTBM");
		if(!handle.isOk()) {
			errorOut = "Failed to open file for writing.";
			return false;
		}

		if(!saver.Save(structureMap, handle)) {
			errorOut = saver.getError();
			return false;
		}

		return true;
	}

	bool LoadStructureFile(const wxString& path, BaseMap& outMap, Position& outCopyPos, wxString& errorOut)
	{
		outMap.clear(true);

		Map tempMap;
		StructureIOMapOTBM loader(tempMap.getVersion());
		DiskNodeFileReadHandle handle(nstr(path), StringVector(1, "OTBM"));
		if(!handle.isOk()) {
			errorOut = "Failed to open file.";
			return false;
		}

		if(!loader.Load(tempMap, handle)) {
			errorOut = loader.getError();
			return false;
		}

		bool hasPos = false;
		Position minPos;

		for(MapIterator it = tempMap.begin(); it != tempMap.end(); ++it) {
			Tile* tile = (*it)->get();
			if(!tile || tile->size() == 0) {
				continue;
			}

			const Position& pos = tile->getPosition();
			if(!hasPos) {
				minPos = pos;
				hasPos = true;
			} else {
				minPos.x = std::min(minPos.x, pos.x);
				minPos.y = std::min(minPos.y, pos.y);
				minPos.z = std::min(minPos.z, pos.z);
			}
		}

		if(!hasPos) {
			errorOut = "Structure is empty.";
			return false;
		}

		for(MapIterator it = tempMap.begin(); it != tempMap.end(); ++it) {
			Tile* tile = (*it)->get();
			if(!tile || tile->size() == 0) {
				continue;
			}

			Position relPos = tile->getPosition() - minPos;
			TileLocation* location = outMap.createTileL(relPos);
			Tile* copiedTile = tile->deepCopy(outMap);
			copiedTile->setLocation(location);
			outMap.setTile(relPos, copiedTile);
		}

		outCopyPos = Position(0, 0, 0);
		return true;
	}
}

StructureManagerDialog* StructureManagerDialog::s_active = nullptr;

BEGIN_EVENT_TABLE(StructureManagerDialog, wxDialog)
	EVT_BUTTON(ID_SAVE_STRUCTURE, StructureManagerDialog::OnSaveSelection)
	EVT_BUTTON(ID_PASTE_STRUCTURE, StructureManagerDialog::OnPaste)
	EVT_BUTTON(ID_DELETE_STRUCTURE, StructureManagerDialog::OnDelete)
	EVT_CLOSE(StructureManagerDialog::OnClose)
END_EVENT_TABLE()

StructureManagerDialog::StructureManagerDialog(wxWindow* parent) :
	wxDialog(parent, wxID_ANY, "Structure Manager", wxDefaultPosition, wxSize(720, 420),
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	s_active = this;
	CreateControls();
	LoadStructures();
	Bind(wxEVT_CHAR_HOOK, &StructureManagerDialog::OnCharHook, this);
	CenterOnParent();
}

StructureManagerDialog::~StructureManagerDialog()
{
	if(s_active == this) {
		s_active = nullptr;
	}
}

void StructureManagerDialog::CreateControls()
{
	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	wxBoxSizer* topSizer = new wxBoxSizer(wxHORIZONTAL);
	m_saveButton = newd wxButton(this, ID_SAVE_STRUCTURE, "Save Current Selection...");
	topSizer->Add(m_saveButton, 0, wxRIGHT | wxALIGN_CENTER_VERTICAL, 8);

	m_statusText = newd wxStaticText(this, wxID_ANY, "Select a structure to paste.");
	topSizer->Add(m_statusText, 1, wxALIGN_CENTER_VERTICAL);

	mainSizer->Add(topSizer, 0, wxALL | wxEXPAND, 8);

	wxBoxSizer* contentSizer = new wxBoxSizer(wxHORIZONTAL);

	wxPanel* leftPanel = newd wxPanel(this);
	wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText* searchLabel = newd wxStaticText(leftPanel, wxID_ANY, "Search");
	leftSizer->Add(searchLabel, 0, wxLEFT | wxRIGHT | wxTOP, 6);

	m_searchCtrl = newd wxTextCtrl(leftPanel, ID_SEARCH_FILTER, "");
	m_searchCtrl->SetHint("Type to filter...");
	m_searchCtrl->Bind(wxEVT_TEXT, &StructureManagerDialog::OnSearchChanged, this);
	leftSizer->Add(m_searchCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	m_categoryTree = newd wxTreeCtrl(leftPanel, ID_CATEGORY_TREE, wxDefaultPosition, wxSize(200, -1),
		wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_SINGLE);
	m_categoryTree->Bind(wxEVT_TREE_SEL_CHANGED, &StructureManagerDialog::OnCategoryChanged, this);
	leftSizer->Add(m_categoryTree, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	m_addCategoryButton = newd wxButton(leftPanel, ID_ADD_CATEGORY, "Add Category");
	m_addCategoryButton->Bind(wxEVT_BUTTON, &StructureManagerDialog::OnAddCategory, this);
	leftSizer->Add(m_addCategoryButton, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

	m_addSubcategoryButton = newd wxButton(leftPanel, ID_ADD_SUBCATEGORY, "Add Subcategory");
	m_addSubcategoryButton->Bind(wxEVT_BUTTON, &StructureManagerDialog::OnAddSubcategory, this);
	leftSizer->Add(m_addSubcategoryButton, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	leftPanel->SetSizer(leftSizer);
	contentSizer->Add(leftPanel, 0, wxEXPAND | wxALL, 8);

	wxPanel* centerPanel = newd wxPanel(this);
	wxBoxSizer* centerSizer = new wxBoxSizer(wxVERTICAL);
	m_list = newd wxListBox(centerPanel, ID_STRUCTURE_LIST, wxDefaultPosition, wxSize(320, -1));
	m_list->Bind(wxEVT_LISTBOX, &StructureManagerDialog::OnSelectionChanged, this);
	m_list->Bind(wxEVT_KEY_DOWN, &StructureManagerDialog::OnListKeyDown, this);
	centerSizer->Add(m_list, 1, wxEXPAND | wxALL, 4);
	wxStaticText* navHint = newd wxStaticText(centerPanel, wxID_ANY, "PgUp/PgDn: previous/next item (works with map focus)");
	centerSizer->Add(navHint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 4);
	centerPanel->SetSizer(centerSizer);
	contentSizer->Add(centerPanel, 1, wxEXPAND | wxALL, 8);

	wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);
	m_detailsText = newd wxStaticText(this, wxID_ANY, "Details: Select item...");
	rightSizer->Add(m_detailsText, 0, wxBOTTOM, 8);

	m_pasteButton = newd wxButton(this, ID_PASTE_STRUCTURE, "Paste");
	rightSizer->Add(m_pasteButton, 0, wxBOTTOM, 6);

	m_deleteButton = newd wxButton(this, ID_DELETE_STRUCTURE, "Delete");
	rightSizer->Add(m_deleteButton, 0, wxBOTTOM, 6);

	m_keepPasteCheck = newd wxCheckBox(this, wxID_ANY, "Keep paste active");
	rightSizer->Add(m_keepPasteCheck, 0);

	m_renameCategoryButton = newd wxButton(this, ID_RENAME_CATEGORY, "Rename Category...");
	rightSizer->Add(m_renameCategoryButton, 0, wxTOP, 6);
	m_renameCategoryButton->Bind(wxEVT_BUTTON, &StructureManagerDialog::OnRenameCategory, this);

	contentSizer->Add(rightSizer, 0, wxALL | wxALIGN_TOP, 8);

	mainSizer->Add(contentSizer, 1, wxEXPAND);

	SetSizerAndFit(mainSizer);
	SetMinSize(wxSize(640, 360));
}

void StructureManagerDialog::LoadStructures()
{
	m_entries.clear();
	m_listEntries.clear();
	m_categoryPaths.clear();

	m_baseDir = GetStructuresDirectory();
	m_currentCategoryPath = "";
	m_categoryPaths.push_back("");

	std::function<void(const wxString&, const wxString&)> scanDir;
	scanDir = [&](const wxString& fullPath, const wxString& relativePath) {
		wxDir dir(fullPath);
		if(!dir.IsOpened()) {
			return;
		}

		wxString filename;
		bool contFiles = dir.GetFirst(&filename, "*.otbm", wxDIR_FILES);
		while(contFiles) {
			wxFileName file(fullPath, filename);
			StructureEntry entry;
			entry.name = file.GetName();
			entry.path = file.GetFullPath();
			entry.category = relativePath;
			m_entries.push_back(entry);
			contFiles = dir.GetNext(&filename);
		}

		wxString dirname;
		bool contDirs = dir.GetFirst(&dirname, "", wxDIR_DIRS);
		while(contDirs) {
			wxString childFull = wxFileName(fullPath, dirname).GetFullPath();
			wxString childRel = relativePath.empty() ? dirname : (relativePath + "/" + dirname);
			m_categoryPaths.push_back(childRel);
			scanDir(childFull, childRel);
			contDirs = dir.GetNext(&dirname);
		}
	};

	scanDir(m_baseDir, "");

	std::sort(m_entries.begin(), m_entries.end(), [](const StructureEntry& a, const StructureEntry& b) {
		return a.name.CmpNoCase(b.name) < 0;
	});

	std::sort(m_categoryPaths.begin(), m_categoryPaths.end(), [](const wxString& a, const wxString& b) {
		return a.CmpNoCase(b) < 0;
	});
	m_categoryPaths.erase(std::unique(m_categoryPaths.begin(), m_categoryPaths.end(),
		[](const wxString& a, const wxString& b) { return a.CmpNoCase(b) == 0; }), m_categoryPaths.end());

	BuildCategoryTree();
	RefreshItemList();
}

void StructureManagerDialog::BuildCategoryTree()
{
	if(!m_categoryTree) {
		return;
	}

	m_categoryTree->Freeze();
	m_categoryTree->DeleteAllItems();

	wxTreeItemId root = m_categoryTree->AddRoot("Categories", -1, -1, new CategoryItemData(""));
	std::unordered_map<std::string, wxTreeItemId> nodeByPath;
	nodeByPath[""] = root;

	std::function<wxTreeItemId(const wxString&)> ensureNode = [&](const wxString& path) {
		std::string key = nstr(path);
		auto it = nodeByPath.find(key);
		if(it != nodeByPath.end()) {
			return it->second;
		}
		wxString parentPath;
		wxString name = path;
		int sep = path.Find('/', true);
		if(sep != wxNOT_FOUND) {
			parentPath = path.Left(sep);
			name = path.Mid(sep + 1);
		}
		wxTreeItemId parentId = ensureNode(parentPath);
		wxTreeItemId id = m_categoryTree->AppendItem(parentId, name, -1, -1, new CategoryItemData(path));
		nodeByPath[key] = id;
		return id;
	};

	for(const auto& path : m_categoryPaths) {
		if(path.empty()) {
			continue;
		}
		ensureNode(path);
	}

	m_categoryTree->Expand(root);
	m_categoryTree->SelectItem(root);
	m_categoryTree->Thaw();
}

void StructureManagerDialog::RefreshItemList()
{
	if(!m_list) {
		return;
	}

	m_list->Clear();
	m_listEntries.clear();

	wxString filter = m_searchCtrl ? m_searchCtrl->GetValue() : wxString();
	filter.MakeLower();

	for(const auto& entry : m_entries) {
		if(entry.category != m_currentCategoryPath) {
			continue;
		}
		if(!filter.empty()) {
			wxString lowered = entry.name;
			lowered.MakeLower();
			if(lowered.Find(filter) == wxNOT_FOUND) {
				continue;
			}
		}
		m_list->Append(entry.name);
		m_listEntries.push_back(&entry);
	}
	UpdateSelectionUi();
}

void StructureManagerDialog::UpdateSelectionUi()
{
	const int selection = m_list ? m_list->GetSelection() : wxNOT_FOUND;
	const bool hasSelection = selection != wxNOT_FOUND &&
		selection >= 0 && selection < static_cast<int>(m_listEntries.size());
	if(m_pasteButton) {
		m_pasteButton->Enable(hasSelection);
	}
	if(m_deleteButton) {
		m_deleteButton->Enable(hasSelection);
	}
	if(m_detailsText) {
		if(hasSelection) {
			const auto& entry = *m_listEntries[selection];
			wxString categoryLabel = entry.category.empty() ? wxString("General") : entry.category;
			m_detailsText->SetLabel("Details: " + entry.name + " (" + categoryLabel + ")");
		} else {
			m_detailsText->SetLabel("Details: Select item...");
		}
	}

	if(m_renameCategoryButton) {
		bool canRename = !m_currentCategoryPath.empty();
		m_renameCategoryButton->Enable(canRename);
	}
}

void StructureManagerDialog::SetStatusText(const wxString& text)
{
	if(m_statusText) {
		m_statusText->SetLabel(text);
	}
}

const StructureManagerDialog::StructureEntry* StructureManagerDialog::GetSelectedEntry(int* outIndex) const
{
	if(outIndex) {
		*outIndex = wxNOT_FOUND;
	}
	if(!m_list) {
		return nullptr;
	}
	int selection = m_list->GetSelection();
	if(selection == wxNOT_FOUND || selection < 0 || selection >= static_cast<int>(m_listEntries.size())) {
		return nullptr;
	}
	if(outIndex) {
		*outIndex = selection;
	}
	return m_listEntries[selection];
}

void StructureManagerDialog::SelectCategoryByPath(const wxString& path)
{
	if(!m_categoryTree) {
		return;
	}
	wxTreeItemId root = m_categoryTree->GetRootItem();
	if(!root.IsOk()) {
		return;
	}

	std::function<bool(const wxTreeItemId&)> findNode = [&](const wxTreeItemId& node) {
		if(!node.IsOk()) {
			return false;
		}
		CategoryItemData* data = static_cast<CategoryItemData*>(m_categoryTree->GetItemData(node));
		if(data && data->categoryPath.CmpNoCase(path) == 0) {
			m_categoryTree->SelectItem(node);
			return true;
		}
		wxTreeItemIdValue cookie;
		wxTreeItemId child = m_categoryTree->GetFirstChild(node, cookie);
		while(child.IsOk()) {
			if(findNode(child)) {
				return true;
			}
			child = m_categoryTree->GetNextChild(node, cookie);
		}
		return false;
	};

	findNode(root);
}

void StructureManagerDialog::SelectEntryByName(const wxString& name)
{
	if(!m_list) {
		return;
	}
	for(size_t i = 0; i < m_listEntries.size(); ++i) {
		if(m_listEntries[i]->name.CmpNoCase(name) == 0) {
			m_suppressAutoPaste = true;
			m_list->SetSelection(static_cast<int>(i));
			m_suppressAutoPaste = false;
			UpdateSelectionUi();
			return;
		}
	}
}

void StructureManagerDialog::StartPasteFromEntry(const StructureEntry& entry)
{
	if(!g_gui.IsEditorOpen()) {
		return;
	}

	BaseMap* buffer = newd BaseMap();
	Position copyPos;
	wxString error;
	if(!LoadStructureFile(entry.path, *buffer, copyPos, error)) {
		delete buffer;
		wxMessageBox("Failed to load structure:\n" + error, "Structure Manager", wxOK | wxICON_ERROR, this);
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if(!editor) {
		delete buffer;
		return;
	}

	editor->copybuffer.setBuffer(buffer, copyPos);
	bool keepPaste = m_keepPasteCheck && m_keepPasteCheck->GetValue();
	g_gui.PreparePaste(keepPaste);
	wxString status = "Paste: Click map to place '" + entry.name + "'. Right-click cancel.";
	SetStatusText(status);
	g_gui.SetStatusText(status);
}

void StructureManagerDialog::RenameSelectedCategory()
{
	if(m_currentCategoryPath.empty()) {
		return;
	}

	wxString currentName = m_currentCategoryPath;
	int sep = currentName.Find('/', true);
	if(sep != wxNOT_FOUND) {
		currentName = currentName.Mid(sep + 1);
	}

	wxTextEntryDialog dialog(this, "New category name:", "Rename Category", currentName);
	if(dialog.ShowModal() != wxID_OK) {
		return;
	}

	wxString name = dialog.GetValue();
	name.Trim(true).Trim(false);
	if(name.empty()) {
		wxMessageBox("Category name cannot be empty.", "Structure Manager", wxOK | wxICON_WARNING, this);
		return;
	}

	std::string sanitized = SanitizeFilename(nstr(name));
	wxString safeName = wxstr(sanitized);
	wxString baseDir = GetStructuresDirectory();

	wxString newRelativePath;
	int parentSep = m_currentCategoryPath.Find('/', true);
	if(parentSep != wxNOT_FOUND) {
		wxString parentPath = m_currentCategoryPath.Left(parentSep);
		newRelativePath = parentPath + "/" + safeName;
	} else {
		newRelativePath = safeName;
	}
	wxString newDir = wxFileName(baseDir, newRelativePath).GetFullPath();
	wxString oldDir = wxFileName(baseDir, m_currentCategoryPath).GetFullPath();

	if(wxDirExists(newDir)) {
		wxMessageBox("Category already exists.", "Structure Manager", wxOK | wxICON_INFORMATION, this);
		return;
	}

	if(!wxRenameFile(oldDir, newDir, false)) {
		wxMessageBox("Failed to rename category.", "Structure Manager", wxOK | wxICON_ERROR, this);
		return;
	}

	LoadStructures();
	SelectCategoryByPath(newRelativePath);
}

void StructureManagerDialog::SelectAdjacentEntry(int delta)
{
	if(!m_list || m_listEntries.empty()) {
		return;
	}

	int selection = m_list->GetSelection();
	if(selection == wxNOT_FOUND) {
		selection = (delta > 0) ? 0 : static_cast<int>(m_listEntries.size()) - 1;
	} else {
		selection += delta;
		if(selection < 0) {
			selection = 0;
		} else if(selection >= static_cast<int>(m_listEntries.size())) {
			selection = static_cast<int>(m_listEntries.size()) - 1;
		}
	}

	m_suppressAutoPaste = true;
	m_list->SetSelection(selection);
	m_suppressAutoPaste = false;
	UpdateSelectionUi();

	const StructureEntry& entry = *m_listEntries[selection];
	StartPasteFromEntry(entry);
}

void StructureManagerDialog::OnSaveSelection(wxCommandEvent& WXUNUSED(event))
{
	if(!g_gui.IsEditorOpen()) {
		wxMessageBox("Open a map before saving structures.", "Structure Manager", wxOK | wxICON_INFORMATION, this);
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if(!editor || !editor->hasSelection()) {
		wxMessageBox("Select tiles before saving a structure.", "Structure Manager", wxOK | wxICON_INFORMATION, this);
		return;
	}

	wxTextEntryDialog nameDialog(this, "Structure name:", "Save Structure");
	if(nameDialog.ShowModal() != wxID_OK) {
		return;
	}

	wxString name = nameDialog.GetValue();
	name.Trim(true).Trim(false);
	if(name.empty()) {
		wxMessageBox("Structure name cannot be empty.", "Structure Manager", wxOK | wxICON_WARNING, this);
		return;
	}

	std::string sanitized = SanitizeFilename(nstr(name));
	wxString safeName = wxstr(sanitized);
	const wxString categoryPath = m_currentCategoryPath;
	wxString dir = m_baseDir;
	if(!categoryPath.empty()) {
		dir = wxFileName(m_baseDir, categoryPath).GetFullPath();
	}
	wxString path = wxFileName(dir, safeName + ".otbm").GetFullPath();

	if(wxFileExists(path)) {
		int ret = wxMessageBox("A structure with this name already exists. Overwrite?", "Structure Manager",
			wxYES_NO | wxICON_WARNING, this);
		if(ret != wxYES) {
			return;
		}
	}

	BaseMap buffer;
	Position minPos;
	Position maxPos;
	int tileCount = 0;
	int itemCount = 0;
	if(!BuildSelectionBuffer(*editor, buffer, minPos, maxPos, tileCount, itemCount)) {
		wxMessageBox("Failed to build structure from selection.", "Structure Manager", wxOK | wxICON_ERROR, this);
		return;
	}

	wxString error;
	if(!SaveStructureFile(buffer, minPos, maxPos, editor->getMap().getVersion(), path, error)) {
		wxMessageBox("Failed to save structure:\n" + error, "Structure Manager", wxOK | wxICON_ERROR, this);
		return;
	}

	LoadStructures();
	SelectCategoryByPath(categoryPath);
	SelectEntryByName(safeName);
	SetStatusText(wxString::Format("Saved '%s' (%d tile%s, %d item%s).",
		safeName,
		tileCount, tileCount == 1 ? "" : "s",
		itemCount, itemCount == 1 ? "" : "s"));
	g_gui.SetStatusText("Structure saved.");
}

void StructureManagerDialog::OnPaste(wxCommandEvent& WXUNUSED(event))
{
	const StructureEntry* entry = GetSelectedEntry(nullptr);
	if(!entry) {
		return;
	}
	StartPasteFromEntry(*entry);
}

void StructureManagerDialog::OnDelete(wxCommandEvent& WXUNUSED(event))
{
	int selection = wxNOT_FOUND;
	const StructureEntry* entry = GetSelectedEntry(&selection);
	if(!entry) {
		return;
	}

	int ret = wxMessageBox("Delete structure '" + entry->name + "'?", "Structure Manager",
		wxYES_NO | wxICON_WARNING, this);
	if(ret != wxYES) {
		return;
	}

	if(wxFileExists(entry->path)) {
		wxRemoveFile(entry->path);
	}

	LoadStructures();
	SelectCategoryByPath(entry->category);
	SetStatusText("Structure deleted.");
	g_gui.SetStatusText("Structure deleted.");
}

void StructureManagerDialog::OnSelectionChanged(wxCommandEvent& WXUNUSED(event))
{
	UpdateSelectionUi();

	if(m_suppressAutoPaste) {
		return;
	}

	const StructureEntry* entry = GetSelectedEntry(nullptr);
	if(entry) {
		StartPasteFromEntry(*entry);
	}
}

void StructureManagerDialog::OnListKeyDown(wxKeyEvent& event)
{
	const int key = event.GetKeyCode();
	if(key == WXK_F2) {
		int selection = wxNOT_FOUND;
		const StructureEntry* entry = GetSelectedEntry(&selection);
		if(!entry) {
			return;
		}

		wxTextEntryDialog dialog(this, "New name:", "Rename Structure", entry->name);
		if(dialog.ShowModal() != wxID_OK) {
			return;
		}

		wxString name = dialog.GetValue();
		name.Trim(true).Trim(false);
		if(name.empty()) {
			wxMessageBox("Structure name cannot be empty.", "Structure Manager", wxOK | wxICON_WARNING, this);
			return;
		}

		std::string sanitized = SanitizeFilename(nstr(name));
		wxString safeName = wxstr(sanitized);
		wxString dir = m_baseDir;
		if(!entry->category.empty()) {
			dir = wxFileName(m_baseDir, entry->category).GetFullPath();
		}
		wxString newPath = wxFileName(dir, safeName + ".otbm").GetFullPath();
		if(wxFileExists(newPath)) {
			int ret = wxMessageBox("A structure with this name already exists. Overwrite?", "Structure Manager",
				wxYES_NO | wxICON_WARNING, this);
			if(ret != wxYES) {
				return;
			}
		}

		if(!wxRenameFile(entry->path, newPath, true)) {
			wxMessageBox("Failed to rename structure.", "Structure Manager", wxOK | wxICON_ERROR, this);
			return;
		}

		LoadStructures();
		SelectCategoryByPath(entry->category);
		SelectEntryByName(safeName);
		return;
	}

	if(key == WXK_DELETE || key == WXK_NUMPAD_DELETE) {
		wxCommandEvent dummy;
		OnDelete(dummy);
		return;
	}

	if(key == WXK_UP && event.ControlDown()) {
		SelectAdjacentEntry(-1);
		return;
	}
	if(key == WXK_DOWN && event.ControlDown()) {
		SelectAdjacentEntry(1);
		return;
	}

	event.Skip();
}

void StructureManagerDialog::OnRenameCategory(wxCommandEvent& WXUNUSED(event))
{
	RenameSelectedCategory();
}

void StructureManagerDialog::AddCategory(bool asChild)
{
	wxString parentPath;
	if(asChild) {
		if(m_currentCategoryPath.empty()) {
			wxMessageBox("Select a category to add a subcategory.", "Structure Manager", wxOK | wxICON_INFORMATION, this);
			return;
		}
		parentPath = m_currentCategoryPath;
		int sep = parentPath.Find('/');
		if(sep != wxNOT_FOUND) {
			parentPath = parentPath.Left(sep);
		}
	}

	wxTextEntryDialog dialog(this,
		asChild ? "Subcategory name:" : "Category name:",
		asChild ? "Add Subcategory" : "Add Category");
	if(dialog.ShowModal() != wxID_OK) {
		return;
	}

	wxString name = dialog.GetValue();
	name.Trim(true).Trim(false);
	if(name.empty()) {
		wxMessageBox("Category name cannot be empty.", "Structure Manager", wxOK | wxICON_WARNING, this);
		return;
	}

	std::string sanitized = SanitizeFilename(nstr(name));
	wxString safeName = wxstr(sanitized);

	wxString newRelativePath = parentPath.empty() ? safeName : (parentPath + "/" + safeName);
	wxString newDir = wxFileName(m_baseDir, newRelativePath).GetFullPath();

	if(wxDirExists(newDir)) {
		wxMessageBox("Category already exists.", "Structure Manager", wxOK | wxICON_INFORMATION, this);
		return;
	}

	if(!wxFileName::Mkdir(newDir, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
		wxMessageBox("Failed to create category folder.", "Structure Manager", wxOK | wxICON_ERROR, this);
		return;
	}

	LoadStructures();
	SelectCategoryByPath(newRelativePath);
}

void StructureManagerDialog::OnAddCategory(wxCommandEvent& WXUNUSED(event))
{
	AddCategory(false);
}

void StructureManagerDialog::OnAddSubcategory(wxCommandEvent& WXUNUSED(event))
{
	AddCategory(true);
}

void StructureManagerDialog::OnCategoryChanged(wxTreeEvent& event)
{
	wxTreeItemId item = event.GetItem();
	if(!item.IsOk()) {
		return;
	}

	CategoryItemData* data = static_cast<CategoryItemData*>(m_categoryTree->GetItemData(item));
	m_currentCategoryPath = data ? data->categoryPath : wxString();
	RefreshItemList();
}

void StructureManagerDialog::OnSearchChanged(wxCommandEvent& WXUNUSED(event))
{
	RefreshItemList();
}

void StructureManagerDialog::OnCharHook(wxKeyEvent& event)
{
	const int key = event.GetKeyCode();
	if(key == WXK_F2) {
		wxWindow* focus = wxWindow::FindFocus();
		if(focus && m_categoryTree && (focus == m_categoryTree || focus->GetParent() == m_categoryTree)) {
			RenameSelectedCategory();
			return;
		}
	}

	event.Skip();
}

void StructureManagerDialog::OnClose(wxCloseEvent& WXUNUSED(event))
{
	Destroy();
}

bool StructureManagerDialog::HandleGlobalHotkey(wxKeyEvent& event)
{
	if(!s_active || !s_active->IsShown()) {
		return false;
	}
	return s_active->HandleGlobalHotkeyInternal(event);
}

bool StructureManagerDialog::HandleGlobalHotkeyInternal(wxKeyEvent& event)
{
	const int key = event.GetKeyCode();
	if(key == WXK_PAGEUP || key == WXK_PAGEDOWN) {
		SelectAdjacentEntry(key == WXK_PAGEUP ? -1 : 1);
		return true;
	}
	if(event.ControlDown()) {
		if(key == WXK_UP || key == WXK_NUMPAD_UP) {
			SelectAdjacentEntry(-1);
			return true;
		}
		if(key == WXK_DOWN || key == WXK_NUMPAD_DOWN) {
			SelectAdjacentEntry(1);
			return true;
		}
	}
	return false;
}
