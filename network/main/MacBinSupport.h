/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/* MacBinSupport.h */

#include "xp_file.h"

#define kMBHeaderLength	128L

typedef struct
{
	short	fileState;		/* Current phase of MB read */
	short	fileRefNum;
	long	dataForkLength;
	long	dataBytesRead;
	long	resForkLength;
	long	resBytesRead;
	FSSpec	theFileSpec;
	Byte	mbHeader[kMBHeaderLength];		/* The actual MacBinary header info */
} MB_FileSpec;


#ifdef __cplusplus
extern "C" {
#endif

/* NOTE!! - FSSpecFromPathname_CWrapper is declared in ufilemgr.cp as an
			extern "C" function.  This prototype must match the declaration in
			ufilemgr.cp or bad things will happen
*/
OSErr	FSSpecFromPathname_CWrapper(char * path, FSSpec * outSpec);

int		MB_Stat( const char* name, XP_StatStruct * outStat,  XP_FileType type );

OSErr	MB_Open(const char *name, MB_FileSpec *mbFileSpec);

int32	MB_Read(char *buf, int bufSize, MB_FileSpec *mbFileSpec);

void	MB_Close(MB_FileSpec *mbFileSpec);

#ifdef __cplusplus
}
#endif
