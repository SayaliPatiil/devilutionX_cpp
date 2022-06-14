/**
 * @file gendung.cpp
 *
 * Implementation of general dungeon generation code.
 */
#include <stack>

#include "gendung.h"

#include "drlg_l1.h"
#include "drlg_l2.h"
#include "drlg_l3.h"
#include "drlg_l4.h"
#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "init.h"
#include "lighting.h"
#include "options.h"
#include "town.h"

namespace devilution {

uint8_t dungeon[DMAXX][DMAXY];
uint8_t pdungeon[DMAXX][DMAXY];
bool Protected[DMAXX][DMAXY];
Rectangle SetPieceRoom;
Rectangle SetPiece;
std::unique_ptr<uint16_t[]> pSetPiece;
bool setloadflag;
std::optional<OwnedCelSprite> pSpecialCels;
std::unique_ptr<MegaTile[]> pMegaTiles;
std::unique_ptr<uint16_t[]> pLevelPieces;
std::unique_ptr<byte[]> pDungeonCels;
std::array<uint8_t, MAXTILES + 1> block_lvid;
std::array<bool, MAXTILES + 1> nBlockTable;
std::array<bool, MAXTILES + 1> nSolidTable;
std::array<bool, MAXTILES + 1> nTransTable;
std::array<bool, MAXTILES + 1> nMissileTable;
std::array<bool, MAXTILES + 1> nTrapTable;
Point dminPosition;
Point dmaxPosition;
dungeon_type leveltype;
uint8_t currlevel;
bool setlevel;
_setlevels setlvlnum;
dungeon_type setlvltype;
Point ViewPosition;
ScrollStruct ScrollInfo;
int MicroTileLen;
char TransVal;
bool TransList[256];
int dPiece[MAXDUNX][MAXDUNY];
MICROS dpiece_defs_map_2[MAXDUNX][MAXDUNY];
int8_t dTransVal[MAXDUNX][MAXDUNY];
char dLight[MAXDUNX][MAXDUNY];
char dPreLight[MAXDUNX][MAXDUNY];
DungeonFlag dFlags[MAXDUNX][MAXDUNY];
int8_t dPlayer[MAXDUNX][MAXDUNY];
int16_t dMonster[MAXDUNX][MAXDUNY];
int8_t dCorpse[MAXDUNX][MAXDUNY];
int8_t dObject[MAXDUNX][MAXDUNY];
char dSpecial[MAXDUNX][MAXDUNY];
int themeCount;
THEME_LOC themeLoc[MAXTHEMES];

namespace {

std::unique_ptr<uint8_t[]> LoadLevelSOLData(size_t &tileCount)
{
	switch (leveltype) {
	case DTYPE_TOWN:
		if (gbIsHellfire)
			return LoadFileInMem<uint8_t>("NLevels\\TownData\\Town.SOL", &tileCount);
		return LoadFileInMem<uint8_t>("Levels\\TownData\\Town.SOL", &tileCount);
	case DTYPE_CATHEDRAL:
		return LoadFileInMem<uint8_t>("Levels\\L1Data\\L1.SOL", &tileCount);
	case DTYPE_CATACOMBS:
		return LoadFileInMem<uint8_t>("Levels\\L2Data\\L2.SOL", &tileCount);
	case DTYPE_CAVES:
		return LoadFileInMem<uint8_t>("Levels\\L3Data\\L3.SOL", &tileCount);
	case DTYPE_HELL:
		return LoadFileInMem<uint8_t>("Levels\\L4Data\\L4.SOL", &tileCount);
	case DTYPE_NEST:
		return LoadFileInMem<uint8_t>("NLevels\\L6Data\\L6.SOL", &tileCount);
	case DTYPE_CRYPT:
		return LoadFileInMem<uint8_t>("NLevels\\L5Data\\L5.SOL", &tileCount);
	default:
		app_fatal("FillSolidBlockTbls");
	}
}

bool WillThemeRoomFit(int floor, int x, int y, int minSize, int maxSize, int *width, int *height)
{
	bool yFlag = true;
	bool xFlag = true;
	int xCount = 0;
	int yCount = 0;

	if (x + maxSize > DMAXX && y + maxSize > DMAXY) {
		return false; // Original broken bounds check, avoids lower right corner
	}
	if (x + minSize > DMAXX || y + minSize > DMAXY) {
		return false; // Skip definit OOB cases
	}
	if (!SkipThemeRoom(x, y)) {
		return false;
	}

	int xArray[20] = {};
	int yArray[20] = {};

	for (int ii = 0; ii < maxSize; ii++) {
		if (xFlag && y + ii < DMAXY) {
			for (int xx = x; xx < x + maxSize && xx < DMAXX; xx++) {
				if (dungeon[xx][y + ii] != floor) {
					if (xx >= minSize) {
						break;
					}
					xFlag = false;
				} else {
					xCount++;
				}
			}
			if (xFlag) {
				xArray[ii] = xCount;
				xCount = 0;
			}
		}
		if (yFlag && x + ii < DMAXX) {
			for (int yy = y; yy < y + maxSize && yy < DMAXY; yy++) {
				if (dungeon[x + ii][yy] != floor) {
					if (yy >= minSize) {
						break;
					}
					yFlag = false;
				} else {
					yCount++;
				}
			}
			if (yFlag) {
				yArray[ii] = yCount;
				yCount = 0;
			}
		}
	}

	for (int ii = 0; ii < minSize; ii++) {
		if (xArray[ii] < minSize || yArray[ii] < minSize) {
			return false;
		}
	}

	int xSmallest = xArray[0];
	int ySmallest = yArray[0];

	for (int ii = 0; ii < maxSize; ii++) {
		if (xArray[ii] < minSize || yArray[ii] < minSize) {
			break;
		}
		if (xArray[ii] < xSmallest) {
			xSmallest = xArray[ii];
		}
		if (yArray[ii] < ySmallest) {
			ySmallest = yArray[ii];
		}
	}

	*width = xSmallest - 2;
	*height = ySmallest - 2;
	return true;
}

void CreateThemeRoom(int themeIndex)
{
	const int lx = themeLoc[themeIndex].room.position.x;
	const int ly = themeLoc[themeIndex].room.position.y;
	const int hx = lx + themeLoc[themeIndex].room.size.width;
	const int hy = ly + themeLoc[themeIndex].room.size.height;

	for (int yy = ly; yy < hy; yy++) {
		for (int xx = lx; xx < hx; xx++) {
			if (leveltype == DTYPE_CATACOMBS) {
				if (yy == ly || yy == hy - 1) {
					dungeon[xx][yy] = 2;
				} else if (xx == lx || xx == hx - 1) {
					dungeon[xx][yy] = 1;
				} else {
					dungeon[xx][yy] = 3;
				}
			}
			if (IsAnyOf(leveltype, DTYPE_CAVES, DTYPE_NEST)) {
				if (yy == ly || yy == hy - 1) {
					dungeon[xx][yy] = 134;
				} else if (xx == lx || xx == hx - 1) {
					dungeon[xx][yy] = 137;
				} else {
					dungeon[xx][yy] = 7;
				}
			}
			if (leveltype == DTYPE_HELL) {
				if (yy == ly || yy == hy - 1) {
					dungeon[xx][yy] = 2;
				} else if (xx == lx || xx == hx - 1) {
					dungeon[xx][yy] = 1;
				} else {
					dungeon[xx][yy] = 6;
				}
			}
		}
	}

	if (leveltype == DTYPE_CATACOMBS) {
		dungeon[lx][ly] = 8;
		dungeon[hx - 1][ly] = 7;
		dungeon[lx][hy - 1] = 9;
		dungeon[hx - 1][hy - 1] = 6;
	}
	if (IsAnyOf(leveltype, DTYPE_CAVES, DTYPE_NEST)) {
		dungeon[lx][ly] = 150;
		dungeon[hx - 1][ly] = 151;
		dungeon[lx][hy - 1] = 152;
		dungeon[hx - 1][hy - 1] = 138;
	}
	if (leveltype == DTYPE_HELL) {
		dungeon[lx][ly] = 9;
		dungeon[hx - 1][ly] = 16;
		dungeon[lx][hy - 1] = 15;
		dungeon[hx - 1][hy - 1] = 12;
	}

	if (leveltype == DTYPE_CATACOMBS) {
		switch (GenerateRnd(2)) {
		case 0:
			dungeon[hx - 1][(ly + hy) / 2] = 4;
			break;
		case 1:
			dungeon[(lx + hx) / 2][hy - 1] = 5;
			break;
		}
	}
	if (IsAnyOf(leveltype, DTYPE_CAVES, DTYPE_NEST)) {
		switch (GenerateRnd(2)) {
		case 0:
			dungeon[hx - 1][(ly + hy) / 2] = 147;
			break;
		case 1:
			dungeon[(lx + hx) / 2][hy - 1] = 146;
			break;
		}
	}
	if (leveltype == DTYPE_HELL) {
		switch (GenerateRnd(2)) {
		case 0: {
			int yy = (ly + hy) / 2;
			dungeon[hx - 1][yy - 1] = 53;
			dungeon[hx - 1][yy] = 6;
			dungeon[hx - 1][yy + 1] = 52;
			dungeon[hx - 2][yy - 1] = 54;
		} break;
		case 1: {
			int xx = (lx + hx) / 2;
			dungeon[xx - 1][hy - 1] = 57;
			dungeon[xx][hy - 1] = 6;
			dungeon[xx + 1][hy - 1] = 56;
			dungeon[xx][hy - 2] = 59;
			dungeon[xx - 1][hy - 2] = 58;
		} break;
		}
	}
}

bool IsFloor(Point p, uint8_t floorID)
{
	int i = (p.x - 16) / 2;
	int j = (p.y - 16) / 2;
	if (i < 0 || i >= DMAXX)
		return false;
	if (j < 0 || j >= DMAXY)
		return false;
	return dungeon[i][j] == floorID;
}

void FillTransparencyValues(Point floor, uint8_t floorID)
{
	Direction allDirections[] = {
		Direction::North,
		Direction::South,
		Direction::East,
		Direction::West,
		Direction::NorthEast,
		Direction::NorthWest,
		Direction::SouthEast,
		Direction::SouthWest,
	};

	// We only fill in the surrounding tiles if they are not floor tiles
	// because they would otherwise not be visited by the span filling algorithm
	for (Direction dir : allDirections) {
		Point adjacent = floor + dir;
		if (!IsFloor(adjacent, floorID))
			dTransVal[adjacent.x][adjacent.y] = TransVal;
	}

	dTransVal[floor.x][floor.y] = TransVal;
}

void FindTransparencyValues(Point floor, uint8_t floorID)
{
	// Algorithm adapted from https://en.wikipedia.org/wiki/Flood_fill#Span_Filling
	// Modified to include diagonally adjacent tiles that would otherwise not be visited
	// Also, Wikipedia's selection for the initial seed is incorrect
	using Seed = std::tuple<int, int, int, int>;
	std::stack<Seed> seedStack;
	seedStack.push(std::make_tuple(floor.x, floor.x + 1, floor.y, 1));

	const auto isInside = [&](int x, int y) {
		if (dTransVal[x][y] != 0)
			return false;
		return IsFloor({ x, y }, floorID);
	};

	const auto set = [&](int x, int y) {
		FillTransparencyValues({ x, y }, floorID);
	};

	const Displacement left = { -1, 0 };
	const Displacement right = { 1, 0 };
	const auto checkDiagonals = [&](Point p, Displacement direction) {
		Point up = p + Displacement { 0, -1 };
		Point upOver = up + direction;
		if (!isInside(up.x, up.y) && isInside(upOver.x, upOver.y))
			seedStack.push(std::make_tuple(upOver.x, upOver.x + 1, upOver.y, -1));

		Point down = p + Displacement { 0, 1 };
		Point downOver = down + direction;
		if (!isInside(down.x, down.y) && isInside(downOver.x, downOver.y))
			seedStack.push(std::make_tuple(downOver.x, downOver.x + 1, downOver.y, 1));
	};

	while (!seedStack.empty()) {
		int scanStart, scanEnd, y, dy;
		std::tie(scanStart, scanEnd, y, dy) = seedStack.top();
		seedStack.pop();

		int scanLeft = scanStart;
		if (isInside(scanLeft, y)) {
			while (isInside(scanLeft - 1, y)) {
				set(scanLeft - 1, y);
				scanLeft--;
			}
			checkDiagonals({ scanLeft, y }, left);
		}
		if (scanLeft < scanStart)
			seedStack.push(std::make_tuple(scanLeft, scanStart - 1, y - dy, -dy));

		int scanRight = scanStart;
		while (scanRight < scanEnd) {
			while (isInside(scanRight, y)) {
				set(scanRight, y);
				scanRight++;
			}
			seedStack.push(std::make_tuple(scanLeft, scanRight - 1, y + dy, dy));
			if (scanRight - 1 > scanEnd)
				seedStack.push(std::make_tuple(scanEnd + 1, scanRight - 1, y - dy, -dy));
			if (scanLeft < scanRight)
				checkDiagonals({ scanRight - 1, y }, right);

			while (scanRight < scanEnd && !isInside(scanRight, y))
				scanRight++;
			scanLeft = scanRight;
			if (scanLeft < scanEnd)
				checkDiagonals({ scanLeft, y }, left);
		}
	}
}

} // namespace

dungeon_type GetLevelType(int level)
{
	if (level == 0)
		return DTYPE_TOWN;
	if (level <= 4)
		return DTYPE_CATHEDRAL;
	if (level <= 8)
		return DTYPE_CATACOMBS;
	if (level <= 12)
		return DTYPE_CAVES;
	if (level <= 16)
		return DTYPE_HELL;
	if (level <= 20)
		return DTYPE_NEST;
	if (level <= 24)
		return DTYPE_CRYPT;

	return DTYPE_NONE;
}

void CreateDungeon(uint32_t rseed, lvl_entry entry)
{
	switch (leveltype) {
	case DTYPE_TOWN:
		CreateTown(entry);
		break;
	case DTYPE_CATHEDRAL:
	case DTYPE_CRYPT:
		CreateL5Dungeon(rseed, entry);
		break;
	case DTYPE_CATACOMBS:
		CreateL2Dungeon(rseed, entry);
		break;
	case DTYPE_CAVES:
	case DTYPE_NEST:
		CreateL3Dungeon(rseed, entry);
		break;
	case DTYPE_HELL:
		CreateL4Dungeon(rseed, entry);
		break;
	default:
		app_fatal("Invalid level type");
	}
}

void FillSolidBlockTbls()
{
	size_t tileCount;
	auto pSBFile = LoadLevelSOLData(tileCount);

	for (unsigned i = 0; i < tileCount; i++) {
		uint8_t bv = pSBFile[i];
		nSolidTable[i + 1] = (bv & 0x01) != 0;
		nBlockTable[i + 1] = (bv & 0x02) != 0;
		nMissileTable[i + 1] = (bv & 0x04) != 0;
		nTransTable[i + 1] = (bv & 0x08) != 0;
		nTrapTable[i + 1] = (bv & 0x80) != 0;
		block_lvid[i + 1] = (bv & 0x30) >> 4;
	}
}

void SetDungeonMicros()
{
	MicroTileLen = 10;
	int blocks = 10;

	if (leveltype == DTYPE_TOWN) {
		MicroTileLen = 16;
		blocks = 16;
	} else if (leveltype == DTYPE_HELL) {
		MicroTileLen = 12;
		blocks = 16;
	}

	for (int y = 0; y < MAXDUNY; y++) {
		for (int x = 0; x < MAXDUNX; x++) {
			int lv = dPiece[x][y];
			MICROS &micros = dpiece_defs_map_2[x][y];
			if (lv != 0) {
				lv--;
				uint16_t *pieces = &pLevelPieces[blocks * lv];
				for (int i = 0; i < blocks; i++)
					micros.mt[i] = SDL_SwapLE16(pieces[blocks - 2 + (i & 1) - (i & 0xE)]);
			} else {
				for (int i = 0; i < blocks; i++)
					micros.mt[i] = 0;
			}
		}
	}
}

void DRLG_InitTrans()
{
	memset(dTransVal, 0, sizeof(dTransVal));
	memset(TransList, 0, sizeof(TransList));
	TransVal = 1;
}

void DRLG_RectTrans(Rectangle area)
{
	Point position = area.position;
	Size size = area.size;

	for (int j = position.y; j <= position.y + size.height; j++) {
		for (int i = position.x; i <= position.x + size.width; i++) {
			dTransVal[i][j] = TransVal;
		}
	}

	TransVal++;
}

void DRLG_MRectTrans(Rectangle area)
{
	Point position = area.position.megaToWorld();
	Size size = area.size * 2;

	DRLG_RectTrans({ position + Displacement { 1, 1 }, { size.width - 1, size.height - 1 } });
}

void DRLG_CopyTrans(int sx, int sy, int dx, int dy)
{
	dTransVal[dx][dy] = dTransVal[sx][sy];
}

void DRLG_InitSetPC()
{
	SetPieceRoom = { { -1, -1 }, { -1, -1 } };
	SetPiece = { { 0, 0 }, { 0, 0 } };
}

void DRLG_SetPC()
{
	Make_SetPC(SetPiece);
}

void Make_SetPC(Rectangle area)
{
	Point position = area.position.megaToWorld();
	Size size = area.size * 2;

	for (int j = 0; j < size.height; j++) {
		for (int i = 0; i < size.width; i++) {
			dFlags[position.x + i][position.y + j] |= DungeonFlag::Populated;
		}
	}
}

std::optional<Point> PlaceMiniSet(const Miniset &miniset, int tries, bool drlg1Quirk)
{
	int sw = miniset.size.width;
	int sh = miniset.size.height;
	Point position { GenerateRnd(DMAXX - sw), GenerateRnd(DMAXY - sh) };

	for (int i = 0; i < tries; i++) {
		if (position.x == DMAXX - sw) {
			position.x = 0;
			position.y++;
			if (position.y == DMAXY - sh) {
				position.y = 0;
			}
		}

		// Limit the position of SetPieces for compatibility with Diablo bug
		if (drlg1Quirk) {
			bool valid = true;
			if (position.x <= 12) {
				position.x++;
				valid = false;
			}
			if (position.y <= 12) {
				position.y++;
				valid = false;
			}
			if (!valid) {
				continue;
			}
		}

		if (SetPieceRoom.Contains(position))
			continue;
		if (miniset.matches(position)) {
			miniset.place(position);
			return position;
		}

		position.x++;
	}

	return {};
}

void PlaceDunTiles(const uint16_t *dunData, Point position, int floorId)
{
	int width = SDL_SwapLE16(dunData[0]);
	int height = SDL_SwapLE16(dunData[1]);

	const uint16_t *tileLayer = &dunData[2];

	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			auto tileId = static_cast<uint8_t>(SDL_SwapLE16(tileLayer[j * width + i]));
			if (tileId != 0) {
				dungeon[position.x + i][position.y + j] = tileId;
				Protected[position.x + i][position.y + j] = true;
			} else if (floorId != 0) {
				dungeon[position.x + i][position.y + j] = floorId;
			}
		}
	}
}

