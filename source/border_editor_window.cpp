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
#include "border_editor_window.h"
#include "browse_tile_window.h"
#include "find_item_window.h"
#include "common_windows.h"
#include "graphics.h"
#include "gui.h"
#include "artprovider.h"
#include "items.h"
#include "brush.h"
#include "ground_brush.h"
#include "raw_brush.h"
#include <wx/sizer.h>
#include <wx/gbsizer.h>
#include <wx/statline.h>
#include <wx/tglbtn.h>
#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/filepicker.h>
#include <wx/dnd.h>
#include <wx/listctrl.h>
#include <sstream>
#include <algorithm>

#define BORDER_GRID_SIZE 32
#define BORDER_PREVIEW_SIZE 192
#define BORDER_GRID_CELL_SIZE 32
#define ID_BORDER_GRID_SELECT wxID_HIGHEST + 1
#define ID_GROUND_ITEM_LIST wxID_HIGHEST + 2

// Utility functions for edge string/position conversion
BorderEdgePosition edgeStringToPosition(const std::string& edgeStr) {
    if (edgeStr == "n") return EDGE_N;
    if (edgeStr == "e") return EDGE_E;
    if (edgeStr == "s") return EDGE_S;
    if (edgeStr == "w") return EDGE_W;
    if (edgeStr == "cnw") return EDGE_CNW;
    if (edgeStr == "cne") return EDGE_CNE;
    if (edgeStr == "cse") return EDGE_CSE;
    if (edgeStr == "csw") return EDGE_CSW;
    if (edgeStr == "dnw") return EDGE_DNW;
    if (edgeStr == "dne") return EDGE_DNE;
    if (edgeStr == "dse") return EDGE_DSE;
    if (edgeStr == "dsw") return EDGE_DSW;
    return EDGE_NONE;
}

std::string edgePositionToString(BorderEdgePosition pos) {
    switch (pos) {
        case EDGE_N: return "n";
        case EDGE_E: return "e";
        case EDGE_S: return "s";
        case EDGE_W: return "w";
        case EDGE_CNW: return "cnw";
        case EDGE_CNE: return "cne";
        case EDGE_CSE: return "cse";
        case EDGE_CSW: return "csw";
        case EDGE_DNW: return "dnw";
        case EDGE_DNE: return "dne";
        case EDGE_DSE: return "dse";
        case EDGE_DSW: return "dsw";
        default: return "";
    }
}

wxString GetVersionDataDirectory() {
    const ClientVersion& version = g_gui.GetCurrentVersion();
    FileName data_path = version.getDataPath();
    return data_path.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);
}

static std::string TrimAscii(const std::string& value) {
    size_t start = value.find_first_not_of(" \t\r\n");
    if(start == std::string::npos) {
        return std::string();
    }

    size_t end = value.find_last_not_of(" \t\r\n");
    return value.substr(start, end - start + 1);
}

static bool ParseBorderIdList(const wxString& text, std::vector<int>& outIds) {
    outIds.clear();
    std::stringstream ss(nstr(text));
    std::string token;
    while(std::getline(ss, token, ',')) {
        token = TrimAscii(token);
        if(token.empty()) {
            continue;
        }

        try {
            size_t consumed = 0;
            int id = std::stoi(token, &consumed);
            if(consumed != token.size() || id <= 0 || id > 65535) {
                return false;
            }
            outIds.push_back(id);
        } catch(...) {
            return false;
        }
    }

    return !outIds.empty();
}

static void AppendUniqueBorderId(std::vector<int>& ids, int id) {
    if(id <= 0) {
        return;
    }
    if(std::find(ids.begin(), ids.end(), id) != ids.end()) {
        return;
    }
    ids.push_back(id);
}

static wxString JoinBorderIdList(const std::vector<int>& ids) {
    wxString out;
    for(size_t i = 0; i < ids.size(); ++i) {
        if(i > 0) {
            out += ", ";
        }
        out += wxString::Format("%d", ids[i]);
    }
    return out;
}

static std::vector<int> ReorderBorderIdsWithPrimary(const std::vector<int>& ids, int primaryId) {
    std::vector<int> reordered;
    if(primaryId > 0) {
        reordered.push_back(primaryId);
    }
    for(size_t i = 0; i < ids.size(); ++i) {
        if(ids[i] > 0 && ids[i] != primaryId) {
            AppendUniqueBorderId(reordered, ids[i]);
        }
    }
    return reordered;
}

static wxBitmap BuildSmallBorderPreviewBitmap(uint16_t previewItemId, int width = 32, int height = 32) {
    wxBitmap bitmap(width, height, 32);
    wxMemoryDC dc(bitmap);
    dc.SetBackground(wxBrush(wxColour(24, 28, 34)));
    dc.Clear();
    dc.SetPen(wxPen(wxColour(70, 80, 95)));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.DrawRectangle(0, 0, width, height);

    bool drewSprite = false;
    if(previewItemId > 0) {
        const ItemType& type = g_items.getItemType(previewItemId);
        if(type.id != 0) {
            Sprite* sprite = g_gui.gfx.getSprite(type.clientID);
            if(sprite) {
                sprite->DrawTo(&dc, SPRITE_SIZE_32x32, 0, 0, width, height);
                drewSprite = true;
            }
        }
    }

    if(!drewSprite) {
        dc.SetPen(wxPen(wxColour(115, 130, 150)));
        dc.DrawLine(4, 4, width - 4, height - 4);
        dc.DrawLine(width - 4, 4, 4, height - 4);
    }

    dc.SelectObject(wxNullBitmap);
    return bitmap;
}

static wxString BuildBorderReferenceLabel(const pugi::xml_node& borderNode) {
    wxString align = "outer";
    wxString toValue = "default";
    wxString ids = "";
    wxString borderlist = "";

    if(borderNode.attribute("align")) {
        align = wxString(borderNode.attribute("align").as_string());
    }
    if(borderNode.attribute("to")) {
        toValue = wxString(borderNode.attribute("to").as_string());
    }
    if(borderNode.attribute("id")) {
        ids = wxString(borderNode.attribute("id").as_string());
    }
    if(borderNode.attribute("borderlist")) {
        borderlist = wxString(borderNode.attribute("borderlist").as_string());
    }

    if(!borderlist.IsEmpty()) {
        return wxString::Format("align=%s  to=%s  id=%s  borderlist=%s", align.c_str(), toValue.c_str(), ids.c_str(), borderlist.c_str());
    }

    return wxString::Format("align=%s  to=%s  id=%s", align.c_str(), toValue.c_str(), ids.c_str());
}

static wxString NormalizeSingleLine(wxString text) {
    text.Replace("\r", " ");
    text.Replace("\n", " ");
    text.Replace("\t", " ");
    while(text.Find("  ") != wxNOT_FOUND) {
        text.Replace("  ", " ");
    }
    text.Trim(true).Trim(false);
    return text;
}

static wxString ExtractBorderCommentDescription(const pugi::xml_node& borderNode) {
    std::string description;
    pugi::xml_node commentNode = borderNode.previous_sibling();
    if(commentNode && commentNode.type() == pugi::node_comment) {
        description = commentNode.value();
        description = description.c_str();

        size_t first = description.find_first_not_of(" \t\n\r");
        if(first == std::string::npos) {
            description.clear();
        } else {
            description.erase(0, first);
            size_t last = description.find_last_not_of(" \t\n\r");
            if(last != std::string::npos) {
                description.erase(last + 1);
            }
        }

        if(description.substr(0, 4) == "<!--") {
            description.erase(0, 4);
            size_t trimLeft = description.find_first_not_of(" \t\n\r");
            if(trimLeft == std::string::npos) {
                description.clear();
            } else {
                description.erase(0, trimLeft);
            }
        }

        if(description.length() >= 3 && description.substr(description.length() - 3) == "-->") {
            description.erase(description.length() - 3);
            size_t trimRight = description.find_last_not_of(" \t\n\r");
            if(trimRight != std::string::npos) {
                description.erase(trimRight + 1);
            } else {
                description.clear();
            }
        }
    }

    return NormalizeSingleLine(wxstr(description));
}

class BorderListSlotEditorDialog : public wxDialog {
public:
    struct BorderEntry {
        int id;
        wxString description;
        wxString label;
        uint16_t previewItemId;
    };

