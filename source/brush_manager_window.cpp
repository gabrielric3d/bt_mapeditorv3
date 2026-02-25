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

#include "brush_manager_window.h"

#include "gui.h"
#include "materials.h"
#include "graphics.h"
#include "items.h"

#include <initializer_list>

#include <wx/ffile.h>
#include <wx/file.h>

namespace
{
constexpr const char* DISABLE_ATTRIBUTE = "rme_disabled";

enum
{
	ID_OPEN_FILE = wxID_HIGHEST + 400,
	ID_SAVE_FILE,
	ID_REFRESH_LIST,
	ID_BRUSH_SELECTION,
	ID_BRUSH_VALUE,
	ID_BRUSH_FILTER,
};

enum BrushColumn
{
	COL_ENABLED = 0,
	COL_NAME,
	COL_TYPE,
	COL_SERVER_ID,
};

pugi::xml_attribute FindAttribute(const pugi::xml_node& node, std::initializer_list<const char*> names)
{
	for(const char* name : names) {
		if(pugi::xml_attribute attr = node.attribute(name)) {
			return attr;
		}
	}
	return pugi::xml_attribute();
}

uint32_t ExtractServerId(const pugi::xml_node& node)
{
	if(pugi::xml_attribute attr = FindAttribute(node, { "server_lookid", "serverid", "server_id", "id" })) {
		return attr.as_uint();
	}
	return 0;
}

uint32_t ExtractClientLookId(const pugi::xml_node& node)
{
	if(pugi::xml_attribute attr = node.attribute("lookid")) {
		return attr.as_uint();
	}
	return 0;
}

uint32_t ExtractChildItemId(const pugi::xml_node& node)
{
	for(pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
		if(as_lower_str(child.name()) != "item") {
			continue;
		}

		if(pugi::xml_attribute attr = FindAttribute(child, { "id", "serverid", "server_lookid" })) {
			return attr.as_uint();
		}
	}
	return 0;
}

uint32_t ResolveSpriteId(const pugi::xml_node& node, uint32_t serverId)
{
	if(uint32_t look = ExtractClientLookId(node)) {
		return look;
	}

	const uint32_t itemId = ExtractChildItemId(node);
	const uint32_t lookupId = itemId ? itemId : serverId;
	if(lookupId != 0) {
		const ItemType& type = g_items.getItemType(lookupId);
		if(type.clientID != 0) {
			return type.clientID;
		}
	}
	return 0;
}
}

class BrushSpritePanel : public wxPanel
{
public:
	BrushSpritePanel(wxWindow* parent) :
		wxPanel(parent, wxID_ANY, wxDefaultPosition, wxSize(96, 96)),
		spriteId(0),
		hasSprite(false)
	{
		SetBackgroundColour(wxColour(30, 30, 30));
		Bind(wxEVT_PAINT, &BrushSpritePanel::OnPaint, this);
	}

	void SetSprite(uint32_t id) {
		spriteId = id;
		hasSprite = (id != 0);
		Refresh();
	}

	void Clear() {
		hasSprite = false;
		spriteId = 0;
		Refresh();
	}

private:
	void OnPaint(wxPaintEvent& event) {
		wxPaintDC dc(this);
		dc.SetBackground(wxBrush(GetBackgroundColour()));
		dc.Clear();
		if(!hasSprite)
			return;

		Sprite* sprite = g_gui.gfx.getSprite(spriteId);
		if(!sprite)
			return;

		const wxSize size = GetClientSize();
		const int target = std::min(64, std::min(size.GetWidth(), size.GetHeight()));
		const int x = (size.GetWidth() - target) / 2;
		const int y = (size.GetHeight() - target) / 2;
		sprite->DrawTo(&dc, SPRITE_SIZE_32x32, x, y, target, target);
	}

	uint32_t spriteId;
	bool hasSprite;
};

