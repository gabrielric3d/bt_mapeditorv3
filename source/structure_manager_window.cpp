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
#include <wx/dcclient.h>
#include <wx/dcgraph.h>
#include <wx/filefn.h>
#include <wx/filename.h>
#include <wx/dcbuffer.h>
#include <wx/msgdlg.h>
#include <wx/popupwin.h>
#include <wx/sizer.h>
#include <wx/tokenzr.h>
#include <wx/textdlg.h>
#include <wx/treectrl.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <unordered_map>

#include "copybuffer.h"
#include "client_version.h"
#include "editor.h"
#include "filehandle.h"
#include "gui.h"
#include "iomap_otbm.h"
#include "map.h"
#include "map_display.h"
#include "map_window.h"
#include "settings.h"

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
		ID_REMOVE_CATEGORY,
		ID_REMOVE_SUBCATEGORY,
		ID_SEARCH_FILTER,
		ID_CATEGORY_TREE,
		ID_TUTORIAL_HELP
	};

	const int kTutorialWrapWidth = 360;
	const wxSize kTutorialMinSize(440, 150);
	const int kTutorialFontDelta = -1;
	const int kTutorialOverlayAlpha = 120;
	const int kTutorialHighlightAlpha = 40;

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
		Position maxPos;

		for(MapIterator it = tempMap.begin(); it != tempMap.end(); ++it) {
			Tile* tile = (*it)->get();
			if(!tile || tile->size() == 0) {
				continue;
			}

			const Position& pos = tile->getPosition();
			if(!hasPos) {
				minPos = pos;
				maxPos = pos;
				hasPos = true;
			} else {
				minPos.x = std::min(minPos.x, pos.x);
				minPos.y = std::min(minPos.y, pos.y);
				minPos.z = std::min(minPos.z, pos.z);
				maxPos.x = std::max(maxPos.x, pos.x);
				maxPos.y = std::max(maxPos.y, pos.y);
				maxPos.z = std::max(maxPos.z, pos.z);
			}
		}

		if(!hasPos) {
			errorOut = "Structure is empty.";
			return false;
		}

		Map* outRealMap = dynamic_cast<Map*>(&outMap);
		if(outRealMap) {
			outRealMap->convert(tempMap.getVersion(), false);
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

		if(outRealMap) {
			const int width = std::max(1, maxPos.x - minPos.x + 1);
			const int height = std::max(1, maxPos.y - minPos.y + 1);
			outRealMap->setWidth(width);
			outRealMap->setHeight(height);
		}

		outCopyPos = Position(0, 0, maxPos.z - minPos.z);
		return true;
	}

	wxString BuildCategoryDirectory(const wxString& baseDir, const wxString& categoryPath)
	{
		wxFileName dirName(baseDir, "");
		if(!categoryPath.empty()) {
			wxStringTokenizer tokenizer(categoryPath, "/");
			while(tokenizer.HasMoreTokens()) {
				wxString token = tokenizer.GetNextToken();
				if(!token.empty()) {
					dirName.AppendDir(token);
				}
			}
		}
		return dirName.GetFullPath();
	}

	bool CategoryNameExists(const std::vector<wxString>& paths, const wxString& name, const wxString& excludePath = wxString())
	{
		for(const auto& path : paths) {
			if(path.empty()) {
				continue;
			}
			if(!excludePath.empty() && path.CmpNoCase(excludePath) == 0) {
				continue;
			}

			wxString leaf = path;
			int sep = leaf.Find('/', true);
			if(sep != wxNOT_FOUND) {
				leaf = leaf.Mid(sep + 1);
			}
			if(leaf.CmpNoCase(name) == 0) {
				return true;
			}
		}
		return false;
	}
}

StructureManagerDialog* StructureManagerDialog::s_active = nullptr;

class StructureManagerDialog::StructurePreviewPanel : public wxPanel
{
public:
	explicit StructurePreviewPanel(wxWindow* parent) :
		wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE),
		m_mapWindow(nullptr),
		m_canvas(nullptr),
		m_emptyLabel(nullptr),
		m_hasMap(false),
		m_dragging(false)
	{
		SetMinSize(wxSize(300, 240));

		wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

		m_emptyLabel = newd wxStaticText(this, wxID_ANY, "");
		m_emptyLabel->SetForegroundColour(wxColour(140, 140, 150));
		m_emptyLabel->Hide();

		EnsureEditor();

		m_mapWindow = newd MapWindow(this, *m_editor);
		m_mapWindow->SetPreviewMode(true);
		m_canvas = m_mapWindow->GetCanvas();
		if(m_canvas) {
			m_canvas->SetPreviewMode(true);
			BindPreviewEvents();
		}

		sizer->Add(m_mapWindow, 1, wxEXPAND);
		sizer->Add(m_emptyLabel, 0, wxALIGN_CENTER | wxALL, 8);
		SetSizer(sizer);

		ShowMessage("Select a structure to preview.");
	}

	void Clear(const wxString& message)
	{
		ShowMessage(message);
	}

	bool LoadStructure(const wxString& path, wxString& error)
	{
		EnsureEditor();
		if(!m_editor) {
			error = "Preview editor unavailable.";
			ShowMessage("Preview failed to load.");
			return false;
		}

		Position copyPos;
		if(!LoadStructureFile(path, m_editor->getMap(), copyPos, error)) {
			ShowMessage("Preview failed to load.");
			return false;
		}

		m_hasMap = true;
		ShowMap();
		ResetViewToMap();
		return true;
	}

