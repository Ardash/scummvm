/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef DIRECTOR_UTIL_H
#define DIRECTOR_UTIL_H

namespace Common {
class String;
}

namespace Director {

int castNumToNum(const char *str);
char *numToCastNum(int num);

Common::String convertPath(Common::String &path);

Common::String unixToMacPath(const Common::String &path);

Common::String getPath(Common::String path, Common::String cwd);

bool testPath(Common::String &path, bool directory = false);

Common::String pathMakeRelative(Common::String path, bool recursive = true, bool addexts = true, bool directory = false);

bool hasExtension(Common::String filename);

Common::String testExtensions(Common::String component, Common::String initialPath, Common::String convPath);

Common::String getFileName(Common::String path);

Common::String stripMacPath(const char *name);

Common::String convertMacFilename(const char *name);

Common::String dumpScriptName(const char *prefix, int type, int id, const char *ext);

bool isButtonSprite(SpriteType spriteType);

class RandomState {
public:
	uint32 _seed;
	uint32 _mask;
	uint32 _len;

	RandomState() {
		_seed = _mask = _len = 0;
	}

	void setSeed(int seed);
	uint32 getSeed() { return _seed; }
	int32 getRandom(int32 range);

private:
	void init(int len);
	int32 genNextRandom();
	int32 perlin(int32 val);
};

uint32 readVarInt(Common::SeekableReadStream &stream);

Common::SeekableReadStreamEndian *readZlibData(Common::SeekableReadStream &stream, unsigned long len, unsigned long *outLen, bool bigEndian);

uint16 humanVersion(uint16 ver);

Common::Platform platformFromID(uint16 id);

Common::CodePage getEncoding(Common::Platform platform, Common::Language language);
Common::CodePage detectFontEncoding(Common::Platform platform, uint16 fontId);

int charToNum(Common::u32char_type_t ch);
Common::u32char_type_t numToChar(int num);
int compareStrings(const Common::String &s1, const Common::String &s2);

} // End of namespace Director

#endif
