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

#include "twine/scene/scene.h"
#include "common/memstream.h"
#include "common/stream.h"
#include "common/util.h"
#include "twine/audio/music.h"
#include "twine/audio/sound.h"
#include "twine/debugger/debug_grid.h"
#include "twine/debugger/debug_scene.h"
#include "twine/renderer/redraw.h"
#include "twine/renderer/renderer.h"
#include "twine/renderer/screens.h"
#include "twine/resources/resources.h"
#include "twine/scene/actor.h"
#include "twine/scene/animations.h"
#include "twine/scene/extra.h"
#include "twine/scene/gamestate.h"
#include "twine/scene/grid.h"
#include "twine/scene/movements.h"
#include "twine/text.h"
#include "twine/twine.h"

namespace TwinE {

Scene::~Scene() {
	free(_currentScene);
}

void Scene::setActorStaticFlags(ActorStruct *act, uint32 staticFlags) {
	if (staticFlags & 0x1) {
		act->_staticFlags.bComputeCollisionWithObj = 1;
	}
	if (staticFlags & 0x2) {
		act->_staticFlags.bComputeCollisionWithBricks = 1;
	}
	if (staticFlags & 0x4) {
		act->_staticFlags.bIsZonable = 1;
	}
	if (staticFlags & 0x8) {
		act->_staticFlags.bUsesClipping = 1;
	}
	if (staticFlags & 0x10) {
		act->_staticFlags.bCanBePushed = 1;
	}
	if (staticFlags & 0x20) {
		act->_staticFlags.bComputeLowCollision = 1;
	}
	if (staticFlags & 0x40) {
		act->_staticFlags.bCanDrown = 1;
	}
	if (staticFlags & 0x80) {
		act->_staticFlags.bComputeCollisionWithFloor = 1;
	}

	if (staticFlags & 0x100) {
		act->_staticFlags.bUnk0100 = 1;
	}
	if (staticFlags & 0x200) {
		act->_staticFlags.bIsHidden = 1;
	}
	if (staticFlags & 0x400) {
		act->_staticFlags.bIsSpriteActor = 1;
	}
	if (staticFlags & 0x800) {
		act->_staticFlags.bCanFall = 1;
	}
	if (staticFlags & 0x1000) {
		act->_staticFlags.bDoesntCastShadow = 1;
	}
	if (staticFlags & 0x2000) {
		//act->staticFlags.bIsBackgrounded = 1;
	}
	if (staticFlags & 0x4000) {
		act->_staticFlags.bIsCarrierActor = 1;
	}
	if (staticFlags & 0x8000) {
		act->_staticFlags.bUseMiniZv = 1;
	}
	if (staticFlags & 0x10000) {
		act->_staticFlags.bHasInvalidPosition = 1;
	}
	if (staticFlags & 0x20000) {
		act->_staticFlags.bNoElectricShock = 1;
	}
	if (staticFlags & 0x40000) {
		act->_staticFlags.bHasSpriteAnim3D = 1;
	}
	if (staticFlags & 0x80000) {
		act->_staticFlags.bNoPreClipping = 1;
	}
	if (staticFlags & 0x100000) {
		act->_staticFlags.bHasZBuffer = 1;
	}
	if (staticFlags & 0x200000) {
		act->_staticFlags.bHasZBufferInWater = 1;
	}
}

void Scene::setBonusParameterFlags(ActorStruct *act, uint16 bonusFlags) {
	if (bonusFlags & 0x1) {
		act->_bonusParameter.unk1 = 1;
	}
	if (bonusFlags & 0x2) {
		act->_bonusParameter.unk2 = 1;
	}
	if (bonusFlags & 0x4) {
		act->_bonusParameter.unk3 = 1;
	}
	if (bonusFlags & 0x8) {
		act->_bonusParameter.unk4 = 1;
	}
	if (bonusFlags & 0x10) {
		act->_bonusParameter.kashes = 1;
	}
	if (bonusFlags & 0x20) {
		act->_bonusParameter.lifepoints = 1;
	}
	if (bonusFlags & 0x40) {
		act->_bonusParameter.magicpoints = 1;
	}
	if (bonusFlags & 0x80) {
		act->_bonusParameter.key = 1;
	}
	if (bonusFlags & 0x100) {
		act->_bonusParameter.cloverleaf = 1;
	}
}

bool Scene::loadSceneLBA2() {
	Common::MemoryReadStream stream(_currentScene, _currentSceneSize);
	_sceneTextBank = (TextBankId)stream.readByte();
	_currentGameOverScene = stream.readByte();
	stream.skip(4);

	_alphaLight = ClampAngle(stream.readUint16LE());
	_betaLight = ClampAngle(stream.readUint16LE());
	debug(2, "Using %i and %i as light vectors", _alphaLight, _betaLight);

	_isOutsideScene = stream.readByte();

	for (int i = 0; i < 4; ++i) {
		_sampleAmbiance[i] = stream.readUint16LE();
		_sampleRepeat[i] = stream.readUint16LE();
		_sampleRound[i] = stream.readUint16LE();
		_sampleFrequency[i] = stream.readUint16LE();
		_sampleVolume[i] = stream.readUint16LE();
	}

	_sampleMinDelay = stream.readUint16LE();
	_sampleMinDelayRnd = stream.readUint16LE();

	_sceneMusic = stream.readByte();

	// load hero properties
	_sceneHeroPos.x = stream.readSint16LE();
	_sceneHeroPos.y = stream.readSint16LE();
	_sceneHeroPos.z = stream.readSint16LE();

	_sceneHero->_moveScriptSize = stream.readUint16LE();
	_sceneHero->_moveScript = _currentScene + stream.pos();
	stream.skip(_sceneHero->_moveScriptSize);

	_sceneHero->_lifeScriptSize = stream.readUint16LE();
	_sceneHero->_lifeScript = _currentScene + stream.pos();
	stream.skip(_sceneHero->_lifeScriptSize);

	_sceneNumActors = stream.readUint16LE();
	int cnt = 1;
	for (int32 a = 1; a < _sceneNumActors; a++, cnt++) {
		_engine->_actor->resetActor(a);
		ActorStruct *act = &_sceneActors[a];
		setActorStaticFlags(act, stream.readUint32LE());

		act->loadModel(stream.readUint16LE());

		act->_body = (BodyType)stream.readSint16LE();
		act->_anim = (AnimationTypes)stream.readByte();
		act->_sprite = stream.readUint16LE();
		act->_pos.x = stream.readUint16LE();
		act->_pos.y = stream.readUint16LE();
		act->_pos.z = stream.readUint16LE();
		act->_collisionPos = act->pos();
		act->_strengthOfHit = stream.readByte();
		setBonusParameterFlags(act, stream.readUint16LE());
		act->_angle = stream.readUint16LE();
		act->_speed = stream.readUint16LE();
		act->_controlMode = (ControlMode)stream.readByte();
		act->_cropLeft = stream.readSint16LE();
		act->_delayInMillis = act->_cropLeft; // TODO: this might not be needed
		act->_cropTop = stream.readSint16LE();
		act->_cropRight = stream.readSint16LE();
		act->_cropBottom = stream.readSint16LE();
		act->_followedActor = act->_cropBottom; // TODO: is this needed? and valid?
		act->_bonusAmount = stream.readSint16LE();
		act->_talkColor = stream.readByte();
		if (act->_staticFlags.bHasSpriteAnim3D) {
			/*act->spriteAnim3DNumber = */stream.readSint32LE();
			/*act->spriteSizeHit = */stream.readSint16LE();
			/*act->cropBottom = act->spriteSizeHit;*/
		}
		act->_armor = stream.readByte();
		act->setLife(stream.readByte());

		act->_moveScriptSize = stream.readUint16LE();
		act->_moveScript = _currentScene + stream.pos();
		stream.skip(act->_moveScriptSize);

		act->_lifeScriptSize = stream.readUint16LE();
		act->_lifeScript = _currentScene + stream.pos();
		stream.skip(act->_lifeScriptSize);

		if (_engine->_debugScene->_onlyLoadActor != -1 && _engine->_debugScene->_onlyLoadActor != cnt) {
			_sceneNumActors--;
			a--;
		}
	}

	_sceneNumZones = stream.readUint16LE();
	for (int32 i = 0; i < _sceneNumZones; i++) {
		ZoneStruct *zone = &_sceneZones[i];
		zone->mins.x = stream.readSint32LE();
		zone->mins.y = stream.readSint32LE();
		zone->mins.z = stream.readSint32LE();

		zone->maxs.x = stream.readSint32LE();
		zone->maxs.y = stream.readSint32LE();
		zone->maxs.z = stream.readSint32LE();

		zone->infoData.generic.info0 = stream.readSint32LE();
		zone->infoData.generic.info1 = stream.readSint32LE();
		zone->infoData.generic.info2 = stream.readSint32LE();
		zone->infoData.generic.info3 = stream.readSint32LE();
		zone->infoData.generic.info4 = stream.readSint32LE();
		zone->infoData.generic.info5 = stream.readSint32LE();
		zone->infoData.generic.info6 = stream.readSint32LE();
		zone->infoData.generic.info7 = stream.readSint32LE();

		zone->type = (ZoneType)stream.readUint16LE();
		zone->snap = stream.readUint16LE();
	}

	_sceneNumTracks = stream.readUint16LE();
	for (int32 i = 0; i < _sceneNumTracks; i++) {
		IVec3 *point = &_sceneTracks[i];
		point->x = stream.readSint32LE();
		point->y = stream.readSint32LE();
		point->z = stream.readSint32LE();
	}

	int32 sceneNumPatches = stream.readUint16LE();
	for (int32 i = 0; i < sceneNumPatches; i++) {
		/*size = */stream.readUint16LE();
		/*offset = */stream.readUint16LE();
	}

	return true;
}

bool Scene::loadSceneLBA1() {
	Common::MemoryReadStream stream(_currentScene, _currentSceneSize);

	// load scene ambience properties
	_sceneTextBank = (TextBankId)stream.readByte();
	_currentGameOverScene = stream.readByte();
	stream.skip(4);

	// FIXME: Workaround to fix lighting issue - not using proper dark light
	// Using 1215 and 1087 as light vectors - scene 8
	_alphaLight = ClampAngle(stream.readUint16LE());
	_betaLight = ClampAngle(stream.readUint16LE());
	debug(2, "Using %i and %i as light vectors", _alphaLight, _betaLight);

	for (int i = 0; i < 4; ++i) {
		_sampleAmbiance[i] = stream.readUint16LE();
		_sampleRepeat[i] = stream.readUint16LE();
		_sampleRound[i] = stream.readUint16LE();
	}

	_sampleMinDelay = stream.readUint16LE();
	_sampleMinDelayRnd = stream.readUint16LE();

	_sceneMusic = stream.readByte();

	// load hero properties
	_sceneHeroPos.x = stream.readUint16LE();
	_sceneHeroPos.y = stream.readUint16LE();
	_sceneHeroPos.z = stream.readUint16LE();

	_sceneHero->_moveScriptSize = stream.readUint16LE();
	_sceneHero->_moveScript = _currentScene + stream.pos();
	stream.skip(_sceneHero->_moveScriptSize);

	_sceneHero->_lifeScriptSize = stream.readUint16LE();
	_sceneHero->_lifeScript = _currentScene + stream.pos();
	stream.skip(_sceneHero->_lifeScriptSize);

	_sceneNumActors = stream.readUint16LE();
	int cnt = 1;
	for (int32 a = 1; a < _sceneNumActors; a++, cnt++) {
		_engine->_actor->resetActor(a);

		ActorStruct *act = &_sceneActors[a];
		setActorStaticFlags(act, stream.readUint16LE());

		act->loadModel(stream.readUint16LE());

		act->_body = (BodyType)stream.readByte();
		act->_anim = (AnimationTypes)stream.readByte();
		act->_sprite = stream.readUint16LE();
		act->_pos.x = stream.readUint16LE();
		act->_pos.y = stream.readUint16LE();
		act->_pos.z = stream.readUint16LE();
		act->_collisionPos = act->pos();
		act->_strengthOfHit = stream.readByte();
		setBonusParameterFlags(act, stream.readUint16LE());
		act->_angle = stream.readUint16LE();
		act->_speed = stream.readUint16LE();
		act->_controlMode = (ControlMode)stream.readUint16LE();
		act->_cropLeft = stream.readSint16LE();
		act->_delayInMillis = act->_cropLeft; // TODO: this might not be needed
		act->_cropTop = stream.readSint16LE();
		act->_cropRight = stream.readSint16LE();
		act->_cropBottom = stream.readSint16LE();
		act->_followedActor = act->_cropBottom; // TODO: is this needed? and valid?
		act->_bonusAmount = stream.readByte();
		act->_talkColor = stream.readByte();
		act->_armor = stream.readByte();
		act->setLife(stream.readByte());

		act->_moveScriptSize = stream.readUint16LE();
		act->_moveScript = _currentScene + stream.pos();
		stream.skip(act->_moveScriptSize);

		act->_lifeScriptSize = stream.readUint16LE();
		act->_lifeScript = _currentScene + stream.pos();
		stream.skip(act->_lifeScriptSize);

		if (_engine->_debugScene->_onlyLoadActor != -1 && _engine->_debugScene->_onlyLoadActor != cnt) {
			_sceneNumActors--;
			a--;
		}
	}

	_sceneNumZones = stream.readUint16LE();
	for (int32 i = 0; i < _sceneNumZones; i++) {
		ZoneStruct *zone = &_sceneZones[i];
		zone->mins.x = stream.readUint16LE();
		zone->mins.y = stream.readUint16LE();
		zone->mins.z = stream.readUint16LE();

		zone->maxs.x = stream.readUint16LE();
		zone->maxs.y = stream.readUint16LE();
		zone->maxs.z = stream.readUint16LE();

		zone->type = (ZoneType)stream.readUint16LE();

		zone->infoData.generic.info0 = stream.readUint16LE();
		zone->infoData.generic.info1 = stream.readUint16LE();
		zone->infoData.generic.info2 = stream.readUint16LE();
		zone->infoData.generic.info3 = stream.readUint16LE();

		zone->snap = stream.readUint16LE();
	}

	_sceneNumTracks = stream.readUint16LE();
	for (int32 i = 0; i < _sceneNumTracks; i++) {
		IVec3 *point = &_sceneTracks[i];
		point->x = stream.readUint16LE();
		point->y = stream.readUint16LE();
		point->z = stream.readUint16LE();
	}

	if (_engine->_debugScene->_useScenePatches) {
		// TODO: these were found in the disassembly and might be some script fixes - check me and activate me
		switch (_currentSceneIdx) {
		case LBA1SceneId::Hamalayi_Mountains_landing_place:
			assert(_sceneNumActors >= 22);
			_sceneActors[21]._pos.x = _sceneActors[21]._collisionPos.x = 0x1b00;
			_sceneActors[21]._pos.z = _sceneActors[21]._collisionPos.z = 0x300;
			break;
#if 0
		case LBA1SceneId::Principal_Island_outside_the_fortress:
			assert(sceneNumActors >= 30);
			_sceneActors[29].pos.z = _sceneActors[29].collisionPos.z = 0x703;
			assert(sceneNumZones >= 23);
			// each scene zone entry has 24 bytes
			sceneZones[15].mins.y = 0x450; // [zone:15] 362 (mod:2) offset relative to sceneNumZones
			sceneZones[15].type = 0x2ce0; // [zone:15] 372 (mod:12) offset relative to sceneNumZones
			sceneZones[16].mins.y = 0x5270; // [zone:16] 386 (mod:2) offset relative to sceneNumZones
			sceneZones[16].type = 0x1f90; // [zone:16] 396 (mod:12) offset relative to sceneNumZones
			sceneZones[22].mins.y = 0x1800; // [zone:22] 530 (mod:2) offset relative to sceneNumZones
			sceneZones[15].maxs.x = 0x10f0;
			sceneZones[15].maxs.y = 0x2100; // [zone:15] 366 (mod:6) offset relative to sceneNumZones (4 bytes)
			sceneZones[16].maxs.x = 0x5d10;
			sceneZones[16].maxs.y = 0x1200; // [zone:16] 390 (mod:6) offset relative to sceneNumZones (4 bytes)
			sceneZones[22].maxs.x = 0x22a1;
			sceneZones[22].maxs.y = 0x1800; // [zone:22] 534 (mod:6) offset relative to sceneNumZones (4 bytes)
			sceneZones[22].type = 0x1ae1; // [zone:22] 540 (mod:12) offset relative to sceneNumZones
			break;
		case LBA1SceneId::Tippet_Island_Secret_passage_scene_1:
			// puVar4 is the position of sceneNumZones
			//(ushort*)puVar4[78] = 0xe20;
			break;
		case LBA1SceneId::Principal_Island_inside_the_fortress:
			//(ushort*)puVar4[140] = 0x32;
			break;
#endif
		}
	}

	return true;
}

bool Scene::initScene(int32 index) {
	// load scene from file
	_currentSceneSize = HQR::getAllocEntry(&_currentScene, Resources::HQR_SCENE_FILE, index);
	if (_currentSceneSize == 0) {
		return false;
	}

	if (_engine->isLBA1()) {
		return loadSceneLBA1();
	} else if (_engine->isLBA2()) {
		return loadSceneLBA2();
	}

	return false;
}

void Scene::resetScene() {
	_engine->_extra->resetExtras();

	for (int32 i = 0; i < ARRAYSIZE(_sceneFlags); i++) {
		_sceneFlags[i] = 0;
	}

	for (int32 i = 0; i < OVERLAY_MAX_ENTRIES; i++) {
		_engine->_redraw->overlayList[i].info0 = -1;
	}

	_engine->_screens->_useAlternatePalette = false;
}

void Scene::reloadCurrentScene() {
	_needChangeScene = _currentSceneIdx;
}

void Scene::changeScene() {
	// change twinsen house destroyed hard-coded
	if (_needChangeScene == LBA1SceneId::Citadel_Island_near_twinsens_house && _engine->_gameState->hasOpenedFunfrocksSafe()) {
		_needChangeScene = LBA1SceneId::Citadel_Island_Twinsens_house_destroyed;
	}

	// local backup previous scene
	_previousSceneIdx = _currentSceneIdx;
	_currentSceneIdx = _needChangeScene;

	if (_engine->isLBA1() && _currentSceneIdx >= LBA1SceneId::Citadel_Island_Prison && _currentSceneIdx < LBA1SceneId::SceneIdMax) {
		snprintf(_engine->_gameState->_sceneName, sizeof(_engine->_gameState->_sceneName), "%i %s", _currentSceneIdx, _engine->_holomap->getLocationName(_currentSceneIdx));
	} else {
		snprintf(_engine->_gameState->_sceneName, sizeof(_engine->_gameState->_sceneName), "%i", _currentSceneIdx);
	}
	debug(2, "Entering scene %s (came from %i)", _engine->_gameState->_sceneName, _previousSceneIdx);

	if (_needChangeScene == LBA1SceneId::Polar_Island_end_scene) {
		_engine->unlockAchievement("LBA_ACH_001");
		// if you finish the game in less than 4 hours
		if (_engine->getTotalPlayTime() <= 1000 * 60 * 60 * 4) {
			_engine->unlockAchievement("LBA_ACH_005");
		}
	} else if (_needChangeScene == LBA1SceneId::Brundle_Island_Secret_room) {
		_engine->unlockAchievement("LBA_ACH_006");
	}

	_engine->_sound->stopSamples();

	resetScene();
	_engine->_actor->loadHeroEntities();

	_sceneHero->_controlMode = ControlMode::kManual;
	_sceneHero->_zone = -1;
	_sceneHero->_positionInLifeScript = 0;
	_sceneHero->_positionInMoveScript = -1;
	_sceneHero->_labelIdx = -1;

	initScene(_needChangeScene);

	if (_holomapTrajectory != -1) {
		_engine->_holomap->drawHolomapTrajectory(_holomapTrajectory);
		_holomapTrajectory = -1;
	}

	if (_needChangeScene == LBA1SceneId::Citadel_Island_end_sequence_1 || _needChangeScene == LBA1SceneId::Citadel_Island_end_sequence_2) {
		_sceneTextBank = TextBankId::Tippet_Island;
	}

	_engine->_text->initSceneTextBank();
	_engine->_grid->initGrid(_needChangeScene);

	if (_heroPositionType == ScenePositionType::kZone) {
		_newHeroPos = _zoneHeroPos;
	}

	if (_heroPositionType == ScenePositionType::kScene || _heroPositionType == ScenePositionType::kNoPosition) {
		_newHeroPos = _sceneHeroPos;
	}

	_sceneHero->_pos = _newHeroPos;
	_heroYBeforeFall = _newHeroPos.y;

	_engine->_renderer->setLightVector(_alphaLight, _betaLight, ANGLE_0);

	if (_previousSceneIdx != SCENE_CEILING_GRID_FADE_1 && _previousSceneIdx != _needChangeScene) {
		_engine->_actor->_previousHeroBehaviour = _engine->_actor->_heroBehaviour;
		_engine->_actor->_previousHeroAngle = _sceneHero->_angle;
		_engine->autoSave();
	}

	_engine->_actor->restartHeroScene();

	for (int32 a = 1; a < _sceneNumActors; a++) {
		_engine->_actor->initActor(a);
	}

	_engine->_gameState->_inventoryNumKeys = 0;
	_engine->_disableScreenRecenter = false;
	_heroPositionType = ScenePositionType::kNoPosition;
	_sampleAmbienceTime = 0;

	ActorStruct *followedActor = getActor(_currentlyFollowedActor);
	_engine->_grid->centerOnActor(followedActor);

	_engine->_gameState->_magicBallIdx = -1;
	_engine->_movements->_heroMoved = true;
	_engine->_grid->_useCellingGrid = -1;
	_engine->_grid->_cellingGridIdx = -1;
	_engine->_screens->_lockPalette = false;

	_needChangeScene = SCENE_CEILING_GRID_FADE_1;
	_enableGridTileRendering = true;

	_engine->_renderer->setLightVector(_alphaLight, _betaLight, ANGLE_0);

	if (_sceneMusic != -1) {
		debug(2, "Scene %i music track id: %i", _currentSceneIdx, _sceneMusic);
		_engine->_music->playTrackMusic(_sceneMusic);
	}
}

ActorStruct *Scene::getActor(int32 actorIdx) {
	assert(actorIdx >= 0);
	assert(actorIdx < NUM_MAX_ACTORS);
	return &_sceneActors[actorIdx];
}

void Scene::initSceneVars() {
	_sampleAmbiance[0] = -1;
	_sampleAmbiance[1] = -1;
	_sampleAmbiance[2] = -1;
	_sampleAmbiance[3] = -1;

	_sampleRepeat[0] = 0;
	_sampleRepeat[1] = 0;
	_sampleRepeat[2] = 0;
	_sampleRepeat[3] = 0;

	_sampleRound[0] = 0;
	_sampleRound[1] = 0;
	_sampleRound[2] = 0;
	_sampleRound[3] = 0;

	_sceneNumActors = 0;
	_sceneNumZones = 0;
	_sceneNumTracks = 0;
}

void Scene::playSceneMusic() {
	if (_currentSceneIdx == LBA1SceneId::Tippet_Island_Twinsun_Cafe && _engine->_gameState->hasArrivedHamalayi()) {
		_engine->_music->playMidiMusic(8);
	} else {
		_engine->_music->playMidiMusic(_sceneMusic);
	}
}

void Scene::processEnvironmentSound() {
	if (_engine->_lbaTime >= _sampleAmbienceTime) {
		int16 currentAmb = _engine->getRandomNumber(4); // random ambiance

		for (int32 s = 0; s < 4; s++) {
			if (!(_samplePlayed & (1 << currentAmb))) { // if not already played
				_samplePlayed |= (1 << currentAmb);     // make sample played

				if (_samplePlayed == 15) { // reset if all samples played
					_samplePlayed = 0;
				}

				const int16 sampleIdx = _sampleAmbiance[currentAmb];
				if (sampleIdx != -1) {
					/*int16 decal = _sampleRound[currentAmb];*/
					int16 repeat = _sampleRepeat[currentAmb];

					_engine->_sound->playSample(sampleIdx, repeat, 110, -1, 110);
					break;
				}
			}

			currentAmb++;    // try next ambiance
			currentAmb &= 3; // loop in all 4 ambiances
		}

		// compute next ambiance timer
		_sampleAmbienceTime = _engine->_lbaTime + (_engine->getRandomNumber(_sampleMinDelayRnd) + _sampleMinDelay) * 50;
	}
}

void Scene::processZoneExtraBonus(ZoneStruct *zone) {
	if (zone->infoData.Bonus.used) {
		return;
	}

	const int bonusSprite = _engine->_extra->getBonusSprite(zone->infoData.Bonus.typesFlag);
	if (bonusSprite == -1) {
		return;
	}

	const int16 amount = zone->infoData.Bonus.amount;
	const int32 angle = _engine->_movements->getAngleAndSetTargetActorDistance(ABS(zone->maxs.x + zone->mins.x) / 2, ABS(zone->maxs.z + zone->mins.z) / 2, _sceneHero->_pos.x, _sceneHero->_pos.z);
	const int32 index = _engine->_extra->addExtraBonus(ABS(zone->maxs.x + zone->mins.x) / 2, zone->maxs.y, ABS(zone->maxs.z + zone->mins.z) / 2, ANGLE_63, angle, bonusSprite, amount);

	if (index != -1) {
		_engine->_extra->_extraList[index].type |= ExtraType::TIME_IN;
		zone->infoData.Bonus.used = 1; // set as used
	}
}

void Scene::processActorZones(int32 actorIdx) {
	ActorStruct *actor = &_sceneActors[actorIdx];

	int32 currentX = actor->_pos.x;
	int32 currentY = actor->_pos.y;
	int32 currentZ = actor->_pos.z;

	actor->_zone = -1;
	bool tmpCellingGrid = false;

	if (IS_HERO(actorIdx)) {
		_currentActorInZone = false;
	}

	for (int32 z = 0; z < _sceneNumZones; z++) {
		ZoneStruct *zone = &_sceneZones[z];

		// check if actor is in zone
		if ((currentX >= zone->mins.x && currentX <= zone->maxs.x) &&
		    (currentY >= zone->mins.y && currentY <= zone->maxs.y) &&
		    (currentZ >= zone->mins.z && currentZ <= zone->maxs.z)) {
			switch (zone->type) {
			case ZoneType::kCube:
				if (IS_HERO(actorIdx) && actor->_life > 0) {
					_needChangeScene = zone->infoData.ChangeScene.newSceneIdx;
					_zoneHeroPos.x = actor->_pos.x - zone->mins.x + zone->infoData.ChangeScene.x;
					_zoneHeroPos.y = actor->_pos.y - zone->mins.y + zone->infoData.ChangeScene.y;
					_zoneHeroPos.z = actor->_pos.z - zone->mins.z + zone->infoData.ChangeScene.z;
					_heroPositionType = ScenePositionType::kZone;
				}
				break;
			case ZoneType::kCamera:
				if (_currentlyFollowedActor == actorIdx && !_engine->_debugGrid->_useFreeCamera) {
					_engine->_disableScreenRecenter = true;
					if (_engine->_grid->_newCamera.x != zone->infoData.CameraView.x || _engine->_grid->_newCamera.y != zone->infoData.CameraView.y || _engine->_grid->_newCamera.z != zone->infoData.CameraView.z) {
						_engine->_grid->_newCamera.x = zone->infoData.CameraView.x;
						_engine->_grid->_newCamera.y = zone->infoData.CameraView.y;
						_engine->_grid->_newCamera.z = zone->infoData.CameraView.z;
						_engine->_redraw->_reqBgRedraw = true;
					}
				}
				break;
			case ZoneType::kSceneric:
				actor->_zone = zone->infoData.Sceneric.zoneIdx;
				break;
			case ZoneType::kGrid:
				if (_currentlyFollowedActor == actorIdx) {
					tmpCellingGrid = true;
					if (_engine->_grid->_useCellingGrid != zone->infoData.CeillingGrid.newGrid) {
						if (zone->infoData.CeillingGrid.newGrid != -1) {
							_engine->_grid->createGridMap();
						}

						_engine->_grid->_useCellingGrid = zone->infoData.CeillingGrid.newGrid;
						_engine->_grid->_cellingGridIdx = z;
						ScopedEngineFreeze freeze(_engine);
						_engine->_grid->initCellingGrid(_engine->_grid->_useCellingGrid);
					}
				}
				break;
			case ZoneType::kObject:
				if (IS_HERO(actorIdx) && _engine->_movements->shouldTriggerZoneAction()) {
					_engine->_animations->initAnim(AnimationTypes::kAction, AnimType::kAnimationType_1, AnimationTypes::kStanding, 0);
					processZoneExtraBonus(zone);
				}
				break;
			case ZoneType::kText:
				if (IS_HERO(actorIdx) && _engine->_movements->shouldTriggerZoneAction()) {
					ScopedEngineFreeze scopedFreeze(_engine);
					_engine->exitSceneryView();
					_engine->_text->setFontCrossColor(zone->infoData.DisplayText.textColor);
					_talkingActor = actorIdx;
					_engine->_text->drawTextProgressive(zone->infoData.DisplayText.textIdx);
					_engine->_redraw->redrawEngineActions(true);
				}
				break;
			case ZoneType::kLadder:
				if (IS_HERO(actorIdx) && _engine->_actor->_heroBehaviour != HeroBehaviourType::kProtoPack && (actor->_anim == AnimationTypes::kForward || actor->_anim == AnimationTypes::kTopLadder || actor->_anim == AnimationTypes::kClimbLadder)) {
					_engine->_movements->rotateActor(actor->_boudingBox.mins.x, actor->_boudingBox.mins.z, actor->_angle + ANGLE_360 + ANGLE_135);
					_engine->_renderer->_destPos.x += _engine->_movements->_processActor.x;
					_engine->_renderer->_destPos.z += _engine->_movements->_processActor.z;

					if (_engine->_renderer->_destPos.x >= 0 && _engine->_renderer->_destPos.z >= 0 && _engine->_renderer->_destPos.x <= 0x7E00 && _engine->_renderer->_destPos.z <= 0x7E00) {
						if (_engine->_grid->getBrickShape(_engine->_renderer->_destPos.x, actor->_pos.y + ANGLE_90, _engine->_renderer->_destPos.z) != ShapeType::kNone) {
							_currentActorInZone = true;
							if (actor->_pos.y >= ABS(zone->mins.y + zone->maxs.y) / 2) {
								_engine->_animations->initAnim(AnimationTypes::kTopLadder, AnimType::kAnimationType_2, AnimationTypes::kStanding, actorIdx); // reached end of ladder
							} else {
								_engine->_animations->initAnim(AnimationTypes::kClimbLadder, AnimType::kAnimationTypeLoop, AnimationTypes::kAnimInvalid, actorIdx); // go up in ladder
							}
						}
					}
				}
				break;
			}
		}
	}

	if (!tmpCellingGrid && actorIdx == _currentlyFollowedActor && _engine->_grid->_useCellingGrid != -1) {
		_engine->_grid->_useCellingGrid = -1;
		_engine->_grid->_cellingGridIdx = -1;
		_engine->_grid->createGridMap();
		_engine->_redraw->_reqBgRedraw = true;
	}
}

void Scene::stopRunningGame() {
	free(_currentScene);
	_currentScene = nullptr;
}

} // namespace TwinE
