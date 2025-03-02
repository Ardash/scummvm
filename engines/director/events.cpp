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

#include "graphics/macgui/macwindowmanager.h"

#include "director/director.h"
#include "director/movie.h"
#include "director/score.h"
#include "director/cursor.h"
#include "director/channel.h"
#include "director/sprite.h"
#include "director/window.h"
#include "director/castmember.h"
#include "director/lingo/lingo.h"

namespace Director {

uint32 DirectorEngine::getMacTicks() { return g_system->getMillis() * 60 / 1000.; }

bool DirectorEngine::processEvents(bool captureClick) {
	debugC(3, kDebugEvents, "\n@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@");
	debugC(3, kDebugEvents, "@@@@   Processing events");
	debugC(3, kDebugEvents, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@\n");

	Common::Event event;
	while (g_system->getEventManager()->pollEvent(event)) {
		_wm->processEvent(event);

		switch (event.type) {
		case Common::EVENT_QUIT:
			_stage->getCurrentMovie()->getScore()->_playState = kPlayStopped;
			break;
		case Common::EVENT_LBUTTONDOWN:
			if (captureClick)
				return true;
			break;
		default:
			break;
		}
	}

	return false;
}

bool Window::processEvent(Common::Event &event) {
	bool flag = MacWindow::processEvent(event);

	if (_currentMovie && _currentMovie->processEvent(event))
		flag = true;

	return flag;
}

bool Movie::processEvent(Common::Event &event) {
	Score *sc = getScore();
	if (sc->getCurrentFrame() >= sc->_frames.size()) {
		warning("processEvents: request to access frame %d of %d", sc->getCurrentFrame(), sc->_frames.size() - 1);
		return false;
	}
	uint16 spriteId = 0;

	Common::Point pos;

	switch (event.type) {
	case Common::EVENT_MOUSEMOVE:
		pos = _window->getMousePos();

		_lastEventTime = g_director->getMacTicks();
		_lastRollTime =	 _lastEventTime;

		sc->renderCursor(pos);

		// hiliteChannelId is specified for BitMap castmember, so we deal with them separately with other castmember
		// if we are moving out of bounds, then we don't hilite it anymore
		if (_currentHiliteChannelId && !sc->_channels[_currentHiliteChannelId]->isMouseIn(pos)) {
			g_director->getCurrentWindow()->setDirty(true);
			g_director->getCurrentWindow()->addDirtyRect(sc->_channels[_currentHiliteChannelId]->getBbox());
			_currentHiliteChannelId = 0;
			_currentHandlingChannelId = 0;
		}

		if (_currentHandlingChannelId && !sc->_channels[_currentHandlingChannelId]->getBbox().contains(pos))
			_currentHandlingChannelId = 0;

		// for the list style button, we still have chance to trigger events though button.
		if (!(g_director->_wm->_mode & Graphics::kWMModeButtonDialogStyle) && g_director->_wm->_mouseDown && g_director->_wm->_hilitingWidget) {
			if (g_director->getVersion() < 400)
				spriteId = sc->getActiveSpriteIDFromPos(pos);
			else
				spriteId = sc->getMouseSpriteIDFromPos(pos);

			_currentHandlingChannelId = spriteId;
			if (spriteId > 0 && sc->_channels[spriteId]->_sprite->shouldHilite()) {
				_currentHiliteChannelId = spriteId;
				g_director->getCurrentWindow()->setDirty(true);
				g_director->getCurrentWindow()->addDirtyRect(sc->_channels[_currentHiliteChannelId]->getBbox());
			}
		}

		if (_currentDraggedChannel) {
			if (_currentDraggedChannel->_sprite->_moveable) {
				pos = _window->getMousePos();

				_currentDraggedChannel->addDelta(pos - _draggingSpritePos);
				_draggingSpritePos = pos;
			} else {
				_currentDraggedChannel = nullptr;
			}
		}
		return true;

	case Common::EVENT_LBUTTONDOWN:
		if (sc->_waitForClick) {
			sc->_waitForClick = false;
			_vm->setCursor(kCursorDefault);
		} else {
			pos = _window->getMousePos();

			// D3 doesn't have both mouse up and down.
			// But we still want to know if the mouse is down for press effects.
			// Since we don't have mouse up and down before D3, then we use ActiveSprite
			if (g_director->getVersion() < 400)
				spriteId = sc->getActiveSpriteIDFromPos(pos);
			else
				spriteId = sc->getMouseSpriteIDFromPos(pos);

			// is this variable unused here?
			_currentClickOnSpriteId = sc->getActiveSpriteIDFromPos(pos);

			_currentHandlingChannelId = spriteId;
			if (spriteId > 0 && sc->_channels[spriteId]->_sprite->shouldHilite()) {
				_currentHiliteChannelId = spriteId;
				g_director->_wm->_hilitingWidget = true;
				g_director->getCurrentWindow()->setDirty(true);
				g_director->getCurrentWindow()->addDirtyRect(sc->_channels[_currentHiliteChannelId]->getBbox());
			}

			_lastEventTime = g_director->getMacTicks();
			_lastClickTime = _lastEventTime;
			_lastClickPos = pos;

			debugC(3, kDebugEvents, "event: Button Down @(%d, %d), movie '%s', sprite id: %d", pos.x, pos.y, _macName.c_str(), spriteId);
			registerEvent(kEventMouseDown, spriteId);

			if (sc->_channels[spriteId]->_sprite->_moveable) {
				_draggingSpritePos = _window->getMousePos();
				_currentDraggedChannel = sc->_channels[spriteId];
			}
		}

		return true;

	case Common::EVENT_LBUTTONUP:
		pos = _window->getMousePos();

		_currentClickOnSpriteId = sc->getActiveSpriteIDFromPos(pos);

		if (_currentHiliteChannelId && sc->_channels[_currentHiliteChannelId]) {
			g_director->getCurrentWindow()->setDirty(true);
			g_director->getCurrentWindow()->addDirtyRect(sc->_channels[_currentHiliteChannelId]->getBbox());
		}

		g_director->_wm->_hilitingWidget = false;

		debugC(3, kDebugEvents, "event: Button Up @(%d, %d), movie '%s', sprite id: %d", pos.x, pos.y, _macName.c_str(), _currentHandlingChannelId);

		_currentDraggedChannel = nullptr;

		if (_currentHandlingChannelId) {
			CastMember *cast = getCastMember(sc->_channels[_currentHandlingChannelId]->_sprite->_castId);
			if (cast && cast->_type == kCastButton)
				cast->_hilite = !cast->_hilite;
		}

		registerEvent(kEventMouseUp, _currentHandlingChannelId);
		sc->renderCursor(pos);

		_currentHiliteChannelId = 0;
		_currentHandlingChannelId = 0;
		return true;

	case Common::EVENT_KEYDOWN:
		_keyCode = _vm->_macKeyCodes.contains(event.kbd.keycode) ? _vm->_macKeyCodes[event.kbd.keycode] : 0;
		_key = (unsigned char)(event.kbd.ascii & 0xff);
		_keyFlags = event.kbd.flags;

		debugC(1, kDebugEvents, "processEvents(): movie '%s', keycode: %d", _macName.c_str(), _keyCode);

		_lastEventTime = g_director->getMacTicks();
		_lastKeyTime = _lastEventTime;
		registerEvent(kEventKeyDown);
		return true;

	case Common::EVENT_KEYUP:
		_keyFlags = event.kbd.flags;
		return true;

	default:
		break;
	}

	return false;
}

} // End of namespace Director