BrushManagerDialog::BrushManagerDialog(wxWindow* parent) :
	wxDialog(parent, wxID_ANY, "Brush Manager", wxDefaultPosition, wxSize(900, 520),
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	currentSelection(-1)
{
	wxBoxSizer* rootSizer = newd wxBoxSizer(wxVERTICAL);

	wxStaticText* introText = newd wxStaticText(this, wxID_ANY, "Loaded brush XML files");
	introText->SetForegroundColour(*wxWHITE);
	rootSizer->Add(introText, 0, wxALL, 5);

	wxBoxSizer* contentSizer = newd wxBoxSizer(wxHORIZONTAL);

	listCtrl = newd wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
		wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES);
	listCtrl->SetTextColour(*wxWHITE);
	listCtrl->AppendColumn("File", wxLIST_FORMAT_LEFT, 160);
	listCtrl->AppendColumn("Full path");
	listCtrl->AppendColumn("Status", wxLIST_FORMAT_LEFT, 90);
	contentSizer->Add(listCtrl, 1, wxEXPAND | wxRIGHT, 5);

	wxBoxSizer* rightPane = newd wxBoxSizer(wxVERTICAL);

	wxStaticText* brushLabel = newd wxStaticText(this, wxID_ANY, "Brushes");
	brushLabel->SetForegroundColour(*wxWHITE);
	rightPane->Add(brushLabel, 0, wxBOTTOM, 2);

	brushFilterCtrl = newd wxComboBox(this, ID_BRUSH_FILTER, "", wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN);
	brushFilterCtrl->SetHint("Filter brushes...");
	brushFilterCtrl->SetForegroundColour(*wxBLACK);
	brushFilterCtrl->SetBackgroundColour(*wxWHITE);

	// Disable global hotkeys when filter has focus to prevent interference while typing
	brushFilterCtrl->Bind(wxEVT_SET_FOCUS, [](wxFocusEvent& event) {
		g_gui.DisableHotkeys();
		event.Skip();
	});
	brushFilterCtrl->Bind(wxEVT_KILL_FOCUS, [](wxFocusEvent& event) {
		g_gui.EnableHotkeys();
		event.Skip();
	});

	rightPane->Add(brushFilterCtrl, 0, wxEXPAND | wxBOTTOM, 2);

	brushListCtrl = newd wxDataViewListCtrl(this, ID_BRUSH_SELECTION, wxDefaultPosition, wxDefaultSize,
		wxDV_SINGLE | wxDV_ROW_LINES | wxDV_VERT_RULES);
	brushListCtrl->SetForegroundColour(*wxBLACK);
	brushListCtrl->SetBackgroundColour(*wxWHITE);
	brushListCtrl->AppendToggleColumn("Enabled", wxDATAVIEW_CELL_ACTIVATABLE, 70, wxALIGN_CENTER);
	brushListCtrl->AppendTextColumn("Name", wxDATAVIEW_CELL_INERT, 160);
	brushListCtrl->AppendTextColumn("Type", wxDATAVIEW_CELL_INERT, 80);
	brushListCtrl->AppendTextColumn("Server ID", wxDATAVIEW_CELL_INERT, 80);
	rightPane->Add(brushListCtrl, 1, wxEXPAND | wxBOTTOM, 5);

	spritePreview = newd BrushSpritePanel(this);
	spritePreview->SetMinSize(wxSize(80, 80));
	spritePreview->Hide();
	rightPane->Add(spritePreview, 0, wxEXPAND | wxBOTTOM, 5);

	xmlContentPanel = newd wxPanel(this);
	wxBoxSizer* xmlPanelSizer = newd wxBoxSizer(wxVERTICAL);
	xmlContentsLabel = newd wxStaticText(xmlContentPanel, wxID_ANY, "XML Contents");
	xmlContentsLabel->SetForegroundColour(*wxWHITE);
	xmlPanelSizer->Add(xmlContentsLabel, 0, wxBOTTOM, 2);

	editorCtrl = newd wxTextCtrl(xmlContentPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE | wxTE_DONTWRAP | wxTE_READONLY | wxTE_RICH2);
	editorCtrl->SetForegroundColour(*wxWHITE);
	editorCtrl->SetBackgroundColour(wxColour(30, 30, 30));
	wxFont editorFont(wxFontInfo(10).Family(wxFONTFAMILY_TELETYPE));
	editorCtrl->SetFont(editorFont);
	xmlPanelSizer->Add(editorCtrl, 1, wxEXPAND);
	xmlContentPanel->SetSizer(xmlPanelSizer);
	xmlContentPanel->Hide();

	rightPane->Add(xmlContentPanel, 1, wxEXPAND);

	contentSizer->Add(rightPane, 1, wxEXPAND);

	rootSizer->Add(contentSizer, 1, wxEXPAND | wxLEFT | wxRIGHT, 5);

	statusLabel = newd wxStaticText(this, wxID_ANY, "");
	statusLabel->SetForegroundColour(*wxWHITE);
	rootSizer->Add(statusLabel, 0, wxEXPAND | wxALL, 5);

	wxBoxSizer* buttonSizer = newd wxBoxSizer(wxHORIZONTAL);
	openButton = newd wxButton(this, ID_OPEN_FILE, "Open File");
	buttonSizer->Add(openButton, 0, wxRIGHT, 5);
	saveButton = newd wxButton(this, ID_SAVE_FILE, "Save Changes");
	buttonSizer->Add(saveButton, 0, wxRIGHT, 5);
	wxButton* refreshButton = newd wxButton(this, ID_REFRESH_LIST, "Refresh");
	buttonSizer->Add(refreshButton, 0, wxRIGHT, 5);
	wxButton* closeButton = newd wxButton(this, wxID_CANCEL, "Close");
	buttonSizer->Add(closeButton, 0);

	rootSizer->Add(buttonSizer, 0, wxALIGN_RIGHT | wxALL, 5);

	SetSizerAndFit(rootSizer);
	rootSizer->SetSizeHints(this);

	openButton->Bind(wxEVT_BUTTON, &BrushManagerDialog::OnOpenFile, this);
	saveButton->Bind(wxEVT_BUTTON, &BrushManagerDialog::OnSaveFile, this);
	refreshButton->Bind(wxEVT_BUTTON, &BrushManagerDialog::OnRefresh, this);
	listCtrl->Bind(wxEVT_LIST_ITEM_ACTIVATED, &BrushManagerDialog::OnListActivated, this);
	listCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &BrushManagerDialog::OnSelectionChanged, this);
	listCtrl->Bind(wxEVT_LIST_ITEM_DESELECTED, &BrushManagerDialog::OnSelectionChanged, this);
	brushListCtrl->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &BrushManagerDialog::OnBrushSelection, this);
	brushListCtrl->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED, &BrushManagerDialog::OnBrushValueChanged, this);
	brushFilterCtrl->Bind(wxEVT_TEXT, &BrushManagerDialog::OnBrushFilterChanged, this);

	currentEntry = nullptr;
	currentSelection = -1;
	Reload();
	UpdateSelectionUi();
	CentreOnParent();
}

