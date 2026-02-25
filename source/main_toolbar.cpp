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
#include "main_toolbar.h"
#include "gui.h"
#include "editor.h"
#include "settings.h"
#include "brush.h"
#include "pngfiles.h"
#include "artprovider.h"
#include "theme.h"

#include <wx/artprov.h>
#include <wx/dir.h>
#include <wx/datetime.h>
#include <wx/filename.h>
#include <wx/mstream.h>
#include <wx/textdlg.h>
#include <wx/utils.h>

const wxString MainToolBar::STANDARD_BAR_NAME = "standard_toolbar";
const wxString MainToolBar::BRUSHES_BAR_NAME = "brushes_toolbar";
const wxString MainToolBar::POSITION_BAR_NAME = "position_toolbar";
const wxString MainToolBar::SIZES_BAR_NAME = "sizes_toolbar";
const wxString MainToolBar::INDICATORS_BAR_NAME = "indicators_toolbar";

#define loadPNGFile(name) _wxGetBitmapFromMemory(name, sizeof(name))
inline wxBitmap* _wxGetBitmapFromMemory(const unsigned char* data, int length)
{
	wxMemoryInputStream is(data, length);
	wxImage img(is, "image/png");
	if(!img.IsOk()) return nullptr;
	return newd wxBitmap(img, -1);
}

namespace {

void ApplyToolbarTheme(wxAuiToolBar* toolbar)
{
	if(!toolbar) {
		return;
	}

	const ThemeColors& theme = Theme::Dark();
	toolbar->SetBackgroundColour(theme.controlBase);
	toolbar->SetForegroundColour(theme.text);
	toolbar->SetGripperVisible(false);
}

void StylePositionControl(wxWindow* window)
{
	if(!window) {
		return;
	}
	const ThemeColors& theme = Theme::Dark();
	window->SetBackgroundColour(theme.surfaceAlt);
	window->SetForegroundColour(theme.text);
}

wxString QuoteShellArgument(const wxString& value)
{
	wxString escaped = value;
	escaped.Replace("\"", "\"\"");
	return "\"" + escaped + "\"";
}

bool ExecuteCommandSync(const wxString& command, wxArrayString& output, wxArrayString& errors, long& exitCode, const wxString& workingDirectory = wxEmptyString)
{
	output.clear();
	errors.clear();
	wxString normalized_working_directory = workingDirectory;
	normalized_working_directory.Trim(true);
	normalized_working_directory.Trim(false);

	wxExecuteEnv env;
	wxExecuteEnv* env_ptr = nullptr;
	if(!normalized_working_directory.empty()) {
		env.cwd = normalized_working_directory;
		env_ptr = &env;
	}
	exitCode = wxExecute(command, output, errors, wxEXEC_SYNC | wxEXEC_NODISABLE, env_ptr);
#ifdef __WINDOWS__
	if(exitCode != 0 && !normalized_working_directory.empty()) {
		// Some Windows setups ignore wxExecuteEnv::cwd with captured output; retry via explicit cmd cd.
		output.clear();
		errors.clear();
		const wxString fallback = "cmd.exe /C cd /D " + QuoteShellArgument(normalized_working_directory) + " && " + command;
		exitCode = wxExecute(fallback, output, errors, wxEXEC_SYNC | wxEXEC_NODISABLE);
	}
#endif
	return exitCode == 0;
}

wxString BuildCommandOutput(const wxArrayString& output, const wxArrayString& errors)
{
	wxString message;
	int lines_added = 0;
	const int max_lines = 12;

	auto append_lines = [&message, &lines_added, max_lines](const wxArrayString& lines) {
		for(size_t i = 0; i < lines.size() && lines_added < max_lines; ++i) {
			wxString line = lines[i];
			line.Trim(true);
			line.Trim(false);
			if(line.empty()) {
				continue;
			}
			if(!message.empty()) {
				message << "\n";
			}
			message << line;
			++lines_added;
		}
	};

	append_lines(errors);
	append_lines(output);

	if(message.empty()) {
		message = "No command output was returned.";
	}
	return message;
}

bool IsNothingToCommitMessage(const wxString& commandOutput)
{
	wxString lower = commandOutput.Lower();
	return lower.Find("nothing to commit") != wxNOT_FOUND ||
		lower.Find("working tree clean") != wxNOT_FOUND ||
		lower.Find("no changes added to commit") != wxNOT_FOUND;
}

bool PathEquals(const wxString& left, const wxString& right)
{
#ifdef __WINDOWS__
	return left.CmpNoCase(right) == 0;
#else
	return left == right;
#endif
}

void AddUniquePath(wxArrayString& paths, const wxString& value)
{
	if(value.empty()) {
		return;
	}
	for(size_t i = 0; i < paths.size(); ++i) {
		if(PathEquals(paths[i], value)) {
			return;
		}
	}
	paths.Add(value);
}

void AddUniqueNormalizedPath(wxArrayString& paths, const wxString& value, bool treatAsDirectory = false)
{
	wxString path = value;
	path.Trim(true);
	path.Trim(false);
	if(path.empty()) {
		return;
	}

	wxFileName file_name;
	if(treatAsDirectory || wxFileName::DirExists(path)) {
		file_name.AssignDir(path);
	} else {
		file_name.Assign(path);
	}
	file_name.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_CASE);
	AddUniquePath(paths, file_name.GetFullPath());
}

bool HasGitMarker(const wxString& directoryPath);

void AddUniqueExistingDirectory(wxArrayString& paths, const wxString& value)
{
	wxString path = value;
	path.Trim(true);
	path.Trim(false);
	if(path.empty()) {
		return;
	}
	if(wxFileName::FileExists(path)) {
		wxFileName file_name(path);
		path = file_name.GetPath();
	}
	if(!wxFileName::DirExists(path)) {
		return;
	}
	AddUniqueNormalizedPath(paths, path, true);
}

bool TryResolveGitRepoRoot(const wxString& searchPath, wxString& repoRoot)
{
	wxArrayString output;
	wxArrayString errors;
	long exitCode = -1;

	wxString normalized_search_path = searchPath;
	normalized_search_path.Trim(true);
	normalized_search_path.Trim(false);
	if(normalized_search_path.empty()) {
		return false;
	}

	wxFileName search_path_name;
	if(wxFileName::FileExists(normalized_search_path)) {
		search_path_name.Assign(normalized_search_path);
		normalized_search_path = search_path_name.GetPath();
	} else {
		search_path_name.AssignDir(normalized_search_path);
	}
	search_path_name.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_CASE);
	normalized_search_path = search_path_name.GetPath(wxPATH_GET_VOLUME);
	normalized_search_path.Trim(true);
	normalized_search_path.Trim(false);
	if(normalized_search_path.empty() || !wxFileName::DirExists(normalized_search_path)) {
		return false;
	}

	const wxString command = "git rev-parse --show-toplevel";
	if(ExecuteCommandSync(command, output, errors, exitCode, normalized_search_path) && !output.empty()) {
		repoRoot = output.front();
		repoRoot.Trim(true);
		repoRoot.Trim(false);
		return !repoRoot.empty();
	}

	wxString current_dir = normalized_search_path;
	wxString previous_dir;
	while(!current_dir.empty() && !PathEquals(current_dir, previous_dir)) {
		if(HasGitMarker(current_dir)) {
			repoRoot = current_dir;
			return true;
		}

		wxFileName parent_dir;
		parent_dir.AssignDir(current_dir);
		if(parent_dir.GetDirCount() == 0) {
			break;
		}
		parent_dir.RemoveLastDir();

		previous_dir = current_dir;
		current_dir = parent_dir.GetPath(wxPATH_GET_VOLUME);
		current_dir.Trim(true);
		current_dir.Trim(false);
	}
	return false;
}

bool ConvertToRepoRelativePath(const wxString& absolutePath, const wxString& repoRoot, wxString& relativePath)
{
	wxString abs = absolutePath;
	abs.Trim(true);
	abs.Trim(false);
	if(abs.empty() || repoRoot.empty()) {
		return false;
	}

	wxFileName file_name;
	if(wxFileName::DirExists(abs)) {
		file_name.AssignDir(abs);
	} else {
		file_name.Assign(abs);
	}
	file_name.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_CASE);

	wxFileName repo_dir;
	repo_dir.AssignDir(repoRoot);
	repo_dir.Normalize(wxPATH_NORM_DOTS | wxPATH_NORM_ABSOLUTE | wxPATH_NORM_CASE);
	const wxString normalized_repo = repo_dir.GetPath(wxPATH_GET_VOLUME | wxPATH_GET_SEPARATOR);

	if(!file_name.MakeRelativeTo(normalized_repo)) {
		return false;
	}

	relativePath = file_name.GetFullPath();
	relativePath.Trim(true);
	relativePath.Trim(false);

	if(relativePath.empty() || relativePath == "." || relativePath == "..") {
		return false;
	}

	if(relativePath.StartsWith("../") || relativePath.StartsWith("..\\") ||
		relativePath.StartsWith(".." + wxString(wxFILE_SEP_PATH))) {
		return false;
	}

	return true;
}