    BorderListSlotEditorDialog(wxWindow* parent, const std::vector<int>& currentSlots, const std::vector<BorderEntry>& entries) :
        wxDialog(parent, wxID_ANY, "Border List Slot Editor", wxDefaultPosition, wxSize(880, 520), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
        m_slots(currentSlots),
        m_entries(entries),
        m_filterCtrl(nullptr),
        m_slotList(nullptr),
        m_availableList(nullptr),
        m_availableImageList(nullptr),
        m_prevPageButton(nullptr),
        m_nextPageButton(nullptr),
        m_pageInfoLabel(nullptr),
        m_totalPages(1),
        m_currentPage(0),
        m_pageSize(100) {

        wxBoxSizer* rootSizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* header = new wxStaticText(
            this,
            wxID_ANY,
            "Left: click a slot to type/edit ID. Right: available borders from borders.xml with search + pagination."
        );
        rootSizer->Add(header, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 10);

        wxBoxSizer* contentSizer = new wxBoxSizer(wxHORIZONTAL);

        wxStaticBoxSizer* slotsSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Border List Slots");
        m_slotList = new wxListBox(this, wxID_ANY);
        m_slotList->SetMinSize(wxSize(280, 320));
        m_slotList->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        slotsSizer->Add(m_slotList, 1, wxEXPAND | wxALL, 8);

        wxBoxSizer* slotButtonsSizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton* addSlotButton = new wxButton(this, wxID_ANY, "Add Slot", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        wxButton* editSlotButton = new wxButton(this, wxID_ANY, "Edit Slot", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        wxButton* removeSlotButton = new wxButton(this, wxID_ANY, "Remove Slot", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        slotButtonsSizer->Add(addSlotButton, 0, wxRIGHT, 6);
        slotButtonsSizer->Add(editSlotButton, 0, wxRIGHT, 6);
        slotButtonsSizer->Add(removeSlotButton, 0);
        slotsSizer->Add(slotButtonsSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
        contentSizer->Add(slotsSizer, 0, wxEXPAND | wxALL, 8);

        wxStaticBoxSizer* catalogSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Available Borders (borders.xml)");
        m_filterCtrl = new wxTextCtrl(this, wxID_ANY);
        m_filterCtrl->SetHint("Filter by border id or description...");
        catalogSizer->Add(m_filterCtrl, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 8);

        m_availableList = new wxListCtrl(
            this,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxBORDER_SUNKEN
        );
        m_availableList->SetMinSize(wxSize(430, 320));
        m_availableList->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        m_availableList->AppendColumn("Border Preview / ID", wxLIST_FORMAT_LEFT, 420);
        m_availableImageList = new wxImageList(32, 32, true);
        m_availableList->AssignImageList(m_availableImageList, wxIMAGE_LIST_SMALL);
        catalogSizer->Add(m_availableList, 1, wxEXPAND | wxALL, 8);

        wxBoxSizer* paginationSizer = new wxBoxSizer(wxHORIZONTAL);
        m_prevPageButton = new wxButton(this, wxID_ANY, "< Prev", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        m_nextPageButton = new wxButton(this, wxID_ANY, "Next >", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
        m_pageInfoLabel = new wxStaticText(this, wxID_ANY, "Page 1/1");
        paginationSizer->Add(m_prevPageButton, 0, wxRIGHT, 8);
        paginationSizer->Add(m_pageInfoLabel, 1, wxALIGN_CENTER_VERTICAL);
        paginationSizer->Add(m_nextPageButton, 0, wxLEFT, 8);
        catalogSizer->Add(paginationSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);

        wxBoxSizer* applySizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton* applyToSlotButton = new wxButton(this, wxID_ANY, "Apply To Selected Slot");
        wxButton* appendSlotButton = new wxButton(this, wxID_ANY, "Append As New Slot");
        applySizer->Add(applyToSlotButton, 0, wxRIGHT, 8);
        applySizer->Add(appendSlotButton, 0);
        catalogSizer->Add(applySizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
        contentSizer->Add(catalogSizer, 1, wxEXPAND | wxALL, 8);

        rootSizer->Add(contentSizer, 1, wxEXPAND);

        wxStdDialogButtonSizer* dialogButtons = new wxStdDialogButtonSizer();
        wxButton* okButton = new wxButton(this, wxID_OK, "Use Slots");
        wxButton* cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
        dialogButtons->AddButton(okButton);
        dialogButtons->AddButton(cancelButton);
        dialogButtons->Realize();
        rootSizer->Add(dialogButtons, 0, wxEXPAND | wxALL, 10);

        SetSizer(rootSizer);

        if(m_slotList) {
            m_slotList->Bind(wxEVT_LISTBOX_DCLICK, &BorderListSlotEditorDialog::OnEditSelectedSlot, this);
        }
        if(m_filterCtrl) {
            m_filterCtrl->Bind(wxEVT_TEXT, &BorderListSlotEditorDialog::OnFilterChanged, this);
        }
        if(m_availableList) {
            m_availableList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &BorderListSlotEditorDialog::OnAvailableItemActivated, this);
            m_availableList->Bind(wxEVT_LIST_ITEM_SELECTED, &BorderListSlotEditorDialog::OnAvailableItemSelected, this);
        }
        if(addSlotButton) {
            addSlotButton->Bind(wxEVT_BUTTON, &BorderListSlotEditorDialog::OnAddSlot, this);
        }
        if(editSlotButton) {
            editSlotButton->Bind(wxEVT_BUTTON, &BorderListSlotEditorDialog::OnEditSelectedSlot, this);
        }
        if(removeSlotButton) {
            removeSlotButton->Bind(wxEVT_BUTTON, &BorderListSlotEditorDialog::OnRemoveSelectedSlot, this);
        }
        if(m_prevPageButton) {
            m_prevPageButton->Bind(wxEVT_BUTTON, &BorderListSlotEditorDialog::OnPrevPage, this);
        }
        if(m_nextPageButton) {
            m_nextPageButton->Bind(wxEVT_BUTTON, &BorderListSlotEditorDialog::OnNextPage, this);
        }
        if(applyToSlotButton) {
            applyToSlotButton->Bind(wxEVT_BUTTON, &BorderListSlotEditorDialog::OnApplyToSlot, this);
        }
        if(appendSlotButton) {
            appendSlotButton->Bind(wxEVT_BUTTON, &BorderListSlotEditorDialog::OnAppendSlotFromAvailable, this);
        }
        Bind(wxEVT_BUTTON, &BorderListSlotEditorDialog::OnConfirm, this, wxID_OK);

        RefreshSlotsList();
        RebuildAvailableList();
        if(m_filterCtrl) {
            m_filterCtrl->SetFocus();
        }
        CenterOnParent();
    }

    std::vector<int> GetSlotIds() const {
        std::vector<int> clean;
        for(size_t i = 0; i < m_slots.size(); ++i) {
            if(m_slots[i] > 0) {
                clean.push_back(m_slots[i]);
            }
        }
        return clean;
    }

private:
    bool TryParsePositiveId(const wxString& text, int& outId) const {
        wxString trimmed = text;
        trimmed.Trim(true).Trim(false);
        if(trimmed.IsEmpty()) {
            outId = 0;
            return false;
        }
        long value = 0;
        if(!trimmed.ToLong(&value) || value <= 0 || value > 65535) {
            outId = 0;
            return false;
        }
        outId = static_cast<int>(value);
        return true;
    }

    int GetSelectedSlotIndex() const {
        if(!m_slotList || m_slots.empty()) {
            return -1;
        }
        int idx = m_slotList->GetSelection();
        if(idx == wxNOT_FOUND || idx < 0 || idx >= static_cast<int>(m_slots.size())) {
            return -1;
        }
        return idx;
    }

    int GetSelectedAvailableBorderId() const {
        if(!m_availableList) {
            return 0;
        }
        long selection = m_availableList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if(selection == wxNOT_FOUND || selection < 0 || selection >= static_cast<long>(m_visibleEntryIndices.size())) {
            return 0;
        }
        int entryIndex = m_visibleEntryIndices[static_cast<size_t>(selection)];
        if(entryIndex < 0 || entryIndex >= static_cast<int>(m_entries.size())) {
            return 0;
        }
        return m_entries[entryIndex].id;
    }

    void RefreshSlotsList(int preferredSelection = -1) {
        if(!m_slotList) {
            return;
        }

        int oldSelection = m_slotList->GetSelection();
        m_slotList->Clear();

        for(size_t i = 0; i < m_slots.size(); ++i) {
            m_slotList->Append(wxString::Format("Slot %03d | Border ID %d", static_cast<int>(i + 1), m_slots[i]));
        }

        if(m_slots.empty()) {
            m_slotList->Append("<no slots>");
            m_slotList->Enable(false);
        } else {
            m_slotList->Enable(true);
            int selectionToSet = preferredSelection;
            if(selectionToSet < 0 || selectionToSet >= static_cast<int>(m_slots.size())) {
                selectionToSet = oldSelection;
            }
            if(selectionToSet < 0 || selectionToSet >= static_cast<int>(m_slots.size())) {
                selectionToSet = 0;
            }
            m_slotList->SetSelection(selectionToSet);
        }
    }

    void RebuildAvailableList() {
        if(!m_availableList) {
            return;
        }

        wxString filterText = m_filterCtrl ? m_filterCtrl->GetValue() : wxString();
        wxString filterLower = filterText;
        filterLower.MakeLower();

        m_filteredEntryIndices.clear();
        for(size_t i = 0; i < m_entries.size(); ++i) {
            wxString label = m_entries[i].label;
            wxString description = m_entries[i].description;
            wxString idText = wxString::Format("%d", m_entries[i].id);
            label.MakeLower();
            description.MakeLower();

            if(filterLower.IsEmpty() ||
               label.Find(filterLower) != wxNOT_FOUND ||
               description.Find(filterLower) != wxNOT_FOUND ||
               idText.Find(filterLower) != wxNOT_FOUND) {
                m_filteredEntryIndices.push_back(static_cast<int>(i));
            }
        }

        size_t count = m_filteredEntryIndices.size();
        m_totalPages = (count == 0 ? 1 : ((count + m_pageSize - 1) / m_pageSize));
        if(m_currentPage >= m_totalPages) {
            m_currentPage = m_totalPages - 1;
        }

        m_availableList->DeleteAllItems();
        if(m_availableImageList) {
            m_availableImageList->RemoveAll();
        }
        m_visibleEntryIndices.clear();

        size_t start = m_currentPage * m_pageSize;
        size_t end = std::min(start + m_pageSize, count);
        for(size_t i = start; i < end; ++i) {
            int entryIndex = m_filteredEntryIndices[i];
            m_visibleEntryIndices.push_back(entryIndex);

            const BorderEntry& entry = m_entries[entryIndex];
            wxString rowLabel = wxString::Format("Slot %03d | ID %d", static_cast<int>(i + 1), entry.id);
            if(!entry.description.IsEmpty()) {
                rowLabel += wxString::Format(" | %s", entry.description);
            }
            int imageIndex = -1;
            if(m_availableImageList) {
                imageIndex = m_availableImageList->Add(BuildBorderPreviewBitmap(entry));
            }
            m_availableList->InsertItem(static_cast<long>(m_availableList->GetItemCount()), rowLabel, imageIndex);
        }

        if(!m_visibleEntryIndices.empty()) {
            m_availableList->SetItemState(0, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED, wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            m_availableList->EnsureVisible(0);
        }
        m_availableList->SetColumnWidth(0, wxLIST_AUTOSIZE_USEHEADER);

        if(m_pageInfoLabel) {
            m_pageInfoLabel->SetLabel(wxString::Format(
                "Page %d/%d   (%d results)",
                static_cast<int>(m_currentPage + 1),
                static_cast<int>(m_totalPages),
                static_cast<int>(count)
            ));
        }
        if(m_prevPageButton) {
            m_prevPageButton->Enable(m_currentPage > 0);
        }
        if(m_nextPageButton) {
            m_nextPageButton->Enable((m_currentPage + 1) < m_totalPages);
        }
    }

    wxBitmap BuildBorderPreviewBitmap(const BorderEntry& entry) const {
        const int width = 32;
        const int height = 32;
        wxBitmap bitmap(width, height, 32);
        wxMemoryDC dc(bitmap);
        dc.SetBackground(wxBrush(wxColour(24, 28, 34)));
        dc.Clear();
        dc.SetPen(wxPen(wxColour(70, 80, 95)));
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(0, 0, width, height);

        bool drewSprite = false;
        if(entry.previewItemId > 0) {
            const ItemType& type = g_items.getItemType(entry.previewItemId);
            if(type.id != 0) {
                Sprite* sprite = g_gui.gfx.getSprite(type.clientID);
                if(sprite) {
                    sprite->DrawTo(&dc, SPRITE_SIZE_32x32, 0, 0, width, height);
                    drewSprite = true;
                }
            }
        }

        if(!drewSprite) {
            dc.SetPen(wxPen(wxColour(115, 130, 150)));
            dc.DrawLine(4, 4, width - 4, height - 4);
            dc.DrawLine(width - 4, 4, 4, height - 4);
        }

        dc.SelectObject(wxNullBitmap);
        return bitmap;
    }

    void OnAvailableItemActivated(wxListEvent& WXUNUSED(event)) {
        wxCommandEvent cmd;
        OnApplyToSlot(cmd);
    }

    void OnAvailableItemSelected(wxListEvent& event) {
        event.Skip();
    }

    void OnAddSlot(wxCommandEvent& WXUNUSED(event)) {
        wxTextEntryDialog dialog(this, "Type the border ID for the new slot:", "Add Slot");
        if(dialog.ShowModal() != wxID_OK) {
            return;
        }

        int borderId = 0;
        if(!TryParsePositiveId(dialog.GetValue(), borderId)) {
            wxMessageBox("Invalid border ID. Use a positive integer (1-65535).", "Add Slot", wxICON_ERROR);
            return;
        }

        m_slots.push_back(borderId);
        RefreshSlotsList(static_cast<int>(m_slots.size()) - 1);
    }

    void OnEditSelectedSlot(wxCommandEvent& WXUNUSED(event)) {
        int slotIndex = GetSelectedSlotIndex();
        if(slotIndex == -1) {
            wxMessageBox("Select a slot first.", "Edit Slot", wxICON_INFORMATION);
            return;
        }

        wxTextEntryDialog dialog(
            this,
            "Type a new border ID (leave empty to remove this slot):",
            "Edit Slot",
            wxString::Format("%d", m_slots[slotIndex])
        );
        if(dialog.ShowModal() != wxID_OK) {
            return;
        }

        wxString text = dialog.GetValue();
        wxString trimmed = text;
        trimmed.Trim(true).Trim(false);
        if(trimmed.IsEmpty()) {
            m_slots.erase(m_slots.begin() + slotIndex);
            RefreshSlotsList(slotIndex);
            return;
        }

        int borderId = 0;
        if(!TryParsePositiveId(trimmed, borderId)) {
            wxMessageBox("Invalid border ID. Use a positive integer (1-65535).", "Edit Slot", wxICON_ERROR);
            return;
        }

        m_slots[slotIndex] = borderId;
        RefreshSlotsList(slotIndex);
    }

    void OnRemoveSelectedSlot(wxCommandEvent& WXUNUSED(event)) {
        int slotIndex = GetSelectedSlotIndex();
        if(slotIndex == -1) {
            wxMessageBox("Select a slot first.", "Remove Slot", wxICON_INFORMATION);
            return;
        }

        m_slots.erase(m_slots.begin() + slotIndex);
        RefreshSlotsList(slotIndex);
    }

    void OnFilterChanged(wxCommandEvent& WXUNUSED(event)) {
        m_currentPage = 0;
        RebuildAvailableList();
    }

    void OnPrevPage(wxCommandEvent& WXUNUSED(event)) {
        if(m_currentPage > 0) {
            --m_currentPage;
            RebuildAvailableList();
        }
    }

    void OnNextPage(wxCommandEvent& WXUNUSED(event)) {
        if((m_currentPage + 1) < m_totalPages) {
            ++m_currentPage;
            RebuildAvailableList();
        }
    }

    void OnApplyToSlot(wxCommandEvent& WXUNUSED(event)) {
        int borderId = GetSelectedAvailableBorderId();
        if(borderId <= 0) {
            wxMessageBox("Select an available border first.", "Apply Border", wxICON_INFORMATION);
            return;
        }

        int slotIndex = GetSelectedSlotIndex();
        if(slotIndex == -1) {
            m_slots.push_back(borderId);
            RefreshSlotsList(static_cast<int>(m_slots.size()) - 1);
            return;
        }

        m_slots[slotIndex] = borderId;
        RefreshSlotsList(slotIndex);
    }

    void OnAppendSlotFromAvailable(wxCommandEvent& WXUNUSED(event)) {
        int borderId = GetSelectedAvailableBorderId();
        if(borderId <= 0) {
            wxMessageBox("Select an available border first.", "Append Slot", wxICON_INFORMATION);
            return;
        }

        m_slots.push_back(borderId);
        RefreshSlotsList(static_cast<int>(m_slots.size()) - 1);
    }

    void OnConfirm(wxCommandEvent& WXUNUSED(event)) {
        EndModal(wxID_OK);
    }

    std::vector<int> m_slots;
    std::vector<BorderEntry> m_entries;
    std::vector<int> m_filteredEntryIndices;
    std::vector<int> m_visibleEntryIndices;
    wxTextCtrl* m_filterCtrl;
    wxListBox* m_slotList;
    wxListCtrl* m_availableList;
    wxImageList* m_availableImageList;
    wxButton* m_prevPageButton;
    wxButton* m_nextPageButton;
    wxStaticText* m_pageInfoLabel;
    size_t m_totalPages;
    size_t m_currentPage;
    const size_t m_pageSize;
};

// Add a helper function at the top of the file to get item ID from brush
uint16_t GetItemIDFromBrush(Brush* brush) {
    if (!brush) {
        wxLogDebug("GetItemIDFromBrush: Brush is null");
        OutputDebugStringA("GetItemIDFromBrush: Brush is null\n");
        return 0;
    }
    
    uint16_t id = 0;
    
    wxLogDebug("GetItemIDFromBrush: Checking brush type: %s", wxString(brush->getName()).c_str());
    OutputDebugStringA(wxString::Format("GetItemIDFromBrush: Checking brush type: %s\n", wxString(brush->getName()).c_str()).mb_str());
    
    // First prioritize RAW brush - this is the most direct approach
    if (brush->isRaw()) {
        RAWBrush* rawBrush = brush->asRaw();
        if (rawBrush) {
            id = rawBrush->getItemID();
            wxLogDebug("GetItemIDFromBrush: Found RAW brush ID: %d", id);
            OutputDebugStringA(wxString::Format("GetItemIDFromBrush: Found RAW brush ID: %d\n", id).mb_str());
            if (id > 0) {
                return id;
            }
        }
    } 
    
    // Then try getID which sometimes works directly
    id = brush->getID();
    if (id > 0) {
        wxLogDebug("GetItemIDFromBrush: Got ID from brush->getID(): %d", id);
        OutputDebugStringA(wxString::Format("GetItemIDFromBrush: Got ID from brush->getID(): %d\n", id).mb_str());
        return id;
    }
    
    // Try getLookID which works for most other brush types
    id = brush->getLookID();
    if (id > 0) {
        wxLogDebug("GetItemIDFromBrush: Got ID from getLookID(): %d", id);
        OutputDebugStringA(wxString::Format("GetItemIDFromBrush: Got ID from getLookID(): %d\n", id).mb_str());
        return id;
    }
    
    // Try specific brush type methods - when all else fails
    if (brush->isGround()) {
        wxLogDebug("GetItemIDFromBrush: Detected Ground brush");
        OutputDebugStringA("GetItemIDFromBrush: Detected Ground brush\n");
        GroundBrush* groundBrush = brush->asGround();
        if (groundBrush) {
            // For ground brush, id is usually the server_lookid from grounds.xml
            // Try to find something else
            wxLogDebug("GetItemIDFromBrush: Failed to get ID for Ground brush");
            OutputDebugStringA("GetItemIDFromBrush: Failed to get ID for Ground brush\n");
        }
    }
    else if (brush->isWall()) {
        wxLogDebug("GetItemIDFromBrush: Detected Wall brush");
        OutputDebugStringA("GetItemIDFromBrush: Detected Wall brush\n");
        WallBrush* wallBrush = brush->asWall();
        if (wallBrush) {
            wxLogDebug("GetItemIDFromBrush: Failed to get ID for Wall brush");
            OutputDebugStringA("GetItemIDFromBrush: Failed to get ID for Wall brush\n");
        }
    }
    else if (brush->isDoodad()) {
        wxLogDebug("GetItemIDFromBrush: Detected Doodad brush");
        OutputDebugStringA("GetItemIDFromBrush: Detected Doodad brush\n");
        DoodadBrush* doodadBrush = brush->asDoodad();
        if (doodadBrush) {
            wxLogDebug("GetItemIDFromBrush: Failed to get ID for Doodad brush");
            OutputDebugStringA("GetItemIDFromBrush: Failed to get ID for Doodad brush\n");
        }
    }
    
    if (id == 0) {
        wxLogDebug("GetItemIDFromBrush: Failed to get item ID from brush %s", wxString(brush->getName()).c_str());
        OutputDebugStringA(wxString::Format("GetItemIDFromBrush: Failed to get item ID from brush %s\n", wxString(brush->getName()).c_str()).mb_str());
    }
    
    return id;
}

// Event table for BorderEditorDialog
BEGIN_EVENT_TABLE(BorderEditorDialog, wxDialog)
    EVT_BUTTON(wxID_ADD, BorderEditorDialog::OnAddItem)
    EVT_BUTTON(wxID_CLEAR, BorderEditorDialog::OnClear)
    EVT_BUTTON(wxID_SAVE, BorderEditorDialog::OnSave)
    EVT_BUTTON(wxID_CLOSE, BorderEditorDialog::OnClose)
    EVT_BUTTON(wxID_FIND, BorderEditorDialog::OnBrowse)
    EVT_COMBOBOX(wxID_ANY, BorderEditorDialog::OnLoadBorder)
    EVT_NOTEBOOK_PAGE_CHANGED(wxID_ANY, BorderEditorDialog::OnPageChanged)
    EVT_BUTTON(wxID_ADD + 100, BorderEditorDialog::OnAddGroundItem)
    EVT_BUTTON(wxID_REMOVE, BorderEditorDialog::OnRemoveGroundItem)
    EVT_BUTTON(wxID_FIND + 100, BorderEditorDialog::OnGroundBrowse)
    EVT_COMBOBOX(wxID_ANY + 100, BorderEditorDialog::OnLoadGroundBrush)
END_EVENT_TABLE()

// Event table for BorderItemButton
BEGIN_EVENT_TABLE(BorderItemButton, wxButton)
    EVT_PAINT(BorderItemButton::OnPaint)
END_EVENT_TABLE()

// Event table for BorderGridPanel
BEGIN_EVENT_TABLE(BorderGridPanel, wxPanel)
    EVT_PAINT(BorderGridPanel::OnPaint)
    EVT_LEFT_UP(BorderGridPanel::OnMouseClick)
    EVT_LEFT_DOWN(BorderGridPanel::OnMouseDown)
END_EVENT_TABLE()

class BorderGridDropTarget : public wxTextDropTarget {
public:
    explicit BorderGridDropTarget(BorderGridPanel* grid) : m_grid(grid) { }

    bool OnDropText(wxCoord x, wxCoord y, const wxString& data) override {
        if (!m_grid || !data.StartsWith("ITEM_ID:")) {
            return false;
        }

        wxString idStr = data.Mid(8);
        long itemId = 0;
        if (!idStr.ToLong(&itemId) || itemId <= 0 || itemId > 0xFFFF) {
            return false;
        }

        BorderEdgePosition pos = m_grid->GetPositionFromCoordinates(x, y);
        if (pos == EDGE_NONE) {
            return false;
        }

        m_grid->SetSelectedPosition(pos);

        wxWindow* parent = m_grid->GetParent();
        while (parent && !dynamic_cast<BorderEditorDialog*>(parent)) {
            parent = parent->GetParent();
        }

        BorderEditorDialog* dialog = dynamic_cast<BorderEditorDialog*>(parent);
        if (!dialog) {
            return false;
        }

        dialog->ApplyItemToPosition(pos, static_cast<uint16_t>(itemId));
        return true;
    }

private:
    BorderGridPanel* m_grid;
};

// Event table for BorderPreviewPanel
BEGIN_EVENT_TABLE(BorderPreviewPanel, wxPanel)
    EVT_PAINT(BorderPreviewPanel::OnPaint)
END_EVENT_TABLE()

BorderEditorDialog::BorderEditorDialog(wxWindow* parent, const wxString& title, bool modifyGroundBordersOnly, const wxString& initialGroundBrushName) :
    wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(1000, 650),
        wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
    m_nextBorderId(1),
    m_activeTab(0),
    m_modifyGroundBordersOnly(modifyGroundBordersOnly),
    m_initialGroundBrushName(initialGroundBrushName),
    m_borderPreviewCatalogLoaded(false) {
    
    CreateGUIControls();
    LoadExistingBorders();
    LoadExistingGroundBrushes();
    LoadTilesets();  // Load available tilesets
    
    // Set ID to next available ID
    m_idCtrl->SetValue(m_nextBorderId);
    if(m_borderIdsCtrl) {
        m_borderIdsCtrl->SetValue(wxString::Format("%d", m_nextBorderId));
    }

    if(m_modifyGroundBordersOnly) {
        ConfigureModifyGroundBordersMode();
    }
    
    // Center the dialog
    CenterOnParent();
}

BorderEditorDialog::~BorderEditorDialog() {
    // Nothing to destroy manually
}

void BorderEditorDialog::CreateGUIControls() {
    m_borderIdsPreviewCtrl = nullptr;
    m_borderToNoneIdsPreviewCtrl = nullptr;
    m_borderListIdsPreviewCtrl = nullptr;
    m_borderIdsPreviewImages = nullptr;
    m_borderToNoneIdsPreviewImages = nullptr;
    m_borderListIdsPreviewImages = nullptr;

    wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
    
    // Common properties - more compact horizontal layout
    wxStaticBoxSizer* commonPropertiesSizer = new wxStaticBoxSizer(wxVERTICAL, this, "Common Properties");
    
    wxBoxSizer* commonPropertiesHorizSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Name field
    wxBoxSizer* nameSizer = new wxBoxSizer(wxVERTICAL);
    nameSizer->Add(new wxStaticText(this, wxID_ANY, "Name:"), 0);
    m_nameCtrl = new wxTextCtrl(this, wxID_ANY);
    m_nameCtrl->SetToolTip("Descriptive name for the border/brush");
    nameSizer->Add(m_nameCtrl, 0, wxEXPAND | wxTOP, 2);
    commonPropertiesHorizSizer->Add(nameSizer, 1, wxEXPAND | wxRIGHT, 10);
    
    // ID field
    wxBoxSizer* idSizer = new wxBoxSizer(wxVERTICAL);
    idSizer->Add(new wxStaticText(this, wxID_ANY, "ID:"), 0);
    m_idCtrl = new wxSpinCtrl(this, wxID_ANY, "1", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 1, 1000);
    m_idCtrl->SetToolTip("Unique identifier for this border/brush");
    idSizer->Add(m_idCtrl, 0, wxEXPAND | wxTOP, 2);
    commonPropertiesHorizSizer->Add(idSizer, 0, wxEXPAND);
    
    commonPropertiesSizer->Add(commonPropertiesHorizSizer, 0, wxEXPAND | wxALL, 5);
    topSizer->Add(commonPropertiesSizer, 0, wxEXPAND | wxALL, 5);
    
    // Create notebook with Border and Ground tabs
    m_notebook = new wxNotebook(this, wxID_ANY);
    
    // ========== BORDER TAB ==========
    m_borderPanel = new wxPanel(m_notebook);
    wxBoxSizer* borderSizer = new wxBoxSizer(wxVERTICAL);
    
    // Border Properties - more compact layout
    wxStaticBoxSizer* borderPropertiesSizer = new wxStaticBoxSizer(wxVERTICAL, m_borderPanel, "Border Properties");
    
    // Two-column horizontal layout
    wxBoxSizer* borderPropsHorizSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Left column - Group and Type
    wxBoxSizer* leftColSizer = new wxBoxSizer(wxVERTICAL);
    
    // Border Group
    wxBoxSizer* groupSizer = new wxBoxSizer(wxVERTICAL);
    groupSizer->Add(new wxStaticText(m_borderPanel, wxID_ANY, "Group:"), 0);
    m_groupCtrl = new wxSpinCtrl(m_borderPanel, wxID_ANY, "0", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 1000);
    m_groupCtrl->SetToolTip("Optional group identifier (0 = no group)");
    groupSizer->Add(m_groupCtrl, 0, wxEXPAND | wxTOP, 2);
    leftColSizer->Add(groupSizer, 0, wxEXPAND | wxBOTTOM, 5);
    
    // Border Type
    wxBoxSizer* typeSizer = new wxBoxSizer(wxVERTICAL);
    typeSizer->Add(new wxStaticText(m_borderPanel, wxID_ANY, "Type:"), 0);
    wxBoxSizer* checkboxSizer = new wxBoxSizer(wxHORIZONTAL);
    m_isOptionalCheck = new wxCheckBox(m_borderPanel, wxID_ANY, "Optional");
    m_isOptionalCheck->SetToolTip("Marks this border as optional");
    m_isGroundCheck = new wxCheckBox(m_borderPanel, wxID_ANY, "Ground");
    m_isGroundCheck->SetToolTip("Marks this border as a ground border");
    checkboxSizer->Add(m_isOptionalCheck, 0, wxRIGHT, 10);
    checkboxSizer->Add(m_isGroundCheck, 0);
    typeSizer->Add(checkboxSizer, 0, wxEXPAND | wxTOP, 2);
    leftColSizer->Add(typeSizer, 0, wxEXPAND);
    
    borderPropsHorizSizer->Add(leftColSizer, 1, wxEXPAND | wxRIGHT, 10);
    
    // Right column - Load Existing
    wxBoxSizer* rightColSizer = new wxBoxSizer(wxVERTICAL);
    rightColSizer->Add(new wxStaticText(m_borderPanel, wxID_ANY, "Load Existing:"), 0);
    m_existingBordersCombo = new wxComboBox(m_borderPanel, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY | wxCB_DROPDOWN);
    m_existingBordersCombo->SetToolTip("Load an existing border as template");
    rightColSizer->Add(m_existingBordersCombo, 0, wxEXPAND | wxTOP, 2);
    
    borderPropsHorizSizer->Add(rightColSizer, 1, wxEXPAND);
    
    borderPropertiesSizer->Add(borderPropsHorizSizer, 0, wxEXPAND | wxALL, 5);
    borderSizer->Add(borderPropertiesSizer, 0, wxEXPAND | wxALL, 5);
    
    // Border content area with grid and preview
    wxBoxSizer* borderContentSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Left side - Grid Editor
    wxStaticBoxSizer* gridSizer = new wxStaticBoxSizer(wxVERTICAL, m_borderPanel, "Border Grid");
    m_gridPanel = new BorderGridPanel(m_borderPanel);
    m_gridPanel->SetDropTarget(new BorderGridDropTarget(m_gridPanel));
    gridSizer->Add(m_gridPanel, 1, wxEXPAND | wxALL, 5);
    
    // Add instruction label
    wxStaticText* instructions = new wxStaticText(m_borderPanel, wxID_ANY, 
        "Click on a grid position to place the currently selected brush.\n"
        "The item ID will be extracted automatically from the brush.");
    instructions->SetForegroundColour(*wxBLUE);
    gridSizer->Add(instructions, 0, wxEXPAND | wxALL, 5);
    
    // Current selected item controls
    wxBoxSizer* itemSizer = new wxBoxSizer(wxHORIZONTAL);
    itemSizer->Add(new wxStaticText(m_borderPanel, wxID_ANY, "Item ID:"), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    m_itemIdCtrl = new wxSpinCtrl(m_borderPanel, wxID_ANY, "0", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535);
    m_itemIdCtrl->SetToolTip("Enter an item ID manually if you don't want to use the current brush");
    itemSizer->Add(m_itemIdCtrl, 0, wxRIGHT, 5);
    wxButton* browseButton = new wxButton(m_borderPanel, wxID_FIND, "Browse...", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    browseButton->SetToolTip("Browse for an item to use instead of the current brush");
    itemSizer->Add(browseButton, 0, wxRIGHT, 5);
    wxButton* addButton = new wxButton(m_borderPanel, wxID_ADD, "Add Manually", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    addButton->SetToolTip("Add the item ID manually to the currently selected position");
    itemSizer->Add(addButton, 0);
    
    gridSizer->Add(itemSizer, 0, wxEXPAND | wxALL, 5);
    
    // Add grid editor to content sizer
    borderContentSizer->Add(gridSizer, 1, wxEXPAND | wxALL, 5);
    
    // Right side - Preview Panel
    wxStaticBoxSizer* previewSizer = new wxStaticBoxSizer(wxVERTICAL, m_borderPanel, "Preview");
    m_previewPanel = new BorderPreviewPanel(m_borderPanel);
    previewSizer->Add(m_previewPanel, 1, wxEXPAND | wxALL, 5);
    
    // Add preview to content sizer
    borderContentSizer->Add(previewSizer, 1, wxEXPAND | wxALL, 5);
    
    // Add content sizer to main border sizer
    borderSizer->Add(borderContentSizer, 1, wxEXPAND | wxALL, 5);
    
    // Bottom buttons for border tab
    wxBoxSizer* borderButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    borderButtonSizer->Add(new wxButton(m_borderPanel, wxID_CLEAR, "Clear"), 0, wxRIGHT, 5);
    borderButtonSizer->Add(new wxButton(m_borderPanel, wxID_SAVE, "Save Border"), 0, wxRIGHT, 5);
    borderButtonSizer->AddStretchSpacer(1);
    borderButtonSizer->Add(new wxButton(m_borderPanel, wxID_CLOSE, "Close"), 0);
    
    borderSizer->Add(borderButtonSizer, 0, wxEXPAND | wxALL, 5);
    
    m_borderPanel->SetSizer(borderSizer);
    
    // ========== GROUND TAB ==========
    m_groundPanel = new wxPanel(m_notebook);
    wxBoxSizer* groundSizer = new wxBoxSizer(wxVERTICAL);
    
    // Ground Brush Properties - more compact layout
    wxStaticBoxSizer* groundPropertiesSizer = new wxStaticBoxSizer(wxVERTICAL, m_groundPanel, "Ground Brush Properties");
    
    // Two rows of two columns each
    wxBoxSizer* topRowSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Tileset selector
    wxBoxSizer* tilesetSizer = new wxBoxSizer(wxVERTICAL);
    tilesetSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Tileset:"), 0);
    m_tilesetChoice = new wxChoice(m_groundPanel, wxID_ANY);
    m_tilesetChoice->SetToolTip("Select tileset to add this brush to");
    tilesetSizer->Add(m_tilesetChoice, 0, wxEXPAND | wxTOP, 2);
    topRowSizer->Add(tilesetSizer, 1, wxEXPAND | wxRIGHT, 10);
    
    // Server Look ID
    wxBoxSizer* serverIdSizer = new wxBoxSizer(wxVERTICAL);
    serverIdSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Server Look ID:"), 0);
    m_serverLookIdCtrl = new wxSpinCtrl(m_groundPanel, wxID_ANY, "0", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 65535);
    m_serverLookIdCtrl->SetToolTip("Server-side item ID");
    serverIdSizer->Add(m_serverLookIdCtrl, 0, wxEXPAND | wxTOP, 2);
    topRowSizer->Add(serverIdSizer, 1, wxEXPAND);
    
    groundPropertiesSizer->Add(topRowSizer, 0, wxEXPAND | wxALL, 5);
    
    // Second row
    wxBoxSizer* bottomRowSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Z-Order
    wxBoxSizer* zOrderSizer = new wxBoxSizer(wxVERTICAL);
    zOrderSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Z-Order:"), 0);
    m_zOrderCtrl = new wxSpinCtrl(m_groundPanel, wxID_ANY, "0", wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 0, 10000);
    m_zOrderCtrl->SetToolTip("Z-Order for display");
    zOrderSizer->Add(m_zOrderCtrl, 0, wxEXPAND | wxTOP, 2);
    bottomRowSizer->Add(zOrderSizer, 1, wxEXPAND | wxRIGHT, 10);
    
    // Existing ground brushes dropdown
    wxBoxSizer* existingSizer = new wxBoxSizer(wxVERTICAL);
    existingSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Load Existing:"), 0);
    m_existingGroundBrushesCombo = new wxComboBox(m_groundPanel, wxID_ANY + 100, "", wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY | wxCB_DROPDOWN);
    m_existingGroundBrushesCombo->SetToolTip("Load an existing ground brush as template");
    existingSizer->Add(m_existingGroundBrushesCombo, 0, wxEXPAND | wxTOP, 2);
    bottomRowSizer->Add(existingSizer, 1, wxEXPAND);
    
    groundPropertiesSizer->Add(bottomRowSizer, 0, wxEXPAND | wxALL, 5);
    
    groundSizer->Add(groundPropertiesSizer, 0, wxEXPAND | wxALL, 5);
    
    // Ground Items
    wxStaticBoxSizer* groundItemsSizer = new wxStaticBoxSizer(wxVERTICAL, m_groundPanel, "Ground Items");
    
    // List of ground items - set a smaller height
    m_groundItemsList = new wxListBox(m_groundPanel, ID_GROUND_ITEM_LIST, wxDefaultPosition, wxSize(-1, 100), 0, nullptr, wxLB_SINGLE);
    groundItemsSizer->Add(m_groundItemsList, 0, wxEXPAND | wxALL, 5);
    
    // Controls for adding/removing ground items
    wxBoxSizer* groundItemRowSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Left side - item ID and chance
    wxBoxSizer* itemDetailsSizer = new wxBoxSizer(wxHORIZONTAL);
    
    // Item ID input
    wxBoxSizer* itemIdSizer = new wxBoxSizer(wxVERTICAL);
    itemIdSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Item ID:"), 0);
    m_groundItemIdCtrl = new wxSpinCtrl(m_groundPanel, wxID_ANY, "0", wxDefaultPosition, wxSize(80, -1), wxSP_ARROW_KEYS, 0, 65535);
    m_groundItemIdCtrl->SetToolTip("ID of the item to add");
    itemIdSizer->Add(m_groundItemIdCtrl, 0, wxEXPAND | wxTOP, 2);
    itemDetailsSizer->Add(itemIdSizer, 0, wxEXPAND | wxRIGHT, 5);
    
    // Chance input
    wxBoxSizer* chanceSizer = new wxBoxSizer(wxVERTICAL);
    chanceSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Chance:"), 0);
    m_groundItemChanceCtrl = new wxSpinCtrl(m_groundPanel, wxID_ANY, "10", wxDefaultPosition, wxSize(60, -1), wxSP_ARROW_KEYS, 1, 10000);
    m_groundItemChanceCtrl->SetToolTip("Chance of this item appearing");
    chanceSizer->Add(m_groundItemChanceCtrl, 0, wxEXPAND | wxTOP, 2);
    itemDetailsSizer->Add(chanceSizer, 0, wxEXPAND);
    
    groundItemRowSizer->Add(itemDetailsSizer, 1, wxEXPAND | wxRIGHT, 10);
    
    // Right side - buttons
    wxBoxSizer* itemButtonsSizer = new wxBoxSizer(wxVERTICAL);
    itemButtonsSizer->AddStretchSpacer();
    
    wxBoxSizer* buttonsSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* groundBrowseButton = new wxButton(m_groundPanel, wxID_FIND + 100, "Browse...", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    groundBrowseButton->SetToolTip("Browse for an item");
    buttonsSizer->Add(groundBrowseButton, 0, wxRIGHT, 5);
    
    m_addGroundItemButton = new wxButton(m_groundPanel, wxID_ADD + 100, "Add", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    m_addGroundItemButton->SetToolTip("Add this item to the list");
    buttonsSizer->Add(m_addGroundItemButton, 0, wxRIGHT, 5);
    
    m_removeGroundItemButton = new wxButton(m_groundPanel, wxID_REMOVE, "Remove", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    m_removeGroundItemButton->SetToolTip("Remove the selected item");
    buttonsSizer->Add(m_removeGroundItemButton, 0);
    
    itemButtonsSizer->Add(buttonsSizer, 0, wxEXPAND);
    groundItemRowSizer->Add(itemButtonsSizer, 0, wxEXPAND);
    
    groundItemsSizer->Add(groundItemRowSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    groundSizer->Add(groundItemsSizer, 0, wxEXPAND | wxALL, 5); // Changed from 1 to 0 to not expand
    
    // Grid and border selection for ground tab
    wxStaticBoxSizer* groundBorderSizer = new wxStaticBoxSizer(wxVERTICAL, m_groundPanel, "Border for Ground Brush");
    
    // First row - Border alignment and 'to none' option
    wxBoxSizer* borderRow1 = new wxBoxSizer(wxHORIZONTAL);
    
    // Border alignment
    wxBoxSizer* alignSizer = new wxBoxSizer(wxVERTICAL);
    alignSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Border Alignment:"), 0);
    wxArrayString alignOptions;
    alignOptions.Add("outer");
    alignOptions.Add("inner");
    m_borderAlignmentChoice = new wxChoice(m_groundPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, alignOptions);
    m_borderAlignmentChoice->SetSelection(0); // Default to "outer"
    m_borderAlignmentChoice->SetToolTip("Alignment type for the border");
    alignSizer->Add(m_borderAlignmentChoice, 0, wxEXPAND | wxTOP, 2);
    borderRow1->Add(alignSizer, 1, wxEXPAND | wxRIGHT, 10);
    
    // Border options (checkboxes)
    wxBoxSizer* optionsSizer = new wxBoxSizer(wxVERTICAL);
    optionsSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Border Options:"), 0);
    wxBoxSizer* checksSizer = new wxBoxSizer(wxHORIZONTAL);
    m_includeToNoneCheck = new wxCheckBox(m_groundPanel, wxID_ANY, "To None");
    m_includeToNoneCheck->SetValue(true); // Default to checked
    m_includeToNoneCheck->SetToolTip("Adds additional border with 'to none' attribute");
    m_includeInnerCheck = new wxCheckBox(m_groundPanel, wxID_ANY, "Inner Border");
    m_includeInnerCheck->SetToolTip("Adds additional inner border with same ID");
    checksSizer->Add(m_includeToNoneCheck, 0, wxRIGHT, 10);
    checksSizer->Add(m_includeInnerCheck, 0);
    optionsSizer->Add(checksSizer, 0, wxEXPAND | wxTOP, 2);
    borderRow1->Add(optionsSizer, 1, wxEXPAND);
    
    groundBorderSizer->Add(borderRow1, 0, wxEXPAND | wxALL, 5);

    // Second row - border ID lists
    wxBoxSizer* borderRow2 = new wxBoxSizer(wxHORIZONTAL);

    wxBoxSizer* borderIdsSizer = new wxBoxSizer(wxVERTICAL);
    borderIdsSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Border IDs:"), 0);
    m_borderIdsCtrl = new wxTextCtrl(m_groundPanel, wxID_ANY, "");
    m_borderIdsCtrl->SetToolTip("Comma-separated border IDs. Example: 258, 256");
    borderIdsSizer->Add(m_borderIdsCtrl, 0, wxEXPAND | wxTOP, 2);
    borderIdsSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Preview (click to activate as first ID):"), 0, wxTOP, 4);
    m_borderIdsPreviewCtrl = new wxListCtrl(
        m_groundPanel,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxLC_ICON | wxLC_AUTOARRANGE | wxLC_SINGLE_SEL | wxBORDER_SUNKEN
    );
    m_borderIdsPreviewCtrl->SetMinSize(wxSize(-1, 78));
    m_borderIdsPreviewCtrl->SetToolTip("Click a border preview to make it the active outer border ID.");
    m_borderIdsPreviewImages = new wxImageList(32, 32, true);
    m_borderIdsPreviewCtrl->AssignImageList(m_borderIdsPreviewImages, wxIMAGE_LIST_NORMAL);
    borderIdsSizer->Add(m_borderIdsPreviewCtrl, 0, wxEXPAND | wxTOP, 2);
    borderRow2->Add(borderIdsSizer, 1, wxEXPAND | wxRIGHT, 10);

    wxBoxSizer* toNoneIdsSizer = new wxBoxSizer(wxVERTICAL);
    toNoneIdsSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "To None IDs (Optional):"), 0);
    m_borderToNoneIdsCtrl = new wxTextCtrl(m_groundPanel, wxID_ANY, "");
    m_borderToNoneIdsCtrl->SetToolTip("Optional comma-separated IDs for to=\"none\". Leave empty to reuse Border IDs.");
    toNoneIdsSizer->Add(m_borderToNoneIdsCtrl, 0, wxEXPAND | wxTOP, 2);
    toNoneIdsSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Preview (click to activate as first ID):"), 0, wxTOP, 4);
    m_borderToNoneIdsPreviewCtrl = new wxListCtrl(
        m_groundPanel,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxLC_ICON | wxLC_AUTOARRANGE | wxLC_SINGLE_SEL | wxBORDER_SUNKEN
    );
    m_borderToNoneIdsPreviewCtrl->SetMinSize(wxSize(-1, 78));
    m_borderToNoneIdsPreviewCtrl->SetToolTip("Click a border preview to make it the first To None ID.");
    m_borderToNoneIdsPreviewImages = new wxImageList(32, 32, true);
    m_borderToNoneIdsPreviewCtrl->AssignImageList(m_borderToNoneIdsPreviewImages, wxIMAGE_LIST_NORMAL);
    toNoneIdsSizer->Add(m_borderToNoneIdsPreviewCtrl, 0, wxEXPAND | wxTOP, 2);
    borderRow2->Add(toNoneIdsSizer, 1, wxEXPAND);

    groundBorderSizer->Add(borderRow2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Third row - optional borderlist attribute used for quick border variant switching.
    wxBoxSizer* borderRow3 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* borderListSizer = new wxBoxSizer(wxVERTICAL);
    borderListSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Border List IDs (Optional):"), 0);
    m_borderListCtrl = new wxTextCtrl(m_groundPanel, wxID_ANY, "");
    m_borderListCtrl->SetToolTip("Comma-separated variant IDs stored in borderlist. Example: 201, 202, 2023");
    borderListSizer->Add(m_borderListCtrl, 0, wxEXPAND | wxTOP, 2);
    borderListSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Preview (click to activate in outer IDs):"), 0, wxTOP, 4);
    m_borderListIdsPreviewCtrl = new wxListCtrl(
        m_groundPanel,
        wxID_ANY,
        wxDefaultPosition,
        wxDefaultSize,
        wxLC_ICON | wxLC_AUTOARRANGE | wxLC_SINGLE_SEL | wxBORDER_SUNKEN
    );
    m_borderListIdsPreviewCtrl->SetMinSize(wxSize(-1, 88));
    m_borderListIdsPreviewCtrl->SetToolTip("Click a border preview to promote it to Border IDs.");
    m_borderListIdsPreviewImages = new wxImageList(32, 32, true);
    m_borderListIdsPreviewCtrl->AssignImageList(m_borderListIdsPreviewImages, wxIMAGE_LIST_NORMAL);
    borderListSizer->Add(m_borderListIdsPreviewCtrl, 0, wxEXPAND | wxTOP, 2);
    borderRow3->Add(borderListSizer, 1, wxEXPAND);
    m_searchBorderListButton = new wxButton(m_groundPanel, wxID_ANY, "Slots...", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    m_searchBorderListButton->SetToolTip("Open slot editor with available borders, search and pagination.");
    borderRow3->Add(m_searchBorderListButton, 0, wxALIGN_BOTTOM | wxLEFT, 8);
    groundBorderSizer->Add(borderRow3, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Fourth row - quick promote from borderlist to outer.
    wxBoxSizer* borderRow4 = new wxBoxSizer(wxHORIZONTAL);
    wxBoxSizer* borderListPreviewSizer = new wxBoxSizer(wxVERTICAL);
    borderListPreviewSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Border List Preview (select one):"), 0);
    m_borderListIdsList = new wxListBox(m_groundPanel, wxID_ANY);
    m_borderListIdsList->SetMinSize(wxSize(-1, 90));
    m_borderListIdsList->SetToolTip("Select a borderlist ID and promote it to become the first ID in Border IDs.");
    borderListPreviewSizer->Add(m_borderListIdsList, 1, wxEXPAND | wxTOP, 2);
    borderRow4->Add(borderListPreviewSizer, 1, wxEXPAND | wxRIGHT, 10);

    wxBoxSizer* promoteActionsSizer = new wxBoxSizer(wxVERTICAL);
    m_promoteBorderListIdButton = new wxButton(m_groundPanel, wxID_ANY, "Promote Selected To Outer");
    m_promoteBorderListIdButton->SetToolTip("Moves the selected borderlist ID to the first position in Border IDs.");
    promoteActionsSizer->Add(m_promoteBorderListIdButton, 0, wxEXPAND | wxBOTTOM, 6);
    m_promoteAlsoToNoneCheck = new wxCheckBox(m_groundPanel, wxID_ANY, "Also apply to To None IDs");
    m_promoteAlsoToNoneCheck->SetValue(false);
    promoteActionsSizer->Add(m_promoteAlsoToNoneCheck, 0, wxEXPAND);
    borderRow4->Add(promoteActionsSizer, 0, wxALIGN_TOP);
    groundBorderSizer->Add(borderRow4, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Fifth row - preview of every border entry currently attached to this ground brush.
    wxBoxSizer* refsSizer = new wxBoxSizer(wxVERTICAL);
    refsSizer->Add(new wxStaticText(m_groundPanel, wxID_ANY, "Current Border Entries:"), 0);
    m_groundBorderRefsList = new wxListBox(m_groundPanel, wxID_ANY);
    m_groundBorderRefsList->SetMinSize(wxSize(-1, 80));
    m_groundBorderRefsList->SetToolTip("Shows all <border .../> references currently present in this ground brush.");
    refsSizer->Add(m_groundBorderRefsList, 1, wxEXPAND | wxTOP, 2);
    groundBorderSizer->Add(refsSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Border ID notice (red text)
    wxBoxSizer* borderIdSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* borderIdLabel = new wxStaticText(m_groundPanel, wxID_ANY, "Fallback Border ID:");
    borderIdSizer->Add(borderIdLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    wxStaticText* borderId = new wxStaticText(m_groundPanel, wxID_ANY, "Uses 'Common Properties > ID' when Border IDs is empty");
    borderId->SetForegroundColour(*wxRED);
    borderIdSizer->Add(borderId, 1, wxALIGN_CENTER_VERTICAL);
    
    groundBorderSizer->Add(borderIdSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    
    // Grid use instruction - shorter text
    wxStaticText* gridInstructions = new wxStaticText(m_groundPanel, wxID_ANY, 
        "Use the grid in the Border tab to define borders for this ground brush.");
    gridInstructions->SetForegroundColour(*wxBLUE);
    groundBorderSizer->Add(gridInstructions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    
    groundSizer->Add(groundBorderSizer, 0, wxEXPAND | wxALL, 5);
    
    // Bottom buttons for ground tab
    wxBoxSizer* groundButtonSizer = new wxBoxSizer(wxHORIZONTAL);
    m_clearGroundButton = new wxButton(m_groundPanel, wxID_CLEAR, "Clear");
    groundButtonSizer->Add(m_clearGroundButton, 0, wxRIGHT, 5);
    m_saveGroundButton = new wxButton(m_groundPanel, wxID_SAVE, "Save Ground");
    groundButtonSizer->Add(m_saveGroundButton, 0, wxRIGHT, 5);
    groundButtonSizer->AddStretchSpacer(1);
    groundButtonSizer->Add(new wxButton(m_groundPanel, wxID_CLOSE, "Close"), 0);
    
    groundSizer->Add(groundButtonSizer, 0, wxEXPAND | wxALL, 5);
    
    m_groundPanel->SetSizer(groundSizer);
    
    // Add tabs to notebook
    m_notebook->AddPage(m_borderPanel, "Border");
    m_notebook->AddPage(m_groundPanel, "Ground");
    
    topSizer->Add(m_notebook, 1, wxEXPAND | wxALL, 5);
    
    SetSizer(topSizer);
    if(m_borderListCtrl) {
        m_borderListCtrl->Bind(wxEVT_TEXT, &BorderEditorDialog::OnBorderListTextChanged, this);
    }
    if(m_borderIdsCtrl) {
        m_borderIdsCtrl->Bind(wxEVT_TEXT, &BorderEditorDialog::OnBorderListTextChanged, this);
    }
    if(m_borderToNoneIdsCtrl) {
        m_borderToNoneIdsCtrl->Bind(wxEVT_TEXT, &BorderEditorDialog::OnBorderListTextChanged, this);
    }
    if(m_promoteBorderListIdButton) {
        m_promoteBorderListIdButton->Bind(wxEVT_BUTTON, &BorderEditorDialog::OnPromoteBorderListId, this);
    }
    if(m_searchBorderListButton) {
        m_searchBorderListButton->Bind(wxEVT_BUTTON, &BorderEditorDialog::OnSearchBorderListId, this);
    }
    if(m_borderIdsPreviewCtrl) {
        m_borderIdsPreviewCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &BorderEditorDialog::OnBorderIdsPreviewSelected, this);
    }
    if(m_borderToNoneIdsPreviewCtrl) {
        m_borderToNoneIdsPreviewCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &BorderEditorDialog::OnToNoneIdsPreviewSelected, this);
    }
    if(m_borderListIdsPreviewCtrl) {
        m_borderListIdsPreviewCtrl->Bind(wxEVT_LIST_ITEM_SELECTED, &BorderEditorDialog::OnBorderListIdsPreviewSelected, this);
    }
    RefreshBorderListIdsUI();
    Layout();
}

void BorderEditorDialog::ConfigureModifyGroundBordersMode() {
    SetTitle("Modify Ground Borders");

    // Focus this workflow on the ground tab and keep border definitions read-only here.
    if(m_notebook) {
        m_notebook->ChangeSelection(1);
        m_activeTab = 1;
    }
    if(m_borderPanel) {
        m_borderPanel->Enable(false);
    }

    // Existing ground brush selector remains editable; all creation-oriented fields are read-only.
    if(m_nameCtrl) {
        m_nameCtrl->SetEditable(false);
    }
    if(m_idCtrl) {
        m_idCtrl->Enable(false);
    }
    if(m_serverLookIdCtrl) {
        m_serverLookIdCtrl->Enable(false);
    }
    if(m_zOrderCtrl) {
        m_zOrderCtrl->Enable(false);
    }
    if(m_groundItemIdCtrl) {
        m_groundItemIdCtrl->Enable(false);
    }
    if(m_groundItemChanceCtrl) {
        m_groundItemChanceCtrl->Enable(false);
    }
    if(m_addGroundItemButton) {
        m_addGroundItemButton->Enable(false);
    }
    if(m_removeGroundItemButton) {
        m_removeGroundItemButton->Enable(false);
    }
    if(m_clearGroundButton) {
        m_clearGroundButton->Enable(false);
    }
    if(m_tilesetChoice) {
        m_tilesetChoice->Enable(false);
    }
    if(m_saveGroundButton) {
        m_saveGroundButton->SetLabel("Save Borders");
    }

    if(!m_initialGroundBrushName.IsEmpty()) {
        SelectGroundBrushByName(m_initialGroundBrushName);
    }
}

void BorderEditorDialog::SelectGroundBrushByName(const wxString& brushName) {
    if(!m_existingGroundBrushesCombo || brushName.IsEmpty()) {
        return;
    }

    const unsigned int count = m_existingGroundBrushesCombo->GetCount();
    for(unsigned int i = 1; i < count; ++i) {
        if(m_existingGroundBrushesCombo->GetString(i) == brushName) {
            m_existingGroundBrushesCombo->SetSelection(i);
            wxCommandEvent loadEvent(wxEVT_COMBOBOX, m_existingGroundBrushesCombo->GetId());
            OnLoadGroundBrush(loadEvent);
            return;
        }
    }
}

void BorderEditorDialog::LoadExistingBorders() {
    // Clear the combobox
    m_existingBordersCombo->Clear();
    
    // Add an empty entry
    m_existingBordersCombo->Append("<Create New>");
    m_existingBordersCombo->SetSelection(0);
    
    wxString dataDir = GetVersionDataDirectory();
    wxString bordersFile = dataDir + "borders.xml";
    
    if (!wxFileExists(bordersFile)) {
        wxMessageBox("Cannot find borders.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }
    
    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(bordersFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load borders.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    pugi::xml_node materials = doc.child("materials");
    if (!materials) {
        wxMessageBox("Invalid borders.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    int highestId = 0;
    
    // Parse all borders
    for (pugi::xml_node borderNode = materials.child("border"); borderNode; borderNode = borderNode.next_sibling("border")) {
        pugi::xml_attribute idAttr = borderNode.attribute("id");
        if (!idAttr) continue;
        
        int id = idAttr.as_int();
        if (id > highestId) {
            highestId = id;
        }
        
        // Get the comment node before this border for its description
        std::string description;
        pugi::xml_node commentNode = borderNode.previous_sibling();
        if (commentNode && commentNode.type() == pugi::node_comment) {
            description = commentNode.value();
            // Extract the actual comment text by removing XML comment markers
            description = description.c_str(); // Ensure we have a clean copy
            
            // Trim leading and trailing whitespace first
            description.erase(0, description.find_first_not_of(" \t\n\r"));
            description.erase(description.find_last_not_of(" \t\n\r") + 1);
            
            // Remove leading "<!--" if present
            if (description.substr(0, 4) == "<!--") {
                description.erase(0, 4);
                // Trim whitespace after removing the marker
                description.erase(0, description.find_first_not_of(" \t\n\r"));
            }
            
            // Remove trailing "-->" if present
            if (description.length() >= 3 && description.substr(description.length() - 3) == "-->") {
                description.erase(description.length() - 3);
                // Trim whitespace after removing the marker
                description.erase(description.find_last_not_of(" \t\n\r") + 1);
            }
        }
        
        // Add to combobox
        wxString label = wxString::Format("Border %d", id);
        if (!description.empty()) {
            label += wxString::Format(" (%s)", wxstr(description));
        }
        
        m_existingBordersCombo->Append(label, new wxStringClientData(wxString::Format("%d", id)));
    }
    
    // Set the next border ID to one higher than the highest found
    m_nextBorderId = highestId + 1;
    m_idCtrl->SetValue(m_nextBorderId);
}

void BorderEditorDialog::OnLoadBorder(wxCommandEvent& event) {
    int selection = m_existingBordersCombo->GetSelection();
    if (selection <= 0) {
        // Selected "Create New" or nothing
        ClearItems();
        return;
    }
    
    wxStringClientData* data = static_cast<wxStringClientData*>(m_existingBordersCombo->GetClientObject(selection));
    if (!data) return;
    
    int borderId = wxAtoi(data->GetData());
    
    wxString dataDir = GetVersionDataDirectory();
    wxString bordersFile = dataDir + "borders.xml";
    
    if (!wxFileExists(bordersFile)) {
        wxMessageBox("Cannot find borders.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }
    
    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(bordersFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load borders.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    // Clear existing items
    ClearItems();
    
    // Look for the border with the specified ID
    pugi::xml_node materials = doc.child("materials");
    for (pugi::xml_node borderNode = materials.child("border"); borderNode; borderNode = borderNode.next_sibling("border")) {
        pugi::xml_attribute idAttr = borderNode.attribute("id");
        if (!idAttr || idAttr.as_int() != borderId) continue;
        
        // Set the ID in the control
        m_idCtrl->SetValue(borderId);
        
        // Check for border type
        pugi::xml_attribute typeAttr = borderNode.attribute("type");
        if (typeAttr) {
            std::string type = typeAttr.as_string();
            m_isOptionalCheck->SetValue(type == "optional");
        } else {
            m_isOptionalCheck->SetValue(false);
        }
        
        // Check for border group
        pugi::xml_attribute groupAttr = borderNode.attribute("group");
        if (groupAttr) {
            m_groupCtrl->SetValue(groupAttr.as_int());
        } else {
            m_groupCtrl->SetValue(0);
        }
        
        // Get the comment node before this border for its description
        pugi::xml_node commentNode = borderNode.previous_sibling();
        if (commentNode && commentNode.type() == pugi::node_comment) {
            std::string description = commentNode.value();
            // Extract the actual comment text by removing XML comment markers
            description = description.c_str(); // Ensure we have a clean copy
            
            // Trim leading and trailing whitespace first
            description.erase(0, description.find_first_not_of(" \t\n\r"));
            description.erase(description.find_last_not_of(" \t\n\r") + 1);
            
            // Remove leading "<!--" if present
            if (description.substr(0, 4) == "<!--") {
                description.erase(0, 4);
                // Trim whitespace after removing the marker
                description.erase(0, description.find_first_not_of(" \t\n\r"));
            }
            
            // Remove trailing "-->" if present
            if (description.length() >= 3 && description.substr(description.length() - 3) == "-->") {
                description.erase(description.length() - 3);
                // Trim whitespace after removing the marker
                description.erase(description.find_last_not_of(" \t\n\r") + 1);
            }
            
            m_nameCtrl->SetValue(wxstr(description));
        } else {
            m_nameCtrl->SetValue("");
        }
        
        // Load all border items
        for (pugi::xml_node itemNode = borderNode.child("borderitem"); itemNode; itemNode = itemNode.next_sibling("borderitem")) {
            pugi::xml_attribute edgeAttr = itemNode.attribute("edge");
            pugi::xml_attribute itemAttr = itemNode.attribute("item");
            
            if (!edgeAttr || !itemAttr) continue;
            
            BorderEdgePosition pos = edgeStringToPosition(edgeAttr.as_string());
            uint16_t itemId = itemAttr.as_uint();
            
            if (pos != EDGE_NONE && itemId > 0) {
                m_borderItems.push_back(BorderItem(pos, itemId));
                m_gridPanel->SetItemId(pos, itemId);
            }
        }
        
        break;
    }
    
    // Update the preview
    UpdatePreview();
    
    // Keep selection
    m_existingBordersCombo->SetSelection(selection);
}

void BorderEditorDialog::OnItemIdChanged(wxCommandEvent& event) {
    // This event handler would update the display when an item ID is entered manually
    // but we're handling this directly in OnAddItem instead
}

void BorderEditorDialog::ApplyItemToPosition(BorderEdgePosition pos, uint16_t itemId) {
    if (pos == EDGE_NONE || itemId == 0) {
        return;
    }

    if (m_itemIdCtrl) {
        m_itemIdCtrl->SetValue(itemId);
    }

    bool updated = false;
    for (size_t i = 0; i < m_borderItems.size(); i++) {
        if (m_borderItems[i].position == pos) {
            m_borderItems[i].itemId = itemId;
            updated = true;
            break;
        }
    }

    if (!updated) {
        m_borderItems.push_back(BorderItem(pos, itemId));
    }

    m_gridPanel->SetItemId(pos, itemId);
    UpdatePreview();
}

void BorderEditorDialog::OnBrowse(wxCommandEvent& event) {
    // Open the Find Item dialog instead
    FindItemDialog dialog(this, "Select Border Item");
    
    if (dialog.ShowModal() == wxID_OK) {
        // Get the selected item ID
        uint16_t itemId = dialog.getResultID();
        
        // Find the item ID spin control
        wxSpinCtrl* itemIdCtrl = nullptr;
        wxWindowList& children = GetChildren();
        for (wxWindowList::iterator it = children.begin(); it != children.end(); ++it) {
            wxSpinCtrl* spinCtrl = dynamic_cast<wxSpinCtrl*>(*it);
            if (spinCtrl && spinCtrl != m_idCtrl) {
                itemIdCtrl = spinCtrl;
                break;
            }
        }
        
        if (itemIdCtrl && itemId > 0) {
            itemIdCtrl->SetValue(itemId);
        }
    }
}

void BorderEditorDialog::OnPositionSelected(wxCommandEvent& event) {
    // Get the position from the event
    BorderEdgePosition pos = static_cast<BorderEdgePosition>(event.GetInt());
    wxLogDebug("OnPositionSelected: Position %s selected", wxstr(edgePositionToString(pos)).c_str());
    OutputDebugStringA(wxString::Format("BorderEditor: Position %s selected\n", wxstr(edgePositionToString(pos)).c_str()).mb_str());
    
    // Get the item ID from the current brush
    Brush* currentBrush = g_gui.GetCurrentBrush();
    if (!currentBrush) {
        wxLogDebug("OnPositionSelected: No current brush selected");
        OutputDebugStringA("BorderEditor: No current brush selected\n");
        wxMessageBox("Please select a brush or item first.", "No Brush Selected", wxICON_INFORMATION);
        return;
    }
    
    wxLogDebug("OnPositionSelected: Using brush: %s", wxString(currentBrush->getName()).c_str());
    OutputDebugStringA(wxString::Format("BorderEditor: Using brush: %s\n", wxString(currentBrush->getName()).c_str()).mb_str());
    
    // Try to get the item ID directly - check if it's a RAW brush first
    uint16_t itemId = 0;
    if (currentBrush->isRaw()) {
        RAWBrush* rawBrush = currentBrush->asRaw();
        if (rawBrush) {
            itemId = rawBrush->getItemID();
            wxLogDebug("OnPositionSelected: Got item ID %d directly from RAW brush", itemId);
            OutputDebugStringA(wxString::Format("BorderEditor: Got item ID %d directly from RAW brush\n", itemId).mb_str());
        } else {
            wxLogDebug("OnPositionSelected: Failed to cast to RAW brush");
            OutputDebugStringA("BorderEditor: Failed to cast to RAW brush\n");
        }
    } else {
        OutputDebugStringA(wxString::Format("BorderEditor: Current brush is NOT a RAW brush, is: %s\n", 
            currentBrush->isGround() ? "Ground" : 
            currentBrush->isWall() ? "Wall" : 
            currentBrush->isDoodad() ? "Doodad" : "Other").mb_str());
    }
    
    // If we didn't get an ID from the RAW brush method, try the generic method
    if (itemId == 0) {
        itemId = GetItemIDFromBrush(currentBrush);
        wxLogDebug("OnPositionSelected: Got item ID %d from GetItemIDFromBrush", itemId);
        OutputDebugStringA(wxString::Format("BorderEditor: Got item ID %d from GetItemIDFromBrush\n", itemId).mb_str());
    }
    
    if (itemId > 0) {
        ApplyItemToPosition(pos, itemId);
    } else {
        // If we couldn't get an item ID from the brush, check if there's a value in the item ID control
        itemId = m_itemIdCtrl->GetValue();
        
        if (itemId > 0) {
            ApplyItemToPosition(pos, itemId);
        } else {
            wxLogDebug("No valid item ID found from current brush: %s", wxString(currentBrush->getName()).c_str());
            OutputDebugStringA(wxString::Format("BorderEditor: No valid item ID found from current brush: %s\n", wxString(currentBrush->getName()).c_str()).mb_str());
            wxMessageBox("Could not get a valid item ID from the current brush. Please select an item brush or use the Browse button to select an item manually.", "Invalid Brush", wxICON_INFORMATION);
        }
    }
}

void BorderEditorDialog::OnAddItem(wxCommandEvent& event) {
    // Get the currently selected position in the grid panel
    static BorderEdgePosition lastSelectedPos = EDGE_NONE;
    BorderEdgePosition selectedPos = m_gridPanel->GetSelectedPosition();
    
    // If no position is currently selected, use the last selected position
    if (selectedPos == EDGE_NONE) {
        selectedPos = lastSelectedPos;
    }
    
    if (selectedPos == EDGE_NONE) {
        wxMessageBox("Please select a position on the grid first by clicking on it.", "Error", wxICON_ERROR);
        return;
    }
    
    // Save this position for future use
    lastSelectedPos = selectedPos;
    
    // Get the item ID from the control (now using the class member)
    uint16_t itemId = m_itemIdCtrl->GetValue();
    
    if (itemId == 0) {
        wxMessageBox("Please enter a valid item ID or use the Browse button.", "Error", wxICON_ERROR);
        return;
    }
    
    ApplyItemToPosition(selectedPos, itemId);
    
    // Log the addition for debugging
    wxLogDebug("Added item ID %d at position %s via Add button", 
               itemId, wxstr(edgePositionToString(selectedPos)).c_str());
}

void BorderEditorDialog::OnClear(wxCommandEvent& event) {
    if(m_modifyGroundBordersOnly && m_activeTab != 0) {
        return;
    }

    if (m_activeTab == 0) {
        // Border tab
    ClearItems();
    } else {
        // Ground tab
        ClearGroundItems();
    }
}

void BorderEditorDialog::ClearItems() {
    m_borderItems.clear();
    m_gridPanel->Clear();
    m_previewPanel->Clear();
    
    // Reset controls to defaults
    m_idCtrl->SetValue(m_nextBorderId);
    m_nameCtrl->SetValue("");
    m_isOptionalCheck->SetValue(false);
    m_isGroundCheck->SetValue(false);
    m_groupCtrl->SetValue(0);
    
    // Set combo selection to "Create New"
    m_existingBordersCombo->SetSelection(0);
}

void BorderEditorDialog::UpdatePreview() {
    m_previewPanel->SetBorderItems(m_borderItems);
    m_previewPanel->Refresh();
}

bool BorderEditorDialog::ValidateBorder() {
    // Check for empty name
    if (m_nameCtrl->GetValue().IsEmpty()) {
        wxMessageBox("Please enter a name for the border.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    if (m_borderItems.empty()) {
        wxMessageBox("The border must have at least one item.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    // Check that there are no duplicate positions
    std::set<BorderEdgePosition> positions;
    for (const BorderItem& item : m_borderItems) {
        if (positions.find(item.position) != positions.end()) {
            wxMessageBox("The border contains duplicate positions.", "Validation Error", wxICON_ERROR);
            return false;
        }
        positions.insert(item.position);
    }
    
    // Check for ID validity
    int id = m_idCtrl->GetValue();
    if (id <= 0) {
        wxMessageBox("Border ID must be greater than 0.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    return true;
}

void BorderEditorDialog::SaveBorder() {
    if (!ValidateBorder()) {
        return;
    }
    
    // Get the border properties
    int id = m_idCtrl->GetValue();
    wxString name = m_nameCtrl->GetValue();
    
    // Double check that we have a name (it's also checked in ValidateBorder)
    if (name.IsEmpty()) {
        wxMessageBox("You must provide a name for the border.", "Error", wxICON_ERROR);
        return;
    }
    
    bool isOptional = m_isOptionalCheck->GetValue();
    bool isGround = m_isGroundCheck->GetValue();
    int group = m_groupCtrl->GetValue();
    
    wxString dataDir = GetVersionDataDirectory();
    wxString bordersFile = dataDir + "borders.xml";
    
    if (!wxFileExists(bordersFile)) {
        wxMessageBox("Cannot find borders.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }
    
    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(bordersFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load borders.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    pugi::xml_node materials = doc.child("materials");
    if (!materials) {
        wxMessageBox("Invalid borders.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    // Check if a border with this ID already exists
    bool borderExists = false;
    pugi::xml_node existingBorder;
    
    for (pugi::xml_node borderNode = materials.child("border"); borderNode; borderNode = borderNode.next_sibling("border")) {
        pugi::xml_attribute idAttr = borderNode.attribute("id");
        if (idAttr && idAttr.as_int() == id) {
            borderExists = true;
            existingBorder = borderNode;
            break;
        }
    }
    
    if (borderExists) {
        // Check if there's a comment node before the existing border
        pugi::xml_node commentNode = existingBorder.previous_sibling();
        bool hadComment = (commentNode && commentNode.type() == pugi::node_comment);
        
        // Ask for confirmation to overwrite
        if (wxMessageBox("A border with ID " + wxString::Format("%d", id) + " already exists. Do you want to overwrite it?", 
                        "Confirm Overwrite", wxYES_NO | wxICON_QUESTION) != wxYES) {
            return;
        }
        
        // If there was a comment node, remove it too
        if (hadComment) {
            materials.remove_child(commentNode);
        }
        
        // Remove the existing border
        materials.remove_child(existingBorder);
    }
    
 
    
    // Create the new border node
    pugi::xml_node borderNode = materials.append_child("border");
    borderNode.append_attribute("id").set_value(id);
    
    if (isOptional) {
        borderNode.append_attribute("type").set_value("optional");
    }
    
    if (isGround) {
        borderNode.append_attribute("ground").set_value("true");
    }
    
    if (group > 0) {
        borderNode.append_attribute("group").set_value(group);
    }
    
    // Add all border items
    for (const BorderItem& item : m_borderItems) {
        pugi::xml_node itemNode = borderNode.append_child("borderitem");
        itemNode.append_attribute("edge").set_value(edgePositionToString(item.position).c_str());
        itemNode.append_attribute("item").set_value(item.itemId);
    }
    
    // Save the file
    if (!doc.save_file(nstr(bordersFile).c_str())) {
        wxMessageBox("Failed to save changes to borders.xml", "Error", wxICON_ERROR);
        return;
    }

    m_borderPreviewCatalogLoaded = false;
    m_borderPreviewItemById.clear();
    RefreshInlineBorderIdPreviews();
    
    wxMessageBox("Border saved successfully.", "Success", wxICON_INFORMATION);
    
    // Reload the existing borders list
    LoadExistingBorders();
}

void BorderEditorDialog::OnSave(wxCommandEvent& event) {
    if(m_modifyGroundBordersOnly) {
        SaveGroundBrush();
        return;
    }

    if (m_activeTab == 0) {
        // Border tab
    SaveBorder();
    } else {
        // Ground tab
        SaveGroundBrush();
    }
}

void BorderEditorDialog::OnClose(wxCommandEvent& event) {
    Close();
}

void BorderEditorDialog::OnGridCellClicked(wxMouseEvent& event) {
    // This event is handled by the BorderGridPanel directly
    event.Skip();
}

// ============================================================================
// BorderItemButton

BorderItemButton::BorderItemButton(wxWindow* parent, BorderEdgePosition position, wxWindowID id) :
    wxButton(parent, id, "", wxDefaultPosition, wxSize(32, 32)),
    m_itemId(0),
    m_position(position) {
    // Set up the button to show sprite graphics
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

BorderItemButton::~BorderItemButton() {
    // No need to destroy anything manually
}

void BorderItemButton::SetItemId(uint16_t id) {
    m_itemId = id;
    Refresh();
}

void BorderItemButton::OnPaint(wxPaintEvent& event) {
    wxPaintDC dc(this);
    
    // Draw the button background
    wxRect rect = GetClientRect();
    dc.SetBrush(wxBrush(GetBackgroundColour()));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(rect);
    
    // Draw the item sprite if available
    if (m_itemId > 0) {
        const ItemType& type = g_items.getItemType(m_itemId);
        if (type.id != 0) {
            Sprite* sprite = g_gui.gfx.getSprite(type.clientID);
            if (sprite) {
                sprite->DrawTo(&dc, SPRITE_SIZE_32x32, 0, 0, rect.GetWidth(), rect.GetHeight());
            }
        }
    }
    
    // Draw a border around the button if it's focused
    if (HasFocus()) {
        dc.SetPen(*wxBLACK_PEN);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.DrawRectangle(rect);
    }
}

// ============================================================================
// BorderGridPanel

BorderGridPanel::BorderGridPanel(wxWindow* parent, wxWindowID id) :
    wxPanel(parent, id, wxDefaultPosition, wxDefaultSize, wxBORDER_SUNKEN)
{
    m_items.clear();
    m_selectedPosition = EDGE_NONE;
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

BorderGridPanel::~BorderGridPanel() {
    // Nothing to destroy manually
}

void BorderGridPanel::SetItemId(BorderEdgePosition pos, uint16_t itemId) {
    if (pos >= 0 && pos < EDGE_COUNT) {
        m_items[pos] = itemId;
        Refresh();
    }
}

uint16_t BorderGridPanel::GetItemId(BorderEdgePosition pos) const {
    auto it = m_items.find(pos);
    if (it != m_items.end()) {
        return it->second;
    }
    return 0;
}

void BorderGridPanel::Clear() {
    for (auto& item : m_items) {
        item.second = 0;
    }
    Refresh();
}

void BorderGridPanel::SetSelectedPosition(BorderEdgePosition pos) {
    m_selectedPosition = pos;
    Refresh();
}

void BorderGridPanel::OnPaint(wxPaintEvent& event) {
    wxAutoBufferedPaintDC dc(this);
    
    // Draw the panel background
    wxRect rect = GetClientRect();
    dc.SetBackground(wxBrush(wxColour(200, 200, 200)));
    dc.Clear();
    
    // Draw the grid layout
    dc.SetPen(wxPen(wxColour(100, 100, 100)));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    
    // Calculate sizes for the three grid sections
    const int total_width = rect.GetWidth();
    const int total_height = rect.GetHeight();
    const int grid_cell_size = 64;
    const int CELL_PADDING = 4;
    
    // Section 1: Normal directions (N, E, S, W) - 2x2 grid
    const int normal_grid_size = 2;
    const int normal_grid_width = normal_grid_size * grid_cell_size;
    const int normal_grid_height = normal_grid_size * grid_cell_size;
    const int normal_offset_x = (total_width / 3 - normal_grid_width) / 2;
    const int normal_offset_y = (total_height / 2 - normal_grid_height) / 2;
    
    // Section 2: Corner positions (CNW, CNE, CSW, CSE) - 2x2 grid
    const int corner_grid_size = 2;
    const int corner_grid_width = corner_grid_size * grid_cell_size;
    const int corner_grid_height = corner_grid_size * grid_cell_size;
    const int corner_offset_x = total_width / 3 + (total_width / 3 - corner_grid_width) / 2;
    const int corner_offset_y = (total_height / 2 - corner_grid_height) / 2;
    
    // Section 3: Diagonal positions (DNW, DNE, DSW, DSE) - 2x2 grid
    const int diag_grid_size = 2;
    const int diag_grid_width = diag_grid_size * grid_cell_size;
    const int diag_grid_height = diag_grid_size * grid_cell_size;
    const int diag_offset_x = 2 * total_width / 3 + (total_width / 3 - diag_grid_width) / 2;
    const int diag_offset_y = (total_height / 2 - diag_grid_height) / 2;
    
    // Section labels
    dc.SetTextForeground(wxColour(0, 0, 0));
    dc.SetFont(wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    
    dc.DrawText("Normal", normal_offset_x, 10);
    dc.DrawText("Corner", corner_offset_x, 10);
    dc.DrawText("Diagonal", diag_offset_x, 10);
    
    // Helper function to draw a grid
    auto drawGrid = [&](int offsetX, int offsetY, int gridSize, int cellSize) {
        for (int i = 0; i <= gridSize; i++) {
            // Vertical lines
            dc.DrawLine(
                offsetX + i * cellSize, 
                offsetY, 
                offsetX + i * cellSize, 
                offsetY + gridSize * cellSize
            );
            
            // Horizontal lines
            dc.DrawLine(
                offsetX, 
                offsetY + i * cellSize, 
                offsetX + gridSize * cellSize, 
                offsetY + i * cellSize
            );
        }
    };
    
    // Draw the three grid sections
    drawGrid(normal_offset_x, normal_offset_y, normal_grid_size, grid_cell_size);
    drawGrid(corner_offset_x, corner_offset_y, corner_grid_size, grid_cell_size);
    drawGrid(diag_offset_x, diag_offset_y, diag_grid_size, grid_cell_size);
    
    // Set font for position labels
    dc.SetTextForeground(wxColour(50, 50, 50));
    dc.SetFont(wxFont(8, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    
    // Function to draw an item at a position
    auto drawItemAtPos = [&](BorderEdgePosition pos, int gridX, int gridY, int offsetX, int offsetY) {
        int x = offsetX + gridX * grid_cell_size + CELL_PADDING;
        int y = offsetY + gridY * grid_cell_size + CELL_PADDING;
        
        // Highlight selected position
        if (pos == m_selectedPosition) {
            dc.SetPen(*wxRED_PEN);
            dc.SetBrush(wxBrush(wxColour(255, 200, 200)));
            dc.DrawRectangle(x - CELL_PADDING, y - CELL_PADDING, 
                         grid_cell_size, grid_cell_size);
            dc.SetPen(wxPen(wxColour(100, 100, 100)));
        }
        
        // Draw position label
        wxString label = wxstr(edgePositionToString(pos));
        wxSize textSize = dc.GetTextExtent(label);
        dc.DrawText(label, x + (grid_cell_size - 2 * CELL_PADDING - textSize.GetWidth()) / 2, 
                  y + grid_cell_size - 2 * CELL_PADDING - textSize.GetHeight());
        
        // Draw sprite if available
        uint16_t itemId = GetItemId(pos);
        if (itemId > 0) {
            const ItemType& type = g_items.getItemType(itemId);
            if (type.id != 0) {
                Sprite* sprite = g_gui.gfx.getSprite(type.clientID);
                if (sprite) {
                    sprite->DrawTo(&dc, SPRITE_SIZE_32x32, 
                                 x, y, 
                                 grid_cell_size - 2 * CELL_PADDING, 
                                 grid_cell_size - 2 * CELL_PADDING);
                }
            }
        }
    };
    
    // Draw normal direction items
    drawItemAtPos(EDGE_N, 0, 0, normal_offset_x, normal_offset_y);
    drawItemAtPos(EDGE_E, 1, 0, normal_offset_x, normal_offset_y);
    drawItemAtPos(EDGE_S, 0, 1, normal_offset_x, normal_offset_y);
    drawItemAtPos(EDGE_W, 1, 1, normal_offset_x, normal_offset_y);
    
    // Draw corner items
    drawItemAtPos(EDGE_CNW, 0, 0, corner_offset_x, corner_offset_y);
    drawItemAtPos(EDGE_CNE, 1, 0, corner_offset_x, corner_offset_y);
    drawItemAtPos(EDGE_CSW, 0, 1, corner_offset_x, corner_offset_y);
    drawItemAtPos(EDGE_CSE, 1, 1, corner_offset_x, corner_offset_y);
    
    // Draw diagonal items
    drawItemAtPos(EDGE_DNW, 0, 0, diag_offset_x, diag_offset_y);
    drawItemAtPos(EDGE_DNE, 1, 0, diag_offset_x, diag_offset_y);
    drawItemAtPos(EDGE_DSW, 0, 1, diag_offset_x, diag_offset_y);
    drawItemAtPos(EDGE_DSE, 1, 1, diag_offset_x, diag_offset_y);
    
    // Remove the second row of information since we've added better instructions to the panel
}

wxPoint BorderGridPanel::GetPositionCoordinates(BorderEdgePosition pos) const {
    switch (pos) {
        case EDGE_N:   return wxPoint(1, 0);
        case EDGE_E:   return wxPoint(2, 1);
        case EDGE_S:   return wxPoint(1, 2);
        case EDGE_W:   return wxPoint(0, 1);
        case EDGE_CNW: return wxPoint(0, 0);
        case EDGE_CNE: return wxPoint(2, 0);
        case EDGE_CSE: return wxPoint(2, 2);
        case EDGE_CSW: return wxPoint(0, 2);
        case EDGE_DNW: return wxPoint(0.5, 0.5);
        case EDGE_DNE: return wxPoint(1.5, 0.5);
        case EDGE_DSE: return wxPoint(1.5, 1.5);
        case EDGE_DSW: return wxPoint(0.5, 1.5);
        default:       return wxPoint(-1, -1);
    }
}

BorderEdgePosition BorderGridPanel::GetPositionFromCoordinates(int x, int y) const
{
    // Calculate sizes for the three grid sections
    wxSize size = GetClientSize();
    const int total_width = size.GetWidth();
    const int total_height = size.GetHeight();
    const int grid_cell_size = 64;
    
    // Section 1: Normal directions (N, E, S, W) - 2x2 grid
    const int normal_grid_size = 2;
    const int normal_grid_width = normal_grid_size * grid_cell_size;
    const int normal_grid_height = normal_grid_size * grid_cell_size;
    const int normal_offset_x = (total_width / 3 - normal_grid_width) / 2;
    const int normal_offset_y = (total_height / 2 - normal_grid_height) / 2;
    
    // Section 2: Corner positions (CNW, CNE, CSW, CSE) - 2x2 grid
    const int corner_grid_size = 2;
    const int corner_grid_width = corner_grid_size * grid_cell_size;
    const int corner_grid_height = corner_grid_size * grid_cell_size;
    const int corner_offset_x = total_width / 3 + (total_width / 3 - corner_grid_width) / 2;
    const int corner_offset_y = (total_height / 2 - corner_grid_height) / 2;
    
    // Section 3: Diagonal positions (DNW, DNE, DSW, DSE) - 2x2 grid
    const int diag_grid_size = 2;
    const int diag_grid_width = diag_grid_size * grid_cell_size;
    const int diag_grid_height = diag_grid_size * grid_cell_size;
    const int diag_offset_x = 2 * total_width / 3 + (total_width / 3 - diag_grid_width) / 2;
    const int diag_offset_y = (total_height / 2 - diag_grid_height) / 2;
    
    // Check which grid section the click is in and calculate the grid cell
    
    // Normal grid
    if (x >= normal_offset_x && x < normal_offset_x + normal_grid_width &&
        y >= normal_offset_y && y < normal_offset_y + normal_grid_height) {
        
        int gridX = (x - normal_offset_x) / grid_cell_size;
        int gridY = (y - normal_offset_y) / grid_cell_size;
        
        if (gridX == 0 && gridY == 0) return EDGE_N;
        if (gridX == 1 && gridY == 0) return EDGE_E;
        if (gridX == 0 && gridY == 1) return EDGE_S;
        if (gridX == 1 && gridY == 1) return EDGE_W;
    }
    
    // Corner grid
    if (x >= corner_offset_x && x < corner_offset_x + corner_grid_width &&
        y >= corner_offset_y && y < corner_offset_y + corner_grid_height) {
        
        int gridX = (x - corner_offset_x) / grid_cell_size;
        int gridY = (y - corner_offset_y) / grid_cell_size;
        
        if (gridX == 0 && gridY == 0) return EDGE_CNW;
        if (gridX == 1 && gridY == 0) return EDGE_CNE;
        if (gridX == 0 && gridY == 1) return EDGE_CSW;
        if (gridX == 1 && gridY == 1) return EDGE_CSE;
    }
    
    // Diagonal grid
    if (x >= diag_offset_x && x < diag_offset_x + diag_grid_width &&
        y >= diag_offset_y && y < diag_offset_y + diag_grid_height) {
        
        int gridX = (x - diag_offset_x) / grid_cell_size;
        int gridY = (y - diag_offset_y) / grid_cell_size;
        
        if (gridX == 0 && gridY == 0) return EDGE_DNW;
        if (gridX == 1 && gridY == 0) return EDGE_DNE;
        if (gridX == 0 && gridY == 1) return EDGE_DSW;
        if (gridX == 1 && gridY == 1) return EDGE_DSE;
    }
    
    return EDGE_NONE;
}

void BorderGridPanel::OnMouseClick(wxMouseEvent& event) {
    int x = event.GetX();
    int y = event.GetY();
    
    BorderEdgePosition pos = GetPositionFromCoordinates(x, y);
    if (pos != EDGE_NONE) {
        // Set the position as selected in the grid
        SetSelectedPosition(pos);
        
        // Notify the parent dialog that a position was selected
        wxCommandEvent selEvent(wxEVT_COMMAND_BUTTON_CLICKED, ID_BORDER_GRID_SELECT);
        selEvent.SetInt(static_cast<int>(pos));
        
        // Find the parent BorderEditorDialog
        wxWindow* parent = GetParent();
        while (parent && !dynamic_cast<BorderEditorDialog*>(parent)) {
            parent = parent->GetParent();
        }
        
        // Send the event to the parent dialog
        BorderEditorDialog* dialog = dynamic_cast<BorderEditorDialog*>(parent);
        if (dialog) {
            // Call the event handler directly
            OutputDebugStringA(wxString::Format("BorderGridPanel::OnMouseClick: Calling OnPositionSelected directly for position %s\n", 
                            wxstr(edgePositionToString(pos)).c_str()).mb_str());
            dialog->OnPositionSelected(selEvent);
        } else {
            // If we couldn't find the parent dialog, post the event to the parent
            OutputDebugStringA("BorderGridPanel::OnMouseClick: Could not find BorderEditorDialog parent, posting event\n");
            wxPostEvent(GetParent(), selEvent);
        }
    }
}

void BorderGridPanel::OnMouseDown(wxMouseEvent& event) {
    // Get the position from the coordinates
    BorderEdgePosition pos = GetPositionFromCoordinates(event.GetX(), event.GetY());
    
    // Set the selected position
    SetSelectedPosition(pos);
    
    event.Skip();
}

// ============================================================================
// BorderPreviewPanel

BorderPreviewPanel::BorderPreviewPanel(wxWindow* parent, wxWindowID id) :
    wxPanel(parent, id, wxDefaultPosition, wxSize(BORDER_PREVIEW_SIZE, BORDER_PREVIEW_SIZE)) {
    // Set up the panel to handle painting
    SetBackgroundStyle(wxBG_STYLE_PAINT);
}

BorderPreviewPanel::~BorderPreviewPanel() {
    // Nothing to destroy manually
}

void BorderPreviewPanel::SetBorderItems(const std::vector<BorderItem>& items) {
    m_borderItems = items;
    Refresh();
}

void BorderPreviewPanel::Clear() {
    m_borderItems.clear();
    Refresh();
}

void BorderPreviewPanel::OnPaint(wxPaintEvent& event) {
    wxAutoBufferedPaintDC dc(this);
    
    // Draw the panel background
    wxRect rect = GetClientRect();
    dc.SetBackground(wxBrush(wxColour(240, 240, 240)));
    dc.Clear();
    
    // Draw a grid to simulate a map
    const int GRID_SIZE = 5;
    const int preview_cell_size = BORDER_PREVIEW_SIZE / GRID_SIZE;
    
    // Draw grid lines
    dc.SetPen(wxPen(wxColour(200, 200, 200)));
    for (int i = 0; i <= GRID_SIZE; i++) {
        // Vertical lines
        dc.DrawLine(i * preview_cell_size, 0, i * preview_cell_size, BORDER_PREVIEW_SIZE);
        // Horizontal lines
        dc.DrawLine(0, i * preview_cell_size, BORDER_PREVIEW_SIZE, i * preview_cell_size);
    }
    
    // Draw a sample ground tile in the center
    dc.SetBrush(wxBrush(wxColour(120, 180, 100)));
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle((GRID_SIZE / 2) * preview_cell_size, (GRID_SIZE / 2) * preview_cell_size, preview_cell_size, preview_cell_size);
    
    // Draw border items around the center
    for (const BorderItem& item : m_borderItems) {
        wxPoint offset(0, 0);
        
        // Calculate position based on the edge type
        switch (item.position) {
            case EDGE_N:   offset = wxPoint(0, -1); break;
            case EDGE_E:   offset = wxPoint(1, 0); break;
            case EDGE_S:   offset = wxPoint(0, 1); break;
            case EDGE_W:   offset = wxPoint(-1, 0); break;
            case EDGE_CNW: offset = wxPoint(-1, -1); break;
            case EDGE_CNE: offset = wxPoint(1, -1); break;
            case EDGE_CSE: offset = wxPoint(1, 1); break;
            case EDGE_CSW: offset = wxPoint(-1, 1); break;
            case EDGE_DNW: offset = wxPoint(-1, -1); break; // Diagonal positions use same offsets as corners
            case EDGE_DNE: offset = wxPoint(1, -1); break;
            case EDGE_DSE: offset = wxPoint(1, 1); break;
            case EDGE_DSW: offset = wxPoint(-1, 1); break;
            default: continue;
        }
        
        // Calculate the position on the grid
        int x = (GRID_SIZE / 2 + offset.x) * preview_cell_size;
        int y = (GRID_SIZE / 2 + offset.y) * preview_cell_size;
        
        // Draw the item sprite
        const ItemType& type = g_items.getItemType(item.itemId);
        if (type.id != 0) {
            Sprite* sprite = g_gui.gfx.getSprite(type.clientID);
            if (sprite) {
                sprite->DrawTo(&dc, SPRITE_SIZE_32x32, x, y, preview_cell_size, preview_cell_size);
            }
        }
    }
}

void BorderEditorDialog::LoadExistingGroundBrushes() {
    // Clear the combo box
    m_existingGroundBrushesCombo->Clear();
    
    // Add first option depending on the dialog mode.
    if(m_modifyGroundBordersOnly) {
        m_existingGroundBrushesCombo->Append("<Select Existing Ground>");
    } else {
        m_existingGroundBrushesCombo->Append("<Create New>");
    }
    m_existingGroundBrushesCombo->SetSelection(0);
    
    wxString dataDir = GetVersionDataDirectory();
    wxString groundsFile = dataDir + "grounds.xml";
    
    if (!wxFileExists(groundsFile)) {
        wxMessageBox("Cannot find grounds.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }
    
    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(groundsFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load grounds.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    // Look for all brush nodes
    pugi::xml_node materials = doc.child("materials");
    if (!materials) {
        wxMessageBox("Invalid grounds.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    for (pugi::xml_node brushNode = materials.child("brush"); brushNode; brushNode = brushNode.next_sibling("brush")) {
        pugi::xml_attribute nameAttr = brushNode.attribute("name");
        pugi::xml_attribute serverLookIdAttr = brushNode.attribute("server_lookid");
        pugi::xml_attribute typeAttr = brushNode.attribute("type");
        
        // Only include ground brushes
        if (!typeAttr || std::string(typeAttr.as_string()) != "ground") {
            continue;
        }
        
        if (nameAttr && serverLookIdAttr) {
            wxString brushName = wxString(nameAttr.as_string());
            int serverId = serverLookIdAttr.as_int();
            
            // Add the brush name to the combo box with the serverId as client data
            wxStringClientData* data = new wxStringClientData(wxString::Format("%d", serverId));
            m_existingGroundBrushesCombo->Append(brushName, data);
        }
    }
}

void BorderEditorDialog::ClearGroundItems() {
    m_groundItems.clear();
    m_nameCtrl->SetValue("");
    m_idCtrl->SetValue(m_nextBorderId);
    if(m_borderIdsCtrl) {
        m_borderIdsCtrl->SetValue(wxString::Format("%d", m_nextBorderId));
    }
    if(m_borderToNoneIdsCtrl) {
        m_borderToNoneIdsCtrl->SetValue("");
    }
    if(m_borderListCtrl) {
        m_borderListCtrl->SetValue("");
    }
    if(m_groundBorderRefsList) {
        m_groundBorderRefsList->Clear();
    }
    m_serverLookIdCtrl->SetValue(0);
    m_zOrderCtrl->SetValue(0);
    m_groundItemIdCtrl->SetValue(0);
    m_groundItemChanceCtrl->SetValue(10);
    
    // Reset border alignment options
    m_borderAlignmentChoice->SetSelection(0); // Default to "outer"
    m_includeToNoneCheck->SetValue(true);     // Default to checked
    m_includeInnerCheck->SetValue(false);     // Default to unchecked
    
    UpdateGroundItemsList();
    RefreshBorderListIdsUI();
}

void BorderEditorDialog::UpdateGroundItemsList() {
    m_groundItemsList->Clear();
    
    for (const GroundItem& item : m_groundItems) {
        wxString itemText = wxString::Format("Item ID: %d, Chance: %d", item.itemId, item.chance);
        m_groundItemsList->Append(itemText);
    }
}

bool BorderEditorDialog::EnsureBorderPreviewCatalogLoaded() {
    if(m_borderPreviewCatalogLoaded) {
        return true;
    }

    m_borderPreviewItemById.clear();

    wxString dataDir = GetVersionDataDirectory();
    wxString bordersFile = dataDir + "borders.xml";
    if(!wxFileExists(bordersFile)) {
        return false;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result parseResult = doc.load_file(nstr(bordersFile).c_str());
    if(!parseResult) {
        return false;
    }

    pugi::xml_node materials = doc.child("materials");
    if(!materials) {
        return false;
    }

    for(pugi::xml_node borderNode = materials.child("border"); borderNode; borderNode = borderNode.next_sibling("border")) {
        pugi::xml_attribute idAttr = borderNode.attribute("id");
        if(!idAttr) {
            continue;
        }

        int borderId = idAttr.as_int();
        if(borderId <= 0) {
            continue;
        }

        uint16_t previewItemId = 0;
        for(pugi::xml_node borderItemNode = borderNode.child("borderitem"); borderItemNode; borderItemNode = borderItemNode.next_sibling("borderitem")) {
            pugi::xml_attribute itemAttr = borderItemNode.attribute("item");
            if(itemAttr) {
                previewItemId = static_cast<uint16_t>(itemAttr.as_uint());
                if(previewItemId > 0) {
                    break;
                }
            }
        }

        m_borderPreviewItemById[borderId] = previewItemId;
    }

    m_borderPreviewCatalogLoaded = true;
    return !m_borderPreviewItemById.empty();
}

static void PopulateBorderIdPreviewList(
    wxListCtrl* listCtrl,
    wxImageList* imageList,
    const wxString& rawValue,
    const wxString& emptyLabel,
    const std::map<int, uint16_t>& previewItemById
) {
    if(!listCtrl || !imageList) {
        return;
    }

    listCtrl->DeleteAllItems();
    imageList->RemoveAll();

    wxString value = rawValue;
    value.Trim(true).Trim(false);
    if(value.IsEmpty()) {
        long idx = listCtrl->InsertItem(0, emptyLabel);
        listCtrl->SetItemData(idx, 0);
        return;
    }

    std::vector<int> parsedIds;
    if(!ParseBorderIdList(value, parsedIds)) {
        long idx = listCtrl->InsertItem(0, "<invalid IDs>");
        listCtrl->SetItemData(idx, 0);
        return;
    }

    for(size_t i = 0; i < parsedIds.size(); ++i) {
        int borderId = parsedIds[i];
        uint16_t previewItemId = 0;
        std::map<int, uint16_t>::const_iterator it = previewItemById.find(borderId);
        if(it != previewItemById.end()) {
            previewItemId = it->second;
        }

        int imageIndex = imageList->Add(BuildSmallBorderPreviewBitmap(previewItemId));
        long idx = listCtrl->InsertItem(static_cast<long>(listCtrl->GetItemCount()), wxString::Format("ID %d", borderId), imageIndex);
        listCtrl->SetItemData(idx, static_cast<long>(borderId));
    }

    listCtrl->Arrange();
}

void BorderEditorDialog::RefreshInlineBorderIdPreviews() {
    EnsureBorderPreviewCatalogLoaded();

    PopulateBorderIdPreviewList(
        m_borderIdsPreviewCtrl,
        m_borderIdsPreviewImages,
        m_borderIdsCtrl ? m_borderIdsCtrl->GetValue() : wxString(),
        "<empty>",
        m_borderPreviewItemById
    );

    PopulateBorderIdPreviewList(
        m_borderToNoneIdsPreviewCtrl,
        m_borderToNoneIdsPreviewImages,
        m_borderToNoneIdsCtrl ? m_borderToNoneIdsCtrl->GetValue() : wxString(),
        "<empty: uses Border IDs>",
        m_borderPreviewItemById
    );

    PopulateBorderIdPreviewList(
        m_borderListIdsPreviewCtrl,
        m_borderListIdsPreviewImages,
        m_borderListCtrl ? m_borderListCtrl->GetValue() : wxString(),
        "<empty>",
        m_borderPreviewItemById
    );
}

void BorderEditorDialog::RefreshBorderListIdsUI() {
    if(!m_borderListIdsList) {
        RefreshInlineBorderIdPreviews();
        return;
    }

    int previouslySelectedId = 0;
    int previousSelection = m_borderListIdsList->GetSelection();
    if(previousSelection != wxNOT_FOUND) {
        wxString currentLabel = m_borderListIdsList->GetString(previousSelection);
        wxString numericPart = currentLabel.BeforeFirst(' ');
        long parsedId = 0;
        if(numericPart.ToLong(&parsedId) && parsedId > 0) {
            previouslySelectedId = static_cast<int>(parsedId);
        }
    }

    m_borderListIdsList->Clear();

    std::vector<int> borderListIds;
    bool borderListValid = false;
    if(m_borderListCtrl && !m_borderListCtrl->GetValue().IsEmpty()) {
        borderListValid = ParseBorderIdList(m_borderListCtrl->GetValue(), borderListIds);
    }

    int currentOuterId = 0;
    if(m_borderIdsCtrl && !m_borderIdsCtrl->GetValue().IsEmpty()) {
        std::vector<int> currentBorderIds;
        if(ParseBorderIdList(m_borderIdsCtrl->GetValue(), currentBorderIds) && !currentBorderIds.empty()) {
            currentOuterId = currentBorderIds.front();
        }
    } else if(m_idCtrl && m_idCtrl->GetValue() > 0) {
        currentOuterId = m_idCtrl->GetValue();
    }

    if(!borderListValid) {
        if(m_borderListCtrl && !m_borderListCtrl->GetValue().IsEmpty()) {
            m_borderListIdsList->Append("<invalid borderlist>");
        }
        if(m_promoteBorderListIdButton) {
            m_promoteBorderListIdButton->Enable(false);
        }
        RefreshInlineBorderIdPreviews();
        return;
    }

    for(size_t i = 0; i < borderListIds.size(); ++i) {
        int borderId = borderListIds[i];
        wxString label = wxString::Format("%d", borderId);
        if(borderId == currentOuterId) {
            label += " (outer atual)";
        }
        m_borderListIdsList->Append(label);
    }

    int selectionToSet = wxNOT_FOUND;
    if(previouslySelectedId > 0) {
        for(unsigned int i = 0; i < m_borderListIdsList->GetCount(); ++i) {
            wxString label = m_borderListIdsList->GetString(i);
            wxString numericPart = label.BeforeFirst(' ');
            long parsedId = 0;
            if(numericPart.ToLong(&parsedId) && parsedId == previouslySelectedId) {
                selectionToSet = static_cast<int>(i);
                break;
            }
        }
    }
    if(selectionToSet == wxNOT_FOUND && m_borderListIdsList->GetCount() > 0) {
        selectionToSet = 0;
    }
    if(selectionToSet != wxNOT_FOUND) {
        m_borderListIdsList->SetSelection(selectionToSet);
    }

    if(m_promoteBorderListIdButton) {
        m_promoteBorderListIdButton->Enable(m_borderListIdsList->GetCount() > 0 && selectionToSet != wxNOT_FOUND);
    }

    RefreshInlineBorderIdPreviews();
}

void BorderEditorDialog::OnBorderListTextChanged(wxCommandEvent& event) {
    RefreshBorderListIdsUI();
    event.Skip();
}

void BorderEditorDialog::OnBorderIdsPreviewSelected(wxListEvent& event) {
    if(!m_borderIdsPreviewCtrl || !m_borderIdsCtrl) {
        return;
    }

    long selectedData = m_borderIdsPreviewCtrl->GetItemData(event.GetIndex());
    int selectedId = static_cast<int>(selectedData);
    if(selectedId <= 0) {
        return;
    }

    std::vector<int> currentIds;
    wxString currentValue = m_borderIdsCtrl->GetValue();
    currentValue.Trim(true).Trim(false);
    if(!currentValue.IsEmpty()) {
        ParseBorderIdList(currentValue, currentIds);
    } else if(m_idCtrl && m_idCtrl->GetValue() > 0) {
        currentIds.push_back(m_idCtrl->GetValue());
    }

    std::vector<int> reordered = ReorderBorderIdsWithPrimary(currentIds, selectedId);
    if(reordered.empty()) {
        reordered.push_back(selectedId);
    }
    m_borderIdsCtrl->ChangeValue(JoinBorderIdList(reordered));
    if(m_idCtrl) {
        m_idCtrl->SetValue(selectedId);
    }
    RefreshBorderListIdsUI();
}

void BorderEditorDialog::OnToNoneIdsPreviewSelected(wxListEvent& event) {
    if(!m_borderToNoneIdsPreviewCtrl || !m_borderToNoneIdsCtrl) {
        return;
    }

    long selectedData = m_borderToNoneIdsPreviewCtrl->GetItemData(event.GetIndex());
    int selectedId = static_cast<int>(selectedData);
    if(selectedId <= 0) {
        return;
    }

    std::vector<int> currentIds;
    wxString currentValue = m_borderToNoneIdsCtrl->GetValue();
    currentValue.Trim(true).Trim(false);
    if(!currentValue.IsEmpty()) {
        ParseBorderIdList(currentValue, currentIds);
    }

    std::vector<int> reordered = ReorderBorderIdsWithPrimary(currentIds, selectedId);
    if(reordered.empty()) {
        reordered.push_back(selectedId);
    }
    m_borderToNoneIdsCtrl->ChangeValue(JoinBorderIdList(reordered));
    RefreshBorderListIdsUI();
}

void BorderEditorDialog::OnBorderListIdsPreviewSelected(wxListEvent& event) {
    if(!m_borderListIdsPreviewCtrl || !m_borderListCtrl || !m_borderIdsCtrl) {
        return;
    }

    long selectedData = m_borderListIdsPreviewCtrl->GetItemData(event.GetIndex());
    int selectedId = static_cast<int>(selectedData);
    if(selectedId <= 0) {
        return;
    }

    std::vector<int> borderListIds;
    if(!ParseBorderIdList(m_borderListCtrl->GetValue(), borderListIds)) {
        return;
    }

    std::vector<int> reorderedBorderList = ReorderBorderIdsWithPrimary(borderListIds, selectedId);
    m_borderListCtrl->ChangeValue(JoinBorderIdList(reorderedBorderList));

    std::vector<int> currentOuterIds;
    if(!m_borderIdsCtrl->GetValue().IsEmpty()) {
        ParseBorderIdList(m_borderIdsCtrl->GetValue(), currentOuterIds);
    }
    std::vector<int> reorderedOuterIds = ReorderBorderIdsWithPrimary(currentOuterIds, selectedId);
    if(reorderedOuterIds.size() <= 1) {
        for(size_t i = 0; i < borderListIds.size(); ++i) {
            if(borderListIds[i] != selectedId) {
                AppendUniqueBorderId(reorderedOuterIds, borderListIds[i]);
            }
        }
    }
    if(!reorderedOuterIds.empty()) {
        m_borderIdsCtrl->ChangeValue(JoinBorderIdList(reorderedOuterIds));
    }

    if(m_promoteAlsoToNoneCheck && m_promoteAlsoToNoneCheck->IsChecked() && m_borderToNoneIdsCtrl) {
        std::vector<int> currentToNoneIds;
        if(!m_borderToNoneIdsCtrl->GetValue().IsEmpty()) {
            ParseBorderIdList(m_borderToNoneIdsCtrl->GetValue(), currentToNoneIds);
        }
        std::vector<int> reorderedToNoneIds = ReorderBorderIdsWithPrimary(currentToNoneIds, selectedId);
        if(reorderedToNoneIds.size() <= 1) {
            for(size_t i = 0; i < borderListIds.size(); ++i) {
                if(borderListIds[i] != selectedId) {
                    AppendUniqueBorderId(reorderedToNoneIds, borderListIds[i]);
                }
            }
        }
        if(!reorderedToNoneIds.empty()) {
            m_borderToNoneIdsCtrl->ChangeValue(JoinBorderIdList(reorderedToNoneIds));
        }
    }

    if(m_idCtrl) {
        m_idCtrl->SetValue(selectedId);
    }
    RefreshBorderListIdsUI();
}

void BorderEditorDialog::OnPromoteBorderListId(wxCommandEvent& WXUNUSED(event)) {
    if(!m_borderListIdsList || !m_borderIdsCtrl || !m_borderListCtrl) {
        return;
    }

    int selection = m_borderListIdsList->GetSelection();
    if(selection == wxNOT_FOUND) {
        wxMessageBox("Select a borderlist ID first.", "Border List", wxICON_INFORMATION);
        return;
    }

    wxString selectedLabel = m_borderListIdsList->GetString(selection);
    wxString selectedNumeric = selectedLabel.BeforeFirst(' ');
    long selectedIdLong = 0;
    if(!selectedNumeric.ToLong(&selectedIdLong) || selectedIdLong <= 0) {
        wxMessageBox("Invalid selected borderlist ID.", "Border List", wxICON_ERROR);
        return;
    }
    int selectedId = static_cast<int>(selectedIdLong);

    std::vector<int> borderListIds;
    if(!ParseBorderIdList(m_borderListCtrl->GetValue(), borderListIds)) {
        wxMessageBox("Border List IDs must be valid before promoting.", "Border List", wxICON_ERROR);
        return;
    }

    std::vector<int> currentOuterIds;
    if(!m_borderIdsCtrl->GetValue().IsEmpty()) {
        if(!ParseBorderIdList(m_borderIdsCtrl->GetValue(), currentOuterIds)) {
            wxMessageBox("Current Border IDs are invalid. Fix them before promoting.", "Border IDs", wxICON_ERROR);
            return;
        }
    }

    std::vector<int> reorderedOuterIds;
    AppendUniqueBorderId(reorderedOuterIds, selectedId);
    if(!currentOuterIds.empty()) {
        for(size_t i = 0; i < currentOuterIds.size(); ++i) {
            if(currentOuterIds[i] != selectedId) {
                AppendUniqueBorderId(reorderedOuterIds, currentOuterIds[i]);
            }
        }
    } else {
        for(size_t i = 0; i < borderListIds.size(); ++i) {
            if(borderListIds[i] != selectedId) {
                AppendUniqueBorderId(reorderedOuterIds, borderListIds[i]);
            }
        }
    }

    bool applyToNone = m_promoteAlsoToNoneCheck && m_promoteAlsoToNoneCheck->IsChecked() && m_borderToNoneIdsCtrl;
    std::vector<int> reorderedToNoneIds;
    if(applyToNone) {
        std::vector<int> currentToNoneIds;
        if(!m_borderToNoneIdsCtrl->GetValue().IsEmpty()) {
            if(!ParseBorderIdList(m_borderToNoneIdsCtrl->GetValue(), currentToNoneIds)) {
                wxMessageBox("To None IDs are invalid. Fix them before applying promotion to To None.", "To None IDs", wxICON_ERROR);
                return;
            }
        }

        AppendUniqueBorderId(reorderedToNoneIds, selectedId);
        if(!currentToNoneIds.empty()) {
            for(size_t i = 0; i < currentToNoneIds.size(); ++i) {
                if(currentToNoneIds[i] != selectedId) {
                    AppendUniqueBorderId(reorderedToNoneIds, currentToNoneIds[i]);
                }
            }
        } else {
            for(size_t i = 0; i < borderListIds.size(); ++i) {
                if(borderListIds[i] != selectedId) {
                    AppendUniqueBorderId(reorderedToNoneIds, borderListIds[i]);
                }
            }
        }
    }

    m_borderIdsCtrl->SetValue(JoinBorderIdList(reorderedOuterIds));
    if(applyToNone) {
        m_borderToNoneIdsCtrl->SetValue(JoinBorderIdList(reorderedToNoneIds));
    }

    RefreshBorderListIdsUI();
}

void BorderEditorDialog::OnSearchBorderListId(wxCommandEvent& WXUNUSED(event)) {
    if(!m_borderListCtrl) {
        return;
    }

    wxString dataDir = GetVersionDataDirectory();
    wxString bordersFile = dataDir + "borders.xml";
    if(!wxFileExists(bordersFile)) {
        wxMessageBox("Cannot find borders.xml file in the data directory.", "Search Border IDs", wxICON_ERROR);
        return;
    }

    pugi::xml_document doc;
    pugi::xml_parse_result parseResult = doc.load_file(nstr(bordersFile).c_str());
    if(!parseResult) {
        wxMessageBox("Failed to load borders.xml: " + wxString(parseResult.description()), "Search Border IDs", wxICON_ERROR);
        return;
    }

    pugi::xml_node materials = doc.child("materials");
    if(!materials) {
        wxMessageBox("Invalid borders.xml file: missing 'materials' node", "Search Border IDs", wxICON_ERROR);
        return;
    }

    std::vector<BorderListSlotEditorDialog::BorderEntry> entries;
    for(pugi::xml_node borderNode = materials.child("border"); borderNode; borderNode = borderNode.next_sibling("border")) {
        pugi::xml_attribute idAttr = borderNode.attribute("id");
        if(!idAttr) {
            continue;
        }

        int borderId = idAttr.as_int();
        if(borderId <= 0) {
            continue;
        }

        uint16_t previewItemId = 0;
        for(pugi::xml_node borderItemNode = borderNode.child("borderitem"); borderItemNode; borderItemNode = borderItemNode.next_sibling("borderitem")) {
            pugi::xml_attribute itemAttr = borderItemNode.attribute("item");
            if(itemAttr) {
                previewItemId = static_cast<uint16_t>(itemAttr.as_uint());
                if(previewItemId > 0) {
                    break;
                }
            }
        }

        wxString description = ExtractBorderCommentDescription(borderNode);
        BorderListSlotEditorDialog::BorderEntry entry;
        entry.id = borderId;
        entry.description = NormalizeSingleLine(description);
        entry.previewItemId = previewItemId;
        if(entry.description.IsEmpty()) {
            entry.label = wxString::Format("ID %d", borderId);
        } else {
            entry.label = wxString::Format("ID %d | %s", borderId, entry.description);
        }
        entries.push_back(entry);
    }

    if(entries.empty()) {
        wxMessageBox("No border entries found in borders.xml.", "Search Border IDs", wxICON_INFORMATION);
        return;
    }

    std::vector<int> currentIds;
    wxString currentValue = m_borderListCtrl->GetValue();
    currentValue.Trim(true).Trim(false);
    if(!currentValue.IsEmpty()) {
        if(!ParseBorderIdList(currentValue, currentIds)) {
            wxMessageBox("Border List IDs is invalid. Please fix it before opening slot editor.", "Search Border IDs", wxICON_ERROR);
            return;
        }
    }

    BorderListSlotEditorDialog dialog(this, currentIds, entries);
    if(dialog.ShowModal() != wxID_OK) {
        return;
    }

    std::vector<int> finalIds = dialog.GetSlotIds();
    m_borderListCtrl->SetValue(JoinBorderIdList(finalIds));
    RefreshBorderListIdsUI();
}

void BorderEditorDialog::OnPageChanged(wxBookCtrlEvent& event) {
    if(m_modifyGroundBordersOnly && event.GetSelection() != 1) {
        m_notebook->ChangeSelection(1);
        m_activeTab = 1;
        return;
    }

    m_activeTab = event.GetSelection();
    
    // When switching to the ground tab, use the same border items for the ground brush
    if (m_activeTab == 1) {
        // Update the ground items preview (not implemented yet)
    }
}

void BorderEditorDialog::OnAddGroundItem(wxCommandEvent& event) {
    uint16_t itemId = m_groundItemIdCtrl->GetValue();
    int chance = m_groundItemChanceCtrl->GetValue();
    
    if (itemId > 0) {
        // Check if this item already exists, if so update its chance
        bool updated = false;
        for (size_t i = 0; i < m_groundItems.size(); i++) {
            if (m_groundItems[i].itemId == itemId) {
                m_groundItems[i].chance = chance;
                updated = true;
                break;
            }
        }
        
        if (!updated) {
            // Add new item
            m_groundItems.push_back(GroundItem(itemId, chance));
        }
        
        // Update the list
        UpdateGroundItemsList();
    }
}

void BorderEditorDialog::OnRemoveGroundItem(wxCommandEvent& event) {
    int selection = m_groundItemsList->GetSelection();
    if (selection != wxNOT_FOUND && selection < static_cast<int>(m_groundItems.size())) {
        m_groundItems.erase(m_groundItems.begin() + selection);
        UpdateGroundItemsList();
    }
}

void BorderEditorDialog::OnGroundBrowse(wxCommandEvent& event) {
    // Open the Find Item dialog to select a ground item
    FindItemDialog dialog(this, "Select Ground Item");
    
    if (dialog.ShowModal() == wxID_OK) {
        // Get the selected item ID
        uint16_t itemId = dialog.getResultID();
        
        // Set the ground item ID field
        if (itemId > 0) {
            m_groundItemIdCtrl->SetValue(itemId);
        }
    }
}

void BorderEditorDialog::OnLoadGroundBrush(wxCommandEvent& event) {
    int selection = m_existingGroundBrushesCombo->GetSelection();
    if (selection <= 0) {
        // Selected "Create New" or nothing
        ClearGroundItems();
        return;
    }
    
    wxStringClientData* data = static_cast<wxStringClientData*>(m_existingGroundBrushesCombo->GetClientObject(selection));
    if (!data) return;
    
    int serverLookId = wxAtoi(data->GetData());
    
    wxString dataDir = GetVersionDataDirectory();
    wxString groundsFile = dataDir + "grounds.xml";
    
    if (!wxFileExists(groundsFile)) {
        wxMessageBox("Cannot find grounds.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }
    
    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(groundsFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load grounds.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    // Clear existing items
    ClearGroundItems();
    
    // Find the brush with the specified server_lookid
    pugi::xml_node materials = doc.child("materials");
    if (!materials) {
        wxMessageBox("Invalid grounds.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    for (pugi::xml_node brushNode = materials.child("brush"); brushNode; brushNode = brushNode.next_sibling("brush")) {
        pugi::xml_attribute serverLookIdAttr = brushNode.attribute("server_lookid");
        
        if (serverLookIdAttr && serverLookIdAttr.as_int() == serverLookId) {
            // Found the brush, load its properties
            pugi::xml_attribute nameAttr = brushNode.attribute("name");
            pugi::xml_attribute zOrderAttr = brushNode.attribute("z-order");
            
            if (nameAttr) {
                m_nameCtrl->SetValue(wxString(nameAttr.as_string()));
            }
            
            m_serverLookIdCtrl->SetValue(serverLookId);
            
            if (zOrderAttr) {
                m_zOrderCtrl->SetValue(zOrderAttr.as_int());
            }
            
            // Load all item nodes
            for (pugi::xml_node itemNode = brushNode.child("item"); itemNode; itemNode = itemNode.next_sibling("item")) {
                pugi::xml_attribute idAttr = itemNode.attribute("id");
                pugi::xml_attribute chanceAttr = itemNode.attribute("chance");
                
                if (idAttr) {
                    uint16_t itemId = idAttr.as_uint();
                    int chance = chanceAttr ? chanceAttr.as_int() : 10;
                    
                    m_groundItems.push_back(GroundItem(itemId, chance));
                }
            }
            
            // Load all border nodes and add to the border grid
            m_borderItems.clear();
            m_gridPanel->Clear();
            
            // Reset alignment options
            m_borderAlignmentChoice->SetSelection(0); // Default to "outer"
            m_includeToNoneCheck->SetValue(true);     // Default to checked
            m_includeInnerCheck->SetValue(false);     // Default to unchecked
            if(m_borderIdsCtrl) {
                m_borderIdsCtrl->SetValue("");
            }
            if(m_borderToNoneIdsCtrl) {
                m_borderToNoneIdsCtrl->SetValue("");
            }
            if(m_borderListCtrl) {
                m_borderListCtrl->SetValue("");
            }
            if(m_groundBorderRefsList) {
                m_groundBorderRefsList->Clear();
            }
            
            bool hasNormalBorder = false;
            bool hasToNoneBorder = false;
            bool hasInnerBorder = false;
            bool hasInnerToNoneBorder = false;
            wxString alignment = "outer"; // Default
            wxString loadedBorderIds;
            wxString loadedToNoneIds;
            wxString loadedBorderList;
            
            for (pugi::xml_node borderNode = brushNode.child("border"); borderNode; borderNode = borderNode.next_sibling("border")) {
                pugi::xml_attribute alignAttr = borderNode.attribute("align");
                pugi::xml_attribute toAttr = borderNode.attribute("to");
                pugi::xml_attribute idAttr = borderNode.attribute("id");
                pugi::xml_attribute borderListAttr = borderNode.attribute("borderlist");

                if(m_groundBorderRefsList) {
                    m_groundBorderRefsList->Append(BuildBorderReferenceLabel(borderNode));
                }

                if(borderListAttr && loadedBorderList.IsEmpty()) {
                    loadedBorderList = wxString(borderListAttr.as_string());
                }
                
                if (idAttr) {
                    wxString borderIdList = wxString(idAttr.as_string());
                    std::vector<int> parsedBorderIds;
                    int borderId = 0;
                    if(ParseBorderIdList(borderIdList, parsedBorderIds)) {
                        borderId = parsedBorderIds.front();
                        m_idCtrl->SetValue(borderId);
                    }
                    
                    // Check border type and attributes
                    if (alignAttr) {
                        wxString alignVal = wxString(alignAttr.as_string());
                        
                        if (alignVal == "outer") {
                            if (toAttr && wxString(toAttr.as_string()) == "none") {
                                hasToNoneBorder = true;
                                if(loadedToNoneIds.IsEmpty()) {
                                    loadedToNoneIds = borderIdList;
                                }
                            } else {
                                hasNormalBorder = true;
                                alignment = "outer";
                                if(loadedBorderIds.IsEmpty()) {
                                    loadedBorderIds = borderIdList;
                                }
                            }
                        } else if (alignVal == "inner") {
                            if (toAttr && wxString(toAttr.as_string()) == "none") {
                                hasInnerToNoneBorder = true;
                                if(loadedToNoneIds.IsEmpty()) {
                                    loadedToNoneIds = borderIdList;
                                }
                            } else {
                                hasInnerBorder = true;
                                if(loadedBorderIds.IsEmpty()) {
                                    loadedBorderIds = borderIdList;
                                }
                                if(!hasNormalBorder) {
                                    alignment = "inner";
                                }
                            }
                        }
                    }
                    
                    // Load the border details from borders.xml
                    wxString bordersFile = dataDir + "borders.xml";
                    
                    if (!wxFileExists(bordersFile)) {
                        // Just skip if we can't find borders.xml
                        continue;
                    }
                    
                    pugi::xml_document bordersDoc;
                    pugi::xml_parse_result bordersResult = bordersDoc.load_file(nstr(bordersFile).c_str());
                    
                    if (!bordersResult) {
                        // Skip if we can't load borders.xml
                        continue;
                    }
                    
                    pugi::xml_node bordersMaterials = bordersDoc.child("materials");
                    if (!bordersMaterials) {
                        continue;
                    }
                    
                    if(borderId <= 0) {
                        continue;
                    }

                    for (pugi::xml_node targetBorder = bordersMaterials.child("border"); targetBorder; targetBorder = targetBorder.next_sibling("border")) {
                        pugi::xml_attribute targetIdAttr = targetBorder.attribute("id");
                        
                        if (targetIdAttr && targetIdAttr.as_int() == borderId) {
                            // Found the border, load its items
                            for (pugi::xml_node borderItemNode = targetBorder.child("borderitem"); borderItemNode; borderItemNode = borderItemNode.next_sibling("borderitem")) {
                                pugi::xml_attribute edgeAttr = borderItemNode.attribute("edge");
                                pugi::xml_attribute itemAttr = borderItemNode.attribute("item");
                                
                                if (!edgeAttr || !itemAttr) continue;
                                
                                BorderEdgePosition pos = edgeStringToPosition(edgeAttr.as_string());
                                uint16_t borderItemId = itemAttr.as_uint();
                                
                                if (pos != EDGE_NONE && borderItemId > 0) {
                                    bool updated = false;
                                    for(BorderItem& loadedItem : m_borderItems) {
                                        if(loadedItem.position == pos) {
                                            loadedItem.itemId = borderItemId;
                                            updated = true;
                                            break;
                                        }
                                    }
                                    if(!updated) {
                                        m_borderItems.push_back(BorderItem(pos, borderItemId));
                                    }
                                    m_gridPanel->SetItemId(pos, borderItemId);
                                }
                            }
                            
                            break;
                        }
                    }
                }
            }

            if(m_groundBorderRefsList && m_groundBorderRefsList->GetCount() == 0) {
                m_groundBorderRefsList->Append("<No border entries>");
            }
            
            // Update the ground items list and border preview
            UpdateGroundItemsList();
            UpdatePreview();
            
            // Apply the loaded border alignment settings
            if (hasInnerBorder) {
                m_includeInnerCheck->SetValue(true);
            }
            
            if (!hasToNoneBorder && !hasInnerToNoneBorder) {
                m_includeToNoneCheck->SetValue(false);
            }
            
            int alignmentIndex = 0; // Default to outer
            if (alignment == "inner") {
                alignmentIndex = 1;
            }
            m_borderAlignmentChoice->SetSelection(alignmentIndex);
            if(loadedBorderIds.IsEmpty() && m_idCtrl->GetValue() > 0) {
                loadedBorderIds = wxString::Format("%d", m_idCtrl->GetValue());
            }
            if(m_borderIdsCtrl) {
                m_borderIdsCtrl->SetValue(loadedBorderIds);
            }
            if(m_borderToNoneIdsCtrl) {
                m_borderToNoneIdsCtrl->SetValue(loadedToNoneIds);
            }
            if(m_borderListCtrl) {
                m_borderListCtrl->SetValue(loadedBorderList);
            }
            RefreshBorderListIdsUI();
            
            break;
        }
    }
    
    // Keep selection
    m_existingGroundBrushesCombo->SetSelection(selection);
}

bool BorderEditorDialog::ValidateGroundBrush() {
    if(m_modifyGroundBordersOnly) {
        if(!m_existingGroundBrushesCombo || m_existingGroundBrushesCombo->GetSelection() <= 0) {
            wxMessageBox("Select an existing ground brush first.", "Validation Error", wxICON_ERROR);
            return false;
        }
    }

    // Check for empty name
    if (!m_modifyGroundBordersOnly && m_nameCtrl->GetValue().IsEmpty()) {
        wxMessageBox("Please enter a name for the ground brush.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    if (!m_modifyGroundBordersOnly && m_groundItems.empty()) {
        wxMessageBox("The ground brush must have at least one item.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    if (!m_modifyGroundBordersOnly && m_serverLookIdCtrl->GetValue() <= 0) {
        wxMessageBox("You must specify a valid server look ID.", "Validation Error", wxICON_ERROR);
        return false;
    }
    
    // Check tileset selection
    if (!m_modifyGroundBordersOnly && m_tilesetChoice->GetSelection() == wxNOT_FOUND) {
        wxMessageBox("Please select a tileset for the ground brush.", "Validation Error", wxICON_ERROR);
        return false;
    }

    if(m_borderIdsCtrl && !m_borderIdsCtrl->GetValue().IsEmpty()) {
        std::vector<int> parsed;
        if(!ParseBorderIdList(m_borderIdsCtrl->GetValue(), parsed)) {
            wxMessageBox("Border IDs must be comma-separated positive integers. Example: 258, 256", "Validation Error", wxICON_ERROR);
            return false;
        }
    }

    if(m_borderToNoneIdsCtrl && !m_borderToNoneIdsCtrl->GetValue().IsEmpty()) {
        std::vector<int> parsed;
        if(!ParseBorderIdList(m_borderToNoneIdsCtrl->GetValue(), parsed)) {
            wxMessageBox("To None IDs must be comma-separated positive integers. Example: 258, 256", "Validation Error", wxICON_ERROR);
            return false;
        }
    }

    if(m_borderListCtrl && !m_borderListCtrl->GetValue().IsEmpty()) {
        std::vector<int> parsed;
        if(!ParseBorderIdList(m_borderListCtrl->GetValue(), parsed)) {
            wxMessageBox("Border List IDs must be comma-separated positive integers. Example: 201, 202, 2023", "Validation Error", wxICON_ERROR);
            return false;
        }
    }
    
    return true;
}

void BorderEditorDialog::SaveGroundBrush() {
    if (!ValidateGroundBrush()) {
        return;
    }
    
    // Get the ground brush properties
    wxString name = m_nameCtrl->GetValue();
    if(m_modifyGroundBordersOnly && m_existingGroundBrushesCombo && m_existingGroundBrushesCombo->GetSelection() > 0) {
        name = m_existingGroundBrushesCombo->GetStringSelection();
    }
    
    // Double check that we have a name (it's also checked in ValidateGroundBrush)
    if (name.IsEmpty()) {
        wxMessageBox("You must provide a name for the ground brush.", "Error", wxICON_ERROR);
        return;
    }
    
    int serverId = m_serverLookIdCtrl->GetValue();
    int zOrder = m_zOrderCtrl->GetValue();
    wxString borderIdsText = m_borderIdsCtrl ? m_borderIdsCtrl->GetValue() : wxString();
    borderIdsText.Trim(true).Trim(false);

    std::vector<int> parsedBorderIds;
    if(!borderIdsText.IsEmpty()) {
        if(!ParseBorderIdList(borderIdsText, parsedBorderIds)) {
            wxMessageBox("Border IDs must be comma-separated positive integers. Example: 258, 256", "Validation Error", wxICON_ERROR);
            return;
        }
    } else if(m_idCtrl->GetValue() > 0) {
        borderIdsText = wxString::Format("%d", m_idCtrl->GetValue());
        parsedBorderIds.push_back(m_idCtrl->GetValue());
    }

    wxString toNoneIdsText = m_borderToNoneIdsCtrl ? m_borderToNoneIdsCtrl->GetValue() : wxString();
    toNoneIdsText.Trim(true).Trim(false);
    if(!toNoneIdsText.IsEmpty()) {
        std::vector<int> parsedToNoneIds;
        if(!ParseBorderIdList(toNoneIdsText, parsedToNoneIds)) {
            wxMessageBox("To None IDs must be comma-separated positive integers. Example: 204, 271", "Validation Error", wxICON_ERROR);
            return;
        }
    } else {
        toNoneIdsText = borderIdsText;
    }

    wxString borderListText = m_borderListCtrl ? m_borderListCtrl->GetValue() : wxString();
    borderListText.Trim(true).Trim(false);
    if(!borderListText.IsEmpty()) {
        std::vector<int> parsedBorderList;
        if(!ParseBorderIdList(borderListText, parsedBorderList)) {
            wxMessageBox("Border List IDs must be comma-separated positive integers. Example: 201, 202, 2023", "Validation Error", wxICON_ERROR);
            return;
        }
    }

    if(!parsedBorderIds.empty()) {
        // Keep common ID in sync with the primary border for SaveBorder()/UI compatibility.
        m_idCtrl->SetValue(parsedBorderIds.front());
    }
    
    wxString dataDir = GetVersionDataDirectory();
    wxString groundsFile = dataDir + "grounds.xml";
    
    if (!wxFileExists(groundsFile)) {
        wxMessageBox("Cannot find grounds.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }
    
    // Make sure the border is saved first if we have border items (full editor mode only).
    if (!m_modifyGroundBordersOnly && !m_borderItems.empty()) {
        SaveBorder();
    }
    
    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(groundsFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load grounds.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    pugi::xml_node materials = doc.child("materials");
    if (!materials) {
        wxMessageBox("Invalid grounds.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    // Check if a brush with this name already exists
    bool brushExists = false;
    pugi::xml_node existingBrush;
    
    for (pugi::xml_node brushNode = materials.child("brush"); brushNode; brushNode = brushNode.next_sibling("brush")) {
        pugi::xml_attribute nameAttr = brushNode.attribute("name");
        
        if (nameAttr && wxString(nameAttr.as_string()) == name) {
            brushExists = true;
            existingBrush = brushNode;
            break;
        }
    }
    
    if (brushExists) {
        // Ask for confirmation to overwrite (only in full editor mode).
        if (!m_modifyGroundBordersOnly &&
            wxMessageBox("A ground brush with name '" + name + "' already exists. Do you want to overwrite it?",
                        "Confirm Overwrite", wxYES_NO | wxICON_QUESTION) != wxYES) {
            return;
        }
        
        // Remove the existing brush
        materials.remove_child(existingBrush);
    } else if(m_modifyGroundBordersOnly) {
        wxMessageBox("This mode only edits existing ground brushes.", "Error", wxICON_ERROR);
        return;
    }
    
    // Create the new brush node
    pugi::xml_node brushNode = materials.append_child("brush");
    brushNode.append_attribute("name").set_value(nstr(name).c_str());
    brushNode.append_attribute("type").set_value("ground");
    brushNode.append_attribute("server_lookid").set_value(serverId);
    brushNode.append_attribute("z-order").set_value(zOrder);
    
    // Add all ground items
    for (const GroundItem& item : m_groundItems) {
        pugi::xml_node itemNode = brushNode.append_child("item");
        itemNode.append_attribute("id").set_value(item.itemId);
        itemNode.append_attribute("chance").set_value(item.chance);
    }
    
    // Add border reference if we have border items, or if border IDs are specified (even without items)
    if (!m_borderItems.empty() || !borderIdsText.IsEmpty()) {
        // Get alignment type
        wxString alignmentType = m_borderAlignmentChoice->GetStringSelection();
        
        // Main border
        pugi::xml_node borderNode = brushNode.append_child("border");
        borderNode.append_attribute("align").set_value(nstr(alignmentType).c_str());
        borderNode.append_attribute("id").set_value(nstr(borderIdsText).c_str());
        if(!borderListText.IsEmpty()) {
            borderNode.append_attribute("borderlist").set_value(nstr(borderListText).c_str());
        }
        
        // "to none" border if checked
        if (m_includeToNoneCheck->IsChecked()) {
            pugi::xml_node borderToNoneNode = brushNode.append_child("border");
            borderToNoneNode.append_attribute("align").set_value(nstr(alignmentType).c_str());
            borderToNoneNode.append_attribute("to").set_value("none");
            borderToNoneNode.append_attribute("id").set_value(nstr(toNoneIdsText).c_str());
            if(!borderListText.IsEmpty()) {
                borderToNoneNode.append_attribute("borderlist").set_value(nstr(borderListText).c_str());
            }
        }
        
        // Inner border if checked
        if (m_includeInnerCheck->IsChecked()) {
            pugi::xml_node innerBorderNode = brushNode.append_child("border");
            innerBorderNode.append_attribute("align").set_value("inner");
            innerBorderNode.append_attribute("id").set_value(nstr(borderIdsText).c_str());
            if(!borderListText.IsEmpty()) {
                innerBorderNode.append_attribute("borderlist").set_value(nstr(borderListText).c_str());
            }
            
            // Inner "to none" border if checked
            if (m_includeToNoneCheck->IsChecked()) {
                pugi::xml_node innerToNoneNode = brushNode.append_child("border");
                innerToNoneNode.append_attribute("align").set_value("inner");
                innerToNoneNode.append_attribute("to").set_value("none");
                innerToNoneNode.append_attribute("id").set_value(nstr(toNoneIdsText).c_str());
                if(!borderListText.IsEmpty()) {
                    innerToNoneNode.append_attribute("borderlist").set_value(nstr(borderListText).c_str());
                }
            }
        }
    }
    
    // Save the file
    if (!doc.save_file(nstr(groundsFile).c_str())) {
        wxMessageBox("Failed to save changes to grounds.xml", "Error", wxICON_ERROR);
        return;
    }
    
    // In modify-only mode we stop after updating grounds.xml.
    if (m_modifyGroundBordersOnly) {
        wxMessageBox("Ground borders updated successfully.", "Success", wxICON_INFORMATION);
        LoadExistingGroundBrushes();
        SelectGroundBrushByName(name);
        return;
    }

    // Now also add this brush to the selected tileset
    int tilesetSelection = m_tilesetChoice->GetSelection();
    if (tilesetSelection == wxNOT_FOUND) {
        wxMessageBox("Please select a tileset.", "Validation Error", wxICON_ERROR);
        return;
    }
    wxString tilesetName = m_tilesetChoice->GetString(tilesetSelection);

    wxString tilesetsFile = dataDir + "tilesets.xml";
    
    if (!wxFileExists(tilesetsFile)) {
        wxMessageBox("Cannot find tilesets.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }
    
    // Load the tilesets XML file
    pugi::xml_document tilesetsDoc;
    pugi::xml_parse_result tilesetsResult = tilesetsDoc.load_file(nstr(tilesetsFile).c_str());
    
    if (!tilesetsResult) {
        wxMessageBox("Failed to load tilesets.xml: " + wxString(tilesetsResult.description()), "Error", wxICON_ERROR);
        return;
    }
    
    pugi::xml_node tilesetsMaterials = tilesetsDoc.child("materials");
    if (!tilesetsMaterials) {
        wxMessageBox("Invalid tilesets.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    // Find the selected tileset
    bool tilesetFound = false;
    for (pugi::xml_node tilesetNode = tilesetsMaterials.child("tileset"); tilesetNode; tilesetNode = tilesetNode.next_sibling("tileset")) {
        pugi::xml_attribute nameAttr = tilesetNode.attribute("name");
        
        if (nameAttr && wxString(nameAttr.as_string()) == tilesetName) {
            // Find the terrain node
            pugi::xml_node terrainNode = tilesetNode.child("terrain");
            if (!terrainNode) {
                // Create terrain node if it doesn't exist
                terrainNode = tilesetNode.append_child("terrain");
            }
            
            // Check if the brush is already in this tileset
            bool brushFound = false;
            for (pugi::xml_node brushNode = terrainNode.child("brush"); brushNode; brushNode = brushNode.next_sibling("brush")) {
                pugi::xml_attribute brushNameAttr = brushNode.attribute("name");
                
                if (brushNameAttr && wxString(brushNameAttr.as_string()) == name) {
                    brushFound = true;
                    break;
                }
            }
            
            // If brush not found, add it
            if (!brushFound) {
                // Add a brush node directly under terrain - no empty attributes
                pugi::xml_node newBrushNode = terrainNode.append_child("brush");
                newBrushNode.append_attribute("name").set_value(nstr(name).c_str());
            }
            
            tilesetFound = true;
            break;
        }
    }
    
    if (!tilesetFound) {
        wxMessageBox("Selected tileset not found in tilesets.xml", "Error", wxICON_ERROR);
        return;
    }
    
    // Save the tilesets.xml file
    if (!tilesetsDoc.save_file(nstr(tilesetsFile).c_str())) {
        wxMessageBox("Failed to save changes to tilesets.xml", "Error", wxICON_ERROR);
        return;
    }
    
    wxMessageBox("Ground brush saved successfully and added to the " + tilesetName + " tileset.", "Success", wxICON_INFORMATION);
    
    // Reload the existing ground brushes list
    LoadExistingGroundBrushes();
    SelectGroundBrushByName(name);
}

void BorderEditorDialog::LoadTilesets() {
    // Clear the choice control
    m_tilesetChoice->Clear();
    m_tilesets.clear();
    
    wxString dataDir = GetVersionDataDirectory();
    wxString tilesetsFile = dataDir + "tilesets.xml";
    
    if (!wxFileExists(tilesetsFile)) {
        wxMessageBox("Cannot find tilesets.xml file in the data directory.", "Error", wxICON_ERROR);
        return;
    }
    
    // Load the XML file
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(nstr(tilesetsFile).c_str());
    
    if (!result) {
        wxMessageBox("Failed to load tilesets.xml: " + wxString(result.description()), "Error", wxICON_ERROR);
        return;
    }
    
    pugi::xml_node materials = doc.child("materials");
    if (!materials) {
        wxMessageBox("Invalid tilesets.xml file: missing 'materials' node", "Error", wxICON_ERROR);
        return;
    }
    
    // Parse all tilesets
    wxArrayString tilesetNames; // Store in sorted order
    for (pugi::xml_node tilesetNode = materials.child("tileset"); tilesetNode; tilesetNode = tilesetNode.next_sibling("tileset")) {
        pugi::xml_attribute nameAttr = tilesetNode.attribute("name");
        
        if (nameAttr) {
            wxString tilesetName = wxString(nameAttr.as_string());
            
            // Add to our array of names
            tilesetNames.Add(tilesetName);
            
            // Add to the map for later use
            m_tilesets[tilesetName] = tilesetName;
        }
    }
    
    // Sort tileset names alphabetically
    tilesetNames.Sort();
    
    // Add sorted names to the choice control
    for (size_t i = 0; i < tilesetNames.GetCount(); ++i) {
        m_tilesetChoice->Append(tilesetNames[i]);
    }
    
    // Select the first tileset by default if any exist
    if (m_tilesetChoice->GetCount() > 0) {
        m_tilesetChoice->SetSelection(0);
    }
} 
