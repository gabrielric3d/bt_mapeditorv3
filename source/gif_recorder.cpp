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

#include "gif_recorder.h"

#include <algorithm>
#include <unordered_map>

#include <wx/wfstream.h>

namespace
{
constexpr int kPaletteSize = 256;
constexpr int kLzwColorDepth = 8;
constexpr int kGifLzwLimit = 4096;

class BitWriter
{
public:
	void Write(uint16_t code, int bits)
	{
		bitBuffer |= static_cast<uint32_t>(code) << bitCount;
		bitCount += bits;
		while(bitCount >= 8) {
			data.push_back(static_cast<uint8_t>(bitBuffer & 0xFF));
			bitBuffer >>= 8;
			bitCount -= 8;
		}
	}

	void Flush()
	{
		if(bitCount > 0) {
			data.push_back(static_cast<uint8_t>(bitBuffer & 0xFF));
			bitBuffer = 0;
			bitCount = 0;
		}
	}

	const std::vector<uint8_t>& GetData() const noexcept { return data; }

private:
	uint32_t bitBuffer = 0;
	int bitCount = 0;
	std::vector<uint8_t> data;
};

uint8_t Quantize332(uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t r_idx = r >> 5;
	uint8_t g_idx = g >> 5;
	uint8_t b_idx = b >> 6;
	return static_cast<uint8_t>((r_idx << 5) | (g_idx << 2) | b_idx);
}

uint8_t ChannelValue(uint8_t idx, uint8_t maxValue)
{
	if(maxValue == 0)
		return 0;
	return static_cast<uint8_t>((idx * 255) / maxValue);
}
} // namespace

AnimatedGifWriter::AnimatedGifWriter() :
	stream(nullptr),
	canvasWidth(0),
	canvasHeight(0),
	delayCentiseconds(0)
{
	buildPalette();
}

AnimatedGifWriter::~AnimatedGifWriter()
{
	Close();
}

bool AnimatedGifWriter::Open(const wxFileName& path, uint16_t width, uint16_t height, uint16_t delayCs, uint16_t loopCount)
{
	Close();

	filePath = path;
	stream = std::make_unique<wxFileOutputStream>(path.GetFullPath());
	if(!stream || !stream->IsOk()) {
		stream.reset();
		return false;
	}

	canvasWidth = width;
	canvasHeight = height;
	delayCentiseconds = delayCs;
	indexBuffer.resize(static_cast<size_t>(canvasWidth) * canvasHeight);

	if(!writeHeader(loopCount)) {
		Close();
		return false;
	}

	return true;
}

bool AnimatedGifWriter::IsOpen() const
{
	return stream && stream->IsOk();
}

void AnimatedGifWriter::Close()
{
	if(stream && stream->IsOk()) {
		writeTrailer();
	}
	stream.reset();
}

bool AnimatedGifWriter::WriteFrame(const uint8_t* rgbData)
{
	if(!IsOpen() || !rgbData)
		return false;

	convertToPaletteIndices(rgbData, indexBuffer);

	std::vector<uint8_t> encodedBlocks;
	if(!encodeLzw(indexBuffer, encodedBlocks))
		return false;

	writeGraphicControlExtension();
	writeImageDescriptor();

	const uint8_t minCodeSize = kLzwColorDepth;
	if(!writeData(&minCodeSize, 1))
		return false;

	if(!encodedBlocks.empty() && !writeData(encodedBlocks.data(), encodedBlocks.size()))
		return false;

	return stream->IsOk();
}

bool AnimatedGifWriter::writeHeader(uint16_t loopCount)
{
	static const uint8_t signature[] = {'G','I','F','8','9','a'};
	if(!writeData(signature, sizeof(signature)))
		return false;

	writeLE16(canvasWidth);
	writeLE16(canvasHeight);

	const uint8_t packed = 0xF7; // global color table present + 8bpp + 256 entries
	const uint8_t bgColorIndex = 0;
	const uint8_t pixelAspectRatio = 0;

	if(!writeData(&packed, 1) || !writeData(&bgColorIndex, 1) || !writeData(&pixelAspectRatio, 1))
		return false;

	writeColorTable();
	writeLoopExtension(loopCount);
	return stream->IsOk();
}

bool AnimatedGifWriter::writeData(const void* buffer, size_t len)
{
	if(!stream || !stream->IsOk())
		return false;
	stream->Write(buffer, len);
	return stream->IsOk();
}

void AnimatedGifWriter::writeColorTable()
{
	writeData(palette.data(), palette.size());
}

void AnimatedGifWriter::writeLoopExtension(uint16_t loopCount)
{
	// Application extension: NETSCAPE2.0 for loop count
	const uint8_t introducer = 0x21;
	const uint8_t label = 0xFF;
	const uint8_t blockSize = 0x0B;
	static const char appId[] = "NETSCAPE2.0";

	writeData(&introducer, 1);
	writeData(&label, 1);
	writeData(&blockSize, 1);
	writeData(appId, blockSize);

	const uint8_t subBlockLen = 0x03;
	const uint8_t subBlockId = 0x01;
	writeData(&subBlockLen, 1);
	writeData(&subBlockId, 1);
	writeLE16(loopCount);

	const uint8_t terminator = 0x00;
	writeData(&terminator, 1);
}

