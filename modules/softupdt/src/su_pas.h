/* -*- Mode: C++; tab-width: 4; tabs-indent-mode: nil -*-
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef SU_PAS_H
#define SU_PAS_H


#include <Errors.h>
#include <Types.h>
#include <Files.h>
#include <Script.h>
#include <Resources.h>

typedef struct PASHeader /* header portion of Patchable AppleSingle */
{
       UInt32 magicNum; 	/* internal file type tag = 0x00244200*/
       UInt32 versionNum; 	/* format version: 1 = 0x00010000 */
       UInt8 filler[16]; 	/* filler */
       UInt16 numEntries; 	/* number of entries which follow */
} PASHeader ; 


typedef struct PASEntry 	/* one Patchable AppleSingle entry descriptor */
{
        UInt32 entryID; 	/* entry type: see list, 0 invalid 		*/
        UInt32 entryOffset; /* offset, in bytes, from beginning 	*/
                            /* of file to this entry's data 		*/
        UInt32 entryLength; /* length of data in octets 			*/
        
} PASEntry; 


typedef struct PASMiscInfo
{
	short	fileHasResFork;
	short	fileResAttrs;
	OSType 	fileType;
	OSType	fileCreator;
	UInt32	fileFlags;

} PASMiscInfo; 


typedef struct PASResFork
{
	short	NumberOfTypes;
	
} PASResFork; 


typedef struct PASResource
{
	short			attr;
	short			attrID;
	OSType			attrType;
	Str255			attrName;
	unsigned long	length;
		
} PASResource; 



#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif


#define kCreator	'MOSS'
#define kType		'PASf'
#define PAS_BUFFER_SIZE (1024*512)

#define PAS_MAGIC_NUM	(0x00244200)
#define PAS_VERSION		(0x00010000)

enum
{
	ePas_Data	=	1,
	ePas_Misc,
	ePas_Resource
};

#ifdef __cplusplus
extern "C" {
#endif

/* Prototypes */
OSErr PAS_EncodeFile(FSSpec *inSpec, FSSpec *outSpec);
OSErr PAS_DecodeFile(FSSpec *inSpec, FSSpec *outSpec);



#ifdef __cplusplus
}
#endif

#endif /* SU_PAS_H */