private:
	void EnsureEditor()
	{
		if(m_editor) {
			return;
		}

		MapVersion version;
		ClientVersionID currentId = g_gui.GetCurrentVersionID();
		if(currentId == CLIENT_VERSION_NONE) {
			ClientVersion* latest = ClientVersion::getLatestVersion();
			if(latest) {
				version.client = latest->getID();
				version.otbm = latest->getPrefferedMapVersionID();
			}
		} else {
			version.client = currentId;
			version.otbm = g_gui.GetCurrentVersion().getPrefferedMapVersionID();
		}

		m_editor.reset(newd Editor(m_copyBuffer, version, true));
	}

	void ShowMessage(const wxString& message)
	{
		m_hasMap = false;
		if(m_emptyLabel) {
			m_emptyLabel->SetLabel(message);
			m_emptyLabel->Show();
		}
		if(m_mapWindow) {
			m_mapWindow->Hide();
		}
		Layout();
	}

	void ShowMap()
	{
		if(m_emptyLabel) {
			m_emptyLabel->Hide();
		}
		if(m_mapWindow) {
			m_mapWindow->Show();
		}
		Layout();
	}

	void ResetViewToMap()
	{
		if(!m_editor || !m_mapWindow || !m_canvas) {
			return;
		}

		Map& map = m_editor->getMap();
		int floor = FindLowestFloor(map);
		m_canvas->ChangeFloor(floor);
		UpdateBounds(map);
		m_mapWindow->SetPreviewBounds(m_boundsWidth, m_boundsHeight);
		m_mapWindow->FitToMap();
		FitZoomToView();
		CenterMap(floor);
		m_canvas->Refresh();
	}

	void FitZoomToView()
	{
		if(!m_canvas || !m_mapWindow) {
			return;
		}

		int viewWidth = 0;
		int viewHeight = 0;
		m_mapWindow->GetViewSize(&viewWidth, &viewHeight);
		if(viewWidth <= 0 || viewHeight <= 0) {
			return;
		}

		const int mapWidth = std::max(1, m_boundsWidth);
		const int mapHeight = std::max(1, m_boundsHeight);
		const int padding = 16;
		const double usableWidth = std::max(1, viewWidth - padding);
		const double usableHeight = std::max(1, viewHeight - padding);

		double zoomX = (mapWidth * rme::TileSize) / usableWidth;
		double zoomY = (mapHeight * rme::TileSize) / usableHeight;
		double zoom = std::max(zoomX, zoomY);
		if(zoom < 1.0) {
			zoom = 1.0;
		}
		if(zoom < 0.125) {
			zoom = 0.125;
		}
		if(zoom > 25.0) {
			zoom = 25.0;
		}

		m_canvas->SetZoom(zoom);
	}

	void CenterMap(int floor)
	{
		if(!m_mapWindow) {
			return;
		}

		const int mapWidth = std::max(1, m_boundsWidth);
		const int mapHeight = std::max(1, m_boundsHeight);
		Position center(m_boundsMinX + mapWidth / 2, m_boundsMinY + mapHeight / 2, floor);
		m_mapWindow->SetScreenCenterPosition(center);
	}

	void UpdateBounds(BaseMap& map)
	{
		bool hasPos = false;
		int minX = 0;
		int minY = 0;
		int maxX = 0;
		int maxY = 0;

		for(MapIterator it = map.begin(); it != map.end(); ++it) {
			Tile* tile = (*it)->get();
			if(!tile || tile->size() == 0) {
				continue;
			}

			const Position& pos = tile->getPosition();
			if(!hasPos) {
				minX = maxX = pos.x;
				minY = maxY = pos.y;
				hasPos = true;
			} else {
				minX = std::min(minX, pos.x);
				maxX = std::max(maxX, pos.x);
				minY = std::min(minY, pos.y);
				maxY = std::max(maxY, pos.y);
			}
		}

		if(hasPos) {
			m_boundsMinX = minX;
			m_boundsMinY = minY;
			m_boundsWidth = std::max(1, maxX - minX + 1);
			m_boundsHeight = std::max(1, maxY - minY + 1);
		} else {
			m_boundsMinX = 0;
			m_boundsMinY = 0;
			m_boundsWidth = 1;
			m_boundsHeight = 1;
		}
	}

	int FindLowestFloor(BaseMap& map) const
	{
		bool hasPos = false;
		int minZ = rme::MapMaxLayer;
		for(MapIterator it = map.begin(); it != map.end(); ++it) {
			Tile* tile = (*it)->get();
			if(!tile || tile->size() == 0) {
				continue;
			}

			const int z = tile->getPosition().z;
			if(!hasPos) {
				minZ = z;
				hasPos = true;
			} else {
				minZ = std::min(minZ, z);
			}
		}
		return hasPos ? minZ : rme::MapGroundLayer;
	}

	void BindPreviewEvents()
	{
		if(!m_canvas) {
			return;
		}

		m_canvas->Bind(wxEVT_LEFT_DOWN, &StructurePreviewPanel::OnPanStart, this);
		m_canvas->Bind(wxEVT_RIGHT_DOWN, &StructurePreviewPanel::OnPanStart, this);
		m_canvas->Bind(wxEVT_LEFT_UP, &StructurePreviewPanel::OnPanEnd, this);
		m_canvas->Bind(wxEVT_RIGHT_UP, &StructurePreviewPanel::OnPanEnd, this);
		m_canvas->Bind(wxEVT_MOTION, &StructurePreviewPanel::OnPanMove, this);
		m_canvas->Bind(wxEVT_MOUSEWHEEL, &StructurePreviewPanel::OnMouseWheel, this);
		m_canvas->Bind(wxEVT_LEFT_DCLICK, &StructurePreviewPanel::OnIgnoreMouse, this);
		m_canvas->Bind(wxEVT_RIGHT_DCLICK, &StructurePreviewPanel::OnIgnoreMouse, this);
		m_canvas->Bind(wxEVT_MIDDLE_DCLICK, &StructurePreviewPanel::OnIgnoreMouse, this);
		m_canvas->Bind(wxEVT_MIDDLE_DOWN, &StructurePreviewPanel::OnIgnoreMouse, this);
		m_canvas->Bind(wxEVT_MIDDLE_UP, &StructurePreviewPanel::OnIgnoreMouse, this);
#ifdef wxEVT_AUX1_DOWN
		m_canvas->Bind(wxEVT_AUX1_DOWN, &StructurePreviewPanel::OnIgnoreMouse, this);
#endif
#ifdef wxEVT_AUX1_UP
		m_canvas->Bind(wxEVT_AUX1_UP, &StructurePreviewPanel::OnIgnoreMouse, this);
#endif
#ifdef wxEVT_AUX2_DOWN
		m_canvas->Bind(wxEVT_AUX2_DOWN, &StructurePreviewPanel::OnIgnoreMouse, this);
#endif
#ifdef wxEVT_AUX2_UP
		m_canvas->Bind(wxEVT_AUX2_UP, &StructurePreviewPanel::OnIgnoreMouse, this);
#endif
#ifdef wxEVT_AUX1_DCLICK
		m_canvas->Bind(wxEVT_AUX1_DCLICK, &StructurePreviewPanel::OnIgnoreMouse, this);
#endif
#ifdef wxEVT_AUX2_DCLICK
		m_canvas->Bind(wxEVT_AUX2_DCLICK, &StructurePreviewPanel::OnIgnoreMouse, this);
#endif
		m_canvas->Bind(wxEVT_KEY_DOWN, &StructurePreviewPanel::OnIgnoreKey, this);
		m_canvas->Bind(wxEVT_KEY_UP, &StructurePreviewPanel::OnIgnoreKey, this);
	}

	void OnPanStart(wxMouseEvent& event)
	{
		if(!m_hasMap || !m_canvas) {
			return;
		}
		m_dragging = true;
		m_lastDragPos = event.GetPosition();
		if(!m_canvas->HasCapture()) {
			m_canvas->CaptureMouse();
		}
		m_canvas->SetFocus();
	}

	void OnPanEnd(wxMouseEvent& WXUNUSED(event))
	{
		if(!m_dragging) {
			return;
		}
		m_dragging = false;
		if(m_canvas && m_canvas->HasCapture()) {
			m_canvas->ReleaseMouse();
		}
	}

	void OnPanMove(wxMouseEvent& event)
	{
		if(!m_dragging || !m_mapWindow || !m_canvas) {
			return;
		}

		wxPoint delta = event.GetPosition() - m_lastDragPos;
		m_lastDragPos = event.GetPosition();
		const double speed = g_settings.getFloat(Config::SCROLL_SPEED);
		const double zoom = m_canvas->GetZoom();
		m_mapWindow->ScrollRelative(int(speed * zoom * delta.x), int(speed * zoom * delta.y));
		m_canvas->Refresh();
	}

	void OnMouseWheel(wxMouseEvent& event)
	{
		if(!m_hasMap || !m_canvas) {
			return;
		}

		double diff = -event.GetWheelRotation() * g_settings.getFloat(Config::ZOOM_SPEED) / 640.0;
		m_canvas->ZoomBy(diff, event.GetPosition());
	}

	void OnIgnoreMouse(wxMouseEvent& WXUNUSED(event))
	{
		// Swallow events to keep preview read-only.
	}

	void OnIgnoreKey(wxKeyEvent& WXUNUSED(event))
	{
		// Swallow events to keep preview read-only.
	}

	CopyBuffer m_copyBuffer;
	std::unique_ptr<Editor> m_editor;
	MapWindow* m_mapWindow;
	MapCanvas* m_canvas;
	wxStaticText* m_emptyLabel;
	bool m_hasMap;
	bool m_dragging;
	wxPoint m_lastDragPos;
	int m_boundsMinX = 0;
	int m_boundsMinY = 0;
	int m_boundsWidth = 1;
	int m_boundsHeight = 1;
};