wxString BuildGitPathspecArgs(const wxArrayString& paths)
{
	wxString args;
	for(size_t i = 0; i < paths.size(); ++i) {
		wxString path = paths[i];
		path.Replace("\\", "/");
		args += " " + QuoteShellArgument(path);
	}
	return args;
}

bool HasGitMarker(const wxString& directoryPath)
{
	const wxString gitPath = directoryPath + wxFILE_SEP_PATH + ".git";
	return wxFileName::DirExists(gitPath) || wxFileName::FileExists(gitPath);
}

void CollectGitReposRecursively(const wxString& rootPath, int remainingDepth, wxArrayString& outRepos)
{
	if(remainingDepth < 0 || !wxFileName::DirExists(rootPath)) {
		return;
	}

	if(HasGitMarker(rootPath)) {
		AddUniquePath(outRepos, rootPath);
		return;
	}

	if(remainingDepth == 0) {
		return;
	}

	wxDir dir(rootPath);
	if(!dir.IsOpened()) {
		return;
	}

	wxString childName;
	bool hasMore = dir.GetFirst(&childName, wxEmptyString, wxDIR_DIRS);
	while(hasMore) {
		if(childName != "." && childName != "..") {
			const wxString childPath = rootPath + wxFILE_SEP_PATH + childName;
			CollectGitReposRecursively(childPath, remainingDepth - 1, outRepos);
		}
		hasMore = dir.GetNext(&childName);
	}
}

wxString BuildPathsList(const wxArrayString& paths, size_t maxItems)
{
	wxString message;
	const size_t count = std::min(maxItems, paths.size());
	for(size_t i = 0; i < count; ++i) {
		message += "\n- " + paths[i];
	}
	if(paths.size() > count) {
		message += "\n- ...";
	}
	return message;
}

}

