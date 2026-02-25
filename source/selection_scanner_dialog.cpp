//////////////////////////////////////////////////////////////////////
// This file is part of Remere's Map Editor
//////////////////////////////////////////////////////////////////////

#include "main.h"

#include "selection_scanner_dialog.h"
#include "gui.h"
#include "editor.h"
#include "map.h"
#include "tile.h"
#include "selection.h"

#include <wx/wx.h>
#include <wx/clipbrd.h>
#include <sstream>
#include <algorithm>
#include <vector>

extern GUI g_gui;

void SelectionScannerDialog::Show(wxWindow* parent) {
	if(!g_gui.IsEditorOpen())
		return;

	Editor* editor = g_gui.GetCurrentEditor();
	if(!editor)
		return;

	Selection& sel = editor->getSelection();
	if(sel.empty()) {
		wxMessageBox("No tiles are selected. Select an area first.",
			"Selection Item Scanner", wxOK | wxICON_INFORMATION, parent);
		return;
	}

	const TileSet& tiles = sel.getTiles();

	// Find min/max to compute center
	int minX = INT_MAX, maxX = INT_MIN;
	int minY = INT_MAX, maxY = INT_MIN;

	for(Tile* tile : tiles) {
		if(!tile) continue;
		int tx = tile->getX();
		int ty = tile->getY();
		if(tx < minX) minX = tx;
		if(tx > maxX) maxX = tx;
		if(ty < minY) minY = ty;
		if(ty > maxY) maxY = ty;
	}

	int centerX = (minX + maxX) / 2;
	int centerY = (minY + maxY) / 2;

	// Collect tiles into a sortable vector for deterministic output
	std::vector<Tile*> sortedTiles(tiles.begin(), tiles.end());
	std::sort(sortedTiles.begin(), sortedTiles.end(),
		[](const Tile* a, const Tile* b) {
			if(a->getY() != b->getY()) return a->getY() < b->getY();
			if(a->getX() != b->getX()) return a->getX() < b->getX();
			return a->getZ() < b->getZ();
		});

	// Build XML output
	std::ostringstream os;
	int totalItems = 0;

	for(Tile* tile : sortedTiles) {
		if(!tile) continue;

		int offsetX = tile->getX() - centerX;
		int offsetY = tile->getY() - centerY;

		// Collect all item IDs on this tile
		std::vector<uint16_t> itemIds;
		if(tile->ground) {
			itemIds.push_back(tile->ground->getID());
		}
		for(Item* item : tile->items) {
			if(!item) continue;
			itemIds.push_back(item->getID());
		}

		if(itemIds.empty())
			continue;

		os << "\t\t<tile x=\"" << offsetX << "\" y=\"" << offsetY << "\">";
		for(uint16_t id : itemIds) {
			os << " <item id=\"" << id << "\" />";
		}
		os << " </tile>\n";

		totalItems += (int)itemIds.size();
	}

	std::string xmlOutput = os.str();

	if(xmlOutput.empty()) {
		wxMessageBox("No items found on selected tiles.",
			"Selection Item Scanner", wxOK | wxICON_INFORMATION, parent);
		return;
	}

	// Show dialog with results
	wxDialog dlg(parent, wxID_ANY, "Selection Item Scanner",
		wxDefaultPosition, wxDefaultSize,
		wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER);

	wxBoxSizer* topsizer = newd wxBoxSizer(wxVERTICAL);

	// Info label
	wxString infoText;
	infoText << "Scanned " << (int)sortedTiles.size() << " tiles, "
		<< totalItems << " items. "
		<< "Center: (" << centerX << ", " << centerY << ")";
	topsizer->Add(newd wxStaticText(&dlg, wxID_ANY, infoText),
		0, wxALL, 10);

	// XML text control
	wxTextCtrl* textCtrl = newd wxTextCtrl(&dlg, wxID_ANY,
		wxstr(xmlOutput), wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
	textCtrl->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
	textCtrl->SetMinSize(wxSize(600, 400));
	topsizer->Add(textCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

	// Button row
	wxBoxSizer* btnSizer = newd wxBoxSizer(wxHORIZONTAL);
	wxButton* copyBtn = newd wxButton(&dlg, wxID_COPY, "Copy to Clipboard");
	wxButton* closeBtn = newd wxButton(&dlg, wxID_CANCEL, "Close");
	btnSizer->Add(copyBtn, 0, wxRIGHT, 10);
	btnSizer->Add(closeBtn, 0);
	topsizer->Add(btnSizer, 0, wxALIGN_CENTER | wxALL, 10);

	dlg.SetSizerAndFit(topsizer);
	dlg.Centre(wxBOTH);

	// Bind copy button
	copyBtn->Bind(wxEVT_BUTTON, [&xmlOutput](wxCommandEvent&) {
		if(wxTheClipboard->Open()) {
			wxTheClipboard->SetData(newd wxTextDataObject(wxstr(xmlOutput)));
			wxTheClipboard->Close();
			g_gui.SetStatusText("XML copied to clipboard.");
		}
	});

	dlg.ShowModal();
}
