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

#ifndef DIRECTOR_LINGO_XLIBS_SERIALPORTXOBJ_H
#define DIRECTOR_LINGO_XLIBS_SERIALPORTXOBJ_H

namespace Director {

class SerialPortXObject : public Object<SerialPortXObject> {
public:
	SerialPortXObject(ObjectType objType);
};

namespace SerialPortXObj {

void open(int type);
void close(int type);

void m_new(int nargs);
void m_getPortNum(int nargs);
void m_writeString(int nargs);
void m_writeChar(int nargs);
void m_readString(int nargs);
void m_readChar(int nargs);
void m_readCount(int nargs);
void m_readFlush(int nargs);
void m_configChan(int nargs);
void m_hShakeChan(int nargs);
void m_setUp(int nargs);

} // End of namespace SerialPortXObj

} // End of namespace Director

#endif