void DRLG_PlaceThemeRooms(int minSize, int maxSize, int floor, int freq, bool rndSize)
{
	themeCount = 0;
	memset(themeLoc, 0, sizeof(*themeLoc));
	for (int j = 0; j < DMAXY; j++) {
		for (int i = 0; i < DMAXX; i++) {
			int themeW = 0;
			int themeH = 0;
			if (dungeon[i][j] == floor && GenerateRnd(freq) == 0 && WillThemeRoomFit(floor, i, j, minSize, maxSize, &themeW, &themeH)) {
				if (rndSize) {
					int min = minSize - 2;
					int max = maxSize - 2;
					themeW = min + GenerateRnd(GenerateRnd(themeW - min + 1));
					if (themeW < min || themeW > max)
						themeW = min;
					themeH = min + GenerateRnd(GenerateRnd(themeH - min + 1));
					if (themeH < min || themeH > max)
						themeH = min;
				}
				themeLoc[themeCount].room = { { i + 1, j + 1 }, { themeW, themeH } };
				if (IsAnyOf(leveltype, DTYPE_CAVES, DTYPE_NEST)) {
					DRLG_RectTrans({ Point(i, j).megaToWorld() + Displacement { 4, 4 }, { themeW * 2 - 5, themeH * 2 - 5 } });
				} else {
					DRLG_MRectTrans({ themeLoc[themeCount].room.position, { themeW - 1, themeH - 1 } });
				}
				themeLoc[themeCount].ttval = TransVal - 1;
				CreateThemeRoom(themeCount);
				themeCount++;
			}
		}
	}
} // namespace

