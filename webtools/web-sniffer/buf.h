/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is SniffURI.
 *
 * The Initial Developer of the Original Code is
 * Erik van der Poel <erik@vanderpoel.org>.
 * Portions created by the Initial Developer are Copyright (C) 1998-2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _BUF_H_
#define _BUF_H_

typedef struct Buf Buf;

Buf *bufAlloc(int fd);
unsigned char *bufCopy(Buf *buf);
unsigned char *bufCopyLower(Buf *buf);
unsigned char *bufCopyMemory(Buf *buf, unsigned long *len);
unsigned long bufCurrent(Buf *buf);
int bufError(Buf *buf);
void bufFree(Buf *buf);
unsigned short bufGetByte(Buf *buf);
void bufMark(Buf *buf, int offset);
void bufPutChar(Buf *buf, unsigned char c);
void bufPutString(Buf *buf, unsigned char *str);
void bufSend(Buf *buf);
void bufSet(Buf *buf, unsigned long offset);
void bufSetFD(Buf *buf, int fd);
unsigned short bufTrimTrailingWhiteSpace(Buf *buf);
void bufUnGetByte(Buf *buf);
void bufWrite(Buf *buf);

#endif /* _BUF_H_ */
