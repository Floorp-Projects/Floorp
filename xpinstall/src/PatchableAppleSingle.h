/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Douglas Turner <dougt@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
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



#if PRAGMA_STRUCT_ALIGN
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
