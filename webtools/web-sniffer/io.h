/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Web Sniffer.
 * 
 * The Initial Developer of the Original Code is Erik van der Poel.
 * Portions created by Erik van der Poel are
 * Copyright (C) 1998,1999,2000 Erik van der Poel.
 * All Rights Reserved.
 * 
 * Contributor(s): 
 */

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
