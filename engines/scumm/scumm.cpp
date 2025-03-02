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

#include "common/config-manager.h"
#include "common/debug-channels.h"
#include "common/macresman.h"
#include "common/md5.h"
#include "common/events.h"
#include "common/system.h"
#include "common/translation.h"

#include "engines/util.h"

#include "gui/message.h"

#include "graphics/cursorman.h"

#include "scumm/akos.h"
#include "scumm/charset.h"
#include "scumm/costume.h"
#include "scumm/debugger.h"
#include "scumm/dialogs.h"
#include "scumm/file.h"
#include "scumm/file_nes.h"
#include "scumm/imuse/imuse.h"
#include "scumm/imuse_digi/dimuse.h"
#include "scumm/smush/smush_mixer.h"
#include "scumm/smush/smush_player.h"
#include "scumm/players/player_towns.h"
#include "scumm/insane/insane.h"
#include "scumm/he/animation_he.h"
#include "scumm/he/intern_he.h"
#include "scumm/he/logic_he.h"
#include "scumm/he/sound_he.h"
#include "scumm/object.h"
#include "scumm/players/player_ad.h"
#include "scumm/players/player_nes.h"
#include "scumm/players/player_sid.h"
#include "scumm/players/player_pce.h"
#include "scumm/players/player_apple2.h"
#include "scumm/players/player_v1.h"
#include "scumm/players/player_v2.h"
#include "scumm/players/player_v2cms.h"
#include "scumm/players/player_v2a.h"
#include "scumm/players/player_v3a.h"
#include "scumm/players/player_v3m.h"
#include "scumm/players/player_v4a.h"
#include "scumm/players/player_v5m.h"
#include "scumm/players/player_he.h"
#include "scumm/resource.h"
#include "scumm/he/resource_he.h"
#include "scumm/he/moonbase/moonbase.h"
#include "scumm/scumm_v0.h"
#include "scumm/scumm_v8.h"
#include "scumm/sound.h"
#include "scumm/imuse/sysex.h"
#include "scumm/he/localizer.h"
#include "scumm/he/sprite_he.h"
#include "scumm/he/cup_player_he.h"
#include "scumm/util.h"
#include "scumm/verbs.h"
#include "scumm/imuse/drivers/pcspk.h"
#include "scumm/imuse/drivers/mac_m68k.h"
#include "scumm/imuse/drivers/amiga.h"
#include "scumm/imuse/drivers/fmtowns.h"
#include "scumm/detection_steam.h"

#include "backends/audiocd/audiocd.h"

#include "audio/mixer.h"

using Common::File;