MainToolBar::MainToolBar(wxWindow* parent, wxAuiManager* manager)
{
	wxSize icon_size = FROM_DIP(parent, wxSize(16, 16));
	wxBitmap new_bitmap = wxArtProvider::GetBitmap(wxART_NEW, wxART_TOOLBAR, icon_size);
	wxBitmap open_bitmap = wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_TOOLBAR, icon_size);
	wxBitmap save_bitmap = wxArtProvider::GetBitmap(wxART_FILE_SAVE, wxART_TOOLBAR, icon_size);
	wxBitmap saveas_bitmap = wxArtProvider::GetBitmap(wxART_FILE_SAVE_AS, wxART_TOOLBAR, icon_size);
	wxBitmap undo_bitmap = wxArtProvider::GetBitmap(wxART_UNDO, wxART_TOOLBAR, icon_size);
	wxBitmap redo_bitmap = wxArtProvider::GetBitmap(wxART_REDO, wxART_TOOLBAR, icon_size);
	wxBitmap cut_bitmap = wxArtProvider::GetBitmap(wxART_CUT, wxART_TOOLBAR, icon_size);
	wxBitmap copy_bitmap = wxArtProvider::GetBitmap(wxART_COPY, wxART_TOOLBAR, icon_size);
	wxBitmap paste_bitmap = wxArtProvider::GetBitmap(wxART_PASTE, wxART_TOOLBAR, icon_size);

	standard_toolbar = newd wxAuiToolBar(parent, TOOLBAR_STANDARD, wxDefaultPosition, wxDefaultSize, wxAUI_TB_DEFAULT_STYLE);
	standard_toolbar->SetToolBitmapSize(icon_size);
	standard_toolbar->AddTool(wxID_NEW, wxEmptyString, new_bitmap, wxNullBitmap, wxITEM_NORMAL, "New Map", wxEmptyString, NULL);
	standard_toolbar->AddTool(wxID_OPEN, wxEmptyString, open_bitmap, wxNullBitmap, wxITEM_NORMAL, "Open Map", wxEmptyString, NULL);
	standard_toolbar->AddTool(wxID_SAVE, wxEmptyString, save_bitmap, wxNullBitmap, wxITEM_NORMAL, "Save Map", wxEmptyString, NULL);
	standard_toolbar->AddTool(wxID_SAVEAS, wxEmptyString, saveas_bitmap, wxNullBitmap, wxITEM_NORMAL, "Save Map As...", wxEmptyString, NULL);
	standard_toolbar->AddSeparator();
	standard_toolbar->AddTool(wxID_UNDO, wxEmptyString, undo_bitmap, wxNullBitmap, wxITEM_NORMAL, "Undo", wxEmptyString, NULL);
	standard_toolbar->AddTool(wxID_REDO, wxEmptyString, redo_bitmap, wxNullBitmap, wxITEM_NORMAL, "Redo", wxEmptyString, NULL);
	standard_toolbar->AddSeparator();
	standard_toolbar->AddTool(wxID_CUT, wxEmptyString, cut_bitmap, wxNullBitmap, wxITEM_NORMAL, "Cut", wxEmptyString, NULL);
	standard_toolbar->AddTool(wxID_COPY, wxEmptyString, copy_bitmap, wxNullBitmap, wxITEM_NORMAL, "Copy", wxEmptyString, NULL);
	standard_toolbar->AddTool(wxID_PASTE, wxEmptyString, paste_bitmap, wxNullBitmap, wxITEM_NORMAL, "Paste", wxEmptyString, NULL);
	standard_toolbar->AddSeparator();
	standard_toolbar->AddStretchSpacer(1);
	const wxSize deploy_size = FROM_DIP(parent, wxSize(180, 24));
	deploy_button = newd wxButton(standard_toolbar, TOOLBAR_DEPLOY_MAP, "DEPLOY", wxDefaultPosition, deploy_size);
	wxFont deploy_font = deploy_button->GetFont().Bold();
	deploy_button->SetFont(deploy_font);
	deploy_button->SetBackgroundColour(wxColour(44, 170, 78));
	deploy_button->SetForegroundColour(*wxWHITE);
	deploy_button->SetToolTip("Deploy map to git (commit + push)");
	deploy_button->Enable(false);
	deploy_button->SetMinSize(deploy_size);
	deploy_button->SetMaxSize(deploy_size);
	wxAuiToolBarItem* deploy_item = standard_toolbar->AddControl(deploy_button);
	if(deploy_item) {
		deploy_item->SetMinSize(deploy_size);
		deploy_item->SetProportion(0);
	}
	standard_toolbar->Realize();
	ApplyToolbarTheme(standard_toolbar);
	Theme::ApplyText(standard_toolbar, true);

	wxBitmap* border_bitmap = loadPNGFile(optional_border_small_png);
	wxBitmap* eraser_bitmap = loadPNGFile(eraser_small_png);
	wxBitmap pz_bitmap = wxArtProvider::GetBitmap(ART_PZ_BRUSH, wxART_TOOLBAR, icon_size);
	wxBitmap nopvp_bitmap = wxArtProvider::GetBitmap(ART_NOPVP_BRUSH, wxART_TOOLBAR, icon_size);
	wxBitmap nologout_bitmap = wxArtProvider::GetBitmap(ART_NOLOOUT_BRUSH, wxART_TOOLBAR, icon_size);
	wxBitmap pvp_bitmap = wxArtProvider::GetBitmap(ART_PVP_BRUSH, wxART_TOOLBAR, icon_size);
	wxBitmap* normal_bitmap = loadPNGFile(door_normal_small_png);
	wxBitmap* locked_bitmap = loadPNGFile(door_locked_small_png);
	wxBitmap* magic_bitmap = loadPNGFile(door_magic_small_png);
	wxBitmap* quest_bitmap = loadPNGFile(door_quest_small_png);
	wxBitmap* hatch_bitmap = loadPNGFile(window_hatch_small_png);
	wxBitmap* window_bitmap = loadPNGFile(window_normal_small_png);

	brushes_toolbar = newd wxAuiToolBar(parent, TOOLBAR_BRUSHES, wxDefaultPosition, wxDefaultSize, wxAUI_TB_DEFAULT_STYLE);
	brushes_toolbar->SetToolBitmapSize(icon_size);
	brushes_toolbar->AddTool(PALETTE_TERRAIN_OPTIONAL_BORDER_TOOL, wxEmptyString, *border_bitmap, wxNullBitmap, wxITEM_CHECK, "Border", wxEmptyString, NULL);
	brushes_toolbar->AddTool(PALETTE_TERRAIN_ERASER, wxEmptyString, *eraser_bitmap, wxNullBitmap, wxITEM_CHECK, "Eraser", wxEmptyString, NULL);
	brushes_toolbar->AddSeparator();
	brushes_toolbar->AddTool(PALETTE_TERRAIN_PZ_TOOL, wxEmptyString, pz_bitmap, wxNullBitmap, wxITEM_CHECK, "Protected Zone", wxEmptyString, NULL);
	brushes_toolbar->AddTool(PALETTE_TERRAIN_NOPVP_TOOL, wxEmptyString, nopvp_bitmap, wxNullBitmap, wxITEM_CHECK, "No PvP Zone", wxEmptyString, NULL);
	brushes_toolbar->AddTool(PALETTE_TERRAIN_NOLOGOUT_TOOL, wxEmptyString, nologout_bitmap, wxNullBitmap, wxITEM_CHECK, "No Logout Zone", wxEmptyString, NULL);
	brushes_toolbar->AddTool(PALETTE_TERRAIN_PVPZONE_TOOL, wxEmptyString, pvp_bitmap, wxNullBitmap, wxITEM_CHECK, "PvP Zone", wxEmptyString, NULL);
	brushes_toolbar->AddTool(PALETTE_TERRAIN_WORLDBOSS_TOOL, wxEmptyString, pvp_bitmap, wxNullBitmap, wxITEM_CHECK, "World Boss Zone", wxEmptyString, NULL);
	brushes_toolbar->AddSeparator();
	brushes_toolbar->AddTool(PALETTE_TERRAIN_NORMAL_DOOR, wxEmptyString, *normal_bitmap, wxNullBitmap, wxITEM_CHECK, "Normal Door", wxEmptyString, NULL);
	brushes_toolbar->AddTool(PALETTE_TERRAIN_LOCKED_DOOR, wxEmptyString, *locked_bitmap, wxNullBitmap, wxITEM_CHECK, "Locked Door", wxEmptyString, NULL);
	brushes_toolbar->AddTool(PALETTE_TERRAIN_MAGIC_DOOR, wxEmptyString, *magic_bitmap, wxNullBitmap, wxITEM_CHECK, "Magic Door", wxEmptyString, NULL);
	brushes_toolbar->AddTool(PALETTE_TERRAIN_QUEST_DOOR, wxEmptyString, *quest_bitmap, wxNullBitmap, wxITEM_CHECK, "Quest Door", wxEmptyString, NULL);
	brushes_toolbar->AddTool(PALETTE_TERRAIN_HATCH_DOOR, wxEmptyString, *hatch_bitmap, wxNullBitmap, wxITEM_CHECK, "Hatch Window", wxEmptyString, NULL);
	brushes_toolbar->AddTool(PALETTE_TERRAIN_WINDOW_DOOR, wxEmptyString, *window_bitmap, wxNullBitmap, wxITEM_CHECK, "Window", wxEmptyString, NULL);
	brushes_toolbar->Realize();
	ApplyToolbarTheme(brushes_toolbar);
	Theme::ApplyText(brushes_toolbar, true);

	wxBitmap go_bitmap = wxArtProvider::GetBitmap(ART_POSITION_GO, wxART_TOOLBAR, icon_size);

	position_toolbar = newd wxAuiToolBar(parent, TOOLBAR_POSITION, wxDefaultPosition, wxDefaultSize, wxAUI_TB_DEFAULT_STYLE | wxAUI_TB_HORZ_TEXT);
	position_toolbar->SetToolBitmapSize(icon_size);
	x_control = newd NumberTextCtrl(position_toolbar, wxID_ANY, 0, 0, rme::MapMaxWidth, wxTE_PROCESS_ENTER, "X", wxDefaultPosition, FROM_DIP(parent, wxSize(60, 20)));
	x_control->SetToolTip("X Coordinate");
	y_control = newd NumberTextCtrl(position_toolbar, wxID_ANY, 0, 0, rme::MapMaxHeight, wxTE_PROCESS_ENTER, "Y", wxDefaultPosition, FROM_DIP(parent, wxSize(60, 20)));
	y_control->SetToolTip("Y Coordinate");
	z_control = newd NumberTextCtrl(position_toolbar, wxID_ANY, 0, 0, rme::MapMaxLayer, wxTE_PROCESS_ENTER, "Z", wxDefaultPosition, FROM_DIP(parent, wxSize(35, 20)));
	z_control->SetToolTip("Z Coordinate");
	go_button = newd wxButton(position_toolbar, TOOLBAR_POSITION_GO, wxEmptyString, wxDefaultPosition, FROM_DIP(parent, wxSize(22, 20)));
	go_button->SetBitmap(go_bitmap);
	go_button->SetToolTip("Go To Position");
	position_toolbar->AddControl(x_control);
	position_toolbar->AddControl(y_control);
	position_toolbar->AddControl(z_control);
	position_toolbar->AddControl(go_button);
	position_toolbar->Realize();
	ApplyToolbarTheme(position_toolbar);
	Theme::ApplyText(position_toolbar, true);
	StylePositionControl(x_control);
	StylePositionControl(y_control);
	StylePositionControl(z_control);
	go_button->SetBackgroundColour(Theme::Dark().controlHover);
	go_button->SetForegroundColour(Theme::Dark().text);

	wxBitmap circular_bitmap = wxArtProvider::GetBitmap(ART_CIRCULAR, wxART_TOOLBAR, icon_size);
	wxBitmap rectangular_bitmap = wxArtProvider::GetBitmap(ART_RECTANGULAR, wxART_TOOLBAR, icon_size);
	wxBitmap size1_bitmap = wxArtProvider::GetBitmap(ART_RECTANGULAR_1, wxART_TOOLBAR, icon_size);
	wxBitmap size2_bitmap = wxArtProvider::GetBitmap(ART_RECTANGULAR_2, wxART_TOOLBAR, icon_size);
	wxBitmap size3_bitmap = wxArtProvider::GetBitmap(ART_RECTANGULAR_3, wxART_TOOLBAR, icon_size);
	wxBitmap size4_bitmap = wxArtProvider::GetBitmap(ART_RECTANGULAR_4, wxART_TOOLBAR, icon_size);
	wxBitmap size5_bitmap = wxArtProvider::GetBitmap(ART_RECTANGULAR_5, wxART_TOOLBAR, icon_size);
	wxBitmap size6_bitmap = wxArtProvider::GetBitmap(ART_RECTANGULAR_6, wxART_TOOLBAR, icon_size);
	wxBitmap size7_bitmap = wxArtProvider::GetBitmap(ART_RECTANGULAR_7, wxART_TOOLBAR, icon_size);

	sizes_toolbar = newd wxAuiToolBar(parent, TOOLBAR_SIZES, wxDefaultPosition, wxDefaultSize, wxAUI_TB_DEFAULT_STYLE);
	sizes_toolbar->SetToolBitmapSize(icon_size);
	sizes_toolbar->AddTool(TOOLBAR_SIZES_RECTANGULAR, wxEmptyString, rectangular_bitmap, wxNullBitmap, wxITEM_CHECK, "Rectangular Brush", wxEmptyString, NULL);
	sizes_toolbar->AddTool(TOOLBAR_SIZES_CIRCULAR, wxEmptyString, circular_bitmap, wxNullBitmap, wxITEM_CHECK, "Circular Brush", wxEmptyString, NULL);
	sizes_toolbar->AddSeparator();
	sizes_toolbar->AddTool(TOOLBAR_SIZES_1, wxEmptyString, size1_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 1", wxEmptyString, NULL);
	sizes_toolbar->AddTool(TOOLBAR_SIZES_2, wxEmptyString, size2_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 2", wxEmptyString, NULL);
	sizes_toolbar->AddTool(TOOLBAR_SIZES_3, wxEmptyString, size3_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 3", wxEmptyString, NULL);
	sizes_toolbar->AddTool(TOOLBAR_SIZES_4, wxEmptyString, size4_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 4", wxEmptyString, NULL);
	sizes_toolbar->AddTool(TOOLBAR_SIZES_5, wxEmptyString, size5_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 5", wxEmptyString, NULL);
	sizes_toolbar->AddTool(TOOLBAR_SIZES_6, wxEmptyString, size6_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 6", wxEmptyString, NULL);
	sizes_toolbar->AddTool(TOOLBAR_SIZES_7, wxEmptyString, size7_bitmap, wxNullBitmap, wxITEM_CHECK, "Size 7", wxEmptyString, NULL);
	sizes_toolbar->Realize();
	sizes_toolbar->ToggleTool(TOOLBAR_SIZES_RECTANGULAR, true);
	sizes_toolbar->ToggleTool(TOOLBAR_SIZES_1, true);
	ApplyToolbarTheme(sizes_toolbar);
	Theme::ApplyText(sizes_toolbar, true);

	wxBitmap hooks_bitmap = wxArtProvider::GetBitmap(ART_HOOKS_TOOLBAR, wxART_TOOLBAR, icon_size);
	wxBitmap pickupables_bitmap = wxArtProvider::GetBitmap(ART_PICKUPABLE_TOOLBAR, wxART_TOOLBAR, icon_size);
	wxBitmap moveables_bitmap = wxArtProvider::GetBitmap(ART_MOVEABLE_TOOLBAR, wxART_TOOLBAR, icon_size);
	wxBitmap wall_borders_bitmap = wxArtProvider::GetBitmap(ART_WALL_BORDERS_TOOLBAR, wxART_TOOLBAR, icon_size);
	wxBitmap mountain_overlay_bitmap = wxArtProvider::GetBitmap(ART_MOUNTAIN_OVERLAY_TOOLBAR, wxART_TOOLBAR, icon_size);
	wxBitmap stair_direction_bitmap = wxArtProvider::GetBitmap(ART_STAIR_DIRECTION_TOOLBAR, wxART_TOOLBAR, icon_size);

	indicators_toolbar = newd wxAuiToolBar(parent, TOOLBAR_INDICATORS, wxDefaultPosition, wxDefaultSize, wxAUI_TB_DEFAULT_STYLE);
	indicators_toolbar->SetToolBitmapSize(icon_size);
	indicators_toolbar->AddTool(TOOLBAR_HOOKS, wxEmptyString, hooks_bitmap, wxNullBitmap, wxITEM_CHECK, "Wall Hooks", wxEmptyString, NULL);
	indicators_toolbar->AddTool(TOOLBAR_PICKUPABLES, wxEmptyString, pickupables_bitmap, wxNullBitmap, wxITEM_CHECK, "Pickupables", wxEmptyString, NULL);
	indicators_toolbar->AddTool(TOOLBAR_MOVEABLES, wxEmptyString, moveables_bitmap, wxNullBitmap, wxITEM_CHECK, "Moveables", wxEmptyString, NULL);
	indicators_toolbar->AddTool(TOOLBAR_WALL_BORDERS, wxEmptyString, wall_borders_bitmap, wxNullBitmap, wxITEM_CHECK, "Wall Borders", wxEmptyString, NULL);
	indicators_toolbar->AddTool(TOOLBAR_MOUNTAIN_OVERLAY, wxEmptyString, mountain_overlay_bitmap, wxNullBitmap, wxITEM_CHECK, "Mountain Overlay", wxEmptyString, NULL);
	indicators_toolbar->AddTool(TOOLBAR_STAIR_DIRECTION, wxEmptyString, stair_direction_bitmap, wxNullBitmap, wxITEM_CHECK, "Stair Direction", wxEmptyString, NULL);
	indicators_toolbar->Realize();
	indicators_toolbar->ToggleTool(TOOLBAR_HOOKS, g_settings.getBoolean(Config::SHOW_WALL_HOOKS));
	indicators_toolbar->ToggleTool(TOOLBAR_PICKUPABLES, g_settings.getBoolean(Config::SHOW_PICKUPABLES));
	indicators_toolbar->ToggleTool(TOOLBAR_MOVEABLES, g_settings.getBoolean(Config::SHOW_MOVEABLES));
	indicators_toolbar->ToggleTool(TOOLBAR_WALL_BORDERS, g_settings.getBoolean(Config::SHOW_WALL_BORDERS));
	indicators_toolbar->ToggleTool(TOOLBAR_MOUNTAIN_OVERLAY, g_settings.getBoolean(Config::SHOW_MOUNTAIN_OVERLAY));
	indicators_toolbar->ToggleTool(TOOLBAR_STAIR_DIRECTION, g_settings.getBoolean(Config::SHOW_STAIR_DIRECTION));
	ApplyToolbarTheme(indicators_toolbar);
	Theme::ApplyText(indicators_toolbar, true);

	manager->AddPane(standard_toolbar, wxAuiPaneInfo().Name(STANDARD_BAR_NAME).ToolbarPane().Top().Row(1).Position(1).Floatable(false));
	manager->AddPane(brushes_toolbar, wxAuiPaneInfo().Name(BRUSHES_BAR_NAME).ToolbarPane().Top().Row(1).Position(2).Floatable(false));
	manager->AddPane(position_toolbar, wxAuiPaneInfo().Name(POSITION_BAR_NAME).ToolbarPane().Top().Row(1).Position(4).Floatable(false));
	manager->AddPane(sizes_toolbar, wxAuiPaneInfo().Name(SIZES_BAR_NAME).ToolbarPane().Top().Row(1).Position(3).Floatable(false));
	manager->AddPane(indicators_toolbar, wxAuiPaneInfo().Name(INDICATORS_BAR_NAME).ToolbarPane().Top().Row(1).Position(5).Floatable(false));

	standard_toolbar->Bind(wxEVT_COMMAND_MENU_SELECTED, &MainToolBar::OnStandardButtonClick, this);
	brushes_toolbar->Bind(wxEVT_COMMAND_MENU_SELECTED, &MainToolBar::OnBrushesButtonClick, this);
	x_control->Bind(wxEVT_TEXT_PASTE, &MainToolBar::OnPastePositionText, this);
	x_control->Bind(wxEVT_KEY_UP, &MainToolBar::OnPositionKeyUp, this);
	y_control->Bind(wxEVT_TEXT_PASTE, &MainToolBar::OnPastePositionText, this);
	y_control->Bind(wxEVT_KEY_UP, &MainToolBar::OnPositionKeyUp, this);
	z_control->Bind(wxEVT_TEXT_PASTE, &MainToolBar::OnPastePositionText, this);
	z_control->Bind(wxEVT_KEY_UP, &MainToolBar::OnPositionKeyUp, this);
	go_button->Bind(wxEVT_BUTTON, &MainToolBar::OnPositionButtonClick, this);
	deploy_button->Bind(wxEVT_BUTTON, &MainToolBar::OnDeployButtonClick, this);
	sizes_toolbar->Bind(wxEVT_COMMAND_MENU_SELECTED, &MainToolBar::OnSizesButtonClick, this);
	indicators_toolbar->Bind(wxEVT_COMMAND_MENU_SELECTED, &MainToolBar::OnIndicatorsButtonClick, this);

	HideAll();
}

MainToolBar::~MainToolBar()
{
	standard_toolbar->Unbind(wxEVT_COMMAND_MENU_SELECTED, &MainToolBar::OnStandardButtonClick, this);
	brushes_toolbar->Unbind(wxEVT_COMMAND_MENU_SELECTED, &MainToolBar::OnBrushesButtonClick, this);
	x_control->Unbind(wxEVT_TEXT_PASTE, &MainToolBar::OnPastePositionText, this);
	x_control->Unbind(wxEVT_KEY_UP, &MainToolBar::OnPositionKeyUp, this);
	y_control->Unbind(wxEVT_TEXT_PASTE, &MainToolBar::OnPastePositionText, this);
	y_control->Unbind(wxEVT_KEY_UP, &MainToolBar::OnPositionKeyUp, this);
	z_control->Unbind(wxEVT_TEXT_PASTE, &MainToolBar::OnPastePositionText, this);
	z_control->Unbind(wxEVT_KEY_UP, &MainToolBar::OnPositionKeyUp, this);
	go_button->Unbind(wxEVT_BUTTON, &MainToolBar::OnPositionButtonClick, this);
	deploy_button->Unbind(wxEVT_BUTTON, &MainToolBar::OnDeployButtonClick, this);
	sizes_toolbar->Unbind(wxEVT_COMMAND_MENU_SELECTED, &MainToolBar::OnSizesButtonClick, this);
	indicators_toolbar->Unbind(wxEVT_COMMAND_MENU_SELECTED, &MainToolBar::OnIndicatorsButtonClick, this);
}

void MainToolBar::UpdateButtons()
{
	Editor* editor = g_gui.GetCurrentEditor();
	if(editor) {
		standard_toolbar->EnableTool(wxID_UNDO, editor->canUndo());
		standard_toolbar->EnableTool(wxID_REDO, editor->canRedo());
		standard_toolbar->EnableTool(wxID_PASTE, editor->copybuffer.canPaste());
	} else {
		standard_toolbar->EnableTool(wxID_UNDO, false);
		standard_toolbar->EnableTool(wxID_REDO, false);
		standard_toolbar->EnableTool(wxID_PASTE, false);
	}

	bool has_map = editor != nullptr;
	bool is_host = has_map && !editor->IsLiveClient();

	standard_toolbar->EnableTool(wxID_SAVE, is_host);
	standard_toolbar->EnableTool(wxID_SAVEAS, is_host);
	standard_toolbar->EnableTool(wxID_CUT, has_map);
	standard_toolbar->EnableTool(wxID_COPY, has_map);
	if(deploy_button) {
		const bool has_deploy_target = has_map && is_host && editor->getMap().hasFile();
		deploy_button->Enable(has_deploy_target);
	}
	standard_toolbar->Refresh();

	brushes_toolbar->EnableTool(PALETTE_TERRAIN_OPTIONAL_BORDER_TOOL, has_map);
	brushes_toolbar->EnableTool(PALETTE_TERRAIN_ERASER, has_map);
	brushes_toolbar->EnableTool(PALETTE_TERRAIN_PZ_TOOL, has_map);
	brushes_toolbar->EnableTool(PALETTE_TERRAIN_NOPVP_TOOL, has_map);
	brushes_toolbar->EnableTool(PALETTE_TERRAIN_NOLOGOUT_TOOL, has_map);
	brushes_toolbar->EnableTool(PALETTE_TERRAIN_PVPZONE_TOOL, has_map);
	brushes_toolbar->EnableTool(PALETTE_TERRAIN_NORMAL_DOOR, has_map);
	brushes_toolbar->EnableTool(PALETTE_TERRAIN_LOCKED_DOOR, has_map);
	brushes_toolbar->EnableTool(PALETTE_TERRAIN_MAGIC_DOOR, has_map);
	brushes_toolbar->EnableTool(PALETTE_TERRAIN_QUEST_DOOR, has_map);
	brushes_toolbar->EnableTool(PALETTE_TERRAIN_HATCH_DOOR, has_map);
	brushes_toolbar->EnableTool(PALETTE_TERRAIN_WINDOW_DOOR, has_map);
	brushes_toolbar->Refresh();

	position_toolbar->EnableTool(TOOLBAR_POSITION_GO, has_map);
	x_control->Enable(has_map);
	y_control->Enable(has_map);
	z_control->Enable(has_map);

	if(has_map) {
		x_control->SetMaxValue(editor->getMapWidth());
		y_control->SetMaxValue(editor->getMapHeight());
	}

	sizes_toolbar->EnableTool(TOOLBAR_SIZES_CIRCULAR, has_map);
	sizes_toolbar->EnableTool(TOOLBAR_SIZES_RECTANGULAR, has_map);
	sizes_toolbar->EnableTool(TOOLBAR_SIZES_1, has_map);
	sizes_toolbar->EnableTool(TOOLBAR_SIZES_2, has_map);
	sizes_toolbar->EnableTool(TOOLBAR_SIZES_3, has_map);
	sizes_toolbar->EnableTool(TOOLBAR_SIZES_4, has_map);
	sizes_toolbar->EnableTool(TOOLBAR_SIZES_5, has_map);
	sizes_toolbar->EnableTool(TOOLBAR_SIZES_6, has_map);
	sizes_toolbar->EnableTool(TOOLBAR_SIZES_7, has_map);
	sizes_toolbar->Refresh();
}

void MainToolBar::UpdateBrushButtons()
{
	Brush* brush = g_gui.GetCurrentBrush();
	if(brush) {
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_OPTIONAL_BORDER_TOOL, brush == g_gui.optional_brush);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_ERASER, brush == g_gui.eraser);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_PZ_TOOL, brush == g_gui.pz_brush);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_NOPVP_TOOL, brush == g_gui.rook_brush);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_NOLOGOUT_TOOL, brush == g_gui.nolog_brush);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_PVPZONE_TOOL, brush == g_gui.pvp_brush);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_WORLDBOSS_TOOL, brush == g_gui.worldboss_brush);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_NORMAL_DOOR, brush == g_gui.normal_door_brush);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_LOCKED_DOOR, brush == g_gui.locked_door_brush);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_MAGIC_DOOR, brush == g_gui.magic_door_brush);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_QUEST_DOOR, brush == g_gui.quest_door_brush);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_HATCH_DOOR, brush == g_gui.hatch_door_brush);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_WINDOW_DOOR, brush == g_gui.window_door_brush);
	} else {
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_OPTIONAL_BORDER_TOOL, false);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_ERASER, false);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_PZ_TOOL, false);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_NOPVP_TOOL, false);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_NOLOGOUT_TOOL, false);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_PVPZONE_TOOL, false);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_WORLDBOSS_TOOL, false);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_NORMAL_DOOR, false);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_LOCKED_DOOR, false);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_MAGIC_DOOR, false);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_QUEST_DOOR, false);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_HATCH_DOOR, false);
		brushes_toolbar->ToggleTool(PALETTE_TERRAIN_WINDOW_DOOR, false);
	}
	g_gui.GetAuiManager()->Update();
}

