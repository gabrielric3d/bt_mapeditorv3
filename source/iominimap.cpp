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

#include "iominimap.h"

#include "tile.h"
#include "filehandle.h"
#include "editor.h"
#include "gui.h"

#include <wx/image.h>
#include <zlib.h>

void MinimapBlock::updateTile(int x, int y, const MinimapTile& tile)
{
	m_tiles[getTileIndex(x, y)] = tile;
}

IOMinimap::IOMinimap(Editor* editor, MinimapExportFormat format, MinimapExportMode mode, bool updateLoadbar) :
	m_editor(editor),
	m_format(format),
	m_mode(mode),
	m_updateLoadbar(updateLoadbar)
{
}

bool IOMinimap::saveMinimap(const std::string& directory, const std::string& name, int floor)
{
	if(m_mode == MinimapExportMode::AllFloors || m_mode == MinimapExportMode::SelectedArea || m_mode == MinimapExportMode::AreaView) {
		floor = -1;
	} else if(m_mode == MinimapExportMode::GroundFloor) {
		floor = rme::MapGroundLayer;
	} else if(m_mode == MinimapExportMode::SpecificFloor) {
		if(floor < rme::MapMinLayer || floor > rme::MapMaxLayer) {
			floor = rme::MapGroundLayer;
		}
	}

	m_floor = floor;

	if (m_format == MinimapExportFormat::Otmm) {
		return saveOtmm(wxFileName(directory, name + ".otmm"));
	}
	return saveImage(directory, name);
}

bool IOMinimap::saveOtmm(const wxFileName& file)
{
	try
	{
		FileWriteHandle writer(file.GetFullPath().ToStdString());
		if(!writer.isOk()) {
			//error("Unable to open file %s for save minimap", file);
			return false;
		}

		//TODO: compression flag with zlib
		uint32_t flags = 0;

		// header
		writer.addU32(OTMM_SIGNATURE);
		writer.addU16(0); // data start, will be overwritten later
		writer.addU16(OTMM_VERSION);
		writer.addU32(flags);

		// version 1 header
		writer.addString("OTMM 1.0"); // description

		// go back and rewrite where the map data starts
		uint32_t start = writer.tell();
		writer.seek(4);
		writer.addU16(start);
		writer.seek(start);

		unsigned long blockSize = MMBLOCK_SIZE * MMBLOCK_SIZE * sizeof(MinimapTile);
		std::vector<uint8_t> buffer(compressBound(blockSize));
		constexpr int COMPRESS_LEVEL = 3;

		readBlocks();

		for(uint8_t z = 0; z <= rme::MapMaxLayer; ++z) {
			for(auto& it : m_blocks[z]) {
				int index = it.first;
				auto& block = it.second;

				// write index pos
				uint16_t x = static_cast<uint16_t>((index % (65536 / MMBLOCK_SIZE)) * MMBLOCK_SIZE);
				uint16_t y = static_cast<uint16_t>((index / (65536 / MMBLOCK_SIZE)) * MMBLOCK_SIZE);
				writer.addU16(x);
				writer.addU16(y);
				writer.addU8(z);

				unsigned long len = blockSize;
				int ret = compress2(buffer.data(), &len, (uint8_t*)&block.getTiles(), blockSize, COMPRESS_LEVEL);
				assert(ret == Z_OK);
				writer.addU16(len);
				writer.addRAW(buffer.data(), len);
			}
			m_blocks[z].clear();
		}

		// end of file is an invalid pos
		writer.addU16(65535);
		writer.addU16(65535);
		writer.addU8(255);

		writer.flush();
		writer.close();
	} catch(std::exception& e) {
		m_error = wxString::Format("failed to save OTMM minimap: %s", e.what());
		return false;
	}

	return true;
}

bool IOMinimap::saveImage(const std::string& directory, const std::string& name)
{
	try
	{
		switch(m_mode)
		{
			case MinimapExportMode::AllFloors:
			case MinimapExportMode::GroundFloor:
			case MinimapExportMode::SpecificFloor: {
				exportMinimap(directory, name);
				break;
			}
			case MinimapExportMode::SelectedArea: {
				exportSelection(directory, name);
				break;
			}
			case MinimapExportMode::AreaView: {
				exportAreaView(directory, name);
				break;
			}
		}
	}
	catch(std::bad_alloc&)
	{
		m_error = "There is not enough memory available to complete the operation.";
	}

	return true;
}