void DRLG_HoldThemeRooms()
{
	for (int i = 0; i < themeCount; i++) {
		for (int y = themeLoc[i].room.position.y; y < themeLoc[i].room.position.y + themeLoc[i].room.size.height - 1; y++) {
			for (int x = themeLoc[i].room.position.x; x < themeLoc[i].room.position.x + themeLoc[i].room.size.width - 1; x++) {
				int xx = 2 * x + 16;
				int yy = 2 * y + 16;
				dFlags[xx][yy] |= DungeonFlag::Populated;
				dFlags[xx + 1][yy] |= DungeonFlag::Populated;
				dFlags[xx][yy + 1] |= DungeonFlag::Populated;
				dFlags[xx + 1][yy + 1] |= DungeonFlag::Populated;
			}
		}
	}
}

void DRLG_LPass3(int lv)
{
	{
		MegaTile mega = pMegaTiles[lv];
		int v1 = SDL_SwapLE16(mega.micro1) + 1;
		int v2 = SDL_SwapLE16(mega.micro2) + 1;
		int v3 = SDL_SwapLE16(mega.micro3) + 1;
		int v4 = SDL_SwapLE16(mega.micro4) + 1;

		for (int j = 0; j < MAXDUNY; j += 2) {
			for (int i = 0; i < MAXDUNX; i += 2) {
				dPiece[i + 0][j + 0] = v1;
				dPiece[i + 1][j + 0] = v2;
				dPiece[i + 0][j + 1] = v3;
				dPiece[i + 1][j + 1] = v4;
			}
		}
	}

	int yy = 16;
	for (int j = 0; j < DMAXY; j++) {
		int xx = 16;
		for (int i = 0; i < DMAXX; i++) { // NOLINT(modernize-loop-convert)
			int v1 = 0;
			int v2 = 0;
			int v3 = 0;
			int v4 = 0;

			int tileId = dungeon[i][j] - 1;
			if (tileId >= 0) {
				MegaTile mega = pMegaTiles[tileId];
				v1 = SDL_SwapLE16(mega.micro1) + 1;
				v2 = SDL_SwapLE16(mega.micro2) + 1;
				v3 = SDL_SwapLE16(mega.micro3) + 1;
				v4 = SDL_SwapLE16(mega.micro4) + 1;
			}
			dPiece[xx + 0][yy + 0] = v1;
			dPiece[xx + 1][yy + 0] = v2;
			dPiece[xx + 0][yy + 1] = v3;
			dPiece[xx + 1][yy + 1] = v4;
			xx += 2;
		}
		yy += 2;
	}
}