void MainToolBar::UpdateBrushSize(BrushShape shape, int size)
{
	if(shape == BRUSHSHAPE_CIRCLE) {
		sizes_toolbar->ToggleTool(TOOLBAR_SIZES_CIRCULAR, true);
		sizes_toolbar->ToggleTool(TOOLBAR_SIZES_RECTANGULAR, false);

		wxSize icon_size = wxSize(16, 16);
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_1, wxArtProvider::GetBitmap(ART_CIRCULAR_1, wxART_TOOLBAR, icon_size));
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_2, wxArtProvider::GetBitmap(ART_CIRCULAR_2, wxART_TOOLBAR, icon_size));
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_3, wxArtProvider::GetBitmap(ART_CIRCULAR_3, wxART_TOOLBAR, icon_size));
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_4, wxArtProvider::GetBitmap(ART_CIRCULAR_4, wxART_TOOLBAR, icon_size));
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_5, wxArtProvider::GetBitmap(ART_CIRCULAR_5, wxART_TOOLBAR, icon_size));
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_6, wxArtProvider::GetBitmap(ART_CIRCULAR_6, wxART_TOOLBAR, icon_size));
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_7, wxArtProvider::GetBitmap(ART_CIRCULAR_7, wxART_TOOLBAR, icon_size));
	} else {
		sizes_toolbar->ToggleTool(TOOLBAR_SIZES_CIRCULAR, false);
		sizes_toolbar->ToggleTool(TOOLBAR_SIZES_RECTANGULAR, true);

		wxSize icon_size = wxSize(16, 16);
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_1, wxArtProvider::GetBitmap(ART_RECTANGULAR_1, wxART_TOOLBAR, icon_size));
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_2, wxArtProvider::GetBitmap(ART_RECTANGULAR_2, wxART_TOOLBAR, icon_size));
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_3, wxArtProvider::GetBitmap(ART_RECTANGULAR_3, wxART_TOOLBAR, icon_size));
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_4, wxArtProvider::GetBitmap(ART_RECTANGULAR_4, wxART_TOOLBAR, icon_size));
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_5, wxArtProvider::GetBitmap(ART_RECTANGULAR_5, wxART_TOOLBAR, icon_size));
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_6, wxArtProvider::GetBitmap(ART_RECTANGULAR_6, wxART_TOOLBAR, icon_size));
		sizes_toolbar->SetToolBitmap(TOOLBAR_SIZES_7, wxArtProvider::GetBitmap(ART_RECTANGULAR_7, wxART_TOOLBAR, icon_size));
	}

	sizes_toolbar->ToggleTool(TOOLBAR_SIZES_1, size == 0);
	sizes_toolbar->ToggleTool(TOOLBAR_SIZES_2, size == 1);
	sizes_toolbar->ToggleTool(TOOLBAR_SIZES_3, size == 2);
	sizes_toolbar->ToggleTool(TOOLBAR_SIZES_4, size == 4);
	sizes_toolbar->ToggleTool(TOOLBAR_SIZES_5, size == 6);
	sizes_toolbar->ToggleTool(TOOLBAR_SIZES_6, size == 8);
	sizes_toolbar->ToggleTool(TOOLBAR_SIZES_7, size == 11);

	g_gui.GetAuiManager()->Update();
}

