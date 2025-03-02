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

#include "common/endian.h"

#include "graphics/macgui/mactext.h"

#include "director/director.h"
#include "director/cast.h"
#include "director/channel.h"
#include "director/castmember.h"
#include "director/window.h"
#include "director/util.h"
#include "director/lingo/lingo.h"
#include "director/lingo/lingo-code.h"
#include "director/lingo/lingo-object.h"
#include "director/lingo/lingo-the.h"

#include "director/lingo/xlibs/cdromxobj.h"
#include "director/lingo/xlibs/fileio.h"
#include "director/lingo/xlibs/flushxobj.h"
#include "director/lingo/xlibs/fplayxobj.h"
#include "director/lingo/xlibs/labeldrvxobj.h"
#include "director/lingo/xlibs/orthoplayxobj.h"
#include "director/lingo/xlibs/palxobj.h"
#include "director/lingo/xlibs/popupmenuxobj.h"
#include "director/lingo/xlibs/serialportxobj.h"
#include "director/lingo/xlibs/soundjam.h"
#include "director/lingo/xlibs/videodiscxobj.h"
#include "director/lingo/xlibs/winxobj.h"

namespace Director {

static struct PredefinedProto {
	const char *name;
	void (*func)(int);
	int minArgs;	// -1 -- arglist
	int maxArgs;
	int type;
	int version;
} predefinedMethods[] = {
	// all except window
	{ "new",					LM::m_new,					-1, 0,	kAllObj, 				200 },	// D2

	// factory and XObject
	{ "describe",				LM::m_describe,				 0, 0,	kXObj,					200 },	// D2
	{ "dispose",				LM::m_dispose,				 0, 0,	kFactoryObj | kXObj,	200 },	// D2
	{ "get",					LM::m_get,					 1, 1,	kFactoryObj,			200 },	// D2
	{ "instanceRespondsTo",		LM::m_instanceRespondsTo,	 1, 1,	kXObj,					300 },		// D3
	{ "messageList",			LM::m_messageList,			 0, 0,	kXObj,					300 },		// D3
	{ "name",					LM::m_name,					 0, 0,	kXObj,					300 },		// D3
	{ "perform",				LM::m_perform,				-1, 0,	kFactoryObj | kXObj, 	300 },		// D3
	{ "put",					LM::m_put,					 2, 2,	kFactoryObj,			200 },	// D2
	{ "respondsTo",				LM::m_respondsTo,			 1, 1,	kXObj,					200 },	// D2

	// script object and Xtra
	{ "birth",					LM::m_new,					-1, 0,	kScriptObj | kXtraObj, 	400 },			// D4

	{ 0, 0, 0, 0, 0, 0 }
};

static MethodProto windowMethods[] = {
	// window / stage
	{ "close",					LM::m_close,				 0, 0,	400 },			// D4
	{ "forget",					LM::m_forget,				 0, 0,	400 },			// D4
	{ "open",					LM::m_open,					 0, 0,	400 },			// D4
	{ "moveToBack",				LM::m_moveToBack,			 0, 0,	400 },			// D4
	{ "moveToFront",			LM::m_moveToFront,			 0, 0,	400 },			// D4
	{ 0, 0, 0, 0, 0 }
};

void Lingo::initMethods() {
	for (PredefinedProto *mtd = predefinedMethods; mtd->name; mtd++) {
		if (mtd->version > _vm->getVersion())
			continue;

		Symbol sym;
		sym.name = new Common::String(mtd->name);
		sym.type = HBLTIN;
		sym.nargs = mtd->minArgs;
		sym.maxArgs = mtd->maxArgs;
		sym.targetType = mtd->type;
		sym.u.bltin = mtd->func;
		_methods[mtd->name] = sym;
	}
	Window::initMethods(windowMethods);
}

void Lingo::cleanupMethods() {
	_methods.clear();
	Window::cleanupMethods();
}

static struct XLibProto {
	const char *name;
	void (*opener)(int);
	void (*closer)(int);
	int type;
	int version;
} xlibs[] = {
	{ "CD-ROM XObj",			CDROMXObj::open,		CDROMXObj::close,			kXObj | kXtraObj,		200 },	// D2
	{ "FileIO",					FileIO::open,			FileIO::close,				kXObj | kXtraObj,		200 },	// D2
	{ "FlushXObj",				FlushXObj::open,		FlushXObj::close,			kXObj,					400 },	// D4
	{ "FPlayXObj",				FPlayXObj::open,		FPlayXObj::close,			kXObj,					200 },	// D2
	{ "OrthoPlay XObj",			OrthoPlayXObj::open,	OrthoPlayXObj::close,		kXObj,					400 }, 	// D4
	{ "PalXObj",				PalXObj::open,			PalXObj::close,				kXObj,					400 }, 	// D4
	{ "PopUp Menu XObj",		PopUpMenuXObj::open,	PopUpMenuXObj::close,		kXObj | kXtraObj,		200 }, 	// D2
	{ "LabelDrv",				LabelDrvXObj::open,		LabelDrvXObj::close,		kXObj,					400 }, 	// D4
	{ "SerialPort",				SerialPortXObj::open,	SerialPortXObj::close,		kXObj | kXtraObj,		200 },	// D2
	{ "SoundJam",				SoundJam::open,			SoundJam::close,			kXObj,					400 },	// D4
	{ "Videodisc XObj",			VideodiscXObj::open,	VideodiscXObj::close,		kXObj | kXtraObj,		200 }, 	// D2
	{ "winXObj",				RearWindowXObj::open,	RearWindowXObj::close,		kXObj,					400 },	// D4
	{ 0, 0, 0, 0, 0 }

};

void Lingo::initXLibs() {
	for (XLibProto *lib = xlibs; lib->name; lib++) {
		if (lib->version > _vm->getVersion())
			continue;

		Symbol openSym;
		openSym.name = new Common::String(lib->name);
		openSym.type = HBLTIN;
		openSym.nargs = 0;
		openSym.maxArgs = 0;
		openSym.targetType = lib->type;
		openSym.u.bltin = lib->opener;
		_xlibOpeners[lib->name] = openSym;

		Symbol closeSym;
		closeSym.name = new Common::String(lib->name);
		closeSym.type = HBLTIN;
		closeSym.nargs = 0;
		closeSym.maxArgs = 0;
		openSym.targetType = lib->type;
		closeSym.u.bltin = lib->closer;
		_xlibClosers[lib->name] = closeSym;
	}
}

void Lingo::cleanupXLibs() {
	_xlibOpeners.clear();
	_xlibClosers.clear();
}

Common::String Lingo::normalizeXLibName(Common::String name) {
	Common::Platform platform = _vm->getPlatform();
	if (platform == Common::kPlatformMacintosh) {
		int pos = name.findLastOf(':');
		name = name.substr(pos + 1, name.size());
	} else if (platform == Common::kPlatformWindows) {
		if (name.hasSuffixIgnoreCase(".dll"))
			name = name.substr(0, name.size() - 4);
	}

	name.trim();

	return name;
}

void Lingo::openXLib(Common::String name, ObjectType type) {
	name = normalizeXLibName(name);

	if (_openXLibs.contains(name))
		return;

	_openXLibs[name] = type;

	if (_xlibOpeners.contains(name)) {
		Symbol sym = _xlibOpeners[name];
		(*sym.u.bltin)(type);
	} else {
		warning("Lingo::openXLib: Unimplemented xlib: '%s'", name.c_str());
	}
}

void Lingo::closeXLib(Common::String name) {
	name = normalizeXLibName(name);

	if (!_openXLibs.contains(name)) {
		warning("Lingo::closeXLib: xlib %s is not open", name.c_str());
		return;
	}

	ObjectType type = _openXLibs[name];
	_openXLibs.erase(name);

	if (_xlibClosers.contains(name)) {
		Symbol sym = _xlibClosers[name];
		(*sym.u.bltin)(type);
	} else {
		warning("Lingo::closeXLib: Unimplemented xlib: '%s'", name.c_str());
	}
}

void Lingo::reloadOpenXLibs() {
	OpenXLibsHash openXLibsCopy = _openXLibs;
	for (OpenXLibsHash::iterator it = openXLibsCopy.begin(); it != openXLibsCopy.end(); ++it) {
		closeXLib(it->_key);
		openXLib(it->_key, it->_value);
	}
}

// Initialization/disposal

void LM::m_new(int nargs) {
	// This is usually overridden by a user-defined mNew
	g_lingo->printSTUBWithArglist("m_new", nargs);
	g_lingo->push(g_lingo->_currentMe);
}

void LM::m_dispose(int nargs) {
	g_lingo->_currentMe.u.obj->dispose();
}

/* ScriptContext */

ScriptContext::ScriptContext(Common::String name, ScriptType type, int id)
	: Object<ScriptContext>(name), _scriptType(type), _id(id) {
	_objType = kScriptObj;
}

ScriptContext::ScriptContext(const ScriptContext &sc) : Object<ScriptContext>(sc) {
	_scriptType = sc._scriptType;
	_functionNames = sc._functionNames;
	for (SymbolHash::iterator it = sc._functionHandlers.begin(); it != sc._functionHandlers.end(); ++it) {
		_functionHandlers[it->_key] = it->_value;
		_functionHandlers[it->_key].ctx = this;
	}
	for (Common::HashMap<uint32, Symbol>::iterator it = sc._eventHandlers.begin(); it != sc._eventHandlers.end(); ++it) {
		_eventHandlers[it->_key] = it->_value;
		_eventHandlers[it->_key].ctx = this;
	}
	_constants = sc._constants;
	_properties = sc._properties;

	_id = sc._id;
}

ScriptContext::~ScriptContext() {}

Common::String ScriptContext::asString() {
	return Common::String::format("script: #%s %d %p", _name.c_str(), _inheritanceLevel, (void *)this);
}

Symbol ScriptContext::define(const Common::String &name, ScriptData *code, Common::Array<Common::String> *argNames, Common::Array<Common::String> *varNames) {
	Symbol sym;
	sym.name = new Common::String(name);
	sym.type = HANDLER;
	sym.u.defn = code;
	sym.nargs = argNames->size();
	sym.maxArgs = argNames->size();
	sym.argNames = argNames;
	sym.varNames = varNames;
	sym.ctx = this;

	if (debugChannelSet(1, kDebugCompile)) {
		uint pc = 0;
		while (pc < sym.u.defn->size()) {
			uint spc = pc;
			Common::String instr = g_lingo->decodeInstruction(sym.u.defn, pc, &pc);
			debugC(1, kDebugCompile, "[%5d] %s", spc, instr.c_str());
		}
		debugC(1, kDebugCompile, "<end define code>");
	}

	if (!g_lingo->_eventHandlerTypeIds.contains(name)) {
		_functionHandlers[name] = sym;
	} else {
		_eventHandlers[g_lingo->_eventHandlerTypeIds[name]] = sym;
	}

	return sym;
}

Symbol ScriptContext::getMethod(const Common::String &methodName) {
	Symbol sym;

	if (_functionHandlers.contains(methodName)) {
		sym = _functionHandlers[methodName];
		sym.target = this;
		return sym;
	}

	sym = Object<ScriptContext>::getMethod(methodName);
	if (sym.type != VOIDSYM)
		return sym;

	if (_objType == kScriptObj) {
		if (_properties.contains("ancestor") && _properties["ancestor"].type == OBJECT
				&& (_properties["ancestor"].u.obj->getObjType() & (kScriptObj | kXtraObj))) {
			// ancestor method
			debugC(3, kDebugLingoExec, "Calling method '%s' on ancestor: <%s>", methodName.c_str(), _properties["ancestor"].asString(true).c_str());
			return _properties["ancestor"].u.obj->getMethod(methodName);
		}
	}

	return sym;
}

bool ScriptContext::hasProp(const Common::String &propName) {
	if (_disposed) {
		error("Property '%s' accessed on disposed object <%s>", propName.c_str(), Datum(this).asString(true).c_str());
	}
	if (_properties.contains(propName)) {
		return true;
	}
	if (_objType == kScriptObj) {
		if (_properties.contains("ancestor") && _properties["ancestor"].type == OBJECT
				&& (_properties["ancestor"].u.obj->getObjType() & (kScriptObj | kXtraObj))) {
			return _properties["ancestor"].u.obj->hasProp(propName);
		}
	}
	return false;
}

Datum ScriptContext::getProp(const Common::String &propName) {
	if (_disposed) {
		error("Property '%s' accessed on disposed object <%s>", propName.c_str(), Datum(this).asString(true).c_str());
	}
	if (_properties.contains(propName)) {
		return _properties[propName];
	}
	if (_objType == kScriptObj) {
		if (_properties.contains("ancestor") && _properties["ancestor"].type == OBJECT
				&& (_properties["ancestor"].u.obj->getObjType() & (kScriptObj | kXtraObj))) {
			debugC(3, kDebugLingoExec, "Getting prop '%s' from ancestor: <%s>", propName.c_str(), _properties["ancestor"].asString(true).c_str());
			return _properties["ancestor"].u.obj->getProp(propName);
		}
	}
	return _properties[propName]; // return new property
}

bool ScriptContext::setProp(const Common::String &propName, const Datum &value) {
	if (_disposed) {
		error("Property '%s' accessed on disposed object <%s>", propName.c_str(), Datum(this).asString(true).c_str());
	}
	if (_properties.contains(propName)) {
		_properties[propName] = value;
		return true;
	}
	if (_objType == kScriptObj) {
		if (_properties.contains("ancestor") && _properties["ancestor"].type == OBJECT
				&& (_properties["ancestor"].u.obj->getObjType() & (kScriptObj | kXtraObj))) {
			debugC(3, kDebugLingoExec, "Getting prop '%s' from ancestor: <%s>", propName.c_str(), _properties["ancestor"].asString(true).c_str());
			return _properties["ancestor"].u.obj->setProp(propName, value);
		}
	}
	return false;
}

// Object array

void LM::m_get(int nargs) {
	ScriptContext *me = static_cast<ScriptContext *>(g_lingo->_currentMe.u.obj);
	Datum indexD = g_lingo->pop();
	uint index = MAX(0, indexD.asInt());
	if (me->_objArray.contains(index)) {
		g_lingo->push(me->_objArray[index]);
	} else {
		g_lingo->push(Datum(0));
	}
}

void LM::m_put(int nargs) {
	ScriptContext *me = static_cast<ScriptContext *>(g_lingo->_currentMe.u.obj);
	Datum value = g_lingo->pop();
	Datum indexD = g_lingo->pop();
	uint index = MAX(0, indexD.asInt());
	me->_objArray[index] = value;
}

// Other

void LM::m_perform(int nargs) {
	// Lingo doesn't seem to bother cloning the object when
	// mNew is called with mPerform
	AbstractObject *me = g_lingo->_currentMe.u.obj;
	Datum methodName = g_lingo->_stack.remove_at(g_lingo->_stack.size() - nargs); // Take method name out of stack
	nargs -= 1;
	Symbol funcSym = me->getMethod(*methodName.u.s);
	LC::call(funcSym, nargs, true);
}

// XObject

void LM::m_describe(int nargs) {
	warning("STUB: m_describe");
}

void LM::m_instanceRespondsTo(int nargs) {
	AbstractObject *me = g_lingo->_currentMe.u.obj;
	Datum d = g_lingo->pop();
	Common::String methodName = d.asString();

	if (me->getMethod(methodName).type != VOIDSYM) {
		g_lingo->push(Datum(1));
	} else {
		g_lingo->push(Datum(0));
	}
}

void LM::m_messageList(int nargs) {
	warning("STUB: m_messageList");
	g_lingo->push(Datum(""));
}

void LM::m_name(int nargs) {
	AbstractObject *me = g_lingo->_currentMe.u.obj;
	g_lingo->push(me->getName());
}

void LM::m_respondsTo(int nargs) {
	AbstractObject *me = g_lingo->_currentMe.u.obj;
	Datum d = g_lingo->pop();
	Common::String methodName = d.asString();

	// TODO: Check inheritance level
	if (me->getMethod(methodName).type != VOIDSYM) {
		g_lingo->push(Datum(1));
	} else {
		g_lingo->push(Datum(0));
	}
}

// Window

Common::String Window::asString() {
	return "window \"" + getName() + "\"";
}

bool Window::hasProp(const Common::String &propName) {
	Common::String fieldName = Common::String::format("%d%s", kTheWindow, propName.c_str());
	return g_lingo->_theEntityFields.contains(fieldName) && hasField(g_lingo->_theEntityFields[fieldName]->field);
}

Datum Window::getProp(const Common::String &propName) {
	Common::String fieldName = Common::String::format("%d%s", kTheWindow, propName.c_str());
	if (g_lingo->_theEntityFields.contains(fieldName)) {
		return getField(g_lingo->_theEntityFields[fieldName]->field);
	}

	warning("Window::getProp: unknown property '%s'", propName.c_str());
	return Datum();
}

bool Window::setProp(const Common::String &propName, const Datum &value) {
	Common::String fieldName = Common::String::format("%d%s", kTheWindow, propName.c_str());
	if (g_lingo->_theEntityFields.contains(fieldName)) {
		return setField(g_lingo->_theEntityFields[fieldName]->field, value);
	}

	warning("Window::setProp: unknown property '%s'", propName.c_str());
	return false;
}

bool Window::hasField(int field) {
	switch (field) {
	case kTheDrawRect:
	case kTheFileName:
	case kTheModal:
	case kTheSourceRect:
	case kTheTitle:
	case kTheTitleVisible:
	case kTheVisible:
	case kTheWindowType:
		return true;
	default:
		break;
	}
	return false;
}

Datum Window::getField(int field) {
	switch (field) {
	case kTheTitle:
		return getTitle();
	case kTheTitleVisible:
		return isTitleVisible();
	case kTheVisible:
		return isVisible();
	case kTheWindowType:
		return getWindowType();
	default:
		warning("Window::getField: unhandled field '%s'", g_lingo->field2str(field));
		return Datum();
	}
}

bool Window::setField(int field, const Datum &value) {
	switch (field) {
	case kTheTitle:
		setTitle(value.asString());
		return true;
	case kTheTitleVisible:
		setTitleVisible((bool)value.asInt());
		return true;
	case kTheVisible:
		setVisible((bool)value.asInt());
		return true;
	case kTheWindowType:
		setWindowType(value.asInt());
		return true;
	default:
		warning("Window::setField: unhandled field '%s'", g_lingo->field2str(field));
		return false;
	}
}

void LM::m_close(int nargs) {
	Window *me = static_cast<Window *>(g_lingo->_currentMe.u.obj);
	me->setVisible(false);
}

void LM::m_forget(int nargs) {
	Window *me = static_cast<Window *>(g_lingo->_currentMe.u.obj);
	FArray *windowList = g_lingo->_windowList.u.farr;

	uint i;
	for (i = 0; i < windowList->arr.size(); i++) {
		if (windowList->arr[i].type != OBJECT || windowList->arr[i].u.obj->getObjType() != kWindowObj)
			continue;

		Window *window = static_cast<Window *>(windowList->arr[i].u.obj);
		if (window == me)
			break;
	}

	if (i < windowList->arr.size())
		windowList->arr.remove_at(i);

	// remove me from global vars
	for (DatumHash::iterator it = g_lingo->_globalvars.begin(); it != g_lingo->_globalvars.end(); ++it) {
		if (it->_value.type != OBJECT || it->_value.u.obj->getObjType() != kWindowObj)
			continue;

		Window *window = static_cast<Window *>(windowList->arr[i].u.obj);
		if (window == me)
			g_lingo->_globalvars[it->_key] = 0;
	}
}

void LM::m_open(int nargs) {
	Window *me = static_cast<Window *>(g_lingo->_currentMe.u.obj);
	me->setVisible(true);
}

void LM::m_moveToBack(int nargs) {
	g_lingo->printSTUBWithArglist("m_moveToBack", nargs);
	g_lingo->dropStack(nargs);
}

void LM::m_moveToFront(int nargs) {
	g_lingo->printSTUBWithArglist("m_moveToFront", nargs);
	g_lingo->dropStack(nargs);
}

// CastMember

bool CastMember::hasProp(const Common::String &propName) {
	Common::String fieldName = Common::String::format("%d%s", kTheCast, propName.c_str());
	return g_lingo->_theEntityFields.contains(fieldName) && hasField(g_lingo->_theEntityFields[fieldName]->field);
}

Datum CastMember::getProp(const Common::String &propName) {
	Common::String fieldName = Common::String::format("%d%s", kTheCast, propName.c_str());
	if (g_lingo->_theEntityFields.contains(fieldName)) {
		return getField(g_lingo->_theEntityFields[fieldName]->field);
	}

	warning("CastMember::getProp: unknown property '%s'", propName.c_str());
	return Datum();
}

bool CastMember::setProp(const Common::String &propName, const Datum &value) {
	Common::String fieldName = Common::String::format("%d%s", kTheCast, propName.c_str());
	if (g_lingo->_theEntityFields.contains(fieldName)) {
		return setField(g_lingo->_theEntityFields[fieldName]->field, value);
	}

	warning("CastMember::setProp: unknown property '%s'", propName.c_str());
	return false;
}

bool CastMember::hasField(int field) {
	switch (field) {
	case kTheBackColor:
	case kTheCastType:
	case kTheFileName:
	case kTheForeColor:
	case kTheHeight:
	case kTheLoaded:
	case kTheModified:
	case kTheName:
	case kTheNumber:
	case kTheRect:
	case kThePurgePriority:
	case kTheScriptText:
	case kTheSize:
	case kTheWidth:
		return true;
	default:
		break;
	}
	return false;
}

Datum CastMember::getField(int field) {
	Datum d;

	CastMemberInfo *castInfo = _cast->getCastMemberInfo(_castId);
	if (!castInfo)
		warning("CastMember::getField(): CastMember info for %d not found", _castId);

	switch (field) {
	case kTheBackColor:
		d = (int)getBackColor();
		break;
	case kTheCastType:
		d = _type;
		break;
	case kTheFileName:
		if (castInfo)
			d = Datum(castInfo->fileName);
		break;
	case kTheForeColor:
		d = (int)getForeColor();
		break;
	case kTheHeight:
		d = _cast->getCastMemberInitialRect(_castId).height();
		break;
	case kTheLoaded:
		d = 1; // Not loaded handled in Lingo::getTheCast
		break;
	case kTheModified:
		warning("STUB: CastMember::getField():: Unprocessed getting field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
		break;
	case kTheName:
		if (castInfo)
			d = Datum(castInfo->name);
		break;
	case kTheNumber:
		d = _castId;
		break;
	case kTheRect:
		// not sure get the initial rect would be fine to castmember
		d = Datum(_cast->getCastMember(_castId)->_initialRect);
		break;
	case kThePurgePriority:
		d = _purgePriority;
		break;
	case kTheScriptText:
		if (castInfo)
			d = Datum(castInfo->script);
		break;
	case kTheSize:
		d = (int)_size;
		break;
	case kTheWidth:
		d = _cast->getCastMemberInitialRect(_castId).width();
		break;
	default:
		warning("CastMember::getField(): Unprocessed getting field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
	//TODO find out about String fields
	}

	return d;
}

bool CastMember::setField(int field, const Datum &d) {
	CastMemberInfo *castInfo = _cast->getCastMemberInfo(_castId);

	switch (field) {
	case kTheBackColor:
		warning("STUB: CastMember::setField(): Unprocessed setting field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
		return false;
	case kTheCastType:
		warning("CastMember::setField(): Attempt to set read-only field %s of cast %d", g_lingo->entity2str(field), _castId);
		return false;
	case kTheFileName:
		if (!castInfo) {
			warning("CastMember::setField(): CastMember info for %d not found", _castId);
			return false;
		}
		castInfo->fileName = d.asString();
		return true;
	case kTheForeColor:
		warning("STUB: CastMember::setField(): Unprocessed setting field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
		return false;
	case kTheHeight:
		warning("CastMember::setField(): Attempt to set read-only field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
		return false;
	case kTheName:
		if (!castInfo) {
			warning("CastMember::setField(): CastMember info for %d not found", _castId);
			return false;
		}
		castInfo->name = d.asString();
		return true;
	case kTheRect:
		warning("STUB: CastMember::setField(): Unprocessed setting field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
		return false;
	case kThePurgePriority:
		_purgePriority = CLIP<int>(d.asInt(), 0, 3);
		return true;
	case kTheScriptText:
		if (!castInfo) {
			warning("CastMember::setField(): CastMember info for %d not found", _castId);
			return false;
		}
		_cast->_lingoArchive->addCode(*d.u.s, kCastScript, _castId);
		castInfo->script = d.asString();
		return true;
	case kTheWidth:
		warning("CastMember::setField(): Attempt to set read-only field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
		return false;
	default:
		warning("CastMember::setField(): Unprocessed setting field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
	}

	return false;
}

bool DigitalVideoCastMember::hasField(int field) {
	switch (field) {
	case kTheCenter:
	case kTheController:
	case kTheCrop:
	case kTheDirectToStage:
	case kTheDuration:
	case kTheFrameRate:
	case kTheLoop:
	case kTheMovieRate:
	case kTheMovieTime:
	case kThePausedAtStart:
	case kThePreLoad:
	case kTheSound:
	case kTheVideo:
	case kTheVolume:
		return true;
	default:
		break;
	}
	return CastMember::hasField(field);
}

Datum DigitalVideoCastMember::getField(int field) {
	Datum d;

	switch (field) {
	case kTheCenter:
		d = _center;
		break;
	case kTheController:
		d = _showControls;
		break;
	case kTheCrop:
		d = _crop;
		break;
	case kTheDirectToStage:
		d = _directToStage;
		break;
	case kTheDuration:
		d = _duration;
		break;
	case kTheFrameRate:
		d = _frameRate;
		break;
	case kTheLoop:
		d = _looping;
		break;
	case kThePausedAtStart:
		d = _pausedAtStart;
		break;
	case kThePreLoad:
		d = _preload;
		break;
	case kTheSound:
		d = _enableSound;
		break;
	case kTheVideo:
		d = _enableVideo;
		break;
	default:
		d = CastMember::getField(field);
	}

	return d;
}

bool DigitalVideoCastMember::setField(int field, const Datum &d) {
	switch (field) {
	case kTheCenter:
		_center = (bool)d.asInt();
		return true;
	case kTheController:
		_showControls = (bool)d.asInt();
		return true;
	case kTheCrop:
		_crop = (bool)d.asInt();
		return true;
	case kTheDirectToStage:
		_directToStage = (bool)d.asInt();
		return true;
	case kTheDuration:
		warning("DigitalVideoCastMember::setField(): Attempt to set read-only field %s of cast %d", g_lingo->entity2str(field), _castId);
		return false;
	case kTheFrameRate:
		_frameRate = d.asInt();
		setFrameRate(d.asInt());
		return true;
	case kTheLoop:
		_looping = (bool)d.asInt();
		if (_looping && _channel && _channel->_movieRate == 0.0) {
			setMovieRate(1.0);
		}
		return true;
	case kThePausedAtStart:
		_pausedAtStart = (bool)d.asInt();
		return true;
	case kThePreLoad:
		_preload = (bool)d.asInt();
		return true;
	case kTheSound:
		_enableSound = (bool)d.asInt();
		return true;
	case kTheVideo:
		_enableVideo = (bool)d.asInt();
		return true;
	default:
		break;
	}

	return CastMember::setField(field, d);
}

bool BitmapCastMember::hasField(int field) {
	switch (field) {
	case kTheDepth:
	case kTheRegPoint:
	case kThePalette:
	case kThePicture:
		return true;
	default:
		break;
	}
	return CastMember::hasField(field);
}

Datum BitmapCastMember::getField(int field) {
	Datum d;

	switch (field) {
	case kTheDepth:
		warning("STUB: BitmapCastMember::getField(): Unprocessed getting field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
		break;
	case kTheRegPoint:
		warning("STUB: BitmapCastMember::getField(): Unprocessed getting field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
		break;
	case kThePalette:
		d = _clut;
		break;
	case kThePicture:
		warning("STUB: BitmapCastMember::getField(): Unprocessed getting field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
		break;
	default:
		d = CastMember::getField(field);
	}

	return d;
}

bool BitmapCastMember::setField(int field, const Datum &d) {
	switch (field) {
	case kTheDepth:
		warning("STUB: BitmapCastMember::setField(): Unprocessed setting field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
		return false;
	case kTheRegPoint:
		warning("STUB: BitmapCastMember::setField(): Unprocessed setting field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
		return false;
	case kThePalette:
		_clut = d.asInt();
		return true;
	case kThePicture:
		warning("STUB: BitmapCastMember::setField(): Unprocessed setting field \"%s\" of cast %d", g_lingo->field2str(field), _castId);
		return false;
	default:
		break;
	}

	return CastMember::setField(field, d);
}

bool TextCastMember::hasField(int field) {
	switch (field) {
	case kTheHilite:
	case kTheText:
	case kTheTextAlign:
	case kTheTextFont:
	case kTheTextHeight:
	case kTheTextSize:
	case kTheTextStyle:
		return true;
	default:
		break;
	}
	return CastMember::hasField(field);
}

Datum TextCastMember::getField(int field) {
	Datum d;

	switch (field) {
	case kTheHilite:
		d = _hilite;
		break;
	case kTheText:
		d = getText().encode(Common::kUtf8);
		break;
	case kTheTextAlign:
		d.type = STRING;
		switch (_textAlign) {
		case kTextAlignLeft:
			d.u.s = new Common::String("left");
			break;
		case kTextAlignCenter:
			d.u.s = new Common::String("center");
			break;
		case kTextAlignRight:
			d.u.s = new Common::String("right");
			break;
		default:
			warning("TextCastMember::getField(): Invalid text align spec");
			break;
		}
		break;
	case kTheTextFont:
		d.u.i = _fontId;
		break;
	case kTheTextHeight:
		d.u.i = getTextHeight();
		break;
	case kTheTextSize:
		d.u.i = getTextSize();
		break;
	case kTheTextStyle:
		d.u.i = _textSlant;
		break;
	default:
		d = CastMember::getField(field);
	}

	return d;
}

bool TextCastMember::setField(int field, const Datum &d) {
	switch (field) {
	case kTheBackColor:
		{
			uint32 color = g_director->transformColor(d.asInt());
			setColors(nullptr, &color);
		}
		return true;
	case kTheForeColor:
		{
			uint32 color = g_director->transformColor(d.asInt());
			setColors(&color, nullptr);
		}
		return true;
	case kTheHilite:
		// TODO: Understand how texts can be selected programmatically as well.
		// since hilite won't affect text castmember, and we may have button info in text cast in D2/3. so don't check type here
		_hilite = (bool)d.asInt();
		_modified = true;
		return true;
		break;
	case kTheText:
		setText(d.asString());
		return true;
	case kTheTextAlign:
		{
			Common::String select = d.asString(true);
			select.toLowercase();

			TextAlignType align;
			if (select == "\"left\"") {
				align = kTextAlignLeft;
			} else if (select == "\"center\"") {
				align = kTextAlignCenter;
			} else if (select == "\"right\"") {
				align = kTextAlignRight;
			} else {
				warning("TextCastMember::setField(): Unknown text align spec: %s", d.asString(true).c_str());
				break;
			}

			_textAlign = align;
			_modified = true;
	}
		return true;
	case kTheTextFont:
		_fontId = d.asInt();
		_modified = true;
		return false;
	case kTheTextHeight:
		_lineSpacing = d.asInt();
		_modified = true;
		return false;
	case kTheTextSize:
		setTextSize(d.asInt());
		return false;
	case kTheTextStyle:
		_textSlant = d.asInt();
		_modified = true;
		return false;
	default:
		break;
	}

	return CastMember::setField(field, d);
}

bool TextCastMember::hasChunkField(int field) {
	switch (field) {
	case kTheForeColor:
	case kTheTextFont:
	case kTheTextHeight:
	case kTheTextSize:
	case kTheTextStyle:
		return true;
	default:
		break;
	}
	return false;
}

Datum TextCastMember::getChunkField(int field, int start, int end) {
	Datum d;

	Graphics::MacText *macText = ((Graphics::MacText *)_widget);
	if (!_widget)
		warning("TextCastMember::getChunkField getting chunk field when there is no linked widget, returning the default value");

	switch (field) {
	case kTheForeColor:
		if (_widget)
			d.u.i = macText->getTextColor(start, end);
		else
			d.u.i = getForeColor();
		break;
	case kTheTextFont:
		if (_widget)
			d.u.i = macText->getTextFont(start, end);
		else
			d.u.i = _fontId;
		break;
	case kTheTextHeight:
		warning("TextCastMember::getChunkField getting text height(line spacing) is not implemented yet, returning the default one");
		d.u.i = _lineSpacing;
		break;
	case kTheTextSize:
		if (_widget)
			d.u.i = macText->getTextSize(start, end);
		else
			d.u.i = _fontSize;
		break;
	case kTheTextStyle:
		if (_widget)
			d.u.i = macText->getTextSlant(start, end);
		else
			d.u.i = _textSlant;
		break;
	default:
		break;
	}

	return d;
}

bool TextCastMember::setChunkField(int field, int start, int end, const Datum &d) {
	Graphics::MacText *macText = ((Graphics::MacText *)_widget);
	if (!_widget)
		warning("TextCastMember::setChunkField setting chunk field when there is no linked widget");

	switch (field) {
	case kTheForeColor:
		if (_widget)
			macText->setTextColor(d.asInt(), start, end);
		return true;
	case kTheTextFont:
		if (_widget)
			macText->setTextFont(d.asInt(), start, end);
		return true;
	case kTheTextHeight:
		warning("TextCastMember::setChunkField setting text height(line spacing) is not implemented yet");
		return false;
	case kTheTextSize:
		if (_widget)
			macText->setTextSize(d.asInt(), start, end);
		return true;
	case kTheTextStyle:
		if (_widget)
			macText->setTextSlant(d.asInt(), start, end);
		return true;
	default:
		break;
	}

	return false;
}

} // End of namespace Director