void AnimatedGifWriter::writeGraphicControlExtension() const
{
	const uint8_t introducer = 0x21;
	const uint8_t label = 0xF9;
	const uint8_t blockSize = 0x04;
	const uint8_t packed = 0x04; // disposal method = 1 (do not dispose)
	const uint8_t transparentIndex = 0x00;
	const uint8_t terminator = 0x00;

	stream->PutC(introducer);
	stream->PutC(label);
	stream->PutC(blockSize);
	stream->PutC(packed);
	const_cast<AnimatedGifWriter*>(this)->writeLE16(delayCentiseconds);
	stream->PutC(transparentIndex);
	stream->PutC(terminator);
}

void AnimatedGifWriter::writeImageDescriptor() const
{
	const uint8_t separator = 0x2C;
	const uint8_t packed = 0x00; // no local color table

	stream->PutC(separator);
	const_cast<AnimatedGifWriter*>(this)->writeLE16(0);
	const_cast<AnimatedGifWriter*>(this)->writeLE16(0);
	const_cast<AnimatedGifWriter*>(this)->writeLE16(canvasWidth);
	const_cast<AnimatedGifWriter*>(this)->writeLE16(canvasHeight);
	stream->PutC(packed);
}

void AnimatedGifWriter::writeTrailer()
{
	if(!stream || !stream->IsOk())
		return;

	const uint8_t trailer = 0x3B;
	stream->PutC(trailer);
}

void AnimatedGifWriter::writeLE16(uint16_t value)
{
	uint8_t bytes[2];
	bytes[0] = static_cast<uint8_t>(value & 0xFF);
	bytes[1] = static_cast<uint8_t>((value >> 8) & 0xFF);
	writeData(bytes, sizeof(bytes));
}

void AnimatedGifWriter::buildPalette()
{
	for(int r = 0; r < 8; ++r) {
		for(int g = 0; g < 8; ++g) {
			for(int b = 0; b < 4; ++b) {
				const uint8_t index = static_cast<uint8_t>((r << 5) | (g << 2) | b);
				const size_t paletteIndex = static_cast<size_t>(index) * 3;
				palette[paletteIndex + 0] = ChannelValue(r, 7);
				palette[paletteIndex + 1] = ChannelValue(g, 7);
				palette[paletteIndex + 2] = ChannelValue(b, 3);
			}
		}
	}
}

void AnimatedGifWriter::convertToPaletteIndices(const uint8_t* rgbData, std::vector<uint8_t>& indices) const
{
	const size_t pixelCount = static_cast<size_t>(canvasWidth) * canvasHeight;
	if(indices.size() != pixelCount) {
		indices.resize(pixelCount);
	}

	for(size_t i = 0, j = 0; i < pixelCount; ++i, j += 3) {
		const uint8_t r = rgbData[j + 0];
		const uint8_t g = rgbData[j + 1];
		const uint8_t b = rgbData[j + 2];
		indices[i] = Quantize332(r, g, b);
	}
}

bool AnimatedGifWriter::encodeLzw(const std::vector<uint8_t>& indices, std::vector<uint8_t>& outputBlocks) const
{
	if(indices.empty())
		return false;

	std::unordered_map<uint32_t, uint16_t> dictionary;
	dictionary.reserve(5021);

	const int clearCode = 1 << kLzwColorDepth;
	const int endCode = clearCode + 1;
	int nextCode = endCode + 1;
	int codeSize = kLzwColorDepth + 1;
	int maxCode = 1 << codeSize;

	auto resetDictionary = [&]() {
		dictionary.clear();
		nextCode = endCode + 1;
		codeSize = kLzwColorDepth + 1;
		maxCode = 1 << codeSize;
	};

	BitWriter writer;
	writer.Write(static_cast<uint16_t>(clearCode), codeSize);

	uint16_t prefix = indices[0];
	for(size_t i = 1; i < indices.size(); ++i) {
		const uint8_t k = indices[i];
		const uint32_t key = (static_cast<uint32_t>(prefix) << 8) | k;

		const auto iter = dictionary.find(key);
		if(iter != dictionary.end()) {
			prefix = iter->second;
			continue;
		}

		writer.Write(prefix, codeSize);

		if(nextCode < kGifLzwLimit) {
			dictionary[key] = static_cast<uint16_t>(nextCode++);
			if(nextCode == maxCode && codeSize < 12) {
				++codeSize;
				maxCode <<= 1;
			} else if(nextCode == kGifLzwLimit) {
				writer.Write(static_cast<uint16_t>(clearCode), codeSize);
				resetDictionary();
			}
		} else {
			writer.Write(static_cast<uint16_t>(clearCode), codeSize);
			resetDictionary();
		}

		prefix = k;
	}

	writer.Write(prefix, codeSize);
	writer.Write(static_cast<uint16_t>(endCode), codeSize);
	writer.Flush();

	const std::vector<uint8_t>& raw = writer.GetData();
	outputBlocks.clear();
	outputBlocks.reserve(raw.size() + (raw.size() / 255) + 2);

	size_t offset = 0;
	while(offset < raw.size()) {
		const size_t chunk = std::min<size_t>(255, raw.size() - offset);
		outputBlocks.push_back(static_cast<uint8_t>(chunk));
		outputBlocks.insert(outputBlocks.end(), raw.begin() + offset, raw.begin() + offset + chunk);
		offset += chunk;
	}

	outputBlocks.push_back(0x00); // block terminator
	return true;
}