void MainToolBar::UpdateIndicators()
{
	indicators_toolbar->ToggleTool(TOOLBAR_HOOKS, g_settings.getBoolean(Config::SHOW_WALL_HOOKS));
	indicators_toolbar->ToggleTool(TOOLBAR_PICKUPABLES, g_settings.getBoolean(Config::SHOW_PICKUPABLES));
	indicators_toolbar->ToggleTool(TOOLBAR_MOVEABLES, g_settings.getBoolean(Config::SHOW_MOVEABLES));
	indicators_toolbar->ToggleTool(TOOLBAR_WALL_BORDERS, g_settings.getBoolean(Config::SHOW_WALL_BORDERS));
	indicators_toolbar->ToggleTool(TOOLBAR_MOUNTAIN_OVERLAY, g_settings.getBoolean(Config::SHOW_MOUNTAIN_OVERLAY));
	indicators_toolbar->ToggleTool(TOOLBAR_STAIR_DIRECTION, g_settings.getBoolean(Config::SHOW_STAIR_DIRECTION));

	g_gui.GetAuiManager()->Update();
}

void MainToolBar::Show(ToolBarID id, bool show)
{
	wxAuiManager* manager = g_gui.GetAuiManager();
	if(manager) {
		wxAuiPaneInfo& pane = GetPane(id);
		if(pane.IsOk()) {
			pane.Show(show);
			manager->Update();
		}
	}
}

