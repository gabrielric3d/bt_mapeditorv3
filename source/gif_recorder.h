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

#ifndef RME_GIF_RECORDER_H_
#define RME_GIF_RECORDER_H_

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

#include <wx/filename.h>

class wxFileOutputStream;

// Animated GIF writer with a fixed 3-3-2 palette and LZW encoder.
class AnimatedGifWriter
{
public:
	AnimatedGifWriter();
	~AnimatedGifWriter();

	bool Open(const wxFileName& path, uint16_t width, uint16_t height, uint16_t delayCs, uint16_t loopCount = 0);
	bool WriteFrame(const uint8_t* rgbData);
	void Close();
	bool IsOpen() const;

private:
	bool writeHeader(uint16_t loopCount);
	bool writeData(const void* buffer, size_t len);
	void writeColorTable();
	void writeLoopExtension(uint16_t loopCount);
	void writeGraphicControlExtension() const;
	void writeImageDescriptor() const;
	void writeTrailer();
	void writeLE16(uint16_t value);

	void buildPalette();
	void convertToPaletteIndices(const uint8_t* rgbData, std::vector<uint8_t>& indices) const;
	bool encodeLzw(const std::vector<uint8_t>& indices, std::vector<uint8_t>& outputBlocks) const;

private:
	std::unique_ptr<wxFileOutputStream> stream;
	wxFileName filePath;
	std::array<uint8_t, 256 * 3> palette;
	mutable std::vector<uint8_t> indexBuffer;

	uint16_t canvasWidth;
	uint16_t canvasHeight;
	uint16_t delayCentiseconds;
};

#endif
