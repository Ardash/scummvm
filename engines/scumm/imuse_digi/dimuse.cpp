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

#include "common/system.h"
#include "common/timer.h"

#include "scumm/actor.h"
#include "scumm/scumm_v7.h"
#include "scumm/sound.h"
#include "scumm/imuse_digi/dimuse.h"
#include "scumm/imuse_digi/dimuse_bndmgr.h"
#include "scumm/imuse_digi/dimuse_codecs.h"
#include "scumm/imuse_digi/dimuse_track.h"
#include "scumm/imuse_digi/dimuse_tables.h"

#include "audio/audiostream.h"
#include "audio/mixer.h"
#include "audio/decoders/raw.h"

namespace Scumm {

void IMuseDigital::timer_handler(void *refCon) {
	IMuseDigital *imuseDigital = (IMuseDigital *)refCon;
	imuseDigital->callback();
}

IMuseDigital::IMuseDigital(ScummEngine_v7 *scumm, Audio::Mixer *mixer, int fps)
	: _vm(scumm), _mixer(mixer) {
	assert(_vm);
	assert(mixer);

	_pause = false;
	_sound = new ImuseDigiSndMgr(_vm);
	assert(_sound);
	_callbackFps = fps;
	if (_vm->_game.id == GID_FT)
		_callbackFps *= 2;
	resetState();
	for (int l = 0; l < MAX_DIGITAL_TRACKS + MAX_DIGITAL_FADETRACKS; l++) {
		_track[l] = new Track;
		assert(_track[l]);
		_track[l]->reset();
		_track[l]->trackId = l;
	}
	_vm->getTimerManager()->installTimerProc(timer_handler, 1000000 / _callbackFps, this, "IMuseDigital");

	_audioNames = NULL;
	_numAudioNames = 0;
}

IMuseDigital::~IMuseDigital() {
	_vm->getTimerManager()->removeTimerProc(timer_handler);
	stopAllSounds();
	for (int l = 0; l < MAX_DIGITAL_TRACKS + MAX_DIGITAL_FADETRACKS; l++) {
		delete _track[l];
	}
	delete _sound;
	free(_audioNames);
}

static int32 makeMixerFlags(Track *track) {
	const int32 flags = track->mixerFlags;
	int32 mixerFlags = 0;
	if (flags & kFlagUnsigned)
		mixerFlags |= Audio::FLAG_UNSIGNED;
	if (flags & kFlag16Bits)
		mixerFlags |= Audio::FLAG_16BITS;

#ifdef SCUMM_LITTLE_ENDIAN
	if (track->sndDataExtComp)
		mixerFlags |= Audio::FLAG_LITTLE_ENDIAN;
#endif
	if (track->littleEndian)
		mixerFlags |= Audio::FLAG_LITTLE_ENDIAN;
	if (flags & kFlagStereo)
		mixerFlags |= Audio::FLAG_STEREO;
	return mixerFlags;
}

void IMuseDigital::resetState() {
	_curMusicState = 0;
	_curMusicSeq = 0;
	_curMusicCue = 0;
	memset(_attributes, 0, sizeof(_attributes));
	_nextSeqToPlay = 0;
	_stopingSequence = 0;
	_radioChatterSFX = 0;
	_triggerUsed = false;
	_speechIsPlaying = false;
}

static void syncWithSerializer(Common::Serializer &s, Track &t) {
	s.syncAsSByte(t.pan, VER(31));
	s.syncAsSint32LE(t.vol, VER(31));
	s.syncAsSint32LE(t.volFadeDest, VER(31));
	s.syncAsSint32LE(t.volFadeStep, VER(31));
	s.syncAsSint32LE(t.volFadeDelay, VER(31));
	s.syncAsByte(t.volFadeUsed, VER(31));
	s.syncAsSint32LE(t.soundId, VER(31));
	s.syncArray(t.soundName, 15, Common::Serializer::SByte, VER(31));
	s.syncAsByte(t.used, VER(31));
	s.syncAsByte(t.toBeRemoved, VER(31));
	s.syncAsByte(t.souStreamUsed, VER(31));
	s.skip(1, VER(31), VER(76)); // mixerStreamRunning
	s.syncAsSint32LE(t.soundPriority, VER(31));
	s.syncAsSint32LE(t.regionOffset, VER(31));
	s.skip(4, VER(31), VER(31)); // trackOffset
	s.syncAsSint32LE(t.dataOffset, VER(31));
	s.syncAsSint32LE(t.curRegion, VER(31));
	s.syncAsSint32LE(t.curHookId, VER(31));
	s.syncAsSint32LE(t.volGroupId, VER(31));
	s.syncAsSint32LE(t.soundType, VER(31));
	s.syncAsSint32LE(t.feedSize, VER(31));
	s.syncAsSint32LE(t.dataMod12Bit, VER(31));
	s.syncAsSint32LE(t.mixerFlags, VER(31));
	s.skip(4, VER(31), VER(42)); // mixerVol
	s.skip(4, VER(31), VER(42)); // mixerPan
	s.syncAsByte(t.sndDataExtComp, VER(45));
}

void IMuseDigital::saveLoadEarly(Common::Serializer &s) {
	Common::StackLock lock(_mutex, "IMuseDigital::saveLoadEarly()");

	s.skip(4, VER(31), VER(42)); // _volVoice
	s.skip(4, VER(31), VER(42)); // _volSfx
	s.skip(4, VER(31), VER(42)); // _volMusic
	s.syncAsSint32LE(_curMusicState, VER(31));
	s.syncAsSint32LE(_curMusicSeq, VER(31));
	s.syncAsSint32LE(_curMusicCue, VER(31));
	s.syncAsSint32LE(_nextSeqToPlay, VER(31));
	s.syncAsByte(_radioChatterSFX, VER(76));
	s.syncArray(_attributes, 188, Common::Serializer::Sint32LE, VER(31));

	for (int l = 0; l < MAX_DIGITAL_TRACKS + MAX_DIGITAL_FADETRACKS; l++) {
		Track *track = _track[l];
		if (s.isLoading()) {
			track->reset();
		}
		syncWithSerializer(s, *track);
		if (s.isLoading()) {
			_track[l]->trackId = l;
			if (!track->used)
				continue;
			if ((track->toBeRemoved) || (track->souStreamUsed) || (track->curRegion == -1)) {
				track->used = false;
				continue;
			}

			// TODO: The code below has a lot in common with that in IMuseDigital::startSound.
			// Try to refactor them to reduce the code duplication.

			track->soundDesc = _sound->openSound(track->soundId, track->soundName, track->soundType, track->volGroupId, -1);
			if (!track->soundDesc)
				track->soundDesc = _sound->openSound(track->soundId, track->soundName, track->soundType, track->volGroupId, 1);
			if (!track->soundDesc)
				track->soundDesc = _sound->openSound(track->soundId, track->soundName, track->soundType, track->volGroupId, 2);

			if (!track->soundDesc) {
				warning("IMuseDigital::saveOrLoad: Can't open sound so will not be resumed");
				track->used = false;
				continue;
			}

			if (_vm->_game.id == GID_CMI) {
				track->gainRedFadeDest = 127 * 290;
			}

			if (_vm->_game.id == GID_FT) {
				track->gainRedFadeDest = 127 * 180;
			}

			track->sndDataExtComp = _sound->isSndDataExtComp(track->soundDesc);
			track->dataOffset = _sound->getRegionOffset(track->soundDesc, track->curRegion);
			int bits = _sound->getBits(track->soundDesc);
			int channels = _sound->getChannels(track->soundDesc);
			int freq = _sound->getFreq(track->soundDesc);
			track->feedSize = freq * channels;
			track->mixerFlags = 0;
			track->littleEndian = track->soundDesc->littleEndian;
			if (channels == 2)
				track->mixerFlags = kFlagStereo;

			if ((bits == 12) || (bits == 16)) {
				track->mixerFlags |= kFlag16Bits;
				track->feedSize *= 2;
			} else if (bits == 8) {
				track->mixerFlags |= kFlagUnsigned;
			} else
				error("IMuseDigital::saveOrLoad(): Can't handle %d bit samples", bits);

			track->stream = Audio::makeQueuingAudioStream(freq, (track->mixerFlags & kFlagStereo) != 0);

			_mixer->playStream(track->getType(), &track->mixChanHandle, track->stream, -1, track->getVol(), track->getPan());
			_mixer->pauseHandle(track->mixChanHandle, true);
		}
	}
}

void IMuseDigital::callback() {
	Common::StackLock lock(_mutex, "IMuseDigital::callback()");
	_speechIsPlaying = false;
	// Check for any track playing a speech line
	if (_vm->_game.id != GID_DIG) {
		for (int l = 0; l < MAX_DIGITAL_TRACKS; l++) {
			if (_track[l]->used && _track[l]->soundId == kTalkSoundID) {
				// Set flag and break
				_speechIsPlaying = true;
				break;
			}
		}
	}

	for (int l = 0; l < MAX_DIGITAL_TRACKS + MAX_DIGITAL_FADETRACKS; l++) {
		Track *track = _track[l];
		if (track->used) {
			// Ignore tracks which are about to finish. Also, if it did finish in the meantime,
			// mark it as unused.
			if (!track->stream) {
				if (!_mixer->isSoundHandleActive(track->mixChanHandle))
					track->reset();
				continue;
			}

			if (_pause)
				return;

			// Fades and crossfades handling
			if (track->volFadeUsed) {
				if (track->vol == track->volFadeDest) // Sanity check (needed for some edge cases in COMI, FT and DIG)
					track->volFadeUsed = false;
				if (track->volFadeStep < 0) { // Fade out
					if (track->vol > track->volFadeDest) {
						// COMI uses non-linear fade curves
						if (_vm->_game.id == GID_CMI) {
							int tempVolume = transformVolumeEqualPowToLinear(track->vol, 1); // Equal power to linear...
							tempVolume += track->volFadeStep; // Remove step...
							track->vol = transformVolumeLinearToEqualPow(tempVolume, 1); // Linear to equal power...
						} else {
							track->vol += track->volFadeStep; // Remove step...
						}
						if (track->vol <= track->volFadeDest) {
							track->vol = track->volFadeDest;
							track->volFadeUsed = false;
							// In COMI we flush the track if we've faded out to the destination volume;
							// this is because there are no situations in which there is a fade out to a
							// non-zero volume, so this will always mean that we can free the track.
							// This is not true in Full Throttle, for example, so we have to make this distinction.
							if (_vm->_game.id == GID_CMI) {
								flushTrack(track);
								continue;
							}
						}
						if (track->vol == 0) {
							// Fade out complete -> remove this track
							flushTrack(track);
							continue;
						}
					}
				} else if (track->volFadeStep > 0) { // Fade in
					if (track->vol < track->volFadeDest) {
						// Again, COMI uses non-linear fade curves
						// Curiously, FT uses a different curve for fade-ins
						if (_vm->_game.id == GID_CMI) {
							int tempVolume = transformVolumeEqualPowToLinear(track->vol, 1); // Equal power to linear...
							tempVolume += track->volFadeStep; // Add step...
							track->vol = transformVolumeLinearToEqualPow(tempVolume, 1); // Linear to equal power...
						} else if (_vm->_game.id == GID_FT) {
							int tempVolume = transformVolumeEqualPowToLinear(track->vol, 6); // Equal power to linear...
							tempVolume += (track->volFadeStep); // Add step...
							track->vol = transformVolumeLinearToEqualPow(tempVolume, 6); // Linear to equal power...
						} else {
							track->vol += track->volFadeStep; // Add step...
						}
						if (track->vol >= track->volFadeDest) {
							track->vol = track->volFadeDest;
							track->volFadeUsed = false;
						}
					}
				}
 				debug(5, "Fade: sound(%d), Vol(%d) in track(%d)", track->soundId, track->vol / 1000, track->trackId);
			}

			// Music gain reduction during speech (used, at the moment, on FT and COMI)
			if (_vm->_game.id != GID_DIG && track->volGroupId == IMUSE_VOLGRP_MUSIC) {
				if (_speechIsPlaying) {
					// Check if we have to fade down or the reduction volume is already at the right value
					if (track->gainReduction >= track->gainRedFadeDest) {
						track->gainRedFadeUsed = false;
						track->gainReduction = track->gainRedFadeDest; // Clip to destination volume
					} else {
						track->gainRedFadeUsed = true;
					}
					// Gradually bring up the gain reduction (20 ms)
					if (track->gainRedFadeUsed && track->gainReduction < track->gainRedFadeDest) {
						int tempReduction = transformVolumeEqualPowToLinear(track->gainReduction, 2); // Equal power to linear...
						tempReduction += (track->gainRedFadeDest - track->gainReduction) * 60 * (1000 / _callbackFps) / (1000 * 20); // Add step...
						track->gainReduction = transformVolumeLinearToEqualPow(tempReduction, 2); // Linear to equal power...
						debug(5, "Gain reduction: sound(%d), reduction amount(%d) in track(%d)", track->soundId, track->gainReduction / 1000, track->trackId);
					}
				} else if (!_speechIsPlaying && track->gainReduction > 0) {
					// Just like the original interpreter, disable gain reduction immediately without a fade
					track->gainReduction = 0;
					debug(5, "Gain reduction: no speech playing reduction stopped for sound(%d) in track(%d)", track->soundId, track->trackId);
				}
			}

			if (!track->souStreamUsed) {
				assert(track->stream);
				byte *tmpSndBufferPtr = NULL;
				int32 curFeedSize = 0;

				if (track->curRegion == -1) {
					switchToNextRegion(track);
					if (!track->stream)	// Seems we reached the end of the stream
						continue;
				}

				int bits = _sound->getBits(track->soundDesc);
				int channels = _sound->getChannels(track->soundDesc);

				int32 feedSize = track->feedSize / _callbackFps;

				if (track->stream->endOfData()) {
					feedSize *= 2;
				}

				if ((bits == 12) || (bits == 16)) {
					if (channels == 1)
						feedSize &= ~1;
					if (channels == 2)
						feedSize &= ~3;
				} else if (bits == 8) {
					if (channels == 2)
						feedSize &= ~1;
				} else {
					warning("IMuseDigita::callback: Unexpected sample width, %d bits", bits);
					continue;
				}

				if (feedSize == 0)
					continue;

				do {
					if (bits == 12) {
						byte *tmpPtr = NULL;

						feedSize += track->dataMod12Bit;
						int tmpFeedSize12Bits = (feedSize * 3) / 4;
						int tmpLength12Bits = (tmpFeedSize12Bits / 3) * 4;
						track->dataMod12Bit = feedSize - tmpLength12Bits;

						int32 tmpOffset = (track->regionOffset * 3) / 4;
						int tmpFeedSize = _sound->getDataFromRegion(track->soundDesc, track->curRegion, &tmpPtr, tmpOffset, tmpFeedSize12Bits);
						curFeedSize = BundleCodecs::decode12BitsSample(tmpPtr, &tmpSndBufferPtr, tmpFeedSize);

						free(tmpPtr);
					} else if (bits == 16) {
						curFeedSize = _sound->getDataFromRegion(track->soundDesc, track->curRegion, &tmpSndBufferPtr, track->regionOffset, feedSize);
						if (channels == 1) {
							curFeedSize &= ~1;
						}
						if (channels == 2) {
							curFeedSize &= ~3;
						}
					} else if (bits == 8) {
						curFeedSize = _sound->getDataFromRegion(track->soundDesc, track->curRegion, &tmpSndBufferPtr, track->regionOffset, feedSize);
						if (_radioChatterSFX && track->soundId == 10000) {
							if (curFeedSize > feedSize)
								curFeedSize = feedSize;
							byte *buf = (byte *)malloc(curFeedSize);
							int index = 0;
							int count = curFeedSize - 4;
							byte *ptr_1 = tmpSndBufferPtr;
							byte *ptr_2 = tmpSndBufferPtr + 4;
							int value = ptr_1[0] - 0x80;
							value += ptr_1[1] - 0x80;
							value += ptr_1[2] - 0x80;
							value += ptr_1[3] - 0x80;
							do {
								int t = *ptr_1++;
								int v = t - (value / 4);
								value = *ptr_2++ - 0x80 + (value - t + 0x80);
								buf[index++] = v * 2 + 0x80;
							} while (--count);
							buf[curFeedSize - 1] = 0x80;
							buf[curFeedSize - 2] = 0x80;
							buf[curFeedSize - 3] = 0x80;
							buf[curFeedSize - 4] = 0x80;
							free(tmpSndBufferPtr);
							tmpSndBufferPtr = buf;
						}
						if (channels == 2) {
							curFeedSize &= ~1;
						}
					}

					if (curFeedSize > feedSize)
						curFeedSize = feedSize;

					if (_mixer->isReady()) {
						track->stream->queueBuffer(tmpSndBufferPtr, curFeedSize, DisposeAfterUse::YES, makeMixerFlags(track));
						track->regionOffset += curFeedSize;
					} else
						free(tmpSndBufferPtr);

					if (_sound->isEndOfRegion(track->soundDesc, track->curRegion)) {
						switchToNextRegion(track);
						if (!track->stream)	// Seems we reached the end of the stream
							break;
					}
					feedSize -= curFeedSize;
					assert(feedSize >= 0);
				} while (feedSize != 0);
			}
			if (_mixer->isReady()) {
				int effVol = track->getVol();
				int effPan = track->getPan();
				if (_vm->_game.id == GID_CMI) {
					// Adjust audio mix for The Curse of Monkey Island
					if (track->volGroupId == IMUSE_VOLGRP_MUSIC) {
						effVol -= track->gainReduction / 1000;
						if (effVol < 0) // In case a music crossfading happens during gain reduction...
							effVol = 0;
						effVol = int(round(effVol * 1.9)); // Adjust default music mix for COMI
					} else if (track->volGroupId == IMUSE_VOLGRP_VOICE) {
						// Just in case the speakingActor is not being set...
						// This allows for a fallback to pan = 64 (center) and volume = 127 (full)
						if (track->speakingActor != nullptr) {
							effVol = track->speakingActor->_talkVolume;
							// Even though we fixed this in IMuseDigital::setVolume(),
							// some sounds might be started without even calling that function
							if (effVol > 127)
								effVol /= 2;
							effVol = int(round(effVol * 1.04));
							effPan = (track->speakingActor->_talkPan != 64) ? 2 * track->speakingActor->_talkPan - 127 : 0;
						}
					}
				} else if (_vm->_game.id == GID_FT) {
					// Adjust audio mix for Full Throttle
					// (this affects music and sfx only, speech volume mix
					// is changed accordingly in IMuseDigital::startSound()
					// since that's the only handle we have for speech)
					if (track->volGroupId == IMUSE_VOLGRP_MUSIC) {
						// Gain reduction
						effVol -= track->gainReduction / 1000;
						if (effVol < 0) // In case a music crossfading happens during gain reduction...
							effVol = 0;
						effVol = int(round(effVol * 1.5));
					} else {
						effVol = int(round(effVol * 1.1));
					}
				}
				_mixer->setChannelVolume(track->mixChanHandle, effVol);
				_mixer->setChannelBalance(track->mixChanHandle, effPan);
			}
		}
	}
}

void IMuseDigital::switchToNextRegion(Track *track) {
	assert(track);

	if (track->trackId >= MAX_DIGITAL_TRACKS) {
		flushTrack(track);
		debug(5, "SwToNeReg(trackId:%d) - fadetrack can't go next region, exiting SwToNeReg", track->trackId);
		return;
	}

	int num_regions = _sound->getNumRegions(track->soundDesc);

	if (++track->curRegion == num_regions) {
		flushTrack(track);
		debug(5, "SwToNeReg(trackId:%d) - end of region, exiting SwToNeReg", track->trackId);
		return;
	}

	ImuseDigiSndMgr::SoundDesc *soundDesc = track->soundDesc;
	if (_triggerUsed && track->soundDesc->numMarkers) {
		if (_sound->checkForTriggerByRegionAndMarker(soundDesc, track->curRegion, _triggerParams.marker)) {
			if (_vm->_game.id != GID_CMI) {
				debug(5, "SwToNeReg(trackId:%d) - trigger %s reached", track->trackId, _triggerParams.marker);
				debug(5, "SwToNeReg(trackId:%d) - exit current region %d", track->trackId, track->curRegion);
				debug(5, "SwToNeReg(trackId:%d) - call cloneToFadeOutTrack(delay:%d)", track->trackId, _triggerParams.fadeOutDelay);
				Track *fadeTrack = cloneToFadeOutTrack(track, _triggerParams.fadeOutDelay);
				if (fadeTrack) {
					fadeTrack->dataOffset = _sound->getRegionOffset(fadeTrack->soundDesc, fadeTrack->curRegion);
					fadeTrack->regionOffset = 0;
					debug(5, "SwToNeReg(trackId:%d)-sound(%d) select region %d, curHookId: %d", fadeTrack->trackId, fadeTrack->soundId, fadeTrack->curRegion, fadeTrack->curHookId);
					fadeTrack->curHookId = 0;
				}
				flushTrack(track);
				startMusic(_triggerParams.filename, _triggerParams.soundId, _triggerParams.hookId, _triggerParams.volume);
				_triggerUsed = false;
				return;
			} else {
				// Behavior for "_end" (and "exit") marker
				debug(5, "SwToNeReg(trackId:%d) - trigger %s reached", track->trackId, _triggerParams.marker);
				debug(5, "SwToNeReg(trackId:%d) - exit current region %d", track->trackId, track->curRegion);
				debug(5, "SwToNeReg(trackId:%d) - call handleComiFadeOut(delay:%d)", track->trackId, _triggerParams.fadeOutDelay);
				handleFadeOut(track, _triggerParams.fadeOutDelay);
				track->dataOffset = _sound->getRegionOffset(track->soundDesc, track->curRegion);
				track->regionOffset = 0;
				debug(5, "SwToNeReg(trackId:%d)-sound(%d) select region %d, curHookId: %d", track->trackId, track->soundId, track->curRegion, track->curHookId);
				track->curHookId = 0;
				if (!scumm_stricmp(_triggerParams.marker, "exit"))
					startMusic(_triggerParams.filename, _triggerParams.soundId, _triggerParams.hookId, _triggerParams.volume);
				_triggerUsed = false;
				return;
			}
		}
	}

	int jumpId = _sound->getJumpIdByRegionAndHookId(soundDesc, track->curRegion, track->curHookId);
	if ((_vm->_game.id != GID_CMI && jumpId != -1) || (_vm->_game.id == GID_CMI && jumpId != -1 && !track->toBeRemoved && !track->alreadyCrossfading)) {
		int region = _sound->getRegionIdByJumpId(soundDesc, jumpId);
		assert(region != -1);
		int sampleHookId = _sound->getJumpHookId(soundDesc, jumpId);
		assert(sampleHookId != -1);

		bool isJumpToStart = false;
		if (_vm->_game.id == GID_CMI) {
			isJumpToStart = (soundDesc->jump[jumpId].dest == soundDesc->marker[2].pos && !scumm_stricmp(soundDesc->marker[2].ptr, "start"));
		}

		debug(5, "SwToNeReg(trackId:%d) - JUMP found - sound:%d, track hookId:%d, data hookId:%d", track->trackId, track->soundId, track->curHookId, sampleHookId);
		if (track->curHookId == sampleHookId) {
			int fadeDelay = (60 * _sound->getJumpFade(soundDesc, jumpId)) / 1000;
			debug(5, "SwToNeReg(trackId:%d) - sound(%d) match hookId", track->trackId, track->soundId);

			if (_vm->_game.id == GID_CMI) {
				// The original exe crossfades the jump to the destination region.
				// For the time being this is implemented as a simple region switch.
				track->curRegion = region;
			} else {
				if (fadeDelay) {
					debug(5, "SwToNeReg(trackId:%d) - call cloneToFadeOutTrack(delay:%d)", track->trackId, fadeDelay);
					Track *fadeTrack = cloneToFadeOutTrack(track, fadeDelay);
					if (fadeTrack) {
						fadeTrack->dataOffset = _sound->getRegionOffset(fadeTrack->soundDesc, fadeTrack->curRegion);
						fadeTrack->regionOffset = 0;
						debug(5, "SwToNeReg(trackId:%d) - sound(%d) faded track, select region %d, curHookId: %d", fadeTrack->trackId, fadeTrack->soundId, fadeTrack->curRegion, fadeTrack->curHookId);
						fadeTrack->curHookId = 0;
					}
				} else {
					track->curRegion = region;
				}
			}

			debug(5, "SwToNeReg(trackId:%d) - sound(%d) jump to region %d, curHookId: %d", track->trackId, track->soundId, track->curRegion, track->curHookId);
			track->curHookId = 0;
		} else {
			// Check if the jump led to a  "start" marker; if so, we have to enforce it anyway.
			// Fixes bug/edge-case #11956;
			// Go see ImuseDigiSndMgr::getJumpIdByRegionAndHookId(...) for further information.
			if (_vm->_game.id == GID_CMI && isJumpToStart) {
				track->curRegion = region;
				debug(5, "SwToNeReg(trackId:%d) - Enforced sound(%d) jump to region %d marked with a \"start\" marker, hookId(%d)", track->trackId, track->soundId, track->curRegion, track->curHookId);
			} else {
				debug(5, "SwToNeReg(trackId:%d) - Normal switch region, sound(%d), hookId(%d)", track->trackId, track->soundId, track->curHookId);
			}
		}
	} else {
		debug(5, "SwToNeReg(trackId:%d) - Normal switch region, sound(%d), hookId(%d)", track->trackId, track->soundId, track->curHookId);
	}
	debug(5, "SwToNeReg(trackId:%d) - sound(%d), select region %d", track->trackId, track->soundId, track->curRegion);
	track->dataOffset = _sound->getRegionOffset(soundDesc, track->curRegion);
	track->regionOffset = 0;
	debug(5, "SwToNeReg(trackId:%d) - end of func", track->trackId);
}

} // End of namespace Scumm
