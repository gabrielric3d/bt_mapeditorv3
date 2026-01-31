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

#include "brush_tips_window.h"

#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "theme.h"

namespace
{
	wxString BuildTipsText()
	{
		wxString text;
		text
			<< "Atalhos e comportamentos das brushes\n"
			<< "\n"
			<< "Tamanho\n"
			<< "- Alt + scroll do mouse: aumenta/diminui o tamanho da brush.\n"
			<< "- [ ou + : aumenta o tamanho.\n"
			<< "- ] ou - : diminui o tamanho.\n"
			<< "\n"
			<< "Pintura e preenchimento\n"
			<< "- Ctrl + clique/arrastar: apaga usando a brush atual.\n"
			<< "- Ctrl + D (ground): preenchimento estilo bucket (flood fill).\n"
			<< "- Shift + arrastar: desenha uma area/linha com a brush (quando suportado).\n"
			<< "\n"
			<< "Variacoes e selecao\n"
			<< "- Z / X: muda a variacao (quando a brush tem variacoes).\n"
			<< "- Q: volta para a brush anterior.\n"
			<< "\n"
			<< "Ground (modo alternativo)\n"
			<< "- Alt ao pintar ground: substitui apenas tiles do mesmo ground clicado (ou somente tiles vazios).\n";
		return text;
	}
}

BrushTipsDialog::BrushTipsDialog(wxWindow* parent) :
	wxDialog(parent, wxID_ANY, "Brush Tips", wxDefaultPosition, wxSize(640, 520), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
	const ThemeColors& theme = Theme::Dark();
	SetBackgroundColour(theme.background);
	SetForegroundColour(theme.text);

	wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

	wxStaticText* title = new wxStaticText(this, wxID_ANY, "Brush Tips & Shortcuts");
	wxFont titleFont = title->GetFont();
	titleFont.SetWeight(wxFONTWEIGHT_BOLD);
	title->SetFont(titleFont);
	mainSizer->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, 12);

	wxTextCtrl* tips = new wxTextCtrl(this, wxID_ANY, BuildTipsText(), wxDefaultPosition, wxDefaultSize,
		wxTE_MULTILINE | wxTE_READONLY);
	tips->SetBackgroundColour(theme.surface);
	tips->SetForegroundColour(theme.text);
	mainSizer->Add(tips, 1, wxEXPAND | wxALL, 12);

	wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
	buttonSizer->AddStretchSpacer();
	buttonSizer->Add(new wxButton(this, wxID_OK, "OK"), 0, wxRIGHT, 12);
	mainSizer->Add(buttonSizer, 0, wxEXPAND | wxBOTTOM, 12);

	SetSizerAndFit(mainSizer);
	SetMinSize(wxSize(520, 420));
	CentreOnParent();
}