void MainToolBar::HideAll(bool update)
{
	wxAuiManager* manager = g_gui.GetAuiManager();
	if(!manager)
		return;

	wxAuiPaneInfoArray& panes = manager->GetAllPanes();
	for(int i = 0, count = panes.GetCount(); i < count; ++i) {
		if(!panes.Item(i).IsToolbar())
			panes.Item(i).Hide();
	}

	if(update)
		manager->Update();
}

void MainToolBar::LoadPerspective()
{
	wxAuiManager* manager = g_gui.GetAuiManager();
	if(!manager)
		return;

	if(g_settings.getBoolean(Config::SHOW_TOOLBAR_STANDARD)) {
		std::string info = g_settings.getString(Config::TOOLBAR_STANDARD_LAYOUT);
		if(!info.empty())
			manager->LoadPaneInfo(wxString(info), GetPane(TOOLBAR_STANDARD));
		GetPane(TOOLBAR_STANDARD).Show();
	} else
		GetPane(TOOLBAR_STANDARD).Hide();

	if(g_settings.getBoolean(Config::SHOW_TOOLBAR_BRUSHES)) {
		std::string info = g_settings.getString(Config::TOOLBAR_BRUSHES_LAYOUT);
		if(!info.empty())
			manager->LoadPaneInfo(wxString(info), GetPane(TOOLBAR_BRUSHES));
		GetPane(TOOLBAR_BRUSHES).Show();
	} else
		GetPane(TOOLBAR_BRUSHES).Hide();

	if(g_settings.getBoolean(Config::SHOW_TOOLBAR_POSITION)) {
		std::string info = g_settings.getString(Config::TOOLBAR_POSITION_LAYOUT);
		if(!info.empty())
			manager->LoadPaneInfo(wxString(info), GetPane(TOOLBAR_POSITION));
		GetPane(TOOLBAR_POSITION).Show();
	} else
		GetPane(TOOLBAR_POSITION).Hide();

	if(g_settings.getBoolean(Config::SHOW_TOOLBAR_SIZES)) {
		std::string info = g_settings.getString(Config::TOOLBAR_SIZES_LAYOUT);
		if(!info.empty())
			manager->LoadPaneInfo(wxString(info), GetPane(TOOLBAR_SIZES));
		GetPane(TOOLBAR_SIZES).Show();
	} else
		GetPane(TOOLBAR_SIZES).Hide();

	if(g_settings.getBoolean(Config::SHOW_TOOLBAR_INDICATORS)) {
		std::string info = g_settings.getString(Config::TOOLBAR_INDICATORS_LAYOUT);
		if(!info.empty())
			manager->LoadPaneInfo(wxString(info), GetPane(TOOLBAR_INDICATORS));
		GetPane(TOOLBAR_INDICATORS).Show();
	} else
		GetPane(TOOLBAR_INDICATORS).Hide();

	manager->Update();
}

void MainToolBar::SavePerspective()
{
	wxAuiManager* manager = g_gui.GetAuiManager();
	if(!manager)
		return;

	if(g_settings.getBoolean(Config::SHOW_TOOLBAR_STANDARD)) {
		wxString info = manager->SavePaneInfo(GetPane(TOOLBAR_STANDARD));
		g_settings.setString(Config::TOOLBAR_STANDARD_LAYOUT, info.ToStdString());
	}

	if(g_settings.getBoolean(Config::SHOW_TOOLBAR_BRUSHES)) {
		wxString info = manager->SavePaneInfo(GetPane(TOOLBAR_BRUSHES));
		g_settings.setString(Config::TOOLBAR_BRUSHES_LAYOUT, info.ToStdString());
	}

	if(g_settings.getBoolean(Config::SHOW_TOOLBAR_POSITION)) {
		wxString info = manager->SavePaneInfo(GetPane(TOOLBAR_POSITION));
		g_settings.setString(Config::TOOLBAR_POSITION_LAYOUT, info.ToStdString());
	}

	if(g_settings.getBoolean(Config::SHOW_TOOLBAR_SIZES)) {
		wxString info = manager->SavePaneInfo(GetPane(TOOLBAR_SIZES));
		g_settings.setString(Config::TOOLBAR_SIZES_LAYOUT, info.ToStdString());
	}

	if(g_settings.getBoolean(Config::SHOW_TOOLBAR_INDICATORS)) {
		wxString info = manager->SavePaneInfo(GetPane(TOOLBAR_INDICATORS));
		g_settings.setString(Config::SHOW_TOOLBAR_INDICATORS, info.ToStdString());
	}
}

void MainToolBar::OnStandardButtonClick(wxCommandEvent& event)
{
	switch (event.GetId()) {
		case wxID_NEW:
			g_gui.NewMap();
			break;
		case wxID_OPEN:
			g_gui.OpenMap();
			break;
		case wxID_SAVE:
			g_gui.SaveMap();
			break;
		case wxID_SAVEAS:
			g_gui.SaveMapAs();
			break;
		case wxID_UNDO:
			g_gui.DoUndo();
			break;
		case wxID_REDO:
			g_gui.DoRedo();
			break;
		case wxID_CUT:
			g_gui.DoCut();
			break;
		case wxID_COPY:
			g_gui.DoCopy();
			break;
		case wxID_PASTE:
			g_gui.PreparePaste();
			break;
		default:
			break;
	}
}

void MainToolBar::OnBrushesButtonClick(wxCommandEvent& event)
{
	if(!g_gui.IsEditorOpen())
		return;

	switch (event.GetId()) {
		case PALETTE_TERRAIN_OPTIONAL_BORDER_TOOL:
			g_gui.SelectBrush(g_gui.optional_brush);
			break;
		case PALETTE_TERRAIN_ERASER:
			g_gui.SelectBrush(g_gui.eraser);
			break;
		case PALETTE_TERRAIN_PZ_TOOL:
			g_gui.SelectBrush(g_gui.pz_brush);
			break;
		case PALETTE_TERRAIN_NOPVP_TOOL:
			g_gui.SelectBrush(g_gui.rook_brush);
			break;
		case PALETTE_TERRAIN_NOLOGOUT_TOOL:
			g_gui.SelectBrush(g_gui.nolog_brush);
			break;
		case PALETTE_TERRAIN_PVPZONE_TOOL:
			g_gui.SelectBrush(g_gui.pvp_brush);
			break;
		case PALETTE_TERRAIN_WORLDBOSS_TOOL:
			g_gui.SelectBrush(g_gui.worldboss_brush);
			break;
		case PALETTE_TERRAIN_NORMAL_DOOR:
			g_gui.SelectBrush(g_gui.normal_door_brush);
			break;
		case PALETTE_TERRAIN_LOCKED_DOOR:
			g_gui.SelectBrush(g_gui.locked_door_brush);
			break;
		case PALETTE_TERRAIN_MAGIC_DOOR:
			g_gui.SelectBrush(g_gui.magic_door_brush);
			break;
		case PALETTE_TERRAIN_QUEST_DOOR:
			g_gui.SelectBrush(g_gui.quest_door_brush);
			break;
		case PALETTE_TERRAIN_HATCH_DOOR:
			g_gui.SelectBrush(g_gui.hatch_door_brush);
			break;
		case PALETTE_TERRAIN_WINDOW_DOOR:
			g_gui.SelectBrush(g_gui.window_door_brush);
			break;
		default:
			break;
	}
}

