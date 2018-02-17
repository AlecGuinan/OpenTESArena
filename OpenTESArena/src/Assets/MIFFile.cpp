#include <algorithm>
#include <unordered_map>

#include "Compression.h"
#include "MIFFile.h"
#include "../Utilities/Bytes.h"
#include "../Utilities/Debug.h"

#include "components/vfs/manager.hpp"

namespace
{
	const std::string Tag_FLAT = "FLAT";
	const std::string Tag_FLOR = "FLOR";
	const std::string Tag_INFO = "INFO";
	const std::string Tag_INNS = "INNS";
	const std::string Tag_LEVL = "LEVL";
	const std::string Tag_LOCK = "LOCK";
	const std::string Tag_LOOT = "LOOT";
	const std::string Tag_MAP1 = "MAP1";
	const std::string Tag_MAP2 = "MAP2";
	const std::string Tag_NAME = "NAME";
	const std::string Tag_NUMF = "NUMF";
	const std::string Tag_TARG = "TARG";
	const std::string Tag_TRIG = "TRIG";

	// Mappings of .MIF level tags to functions for decoding data, excluding "LEVL".
	const std::unordered_map<std::string, int(*)(MIFFile::Level&, const uint8_t*)> MIFLevelTags =
	{
		{ Tag_FLAT, MIFFile::Level::loadFLAT },
		{ Tag_FLOR, MIFFile::Level::loadFLOR },
		{ Tag_INFO, MIFFile::Level::loadINFO },
		{ Tag_INNS, MIFFile::Level::loadINNS },
		{ Tag_LOCK, MIFFile::Level::loadLOCK },
		{ Tag_LOOT, MIFFile::Level::loadLOOT },
		{ Tag_MAP1, MIFFile::Level::loadMAP1 },
		{ Tag_MAP2, MIFFile::Level::loadMAP2 },
		{ Tag_NAME, MIFFile::Level::loadNAME },
		{ Tag_NUMF, MIFFile::Level::loadNUMF },
		{ Tag_TARG, MIFFile::Level::loadTARG },
		{ Tag_TRIG, MIFFile::Level::loadTRIG }
	};
}

MIFFile::Level::Level()
{
	this->numf = 0;
}

const uint8_t MIFFile::DRY_CHASM = 0xC;
const uint8_t MIFFile::WET_CHASM = 0xD;
const uint8_t MIFFile::LAVA_CHASM = 0xE;
const double MIFFile::ARENA_UNITS = 128.0;

MIFFile::MIFFile(const std::string &filename)
{
	VFS::IStreamPtr stream = VFS::Manager::get().open(filename);
	DebugAssert(stream != nullptr, "Could not open \"" + filename + "\".");

	stream->seekg(0, std::ios::end);
	std::vector<uint8_t> srcData(stream->tellg());
	stream->seekg(0, std::ios::beg);
	stream->read(reinterpret_cast<char*>(srcData.data()), srcData.size());

	// Get data from the header (after "MHDR"). Constant for all levels. The header 
	// size should be 61.
	const uint8_t headerSize = *(srcData.data() + 4);
	const uint8_t unknown1 = *(srcData.data() + 6);
	const uint8_t entryCount = *(srcData.data() + 7);
	const uint16_t mapWidth = Bytes::getLE16(srcData.data() + 27);
	const uint16_t mapDepth = Bytes::getLE16(srcData.data() + 29);
	
	// Load start locations from the header. Not all are set (i.e., some are (0, 0)).
	for (size_t i = 0; i < this->startPoints.size(); i++)
	{
		const uint16_t mifX = Bytes::getLE16(srcData.data() + 8 + (i * 2));
		const uint16_t mifY = Bytes::getLE16(srcData.data() + 16 + (i * 2));

		// Convert the coordinates from .MIF format to voxel format. The remainder
		// of the division is used for positioning within the voxel.
		const double x = static_cast<double>(mifX) / MIFFile::ARENA_UNITS;
		const double y = static_cast<double>(mifY) / MIFFile::ARENA_UNITS;

		this->startPoints.at(i) = Double2(x, y);
	}

	// Get starting level index. The level count (a single byte) comes right after this, 
	// but it is unused in this reader (it's inferred by the level loading loop).
	this->startingLevelIndex = *(srcData.data() + 24);

	// Start of the level data (at each "LEVL"). Some .MIF files have multiple levels,
	// so this needs to be in a loop.
	int levelOffset = headerSize + 6;
	while (levelOffset < srcData.size())
	{
		MIFFile::Level level;

		// Begin loading the level data at the current LEVL, and get the offset
		// to the next LEVL.
		const uint8_t *levelStart = srcData.data() + levelOffset;
		levelOffset += level.load(levelStart);

		// Add to list of levels.
		this->levels.push_back(std::move(level));
	}

	this->width = mapWidth;
	this->depth = mapDepth;
	this->name = filename;
}

