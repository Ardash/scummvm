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

#include "twine/twine.h"
#include "common/config-manager.h"
#include "common/debug.h"
#include "common/error.h"
#include "common/events.h"
#include "common/keyboard.h"
#include "common/savefile.h"
#include "common/scummsys.h"
#include "common/str.h"
#include "common/stream.h"
#include "common/system.h"
#include "common/textconsole.h"
#include "engines/metaengine.h"
#include "engines/util.h"
#include "graphics/cursorman.h"
#include "graphics/font.h"
#include "graphics/fontman.h"
#include "graphics/managed_surface.h"
#include "graphics/palette.h"
#include "graphics/pixelformat.h"
#include "graphics/surface.h"
#include "gui/debugger.h"
#include "twine/audio/music.h"
#include "twine/audio/sound.h"
#include "twine/debugger/console.h"
#include "twine/debugger/debug.h"
#include "twine/debugger/debug_grid.h"
#include "twine/debugger/debug_scene.h"
#include "twine/detection.h"
#include "twine/flamovies.h"
#include "twine/holomap.h"
#include "twine/input.h"
#include "twine/menu/interface.h"
#include "twine/menu/menu.h"
#include "twine/menu/menuoptions.h"
#include "twine/renderer/redraw.h"
#include "twine/renderer/renderer.h"
#include "twine/renderer/screens.h"
#include "twine/resources/hqr.h"
#include "twine/resources/resources.h"
#include "twine/scene/actor.h"
#include "twine/scene/animations.h"
#include "twine/scene/collision.h"
#include "twine/scene/extra.h"
#include "twine/scene/gamestate.h"
#include "twine/scene/grid.h"
#include "twine/scene/movements.h"
#include "twine/scene/scene.h"
#include "twine/script/script_life_v1.h"
#include "twine/script/script_move_v1.h"
#include "twine/text.h"

#define ORIGINAL_WIDTH 640
#define ORIGINAL_HEIGHT 480