void DRLG_Init_Globals()
{
	memset(dFlags, 0, sizeof(dFlags));
	memset(dPlayer, 0, sizeof(dPlayer));
	memset(dMonster, 0, sizeof(dMonster));
	memset(dCorpse, 0, sizeof(dCorpse));
	memset(dObject, 0, sizeof(dObject));
	memset(dSpecial, 0, sizeof(dSpecial));
	int8_t c = DisableLighting ? 0 : 15;
	memset(dLight, c, sizeof(dLight));
}

bool SkipThemeRoom(int x, int y)
{
	for (int i = 0; i < themeCount; i++) {
		if (x >= themeLoc[i].room.position.x - 2 && x <= themeLoc[i].room.position.x + themeLoc[i].room.size.width + 2
		    && y >= themeLoc[i].room.position.y - 2 && y <= themeLoc[i].room.position.y + themeLoc[i].room.size.height + 2)
			return false;
	}

	return true;
}

void InitLevels()
{
	currlevel = 0;
	leveltype = DTYPE_TOWN;
	setlevel = false;
}

void FloodTransparencyValues(uint8_t floorID)
{
	int yy = 16;
	for (int j = 0; j < DMAXY; j++) {
		int xx = 16;
		for (int i = 0; i < DMAXX; i++) {
			if (dungeon[i][j] == floorID && dTransVal[xx][yy] == 0) {
				FindTransparencyValues({ xx, yy }, floorID);
				TransVal++;
			}
			xx += 2;
		}
		yy += 2;
	}
}

} // namespace devilution