namespace Scumm {

// Use g_scumm from error() ONLY
ScummEngine *g_scumm = 0;


struct dbgChannelDesc {
	const char *channel, *desc;
	uint32 flag;
};


ScummEngine::ScummEngine(OSystem *syst, const DetectorResult &dr)
	: Engine(syst),
	  _game(dr.game),
	  _filenamePattern(dr.fp),
	  _language(dr.language),
	  _currentScript(0xFF), // Let debug() work on init stage
	  _messageDialog(0), _pauseDialog(0), _versionDialog(0),
	  _rnd("scumm")
	  {

	_localizer = nullptr;

#ifdef USE_RGB_COLOR
	if (_game.features & GF_16BIT_COLOR) {
		if (_game.platform == Common::kPlatformPCEngine)
			_gdi = new GdiPCEngine(this);
		else if (_game.heversion > 0)
			_gdi = new GdiHE16bit(this);
	} else
#endif
	if (_game.heversion > 0) {
		_gdi = new GdiHE(this);
	} else if (_game.platform == Common::kPlatformNES) {
		_gdi = new GdiNES(this);
	} else if (_game.version <= 1) {
		_gdi = new GdiV1(this);
	} else if (_game.version == 2) {
		_gdi = new GdiV2(this);
	} else {
		_gdi = new Gdi(this);
	}
	_res = new ResourceManager(this);

	// Convert MD5 checksum back into a digest
	for (int i = 0; i < 16; ++i) {
		char tmpStr[3] = "00";
		uint tmpVal;
		tmpStr[0] = dr.md5[2*i];
		tmpStr[1] = dr.md5[2*i+1];
		int res = sscanf(tmpStr, "%x", &tmpVal);
		assert(res == 1);
		_gameMD5[i] = (byte)tmpVal;
	}

	_fileHandle = 0;

	// Init all vars
	_imuse = NULL;
	_imuseDigital = NULL;
	_musicEngine = NULL;
	_townsPlayer = NULL;
	_verbs = NULL;
	_objs = NULL;
	_sound = NULL;
	memset(&vm, 0, sizeof(vm));
	_pauseDialog = NULL;
	_versionDialog = NULL;
	_fastMode = 0;
	_actors = _sortedActors = NULL;
	_arraySlot = NULL;
	_inventory = NULL;
	_newNames = NULL;
	_scummVars = NULL;
	_roomVars = NULL;
	_varwatch = 0;
	_bitVars = NULL;
	_numVariables = 0;
	_numBitVariables = 0;
	_numRoomVariables = 0;
	_numLocalObjects = 0;
	_numGlobalObjects = 0;
	_numArray = 0;
	_numVerbs = 0;
	_numFlObject = 0;
	_numInventory = 0;
	_numRooms = 0;
	_numScripts = 0;
	_numSounds = 0;
	_numCharsets = 0;
	_numNewNames = 0;
	_numGlobalScripts = 0;
	_numCostumes = 0;
	_numImages = 0;
	_numLocalScripts = 60;
	_numSprites = 0;
	_numTalkies = 0;
	_numPalettes = 0;
	_numUnk = 0;
	_curPalIndex = 0;
	_currentRoom = 0;
	_egoPositioned = false;
	_mouseAndKeyboardStat = 0;
	_leftBtnPressed = 0;
	_rightBtnPressed = 0;
	_lastInputScriptTime = 0;
	_bootParam = 0;
	_dumpScripts = false;
	_debugMode = false;
	_objectOwnerTable = NULL;
	_objectRoomTable = NULL;
	_objectStateTable = NULL;
	_numObjectsInRoom = 0;
	_userPut = 0;
	_userState = 0;
	_resourceHeaderSize = 8;
	_saveLoadFlag = 0;
	_saveLoadSlot = 0;
	_lastSaveTime = 0;
	_saveTemporaryState = false;
	memset(_localScriptOffsets, 0, sizeof(_localScriptOffsets));
	_scriptPointer = NULL;
	_scriptOrgPointer = NULL;
	_opcode = 0;
	vm.numNestedScripts = 0;
	_lastCodePtr = NULL;
	_scummStackPos = 0;
	memset(_vmStack, 0, sizeof(_vmStack));
	_fileOffset = 0;
	memset(_resourceMapper, 0, sizeof(_resourceMapper));
	_lastLoadedRoom = 0;
	_roomResource = 0;
	OF_OWNER_ROOM = 0;
	_verbMouseOver = 0;
	_classData = NULL;
	_actorToPrintStrFor = 0;
	_sentenceNum = 0;
	memset(_sentence, 0, sizeof(_sentence));
	memset(_string, 0, sizeof(_string));
	_screenB = 0;
	_screenH = 0;
	_roomHeight = 0;
	_roomWidth = 0;
	_screenHeight = 0;
	_screenWidth = 0;
	for (uint i = 0; i < ARRAYSIZE(_virtscr); i++) {
		_virtscr[i].clear();
	}
	camera.reset();
	memset(_colorCycle, 0, sizeof(_colorCycle));
	memset(_colorUsedByCycle, 0, sizeof(_colorUsedByCycle));
	_ENCD_offs = 0;
	_EXCD_offs = 0;
	_CLUT_offs = 0;
	_EPAL_offs = 0;
	_IM00_offs = 0;
	_PALS_offs = 0;
	_fullRedraw = false;
	_bgNeedsRedraw = false;
	_screenEffectFlag = false;
	_completeScreenRedraw = false;
	_disableFadeInEffect = false;
	memset(&_cursor, 0, sizeof(_cursor));
	memset(_grabbedCursor, 0, sizeof(_grabbedCursor));
	_currentCursor = 0;
	_newEffect = 0;
	_switchRoomEffect2 = 0;
	_switchRoomEffect = 0;

	_bytesPerPixel = 1;
	_doEffect = false;
	_snapScroll = false;
	_shakeEnabled = false;
	_shakeFrame = 0;
	_screenStartStrip = 0;
	_screenEndStrip = 0;
	_screenTop = 0;
	_drawObjectQueNr = 0;
	memset(_drawObjectQue, 0, sizeof(_drawObjectQue));
	_palManipStart = 0;
	_palManipEnd = 0;
	_palManipCounter = 0;
	_palManipPalette = NULL;
	_palManipIntermediatePal = NULL;
	memset(gfxUsageBits, 0, sizeof(gfxUsageBits));
	_hePalettes = NULL;
	_hePaletteSlot = 0;
	_16BitPalette = NULL;
	_macScreen = NULL;
	_macIndy3TextBox = NULL;
#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
	_townsScreen = 0;
	_scrollRequest = _scrollDeltaAdjust = 0;
	_scrollDestOffset = _scrollTimer = 0;
	_refreshNeedCatchUp = false;
	_enableSmoothScrolling = (_game.platform == Common::kPlatformFMTowns);
	memset(_refreshDuration, 0, sizeof(_refreshDuration));
	_refreshArrayPos = 0;
#ifdef USE_RGB_COLOR
	_cjkFont = 0;
#endif
#endif
	_shadowPalette = NULL;
	_shadowPaletteSize = 0;
	_verbPalette = NULL;
	memset(_currentPalette, 0, sizeof(_currentPalette));
	memset(_darkenPalette, 0, sizeof(_darkenPalette));
	memset(_HEV7ActorPalette, 0, sizeof(_HEV7ActorPalette));
	_palDirtyMin = 0;
	_palDirtyMax = 0;
	_haveMsg = 0;
	_haveActorSpeechMsg = false;
	_useTalkAnims = false;
	_defaultTalkDelay = 0;
	_saveSound = 0;
	memset(_extraBoxFlags, 0, sizeof(_extraBoxFlags));
	memset(_scaleSlots, 0, sizeof(_scaleSlots));
	_charset = NULL;
	_charsetColor = 0;
	memset(_charsetColorMap, 0, sizeof(_charsetColorMap));
	memset(_charsetData, 0, sizeof(_charsetData));
	_charsetBufPos = 0;
	memset(_charsetBuffer, 0, sizeof(_charsetBuffer));
	_copyProtection = false;
	_voiceMode = 0;
	_talkDelay = 0;
	_NES_lastTalkingActor = 0;
	_NES_talkColor = 0;
	_keepText = false;
	_msgCount = 0;
	_costumeLoader = NULL;
	_costumeRenderer = NULL;
	_existLanguageFile = false;
	_languageBuffer = 0;
	_numTranslatedLines = 0;
	_translatedLines = 0;
	_languageLineIndex = 0;
	_2byteFontPtr = 0;
	_2byteWidth = 0;
	_2byteHeight = 0;
	_2byteShadow = 0;
	_krStrPost = 0;
	_V1TalkingActor = 0;
	for (int i = 0; i < 20; i++)
		_2byteMultiFontPtr[i] = NULL;
	_NESStartStrip = 0;

	_skipDrawObject = 0;

#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
	_townsPaletteFlags = 0;
	_townsClearLayerFlag = 1;
	_townsActiveLayerFlags = 3;
	_curStringRect.top = -1;
	_curStringRect.left = -1;
	_curStringRect.bottom = -1;
	_curStringRect.right = -1;

	for (int i = 0; i < ARRAYSIZE(_cyclRects); i++) {
		_cyclRects[i].top = 0;
		_cyclRects[i].left = 0;
		_cyclRects[i].bottom = 0;
		_cyclRects[i].right = 0;
	}

	_numCyclRects = 0;
	memset(_scrollFeedStrips, 0, sizeof(_scrollFeedStrips));
#endif

	//
	// Init all VARS to 0xFF
	//
	VAR_KEYPRESS = 0xFF;
	VAR_SYNC = 0xFF;
	VAR_EGO = 0xFF;
	VAR_CAMERA_POS_X = 0xFF;
	VAR_HAVE_MSG = 0xFF;
	VAR_ROOM = 0xFF;
	VAR_OVERRIDE = 0xFF;
	VAR_MACHINE_SPEED = 0xFF;
	VAR_ME = 0xFF;
	VAR_NUM_ACTOR = 0xFF;
	VAR_CURRENT_LIGHTS = 0xFF;
	VAR_CURRENTDRIVE = 0xFF;	// How about merging this with VAR_CURRENTDISK?
	VAR_CURRENTDISK = 0xFF;
	VAR_TMR_1 = 0xFF;
	VAR_TMR_2 = 0xFF;
	VAR_TMR_3 = 0xFF;
	VAR_MUSIC_TIMER = 0xFF;
	VAR_ACTOR_RANGE_MIN = 0xFF;
	VAR_ACTOR_RANGE_MAX = 0xFF;
	VAR_CAMERA_MIN_X = 0xFF;
	VAR_CAMERA_MAX_X = 0xFF;
	VAR_TIMER_NEXT = 0xFF;
	VAR_VIRT_MOUSE_X = 0xFF;
	VAR_VIRT_MOUSE_Y = 0xFF;
	VAR_ROOM_RESOURCE = 0xFF;
	VAR_LAST_SOUND = 0xFF;
	VAR_CUTSCENEEXIT_KEY = 0xFF;
	VAR_OPTIONS_KEY = 0xFF;
	VAR_TALK_ACTOR = 0xFF;
	VAR_CAMERA_FAST_X = 0xFF;
	VAR_SCROLL_SCRIPT = 0xFF;
	VAR_ENTRY_SCRIPT = 0xFF;
	VAR_ENTRY_SCRIPT2 = 0xFF;
	VAR_EXIT_SCRIPT = 0xFF;
	VAR_EXIT_SCRIPT2 = 0xFF;
	VAR_VERB_SCRIPT = 0xFF;
	VAR_SENTENCE_SCRIPT = 0xFF;
	VAR_INVENTORY_SCRIPT = 0xFF;
	VAR_CUTSCENE_START_SCRIPT = 0xFF;
	VAR_CUTSCENE_END_SCRIPT = 0xFF;
	VAR_CHARINC = 0xFF;
	VAR_CHARCOUNT = 0xFF;
	VAR_WALKTO_OBJ = 0xFF;
	VAR_DEBUGMODE = 0xFF;
	VAR_HEAPSPACE = 0xFF;
	VAR_RESTART_KEY = 0xFF;
	VAR_PAUSE_KEY = 0xFF;
	VAR_MOUSE_X = 0xFF;
	VAR_MOUSE_Y = 0xFF;
	VAR_TIMER = 0xFF;
	VAR_TIMER_TOTAL = 0xFF;
	VAR_SOUNDCARD = 0xFF;
	VAR_VIDEOMODE = 0xFF;
	VAR_MAINMENU_KEY = 0xFF;
	VAR_FIXEDDISK = 0xFF;
	VAR_CURSORSTATE = 0xFF;
	VAR_USERPUT = 0xFF;
	VAR_SOUNDRESULT = 0xFF;
	VAR_TALKSTOP_KEY = 0xFF;
	VAR_FADE_DELAY = 0xFF;
	VAR_NOSUBTITLES = 0xFF;

	VAR_SOUNDPARAM = 0xFF;
	VAR_SOUNDPARAM2 = 0xFF;
	VAR_SOUNDPARAM3 = 0xFF;
	VAR_INPUTMODE = 0xFF;
	VAR_MEMORY_PERFORMANCE = 0xFF;

	VAR_VIDEO_PERFORMANCE = 0xFF;
	VAR_ROOM_FLAG = 0xFF;
	VAR_GAME_LOADED = 0xFF;
	VAR_NEW_ROOM = 0xFF;
	VAR_VERSION_KEY = 0xFF;

	VAR_V5_TALK_STRING_Y = 0xFF;

	VAR_ROOM_WIDTH = 0xFF;
	VAR_ROOM_HEIGHT = 0xFF;
	VAR_SUBTITLES = 0xFF;
	VAR_V6_EMSSPACE = 0xFF;

	VAR_CAMERA_POS_Y = 0xFF;
	VAR_CAMERA_MIN_Y = 0xFF;
	VAR_CAMERA_MAX_Y = 0xFF;
	VAR_CAMERA_THRESHOLD_X = 0xFF;
	VAR_CAMERA_THRESHOLD_Y = 0xFF;
	VAR_CAMERA_SPEED_X = 0xFF;
	VAR_CAMERA_SPEED_Y = 0xFF;
	VAR_CAMERA_ACCEL_X = 0xFF;
	VAR_CAMERA_ACCEL_Y = 0xFF;
	VAR_CAMERA_DEST_X = 0xFF;
	VAR_CAMERA_DEST_Y = 0xFF;
	VAR_CAMERA_FOLLOWED_ACTOR = 0xFF;

	VAR_LEFTBTN_DOWN = 0xFF;
	VAR_RIGHTBTN_DOWN = 0xFF;
	VAR_LEFTBTN_HOLD = 0xFF;
	VAR_RIGHTBTN_HOLD = 0xFF;

	VAR_SAVELOAD_SCRIPT = 0xFF;
	VAR_SAVELOAD_SCRIPT2 = 0xFF;

	VAR_DEFAULT_TALK_DELAY = 0xFF;
	VAR_CHARSET_MASK = 0xFF;

	VAR_CUSTOMSCALETABLE = 0xFF;
	VAR_V6_SOUNDMODE = 0xFF;

	VAR_ACTIVE_VERB = 0xFF;
	VAR_ACTIVE_OBJECT1 = 0xFF;
	VAR_ACTIVE_OBJECT2 = 0xFF;
	VAR_VERB_ALLOWED = 0xFF;

	VAR_BLAST_ABOVE_TEXT = 0xFF;
	VAR_VOICE_MODE = 0xFF;
	VAR_MUSIC_BUNDLE_LOADED = 0xFF;
	VAR_VOICE_BUNDLE_LOADED = 0xFF;

	VAR_REDRAW_ALL_ACTORS = 0xFF;
	VAR_SKIP_RESET_TALK_ACTOR = 0xFF;

	VAR_SOUND_CHANNEL = 0xFF;
	VAR_TALK_CHANNEL = 0xFF;
	VAR_SOUNDCODE_TMR = 0xFF;
	VAR_RESERVED_SOUND_CHANNELS = 0xFF;

	VAR_MAIN_SCRIPT = 0xFF;

	VAR_NUM_SCRIPT_CYCLES = 0xFF;
	VAR_SCRIPT_CYCLE = 0xFF;

	VAR_QUIT_SCRIPT = 0xFF;

	VAR_NUM_GLOBAL_OBJS = 0xFF;

	// Use g_scumm from error() ONLY
	g_scumm = this;

	// Read settings from the detector & config manager
	_debugMode = (gDebugLevel >= 0);
	_dumpScripts = ConfMan.getBool("dump_scripts");
	_bootParam = ConfMan.getInt("boot_param");
	// Boot params often need debugging switched on to work
	if (_bootParam)
		_debugMode = true;

	_copyProtection = ConfMan.getBool("copy_protection");
	if (ConfMan.getBool("demo_mode"))
		_game.features |= GF_DEMO;
	if (ConfMan.hasKey("nosubtitles")) {
		// We replaced nosubtitles *ages* ago. Just convert it silently
		debug("Configuration key 'nosubtitles' is deprecated. Converting to 'subtitles'");
		if (!ConfMan.hasKey("subtitles"))
			ConfMan.setBool("subtitles", !ConfMan.getBool("nosubtitles"));
	}

	// Make sure that at least subtitles are enabled
	if (ConfMan.getBool("speech_mute") && !ConfMan.getBool("subtitles"))
		ConfMan.setBool("subtitles", true);

	// TODO Detect subtitle only versions of scumm6 games
	if (ConfMan.getBool("speech_mute"))
		_voiceMode = 2;
	else
		_voiceMode = ConfMan.getBool("subtitles");

	if (ConfMan.hasKey("render_mode")) {
		_renderMode = Common::parseRenderMode(ConfMan.get("render_mode"));
	} else {
		_renderMode = Common::kRenderDefault;
	}

	if (_game.platform == Common::kPlatformFMTowns && _game.id != GID_LOOM && _game.version == 3)
		if (ConfMan.getBool("aspect_ratio") && !ConfMan.getBool("trim_fmtowns_to_200_pixels")) {
			GUI::MessageDialog dialog(
				_("You have enabled 'aspect ratio correction'. However, FM-TOWNS' natural resolution is 320x240, which doesn't allow aspect ratio correction.\n"
				  "Aspect ratio correction can be acheived by trimming the resolution to 320x200, under 'engine' tab."));
			dialog.runModal();
		}

	// Check some render mode restrictions
	if (_game.version <= 1)
		_renderMode = Common::kRenderDefault;

	switch (_renderMode) {
	case Common::kRenderHercA:
	case Common::kRenderHercG:
		if (_game.version > 2 && _game.id != GID_MONKEY_EGA)
			_renderMode = Common::kRenderDefault;
		break;

	case Common::kRenderCGA:
	case Common::kRenderEGA:
	case Common::kRenderAmiga:
		if ((_game.version >= 4 && !(_game.features & GF_16COLOR)
			&& !(_game.platform == Common::kPlatformAmiga && _renderMode == Common::kRenderEGA))
			|| (_game.features & GF_OLD256))
			_renderMode = Common::kRenderDefault;
		break;

	case Common::kRenderMacintoshBW:
		if (_game.platform != Common::kPlatformMacintosh || (_game.id != GID_LOOM && _game.id != GID_INDY3)) {
			_renderMode = Common::kRenderDefault;
		}
		break;

	default:
		break;
	}

	_hexdumpScripts = false;
	_showStack = false;

	if (_game.platform == Common::kPlatformFMTowns && _game.version == 3) {	// FM-TOWNS V3 games originally use 320x240, and we have an option to trim to 200
		_screenWidth = 320;
		if (ConfMan.getBool("trim_fmtowns_to_200_pixels"))
			_screenHeight = 200;
		else
			_screenHeight = 240;
	} else if (_game.version == 8 || _game.heversion >= 71) {
		// COMI uses 640x480. Likewise starting from version 7.1, HE games use
		// 640x480, too.
		_screenWidth = 640;
		_screenHeight = 480;
	} else if (_game.platform == Common::kPlatformNES) {
		_screenWidth = 256;
		_screenHeight = 240;
	} else {
		_screenWidth = 320;
		_screenHeight = 200;
	}

#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
	if (_game.platform == Common::kPlatformFMTowns) {
		ConfMan.registerDefault("smooth_scroll", true);
		if (ConfMan.hasKey("smooth_scroll"))
			_enableSmoothScrolling = ConfMan.getBool("smooth_scroll");
	}
#endif

	_bytesPerPixel = (_game.features & GF_16BIT_COLOR) ? 2 : 1;
	uint8 sizeMult = _bytesPerPixel;

#ifdef USE_RGB_COLOR
#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
	if (_game.platform == Common::kPlatformFMTowns)
		sizeMult = 2;
#endif
#endif

	// Allocate gfx compositing buffer (not needed for V7/V8 games).
	if (_game.version < 7)
		_compositeBuf = (byte *)malloc(_screenWidth * _screenHeight * sizeMult);
	else
		_compositeBuf = 0;

	_herculesBuf = 0;
	if (_renderMode == Common::kRenderHercA || _renderMode == Common::kRenderHercG) {
		_herculesBuf = (byte *)malloc(kHercWidth * kHercHeight);
	}

#ifndef DISABLE_HELP
	// Create custom GMM dialog providing a help subdialog
	assert(!_mainMenuDialog);
	_mainMenuDialog = new ScummMenuDialog(this);
#endif
}


ScummEngine::~ScummEngine() {
	delete _musicEngine;

	_mixer->stopAll();

	if (_actors) {
		for (int i = 0; i < _numActors; ++i)
			delete _actors[i];
		delete[] _actors;
	}

	delete[] _sortedActors;

	delete[] _languageBuffer;
	delete[] _translatedLines;
	delete[] _languageLineIndex;

	if (_2byteFontPtr && !_useMultiFont)
		delete[] _2byteFontPtr;
	for (int i = 0; i < 20; i++)
		if (_2byteMultiFontPtr[i])
			delete _2byteMultiFontPtr[i];
	delete _charset;
	delete _messageDialog;
	delete _pauseDialog;
	delete _versionDialog;
	delete _fileHandle;

	delete _sound;

	delete _costumeLoader;
	delete _costumeRenderer;

	_textSurface.free();

	free(_shadowPalette);
	free(_verbPalette);

	free(_palManipPalette);
	free(_palManipIntermediatePal);

	free(_objectStateTable);
	free(_objectRoomTable);
	free(_objectOwnerTable);
	free(_inventory);
	free(_verbs);
	free(_objs);
	free(_roomVars);
	free(_scummVars);
	free(_bitVars);
	free(_newNames);
	free(_classData);
	free(_arraySlot);

	free(_compositeBuf);
	free(_herculesBuf);

	free(_16BitPalette);

	if (_macScreen) {
		_macScreen->free();
		delete _macScreen;
	}

	if (_macIndy3TextBox) {
		_macIndy3TextBox->free();
		delete _macIndy3TextBox;
	}

#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
	delete _townsScreen;
#ifdef USE_RGB_COLOR
	delete _cjkFont;
#endif
#endif

	delete _res;
	delete _gdi;
}


ScummEngine_v5::ScummEngine_v5(OSystem *syst, const DetectorResult &dr)
 : ScummEngine(syst, dr) {

	// All "classic" games (V5 and older) encrypted their data files
	// with exception of the GF_OLD256 games and the PC-Engine version
	// of Loom.
	if (!(_game.features & GF_OLD256))
		_game.features |= GF_USE_KEY;

	resetCursors();

	// Setup flashlight
	memset(&_flashlight, 0, sizeof(_flashlight));
	_flashlight.xStrips = 7;
	_flashlight.yStrips = 7;
	_flashlight.buffer = NULL;

	memset(_saveLoadVarsFilename, 0, sizeof(_saveLoadVarsFilename));

	_resultVarNumber = 0;
}

ScummEngine_v4::ScummEngine_v4(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v5(syst, dr) {
	_resourceHeaderSize = 6;
	_game.features |= GF_SMALL_HEADER;
}

ScummEngine_v3::ScummEngine_v3(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v4(syst, dr) {
	// All v3 and older games only used 16 colors with exception of the GF_OLD256 games.
	if (!(_game.features & GF_OLD256))
		_game.features |= GF_16COLOR;

	_savePreparedSavegame = NULL;
}

ScummEngine_v3::~ScummEngine_v3() {
	delete _savePreparedSavegame;
}

ScummEngine_v3old::ScummEngine_v3old(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v3(syst, dr) {
	_resourceHeaderSize = 4;
	_game.features |= GF_OLD_BUNDLE;
}

ScummEngine_v2::ScummEngine_v2(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v3old(syst, dr) {

	_inventoryOffset = 0;
	_flashlight.xStrips = 6;
	_flashlight.yStrips = 4;

	VAR_SENTENCE_VERB = 0xFF;
	VAR_SENTENCE_OBJECT1 = 0xFF;
	VAR_SENTENCE_OBJECT2 = 0xFF;
	VAR_SENTENCE_PREPOSITION = 0xFF;
	VAR_BACKUP_VERB = 0xFF;

	VAR_CLICK_AREA = 0xFF;
	VAR_CLICK_VERB = 0xFF;
	VAR_CLICK_OBJECT = 0xFF;
}

ScummEngine_v0::ScummEngine_v0(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v2(syst, dr) {
	_drawDemo = false;
	_currentMode = 0;
	_currentLights = 0;

	_activeVerb = kVerbNone;
	_activeObject = 0;
	_activeObject2 = 0;

	_cmdVerb = kVerbNone;
	_cmdObject = 0;
	_cmdObject2 = 0;

	VAR_ACTIVE_OBJECT2 = 0xFF;
	VAR_IS_SOUND_RUNNING = 0xFF;
	VAR_ACTIVE_VERB = 0xFF;

	DelayReset();

	if (strcmp(dr.fp.pattern, "maniacdemo.d64") == 0 )
		_game.features |= GF_DEMO;
}

void ScummEngine_v0::DelayReset() {
	_V0Delay._screenScroll = false;
	_V0Delay._objectRedrawCount = 0;
	_V0Delay._objectStripRedrawCount = 0;
	_V0Delay._actorRedrawCount = 0;
	_V0Delay._actorLimbRedrawDrawCount = 0;
}

int ScummEngine_v0::DelayCalculateDelta() {
	float Time = 0;

	// These values are made up, based on trial/error with visual inspection against WinVice
	// If anyone feels inclined, the routines in the original engine could be profiled
	// and these values changed accordindly.
	Time += _V0Delay._objectRedrawCount * 7;
	Time += _V0Delay._objectStripRedrawCount * 0.6;
	Time += _V0Delay._actorRedrawCount * 2.0;
	Time += _V0Delay._actorLimbRedrawDrawCount * 0.3;

	if (_V0Delay._screenScroll)
		Time += 3.6f;

	DelayReset();

	return floor(Time + 0.5);
}

ScummEngine_v6::ScummEngine_v6(OSystem *syst, const DetectorResult &dr)
	: ScummEngine(syst, dr) {
	_blastObjectQueuePos = 0;
	for (uint i = 0; i < ARRAYSIZE(_blastObjectQueue); i++) {
		_blastObjectQueue[i].clear();
	}
	_blastTextQueuePos = 0;
	for (uint i = 0; i < ARRAYSIZE(_blastTextQueue); i++) {
		_blastTextQueue[i].clear();
	}

	memset(_akosQueue, 0, sizeof(_akosQueue));
	_akosQueuePos = 0;

	_curActor = 0;
	_curVerb = 0;
	_curVerbSlot = 0;

	_forcedWaitForMessage = false;
	_skipVideo = false;

	VAR_VIDEONAME = 0xFF;
	VAR_RANDOM_NR = 0xFF;
	VAR_STRING2DRAW = 0xFF;

	VAR_TIMEDATE_YEAR = 0xFF;
	VAR_TIMEDATE_MONTH = 0xFF;
	VAR_TIMEDATE_DAY = 0xFF;
	VAR_TIMEDATE_HOUR = 0xFF;
	VAR_TIMEDATE_MINUTE = 0xFF;
	VAR_TIMEDATE_SECOND = 0xFF;
}

ScummEngine_v60he::ScummEngine_v60he(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v6(syst, dr) {
	memset(_hInFileTable, 0, sizeof(_hInFileTable));
	memset(_hOutFileTable, 0, sizeof(_hOutFileTable));

	_actorClipOverride.top = 0;
	_actorClipOverride.bottom = 480;
	_actorClipOverride.left = 0;
	_actorClipOverride.right = 640;

	memset(_heTimers, 0, sizeof(_heTimers));

	if (_game.heversion >= 61)
		_game.features |= GF_NEW_COSTUMES;
}

ScummEngine_v60he::~ScummEngine_v60he() {
	for (int i = 0; i < 17; ++i) {
		delete _hInFileTable[i];
		delete _hOutFileTable[i];
	}
}

ScummEngine_v70he::ScummEngine_v70he(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v60he(syst, dr) {
	if (_game.platform == Common::kPlatformMacintosh && (_game.heversion >= 72 && _game.heversion <= 74))
		_resExtractor = new MacResExtractor(this);
	else
		_resExtractor = new Win32ResExtractor(this);

	_heV7DiskOffsets = NULL;
	_heV7RoomOffsets = NULL;
	_heV7RoomIntOffsets = NULL;

	_heSndSoundId = 0;
	_heSndOffset = 0;
	_heSndChannel = 0;
	_heSndFlags = 0;
	_heSndSoundFreq = 0;
	_heSndPan = 0;
	_heSndVol = 0;

	_numStoredFlObjects = 0;
	_storedFlObjects = (ObjectData *)calloc(100, sizeof(ObjectData));

	VAR_NUM_SOUND_CHANNELS = 0xFF;
}

ScummEngine_v70he::~ScummEngine_v70he() {
	delete _resExtractor;
	free(_heV7DiskOffsets);
	free(_heV7RoomOffsets);
	free(_heV7RoomIntOffsets);
	free(_storedFlObjects);
}

#ifdef ENABLE_HE
ScummEngine_v71he::ScummEngine_v71he(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v70he(syst, dr) {
	_auxBlocksNum = 0;
	for (uint i = 0; i < ARRAYSIZE(_auxBlocks); i++) {
		_auxBlocks[i].clear();
	}
	_auxEntriesNum = 0;
	memset(_auxEntries, 0, sizeof(_auxEntries));

	_wiz = new Wiz(this);

	_skipProcessActors = 0;

	VAR_WIZ_TCOLOR = 0xFF;
}

ScummEngine_v71he::~ScummEngine_v71he() {
	delete _wiz;
}

ScummEngine_v72he::ScummEngine_v72he(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v71he(syst, dr) {
	VAR_NUM_ROOMS = 0xFF;
	VAR_NUM_SCRIPTS = 0xFF;
	VAR_NUM_SOUNDS = 0xFF;
	VAR_NUM_COSTUMES = 0xFF;
	VAR_NUM_IMAGES = 0xFF;
	VAR_NUM_CHARSETS = 0xFF;
	VAR_SOUND_ENABLED = 0xFF;
	VAR_POLYGONS_ONLY = 0xFF;
	VAR_MOUSE_STATE = 0xFF;
	VAR_PLATFORM = 0xFF;
}

ScummEngine_v80he::ScummEngine_v80he(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v72he(syst, dr) {
	_heSndResId = 0;
	_curSndId = 0;
	_sndPtrOffs = 0;
	_sndTmrOffs = 0;
	_sndDataSize = 0;

	VAR_PLATFORM_VERSION = 0xFF;
	VAR_CURRENT_CHARSET = 0xFF;
	VAR_KEY_STATE = 0xFF;
	VAR_COLOR_DEPTH = 0xFF;
}

ScummEngine_v90he::ScummEngine_v90he(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v80he(syst, dr) {
	_moviePlay = new MoviePlayer(this, _mixer);
	_sprite = new Sprite(this);

	memset(_videoParams.filename, 0, sizeof(_videoParams.filename));
	_videoParams.status = 0;
	_videoParams.flags = 0;
	_videoParams.number = 0;
	_videoParams.wizResNum = 0;

	VAR_NUM_SPRITE_GROUPS = 0xFF;
	VAR_NUM_SPRITES = 0xFF;
	VAR_NUM_PALETTES = 0xFF;
	VAR_NUM_UNK = 0xFF;

	VAR_U32_VERSION = 0xFF;
	VAR_U32_ARRAY_UNK = 0xFF;
}

ScummEngine_v90he::~ScummEngine_v90he() {
	delete _moviePlay;
	delete _sprite;
	if (_game.heversion >= 98) {
		delete _logicHE;
	}
	if (_game.heversion >= 99) {
		free(_hePalettes);
	}
}

ScummEngine_v100he::ScummEngine_v100he(OSystem *syst, const DetectorResult &dr) : ScummEngine_v99he(syst, dr) {
	/* Moonbase stuff */
	_moonbase = 0;

	if (_game.id == GID_MOONBASE)
		_moonbase = new Moonbase(this);

	VAR_U32_USER_VAR_A = 0xFF;
	VAR_U32_USER_VAR_B = 0xFF;
	VAR_U32_USER_VAR_C = 0xFF;
	VAR_U32_USER_VAR_D = 0xFF;
	VAR_U32_USER_VAR_E = 0xFF;
	VAR_U32_USER_VAR_F = 0xFF;
}

ScummEngine_v100he::~ScummEngine_v100he() {
	delete _moonbase;
}

ScummEngine_vCUPhe::ScummEngine_vCUPhe(OSystem *syst, const DetectorResult &dr) : Engine(syst){
	_syst = syst;
	_game = dr.game;
	_filenamePattern = dr.fp;

	_cupPlayer = new CUP_Player(syst, this, _mixer);
}

ScummEngine_vCUPhe::~ScummEngine_vCUPhe() {
	delete _cupPlayer;
}

Common::Error ScummEngine_vCUPhe::run() {
	initGraphics(CUP_Player::kDefaultVideoWidth, CUP_Player::kDefaultVideoHeight);

	if (_cupPlayer->open(_filenamePattern.pattern)) {
		_cupPlayer->play();
		_cupPlayer->close();
	}
	return Common::kNoError;
}

void ScummEngine_vCUPhe::parseEvents() {
	Common::Event event;

	while (_eventMan->pollEvent(event)) {
#if 0
		switch (event.type) {

		default:
			break;
		}
#endif
	}
}

#endif

#ifdef ENABLE_SCUMM_7_8
ScummEngine_v7::ScummEngine_v7(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v6(syst, dr) {
	_verbLineSpacing = 10;

	_smushFrameRate = 0;
	_smushVideoShouldFinish = false;
	_smushActive = false;
	_smixer = NULL;
	_splayer = NULL;

	_existLanguageFile = false;
	_languageBuffer = NULL;
	_languageIndex = NULL;
	clearSubtitleQueue();

	_game.features |= GF_NEW_COSTUMES;
}

ScummEngine_v7::~ScummEngine_v7() {
	if (_smixer) {
		_smixer->stop();
		delete _smixer;
	}
	if (_splayer) {
		_splayer->release();
		delete _splayer;
	}

	delete _insane;

	free(_languageBuffer);
	free(_languageIndex);
}

ScummEngine_v8::ScummEngine_v8(OSystem *syst, const DetectorResult &dr)
	: ScummEngine_v7(syst, dr) {
	_objectIDMap = 0;
	_keyScriptKey = 0;
	_keyScriptNo = 0;

	VAR_LANGUAGE = 0xFF;
}

ScummEngine_v8::~ScummEngine_v8() {
	delete[] _objectIDMap;
}
#endif

#pragma mark -
#pragma mark --- Initialization ---
#pragma mark -

Common::Error ScummEngine::init() {

	const Common::FSNode gameDataDir(ConfMan.get("path"));

	// Add default file directories.
	if (((_game.platform == Common::kPlatformAmiga) || (_game.platform == Common::kPlatformAtariST)) && (_game.version <= 4)) {
		// This is for the Amiga version of Indy3/Loom/Maniac/Zak
		SearchMan.addSubDirectoryMatching(gameDataDir, "rooms");
	}

	if ((_game.platform == Common::kPlatformMacintosh) && (_game.version == 3)) {
		// This is for the Mac version of Indy3/Loom
		SearchMan.addSubDirectoryMatching(gameDataDir, "rooms 1");
		SearchMan.addSubDirectoryMatching(gameDataDir, "rooms 2");
		SearchMan.addSubDirectoryMatching(gameDataDir, "rooms 3");
	}

#ifdef ENABLE_SCUMM_7_8
#ifdef MACOSX
	if (_game.version == 8 && !memcmp(gameDataDir.getPath().c_str(), "/Volumes/MONKEY3_", 17)) {
		// Special case for COMI on Mac OS X. The mount points on OS X depend
		// on the volume name. Hence if playing from CD, we'd get a problem.
		// So if loading of a resource file fails, we fall back to the (fixed)
		// CD mount points (/Volumes/MONKEY3_1 and /Volumes/MONKEY3_2).
		//
		// This check for whether we play from CD is very crude, though.

		SearchMan.addSubDirectoryMatching(Common::FSNode("/"), "Volumes/MONKEY3_1/RESOURCE");
		SearchMan.addSubDirectoryMatching(Common::FSNode("/"), "Volumes/MONKEY3_2");
		SearchMan.addSubDirectoryMatching(Common::FSNode("/"), "Volumes/MONKEY3_2/RESOURCE");
	} else
#endif
	if (_game.version == 8)
		// This is for COMI
		SearchMan.addSubDirectoryMatching(gameDataDir, "resource");

	if (_game.version == 7) {
		// This is for Full Throttle & The Dig
		SearchMan.addSubDirectoryMatching(gameDataDir, "video");
		SearchMan.addSubDirectoryMatching(gameDataDir, "data");
	}
#endif

	// Extra directories needed for the Steam versions
	if (_filenamePattern.genMethod == kGenDiskNumSteam || _filenamePattern.genMethod == kGenRoomNumSteam) {
		if (_game.platform == Common::kPlatformWindows) {
			switch (_game.id) {
			case GID_INDY3 :
				SearchMan.addSubDirectoryMatching(gameDataDir, "indy3");
				break;
			case GID_INDY4 :
				SearchMan.addSubDirectoryMatching(gameDataDir, "atlantis");
				break;
			case GID_LOOM :
				SearchMan.addSubDirectoryMatching(gameDataDir, "loom");
				break;
#ifdef ENABLE_SCUMM_7_8
			case GID_DIG :
				SearchMan.addSubDirectoryMatching(gameDataDir, "dig");
				SearchMan.addSubDirectoryMatching(gameDataDir, "dig/video");
				break;
#endif
			default:
				break;
			}
		} else {
			SearchMan.addSubDirectoryMatching(gameDataDir, "Contents");
			SearchMan.addSubDirectoryMatching(gameDataDir, "Contents/MacOS");
			SearchMan.addSubDirectoryMatching(gameDataDir, "Contents/Resources");
			SearchMan.addSubDirectoryMatching(gameDataDir, "Contents/Resources/video");
		}
	}

	// The	kGenUnchanged method is only used for 'container files', i.e. files
	// that contain the real game files bundled together in an archive format.
	// This is the case of the NES, v0 and Mac versions of certain games.
	// Note: All of these can also occur in 'extracted' form, in which case they
	// are treated like any other SCUMM game.
	if (_filenamePattern.genMethod == kGenUnchanged) {

		if (_game.platform == Common::kPlatformNES) {
			// We read data directly from NES ROM instead of extracting it with
			// external tool
			assert(_game.id == GID_MANIAC);
			_fileHandle = new ScummNESFile();
			_containerFile = _filenamePattern.pattern;

			_filenamePattern.pattern = "%.2d.LFL";
			_filenamePattern.genMethod = kGenRoomNum;
		} else if (_game.platform == Common::kPlatformApple2GS) {
			// Read data from Apple II disk images.
			const char *tmpBuf1, *tmpBuf2;
			assert(_game.id == GID_MANIAC);
			tmpBuf1 = "maniac1.dsk";
			tmpBuf2 = "maniac2.dsk";

			_fileHandle = new ScummDiskImage(tmpBuf1, tmpBuf2, _game);
			_containerFile = tmpBuf1;

			_filenamePattern.pattern = "%.2d.LFL";
			_filenamePattern.genMethod = kGenRoomNum;
		} else if (_game.platform == Common::kPlatformC64) {
			// Read data from C64 disk images.
			const char *tmpBuf1, *tmpBuf2;
			assert(_game.id == GID_MANIAC || _game.id == GID_ZAK);
			if (_game.id == GID_MANIAC) {
				if (_game.features & GF_DEMO) {
					tmpBuf1 = "maniacdemo.d64";
					tmpBuf2 = "maniacdemo.d64";
				} else {
					tmpBuf1 = "maniac1.d64";
					tmpBuf2 = "maniac2.d64";
				}
			} else {
				tmpBuf1 = "zak1.d64";
				tmpBuf2 = "zak2.d64";
			}

			_fileHandle = new ScummDiskImage(tmpBuf1, tmpBuf2, _game);
			_containerFile = tmpBuf1;

			_filenamePattern.pattern = "%.2d.LFL";
			_filenamePattern.genMethod = kGenRoomNum;
		} else if (_game.platform == Common::kPlatformMacintosh) {
			// The mac versions of Indy4, Sam&Max, DOTT, FT and The Dig used a
			// special meta (container) file format to store the actual SCUMM data
			// files. The rescumm utility used to be used to extract those files.
			// While that is still possible, we now support reading those files
			// directly. The first step is to check whether one of them is present
			// (we do that here); the rest is handled by the  ScummFile class and
			// code in openResourceFile() (and in the Sound class, for MONSTER.SOU
			// handling).
			assert(_game.version >= 5 && _game.heversion == 0);
			_fileHandle = new ScummFile();
			_containerFile = _filenamePattern.pattern;


			// We now have to determine the correct _filenamePattern. To do this
			// we simply hardcode the possibilities.
			const char *p1 = 0, *p2 = 0;
			switch (_game.id) {
			case GID_INDY4:
				p1 = "atlantis.%03d";
				break;
			case GID_TENTACLE:
				p1 = "tentacle.%03d";
				p2 = "dottdemo.%03d";
				break;
			case GID_SAMNMAX:
				p1 = "samnmax.%03d";
				p2 = "samdemo.%03d";
				break;
			case GID_FT:
				p1 = "ft.la%d";
				p2 = "ftdemo.la%d";
				break;
			case GID_DIG:
				p1 = "dig.la%d";
				break;
			default:
				break;
			}

			// Test which file name to use
			_filenamePattern.genMethod = kGenDiskNum;
			if (!_fileHandle->open(_containerFile))
				error("Couldn't open container file '%s'", _containerFile.c_str());

			if ((_filenamePattern.pattern = p1) && _fileHandle->openSubFile(generateFilename(0))) {
				// Found regular version
			} else if ((_filenamePattern.pattern = p2) && _fileHandle->openSubFile(generateFilename(0))) {
				// Found demo
				_game.features |= GF_DEMO;
			} else
				error("Couldn't find known subfile inside container file '%s'", _containerFile.c_str());

			_fileHandle->close();
		} else {
			error("kGenUnchanged used with unsupported platform");
		}
	} else {
		if (_filenamePattern.genMethod == kGenDiskNumSteam || _filenamePattern.genMethod == kGenRoomNumSteam) {
			// Steam game versions have the index file embedded in the main executable
			const SteamIndexFile *indexFile = lookUpSteamIndexFile(_filenamePattern.pattern, _game.platform);
			if (!indexFile || indexFile->id != _game.id) {
				error("Couldn't find index file description for Steam version");
			} else {
				_fileHandle = new ScummSteamFile(*indexFile);
			}
		} else {
			// Regular access, no container file involved
			_fileHandle = new ScummFile();
		}
	}

	// Steam Win and Mac versions share the same DOS data files. We show Windows or Mac
	// for the platform the detector, but internally we force the platform to DOS, so that
	// the code for handling the original DOS data files is used.
	if (_filenamePattern.genMethod == kGenDiskNumSteam || _filenamePattern.genMethod == kGenRoomNumSteam)
		_game.platform = Common::kPlatformDOS;

	// Load CJK font, if present
	// Load it earlier so _useCJKMode variable could be set
	loadCJKFont();

	Common::String macResourceFile;

	if (_game.platform == Common::kPlatformMacintosh) {
		Common::MacResManager resource;

		// \xAA is a trademark glyph in Mac OS Roman. We try that, but
		// also the Windows version, the UTF-8 version, and just plain
		// without in case the file system can't handle exotic
		// characters like that.

		if (_game.id == GID_INDY3) {
			static const char *indyFileNames[] = {
				"Indy\xAA",
				"Indy\x99",
				"Indy\xE2\x84\xA2",
				"Indy"
			};

			for (int i = 0; i < ARRAYSIZE(indyFileNames); i++) {
				if (resource.exists(indyFileNames[i])) {
					macResourceFile = indyFileNames[i];

					_textSurfaceMultiplier = 2;
					_macScreen = new Graphics::Surface();
					_macScreen->create(640, 400, Graphics::PixelFormat::createFormatCLUT8());

					_macIndy3TextBox = new Graphics::Surface();
					_macIndy3TextBox->create(448, 47, Graphics::PixelFormat::createFormatCLUT8());
					break;
				}
			}

			if (macResourceFile.empty()) {
				GUI::MessageDialog dialog(_(
"Could not find the 'Indy' Macintosh executable. High-resolution fonts will\n"
"be disabled."), _("OK"));
				dialog.runModal();
			}

		} else if (_game.id == GID_LOOM) {
			static const char *loomFileNames[] = {
				"Loom\xAA",
				"Loom\x99",
				"Loom\xE2\x84\xA2",
				"Loom"
			};

			for (int i = 0; i < ARRAYSIZE(loomFileNames); i++) {
				if (resource.exists(loomFileNames[i])) {
					macResourceFile = loomFileNames[i];

					_textSurfaceMultiplier = 2;
					_macScreen = new Graphics::Surface();
					_macScreen->create(640, 400, Graphics::PixelFormat::createFormatCLUT8());
					break;
				}
			}

			if (macResourceFile.empty()) {
				GUI::MessageDialog dialog(_(
"Could not find the 'Loom' Macintosh executable. Music and high-resolution\n"
"versions of font and cursor will be disabled."), _("OK"));
				dialog.runModal();
			}
		} else if (_game.id == GID_MONKEY) {
			// Try both with and without underscore in the
			// filename, because some tools (e.g. hfsutils) may
			// turn the space into an underscore.

			static const char *monkeyIslandFileNames[] = {
			        "Monkey Island",
			        "Monkey_Island"
			};

		       for (int i = 0; i < ARRAYSIZE(monkeyIslandFileNames); i++) {
		                if (resource.exists(monkeyIslandFileNames[i])) {
		                        macResourceFile = monkeyIslandFileNames[i];
		                }
		        }

			if (macResourceFile.empty()) {
			        GUI::MessageDialog dialog(_(
"Could not find the 'Monkey Island' Macintosh executable to read the\n"
"instruments from. Music will be disabled."), _("OK"));
				dialog.runModal();
			}
		}

		if (!_macScreen && _renderMode == Common::kRenderMacintoshBW) {
			_renderMode = Common::kRenderDefault;
		}
	}

	// Initialize backend
	if (_renderMode == Common::kRenderHercA || _renderMode == Common::kRenderHercG) {
		initGraphics(kHercWidth, kHercHeight);
	} else {
		int screenWidth = _screenWidth;
		int screenHeight = _screenHeight;
		if (_useCJKMode || _macScreen) {
			// CJK FT and DIG use usual NUT fonts, not FM-TOWNS ROM, so
			// there is no text surface for them. This takes that into account
			screenWidth *= _textSurfaceMultiplier;
			screenHeight *= _textSurfaceMultiplier;
		}
		if (_game.features & GF_16BIT_COLOR
#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
			|| _game.platform == Common::kPlatformFMTowns
#endif
			) {
#ifdef USE_RGB_COLOR
			_outputPixelFormat = Graphics::PixelFormat(2, 5, 5, 5, 0, 10, 5, 0, 0);

			if (_game.platform != Common::kPlatformFMTowns && _game.platform != Common::kPlatformPCEngine) {
				initGraphics(screenWidth, screenHeight, &_outputPixelFormat);
				if (_outputPixelFormat != _system->getScreenFormat())
					return Common::kUnsupportedColorMode;
			} else {
				Common::List<Graphics::PixelFormat> tryModes = _system->getSupportedFormats();
				for (Common::List<Graphics::PixelFormat>::iterator g = tryModes.begin(); g != tryModes.end(); ++g) {
					if (g->bytesPerPixel != 2 || g->aBits()) {
						g = tryModes.reverse_erase(g);
					} else if (*g == _outputPixelFormat) {
						tryModes.clear();
						tryModes.push_back(_outputPixelFormat);
						break;
					}
				}

				initGraphics(screenWidth, screenHeight, tryModes);
				if (_system->getScreenFormat().bytesPerPixel != 2)
					return Common::kUnsupportedColorMode;
			}
#else
			if (_game.platform == Common::kPlatformFMTowns && _game.version == 3) {
				warning("Starting game without the required 16bit color support.\nYou may experience color glitches");
				initGraphics(screenWidth, screenHeight);
			} else {
				return Common::Error(Common::kUnsupportedColorMode, "16bit color support is required for this game");
			}
#endif
		} else {
#ifdef DISABLE_TOWNS_DUAL_LAYER_MODE
		if (_game.platform == Common::kPlatformFMTowns && _game.version == 5)
			return Common::Error(Common::kUnsupportedColorMode, "This game requires dual graphics layer support which is disabled in this build");
#endif
			initGraphics(screenWidth, screenHeight);

			if (_game.platform == Common::kPlatformNES)
				_system->fillScreen(0x1d);
			}
	}

#ifdef ENABLE_HE
	Localizer *loc = new Localizer();
	if (!loc->isValid())
		delete loc;
	else
		_localizer = loc;
#endif

	_outputPixelFormat = _system->getScreenFormat();

	setupScumm(macResourceFile);

	readIndexFile();

	// Create the debugger now that _numVariables has been set
	setDebugger(new ScummDebugger(this));

	resetScumm();
	resetScummVars();

	if (_game.version >= 5 && _game.version <= 7)
		_sound->setupSound();

	syncSoundSettings();

	return Common::kNoError;
}

void ScummEngine::setupScumm(const Common::String &macResourceFile) {
	Common::String macInstrumentFile;
	Common::String macFontFile;

	if (_game.platform == Common::kPlatformMacintosh) {
		if (_game.id == GID_INDY3) {
			macFontFile = macResourceFile;
		} if (_game.id == GID_LOOM) {
			macInstrumentFile = macResourceFile;
			macFontFile = macResourceFile;
			_macCursorFile = macResourceFile;
		} else if (_game.id == GID_MONKEY) {
			macInstrumentFile = macResourceFile;
		}
	}

	// On some systems it's not safe to run CD audio games from the CD.
	if (_game.features & GF_AUDIOTRACKS && !Common::File::exists("CDDA.SOU")) {
		if (!existExtractedCDAudioFiles()
		    && !isDataAndCDAudioReadFromSameCD()) {
			warnMissingExtractedCDAudio();
		}
		_system->getAudioCDManager()->open();
	}

	// Create the sound manager
	if (_game.heversion > 0)
		_sound = new SoundHE(this, _mixer);
	else
		_sound = new Sound(this, _mixer);

	// Setup the music engine
	setupMusic(_game.midi, macInstrumentFile);

	// Load localization data, if present
	loadLanguageBundle();

	// Create the charset renderer
	setupCharsetRenderer(macFontFile);

	// Create and clear the text surface
	_textSurface.create(_screenWidth * _textSurfaceMultiplier, _screenHeight * _textSurfaceMultiplier, Graphics::PixelFormat::createFormatCLUT8());
	clearTextSurface();

	// Create the costume renderer
	setupCostumeRenderer();

	// Load game from specified slot, if any
	if (ConfMan.hasKey("save_slot")) {
		requestLoad(ConfMan.getInt("save_slot"));
	} else if (!ConfMan.hasKey("boot_param") && _game.id == GID_LOOM && _game.platform == Common::kPlatformFMTowns) {
		// In case we run the Loom FM-Towns version and have no boot parameter
		// nor start save game supplied we will show our own custom difficulty
		// selection dialog, since the original does not have any.
		LoomTownsDifficultyDialog difficultyDialog;
		runDialog(difficultyDialog);

		int difficulty = difficultyDialog.getSelectedDifficulty();
		if (difficulty != -1)
			_bootParam = difficulty;
	}

	_res->allocResTypeData(rtBuffer, 0, 10, kDynamicResTypeMode);

	setupScummVars();

	setupOpcodes();

	if (_game.version == 8)
		_numActors = 80;
	else if (_game.version == 7)
		_numActors = 30;
	else if (_game.id == GID_SAMNMAX)
		_numActors = 30;
	else if (_game.id == GID_MANIAC)
		_numActors = 25;
	else if (_game.heversion >= 80)
		_numActors = 62;
	else if (_game.heversion >= 72)
		_numActors = 30;
	else
		_numActors = 13;

	if (_game.version >= 7)
		OF_OWNER_ROOM = 0xFF;
	else
		OF_OWNER_ROOM = 0x0F;

	// if (_game.id==GID_MONKEY2 && _bootParam == 0)
	//	_bootParam = 10001;

	if (!_copyProtection && _game.id == GID_INDY4 && _bootParam == 0) {
		_bootParam = -7873;
	}

	// This boot param does not exist in the DOS version, but skips straight
	// to the difficulty selection screen in the Mac versions. (One of them
	// didn't show the difficulty selection screen at all, but we patch the
	// boot script to enable that.)
	if (!_copyProtection && _game.id == GID_MONKEY2 && _game.platform == Common::kPlatformMacintosh && _bootParam == 0) {
		_bootParam = -7873;
	}

	if (!_copyProtection && _game.id == GID_SAMNMAX && _bootParam == 0) {
		_bootParam = -1;
	}

	int maxHeapThreshold = -1;

	if (_game.features & GF_16BIT_COLOR) {
		// 16bit color games require double the memory, due to increased resource sizes.
		maxHeapThreshold = 12 * 1024 * 1024;
	} else if (_game.features & GF_NEW_COSTUMES) {
		// Since the new costumes are very big, we increase the heap limit, to avoid having
		// to constantly reload stuff from the data files.
		maxHeapThreshold = 6 * 1024 * 1024;
	} else {
		maxHeapThreshold = 550000;
	}

	_res->setHeapThreshold(400000, maxHeapThreshold);

	free(_compositeBuf);
	_compositeBuf = (byte *)malloc(_screenWidth * _textSurfaceMultiplier * _screenHeight * _textSurfaceMultiplier * _outputPixelFormat.bytesPerPixel);
}

#ifdef ENABLE_SCUMM_7_8
void ScummEngine_v7::setupScumm(const Common::String &macResourceFile) {

	if (_game.id == GID_DIG && (_game.features & GF_DEMO))
		_smushFrameRate = 15;
	else
		_smushFrameRate = (_game.id == GID_FT) ? 10 : 12;

	int dimuseTempo = CLIP(ConfMan.getInt("dimuse_tempo"), 10, 100);
	ConfMan.setInt("dimuse_tempo", dimuseTempo);
	ConfMan.flushToDisk();
	_musicEngine = _imuseDigital = new IMuseDigital(this, _mixer, dimuseTempo);

	ScummEngine::setupScumm(macResourceFile);

	// Create FT INSANE object
	if (_game.id == GID_FT)
		_insane = new Insane(this);
	else
		_insane = 0;

	_smixer = new SmushMixer(_mixer);

	_splayer = new SmushPlayer(this);
}
#endif

void ScummEngine::setupCharsetRenderer(const Common::String &macFontFile) {
	if (_game.version <= 2) {
		if (_game.platform == Common::kPlatformNES)
			_charset = new CharsetRendererNES(this);
		else
			_charset = new CharsetRendererV2(this, _language);
	} else if (_game.version == 3) {
#ifdef USE_RGB_COLOR
		if (_game.platform == Common::kPlatformPCEngine)
			_charset = new CharsetRendererPCE(this);
		else
#endif
		if (_game.platform == Common::kPlatformFMTowns)
			_charset = new CharsetRendererTownsV3(this);
		else if (_game.platform == Common::kPlatformMacintosh && !macFontFile.empty())
			_charset = new CharsetRendererMac(this, macFontFile);
		else
			_charset = new CharsetRendererV3(this);
#ifdef ENABLE_SCUMM_7_8
	} else if (_game.version == 8) {
		_charset = new CharsetRendererNut(this);
#endif
	} else {
#ifdef USE_RGB_COLOR
#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
		if (_game.platform == Common::kPlatformFMTowns)
			_charset = new CharsetRendererTownsClassic(this);
		else
#endif
#endif
			_charset = new CharsetRendererClassic(this);
	}
}

void ScummEngine::setupCostumeRenderer() {
	if (_game.features & GF_NEW_COSTUMES) {
		_costumeRenderer = new AkosRenderer(this);
		_costumeLoader = new AkosCostumeLoader(this);
	} else if (_game.version == 0) {
		_costumeRenderer = new V0CostumeRenderer(this);
		_costumeLoader = new V0CostumeLoader(this);
	} else if (_game.platform == Common::kPlatformNES) {
		_costumeRenderer = new NESCostumeRenderer(this);
		_costumeLoader = new NESCostumeLoader(this);
#ifdef USE_RGB_COLOR
	} else if (_game.platform == Common::kPlatformPCEngine) {
		_costumeRenderer = new PCEngineCostumeRenderer(this);
		_costumeLoader = new ClassicCostumeLoader(this);
#endif
	} else {
		_costumeRenderer = new ClassicCostumeRenderer(this);
		_costumeLoader = new ClassicCostumeLoader(this);
	}
}

void ScummEngine::resetScumm() {
	int i;

	debug(9, "resetScumm");

#ifdef USE_RGB_COLOR
	if (_game.features & GF_16BIT_COLOR
#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
		|| (_game.platform == Common::kPlatformFMTowns)
#endif
		)
		_16BitPalette = (uint16 *)calloc(512, sizeof(uint16));
#endif

	// Indy4 Amiga needs another palette map for the verb area.
	if (_game.platform == Common::kPlatformAmiga && _game.id == GID_INDY4 && !_verbPalette)
		_verbPalette = (uint8 *)calloc(256, 1);

#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
	if (_game.platform == Common::kPlatformFMTowns) {
		delete _townsScreen;
		_scrollRequest = _scrollDeltaAdjust = 0;
		_scrollDestOffset = _scrollTimer = 0;
		_townsScreen = new TownsScreen(_system);
		_townsScreen->setupLayer(0, 512, _screenHeight, _textSurfaceMultiplier, _textSurfaceMultiplier, (_outputPixelFormat.bytesPerPixel == 2) ? 32767 : 256);
		_townsScreen->setupLayer(1, _screenWidth * _textSurfaceMultiplier, _screenHeight * _textSurfaceMultiplier, 1, 1, 16, _textPalette);
	}
#endif

	if (_macScreen) {
		_macScreen->fillRect(Common::Rect(_macScreen->w, _macScreen->h), 0);
	}

	if (_macIndy3TextBox) {
		_macIndy3TextBox->fillRect(Common::Rect(_macIndy3TextBox->w, _macIndy3TextBox->h), 0);
	}

	if (_game.version == 0) {
		initScreens(8, 144);
	} else if ((_game.id == GID_MANIAC) && (_game.version <= 1) && !(_game.platform == Common::kPlatformNES)) {
		initScreens(16, 152);
	} else if (_game.version >= 7 || _game.heversion >= 71) {
		initScreens(0, _screenHeight);
	} else {
		initScreens(16, 144);
	}

	_palManipCounter = 0;

	for (i = 0; i < 256; i++)
		_roomPalette[i] = i;

	resetPalette();
	if (_game.version == 1) {
	} else if (_game.features & GF_16COLOR) {
		for (i = 0; i < 16; i++)
			_shadowPalette[i] = i;
	}

	if (_game.version >= 4 && _game.version <= 7)
		loadCharset(1);

	if (_game.features & GF_OLD_BUNDLE)
		loadCharset(0);

	setShake(0);
	_cursor.animate = 1;

	// Allocate and Initialize actors
	Actor::kInvalidBox = ((_game.features & GF_SMALL_HEADER) ? kOldInvalidBox : kNewInavlidBox);
	_actors = new Actor * [_numActors];
	_sortedActors = new Actor * [_numActors];
	for (i = 0; i < _numActors; ++i) {
		if (_game.version == 0)
			_actors[i] = new Actor_v0(this, i);
		else if (_game.version <= 2)
			_actors[i] = new Actor_v2(this, i);
		else if (_game.version == 3)
			_actors[i] = new Actor_v3(this, i);
		else if (_game.heversion != 0)
			_actors[i] = new ActorHE(this, i);
		else
			_actors[i] = new Actor(this, i);
		_actors[i]->initActor(-1);

		// this is from IDB
		if ((_game.version <= 1) || (_game.id == GID_MANIAC && (_game.features & GF_DEMO)))
			_actors[i]->setActorCostume(i);
	}

	if (_game.id == GID_MANIAC && _game.version <= 1) {
		resetV1ActorTalkColor();
	} else if (_game.id == GID_MANIAC && _game.version == 2 && (_game.features & GF_DEMO)) {
		// HACK Some palette changes needed for demo script
		// in Maniac Mansion (Enhanced)
		_actors[3]->setPalette(3, 1);
		_actors[9]->_talkColor = 15;
		_actors[10]->_talkColor = 7;
		_actors[11]->_talkColor = 2;
		_actors[13]->_talkColor = 5;
		_actors[23]->_talkColor = 14;
	}

	vm.numNestedScripts = 0;
	vm.cutSceneStackPointer = 0;

	memset(vm.cutScenePtr, 0, sizeof(vm.cutScenePtr));
	memset(vm.cutSceneData, 0, sizeof(vm.cutSceneData));

	for (i = 0; i < _numVerbs; i++) {
		_verbs[i].verbid = 0;
		_verbs[i].curRect.right = _screenWidth - 1;
		_verbs[i].oldRect.left = -1;
		_verbs[i].type = 0;
		_verbs[i].color = 2;
		_verbs[i].hicolor = 0;
		_verbs[i].charset_nr = 1;
		_verbs[i].curmode = 0;
		_verbs[i].saveid = 0;
		_verbs[i].center = 0;
		_verbs[i].key = 0;
	}

	if (_game.version >= 7) {
		VAR(VAR_CAMERA_THRESHOLD_X) = 100;
		VAR(VAR_CAMERA_THRESHOLD_Y) = 70;
		VAR(VAR_CAMERA_ACCEL_X) = 100;
		VAR(VAR_CAMERA_ACCEL_Y) = 100;
	} else {
		if (_game.platform == Common::kPlatformNES) {
			camera._leftTrigger = 6;	// 6
			camera._rightTrigger = 21;	// 25
		} else {
			camera._leftTrigger = 10;
			camera._rightTrigger = (_game.heversion >= 71) ? 70 : 30;
		}
		camera._mode = 0;
	}
	camera._follows = 0;

	_virtscr[0].xstart = 0;

	_mouse.x = 104;
	_mouse.y = 56;

	_ENCD_offs = 0;
	_EXCD_offs = 0;

	_currentScript = 0xFF;
	_sentenceNum = 0;

	_currentRoom = 0;
	_numObjectsInRoom = 0;
	_actorToPrintStrFor = 0;

	_charsetBufPos = 0;
	_haveMsg = 0;
	_haveActorSpeechMsg = false;

	_varwatch = -1;
	_screenStartStrip = 0;

	_defaultTalkDelay = 3;
	_talkDelay = 0;
	_keepText = false;
	_nextLeft = 0;
	_nextTop = 0;

	_currentCursor = 0;
	_cursor.state = 0;
	_userPut = 0;

	_newEffect = 129;
	_fullRedraw = true;

	clearDrawObjectQueue();

	if (_game.platform == Common::kPlatformNES)
		decodeNESBaseTiles();

	for (i = 0; i < 6; i++) {
		if (_game.version == 3) { // FIXME - what is this?
			_string[i]._default.xpos = 0;
			_string[i]._default.ypos = 0;
		} else {
			_string[i]._default.xpos = 2;
			_string[i]._default.ypos = 5;
		}
		_string[i]._default.right = _screenWidth - 1;
		_string[i]._default.height = 0;
		_string[i]._default.color = 0xF;
		_string[i]._default.center = 0;
		_string[i]._default.charset = 0;
	}

	// all keys are released
	for (i = 0; i < 512; i++)
		_keyDownMap[i] = false;

	_lastSaveTime = _system->getMillis();
}

void ScummEngine_v0::resetScumm() {
	ScummEngine_v2::resetScumm();
	resetVerbs();
}

void ScummEngine_v2::resetScumm() {
	ScummEngine_v3::resetScumm();

	if (_game.platform == Common::kPlatformNES) {
		initNESMouseOver();
		_switchRoomEffect2 = _switchRoomEffect = 6;
	} else {
		initV2MouseOver();
		// Seems in V2 there was only a single room effect (iris),
		// so we set that here.
		_switchRoomEffect2 = 1;
		_switchRoomEffect = 5;
	}

	_inventoryOffset = 0;
}

void ScummEngine_v3::resetScumm() {
	ScummEngine_v4::resetScumm();


	if (_game.id == GID_LOOM && _game.platform == Common::kPlatformPCEngine) {
		// Load tile set and palette for the distaff
		byte *roomptr = getResourceAddress(rtRoom, 90);
		assert(roomptr);
		const byte *palPtr = findResourceData(MKTAG('C','L','U','T'), roomptr);
		assert(palPtr - 4);
		setPCEPaletteFromPtr(palPtr);
		_gdi->_distaff = true;
		_gdi->loadTiles(roomptr);
		_gdi->_distaff = false;
	}

	delete _savePreparedSavegame;
	_savePreparedSavegame = NULL;
}

void ScummEngine_v4::resetScumm() {
	ScummEngine::resetScumm();

	// WORKAROUND for bug in boot script of Loom (CD)
	// The boot script sets the characters of string 21,
	// before creating the string.resource.
	if (_game.id == GID_LOOM) {
		_res->createResource(rtString, 21, 12);
	}
}

void ScummEngine_v6::resetScumm() {
	ScummEngine::resetScumm();
	setDefaultCursor();
}

void ScummEngine_v60he::resetScumm() {
	ScummEngine_v6::resetScumm();

	// HACK cursor hotspot is wrong
	// Original games used
	// setCursorHotspot(8, 7);
	if (_game.id == GID_FUNPACK)
		setCursorHotspot(16, 16);
}

#ifdef ENABLE_HE
void ScummEngine_v72he::resetScumm() {
	ScummEngine_v60he::resetScumm();

	_stringLength = 1;
	memset(_stringBuffer, 0, sizeof(_stringBuffer));
}

void ScummEngine_v90he::resetScumm() {
	ScummEngine_v72he::resetScumm();

	_heObject = 0;
	_heObjectNum = 0;
	_hePaletteNum = 0;

	_sprite->resetTables(0);
	_wizParams.reset();

	if (_game.heversion >= 98)
		_logicHE = LogicHE::makeLogicHE(this);
}

void ScummEngine_v99he::resetScumm() {
	byte *data;
	Common::String ininame = _targetName + ".ini";
	int len;

	ScummEngine_v90he::resetScumm();

	_hePaletteSlot = (_game.features & GF_16BIT_COLOR) ? 1280 : 1024;
	_hePalettes = (uint8 *)malloc((_numPalettes + 1) * _hePaletteSlot);
	memset(_hePalettes, 0, (_numPalettes + 1) * _hePaletteSlot);

	// Array 129 is set to base name
	len = strlen(_filenamePattern.pattern);
	data = defineArray(129, kStringArray, 0, 0, 0, len);
	memcpy(data, _filenamePattern.pattern, len);

	// Array 132 is set to game path
	data = defineArray(132, kStringArray, 0, 0, 0, 0);

	// Array 137 is set to Windows directory, plus INI file
	len = strlen(ininame.c_str());
	data = defineArray(137, kStringArray, 0, 0, 0, len);
	memcpy(data, ininame.c_str(), len);
}

void ScummEngine_v100he::resetScumm() {
	ScummEngine_v99he::resetScumm();

	memset(_debugInputBuffer, 0, sizeof(_debugInputBuffer));
}
#endif

void ScummEngine::setupMusic(int midi, const Common::String &macInstrumentFile) {
	MidiDriver::DeviceHandle dev = MidiDriver::detectDevice(midi);
	_native_mt32 = ((MidiDriver::getMusicType(dev) == MT_MT32) || ConfMan.getBool("native_mt32"));

	switch (MidiDriver::getMusicType(dev)) {
	case MT_NULL:
		_sound->_musicType = MDT_NONE;
		break;
	case MT_AMIGA:
		_sound->_musicType = MDT_AMIGA;
		break;
	case MT_PCSPK:
		_sound->_musicType = MDT_PCSPK;
		break;
	case MT_PCJR:
		_sound->_musicType = MDT_PCJR;
		break;
	case MT_CMS:
		_sound->_musicType = MDT_CMS;
		break;
	case MT_TOWNS:
		_sound->_musicType = MDT_TOWNS;
		break;
	case MT_ADLIB:
		_sound->_musicType = MDT_ADLIB;
		break;
	case MT_C64:
		_sound->_musicType = MDT_C64;
		break;
	case MT_APPLEIIGS:
		_sound->_musicType = MDT_APPLEIIGS;
		break;
	default:
		_sound->_musicType = MDT_MIDI;
		break;
	}

	if ((_game.id == GID_MONKEY_EGA || (_game.id == GID_LOOM && _game.version == 3))
	   &&  (_game.platform == Common::kPlatformDOS) && _sound->_musicType == MDT_MIDI) {
		Common::String fileName;
		bool missingFile = false;
		if (_game.id == GID_LOOM) {
			Common::File f;
			// The Roland Update does have an 85.LFL, but we don't
			// test for it since the demo doesn't have it.
			for (char c = '2'; c <= '4'; c++) {
				fileName = "8";
				fileName += c;
				fileName += ".LFL";
				if (!Common::File::exists(fileName)) {
					missingFile = true;
					break;
				}
			}
		} else if (_game.id == GID_MONKEY_EGA) {
			fileName = "DISK09.LEC";
			if (!Common::File::exists(fileName)) {
				missingFile = true;
			}
		}

		if (missingFile) {
			GUI::MessageDialog dialog(
				Common::U32String::format(
					_("Native MIDI support requires the Roland Upgrade from LucasArts,\n"
					"but %s is missing. Using AdLib instead."), fileName.c_str()),
				_("OK"));
			dialog.runModal();
			_sound->_musicType = MDT_ADLIB;
		}
	}

	if (_game.platform == Common::kPlatformMacintosh && (_game.id == GID_MONKEY2 || _game.id == GID_INDY4)) {
		// While the Mac versions do have ADL resources, the Mac player
		// doesn't handle them. So if a song is missing a MAC resource,
		// prefer the ROL version over ADL.
		//
		// This is the case in Monkey Island 2, where some key music is
		// missing near the end of the game: The Indiana Jones fanfare
		// when Guybrush uses the rope to get the chest, and the music
		// after the first LeChuck encounter in the underground tunnels
		// below that scene. As well as some others that I haven't
		// identified.
		//
		// Note that this does not seem to be a ScummVM bug. That music
		// was missing when I ran the game in a Mac emulator too!
		//
		// ScummVM would play the ROL music instead, but only if it
		// didn't think it was  using an AdLib music driver. Even if
		// (as in my case) it was only by default. Now we always set
		// MDT_MIDI to ensure consistent behavior. The Mac instrument
		// set isn't quite the same as the MT-32, but it looks like it
		// was based on a subset of it.
		//
		// From what I've seen, when a resource has a Mac version that
		// is all that it has. So there shouldn't be any case where it
		// prefers a ROL resource over MAC.
		//
		// Adding AdLib capabilities to the player may still be a good
		// idea, because there are plenty of sound resources that exist
		// only as ADL and SPK.
		_sound->_musicType = MDT_MIDI;
	}

	// DOTT + SAM use General MIDI, so they shouldn't use GS settings
	if ((_game.id == GID_TENTACLE) || (_game.id == GID_SAMNMAX))
		_enable_gs = false;
	else
		_enable_gs = ConfMan.getBool("enable_gs");

	/* Bind the mixer to the system => mixer will be invoked
	 * automatically when samples need to be generated */
	if (!_mixer->isReady()) {
		warning("Sound mixer initialization failed");
		if (_sound->_musicType == MDT_ADLIB || _sound->_musicType == MDT_PCSPK || _sound->_musicType == MDT_PCJR || _sound->_musicType == MDT_CMS) {
			dev = 0;
			_sound->_musicType = MDT_NONE;
			warning("MIDI driver depends on sound mixer, switching to null MIDI driver");
		}
	}

	// Init iMuse
	if (_game.version >= 7) {
		// Setup for digital iMuse is performed in another place
	} else if (_game.platform == Common::kPlatformApple2GS && _game.version == 0){
		_musicEngine = new Player_AppleII(this, _mixer);
	} else if (_game.platform == Common::kPlatformC64 && _game.version <= 1) {
#ifndef DISABLE_SID
		_musicEngine = new Player_SID(this, _mixer);
#endif
	} else if (_game.platform == Common::kPlatformNES && _game.version == 1) {
#ifndef DISABLE_NES_APU
		_musicEngine = new Player_NES(this, _mixer);
#endif
	} else if (_game.platform == Common::kPlatformAmiga && _game.version == 2) {
		_musicEngine = new Player_V2A(this, _mixer);
	} else if (_game.platform == Common::kPlatformAmiga && _game.version == 3) {
		_musicEngine = new Player_V3A(this, _mixer);
#ifdef USE_RGB_COLOR
	} else if (_game.platform == Common::kPlatformPCEngine && _game.version == 3) {
		_musicEngine = new Player_PCE(this, _mixer);
#endif
	} else if (_game.platform == Common::kPlatformAmiga && _game.version <= 4) {
		_musicEngine = new Player_V4A(this, _mixer);
	} else if (_game.platform == Common::kPlatformMacintosh && _game.id == GID_LOOM) {
		_musicEngine = new Player_V3M(this, _mixer, ConfMan.getBool("mac_v3_low_quality_music"));
		((Player_V3M *)_musicEngine)->init(macInstrumentFile);
	} else if (_game.platform == Common::kPlatformMacintosh && _game.id == GID_MONKEY) {
		_musicEngine = new Player_V5M(this, _mixer);
		((Player_V5M *)_musicEngine)->init(macInstrumentFile);
	} else if (_game.id == GID_MANIAC && _game.version == 1) {
		_musicEngine = new Player_V1(this, _mixer, MidiDriver::getMusicType(dev) != MT_PCSPK);
	} else if (_game.version <= 2) {
		_musicEngine = new Player_V2(this, _mixer, MidiDriver::getMusicType(dev) != MT_PCSPK);
	} else if ((_sound->_musicType == MDT_PCSPK || _sound->_musicType == MDT_PCJR) && (_game.version > 2 && _game.version <= 4)) {
		_musicEngine = new Player_V2(this, _mixer, MidiDriver::getMusicType(dev) != MT_PCSPK);
	} else if (_sound->_musicType == MDT_CMS) {
		_musicEngine = new Player_V2CMS(this, _mixer);
	} else if (_game.platform == Common::kPlatform3DO && _game.heversion <= 62) {
		// 3DO versions use digital music and sound samples.
	} else if (_game.platform == Common::kPlatformFMTowns && (_game.version == 3 || _game.id == GID_MONKEY)) {
		_musicEngine = _townsPlayer = new Player_Towns_v1(this, _mixer);
		if (!_townsPlayer->init())
			error("Failed to initialize FM-Towns audio driver");
	} else if (_game.platform == Common::kPlatformDOS && (_sound->_musicType == MDT_ADLIB) && (_game.id == GID_LOOM || _game.id == GID_INDY3)) {
		// For Indy3 DOS and Loom DOS we use an implementation of the original
		// AD player when AdLib is selected. This fixes sound effects in those
		// games (see for example bug #3830 "INDY3: Non-Looping Sound
		// Effects"). The player itself is also used in Monkey Island DOS
		// EGA/VGA. However, we support multi MIDI for that game and we cannot
		// support this with the Player_AD code at the moment. The reason here
		// is that multi MIDI is supported internally by our iMuse output.
		_musicEngine = new Player_AD(this, _mixer->mutex());
#ifdef ENABLE_HE
	} else if (_game.platform == Common::kPlatformDOS && _sound->_musicType == MDT_ADLIB && _game.heversion >= 60) {
		_musicEngine = new Player_HE(this);
#endif
	} else if (_game.version >= 3 && _game.heversion <= 62) {
		MidiDriver *nativeMidiDriver = 0;
		MidiDriver *adlibMidiDriver = 0;
		bool multi_midi = ConfMan.getBool("multi_midi") && _sound->_musicType != MDT_NONE && _sound->_musicType != MDT_PCSPK && (midi & MDT_ADLIB);
		bool useOnlyNative = false;

		if (isMacM68kIMuse()) {
			// We setup this driver as native MIDI driver to avoid playback
			// of the Mac music via a selected MIDI device.
			nativeMidiDriver = new MacM68kDriver(_mixer);
			// The Mac driver is never MT-32.
			_native_mt32 = false;
			// Ignore non-native drivers. This also ignores the multi MIDI setting.
			useOnlyNative = true;
		} else if (_sound->_musicType == MDT_AMIGA) {
			nativeMidiDriver = new IMuseDriver_Amiga(_mixer);
			_native_mt32 = _enable_gs = false;
			useOnlyNative = true;
		} else if (_sound->_musicType != MDT_ADLIB && _sound->_musicType != MDT_TOWNS && _sound->_musicType != MDT_PCSPK) {
			nativeMidiDriver = MidiDriver::createMidi(dev);
		}

		if (nativeMidiDriver != NULL && _native_mt32)
			nativeMidiDriver->property(MidiDriver::PROP_CHANNEL_MASK, 0x03FE);

		if (!useOnlyNative) {
			if (_sound->_musicType == MDT_TOWNS) {
				adlibMidiDriver = new MidiDriver_TOWNS(_mixer);
			} else if (_sound->_musicType == MDT_ADLIB || multi_midi) {
				adlibMidiDriver = MidiDriver::createMidi(MidiDriver::detectDevice(_sound->_musicType == MDT_TOWNS ? MDT_TOWNS : MDT_ADLIB));
				adlibMidiDriver->property(MidiDriver::PROP_OLD_ADLIB, (_game.features & GF_SMALL_HEADER) ? 1 : 0);
				// Try to use OPL3 mode for Sam&Max when possible.
				adlibMidiDriver->property(MidiDriver::PROP_SCUMM_OPL3, (_game.id == GID_SAMNMAX) ? 1 : 0);
			} else if (_sound->_musicType == MDT_PCSPK) {
				adlibMidiDriver = new PcSpkDriver(_mixer);
			}
		}

		_imuse = IMuse::create(_system, nativeMidiDriver, adlibMidiDriver);

		if (_game.platform == Common::kPlatformFMTowns) {
			_musicEngine = _townsPlayer = new Player_Towns_v2(this, _mixer, _imuse, true);
			if (!_townsPlayer->init())
				error("ScummEngine::setupMusic(): Failed to initialize FM-Towns audio driver");
		} else {
			_musicEngine = _imuse;
		}

		if (_imuse) {
			_imuse->addSysexHandler
				(/*IMUSE_SYSEX_ID*/ 0x7D,
				 (_game.id == GID_SAMNMAX) ? sysexHandler_SamNMax : sysexHandler_Scumm);
			_imuse->property(IMuse::PROP_GAME_ID, _game.id);
			if (ConfMan.hasKey("tempo"))
				_imuse->property(IMuse::PROP_TEMPO_BASE, ConfMan.getInt("tempo"));
			if (midi != MDT_NONE) {
				_imuse->property(IMuse::PROP_NATIVE_MT32, _native_mt32);
				if (MidiDriver::getMusicType(dev) != MT_MT32) // MT-32 Emulation shouldn't be GM/GS initialized
					_imuse->property(IMuse::PROP_GS, _enable_gs);
			}
			if (_game.heversion >= 60) {
				_imuse->property(IMuse::PROP_LIMIT_PLAYERS, 1);
				_imuse->property(IMuse::PROP_RECYCLE_PLAYERS, 1);
			}
			if (_sound->_musicType == MDT_PCSPK)
				_imuse->property(IMuse::PROP_PC_SPEAKER, 1);
			if (_sound->_musicType == MDT_AMIGA)
				_imuse->property(IMuse::PROP_AMIGA, 1);
		}
	}
}

void ScummEngine::syncSoundSettings() {
	Engine::syncSoundSettings();

	// Sync the engine with the config manager
	int soundVolumeMusic = ConfMan.getInt("music_volume");
	int soundVolumeSfx = ConfMan.getInt("sfx_volume");

	bool mute = false;

	if (ConfMan.hasKey("mute")) {
		mute = ConfMan.getBool("mute");

		if (mute)
			soundVolumeMusic = soundVolumeSfx = 0;
	}

	if (_musicEngine) {
		_musicEngine->setMusicVolume(soundVolumeMusic);
	}

	if (_townsPlayer) {
		_townsPlayer->setSfxVolume(soundVolumeSfx);
	}

	if (ConfMan.getBool("speech_mute"))
		_voiceMode = 2;
	else
		_voiceMode = ConfMan.getBool("subtitles");

	if (VAR_VOICE_MODE != 0xFF)
		VAR(VAR_VOICE_MODE) = _voiceMode;

	if (ConfMan.hasKey("talkspeed", _targetName)) {
		_defaultTalkDelay = getTalkSpeed();
		if (VAR_CHARINC != 0xFF)
			VAR(VAR_CHARINC) = _defaultTalkDelay;
	}

	// Backyard Baseball 2003 uses a unique subtitle variable,
	// rather than VAR_SUBTITLES
	if (_game.id == GID_BASEBALL2003) {
		_scummVars[632] = ConfMan.getBool("subtitles");
	}

}

void ScummEngine::setTalkSpeed(int talkspeed) {
	ConfMan.setInt("talkspeed", (talkspeed * 255 + 9 / 2) / 9);
}

int ScummEngine::getTalkSpeed() {
	return (ConfMan.getInt("talkspeed") * 9 + 255 / 2) / 255;
}


#pragma mark -
#pragma mark --- Main loop ---
#pragma mark -

Common::Error ScummEngine::go() {
	setTotalPlayTime();

	// If requested, load a save game instead of running the boot script
	if (_saveLoadFlag != 2 || !loadState(_saveLoadSlot, _saveTemporaryState)) {
		_saveLoadFlag = 0;
		runBootscript();
	} else {
		_saveLoadFlag = 0;
	}

	int diff = 0;	// Duration of one loop iteration

	while (!shouldQuit()) {
		// Randomize the PRNG by calling it at regular intervals. This ensures
		// that it will be in a different state each time you run the program.
		_rnd.getRandomNumber(2);

		// Notify the script about how much time has passed, in ticks (60 ticks per second)
		if (VAR_TIMER != 0xFF)
			VAR(VAR_TIMER) = diff * 60 / 1000;
		if (VAR_TIMER_TOTAL != 0xFF)
			VAR(VAR_TIMER_TOTAL) += diff * 60 / 1000;

		// Determine how long to wait before the next loop iteration should start
		int delta = (VAR_TIMER_NEXT != 0xFF) ? VAR(VAR_TIMER_NEXT) : 4;
#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
		// FM-Towns only. The original has a mechanism to let the scrolling catch up to the engine. This avoids glitches, e. g.
		// when the engine draws actors or objects to the far left/right of the screen while the scrolling hasn't caught up yet.
		// MI2 FM-Towns normally adds an amount of 4 to a counter on each 60 Hz tick from inside an interrupt handler, but only
		// an amount of 3 while the smooth scrolling is in progress. The counter divided by 4 has to reach the VAR_TIMER_NEXT
		// before the main loop continues. We try to imitate that behaviour here to avoid glitches, but without making it
		// overly complicated...
		if (_scrollDeltaAdjust) {
			delta = MAX<int>(0, delta - _scrollDeltaAdjust) + (MIN<int>(_scrollDeltaAdjust, delta) << 2) / 3;
			_scrollDeltaAdjust = 0;
		}
#endif
		if (delta < 1)	// Ensure we don't get into an endless loop
			delta = 1;  // by not decreasing sleepers.

		// WORKAROUND: Unfortunately the MOS 6502 wasn't always fast enough for MM
		//  a number of situations can lead to the engine running at less than 60 ticks per second, without this drop
		//	- A single kid is able to escape via the Dungeon Door (after pushing the brick)
		//	- During the intro, calls to 'SetState08' are made for the lights on the mansion, with a 'breakHere'
		//	  in between each, the reduction in ticks then occurs while affected stripes are being redrawn.
		//	  The music buildup is then out of sync with the text "A Lucasfilm Games Production".
		//	  Each call to 'breakHere' has been replaced with calls to 'Delay' in the V1/V2 versions of the game
		if (_game.version == 0) {
			delta += ((ScummEngine_v0 *)this)->DelayCalculateDelta();
		}

		// WORKAROUND: walking speed in the original v1 interpreter
		// is sometimes slower (e.g. during scrolling) than in ScummVM.
		// This is important for the door-closing action in the dungeon,
		// otherwise (delta < 6) a single kid is able to escape.
		if (_game.version == 1 && isScriptRunning(137)) {
				delta = 6;
		}

		// Wait...
		waitForTimer(delta * 1000 / 60 - diff);

		// Start the stop watch!
		diff = _system->getMillis();

		// Run the main loop
		scummLoop(delta);

		// Halt the stop watch and compute how much time this iteration took.
		diff = _system->getMillis() - diff;


		if (shouldQuit()) {
			// TODO: Maybe perform an autosave on exit?
			runQuitScript();
		}
	}

	return Common::kNoError;
}

void ScummEngine::waitForTimer(int msec_delay) {
	uint32 start_time;

	if (_fastMode & 2)
		msec_delay = 0;
	else if (_fastMode & 1)
		msec_delay = 10;

	start_time = _system->getMillis();

	while (!shouldQuit()) {
		_sound->updateCD(); // Loop CD Audio if needed
		parseEvents();

#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
		uint32 screenUpdateTimerStart = _system->getMillis();
		towns_updateGfx();
#endif
		_system->updateScreen();
		uint32 cur = _system->getMillis();

#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
		// These measurements are used to determine whether the FM-Towns smooth scrolling is likely to fall behind and need to catch
		// up (becoming more sloppy than smooth). Calls to _system->updateScreen() can require way longer than a 60Hz tick, depending
		// on the hardware and the filter setting. In fact, these calls can take way over 100ms for some unfortunate configs.
		_refreshDuration[_refreshArrayPos] = (int)(cur - screenUpdateTimerStart);
		_refreshArrayPos = (_refreshArrayPos + 1) % ARRAYSIZE(_refreshDuration);
#endif
		if (cur >= start_time + msec_delay)
			break;
		_system->delayMillis(10);
	}
}

void ScummEngine_v0::scummLoop(int delta) {
	VAR(VAR_IS_SOUND_RUNNING) = (_sound->_lastSound && _sound->isSoundRunning(_sound->_lastSound) != 0);

	ScummEngine::scummLoop(delta);
}

void ScummEngine::scummLoop(int delta) {
	if (_game.version >= 3) {
		VAR(VAR_TMR_1) += delta;
		VAR(VAR_TMR_2) += delta;
		VAR(VAR_TMR_3) += delta;
		if ((_game.id == GID_INDY3 && _game.platform != Common::kPlatformMacintosh) ||
			_game.id == GID_ZAK) {
			// Amiga/PC versions of Indy3 set three extra timers
			// FM-TOWNS version of Zak sets three extra timers
			VAR(39) += delta;
			VAR(40) += delta;
			VAR(41) += delta;
		}
	}

	if (delta > 15)
		delta = 15;

	decreaseScriptDelay(delta);

	_talkDelay -= delta;
	if (_talkDelay < 0)
		_talkDelay = 0;

	// Record the current ego actor before any scripts (including input scripts)
	// get a chance to run.
	int oldEgo = 0;
	if (VAR_EGO != 0xFF)
		oldEgo = VAR(VAR_EGO);

	// In V1-V3 games, CHARSET_1 is called much earlier than in newer games.
	// See also bug #987 for a case were this makes a difference.
	if (_game.version <= 3)
		CHARSET_1();

	processInput();

	scummLoop_updateScummVars();

	if (_game.features & GF_AUDIOTRACKS) {
		// Covered automatically by the Sound class
	} else if (VAR_MUSIC_TIMER != 0xFF) {
		if (_musicEngine) {
			// The music engine generates the timer data for us.
			VAR(VAR_MUSIC_TIMER) = _musicEngine->getMusicTimer();
		}
	}

	if (VAR_GAME_LOADED != 0xFF)
		VAR(VAR_GAME_LOADED) = 0;
load_game:
	scummLoop_handleSaveLoad();

	if (_completeScreenRedraw) {
		clearCharsetMask();
		_charset->_hasMask = false;

		// HACK as in game save stuff isn't supported currently
		if (_game.id == GID_LOOM) {
			int args[NUM_SCRIPT_LOCAL];
			uint var;
			memset(args, 0, sizeof(args));
			args[0] = 2;

			if (_game.platform == Common::kPlatformMacintosh)
				var = 105;
			// 256 color CD version and PC engine version
			else if (_game.platform == Common::kPlatformPCEngine || _game.version == 4)
				var = 150;
			else
				var = 100;
			byte restoreScript = (_game.platform == Common::kPlatformFMTowns) ? 17 : 18;
			// if verbs should be shown restore them
			if (VAR(var) == 2)
				runScript(restoreScript, 0, 0, args);
		} else if (_game.version > 3) {
			for (int i = 0; i < _numVerbs; i++)
				drawVerb(i, 0);
		} else {
			redrawVerbs();
		}

		handleMouseOver(false);

		_completeScreenRedraw = false;
		_fullRedraw = true;
	}

	if (_game.heversion >= 80) {
		((SoundHE *)_sound)->processSoundCode();
	}
	runAllScripts();
	checkExecVerbs();
	checkAndRunSentenceScript();

	if (shouldQuit())
		return;

	// HACK: If a load was requested, immediately perform it. This avoids
	// drawing the current room right after the load is request but before
	// it is performed. That was annoying esp. if you loaded while a SMUSH
	// cutscene was playing.
	if (_saveLoadFlag && _saveLoadFlag != 1) {
		goto load_game;
	}

#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
	towns_processPalCycleField();
#endif

	if (_currentRoom == 0) {
		if (_game.version > 3)
			CHARSET_1();
		drawDirtyScreenParts();
	} else {
		walkActors();
		moveCamera();
		updateObjectStates();
		if (_game.version > 3)
			CHARSET_1();

		scummLoop_handleDrawing();

		scummLoop_handleActors();

		_fullRedraw = false;

		scummLoop_handleEffects();

		if (VAR_MAIN_SCRIPT != 0xFF && VAR(VAR_MAIN_SCRIPT) != 0) {
			runScript(VAR(VAR_MAIN_SCRIPT), 0, 0, 0);
		}

		// Handle mouse over effects (for verbs).
		handleMouseOver(oldEgo != VAR(VAR_EGO));

		// Render everything to the screen.
		updatePalette();
		drawDirtyScreenParts();

		// FIXME / TODO: Try to move the following to scummLoop_handleSound or
		// scummLoop_handleActors (but watch out for regressions!)
		if (_game.version <= 5)
			playActorSounds();
	}

	scummLoop_handleSound();

	camera._last = camera._cur;

	_res->increaseExpireCounter();

	animateCursor();

	/* show or hide mouse */
	CursorMan.showMouse(_cursor.state > 0);
}

#ifdef ENABLE_HE
void ScummEngine_v90he::scummLoop(int delta) {
	_moviePlay->handleNextFrame();
	if (_game.heversion >= 98) {
		_logicHE->startOfFrame();
	}

	ScummEngine::scummLoop(delta);

	_sprite->updateImages();
	if (_game.heversion >= 98) {
		_logicHE->endOfFrame();
	}
}
#endif

void ScummEngine::scummLoop_updateScummVars() {
	if (_game.version >= 7) {
		VAR(VAR_CAMERA_POS_X) = camera._cur.x;
		VAR(VAR_CAMERA_POS_Y) = camera._cur.y;
	} else if (_game.platform == Common::kPlatformNES) {
#if 0
		// WORKAROUND:
		// Since there are 2 2-stripes wide borders in MM NES screen,
		// we have to compensate for it here. This fixes paning effects.
		// Fixes bug #2266: "MANIACNES: Screen width incorrect, camera halts sometimes"
		// But do not do it when only scrolling right to left, since otherwise Ed will not show
		// up on the doorbell (Bug #5126)
		if (VAR(VAR_CAMERA_POS_X) < (camera._cur.x >> V12_X_SHIFT) + 2)
			VAR(VAR_CAMERA_POS_X) = (camera._cur.x >> V12_X_SHIFT) + 2;
		else
#endif
			VAR(VAR_CAMERA_POS_X) = (camera._cur.x >> V12_X_SHIFT);
	} else if (_game.version <= 2) {
		VAR(VAR_CAMERA_POS_X) = camera._cur.x >> V12_X_SHIFT;
	} else {
		VAR(VAR_CAMERA_POS_X) = camera._cur.x;
	}
	if (_game.version <= 7)
		VAR(VAR_HAVE_MSG) = _haveMsg;

	if (_game.version >= 3) {
		VAR(VAR_VIRT_MOUSE_X) = _virtualMouse.x;
		VAR(VAR_VIRT_MOUSE_Y) = _virtualMouse.y;
		VAR(VAR_MOUSE_X) = _mouse.x;
		VAR(VAR_MOUSE_Y) = _mouse.y;
		if (VAR_DEBUGMODE != 0xFF) {
			// This is NOT for the Mac version of Indy3/Loom
			VAR(VAR_DEBUGMODE) = (_debugMode ? 1 : 0);
		}
	} else if (_game.version >= 1) {
		// We use shifts below instead of dividing by V12_X_MULTIPLIER resp.
		// V12_Y_MULTIPLIER to handle negative coordinates correctly.
		// This fixes e.g. bugs #2268 and #2777.
		VAR(VAR_VIRT_MOUSE_X) = _virtualMouse.x >> V12_X_SHIFT;
		VAR(VAR_VIRT_MOUSE_Y) = _virtualMouse.y >> V12_Y_SHIFT;

		// Adjust mouse coordinates as narrow rooms in NES are centered
		if (_game.platform == Common::kPlatformNES && _NESStartStrip > 0) {
			VAR(VAR_VIRT_MOUSE_X) -= 2;
			if (VAR(VAR_VIRT_MOUSE_X) < 0)
				VAR(VAR_VIRT_MOUSE_X) = 0;
		}
	}
}

void ScummEngine::scummLoop_handleSaveLoad() {
	if (_saveLoadFlag) {
		bool success;
		Common::U32String errMsg;

		if (_game.version == 8 && _saveTemporaryState)
			VAR(VAR_GAME_LOADED) = 0;

		Common::String filename;
		if (_saveLoadFlag == 1) {
			success = saveState(_saveLoadSlot, _saveTemporaryState, filename);
			if (!success)
				errMsg = _("Failed to save game to file:\n\n%s");

			if (success && _saveTemporaryState && VAR_GAME_LOADED != 0xFF && _game.version <= 7)
				VAR(VAR_GAME_LOADED) = 201;

			if (!_saveTemporaryState)
				_lastSaveTime = _system->getMillis();
		} else {
			success = loadState(_saveLoadSlot, _saveTemporaryState, filename);
			if (!success)
				errMsg = _("Failed to load saved game from file:\n\n%s");

			if (success && _saveTemporaryState && VAR_GAME_LOADED != 0xFF)
				VAR(VAR_GAME_LOADED) = (_game.version == 8) ? 1 : 203;
		}

		if (!success) {
			Common::U32String buf = Common::U32String::format(errMsg, filename.c_str());

			GUI::MessageDialog dialog(buf);
			runDialog(dialog);
		} else if (_saveLoadFlag == 1 && _saveLoadSlot != 0 && !_saveTemporaryState) {
			// Display "Save successful" message, except for auto saves
			Common::U32String buf = Common::U32String::format(_("Successfully saved game in file:\n\n%s"), filename.c_str());

			GUI::TimedMessageDialog dialog(buf, 1500);
			runDialog(dialog);
		}
		if (success && _saveLoadFlag != 1)
			clearClickedStatus();

		_saveLoadFlag = 0;
	}
}

void ScummEngine_v4::scummLoop_handleSaveLoad() {
	// copy saveLoadFlag as handleSaveLoad() resets it
	byte saveLoad = _saveLoadFlag;

	ScummEngine_v5::scummLoop_handleSaveLoad();

	// update IQ points after loading
	if (saveLoad == 2) {
		if (_game.id == GID_INDY3)
			updateIQPoints();
	}
}

void ScummEngine_v5::scummLoop_handleSaveLoad() {
	// copy saveLoadFlag as handleSaveLoad() resets it
	byte saveLoad = _saveLoadFlag;

	ScummEngine::scummLoop_handleSaveLoad();

	// update IQ points after loading
	if (saveLoad == 2) {
		if (_game.id == GID_INDY4)
			runScript(145, 0, 0, 0);
	}
}

#ifdef ENABLE_SCUMM_7_8
void ScummEngine_v8::scummLoop_handleSaveLoad() {
	ScummEngine::scummLoop_handleSaveLoad();

	removeBlastObjects();
}
#endif

void ScummEngine::scummLoop_handleDrawing() {
	if (camera._cur != camera._last || _bgNeedsRedraw || _fullRedraw) {
		_V0Delay._screenScroll = true;

		redrawBGAreas();
	}

	processDrawQue();
}

#ifdef ENABLE_SCUMM_7_8
void ScummEngine_v7::scummLoop_handleDrawing() {
	ScummEngine_v6::scummLoop_handleDrawing();

	// Full Throttle always redraws verbs and draws verbs before actors
	if (_game.version >= 7)
		redrawVerbs();
}
#endif

#ifdef ENABLE_HE
void ScummEngine_v90he::scummLoop_handleDrawing() {
	ScummEngine_v80he::scummLoop_handleDrawing();

	if (_game.heversion >= 99)
		_fullRedraw = false;

	if (_game.heversion >= 90) {
		_sprite->resetBackground();
		_sprite->sortActiveSprites();
	}
}
#endif

void ScummEngine_v6::scummLoop_handleActors() {
	setActorRedrawFlags();
	resetActorBgs();
	processActors();
}

void ScummEngine_v5::scummLoop_handleActors() {
	setActorRedrawFlags();
	resetActorBgs();

	if (!(getCurrentLights() & LIGHTMODE_room_lights_on) &&
		  getCurrentLights() & LIGHTMODE_flashlight_on) {
		drawFlashlight();
		setActorRedrawFlags();
	}

	processActors();
}

void ScummEngine::scummLoop_handleEffects() {
	if (_game.version >= 4 && _game.heversion <= 62)
		cyclePalette();
	palManipulate();
	if (_doEffect) {
		_doEffect = false;
		fadeIn(_newEffect);
		clearClickedStatus();
	}
}

void ScummEngine::scummLoop_handleSound() {
	_sound->processSound();
}

#ifdef ENABLE_SCUMM_7_8
void ScummEngine_v7::scummLoop_handleSound() {
	ScummEngine_v6::scummLoop_handleSound();
	if (_imuseDigital) {
		_imuseDigital->flushTracks();
		// In CoMI and the Dig the full (non-demo) version invoke IMuseDigital::refreshScripts
		if ((_game.id == GID_DIG || _game.id == GID_CMI) && !(_game.features & GF_DEMO))
			_imuseDigital->refreshScripts();
	}
	if (_smixer) {
		_smixer->flush();
	}
}
#endif


#pragma mark -
#pragma mark --- SCUMM ---
#pragma mark -

int ScummEngine_v60he::getHETimer(int timer) {
	assertRange(1, timer, 15, "getHETimer: Timer");
	int time = _system->getMillis() - _heTimers[timer];
	return time;
}

void ScummEngine_v60he::setHETimer(int timer) {
	assertRange(1, timer, 15, "setHETimer: Timer");
	_heTimers[timer] = _system->getMillis();
}

void ScummEngine_v60he::pauseHETimers(bool pause) {
	// The HE timers rely on system time which of course doesn't pause when
	// the engine does. By adding the elapsed time we compensate for this.
	// Fixes bug #6352
	if (pause) {
		// Pauses can be layered, we only need the start of the first
		if (!_pauseStartTime)
			_pauseStartTime = _system->getMillis();
	} else {
		int elapsedTime = _system->getMillis() - _pauseStartTime;
		for (int i = 0; i < ARRAYSIZE(_heTimers); i++) {
			if (_heTimers[i] != 0)
				_heTimers[i] += elapsedTime;
		}
		_pauseStartTime = 0;
	}
}

void ScummEngine_v60he::pauseEngineIntern(bool pause) {
	pauseHETimers(pause);

	ScummEngine::pauseEngineIntern(pause);
}

void ScummEngine::pauseGame() {
	pauseDialog();
}

void ScummEngine::restart() {
	// FIXME: This function *leaks memory*, and quite a lot so. For example,
	// we re-init the resource manager, which causes readMAXS() to be called
	// again, which allocates some memory. There are many other leaks, though.

	// TODO: We should also probably be reinitting a lot more stuff.

	// Fingolfin seez: An alternate way to implement restarting would be to create
	// a save state right after startup ... to this end we could introduce a SaveFile
	// subclass which is implemented using a memory buffer (i.e. no actual file is
	// created). Then to restart we just have to load that pseudo save state.


	int i;

	// Reset some stuff
	_currentRoom = 0;
	_currentScript = 0xFF;
	killAllScriptsExceptCurrent();
	setShake(0);
	_sound->stopAllSounds();

	// Clear the script variables
	for (i = 0; i < _numVariables; i++)
		_scummVars[i] = 0;

	// Empty inventory
	for (i = 1; i < _numGlobalObjects; i++)
		clearOwnerOf(i);

	readIndexFile();

	// Reinit scumm variables
	resetScumm();
	resetScummVars();

	// Reinit sound engine
	if (_game.version >= 5 && _game.version <= 7)
		_sound->setupSound();

	// Re-run bootscript
	runBootscript();
}

void ScummEngine::runBootscript() {
	int args[NUM_SCRIPT_LOCAL];
	memset(args, 0, sizeof(args));

	// There are two known versions of Monkey Island 2 for the Mac. This
	// boot param only exists in the floppy release. The version that was
	// distributed on CD has a different boot script which doesn't show
	// the copy protection (or difficulty selection) screen at all. We try
	// to patch the script to put these features back, and use the boot
	// param to bypass the copy protection screen (since ScummVM already
	// disables the copy protection check in it).
	//
	// But if the script patching somehow failed, clear the boot param to
	// avoid errors.

	if (_game.id == GID_MONKEY2 && _game.platform == Common::kPlatformMacintosh && _bootParam == -7873 && !verifyMI2MacBootScript()) {
		warning("Unknown MI2 Mac boot script. Using default boot param");
		_bootParam = 0;
	}

	args[0] = _bootParam;
	if (_game.id == GID_MANIAC && (_game.features & GF_DEMO) && (_game.platform != Common::kPlatformC64))
		runScript(9, 0, 0, args);
	else
		runScript(1, 0, 0, args);
}

#ifdef ENABLE_HE
void ScummEngine_v90he::runBootscript() {
	if (_game.heversion >= 98) {
		_logicHE->initOnce();
		_logicHE->beforeBootScript();
	}

	ScummEngine::runBootscript();
}
#endif

bool ScummEngine::startManiac() {
	Common::String currentPath = ConfMan.get("path");
	Common::String maniacTarget;

	if (!ConfMan.hasKey("easter_egg")) {
		// Look for a game with a game path pointing to a 'Maniac' directory
		// as a subdirectory to the current game.
		Common::ConfigManager::DomainMap::iterator iter = ConfMan.beginGameDomains();
		for (; iter != ConfMan.endGameDomains(); ++iter) {
			Common::ConfigManager::Domain &dom = iter->_value;
			Common::String path = dom.getVal("path");

			if (path.hasPrefix(currentPath)) {
				path.erase(0, currentPath.size());
				// Do a case-insensitive non-path-mode match of the remainder.
				// While strictly speaking it's too broad, this matchString
				// ignores the presence or absence of trailing path separators
				// in either currentPath or path.
				if (path.matchString("*maniac*", true, false)) {
					maniacTarget = iter->_key;
					break;
				}
			}
		}
	} else {
		maniacTarget = ConfMan.get("easter_egg");
	}

	if (!maniacTarget.empty()) {
		// Request a temporary save game to be made.
		_saveLoadFlag = 1;
		_saveLoadSlot = 100;
		_saveTemporaryState = true;

		// Set up the chanined games to Maniac Mansion, and then back
		// to the current game again with that save slot.
		ChainedGamesMan.push(maniacTarget);
		ChainedGamesMan.push(ConfMan.getActiveDomainName(), 100);

		// Force a return to the launcher. This will start the first
		// chained game.
		Common::EventManager *eventMan = g_system->getEventManager();
		Common::Event event;
		event.type = Common::EVENT_RETURN_TO_LAUNCHER;
		eventMan->pushEvent(event);
		return true;
	} else {
		Common::U32String buf = _("Usually, Maniac Mansion would start now. But for that to work, the game files for Maniac Mansion have to be in the 'Maniac' directory inside the Tentacle game directory, and the game has to be added to ScummVM.");
		GUI::MessageDialog dialog(buf);
		runDialog(dialog);
		return false;
	}
}

#pragma mark -
#pragma mark --- GUI ---
#pragma mark -

void ScummEngine::pauseEngineIntern(bool pause) {
	if (pause) {
		// Pause sound & video
		if (_sound) {
			_oldSoundsPaused = _sound->_soundsPaused;
			_sound->pauseSounds(true);
		}
	} else {
#ifndef DISABLE_TOWNS_DUAL_LAYER_MODE
		// Restore FM-Towns graphics
		_scrollTimer = 0;
		towns_updateGfx();
#endif
		// Update the screen to make it less likely that the player will see a
		// brief cursor palette glitch when the GUI is disabled.
		_system->updateScreen();

		// Resume sound & video
		if (_sound)
			_sound->pauseSounds(_oldSoundsPaused);
	}
}

#ifdef ENABLE_SCUMM_7_8
void ScummEngine_v7::pauseEngineIntern(bool pause) {
	if (pause) {
		_splayer->pause();
	} else {
		_splayer->unpause();
	}

	ScummEngine::pauseEngineIntern(pause);
}
#endif

void ScummEngine::messageDialog(const Common::U32String &message) {
	if (!_messageDialog)
		_messageDialog = new InfoDialog(this, message);
	((InfoDialog *)_messageDialog)->setInfoText(message);
	runDialog(*_messageDialog);
}

void ScummEngine::pauseDialog() {
	if (!_pauseDialog)
		_pauseDialog = new PauseDialog(this, 4);
	runDialog(*_pauseDialog);
}

void ScummEngine::versionDialog() {
	if (!_versionDialog)
		_versionDialog = new PauseDialog(this, 1);
	runDialog(*_versionDialog);
}

void ScummEngine::confirmExitDialog() {
	ConfirmDialog d(this, 6);

	if (runDialog(d)) {
		quitGame();
	}
}

void ScummEngine::confirmRestartDialog() {
	ConfirmDialog d(this, 5);

	if (runDialog(d)) {
		restart();
	}
}

char ScummEngine::displayMessage(const char *altButton, const char *message, ...) {
	char buf[STRINGBUFLEN];
	va_list va;

	va_start(va, message);
	vsnprintf(buf, STRINGBUFLEN, message, va);
	va_end(va);

	GUI::MessageDialog dialog(buf, "OK", altButton);
	return runDialog(dialog);
}


#pragma mark -
#pragma mark --- Miscellaneous ---
#pragma mark -

void ScummEngine::errorString(const char *buf1, char *buf2, int buf2Size) {
	if (_currentScript != 0xFF) {
		snprintf(buf2, buf2Size, "(%d:%d:0x%lX): %s", _roomResource,
			vm.slot[_currentScript].number, (long)(_scriptPointer - _scriptOrgPointer), buf1);
	} else {
		strncpy(buf2, buf1, buf2Size);
		if (buf2Size > 0)
			buf2[buf2Size-1] = '\0';
	}
}


} // End of namespace Scumm