void BrushManagerDialog::Reload()
{
	long previousSelection = GetSelection();
	entries.clear();
	listCtrl->DeleteAllItems();
	brushListCtrl->DeleteAllItems();
	displayedBrushes.clear();
	currentEntry = nullptr;
	statusLabel->SetLabel("");

	if(!g_gui.IsVersionLoaded()) {
		statusLabel->SetLabel("No client version is loaded.");
		return;
	}

	FileName dataPath = g_gui.GetCurrentVersion().getDataPath();
	FileName materialsFile = dataPath;
	materialsFile.SetFullName("materials.xml");

	BrushFileEntry mainEntry;
	mainEntry.displayName = "materials.xml";
	mainEntry.path = materialsFile;
	mainEntry.exists = materialsFile.FileExists();
	entries.push_back(mainEntry);

	if(!mainEntry.exists) {
		wxString warning = "materials.xml not found in " + materialsFile.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
		statusLabel->SetLabel(warning);
		PopulateList();
		return;
	}

	pugi::xml_document doc;
	pugi::xml_parse_result result = doc.load_file(materialsFile.GetFullPath().mb_str());
	if(!result) {
		wxString warning = "Failed to parse materials.xml: ";
		warning << wxString(result.description(), wxConvUTF8);
		statusLabel->SetLabel(warning);
		PopulateList();
		return;
	}

	pugi::xml_node root = doc.child("materials");
	if(!root) {
		statusLabel->SetLabel("materials.xml does not contain a <materials> root node.");
		PopulateList();
		return;
	}

	for(pugi::xml_node child = root.first_child(); child; child = child.next_sibling())
	{
		const std::string& childName = as_lower_str(child.name());
		if(childName != "include") {
			continue;
		}

		pugi::xml_attribute fileAttr = child.attribute("file");
		if(!fileAttr) {
			continue;
		}

		wxString includeName = wxString(fileAttr.as_string(), wxConvUTF8);
		if(includeName.IsEmpty()) {
			continue;
		}

		BrushFileEntry entry;
	entry.displayName = includeName;
	entry.path = dataPath;
	entry.path.SetFullName(includeName);
	entry.exists = entry.path.FileExists();

		if(entry.exists) {
			entry.document = std::make_shared<pugi::xml_document>();
			if(entry.document->load_file(entry.path.GetFullPath().mb_str())) {
				ParseBrushes(entry);
			} else {
				entry.document.reset();
			}
		} else {
			entry.document.reset();
		}
		entries.push_back(entry);
	}

	wxString info = "Files loaded from ";
	info << materialsFile.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
	statusLabel->SetLabel(info);

	PopulateList();
}