bool IOMinimap::exportMinimap(const std::string& directory, const std::string& name)
{
	auto& map = m_editor->getMap();
	if(map.size() == 0) {
		return true;
	}

	int min_z = m_floor == -1 ? 0 : m_floor;
	int max_z = m_floor == -1 ? rme::MapMaxLayer : m_floor;

	// Find bounds per floor
	struct FloorBounds {
		int min_x = rme::MapMaxWidth + 1;
		int min_y = rme::MapMaxHeight + 1;
		int max_x = 0;
		int max_y = 0;
		bool valid() const { return max_x >= min_x && max_y >= min_y; }
	};
	FloorBounds bounds[rme::MapLayers];

	int tiles_iterated = 0;
	int total_tiles = static_cast<int>(map.size());

	for(auto it = map.begin(); it != map.end(); ++it) {
		auto tile = (*it)->get();
		if(!tile || (!tile->ground && tile->items.empty())) {
			continue;
		}

		const auto& position = tile->getPosition();
		if(position.z < min_z || position.z > max_z) {
			continue;
		}

		auto& b = bounds[position.z];
		if(position.x < b.min_x) b.min_x = position.x;
		if(position.y < b.min_y) b.min_y = position.y;
		if(position.x > b.max_x) b.max_x = position.x;
		if(position.y > b.max_y) b.max_y = position.y;
	}

	wxString extension = m_format == MinimapExportFormat::Png ? "png" : "bmp";
	wxBitmapType type = m_format == MinimapExportFormat::Png ? wxBITMAP_TYPE_PNG : wxBITMAP_TYPE_BMP;

	for(int z = min_z; z <= max_z; z++) {
		auto& b = bounds[z];
		if(!b.valid()) {
			continue;
		}

		int image_width = b.max_x - b.min_x + 1;
		int image_height = b.max_y - b.min_y + 1;
		size_t pixels_size = static_cast<size_t>(image_width) * image_height * rme::PixelFormatRGB;

		uint8_t* pixels = new uint8_t[pixels_size];
		memset(pixels, 0, pixels_size);

		bool empty = true;
		for(auto it = map.begin(); it != map.end(); ++it) {
			auto tile = (*it)->get();
			if(!tile || (!tile->ground && tile->items.empty())) {
				continue;
			}

			const auto& position = tile->getPosition();
			if(position.z != z) {
				continue;
			}

			if (m_updateLoadbar) {
				++tiles_iterated;
				if(tiles_iterated % 8192 == 0) {
					g_gui.SetLoadDone(int(tiles_iterated / double(total_tiles) * 90.0));
				}
			}

			uint8_t color = tile->getMiniMapColor();
			size_t index = (static_cast<size_t>(position.y - b.min_y) * image_width + (position.x - b.min_x)) * rme::PixelFormatRGB;
			pixels[index  ] = (uint8_t)(static_cast<int>(color / 36) % 6 * 51); // red
			pixels[index+1] = (uint8_t)(static_cast<int>(color / 6) % 6 * 51);  // green
			pixels[index+2] = (uint8_t)(color % 6 * 51);                        // blue
			empty = false;
		}

		if (!empty) {
			wxImage image(image_width, image_height, pixels, true);
			wxFileName file = wxString::Format("%s-%d.%s", name, z, extension);
			file.Normalize(wxPATH_NORM_ALL, directory);
			image.SaveFile(file.GetFullPath(), type);
		}

		delete[] pixels;
	}

	return true;
}