BEGIN_EVENT_TABLE(StructureManagerDialog, wxDialog)
	EVT_BUTTON(ID_SAVE_STRUCTURE, StructureManagerDialog::OnSaveSelection)
	EVT_BUTTON(ID_PASTE_STRUCTURE, StructureManagerDialog::OnPaste)
	EVT_BUTTON(ID_DELETE_STRUCTURE, StructureManagerDialog::OnDelete)
	EVT_CLOSE(StructureManagerDialog::OnClose)
END_EVENT_TABLE()

StructureManagerDialog::StructureManagerDialog(wxWindow* parent) :
	wxDialog(parent, wxID_ANY, "Structure Manager", wxDefaultPosition, wxSize(1080, 630),
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	s_active = this;
	CreateControls();
	LoadStructures();
	Bind(wxEVT_CHAR_HOOK, &StructureManagerDialog::OnCharHook, this);
	Bind(wxEVT_PAINT, &StructureManagerDialog::OnPaint, this);
	Bind(wxEVT_SIZE, &StructureManagerDialog::OnSize, this);
	Bind(wxEVT_MOVE, &StructureManagerDialog::OnMove, this);
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
		wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_HIDE_ROOT | wxTR_SINGLE);
	m_categoryTree->Bind(wxEVT_TREE_SEL_CHANGED, &StructureManagerDialog::OnCategoryChanged, this);
	leftSizer->Add(m_categoryTree, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	m_addCategoryButton = newd wxButton(leftPanel, ID_ADD_CATEGORY, "Add Category");
	m_addCategoryButton->Bind(wxEVT_BUTTON, &StructureManagerDialog::OnAddCategory, this);
	leftSizer->Add(m_addCategoryButton, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

	m_addSubcategoryButton = newd wxButton(leftPanel, ID_ADD_SUBCATEGORY, "Add Subcategory");
	m_addSubcategoryButton->Bind(wxEVT_BUTTON, &StructureManagerDialog::OnAddSubcategory, this);
	leftSizer->Add(m_addSubcategoryButton, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

	m_removeCategoryButton = newd wxButton(leftPanel, ID_REMOVE_CATEGORY, "Remove Category");
	m_removeCategoryButton->Bind(wxEVT_BUTTON, &StructureManagerDialog::OnRemoveCategory, this);
	leftSizer->Add(m_removeCategoryButton, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 4);

	m_removeSubcategoryButton = newd wxButton(leftPanel, ID_REMOVE_SUBCATEGORY, "Remove Subcategory");
	m_removeSubcategoryButton->Bind(wxEVT_BUTTON, &StructureManagerDialog::OnRemoveSubcategory, this);
	leftSizer->Add(m_removeSubcategoryButton, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

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
	centerPanel->SetMinSize(wxSize(260, -1));
	contentSizer->Add(centerPanel, 0, wxEXPAND | wxALL, 8);

	wxPanel* rightPanel = newd wxPanel(this);
	wxBoxSizer* rightSizer = new wxBoxSizer(wxVERTICAL);

	m_detailsText = newd wxStaticText(rightPanel, wxID_ANY, "Details: Select item...");
	rightSizer->Add(m_detailsText, 0, wxBOTTOM, 8);

	m_pasteButton = newd wxButton(rightPanel, ID_PASTE_STRUCTURE, "Paste");
	rightSizer->Add(m_pasteButton, 0, wxBOTTOM, 6);

	m_deleteButton = newd wxButton(rightPanel, ID_DELETE_STRUCTURE, "Delete");
	rightSizer->Add(m_deleteButton, 0, wxBOTTOM, 6);

	m_keepPasteCheck = newd wxCheckBox(rightPanel, wxID_ANY, "Keep paste active");
	rightSizer->Add(m_keepPasteCheck, 0);

	m_renameCategoryButton = newd wxButton(rightPanel, ID_RENAME_CATEGORY, "Rename Category...");
	rightSizer->Add(m_renameCategoryButton, 0, wxTOP, 6);
	m_renameCategoryButton->Bind(wxEVT_BUTTON, &StructureManagerDialog::OnRenameCategory, this);

	m_helpButton = newd wxButton(rightPanel, ID_TUTORIAL_HELP, "How to Use");
	rightSizer->Add(m_helpButton, 0, wxTOP, 6);
	m_helpButton->Bind(wxEVT_BUTTON, &StructureManagerDialog::OnHowToUse, this);

	wxStaticBoxSizer* previewSizer = newd wxStaticBoxSizer(wxVERTICAL, rightPanel, "Preview");
	m_previewPanel = newd StructurePreviewPanel(rightPanel);
	m_previewPanel->Clear("Select a structure to preview.");
	previewSizer->Add(m_previewPanel, 1, wxEXPAND | wxALL, 4);
	rightSizer->Add(previewSizer, 1, wxEXPAND | wxTOP, 12);

	rightPanel->SetSizer(rightSizer);
	rightPanel->SetMinSize(wxSize(360, -1));
	contentSizer->Add(rightPanel, 1, wxALL | wxEXPAND, 8);

	mainSizer->Add(contentSizer, 1, wxEXPAND);

	SetSizerAndFit(mainSizer);
	SetMinSize(wxSize(960, 540));
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
		if(path.empty()) {
			return root;
		}
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
		wxTreeItemId parentId = parentPath.empty() ? root : ensureNode(parentPath);
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
	wxTreeItemIdValue cookie;
	wxTreeItemId firstChild = m_categoryTree->GetFirstChild(root, cookie);
	wxTreeItemId child = firstChild;
	while(child.IsOk()) {
		m_categoryTree->Expand(child);
		child = m_categoryTree->GetNextChild(root, cookie);
	}
	if(firstChild.IsOk()) {
		m_categoryTree->SelectItem(firstChild);
	} else {
		m_currentCategoryPath.clear();
	}
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
	bool hasAnyCategory = false;
	if(m_categoryTree) {
		wxTreeItemId root = m_categoryTree->GetRootItem();
		if(root.IsOk()) {
			wxTreeItemIdValue cookie;
			hasAnyCategory = m_categoryTree->GetFirstChild(root, cookie).IsOk();
		}
	}

	const int selection = m_list ? m_list->GetSelection() : wxNOT_FOUND;
	const bool hasSelection = selection != wxNOT_FOUND &&
		selection >= 0 && selection < static_cast<int>(m_listEntries.size());
	if(m_saveButton) {
		m_saveButton->Enable(!m_tutorialActive && hasAnyCategory);
	}
	if(m_pasteButton) {
		m_pasteButton->Enable(!m_tutorialActive && hasSelection);
	}
	if(m_deleteButton) {
		m_deleteButton->Enable(!m_tutorialActive && hasSelection);
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
		m_renameCategoryButton->Enable(!m_tutorialActive && canRename);
	}

	const bool hasCategory = !m_currentCategoryPath.empty();
	const bool isSubcategory = hasCategory && m_currentCategoryPath.Find('/') != wxNOT_FOUND;
	if(m_addSubcategoryButton) {
		m_addSubcategoryButton->Enable(!m_tutorialActive && hasCategory);
	}
	if(m_removeCategoryButton) {
		m_removeCategoryButton->Enable(!m_tutorialActive && hasCategory && !isSubcategory);
	}
	if(m_removeSubcategoryButton) {
		m_removeSubcategoryButton->Enable(!m_tutorialActive && isSubcategory);
	}

	UpdatePreview(GetSelectedEntry(nullptr));
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

wxString StructureManagerDialog::GetSelectedCategoryPath() const
{
	if(!m_categoryTree) {
		return m_currentCategoryPath;
	}

	wxTreeItemId item = m_categoryTree->GetSelection();
	if(!item.IsOk()) {
		return m_currentCategoryPath;
	}

	wxTreeItemId root = m_categoryTree->GetRootItem();
	if(!root.IsOk()) {
		return m_currentCategoryPath;
	}

	wxArrayString parts;
	wxTreeItemId current = item;
	while(current.IsOk() && current != root) {
		parts.Add(m_categoryTree->GetItemText(current));
		current = m_categoryTree->GetItemParent(current);
	}

	wxString path;
	for(int i = static_cast<int>(parts.size()) - 1; i >= 0; --i) {
		if(!path.empty()) {
			path += "/";
		}
		path += parts[static_cast<size_t>(i)];
	}

	return path;
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

void StructureManagerDialog::UpdatePreview(const StructureEntry* entry)
{
	if(!m_previewPanel) {
		return;
	}

	if(!entry) {
		m_previewPanel->Clear("Select a structure to preview.");
		m_previewPath.clear();
		return;
	}

	if(entry->path == m_previewPath) {
		return;
	}

	wxString error;
	if(!m_previewPanel->LoadStructure(entry->path, error)) {
		m_previewPanel->Clear("Preview failed to load.");
		m_previewPath.clear();
		return;
	}

	m_previewPath = entry->path;
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
	if(CategoryNameExists(m_categoryPaths, safeName, m_currentCategoryPath)) {
		wxMessageBox("A category with this name already exists.", "Structure Manager", wxOK | wxICON_INFORMATION, this);
		return;
	}

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
	const wxString categoryPath = GetSelectedCategoryPath();
	wxString dir = BuildCategoryDirectory(m_baseDir, categoryPath);
	wxFileName dirName(dir, "");
	if(!dirName.DirExists()) {
		if(!dirName.Mkdir(wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
			wxMessageBox("Failed to create category directory.", "Structure Manager", wxOK | wxICON_ERROR, this);
			return;
		}
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
		parentPath = m_currentCategoryPath;
		if(parentPath.empty()) {
			wxMessageBox("Select a category to add a subcategory.", "Structure Manager", wxOK | wxICON_INFORMATION, this);
			return;
		}
		int sep = parentPath.Find('/', true);
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
	if(CategoryNameExists(m_categoryPaths, safeName)) {
		wxMessageBox("A category with this name already exists.", "Structure Manager", wxOK | wxICON_INFORMATION, this);
		return;
	}

	wxString newRelativePath = parentPath.empty() ? safeName : (parentPath + "/" + safeName);
	wxString parentDir = parentPath.empty() ? m_baseDir : wxFileName(m_baseDir, parentPath).GetFullPath();
	wxString newDir = wxFileName(parentDir, safeName).GetFullPath();

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

void StructureManagerDialog::OnRemoveCategory(wxCommandEvent& WXUNUSED(event))
{
	if(m_currentCategoryPath.empty()) {
		wxMessageBox("Select a category to remove.", "Structure Manager", wxOK | wxICON_INFORMATION, this);
		return;
	}
	if(m_currentCategoryPath.Find('/') != wxNOT_FOUND) {
		wxMessageBox("Select a top-level category to remove.", "Structure Manager", wxOK | wxICON_INFORMATION, this);
		return;
	}

	const wxString fullPath = wxFileName(m_baseDir, m_currentCategoryPath).GetFullPath();
	if(!wxDirExists(fullPath)) {
		wxMessageBox("Category folder not found.", "Structure Manager", wxOK | wxICON_ERROR, this);
		return;
	}

	int ret = wxMessageBox("Remove category '" + m_currentCategoryPath + "' and all its structures?",
		"Structure Manager", wxYES_NO | wxICON_WARNING, this);
	if(ret != wxYES) {
		return;
	}

	if(!wxFileName::Rmdir(fullPath, wxPATH_RMDIR_RECURSIVE)) {
		wxMessageBox("Failed to remove category.", "Structure Manager", wxOK | wxICON_ERROR, this);
		return;
	}

	LoadStructures();
	SelectCategoryByPath("");
	SetStatusText("Category removed.");
	g_gui.SetStatusText("Category removed.");
}

void StructureManagerDialog::OnRemoveSubcategory(wxCommandEvent& WXUNUSED(event))
{
	if(m_currentCategoryPath.empty() || m_currentCategoryPath.Find('/') == wxNOT_FOUND) {
		wxMessageBox("Select a subcategory to remove.", "Structure Manager", wxOK | wxICON_INFORMATION, this);
		return;
	}

	const wxString fullPath = wxFileName(m_baseDir, m_currentCategoryPath).GetFullPath();
	if(!wxDirExists(fullPath)) {
		wxMessageBox("Subcategory folder not found.", "Structure Manager", wxOK | wxICON_ERROR, this);
		return;
	}

	int ret = wxMessageBox("Remove subcategory '" + m_currentCategoryPath + "' and all its structures?",
		"Structure Manager", wxYES_NO | wxICON_WARNING, this);
	if(ret != wxYES) {
		return;
	}

	if(!wxFileName::Rmdir(fullPath, wxPATH_RMDIR_RECURSIVE)) {
		wxMessageBox("Failed to remove subcategory.", "Structure Manager", wxOK | wxICON_ERROR, this);
		return;
	}

	wxString parentPath = m_currentCategoryPath;
	int sep = parentPath.Find('/', true);
	if(sep != wxNOT_FOUND) {
		parentPath = parentPath.Left(sep);
	} else {
		parentPath.clear();
	}

	LoadStructures();
	SelectCategoryByPath(parentPath);
	SetStatusText("Subcategory removed.");
	g_gui.SetStatusText("Subcategory removed.");
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

void StructureManagerDialog::OnHowToUse(wxCommandEvent& WXUNUSED(event))
{
	StartTutorial();
}

void StructureManagerDialog::StartTutorial()
{
	m_tutorialActive = true;
	m_tutorialStep = 0;
	m_tutorialLockMove = true;
	m_tutorialLockPos = GetPosition();
	SetTutorialUiEnabled(false);
	SetTutorialUiEnabled(false);

	if(!m_tutorialPopup) {
		m_tutorialPopup = newd wxPopupWindow(this, wxBORDER_SIMPLE);
		wxPanel* panel = newd wxPanel(m_tutorialPopup, wxID_ANY);
		panel->SetBackgroundColour(wxColour(30, 30, 30));
		panel->SetForegroundColour(*wxWHITE);

		m_tutorialStepLabel = newd wxStaticText(panel, wxID_ANY, "");
		m_tutorialStepLabel->SetForegroundColour(*wxWHITE);
		wxFont bold = m_tutorialStepLabel->GetFont();
		bold.SetWeight(wxFONTWEIGHT_BOLD);
		bold.SetPointSize(std::max(6, bold.GetPointSize() + kTutorialFontDelta));
		m_tutorialStepLabel->SetFont(bold);

		m_tutorialBodyText = newd wxStaticText(panel, wxID_ANY, "");
		m_tutorialBodyText->SetForegroundColour(*wxWHITE);
		wxFont bodyFont = m_tutorialBodyText->GetFont();
		bodyFont.SetPointSize(std::max(6, bodyFont.GetPointSize() + kTutorialFontDelta));
		m_tutorialBodyText->SetFont(bodyFont);
		m_tutorialBodyText->Wrap(kTutorialWrapWidth);

		m_tutorialPrevButton = newd wxButton(panel, wxID_ANY, "Back");
		m_tutorialNextButton = newd wxButton(panel, wxID_ANY, "Next");
		m_tutorialCloseButton = newd wxButton(panel, wxID_ANY, "Close");

		wxFont buttonFont = m_tutorialPrevButton->GetFont();
		buttonFont.SetPointSize(std::max(6, buttonFont.GetPointSize() + kTutorialFontDelta));
		m_tutorialPrevButton->SetFont(buttonFont);
		m_tutorialNextButton->SetFont(buttonFont);
		m_tutorialCloseButton->SetFont(buttonFont);

		m_tutorialPrevButton->Bind(wxEVT_BUTTON, &StructureManagerDialog::OnTutorialPrev, this);
		m_tutorialNextButton->Bind(wxEVT_BUTTON, &StructureManagerDialog::OnTutorialNext, this);
		m_tutorialCloseButton->Bind(wxEVT_BUTTON, &StructureManagerDialog::OnTutorialClose, this);

		wxBoxSizer* navSizer = new wxBoxSizer(wxHORIZONTAL);
		navSizer->Add(m_tutorialPrevButton, 0, wxRIGHT, 4);
		navSizer->Add(m_tutorialNextButton, 0, wxRIGHT, 4);
		navSizer->Add(m_tutorialCloseButton, 0);

		wxBoxSizer* panelSizer = new wxBoxSizer(wxVERTICAL);
		panelSizer->Add(m_tutorialStepLabel, 0, wxBOTTOM, 4);
		panelSizer->Add(m_tutorialBodyText, 0, wxBOTTOM, 8);
		panelSizer->AddStretchSpacer(1);
		panelSizer->Add(navSizer, 0, wxALIGN_RIGHT | wxBOTTOM, 4);

		wxBoxSizer* outerSizer = new wxBoxSizer(wxVERTICAL);
		outerSizer->Add(panelSizer, 1, wxEXPAND | wxALL, 8);
		panel->SetSizerAndFit(outerSizer);
		panel->SetMinSize(kTutorialMinSize);

		wxBoxSizer* popupSizer = new wxBoxSizer(wxVERTICAL);
		popupSizer->Add(panel, 1, wxEXPAND);
		m_tutorialPopup->SetSizerAndFit(popupSizer);
		m_tutorialPopup->SetMinSize(kTutorialMinSize);
	}

	m_tutorialPopup->Show();
	m_tutorialPopup->Raise();
	UpdateTutorialStep();
}

void StructureManagerDialog::StopTutorial()
{
	m_tutorialActive = false;
	m_tutorialLockMove = false;
	ClearTutorialOverlay();
	if(m_tutorialPopup) {
		m_tutorialPopup->Hide();
	}
	SetTutorialUiEnabled(true);
	UpdateSelectionUi();
}

void StructureManagerDialog::UpdateTutorialStep()
{
	if(!m_tutorialActive) {
		return;
	}

	const int count = GetTutorialStepCount();
	if(m_tutorialStep < 0) {
		m_tutorialStep = 0;
	} else if(m_tutorialStep >= count) {
		m_tutorialStep = count - 1;
	}

	const TutorialStepInfo info = GetTutorialStepInfo(m_tutorialStep);
	if(m_tutorialStepLabel) {
		m_tutorialStepLabel->SetLabel(wxString::Format("Step %d/%d - %s", m_tutorialStep + 1, count, info.title));
	}
	if(m_tutorialBodyText) {
		m_tutorialBodyText->SetLabel(info.body);
		m_tutorialBodyText->Wrap(kTutorialWrapWidth);
	}

	if(m_tutorialPrevButton) {
		m_tutorialPrevButton->Enable(m_tutorialStep > 0);
	}
	if(m_tutorialNextButton) {
		m_tutorialNextButton->SetLabel(m_tutorialStep + 1 < count ? "Next" : "Finish");
	}

	if(m_tutorialPopup) {
		m_tutorialPopup->Fit();
		wxSize desired = m_tutorialPopup->GetSize();
		desired.SetWidth(std::max(desired.x, kTutorialMinSize.x));
		desired.SetHeight(std::max(desired.y, kTutorialMinSize.y));
		m_tutorialPopup->SetSize(desired);
	}
	PositionTutorialPopup();
	RenderTutorialOverlay();
}

void StructureManagerDialog::RenderTutorialOverlay()
{
	if(!m_tutorialActive) {
		return;
	}

	wxClientDC dc(this);
	wxDCOverlay overlaydc(m_tutorialOverlay, &dc);
	overlaydc.Clear();

	wxRect highlight = GetTutorialHighlightRect();
	wxSize clientSize = GetClientSize();
	wxGCDC gcdc(dc);
	const wxColour overlayColor(0, 0, 0, kTutorialOverlayAlpha);
	const wxColour highlightShade(0, 0, 0, kTutorialHighlightAlpha);
	gcdc.SetPen(*wxTRANSPARENT_PEN);

	if(highlight.IsEmpty()) {
		gcdc.SetBrush(wxBrush(overlayColor));
		gcdc.DrawRectangle(0, 0, clientSize.x, clientSize.y);
		return;
	}

	wxRect glowRect = highlight;
	glowRect.Inflate(4);
	glowRect.Intersect(wxRect(0, 0, clientSize.x, clientSize.y));

	auto drawRect = [&](int x, int y, int w, int h) {
		if(w > 0 && h > 0) {
			gcdc.DrawRectangle(x, y, w, h);
		}
	};

	const int left = glowRect.x;
	const int top = glowRect.y;
	const int right = glowRect.x + glowRect.width;
	const int bottom = glowRect.y + glowRect.height;

	gcdc.SetBrush(wxBrush(overlayColor));
	drawRect(0, 0, clientSize.x, top);
	drawRect(0, top, left, clientSize.y - top);
	drawRect(right, top, clientSize.x - right, clientSize.y - top);
	drawRect(left, bottom, right - left, clientSize.y - bottom);

	gcdc.SetBrush(wxBrush(highlightShade));
	drawRect(left, top, right - left, bottom - top);

	gcdc.SetBrush(*wxTRANSPARENT_BRUSH);
	gcdc.SetPen(wxPen(wxColour(255, 205, 80), 3));
	gcdc.DrawRectangle(glowRect);
}

void StructureManagerDialog::ClearTutorialOverlay()
{
	wxClientDC dc(this);
	wxDCOverlay overlaydc(m_tutorialOverlay, &dc);
	overlaydc.Clear();
	m_tutorialOverlay.Reset();
}

void StructureManagerDialog::PositionTutorialPopup()
{
	if(!m_tutorialPopup) {
		return;
	}

	wxSize clientSize = GetClientSize();
	wxPoint clientOrigin = ClientToScreen(wxPoint(0, 0));
	wxRect clientScreen(clientOrigin, clientSize);

	wxRect highlight = GetTutorialHighlightRect();
	wxRect highlightScreen = highlight;
	highlightScreen.Offset(clientOrigin);

	wxSize popupSize = m_tutorialPopup->GetSize();
	const int margin = 12;
	wxPoint pos(0, 0);

	if(highlight.IsEmpty()) {
		pos.x = clientScreen.x + (clientScreen.width - popupSize.x) / 2;
		pos.y = clientScreen.y + (clientScreen.height - popupSize.y) / 2;
	} else if(highlightScreen.GetRight() + margin + popupSize.x < clientScreen.x + clientScreen.width) {
		pos.x = highlightScreen.GetRight() + margin;
		pos.y = highlightScreen.GetTop();
	} else if(highlightScreen.GetLeft() - margin - popupSize.x > clientScreen.x) {
		pos.x = highlightScreen.GetLeft() - margin - popupSize.x;
		pos.y = highlightScreen.GetTop();
	} else if(highlightScreen.GetBottom() + margin + popupSize.y < clientScreen.y + clientScreen.height) {
		pos.x = highlightScreen.GetLeft();
		pos.y = highlightScreen.GetBottom() + margin;
	} else {
		pos.x = highlightScreen.GetLeft();
		pos.y = std::max(clientScreen.y + margin, highlightScreen.GetTop() - margin - popupSize.y);
	}

	if(pos.x + popupSize.x > clientScreen.x + clientScreen.width) {
		pos.x = std::max(clientScreen.x + margin, clientScreen.x + clientScreen.width - popupSize.x - margin);
	}
	if(pos.y + popupSize.y > clientScreen.y + clientScreen.height) {
		pos.y = std::max(clientScreen.y + margin, clientScreen.y + clientScreen.height - popupSize.y - margin);
	}
	if(pos.x < clientScreen.x + margin) {
		pos.x = clientScreen.x + margin;
	}
	if(pos.y < clientScreen.y + margin) {
		pos.y = clientScreen.y + margin;
	}

	m_tutorialPopup->Move(pos);
}

wxRect StructureManagerDialog::GetTutorialHighlightRect() const
{
	wxWindow* target = nullptr;
	switch(m_tutorialStep) {
		case 0:
			target = m_addCategoryButton;
			break;
		case 1:
			target = m_saveButton;
			break;
		case 2:
			target = m_list;
			break;
		case 3:
			target = m_pasteButton;
			break;
		default:
			break;
	}

	if(!target || !target->IsShownOnScreen()) {
		return wxRect();
	}

	wxPoint screenTopLeft = target->ClientToScreen(wxPoint(0, 0));
	wxPoint clientTopLeft = ScreenToClient(screenTopLeft);
	return wxRect(clientTopLeft, target->GetSize());
}

StructureManagerDialog::TutorialStepInfo StructureManagerDialog::GetTutorialStepInfo(int step) const
{
	switch(step) {
		case 0:
			return {"Create a category", "Click \"Add Category\" to create your first category."};
		case 1:
			return {"Save a structure", "On the map, select the tiles you want and click \"Save Current Selection...\"."};
		case 2:
			return {"Choose a structure", "Select a structure in the list to view its details."};
		case 3:
			return {"Paste on the map", "Click \"Paste\" to place the structure on the map."};
		default:
			return {"Tutorial", "Use the buttons to navigate."};
	}
}

int StructureManagerDialog::GetTutorialStepCount() const
{
	return 4;
}

void StructureManagerDialog::SetTutorialUiEnabled(bool enabled)
{
	if(m_searchCtrl) {
		m_searchCtrl->Enable(enabled);
	}
	if(m_categoryTree) {
		m_categoryTree->Enable(enabled);
	}
	if(m_list) {
		m_list->Enable(enabled);
	}
	if(m_keepPasteCheck) {
		m_keepPasteCheck->Enable(enabled);
	}
	if(m_addCategoryButton) {
		m_addCategoryButton->Enable(enabled);
	}
	if(m_addSubcategoryButton) {
		m_addSubcategoryButton->Enable(enabled);
	}
	if(m_removeCategoryButton) {
		m_removeCategoryButton->Enable(enabled);
	}
	if(m_removeSubcategoryButton) {
		m_removeSubcategoryButton->Enable(enabled);
	}
	if(m_renameCategoryButton) {
		m_renameCategoryButton->Enable(enabled);
	}
	if(m_saveButton) {
		m_saveButton->Enable(enabled);
	}
	if(m_pasteButton) {
		m_pasteButton->Enable(enabled);
	}
	if(m_deleteButton) {
		m_deleteButton->Enable(enabled);
	}
	if(m_helpButton) {
		m_helpButton->Enable(enabled);
	}
}

void StructureManagerDialog::OnTutorialPrev(wxCommandEvent& WXUNUSED(event))
{
	if(m_tutorialStep > 0) {
		--m_tutorialStep;
		UpdateTutorialStep();
	}
}

void StructureManagerDialog::OnTutorialNext(wxCommandEvent& WXUNUSED(event))
{
	if(m_tutorialStep + 1 < GetTutorialStepCount()) {
		++m_tutorialStep;
		UpdateTutorialStep();
	} else {
		StopTutorial();
	}
}

void StructureManagerDialog::OnTutorialClose(wxCommandEvent& WXUNUSED(event))
{
	StopTutorial();
}

void StructureManagerDialog::OnPaint(wxPaintEvent& event)
{
	event.Skip();
	if(m_tutorialActive) {
		RenderTutorialOverlay();
	}
}

void StructureManagerDialog::OnSize(wxSizeEvent& event)
{
	event.Skip();
	if(m_tutorialActive) {
		ClearTutorialOverlay();
		PositionTutorialPopup();
		RenderTutorialOverlay();
	}
}

void StructureManagerDialog::OnMove(wxMoveEvent& event)
{
	if(m_tutorialActive && m_tutorialLockMove) {
		if(!m_tutorialMoveGuard) {
			m_tutorialMoveGuard = true;
			Move(m_tutorialLockPos);
			m_tutorialMoveGuard = false;
		}
		return;
	}

	event.Skip();
	if(m_tutorialActive) {
		ClearTutorialOverlay();
		PositionTutorialPopup();
		RenderTutorialOverlay();
	}
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
	StopTutorial();
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
	if(m_tutorialActive) {
		return false;
	}
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