void BrushManagerDialog::PopulateList()
{
	listCtrl->Freeze();
	listCtrl->DeleteAllItems();
	long previousSelection = currentSelection;

	for(size_t i = 0; i < entries.size(); ++i) {
		const BrushFileEntry& entry = entries[i];
		long index = listCtrl->InsertItem(listCtrl->GetItemCount(), entry.displayName);
		listCtrl->SetItem(index, 1, entry.path.GetFullPath());
		listCtrl->SetItem(index, 2, entry.exists ? "Available" : "Missing");
		if(!entry.exists) {
			listCtrl->SetItemBackgroundColour(index, wxColour(255, 235, 235));
		}
	}

	listCtrl->Thaw();

	if(previousSelection >= 0 && previousSelection < static_cast<long>(entries.size())) {
		listCtrl->SetItemState(previousSelection, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		listCtrl->EnsureVisible(previousSelection);
		currentSelection = previousSelection;
	} else {
		currentSelection = -1;
		editorCtrl->ChangeValue("");
		editorCtrl->SetEditable(false);
	}

	UpdateSelectionUi();
}

long BrushManagerDialog::GetSelection() const
{
	return listCtrl->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
}

void BrushManagerDialog::UpdateSelectionUi()
{
	const long selection = GetSelection();
	const bool hasSelection = selection != -1;
	bool fileExists = false;
	if(hasSelection && selection < static_cast<long>(entries.size())) {
		fileExists = entries[selection].exists && entries[selection].document != nullptr;
	}
	openButton->Enable(hasSelection);
	saveButton->Enable(fileExists);
}

void BrushManagerDialog::OnOpenFile(wxCommandEvent& WXUNUSED(event))
{
	OpenSelectedFile();
}

void BrushManagerDialog::OpenSelectedFile()
{
	long selection = GetSelection();
	if(selection == -1 || selection >= static_cast<long>(entries.size())) {
		wxMessageBox("Select a file entry first.", "Brush Manager", wxOK | wxICON_INFORMATION, this);
		return;
	}

	const BrushFileEntry& entry = entries[selection];
	if(!entry.exists) {
		wxMessageBox("The selected file does not exist:\n" + entry.path.GetFullPath(), "Brush Manager", wxOK | wxICON_WARNING, this);
		return;
	}

	if(!wxLaunchDefaultApplication(entry.path.GetFullPath())) {
		wxMessageBox("Failed to open:\n" + entry.path.GetFullPath(), "Brush Manager", wxOK | wxICON_ERROR, this);
	}
}

void BrushManagerDialog::LoadSelectedFileContent()
{
	long selection = GetSelection();
	if(selection == currentSelection) {
		if(selection >= 0 && selection < static_cast<long>(entries.size())) {
			PopulateBrushList(entries[selection]);
		}
		UpdateSelectionUi();
		return;
	}

	currentSelection = selection;
	currentEntry = nullptr;
	displayedBrushes.clear();
	brushListCtrl->DeleteAllItems();
	editorCtrl->ChangeValue("");
	RefreshBrushPreview(nullptr);

	if(selection == -1 || selection >= static_cast<long>(entries.size())) {
		UpdateSelectionUi();
		return;
	}

	BrushFileEntry& entry = entries[selection];
	if(!entry.exists || !entry.document) {
		SetStatus("File not available: " + entry.path.GetFullPath());
		UpdateSelectionUi();
		return;
	}

	PopulateBrushList(entry);
	UpdateSelectionUi();
}

void BrushManagerDialog::SetStatus(const wxString& text)
{
	statusLabel->SetLabel(text);
}

void BrushManagerDialog::PopulateBrushList(const BrushFileEntry& entry)
{
	currentEntry = const_cast<BrushFileEntry*>(&entry);
	brushListCtrl->DeleteAllItems();
	displayedBrushes.clear();

	wxString loweredFilter = brushFilter.Lower();

	for(size_t i = 0; i < entry.brushes.size(); ++i) {
		const BrushDescriptor& descriptor = entry.brushes[i];
		if(!loweredFilter.empty() && !descriptor.name.Lower().Contains(loweredFilter)) {
			continue;
		}

		wxVector<wxVariant> cols;
		cols.push_back(wxVariant(descriptor.enabled));
		cols.push_back(wxVariant(descriptor.name));
		cols.push_back(wxVariant(descriptor.type));
		cols.push_back(wxVariant(wxString::Format("%u", descriptor.serverId)));
		brushListCtrl->AppendItem(cols);
		displayedBrushes.push_back(i);
	}

	if(displayedBrushes.empty()) {
		SetStatus("No brushes displayed for " + entry.displayName);
		RefreshBrushPreview(nullptr);
	} else {
		SetStatus(wxString::Format("Loaded %zu brushes from %s", displayedBrushes.size(), entry.displayName));
	}
}

void BrushManagerDialog::ParseBrushes(BrushFileEntry& entry)
{
	entry.brushes.clear();
	if(!entry.document) {
		return;
	}

	std::vector<pugi::xml_node> stack;
	for(pugi::xml_node node = entry.document->first_child(); node; node = node.next_sibling()) {
		stack.push_back(node);
	}

	auto appendDescriptor = [&](const pugi::xml_node& sourceNode, const pugi::xml_node& storedNode, bool fromComment)
	{
		if(!sourceNode) {
			return;
		}

		const std::string tag = as_lower_str(sourceNode.name());
		if(tag != "brush" && tag != "border" && tag != "autoborder") {
			return;
		}

		pugi::xml_attribute nameAttr = sourceNode.attribute("name");
		if(!nameAttr) {
			return;
		}

		BrushDescriptor descriptor;
		descriptor.name = wxString(nameAttr.as_string(), wxConvUTF8);
		pugi::xml_attribute typeAttr = sourceNode.attribute("type");
		descriptor.type = typeAttr ? wxString(typeAttr.as_string(), wxConvUTF8) : wxString(tag.c_str(), wxConvUTF8);
		descriptor.serverId = ExtractServerId(sourceNode);
		descriptor.spriteId = ResolveSpriteId(sourceNode, descriptor.serverId);
		descriptor.fileLabel = entry.displayName;
		descriptor.node = storedNode;
		if(fromComment) {
			descriptor.enabled = false;
		} else {
			descriptor.enabled = !storedNode.attribute(DISABLE_ATTRIBUTE).as_bool();
		}
		entry.brushes.push_back(descriptor);
	};

	while(!stack.empty()) {
		pugi::xml_node node = stack.back();
		stack.pop_back();

		if(node.type() == pugi::node_element) {
			appendDescriptor(node, node, false);
			for(pugi::xml_node child = node.first_child(); child; child = child.next_sibling()) {
				if(child.type() == pugi::node_element || child.type() == pugi::node_comment) {
					stack.push_back(child);
				}
			}
		} else if(node.type() == pugi::node_comment) {
			const char* value = node.value();
			if(!value)
				continue;
			pugi::xml_document temp;
			if(!temp.load(value))
				continue;

			pugi::xml_node child = temp.first_child();
			appendDescriptor(child, node, true);
		}
	}
}

void BrushManagerDialog::UpdateSaveButton()
{
	UpdateSelectionUi();
}

void BrushManagerDialog::SaveSelectedFile()
{
	if(currentSelection < 0 || currentSelection >= static_cast<long>(entries.size())) {
		wxMessageBox("Select a file before saving.", "Brush Manager", wxOK | wxICON_INFORMATION, this);
		return;
	}

	BrushFileEntry& entry = entries[currentSelection];
	if(!entry.document) {
		wxMessageBox("Document not loaded for:\n" + entry.path.GetFullPath(), "Brush Manager", wxOK | wxICON_ERROR, this);
		return;
	}

	if(!entry.document->save_file(entry.path.GetFullPath().mb_str())) {
		wxMessageBox("Failed to save file:\n" + entry.path.GetFullPath(), "Brush Manager", wxOK | wxICON_ERROR, this);
		return;
	}

	entry.exists = true;
	SetStatus("Saved " + entry.path.GetFullPath());

	wxString error;
	wxArrayString warnings;
	if(!g_gui.LoadVersion(g_gui.GetCurrentVersionID(), error, warnings, true)) {
		g_gui.PopupDialog("Error reloading data files", error, wxOK);
	} else if(!warnings.empty()) {
		g_gui.ListDialog("Reload warnings", warnings);
	}

	long previous = currentSelection;
	Reload();
	if(previous >= 0 && previous < static_cast<long>(entries.size())) {
		listCtrl->SetItemState(previous, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
		currentSelection = previous;
		LoadSelectedFileContent();
	}
}

void BrushManagerDialog::RefreshBrushPreview(const BrushDescriptor* descriptor)
{
	if(!descriptor) {
		editorCtrl->ChangeValue("");
		xmlContentPanel->Hide();
		spritePreview->Clear();
		spritePreview->Hide();
		xmlContentPanel->GetParent()->Layout();
		return;
	}

	uint32_t spriteId = descriptor->spriteId;
	if(spriteId == 0 && descriptor->serverId != 0) {
		const ItemType& type = g_items.getItemType(descriptor->serverId);
		spriteId = type.clientID;
	}

	bool layoutNeeded = false;
	if(spriteId != 0) {
		spritePreview->SetSprite(spriteId);
		if(!spritePreview->IsShown()) {
			spritePreview->Show();
			layoutNeeded = true;
		}
	} else {
		spritePreview->Clear();
		if(spritePreview->IsShown()) {
			spritePreview->Hide();
			layoutNeeded = true;
		}
	}

	std::ostringstream oss;
	if(descriptor->node.type() == pugi::node_comment) {
		editorCtrl->ChangeValue(wxString(descriptor->node.value(), wxConvUTF8));
	} else {
		descriptor->node.print(oss, "  ", pugi::format_indent);
		editorCtrl->ChangeValue(wxString::FromUTF8(oss.str().c_str()));
	}
	if(!xmlContentPanel->IsShown()) {
		xmlContentPanel->Show();
		layoutNeeded = true;
	}
	if(layoutNeeded) {
		xmlContentPanel->GetParent()->Layout();
	}

	wxString message;
	message << "Brush \"" << descriptor->name << "\" (" << descriptor->type << ", id " << descriptor->serverId << ")";
	SetStatus(message);
}

void BrushManagerDialog::OnRefresh(wxCommandEvent& WXUNUSED(event))
{
	Reload();
}

void BrushManagerDialog::OnListActivated(wxListEvent& WXUNUSED(event))
{
	OpenSelectedFile();
}

void BrushManagerDialog::OnSelectionChanged(wxListEvent& WXUNUSED(event))
{
	UpdateSelectionUi();
	LoadSelectedFileContent();
}

void BrushManagerDialog::OnSaveFile(wxCommandEvent& WXUNUSED(event))
{
	SaveSelectedFile();
}

void BrushManagerDialog::OnEditorText(wxCommandEvent& WXUNUSED(event))
{
	UpdateSaveButton();
}

void BrushManagerDialog::OnBrushSelection(wxDataViewEvent& event)
{
	int row = brushListCtrl->ItemToRow(event.GetItem());
	if(currentEntry && row >= 0 && row < static_cast<int>(displayedBrushes.size())) {
		size_t brushIndex = displayedBrushes[row];
		if(brushIndex < currentEntry->brushes.size()) {
			RefreshBrushPreview(&currentEntry->brushes[brushIndex]);
			return;
		}
	}
	RefreshBrushPreview(nullptr);
}

void BrushManagerDialog::OnBrushValueChanged(wxDataViewEvent& event)
{
	if(event.GetColumn() != COL_ENABLED || !currentEntry) {
		return;
	}

	int row = brushListCtrl->ItemToRow(event.GetItem());
	if(row < 0 || row >= static_cast<int>(displayedBrushes.size())) {
		return;
	}

	size_t brushIndex = displayedBrushes[row];
	if(brushIndex >= currentEntry->brushes.size()) {
		return;
	}

	bool enabled = event.GetValue().GetBool();
	SetBrushEnabled(*currentEntry, brushIndex, enabled);
}

void BrushManagerDialog::OnBrushFilterChanged(wxCommandEvent& event)
{
	brushFilter = event.GetString();
	if(currentSelection >= 0 && currentSelection < static_cast<long>(entries.size())) {
		PopulateBrushList(entries[currentSelection]);
	}
}

void BrushManagerDialog::SetBrushEnabled(BrushFileEntry& entry, size_t brushIndex, bool enabled)
{
	if(brushIndex >= entry.brushes.size()) {
		return;
	}

	BrushDescriptor& descriptor = entry.brushes[brushIndex];
	if(descriptor.enabled == enabled || !entry.document) {
		return;
	}

	if(enabled) {
		if(descriptor.node.type() == pugi::node_comment) {
			pugi::xml_document temp;
			if(temp.load(descriptor.node.value())) {
				pugi::xml_node parent = descriptor.node.parent();
				pugi::xml_node sourceNode = temp.first_child();
				if(!sourceNode) {
					return;
				}

				pugi::xml_node newNode = parent.insert_copy_after(sourceNode, descriptor.node);
				parent.remove_child(descriptor.node);
				descriptor.node = newNode;
				descriptor.serverId = ExtractServerId(newNode);
				descriptor.spriteId = ResolveSpriteId(newNode, descriptor.serverId);
			}
		} else if(descriptor.node.type() == pugi::node_element) {
			if(pugi::xml_attribute attr = descriptor.node.attribute(DISABLE_ATTRIBUTE)) {
				descriptor.node.remove_attribute(attr);
			}
		}
	} else {
		if(descriptor.node.type() == pugi::node_comment) {
			// Already disabled by comment, leave as-is.
			descriptor.enabled = false;
			if(currentEntry == &entry) {
				RefreshBrushPreview(&descriptor);
			}
			return;
		}

		if(descriptor.node.type() == pugi::node_element) {
			pugi::xml_attribute attr = descriptor.node.attribute(DISABLE_ATTRIBUTE);
			if(!attr) {
				attr = descriptor.node.append_attribute(DISABLE_ATTRIBUTE);
			}
			attr.set_value("true");
		}
	}

	descriptor.enabled = enabled;
	if(currentEntry == &entry) {
		RefreshBrushPreview(&descriptor);
	}
}