bool IOMinimap::exportAreaView(const std::string& directory, const std::string& name)
{
	auto& map = m_editor->getMap();
	if(map.size() == 0) {
		return true;
	}

	int from_x = std::min(m_fromPos.x, m_toPos.x);
	int from_y = std::min(m_fromPos.y, m_toPos.y);
	int to_x = std::max(m_fromPos.x, m_toPos.x);
	int to_y = std::max(m_fromPos.y, m_toPos.y);

	int base_width = to_x - from_x + 1;
	int base_height = to_y - from_y + 1;
	int ground_z = rme::MapGroundLayer; // 7

	wxString extension = m_format == MinimapExportFormat::Png ? "png" : "bmp";
	wxBitmapType type = m_format == MinimapExportFormat::Png ? wxBITMAP_TYPE_PNG : wxBITMAP_TYPE_BMP;

	int tiles_iterated = 0;
	int total_tiles = static_cast<int>(map.size());

	if(m_mergeFloors) {
		// Merged: single image with all floors composited
		// Upper floors shift by -1x, -1y per level above ground
		int total_offset = ground_z;
		int image_width = base_width + total_offset;
		int image_height = base_height + total_offset;
		size_t pixels_size = static_cast<size_t>(image_width) * image_height * rme::PixelFormatRGB;

		uint8_t* pixels = new uint8_t[pixels_size];
		memset(pixels, 0, pixels_size);

		// Render order: floor 7 (base), then 6 on top, 5 on top, ... 0 on top
		for(int z = ground_z; z >= 0; z--) {
			int offset = ground_z - z; // z=7 -> 0, z=6 -> 1, z=0 -> 7
			int area_from_x = from_x - offset;
			int area_from_y = from_y - offset;
			int area_to_x = to_x - offset;
			int area_to_y = to_y - offset;

			for(auto it = map.begin(); it != map.end(); ++it) {
				auto tile = (*it)->get();
				if(!tile || (!tile->ground && tile->items.empty())) {
					continue;
				}

				const auto& position = tile->getPosition();
				if(position.z != z) {
					continue;
				}

				if(position.x < area_from_x || position.x > area_to_x ||
				   position.y < area_from_y || position.y > area_to_y) {
					continue;
				}

				if (m_updateLoadbar) {
					++tiles_iterated;
					if(tiles_iterated % 8192 == 0) {
						g_gui.SetLoadDone(int(tiles_iterated / double(total_tiles) * 90.0));
					}
				}

				int img_x = position.x - area_from_x;
				int img_y = position.y - area_from_y;

				uint8_t color = tile->getMiniMapColor();
				size_t index = (static_cast<size_t>(img_y) * image_width + img_x) * rme::PixelFormatRGB;
				pixels[index  ] = (uint8_t)(static_cast<int>(color / 36) % 6 * 51);
				pixels[index+1] = (uint8_t)(static_cast<int>(color / 6) % 6 * 51);
				pixels[index+2] = (uint8_t)(color % 6 * 51);
			}
		}

		wxImage image(image_width, image_height, pixels, true);
		wxFileName file = wxString::Format("%s-areaview.%s", name, extension);
		file.Normalize(wxPATH_NORM_ALL, directory);
		image.SaveFile(file.GetFullPath(), type);
		delete[] pixels;
	} else {
		// Separate: one image per floor
		for(int z = 0; z <= ground_z; z++) {
			int offset = ground_z - z;
			int area_from_x = from_x - offset;
			int area_from_y = from_y - offset;
			int area_to_x = to_x - offset;
			int area_to_y = to_y - offset;
			int image_width = area_to_x - area_from_x + 1;
			int image_height = area_to_y - area_from_y + 1;
			size_t pixels_size = static_cast<size_t>(image_width) * image_height * rme::PixelFormatRGB;

			uint8_t* pixels = new uint8_t[pixels_size];
			memset(pixels, 0, pixels_size);

			bool empty = true;
			for(auto it = map.begin(); it != map.end(); ++it) {
				auto tile = (*it)->get();
				if(!tile || (!tile->ground && tile->items.empty())) {
					continue;
				}

				const auto& position = tile->getPosition();
				if(position.z != z) {
					continue;
				}

				if(position.x < area_from_x || position.x > area_to_x ||
				   position.y < area_from_y || position.y > area_to_y) {
					continue;
				}

				if (m_updateLoadbar) {
					++tiles_iterated;
					if(tiles_iterated % 8192 == 0) {
						g_gui.SetLoadDone(int(tiles_iterated / double(total_tiles) * 90.0));
					}
				}

				int img_x = position.x - area_from_x;
				int img_y = position.y - area_from_y;

				uint8_t color = tile->getMiniMapColor();
				size_t index = (static_cast<size_t>(img_y) * image_width + img_x) * rme::PixelFormatRGB;
				pixels[index  ] = (uint8_t)(static_cast<int>(color / 36) % 6 * 51);
				pixels[index+1] = (uint8_t)(static_cast<int>(color / 6) % 6 * 51);
				pixels[index+2] = (uint8_t)(color % 6 * 51);
				empty = false;
			}

			if(!empty) {
				wxImage image(image_width, image_height, pixels, true);
				wxFileName file = wxString::Format("%s-%d.%s", name, z, extension);
				file.Normalize(wxPATH_NORM_ALL, directory);
				image.SaveFile(file.GetFullPath(), type);
			}
			delete[] pixels;
		}
	}

	return true;
}