MIFFile::~MIFFile()
{

}

std::string MIFFile::mainQuestDungeonFilename(int dungeonX, int dungeonY, int provinceID)
{
	uint32_t mifID = (dungeonY << 16) + dungeonX + provinceID;
	mifID = static_cast<uint32_t>(-static_cast<int32_t>(Bytes::rol32(mifID, 5)));
	const std::string mifName = std::to_string(mifID) + ".MIF";
	return mifName;
}

int MIFFile::getWidth() const
{
	return this->width;
}

int MIFFile::getHeight(int levelIndex) const
{
	return this->levels.at(levelIndex).getHeight();
}

int MIFFile::getDepth() const
{
	return this->depth;
}

int MIFFile::getStartingLevelIndex() const
{
	return this->startingLevelIndex;
}

const std::string &MIFFile::getName() const
{
	return this->name;
}

const std::array<Double2, 4> &MIFFile::getStartPoints() const
{
	return this->startPoints;
}

const std::vector<MIFFile::Level> &MIFFile::getLevels() const
{
	return this->levels;
}

int MIFFile::Level::load(const uint8_t *levelStart)
{
	// Get the size of the level data.
	const uint16_t levelSize = Bytes::getLE16(levelStart + 4);

	// Move the tag pointer while there are tags to read in the current level.
	const uint8_t *tagStart = levelStart + 6;
	const uint8_t *levelEnd = tagStart + levelSize;
	while (tagStart < levelEnd)
	{
		// Check what the four letter tag is (FLOR, MAP1, etc., never LEVL).
		const std::string tag = std::string(tagStart, tagStart + 4);

		// Find the function associated with the tag.
		const auto tagIter = MIFLevelTags.find(tag);
		if (tagIter != MIFLevelTags.end())
		{
			// Load the data and move the offset to the beginning of the next tag.
			auto *loadingFn = tagIter->second;
			tagStart += loadingFn(*this, tagStart);
		}
		else
		{
			// This can only be reached if the .MIF file is corrupted.
			DebugCrash("Unrecognized .MIF tag \"" + tag + "\".");
		}
	}

	return static_cast<int>(levelEnd - levelStart);
}

int MIFFile::Level::getHeight() const
{
	// If there is MAP2 data, then check through each voxel to find the highest point.
	if (this->map2.size() > 0)
	{
		// To do: look at MAP2 voxels and determine highest column.
		return 6;
	}
	else
	{
		// Use the default height -- ground, main floor, and ceiling.
		return 3;
	}
}

int MIFFile::Level::loadFLAT(MIFFile::Level &level, const uint8_t *tagStart)
{
	const uint16_t size = Bytes::getLE16(tagStart + 4);
	const uint8_t *tagDataStart = tagStart + 6;

	// Currently unknown.
	level.flat = std::vector<uint8_t>(size);
	std::copy(tagDataStart, tagDataStart + level.flat.size(), level.flat.begin());

	return size + 6;
}

int MIFFile::Level::loadFLOR(MIFFile::Level &level, const uint8_t *tagStart)
{
	const uint16_t compressedSize = Bytes::getLE16(tagStart + 4);
	const uint16_t uncompressedSize = Bytes::getLE16(tagStart + 6);

	// Allocate space for this floor, using 2 bytes per voxel.
	level.flor = std::vector<uint8_t>(uncompressedSize);
	std::fill(level.flor.begin(), level.flor.end(), 0);

	// Decode the data with type 8 decompression.
	const uint8_t *tagDataStart = tagStart + 8;
	Compression::decodeType08(tagDataStart, tagDataStart + compressedSize, level.flor);

	return compressedSize + 6;
}

int MIFFile::Level::loadINFO(MIFFile::Level &level, const uint8_t *tagStart)
{
	const uint16_t size = Bytes::getLE16(tagStart + 4);
	const uint8_t *tagDataStart = tagStart + 6;

	// Using the size might include some unnecessary empty space, so assume it's 
	// null-terminated instead.
	level.info = std::string(reinterpret_cast<const char*>(tagDataStart));

	return size + 6;
}

int MIFFile::Level::loadINNS(MIFFile::Level &level, const uint8_t *tagStart)
{
	const uint16_t size = Bytes::getLE16(tagStart + 4);
	const uint8_t *tagDataStart = tagStart + 6;

	// Currently unknown.
	level.inns = std::vector<uint8_t>(size);
	std::copy(tagDataStart, tagDataStart + level.inns.size(), level.inns.begin());

	return size + 6;
}

