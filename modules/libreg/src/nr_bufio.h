/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are 
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 *
 * Contributors:
 *     Daniel Veditz <dveditz@netscape.com>
 *     Edward Kandrot <kandrot@netscape.com>
 */

/* nr_bufio.h
 * Buffered I/O routines to improve registry performance
 * 
 * the routines mirror fopen(), fclose() et al
 *
 * __NOTE__: the filenames are *native* filenames, not NSPR names.
 */

#ifndef _NR_BUFIO_H_
#define _NR_BUFIO_H_

typedef struct BufioFileStruct BufioFile;

BufioFile*  bufio_Open(const char* name, const char* mode);
int         bufio_Close(BufioFile* file);
int         bufio_Seek(BufioFile* file, PRInt32 offset, int whence);
PRUint32    bufio_Read(BufioFile* file, char* dest, PRUint32 count);
PRUint32    bufio_Write(BufioFile* file, const char* src, PRUint32 count);
PRInt32     bufio_Tell(BufioFile* file);
int         bufio_Flush(BufioFile* file);
int         bufio_SetBufferSize(BufioFile* file, int bufsize);

#endif  /* _NR_BUFIO_H_ */

