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

#ifndef _IO_H_
#define _IO_H_

typedef struct Input Input;

unsigned char *copy(Input *input);
unsigned char *copyLower(Input *input);
unsigned char *copyMemory(Input *input, unsigned long *len);
const unsigned char *current(Input *input);
unsigned short getByte(Input *input);
void inputFree(Input *input);
unsigned long inputLength(Input *input);
void mark(Input *input, int offset);
Input *readAvailableBytes(int fd);
Input *readStream(int fd, unsigned char *url);
void set(Input *input, const unsigned char *pointer);
unsigned short trimTrailingWhiteSpace(Input *input);
void unGetByte(Input *input);

#endif /* _IO_H_ */