void MainToolBar::OnPositionButtonClick(wxCommandEvent& event)
{
	if(!g_gui.IsEditorOpen())
		return;

	if(event.GetId() == TOOLBAR_POSITION_GO) {
		Position pos(x_control->GetIntValue(), y_control->GetIntValue(), z_control->GetIntValue());
		if(pos.isValid())
			g_gui.SetScreenCenterPosition(pos);
	}
}

void MainToolBar::OnDeployButtonClick(wxCommandEvent& WXUNUSED(event))
{
	if(!g_gui.IsEditorOpen()) {
		return;
	}

	Editor* editor = g_gui.GetCurrentEditor();
	if(!editor || editor->IsLiveClient()) {
		return;
	}

	Map& map = editor->getMap();
	if(!map.hasFile()) {
		wxMessageBox(
			"Nao foi possivel fazer deploy porque o mapa ainda nao foi salvo.",
			"Deploy do Mapa",
			wxOK | wxICON_WARNING,
			g_gui.root);
		return;
	}

	if(map.hasChanged()) {
		const int answer = wxMessageBox(
			"O mapa possui alteracoes nao salvas.\nDeseja salvar antes do deploy?",
			"Deploy do Mapa",
			wxYES_NO | wxCANCEL | wxICON_QUESTION,
			g_gui.root);
		if(answer == wxCANCEL) {
			return;
		}
		if(answer == wxYES) {
			g_gui.SaveMap();
			if(map.hasChanged() || !map.hasFile()) {
				wxMessageBox(
					"Nao foi possivel fazer deploy porque o mapa nao foi salvo.",
					"Deploy do Mapa",
					wxOK | wxICON_WARNING,
					g_gui.root);
				return;
			}
		}
	}

	wxArrayString output;
	wxArrayString errors;
	long exit_code = -1;

	if(!ExecuteCommandSync("git --version", output, errors, exit_code)) {
		wxMessageBox(
			"Nao foi possivel fazer deploy porque o Git nao esta disponivel no sistema.",
			"Deploy do Mapa",
			wxOK | wxICON_ERROR,
			g_gui.root);
		return;
	}

	wxArrayString candidate_paths;
	AddUniqueExistingDirectory(candidate_paths, wxstr(map.getBTMapPath()));
	AddUniqueExistingDirectory(candidate_paths, wxstr(map.getFilename()));

	wxFileName filename_path(wxstr(map.getFilename()));
	AddUniqueExistingDirectory(candidate_paths, filename_path.GetPath());

	if(candidate_paths.empty()) {
		wxMessageBox(
			"Nao foi possivel fazer deploy.\nNao foi possivel determinar o caminho do mapa.",
			"Deploy do Mapa",
			wxOK | wxICON_WARNING,
			g_gui.root);
		return;
	}

	wxString repo_root;
	for(size_t i = 0; i < candidate_paths.size(); ++i) {
		if(TryResolveGitRepoRoot(candidate_paths[i], repo_root)) {
			break;
		}
	}

	if(repo_root.empty()) {
		wxArrayString nested_roots;
		for(size_t i = 0; i < candidate_paths.size(); ++i) {
			const wxString& base = candidate_paths[i];
			AddUniqueExistingDirectory(nested_roots, base);
			AddUniqueExistingDirectory(nested_roots, base + wxFILE_SEP_PATH + "data" + wxFILE_SEP_PATH + "scripts" + wxFILE_SEP_PATH + "maps");
			AddUniqueExistingDirectory(nested_roots, base + wxFILE_SEP_PATH + "data" + wxFILE_SEP_PATH + "world");
			AddUniqueExistingDirectory(nested_roots, base + wxFILE_SEP_PATH + "maps");
		}

		wxArrayString nested_repo_paths;
		for(size_t i = 0; i < nested_roots.size(); ++i) {
			CollectGitReposRecursively(nested_roots[i], 3, nested_repo_paths);
		}

		wxArrayString nested_repo_roots;
		for(size_t i = 0; i < nested_repo_paths.size(); ++i) {
			wxString resolved_root;
			if(TryResolveGitRepoRoot(nested_repo_paths[i], resolved_root)) {
				AddUniquePath(nested_repo_roots, resolved_root);
			}
		}

		if(nested_repo_roots.size() == 1) {
			repo_root = nested_repo_roots[0];
		} else if(nested_repo_roots.size() > 1) {
			wxMessageBox(
				"Nao foi possivel fazer deploy automaticamente.\nForam encontrados varios repositorios Git candidatos:"
				+ BuildPathsList(nested_repo_roots, 6),
				"Deploy do Mapa",
				wxOK | wxICON_WARNING,
				g_gui.root);
			return;
		}
	}

	if(repo_root.empty()) {
		wxMessageBox(
			"Nao foi possivel fazer deploy.\nNenhum repositorio Git foi encontrado para este mapa.\n\nCaminhos analisados:"
			+ BuildPathsList(candidate_paths, 6),
			"Deploy do Mapa",
			wxOK | wxICON_WARNING,
			g_gui.root);
		return;
	}

	wxArrayString deploy_targets_abs;

	const wxString map_filename = wxstr(map.getFilename());
	const bool map_is_directory = wxFileName::DirExists(map_filename) ||
		map_filename.Lower().EndsWith(".btmap");
	AddUniqueNormalizedPath(deploy_targets_abs, map_filename, map_is_directory);

	wxString auxiliary_base_dir;
	const wxString btmap_path = wxstr(map.getBTMapPath());
	if(map.isBTMapFormat() && !btmap_path.empty()) {
		auxiliary_base_dir = btmap_path;
	}
	if(auxiliary_base_dir.empty()) {
		if(wxFileName::DirExists(map_filename)) {
			auxiliary_base_dir = map_filename;
		} else {
			wxFileName map_file(map_filename);
			auxiliary_base_dir = map_file.GetPath();
		}
	}

	auto add_auxiliary_target = [&](const std::string& aux_name) {
		if(aux_name.empty()) {
			return;
		}
		wxFileName aux_file(wxstr(aux_name));
		if(aux_file.IsRelative() && !auxiliary_base_dir.empty()) {
			aux_file.MakeAbsolute(auxiliary_base_dir);
		}
		AddUniqueNormalizedPath(deploy_targets_abs, aux_file.GetFullPath(), false);
	};

	add_auxiliary_target(map.getHouseFilename());
	add_auxiliary_target(map.getSpawnFilename());

	wxArrayString deploy_targets_rel;
	for(size_t i = 0; i < deploy_targets_abs.size(); ++i) {
		wxString relative_target;
		if(ConvertToRepoRelativePath(deploy_targets_abs[i], repo_root, relative_target)) {
			AddUniquePath(deploy_targets_rel, relative_target);
		}
	}

	if(deploy_targets_rel.empty()) {
		wxMessageBox(
			"Nao foi possivel fazer deploy.\nOs arquivos do mapa nao pertencem ao repositorio Git encontrado.\n\nArquivos alvo:"
			+ BuildPathsList(deploy_targets_abs, 6),
			"Deploy do Mapa",
			wxOK | wxICON_WARNING,
			g_gui.root);
		return;
	}

	const wxString pathspec_args = BuildGitPathspecArgs(deploy_targets_rel);

	wxString command = "git remote";
	if(!ExecuteCommandSync(command, output, errors, exit_code, repo_root)) {
		wxMessageBox(
			"Nao foi possivel validar os remotos do repositorio.\n\n" + BuildCommandOutput(output, errors),
			"Deploy do Mapa",
			wxOK | wxICON_ERROR,
			g_gui.root);
		return;
	}

	bool has_remote = false;
	for(size_t i = 0; i < output.size(); ++i) {
		wxString line = output[i];
		line.Trim(true);
		line.Trim(false);
		if(!line.empty()) {
			has_remote = true;
			break;
		}
	}
	if(!has_remote) {
		wxMessageBox(
			"Nao foi possivel fazer deploy.\nEste repositorio nao possui remoto configurado.",
			"Deploy do Mapa",
			wxOK | wxICON_WARNING,
			g_gui.root);
		return;
	}

	const wxString default_commit_message = "Deploy map update - " + wxDateTime::Now().FormatISOCombined(' ');
	wxTextEntryDialog commit_dialog(
		g_gui.root,
		"Escreva a mensagem do commit:",
		"Mensagem do Commit",
		default_commit_message,
		wxOK | wxCANCEL);

	if(commit_dialog.ShowModal() != wxID_OK) {
		return;
	}

	wxString commit_message = commit_dialog.GetValue();
	commit_message.Trim(true);
	commit_message.Trim(false);
	if(commit_message.empty()) {
		wxMessageBox(
			"A mensagem do commit nao pode ser vazia.\nDeploy cancelado.",
			"Deploy do Mapa",
			wxOK | wxICON_WARNING,
			g_gui.root);
		return;
	}

	command = "git add --" + pathspec_args;
	if(!ExecuteCommandSync(command, output, errors, exit_code, repo_root)) {
		wxMessageBox(
			"Nao foi possivel preparar os arquivos do mapa para commit.\n\n" + BuildCommandOutput(output, errors),
			"Deploy do Mapa",
			wxOK | wxICON_ERROR,
			g_gui.root);
		return;
	}

	command = "git add -u --" + pathspec_args;
	if(!ExecuteCommandSync(command, output, errors, exit_code, repo_root)) {
		wxMessageBox(
			"Nao foi possivel sincronizar remocoes dos arquivos do mapa.\n\n" + BuildCommandOutput(output, errors),
			"Deploy do Mapa",
			wxOK | wxICON_ERROR,
			g_gui.root);
		return;
	}

	command = "git commit -m " + QuoteShellArgument(commit_message) + " --" + pathspec_args;
	const bool commit_ok = ExecuteCommandSync(command, output, errors, exit_code, repo_root);
	if(!commit_ok) {
		const wxString commit_output = BuildCommandOutput(output, errors);
		if(!IsNothingToCommitMessage(commit_output)) {
			wxMessageBox(
				"Nao foi possivel concluir o commit.\n\n" + commit_output,
				"Deploy do Mapa",
				wxOK | wxICON_ERROR,
				g_gui.root);
			return;
		}
	}

	command = "git push";
	if(!ExecuteCommandSync(command, output, errors, exit_code, repo_root)) {
		wxMessageBox(
			"Nao foi possivel concluir o push.\n\n" + BuildCommandOutput(output, errors),
			"Deploy do Mapa",
			wxOK | wxICON_ERROR,
			g_gui.root);
		return;
	}

	wxString commit_hash = "N/A";
	command = "git rev-parse --short HEAD";
	if(ExecuteCommandSync(command, output, errors, exit_code, repo_root) && !output.empty()) {
		commit_hash = output.front();
		commit_hash.Trim(true);
		commit_hash.Trim(false);
	}

	wxString success_message = "Deploy concluido com sucesso.\nRepositorio: " + repo_root;
	if(!commit_hash.empty() && commit_hash != "N/A") {
		success_message += "\nCommit: " + commit_hash;
	}
	wxMessageBox(success_message, "Deploy do Mapa", wxOK | wxICON_INFORMATION, g_gui.root);
}

