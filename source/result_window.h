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

#ifndef RME_RESULT_WINDOW_H_
#define RME_RESULT_WINDOW_H_

#include "main.h"
#include "position.h"

#include <vector>

class SearchResultWindow : public wxPanel
{
public:
	SearchResultWindow(wxWindow* parent);
	virtual ~SearchResultWindow();

	void Clear(bool clearFilter = false);
	void AddPosition(wxString description, Position pos);

	void OnClickResult(wxCommandEvent&);
	void OnClickExport(wxCommandEvent&);
	void OnClickClear(wxCommandEvent&);
	void OnSearchChanged(wxCommandEvent&);

protected:
	struct SearchResultEntry {
		wxString description;
		Position pos;
	};

	void ClearListItems();
	void RefreshResults();
	bool MatchesFilter(const wxString& description) const;

	wxTextCtrl* search_ctrl;
	wxListBox* result_list;
	wxString search_query;
	std::vector<SearchResultEntry> results;

	DECLARE_EVENT_TABLE()
};

#endif