int MIFFile::Level::loadLOCK(MIFFile::Level &level, const uint8_t *tagStart)
{
	const uint16_t size = Bytes::getLE16(tagStart + 4);

	// Each lock record is 3 bytes.
	const int lockCount = size / 3;
	level.lock = std::vector<MIFFile::Level::Lock>(lockCount);

	const uint8_t *tagDataStart = tagStart + 6;
	for (int i = 0; i < lockCount; i++)
	{
		const int offset = i * 3;
		const uint8_t x = *(tagDataStart + offset);
		const uint8_t y = *(tagDataStart + offset + 1);
		const uint8_t lockLevel = *(tagDataStart + offset + 2);

		MIFFile::Level::Lock &lock = level.lock.at(i);
		lock.x = x;
		lock.y = y;
		lock.lockLevel = lockLevel;
	}

	return size + 6;
}

int MIFFile::Level::loadLOOT(MIFFile::Level &level, const uint8_t *tagStart)
{
	const uint16_t size = Bytes::getLE16(tagStart + 4);
	const uint8_t *tagDataStart = tagStart + 6;

	// Currently unknown.
	level.loot = std::vector<uint8_t>(size);
	std::copy(tagDataStart, tagDataStart + level.loot.size(), level.loot.begin());

	return size + 6;
}

int MIFFile::Level::loadMAP1(MIFFile::Level &level, const uint8_t *tagStart)
{
	const uint16_t compressedSize = Bytes::getLE16(tagStart + 4);
	const uint16_t uncompressedSize = Bytes::getLE16(tagStart + 6);

	// Allocate space for this floor, using 2 bytes per voxel.
	level.map1 = std::vector<uint8_t>(uncompressedSize);
	std::fill(level.map1.begin(), level.map1.end(), 0);

	// Decode the data with type 8 decompression.
	const uint8_t *tagDataStart = tagStart + 8;
	Compression::decodeType08(tagDataStart, tagDataStart + compressedSize, level.map1);

	return compressedSize + 6;
}

int MIFFile::Level::loadMAP2(MIFFile::Level &level, const uint8_t *tagStart)
{
	const uint16_t compressedSize = Bytes::getLE16(tagStart + 4);
	const uint16_t uncompressedSize = Bytes::getLE16(tagStart + 6);

	// Allocate space for this floor, using 2 bytes per voxel.
	level.map2 = std::vector<uint8_t>(uncompressedSize);
	std::fill(level.map2.begin(), level.map2.end(), 0);

	// Decode the data with type 8 decompression.
	const uint8_t *tagDataStart = tagStart + 8;
	Compression::decodeType08(tagDataStart, tagDataStart + compressedSize, level.map2);

	return compressedSize + 6;
}

int MIFFile::Level::loadNAME(MIFFile::Level &level, const uint8_t *tagStart)
{
	const uint16_t size = Bytes::getLE16(tagStart + 4);
	const uint8_t *tagDataStart = tagStart + 6;

	// Using the size might include some unnecessary empty space, so assume it's 
	// null-terminated instead.
	level.name = std::string(reinterpret_cast<const char*>(tagDataStart));

	return size + 6;
}

int MIFFile::Level::loadNUMF(MIFFile::Level &level, const uint8_t *tagStart)
{
	// Size should always be 1.
	const uint16_t size = Bytes::getLE16(tagStart + 4);
	const uint8_t *tagDataStart = tagStart + 6;

	// Just one byte.
	level.numf = *tagDataStart;

	return size + 6;
}

int MIFFile::Level::loadTARG(MIFFile::Level &level, const uint8_t *tagStart)
{
	const uint16_t size = Bytes::getLE16(tagStart + 4);
	const uint8_t *tagDataStart = tagStart + 6;

	// Currently unknown.
	level.targ = std::vector<uint8_t>(size);
	std::copy(tagDataStart, tagDataStart + level.targ.size(), level.targ.begin());

	return size + 6;
}

int MIFFile::Level::loadTRIG(MIFFile::Level &level, const uint8_t *tagStart)
{
	const uint16_t size = Bytes::getLE16(tagStart + 4);

	// Each trigger record is 4 bytes.
	const int triggerCount = size / 4;
	level.trig = std::vector<MIFFile::Level::Trigger>(triggerCount);

	const uint8_t *tagDataStart = tagStart + 6;
	for (int i = 0; i < triggerCount; i++)
	{
		const int offset = i * 4;
		const uint8_t x = *(tagDataStart + offset);
		const uint8_t y = *(tagDataStart + offset + 1);

		// Some text and sound indices are negative (which means they're unused), 
		// so they need to be signed.
		const int8_t textIndex = *(tagDataStart + offset + 2);
		const int8_t soundIndex = *(tagDataStart + offset + 3);

		MIFFile::Level::Trigger &trigger = level.trig.at(i);
		trigger.x = x;
		trigger.y = y;
		trigger.textIndex = textIndex;
		trigger.soundIndex = soundIndex;
	}

	return size + 6;
}