namespace TwinE {

ScopedEngineFreeze::ScopedEngineFreeze(TwinEEngine *engine) : _engine(engine) {
	_engine->freezeTime();
}

ScopedEngineFreeze::~ScopedEngineFreeze() {
	_engine->unfreezeTime();
}

ScopedCursor::ScopedCursor(TwinEEngine *engine) : _engine(engine) {
	_engine->pushMouseCursorVisible();
}

ScopedCursor::~ScopedCursor() {
	_engine->popMouseCursorVisible();
}

FrameMarker::FrameMarker(TwinEEngine *engine, uint32 fps) : _engine(engine), _fps(fps) {
	_start = g_system->getMillis();
	++_engine->_frameCounter;
}

FrameMarker::~FrameMarker() {
	_engine->_frontVideoBuffer.update();
	if (_fps == 0) {
		return;
	}
	const uint32 end = g_system->getMillis();
	const uint32 frameTime = end - _start;
	const uint32 maxDelay = 1000 / _fps;
	if (frameTime > maxDelay) {
		debug("Frame took longer than the max allowed time: %u (max is %u)", frameTime, maxDelay);
		return;
	}
	const uint32 waitMillis = maxDelay - frameTime;
	g_system->delayMillis(waitMillis);
}

TwineScreen::TwineScreen(TwinEEngine *engine) : _engine(engine) {
}

void TwineScreen::update() {
	static int lastFrame = 0;
	if (lastFrame == _engine->_frameCounter) {
		return;
	}
	lastFrame = _engine->_frameCounter;
	Super::update();
}

TwinEEngine::TwinEEngine(OSystem *system, Common::Language language, uint32 flags, TwineGameType gameType)
	: Engine(system), _gameType(gameType), _gameLang(language), _frontVideoBuffer(this), _gameFlags(flags), _rnd("twine") {
	// Add default file directories
	const Common::FSNode gameDataDir(ConfMan.get("path"));
	SearchMan.addSubDirectoryMatching(gameDataDir, "fla");
	SearchMan.addSubDirectoryMatching(gameDataDir, "vox");
	if (flags & TF_DOTEMU_ENHANCED) {
		SearchMan.addSubDirectoryMatching(gameDataDir, "resources/lba_files/hqr");
		SearchMan.addSubDirectoryMatching(gameDataDir, "resources/lba_files/fla");
		if (_gameLang == Common::Language::DE_DEU) {
			SearchMan.addSubDirectoryMatching(gameDataDir, "resources/lba_files/vox/de_voice");
		}
		if (_gameLang == Common::Language::EN_ANY || _gameLang == Common::Language::EN_GRB || _gameLang == Common::Language::EN_USA) {
			SearchMan.addSubDirectoryMatching(gameDataDir, "resources/lba_files/vox/en_voice");
		}
		if (_gameLang == Common::Language::FR_FRA) {
			SearchMan.addSubDirectoryMatching(gameDataDir, "resources/lba_files/vox/fr_voice");
		}
#ifdef USE_MAD
		SearchMan.addSubDirectoryMatching(gameDataDir, "resources/lba_files/music");
		SearchMan.addSubDirectoryMatching(gameDataDir, "resources/lba_files/midi_mp3");
#endif
#ifdef USE_VORBIS
		SearchMan.addSubDirectoryMatching(gameDataDir, "resources/lba_files/music/ogg");
		SearchMan.addSubDirectoryMatching(gameDataDir, "resources/lba_files/midi_mp3/ogg");
#endif
	}

	setDebugger(new TwinEConsole(this));
	_actor = new Actor(this);
	_animations = new Animations(this);
	_collision = new Collision(this);
	_extra = new Extra(this);
	_gameState = new GameState(this);
	_grid = new Grid(this);
	_movements = new Movements(this);
	_interface = new Interface(this);
	_menu = new Menu(this);
	_flaMovies = new FlaMovies(this);
	_menuOptions = new MenuOptions(this);
	_music = new Music(this);
	_redraw = new Redraw(this);
	_renderer = new Renderer(this);
	_resources = new Resources(this);
	_scene = new Scene(this);
	_screens = new Screens(this);
	_scriptLife = new ScriptLife(this);
	_scriptMove = new ScriptMove(this);
	_holomap = new Holomap(this);
	_sound = new Sound(this);
	_text = new Text(this);
	_debugGrid = new DebugGrid(this);
	_input = new Input(this);
	_debug = new Debug(this);
	_debugScene = new DebugScene(this);
}

TwinEEngine::~TwinEEngine() {
	ConfMan.flushToDisk();
	delete _actor;
	delete _animations;
	delete _collision;
	delete _extra;
	delete _gameState;
	delete _grid;
	delete _movements;
	delete _interface;
	delete _menu;
	delete _flaMovies;
	delete _menuOptions;
	delete _music;
	delete _redraw;
	delete _renderer;
	delete _resources;
	delete _scene;
	delete _screens;
	delete _scriptLife;
	delete _scriptMove;
	delete _holomap;
	delete _sound;
	delete _text;
	delete _debugGrid;
	delete _input;
	delete _debug;
	delete _debugScene;
}

void TwinEEngine::pushMouseCursorVisible() {
	++_mouseCursorState;
	if (!_cfgfile.Mouse) {
		return;
	}
	if (_mouseCursorState == 1) {
		CursorMan.showMouse(_cfgfile.Mouse);
	}
}

void TwinEEngine::popMouseCursorVisible() {
	--_mouseCursorState;
	if (_mouseCursorState == 0) {
		CursorMan.showMouse(false);
	}
}

Common::Error TwinEEngine::run() {
	debug("Based on TwinEngine v0.2.2");
	debug("(c) 2002 The TwinEngine team.");
	debug("(c) 2020, 2021 The ScummVM team.");
	debug("Refer to the credits for further details.");
	debug("The original Little Big Adventure game is:");
	debug("(c) 1994 by Adeline Software International, All Rights Reserved.");

	ConfMan.registerDefault("usehighres", false);
	ConfMan.registerDefault("wallcollision", false);

	Common::String gameTarget = ConfMan.getActiveDomainName();
	AchMan.setActiveDomain(getMetaEngine()->getAchievementsInfo(gameTarget));

	syncSoundSettings();
	int32 w = ORIGINAL_WIDTH;
	int32 h = ORIGINAL_HEIGHT;
	const bool highRes = ConfMan.getBool("usehighres");
	if (highRes) {
		w = 1024;
		h = 768;
	}

	initGraphics(w, h);
	allocVideoMemory(w, h);
	initAll();
	initEngine();
	_sound->stopSamples();
	saveFrontBuffer();

	_menu->init();
	_holomap->loadLocations();

	if (ConfMan.hasKey("save_slot")) {
		const int saveSlot = ConfMan.getInt("save_slot");
		if (saveSlot >= 0 && saveSlot <= 999) {
			Common::Error state = loadGameState(saveSlot);
			if (state.getCode() != Common::kNoError) {
				return state;
			}
		}
	}
	while (!shouldQuit()) {
		readKeys();
		switch (_state) {
		case EngineState::QuitGame: {
			Common::Event event;
			event.type = Common::EVENT_QUIT;
			_system->getEventManager()->pushEvent(event);
			break;
		}
		case EngineState::LoadedGame:
			debug("Loaded game");
			if (_scene->_newHeroPos.x == -1) {
				_scene->_heroPositionType = ScenePositionType::kNoPosition;
			}
			_text->_renderTextTriangle = false;
			_text->textClipSmall();
			_text->_drawTextBoxBackground = true;
			_state = EngineState::GameLoop;
			break;
		case EngineState::GameLoop:
			if (gameEngineLoop()) {
				_menuOptions->showCredits();
				_menuOptions->showEndSequence();
			}
			_state = EngineState::Menu;
			break;
		case EngineState::Menu:
		#if 0
			// this will enter the game and execute the commands in the file "debug"
			_gameState->initEngineVars();
			_text->textClipSmall();
			_text->drawTextBoxBackground = true;
			_text->renderTextTriangle = false;
			if (!((TwinEConsole*)getDebugger())->exec("debug")) {
				debug("Failed to execute debug file before entering the scene");
			}
			gameEngineLoop();
		#else
			_state = _menu->run();
		#endif
			break;
		}
	}

	ConfMan.setBool("combatauto", _actor->_autoAggressive);
	ConfMan.setInt("shadow", _cfgfile.ShadowMode);
	ConfMan.setBool("scezoom", _cfgfile.SceZoom);
	ConfMan.setInt("polygondetails", _cfgfile.PolygonDetails);

	_sound->stopSamples();
	_music->stopTrackMusic();
	_music->stopMidiMusic();
	return Common::kNoError;
}

bool TwinEEngine::hasFeature(EngineFeature f) const {
	switch (f) {
	case EngineFeature::kSupportsReturnToLauncher:
	case EngineFeature::kSupportsLoadingDuringRuntime:
	case EngineFeature::kSupportsSavingDuringRuntime:
	case EngineFeature::kSupportsChangingOptionsDuringRuntime:
		return true;
	default:
		break;
	}
	return false;
}

SaveStateList TwinEEngine::getSaveSlots() const {
	return getMetaEngine()->listSaves(_targetName.c_str());
}

void TwinEEngine::wipeSaveSlot(int slot) {
	Common::SaveFileManager *saveFileMan = getSaveFileManager();
	const Common::String &saveFile = getSaveStateName(slot);
	saveFileMan->removeSavefile(saveFile);
}

bool TwinEEngine::canSaveGameStateCurrently() { return _scene->isGameRunning(); }

Common::Error TwinEEngine::loadGameStream(Common::SeekableReadStream *stream) {
	debug("load game stream");
	if (!_gameState->loadGame(stream)) {
		return Common::Error(Common::kReadingFailed);
	}
	_state = EngineState::LoadedGame;
	return Common::Error(Common::kNoError);
}

Common::Error TwinEEngine::saveGameStream(Common::WriteStream *stream, bool isAutosave) {
	if (!_gameState->saveGame(stream)) {
		return Common::Error(Common::kWritingFailed);
	}
	return Common::Error(Common::kNoError);
}

void TwinEEngine::autoSave() {
	debug("Autosave %s", _gameState->_sceneName);
	saveGameState(getAutosaveSlot(), _gameState->_sceneName, true);
}

void TwinEEngine::allocVideoMemory(int32 w, int32 h) {
	const Graphics::PixelFormat format = Graphics::PixelFormat::createFormatCLUT8();

	_imageBuffer.create(ORIGINAL_WIDTH, ORIGINAL_HEIGHT, format); // original lba1 resolution for a lot of images.

	_workVideoBuffer.create(w, h, format);
	_frontVideoBuffer.create(w, h, format);
	_renderer->init(w, h);
	_grid->init(w, h);
}

static int getLanguageTypeIndex(const char *languageName) {
	char buffer[256];
	Common::strlcpy(buffer, languageName, sizeof(buffer));

	char *ptr = strchr(buffer, ' ');
	if (ptr != nullptr) {
		*ptr = '\0';
	}

	const int32 length = ARRAYSIZE(LanguageTypes);
	for (int32 i = 0; i < length; i++) {
		if (!strcmp(LanguageTypes[i].name, buffer)) {
			return i;
		}
	}

	debug("Failed to detect language %s - falling back to english", languageName);
	// select english for the fan translations
	return 0; // English
}

#define ConfGetOrDefault(key, defaultVal) (ConfMan.hasKey(key) ? ConfMan.get(key) : Common::String(defaultVal))
#define ConfGetIntOrDefault(key, defaultVal) (ConfMan.hasKey(key) ? atoi(ConfMan.get(key).c_str()) : (defaultVal))
#define ConfGetBoolOrDefault(key, defaultVal) (ConfMan.hasKey(key) ? ConfMan.get(key) == "true" || atoi(ConfMan.get(key).c_str()) == 1 : (defaultVal))

void TwinEEngine::initConfigurations() {
	// TODO: use existing entries for some of the settings - like volume and so on.

	const char *lng = Common::getLanguageDescription(_gameLang);
	_cfgfile.LanguageId = getLanguageTypeIndex(lng);
	_cfgfile.Voice = ConfGetBoolOrDefault("voice", true);
	_cfgfile.FlagDisplayText = ConfGetBoolOrDefault("displaytext", true);
	const Common::String midiType = ConfGetOrDefault("miditype", "auto");
	if (midiType == "None") {
		_cfgfile.MidiType = MIDIFILE_NONE;
	} else {
		Common::File midiHqr;
		if (midiHqr.exists(Resources::HQR_MIDI_MI_WIN_FILE)) {
			_cfgfile.MidiType = MIDIFILE_WIN;
			debug("Use %s for midi", Resources::HQR_MIDI_MI_WIN_FILE);
		} else if (midiHqr.exists(Resources::HQR_MIDI_MI_DOS_FILE)) {
			_cfgfile.MidiType = MIDIFILE_DOS;
			debug("Use %s for midi", Resources::HQR_MIDI_MI_DOS_FILE);
		} else {
			_cfgfile.MidiType = MIDIFILE_NONE;
			debug("Could not find midi hqr file");
		}
	}

	if (_gameFlags & TwineFeatureFlags::TF_VERSION_EUROPE) {
		_cfgfile.Version = EUROPE_VERSION;
	} else if (_gameFlags & TwineFeatureFlags::TF_VERSION_USA) {
		_cfgfile.Version = USA_VERSION;
	} else if (_gameFlags & TwineFeatureFlags::TF_VERSION_CUSTOM) {
		_cfgfile.Version = MODIFICATION_VERSION;
	}

	if (_gameFlags & TwineFeatureFlags::TF_USE_GIF) {
		_cfgfile.Movie = CONF_MOVIE_FLAGIF;
	}

	_cfgfile.UseCD = ConfGetBoolOrDefault("usecd", false);
	_cfgfile.Sound = ConfGetBoolOrDefault("sound", true);
	_cfgfile.Fps = ConfGetIntOrDefault("fps", DEFAULT_FRAMES_PER_SECOND);
	_cfgfile.Debug = ConfGetBoolOrDefault("debug", false);
	_cfgfile.Mouse = ConfGetIntOrDefault("mouse", true);

	_cfgfile.UseAutoSaving = ConfGetBoolOrDefault("useautosaving", false);
	_cfgfile.CrossFade = ConfGetBoolOrDefault("crossfade", false);
	_cfgfile.WallCollision = ConfGetBoolOrDefault("wallcollision", false);

	_actor->_autoAggressive = ConfGetBoolOrDefault("combatauto", true);
	_cfgfile.ShadowMode = ConfGetIntOrDefault("shadow", 2);
	_cfgfile.SceZoom = ConfGetBoolOrDefault("scezoom", false);
	_cfgfile.PolygonDetails = ConfGetIntOrDefault("polygondetails", 2);

	debug(1, "UseCD:          %s", (_cfgfile.UseCD ? "true" : "false"));
	debug(1, "Sound:          %s", (_cfgfile.Sound ? "true" : "false"));
	debug(1, "Movie:          %i", _cfgfile.Movie);
	debug(1, "Fps:            %i", _cfgfile.Fps);
	debug(1, "Debug:          %s", (_cfgfile.Debug ? "true" : "false"));
	debug(1, "UseAutoSaving:  %s", (_cfgfile.UseAutoSaving ? "true" : "false"));
	debug(1, "CrossFade:      %s", (_cfgfile.CrossFade ? "true" : "false"));
	debug(1, "WallCollision:  %s", (_cfgfile.WallCollision ? "true" : "false"));
	debug(1, "AutoAggressive: %s", (_actor->_autoAggressive ? "true" : "false"));
	debug(1, "ShadowMode:     %i", _cfgfile.ShadowMode);
	debug(1, "PolygonDetails: %i", _cfgfile.PolygonDetails);
	debug(1, "SceZoom:        %s", (_cfgfile.SceZoom ? "true" : "false"));
}

void TwinEEngine::queueMovie(const char *filename) {
	_queuedFlaMovie = filename;
}

void TwinEEngine::initEngine() {
	_screens->clearScreen();

	// Check if LBA CD-Rom is on drive
	_music->initCdrom();

	_input->enableKeyMap(cutsceneKeyMapId);
	// Display company logo
	bool abort = false;
	abort |= _screens->adelineLogo();

	// verify game version screens
	if (!abort && _cfgfile.Version == EUROPE_VERSION) {
		// Little Big Adventure screen
		abort |= _screens->loadImageDelay(RESSHQR_LBAIMG, RESSHQR_LBAPAL, 3);
		if (!abort) {
			// Electronic Arts Logo
			abort |= _screens->loadImageDelay(RESSHQR_EAIMG, RESSHQR_EAPAL, 2);
		}
	} else if (!abort && _cfgfile.Version == USA_VERSION) {
		// Relentless screen
		abort |= _screens->loadImageDelay(RESSHQR_RELLENTIMG, RESSHQR_RELLENTPAL, 3);
		if (!abort) {
			// Electronic Arts Logo
			abort |= _screens->loadImageDelay(RESSHQR_EAIMG, RESSHQR_EAPAL, 2);
		}
	} else if (!abort && _cfgfile.Version == MODIFICATION_VERSION) {
		// Modification screen
		abort |= _screens->loadImageDelay(RESSHQR_RELLENTIMG, RESSHQR_RELLENTPAL, 2);
	}

	if (!abort) {
		_flaMovies->playFlaMovie(FLA_DRAGON3);
	}
	_input->enableKeyMap(uiKeyMapId);

	_screens->loadMenuImage();
}

void TwinEEngine::initSceneryView() {
	_redraw->_inSceneryView = true;
}

void TwinEEngine::exitSceneryView() {
	_redraw->_inSceneryView = false;
}

void TwinEEngine::initAll() {
	_scene->_sceneHero = _scene->getActor(OWN_ACTOR_SCENE_INDEX);

	// Set clip to fullscreen by default, allows main menu to render properly after load
	_interface->resetClip();

	// getting configuration file
	initConfigurations();

	_resources->initResources();

	exitSceneryView();
}

int TwinEEngine::getRandomNumber(uint max) {
	if (max == 0) {
		return 0;
	}
	return _rnd.getRandomNumber(max - 1);
}

void TwinEEngine::freezeTime() {
	if (_isTimeFreezed == 0) {
		_saveFreezedTime = _lbaTime;
		_pauseToken = pauseEngine();
	}
	_isTimeFreezed++;
}

void TwinEEngine::unfreezeTime() {
	--_isTimeFreezed;
	if (_isTimeFreezed == 0) {
		_lbaTime = _saveFreezedTime;
		_pauseToken.clear();
	}
}

void TwinEEngine::processActorSamplePosition(int32 actorIdx) {
	const ActorStruct *actor = _scene->getActor(actorIdx);
	const int32 channelIdx = _sound->getActorChannel(actorIdx);
	_sound->setSamplePosition(channelIdx, actor->pos());
}

void TwinEEngine::processBookOfBu() {
	_screens->fadeToBlack(_screens->_paletteRGBA);
	_screens->loadImage(RESSHQR_INTROSCREEN1IMG, RESSHQR_INTROSCREEN1PAL);
	_text->initTextBank(TextBankId::Inventory_Intro_and_Holomap);
	_text->_drawTextBoxBackground = false;
	_text->textClipFull();
	_text->setFontCrossColor(COLOR_WHITE);
	const bool tmpFlagDisplayText = _cfgfile.FlagDisplayText;
	_cfgfile.FlagDisplayText = true;
	_text->drawTextProgressive(TextId::kBookOfBu);
	_cfgfile.FlagDisplayText = tmpFlagDisplayText;
	_text->textClipSmall();
	_text->_drawTextBoxBackground = true;
	_text->initSceneTextBank();
	_screens->fadeToBlack(_screens->_paletteRGBACustom);
	_screens->clearScreen();
	setPalette(_screens->_paletteRGBA);
	_screens->_lockPalette = true;
}

void TwinEEngine::processBonusList() {
	_text->initTextBank(TextBankId::Inventory_Intro_and_Holomap);
	_text->textClipFull();
	_text->setFontCrossColor(COLOR_WHITE);
	const bool tmpFlagDisplayText = _cfgfile.FlagDisplayText;
	_cfgfile.FlagDisplayText = true;
	_text->drawTextProgressive(TextId::kBonusList);
	_cfgfile.FlagDisplayText = tmpFlagDisplayText;
	_text->textClipSmall();
	_text->initSceneTextBank();
}

void TwinEEngine::processInventoryAction() {
	ScopedEngineFreeze scoped(this);
	exitSceneryView();
	_menu->processInventoryMenu();

	switch (_loopInventoryItem) {
	case kiHolomap:
		_holomap->processHolomap();
		_screens->_lockPalette = true;
		break;
	case kiMagicBall:
		if (_gameState->_usingSabre) {
			_actor->initModelActor(BodyType::btNormal, OWN_ACTOR_SCENE_INDEX);
		}
		_gameState->_usingSabre = false;
		break;
	case kiUseSabre:
		if (_scene->_sceneHero->_body != BodyType::btSabre) {
			if (_actor->_heroBehaviour == HeroBehaviourType::kProtoPack) {
				_actor->setBehaviour(HeroBehaviourType::kNormal);
			}
			_actor->initModelActor(BodyType::btSabre, OWN_ACTOR_SCENE_INDEX);
			_animations->initAnim(AnimationTypes::kSabreUnknown, AnimType::kAnimationType_1, AnimationTypes::kStanding, OWN_ACTOR_SCENE_INDEX);

			_gameState->_usingSabre = true;
		}
		break;
	case kiBookOfBu: {
		processBookOfBu();
		break;
	}
	case kiProtoPack:
		if (_gameState->hasItem(InventoryItems::kiBookOfBu)) {
			_scene->_sceneHero->_body = BodyType::btNormal;
		} else {
			_scene->_sceneHero->_body = BodyType::btTunic;
		}

		if (_actor->_heroBehaviour == HeroBehaviourType::kProtoPack) {
			_actor->setBehaviour(HeroBehaviourType::kNormal);
		} else {
			_actor->setBehaviour(HeroBehaviourType::kProtoPack);
		}
		break;
	case kiPinguin: {
		ActorStruct *pinguin = _scene->getActor(_scene->_mecaPinguinIdx);

		pinguin->_pos.x = _renderer->_destPos.x + _scene->_sceneHero->_pos.x;
		pinguin->_pos.y = _scene->_sceneHero->_pos.y;
		pinguin->_pos.z = _renderer->_destPos.z + _scene->_sceneHero->_pos.z;
		pinguin->_angle = _scene->_sceneHero->_angle;

		_movements->rotateActor(0, 800, pinguin->_angle);

		if (!_collision->checkCollisionWithActors(_scene->_mecaPinguinIdx)) {
			pinguin->setLife(kActorMaxLife);
			pinguin->_body = BodyType::btNone;
			_actor->initModelActor(BodyType::btNormal, _scene->_mecaPinguinIdx);
			pinguin->_dynamicFlags.bIsDead = 0;
			pinguin->setBrickShape(ShapeType::kNone);
			_movements->moveActor(pinguin->_angle, pinguin->_angle, pinguin->_speed, &pinguin->_move);
			_gameState->removeItem(InventoryItems::kiPinguin);
			pinguin->_delayInMillis = _lbaTime + 1500;
		}
		break;
	}
	case kiBonusList: {
		_redraw->redrawEngineActions(true);
		processBonusList();
		break;
	}
	case kiCloverLeaf:
		if (_scene->_sceneHero->_life < kActorMaxLife) {
			if (_gameState->_inventoryNumLeafs > 0) {
				_scene->_sceneHero->setLife(kActorMaxLife);
				_gameState->setMagicPoints(_gameState->_magicLevelIdx * 20);
				_gameState->addLeafs(-1);
				_redraw->addOverlay(OverlayType::koInventoryItem, InventoryItems::kiCloverLeaf, 0, 0, 0, OverlayPosType::koNormal, 3);
			}
		}
		break;
	}

	_redraw->redrawEngineActions(true);
}

void TwinEEngine::processOptionsMenu() {
	ScopedEngineFreeze scoped(this);
	exitSceneryView();
	_sound->pauseSamples();
	_menu->inGameOptionsMenu();
	_scene->playSceneMusic();
	_sound->resumeSamples();
	_redraw->redrawEngineActions(true);
}

int32 TwinEEngine::runGameEngine() { // mainLoopInteration
	FrameMarker frame(this, 0);
	_input->enableKeyMap(mainKeyMapId);

	readKeys();

	if (!_queuedFlaMovie.empty()) {
		_flaMovies->playFlaMovie(_queuedFlaMovie.c_str());
		_queuedFlaMovie.clear();
	}

	if (_scene->_needChangeScene > -1) {
		if (isDemo() && isLBA1()) {
			// the demo only has these two scenes
			if (_scene->_needChangeScene != LBA1SceneId::Citadel_Island_Prison && _scene->_needChangeScene != LBA1SceneId::Citadel_Island_outside_the_citadel) {
				return 1;
			}
		}
		_scene->changeScene();
	}

	_movements->update();

	_debug->processDebug();

	if (_menuOptions->canShowCredits) {
		// TODO: if current music playing != 8, than play_track(8);
		if (_input->toggleAbortAction()) {
			return 1;
		}
	} else {
		// Process give up menu - Press ESC
		if (_input->toggleAbortAction() && _scene->_sceneHero->_life > 0 && _scene->_sceneHero->_entity != -1 && !_scene->_sceneHero->_staticFlags.bIsHidden) {
			ScopedEngineFreeze scopedFreeze(this);
			exitSceneryView();
			const int giveUp = _menu->giveupMenu();
			if (giveUp == kQuitEngine) {
				return 0;
			}
			if (giveUp == 1) {
				_redraw->redrawEngineActions(true);
				_quitGame = 0;
				return 0;
			}
			_redraw->redrawEngineActions(true);
		}

		if (_input->toggleActionIfActive(TwinEActionType::OptionsMenu)) {
			processOptionsMenu();
		}

		// inventory menu
		_loopInventoryItem = -1;
		if (_input->isActionActive(TwinEActionType::InventoryMenu) && _scene->_sceneHero->_entity != -1 && _scene->_sceneHero->_controlMode == ControlMode::kManual) {
			processInventoryAction();
		}

		if (_input->toggleActionIfActive(TwinEActionType::ChangeBehaviourNormal)) {
			_actor->setBehaviour(HeroBehaviourType::kNormal);
		} else if (_input->toggleActionIfActive(TwinEActionType::ChangeBehaviourAthletic)) {
			_actor->setBehaviour(HeroBehaviourType::kAthletic);
		} else if (_input->toggleActionIfActive(TwinEActionType::ChangeBehaviourAggressive)) {
			_actor->setBehaviour(HeroBehaviourType::kAggressive);
		} else if (_input->toggleActionIfActive(TwinEActionType::ChangeBehaviourDiscreet)) {
			_actor->setBehaviour(HeroBehaviourType::kDiscrete);
		}

		// Process behaviour menu
		if ((_input->isActionActive(TwinEActionType::BehaviourMenu, false) ||
		     _input->isActionActive(TwinEActionType::QuickBehaviourNormal, false) ||
		     _input->isActionActive(TwinEActionType::QuickBehaviourAthletic, false) ||
		     _input->isActionActive(TwinEActionType::QuickBehaviourAggressive, false) ||
		     _input->isActionActive(TwinEActionType::QuickBehaviourDiscreet, false)) &&
		    _scene->_sceneHero->_entity != -1 && _scene->_sceneHero->_controlMode == ControlMode::kManual) {
			if (_input->isActionActive(TwinEActionType::QuickBehaviourNormal, false)) {
				_actor->_heroBehaviour = HeroBehaviourType::kNormal;
			} else if (_input->isActionActive(TwinEActionType::QuickBehaviourAthletic, false)) {
				_actor->_heroBehaviour = HeroBehaviourType::kAthletic;
			} else if (_input->isActionActive(TwinEActionType::QuickBehaviourAggressive, false)) {
				_actor->_heroBehaviour = HeroBehaviourType::kAggressive;
			} else if (_input->isActionActive(TwinEActionType::QuickBehaviourDiscreet, false)) {
				_actor->_heroBehaviour = HeroBehaviourType::kDiscrete;
			}
			ScopedEngineFreeze scopedFreeze(this);
			_menu->processBehaviourMenu();
			_redraw->redrawEngineActions(true);
		}

		// use Proto-Pack
		if (_input->toggleActionIfActive(TwinEActionType::UseProtoPack) && _gameState->hasItem(InventoryItems::kiProtoPack)) {
			if (_gameState->hasItem(InventoryItems::kiBookOfBu)) {
				_scene->_sceneHero->_body = BodyType::btNormal;
			} else {
				_scene->_sceneHero->_body = BodyType::btTunic;
			}

			if (_actor->_heroBehaviour == HeroBehaviourType::kProtoPack) {
				_actor->setBehaviour(HeroBehaviourType::kNormal);
			} else {
				_actor->setBehaviour(HeroBehaviourType::kProtoPack);
			}
		}

		// Recenter Screen
		if (_input->toggleActionIfActive(TwinEActionType::RecenterScreenOnTwinsen) && !_disableScreenRecenter) {
			const ActorStruct *currentlyFollowedActor = _scene->getActor(_scene->_currentlyFollowedActor);
			_grid->centerOnActor(currentlyFollowedActor);
		}

		// Draw holomap
		if (_input->toggleActionIfActive(TwinEActionType::OpenHolomap) && _gameState->hasItem(InventoryItems::kiHolomap) && !_gameState->inventoryDisabled()) {
			_holomap->processHolomap();
			_screens->_lockPalette = true;
			_redraw->redrawEngineActions(true);
		}

		// Process Pause
		if (_input->toggleActionIfActive(TwinEActionType::Pause)) {
			ScopedEngineFreeze scopedFreeze(this);
			const char *PauseString = "Pause";
			_text->setFontColor(COLOR_WHITE);
			if (_redraw->_inSceneryView) {
				_text->drawText(_redraw->_sceneryViewX + 5, _redraw->_sceneryViewY, PauseString);
				_redraw->zoomScreenScale();
			} else {
				const int width = _text->getTextSize(PauseString);
				const int bottom = height() - _text->lineHeight;
				_text->drawText(5, bottom, PauseString);
				copyBlockPhys(5, bottom, 5 + width, bottom + _text->lineHeight);
			}
			do {
				FrameMarker frameWait(this);
				readKeys();
				if (shouldQuit()) {
					break;
				}
			} while (!_input->toggleActionIfActive(TwinEActionType::Pause));
			_redraw->redrawEngineActions(true);
		}
	}

	_loopActorStep = _loopMovePtr.getRealValue(_lbaTime);
	if (!_loopActorStep) {
		_loopActorStep = 1;
	}

	_movements->setActorAngle(ANGLE_0, -ANGLE_90, ANGLE_1, &_loopMovePtr);
	_disableScreenRecenter = false;

	_scene->processEnvironmentSound();

	// Reset HitBy state
	for (int32 a = 0; a < _scene->_sceneNumActors; a++) {
		_scene->getActor(a)->_hitBy = -1;
	}

	_extra->processExtras();

	for (int32 a = 0; a < _scene->_sceneNumActors; a++) {
		ActorStruct *actor = _scene->getActor(a);

		if (actor->_dynamicFlags.bIsDead) {
			continue;
		}

		if (actor->_life == 0) {
			if (IS_HERO(a)) {
				_animations->initAnim(AnimationTypes::kLandDeath, AnimType::kAnimationType_4, AnimationTypes::kStanding, 0);
				actor->_controlMode = ControlMode::kNoMove;
			} else {
				_sound->playSample(Samples::Explode, 1, actor->pos(), a);

				if (a == _scene->_mecaPinguinIdx) {
					_extra->addExtraExplode(actor->pos());
				}
			}

			if (!actor->_bonusParameter.unk1 && (actor->_bonusParameter.cloverleaf || actor->_bonusParameter.kashes || actor->_bonusParameter.key || actor->_bonusParameter.lifepoints || actor->_bonusParameter.magicpoints)) {
				_actor->processActorExtraBonus(a);
			}
		}

		_movements->processActorMovements(a);

		actor->_collisionPos = actor->pos();

		if (actor->_positionInMoveScript != -1) {
			_scriptMove->processMoveScript(a);
		}

		_animations->processActorAnimations(a);

		if (actor->_staticFlags.bIsZonable) {
			_scene->processActorZones(a);
		}

		if (actor->_positionInLifeScript != -1) {
			_scriptLife->processLifeScript(a);
		}

		processActorSamplePosition(a);

		if (_quitGame != -1) {
			return _quitGame;
		}

		if (actor->_staticFlags.bCanDrown) {
			int32 brickSound = _grid->getBrickSoundType(actor->_pos.x, actor->_pos.y - 1, actor->_pos.z);
			actor->_brickSound = brickSound;

			if ((brickSound & 0xF0) == 0xF0) {
				if ((brickSound & 0x0F) == 1) {
					if (IS_HERO(a)) {
						if (_actor->_heroBehaviour != HeroBehaviourType::kProtoPack || actor->_anim != AnimationTypes::kForward) {
							if (!_actor->_cropBottomScreen) {
								_animations->initAnim(AnimationTypes::kDrawn, AnimType::kAnimationType_4, AnimationTypes::kStanding, 0);
								_renderer->projectPositionOnScreen(actor->pos() - _grid->_camera);
								_actor->_cropBottomScreen = _renderer->_projPos.y;
							}
							_renderer->projectPositionOnScreen(actor->pos() - _grid->_camera);
							actor->_controlMode = ControlMode::kNoMove;
							actor->setLife(-1);
							_actor->_cropBottomScreen = _renderer->_projPos.y;
							actor->_staticFlags.bCanDrown |= 0x10; // TODO: doesn't make sense
						}
					} else {
						_sound->playSample(Samples::Explode, 1, actor->pos(), a);
						if (actor->_bonusParameter.cloverleaf || actor->_bonusParameter.kashes || actor->_bonusParameter.key || actor->_bonusParameter.lifepoints || actor->_bonusParameter.magicpoints) {
							if (!actor->_bonusParameter.unk1) {
								_actor->processActorExtraBonus(a);
							}
							actor->setLife(0);
						}
					}
				}
			}
		}

		if (actor->_life <= 0) {
			if (IS_HERO(a)) {
				if (actor->_dynamicFlags.bAnimEnded) {
					if (_gameState->_inventoryNumLeafs > 0) { // use clover leaf automaticaly
						_scene->_sceneHero->_pos = _scene->_newHeroPos;

						_scene->_needChangeScene = _scene->_currentSceneIdx;
						_gameState->setMaxMagicPoints();

						_grid->centerOnActor(_scene->_sceneHero);

						_scene->_heroPositionType = ScenePositionType::kReborn;

						_scene->_sceneHero->setLife(kActorMaxLife);
						_redraw->_reqBgRedraw = true;
						_screens->_lockPalette = true;
						_gameState->addLeafs(-1);
						_actor->_cropBottomScreen = 0;
					} else { // game over
						_gameState->setLeafBoxes(2);
						_gameState->setLeafs(1);
						_gameState->setMaxMagicPoints();
						_actor->_heroBehaviour = _actor->_previousHeroBehaviour;
						actor->_angle = _actor->_previousHeroAngle;
						actor->setLife(kActorMaxLife);

						if (_scene->_previousSceneIdx != _scene->_currentSceneIdx) {
							_scene->_newHeroPos.x = -1;
							_scene->_newHeroPos.y = -1;
							_scene->_newHeroPos.z = -1;
							_scene->_currentSceneIdx = _scene->_previousSceneIdx;
							_scene->stopRunningGame();
						}

						autoSave();
						_gameState->processGameoverAnimation();
						_quitGame = 0;
						return 0;
					}
				}
			} else {
				_actor->processActorCarrier(a);
				actor->_dynamicFlags.bIsDead = 1;
				actor->_entity = -1;
				actor->_zone = -1;
			}
		}

		if (_scene->_needChangeScene != -1) {
			return 0;
		}
	}

	_grid->centerScreenOnActor();

	_redraw->redrawEngineActions(_redraw->_reqBgRedraw);

	// workaround to fix hero redraw after drowning
	if (_actor->_cropBottomScreen && _redraw->_reqBgRedraw) {
		_scene->_sceneHero->_staticFlags.bIsHidden = 1;
		_redraw->redrawEngineActions(true);
		_scene->_sceneHero->_staticFlags.bIsHidden = 0;
	}

	_scene->_needChangeScene = SCENE_CEILING_GRID_FADE_1;
	_redraw->_reqBgRedraw = false;

	return 0;
}

bool TwinEEngine::gameEngineLoop() {
	_redraw->_reqBgRedraw = true;
	_screens->_lockPalette = true;
	_movements->setActorAngle(ANGLE_0, -ANGLE_90, ANGLE_1, &_loopMovePtr);

	while (_quitGame == -1) {
		uint32 start = g_system->getMillis();

		while (g_system->getMillis() < start + _cfgfile.Fps) {
			if (runGameEngine()) {
				return true;
			}
			g_system->delayMillis(1);
		}
		_lbaTime++;
		if (shouldQuit()) {
			break;
		}
	}
	return false;
}

bool TwinEEngine::delaySkip(uint32 time) {
	uint32 startTicks = _system->getMillis();
	uint32 stopTicks = 0;
	do {
		FrameMarker frame(this);
		readKeys();
		if (_input->toggleAbortAction()) {
			return true;
		}
		if (shouldQuit()) {
			return true;
		}
		stopTicks = _system->getMillis() - startTicks;
		//_lbaTime++;
	} while (stopTicks <= time);
	return false;
}

void TwinEEngine::saveFrontBuffer() {
	_screens->copyScreen(_frontVideoBuffer, _workVideoBuffer);
}

void TwinEEngine::restoreFrontBuffer() {
	_screens->copyScreen(_workVideoBuffer, _frontVideoBuffer);
}

void TwinEEngine::blitWorkToFront(const Common::Rect &rect) {
	_interface->blitBox(rect, _workVideoBuffer, _frontVideoBuffer);
	copyBlockPhys(rect);
}

void TwinEEngine::blitFrontToWork(const Common::Rect &rect) {
	_interface->blitBox(rect, _frontVideoBuffer, _workVideoBuffer);
}

void TwinEEngine::setPalette(const uint32 *palette) {
	uint8 pal[NUMOFCOLORS * 3];
	uint8 *out = pal;
	const uint8 *in = (const uint8 *)palette;
	for (int i = 0; i < NUMOFCOLORS; i++) {
		out[0] = in[0];
		out[1] = in[1];
		out[2] = in[2];
		out += 3;
		in += 4;
	}
	setPalette(0, NUMOFCOLORS, pal);
}

void TwinEEngine::setPalette(uint startColor, uint numColors, const byte *palette) {
	_frontVideoBuffer.setPalette(palette, startColor, numColors);
}

void TwinEEngine::copyBlockPhys(const Common::Rect &rect) {
	copyBlockPhys(rect.left, rect.top, rect.right, rect.bottom);
}

void TwinEEngine::copyBlockPhys(int32 left, int32 top, int32 right, int32 bottom) {
	assert(left <= right);
	assert(top <= bottom);
	int32 width = right - left + 1;
	int32 height = bottom - top + 1;
	if (left + width > this->width()) {
		width = this->width() - left;
	}
	if (top + height > this->height()) {
		height = this->height() - top;
	}
	if (width <= 0 || height <= 0) {
		return;
	}
	_frontVideoBuffer.addDirtyRect(Common::Rect(left, top, right, bottom));
}

void TwinEEngine::crossFade(const uint32 *palette) {
	Graphics::ManagedSurface backupSurface;
	Graphics::ManagedSurface newSurface;
	Graphics::ManagedSurface tempSurface;
	Graphics::ManagedSurface surfaceTable;

	Graphics::PixelFormat fmt(4, 8, 8, 8, 8, 24, 16, 8, 0);
	backupSurface.create(_frontVideoBuffer.w, _frontVideoBuffer.h, fmt);
	newSurface.create(_frontVideoBuffer.w, _frontVideoBuffer.h, fmt);
	tempSurface.create(_frontVideoBuffer.w, _frontVideoBuffer.h, Graphics::PixelFormat::createFormatCLUT8());
	tempSurface.setPalette(palette, 0, NUMOFCOLORS);

	surfaceTable.create(_frontVideoBuffer.w, _frontVideoBuffer.h, fmt);

	backupSurface.transBlitFrom(_frontVideoBuffer);
	newSurface.transBlitFrom(tempSurface);

	for (int32 i = 0; i < 8; i++) {
		surfaceTable.blitFrom(backupSurface);
		surfaceTable.transBlitFrom(newSurface, 0, false, 0, i * NUMOFCOLORS / 8);
		_frontVideoBuffer.blitFrom(surfaceTable);
		delaySkip(50);
	}

	_frontVideoBuffer.blitFrom(newSurface);

	backupSurface.free();
	newSurface.free();
	tempSurface.free();
	surfaceTable.free();
}

void TwinEEngine::readKeys() {
	_input->readKeys();
}

void TwinEEngine::drawText(int32 x, int32 y, const Common::String &text, bool center, bool bigFont, int width) {
	const Graphics::Font *font = FontMan.getFontByUsage(bigFont ? Graphics::FontManager::kBigGUIFont : Graphics::FontManager::kGUIFont);
	if (!font) {
		return;
	}
	font->drawString(&_frontVideoBuffer, text,
	                 x, y, width,
	                 _frontVideoBuffer.format.RGBToColor(255, 255, 255),
	                 center ? Graphics::kTextAlignCenter : Graphics::kTextAlignLeft, 0, true);
}

const char *TwinEEngine::getGameId() const {
	if (isLBA1()) {
		return "lba";
	}
	assert(isLBA2());
	return "lba2";
}

bool TwinEEngine::unlockAchievement(const Common::String &id) {
	return AchMan.setAchievement(id);
}

Common::Rect TwinEEngine::centerOnScreenX(int32 w, int32 y, int32 h) const {
	const int32 left = width() / 2 - w / 2;
	const int32 right = left + w;
	const int32 top = y;
	const int32 bottom = top + h;
	return Common::Rect(left, top, right, bottom);
}

Common::Rect TwinEEngine::centerOnScreen(int32 w, int32 h) const {
	const int32 left = width() / 2 - w / 2;
	const int32 right = left + w;
	const int32 top = height() / 2 - h / 2;
	const int32 bottom = top + h;
	return Common::Rect(left, top, right, bottom);
}

} // namespace TwinE