bool IOMinimap::exportSelection(const std::string& directory, const std::string& name)
{
	int min_x = rme::MapMaxWidth + 1;
	int min_y = rme::MapMaxHeight + 1;
	int min_z = rme::MapMaxLayer + 1;
	int max_x = 0, max_y = 0, max_z = 0;

	const auto& selection = m_editor->getSelection();
	const auto& tiles = selection.getTiles();

	for(auto tile : tiles) {
		if(!tile || (!tile->ground && tile->items.empty())) {
			continue;
		}

		const auto& position = tile->getPosition();
		if(position.x < min_x) {
			min_x = position.x;
		}
		if(position.x > max_x) {
			max_x = position.x;
		}

		if(position.y < min_y) {
			min_y = position.y;
		}
		if(position.y > max_y) {
			max_y = position.y;
		}

		if(position.z < min_z) {
			min_z = position.z;
		}
		if(position.z > max_z) {
			max_z = position.z;
		}
	}

	int numtiles = (max_x - min_x) * (max_y - min_y);
	if(numtiles == 0) {
		return false;
	}

	int image_width = max_x - min_x + 1;
	int image_height = max_y - min_y + 1;
	if(image_width > 2048 || image_height > 2048) {
		g_gui.PopupDialog("Error", "Minimap size greater than 2048px.", wxOK);
		return false;
	}

	int pixels_size = image_width * image_height * rme::PixelFormatRGB;
	uint8_t* pixels = new uint8_t[pixels_size];
	auto image = new wxImage(image_width, image_height, pixels, true);

	int tiles_iterated = 0;
	for(int z = min_z; z <= max_z; z++) {
		bool empty = true;
		memset(pixels, 0, pixels_size);
		for(auto tile : tiles) {
			if(tile->getZ() != z) {
				continue;
			}

			if (m_updateLoadbar) {
				tiles_iterated++;
				if(tiles_iterated % 8192 == 0) {
					g_gui.SetLoadDone(int(tiles_iterated / double(tiles.size()) * 90.0));
				}
			}

			if(!tile->ground && tile->items.empty()) {
				continue;
			}

			uint8_t color = tile->getMiniMapColor();
			uint32_t index = ((tile->getY() - min_y) * image_width + (tile->getX() - min_x)) * 3;
			pixels[index  ] = (uint8_t)(static_cast<int>(color / 36) % 6 * 51); // red
			pixels[index+1] = (uint8_t)(static_cast<int>(color / 6) % 6 * 51);  // green
			pixels[index+2] = (uint8_t)(color % 6 * 51);                        // blue
			empty = false;
		}

		if (!empty) {
			image->SetData(pixels, true);
			wxString extension = m_format == MinimapExportFormat::Png ? "png" : "bmp";
			wxBitmapType type = m_format == MinimapExportFormat::Png ? wxBITMAP_TYPE_PNG : wxBITMAP_TYPE_BMP; 
			wxFileName file = wxString::Format("%s-%d.%s", name, z, extension);
			file.Normalize(wxPATH_NORM_ALL, directory);
			image->SaveFile(file.GetFullPath(), type);
		}
	}

	image->Destroy();
	delete[] pixels;
	return true;
}

void IOMinimap::readBlocks()
{
	if (m_mode == MinimapExportMode::SelectedArea && !m_editor->hasSelection()) {
		return;
	}

	auto& map = m_editor->getMap();

	int tiles_iterated = 0;
	for(auto it = map.begin(); it != map.end(); ++it) {
		auto tile = (*it)->get();

		if (m_updateLoadbar) {
			++tiles_iterated;
			if (tiles_iterated % 8192 == 0) {
				g_gui.SetLoadDone(int(tiles_iterated / double(map.size()) * 90.0));
			}
		}

		if(!tile || (!tile->ground && tile->items.empty())) {
			continue;
		}

		const auto& position = tile->getPosition();

		if (m_mode == MinimapExportMode::SelectedArea) {
			if (!tile->isSelected()) {
				continue;
			}
		} else if (m_floor != -1 && position.z != m_floor) {
			continue;
		}

		MinimapTile minimapTile;
		minimapTile.color = tile->getMiniMapColor();
		minimapTile.flags |= MinimapTileWasSeen;
		if (tile->isBlocking()) {
			minimapTile.flags |= MinimapTileNotWalkable;
		}
		//if (!tile->isPathable()) {
			//minimapTile.flags |= MinimapTileNotPathable;
		//}
		minimapTile.speed = std::min<int>((int)std::ceil(tile->getGroundSpeed() / 10.f), 0xFF);

		auto& blocks = m_blocks[position.z];
		uint32_t index = getBlockIndex(position);
		if (blocks.find(index) == blocks.end()) {
			blocks.insert({ index, MinimapBlock() });
		}

		auto& block = blocks.at(index);
		int offset_x = position.x - (position.x % MMBLOCK_SIZE);
		int offset_y = position.y - (position.y % MMBLOCK_SIZE);
		block.updateTile(position.x - offset_x, position.y - offset_y, minimapTile);
	}
}