void MainToolBar::OnPositionKeyUp(wxKeyEvent& event)
{
	if(event.GetKeyCode() == WXK_TAB) {
		if(x_control->HasFocus()) {
			y_control->SelectAll();
			y_control->SetFocus();
		} else if(y_control->HasFocus()) {
			z_control->SelectAll();
			z_control->SetFocus();
		} else if(z_control->HasFocus()) {
			go_button->SetFocus();
		}
	} else if(event.GetKeyCode() == WXK_NUMPAD_ENTER || event.GetKeyCode() == WXK_RETURN) {
		Position pos(x_control->GetIntValue(), y_control->GetIntValue(), z_control->GetIntValue());
		if(pos.isValid())
			g_gui.SetScreenCenterPosition(pos);
	}
	event.Skip();
}

void MainToolBar::OnPastePositionText(wxClipboardTextEvent& event)
{
	Position position;
	if(posFromClipboard(position.x, position.y, position.z)) {
		x_control->SetIntValue(position.x);
		y_control->SetIntValue(position.y);
		z_control->SetIntValue(position.z);
	} else
		event.Skip();
}

void MainToolBar::OnSizesButtonClick(wxCommandEvent& event)
{
	if(!g_gui.IsEditorOpen())
		return;

	switch (event.GetId()) {
		case TOOLBAR_SIZES_CIRCULAR:
			g_gui.SetBrushShape(BRUSHSHAPE_CIRCLE);
			break;
		case TOOLBAR_SIZES_RECTANGULAR:
			g_gui.SetBrushShape(BRUSHSHAPE_SQUARE);
			break;
		case TOOLBAR_SIZES_1:
			g_gui.SetBrushSize(0);
			break;
		case TOOLBAR_SIZES_2:
			g_gui.SetBrushSize(1);
			break;
		case TOOLBAR_SIZES_3:
			g_gui.SetBrushSize(2);
			break;
		case TOOLBAR_SIZES_4:
			g_gui.SetBrushSize(4);
			break;
		case TOOLBAR_SIZES_5:
			g_gui.SetBrushSize(6);
			break;
		case TOOLBAR_SIZES_6:
			g_gui.SetBrushSize(8);
			break;
		case TOOLBAR_SIZES_7:
			g_gui.SetBrushSize(11);
			break;
		default:
			break;
	}
}

void MainToolBar::OnIndicatorsButtonClick(wxCommandEvent& event)
{
	bool toggled = indicators_toolbar->GetToolToggled(event.GetId());
	switch (event.GetId()) {
		case TOOLBAR_HOOKS:
			g_settings.setInteger(Config::SHOW_WALL_HOOKS, toggled);
			g_gui.root->UpdateIndicatorsMenu();
			g_gui.RefreshView();
			break;
		case TOOLBAR_PICKUPABLES:
			g_settings.setInteger(Config::SHOW_PICKUPABLES, toggled);
			g_gui.root->UpdateIndicatorsMenu();
			g_gui.RefreshView();
			break;
		case TOOLBAR_MOVEABLES:
			g_settings.setInteger(Config::SHOW_MOVEABLES, toggled);
			g_gui.root->UpdateIndicatorsMenu();
			g_gui.RefreshView();
			break;
		case TOOLBAR_WALL_BORDERS:
			g_settings.setInteger(Config::SHOW_WALL_BORDERS, toggled);
			g_gui.root->UpdateIndicatorsMenu();
			g_gui.RefreshView();
			break;
		case TOOLBAR_MOUNTAIN_OVERLAY:
			g_settings.setInteger(Config::SHOW_MOUNTAIN_OVERLAY, toggled);
			g_gui.root->UpdateIndicatorsMenu();
			g_gui.RefreshView();
			break;
		case TOOLBAR_STAIR_DIRECTION:
			g_settings.setInteger(Config::SHOW_STAIR_DIRECTION, toggled);
			g_gui.root->UpdateIndicatorsMenu();
			g_gui.RefreshView();
			break;
		default:
			break;
	}
}

wxAuiPaneInfo& MainToolBar::GetPane(ToolBarID id)
{
	wxAuiManager* manager = g_gui.GetAuiManager();
	if(!manager)
		return wxAuiNullPaneInfo;

	switch (id) {
		case TOOLBAR_STANDARD:
			return manager->GetPane(STANDARD_BAR_NAME);
		case TOOLBAR_BRUSHES:
			return manager->GetPane(BRUSHES_BAR_NAME);
		case TOOLBAR_POSITION:
			return manager->GetPane(POSITION_BAR_NAME);
		case TOOLBAR_SIZES:
			return manager->GetPane(SIZES_BAR_NAME);
		case TOOLBAR_INDICATORS:
			return manager->GetPane(INDICATORS_BAR_NAME);
		default:
			return wxAuiNullPaneInfo;
	}
}
