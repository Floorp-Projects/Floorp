/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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

/* Implements a simple AppleSingle decoder, as described in RFC1740 */
/* http://andrew2.andrew.cmu.edu/rfc/rfc1740.html */

/* Code is a bit ugly-looking (not pure C++ style */
/* Apologies to the Mac guys, but I have a feeling that this file might be */
/* cross-product in the future */


/* xp */
#include "su_aplsn.h"
#include "xp_file.h"
#include "xp_mcom.h"

/* Mac */
#include "ufilemgr.h"

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif

/* struct definitions from RFC1740 */

typedef struct ASHeader /* header portion of AppleSingle */ 
{
    /* AppleSingle = 0x00051600; AppleDouble = 0x00051607 */
       uint32 magicNum; /* internal file type tag */ 
       uint32 versionNum; /* format version: 2 = 0x00020000 */ 
       uint8 filler[16]; /* filler, currently all bits 0 */ 
       uint16 numEntries; /* number of entries which follow */
} ASHeader ; /* ASHeader */ 

typedef struct ASEntry /* one AppleSingle entry descriptor */ 
{
	uint32 entryID; /* entry type: see list, 0 invalid */
	uint32 entryOffset; /* offset, in octets, from beginning */
	                          /* of file to this entry's data */
	uint32 entryLength; /* length of data in octets */
} ASEntry; /* ASEntry */

typedef struct ASFinderInfo
{
	FInfo ioFlFndrInfo; /* PBGetFileInfo() or PBGetCatInfo() */
	FXInfo ioFlXFndrInfo; /* PBGetCatInfo() (HFS only) */
} ASFinderInfo; /* ASFinderInfo */

typedef struct ASMacInfo        /* entry ID 10, Macintosh file information */
{
       uint8 filler[3]; /* filler, currently all bits 0 */ 
       uint8 ioFlAttrib; /* PBGetFileInfo() or PBGetCatInfo() */
} ASMacInfo;

typedef struct ASFileDates      /* entry ID 8, file dates info */
{
	int32 create; /* file creation date/time */
	int32 modify; /* last modification date/time */
	int32 backup; /* last backup date/time */
	int32 access; /* last access date/time */
} ASFileDates; /* ASFileDates */

/* entryID list */
#define AS_DATA         1 /* data fork */
#define AS_RESOURCE     2 /* resource fork */
#define AS_REALNAME     3 /* File's name on home file system */
#define AS_COMMENT      4 /* standard Mac comment */
#define AS_ICONBW       5 /* Mac black & white icon */
#define AS_ICONCOLOR    6 /* Mac color icon */
      /*              7       /* not used */
#define AS_FILEDATES    8 /* file dates; create, modify, etc */
#define AS_FINDERINFO   9 /* Mac Finder info & extended info */
#define AS_MACINFO      10 /* Mac file info, attributes, etc */
#define AS_PRODOSINFO   11 /* Pro-DOS file info, attrib., etc */
#define AS_MSDOSINFO    12 /* MS-DOS file info, attributes, etc */
#define AS_AFPNAME      13 /* Short name on AFP server */
#define AS_AFPINFO      14 /* AFP file info, attrib., etc */

#define AS_AFPDIRID     15 /* AFP directory ID */

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

/* su_EntryToMacFile
 * Blasts the bytes specified in the entry to already opened Mac file
 */
static int
su_EntryToMacFile( ASEntry inEntry, XP_File inFile, uint16 inRefNum)
{
#define BUFFER_SIZE 8192

	char buffer[BUFFER_SIZE];
	size_t totalRead = 0, bytesRead;
	OSErr err;

	if ( XP_FileSeek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
		return -1 ;

	while ( totalRead < inEntry.entryLength )
	{
// Should we yield in here?
		bytesRead = XP_FileRead( buffer, BUFFER_SIZE, inFile );
		if ( bytesRead <= 0 )
			return -1;
		long bytesToWrite = totalRead + bytesRead > inEntry.entryLength ? 
									inEntry.entryLength - totalRead :
									bytesRead;

		totalRead += bytesRead;
		err = FSWrite(inRefNum, &bytesToWrite, buffer);
		if (err != noErr)
			return err;
	}
	return 0;	
}

/* su_ProcessDataFork
 * blast the data fork to the disk 
 * returns 0 on success
 */
static int 
su_ProcessDataFork( ASEntry inEntry, XP_File inFile, FSSpec * ioSpec )
{
	int16	refNum;
	OSErr err;
	
	/* Setup the files */
	err = FSpOpenDF (ioSpec, fsWrPerm,  &refNum);

	if ( err == noErr )
		err = su_EntryToMacFile( inEntry, inFile, refNum );
	
	FSClose( refNum );
	return err;
}

/* su_ProcessDataFork
 * blast the resource fork to the disk 
 * returns 0 on success
 */
static int 
su_ProcessResourceFork( ASEntry inEntry, XP_File inFile, FSSpec * ioSpec )
{
	int16	refNum;
	OSErr err;
	
	err = FSpOpenRF (ioSpec, fsWrPerm,  &refNum);

	if ( err == noErr )
		err = su_EntryToMacFile( inEntry, inFile, refNum );
	
	FSClose( refNum );
	return err;
}

/* su_ProcessRealName
 * Renames the file to its real name
 */
static int 
su_ProcessRealName( ASEntry inEntry, XP_File inFile, FSSpec * ioSpec )
{
	Str255 newName;
	OSErr err;

	if ( inEntry.entryLength > 32 )	/* Max file name length for the Mac */
		return -1;
	
	if ( XP_FileSeek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
		return -1 ;

	if ( XP_FileRead( &newName[1], inEntry.entryLength, inFile ) != inEntry.entryLength )
		return -1;
	newName[0] = inEntry.entryLength;
	err =  FSpRename(ioSpec, newName);

	if (err == noErr)
		XP_MEMCPY( ioSpec->name, newName, 32 );
	return err;
}

/* su_ProcessComment
 * Sets the file comment
 */
static int 
su_ProcessComment( ASEntry inEntry, XP_File inFile, FSSpec * ioSpec )
{
	Str255 newComment;
	if ( inEntry.entryLength > 32 )	/* Max file name length for the Mac */
		return -1;
	
	if ( XP_FileSeek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
		return -1 ;

	if ( XP_FileRead( &newComment[1], inEntry.entryLength, inFile ) != inEntry.entryLength )
		return -1;
	newComment[0] = inEntry.entryLength;
	CFileMgr::FileSetComment(*ioSpec, newComment );
	return 0;
}

/* su_ProcessComment
 * Sets the modification dates
 */
static int 
su_ProcessFileDates( ASEntry inEntry, XP_File inFile, FSSpec * ioSpec )
{
	ASFileDates dates;
	OSErr err;
	if ( inEntry.entryLength != sizeof(dates) )	/* Max file name length for the Mac */
		return -1;

	if ( XP_FileSeek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
		return -1 ;

	if ( XP_FileRead( &dates, inEntry.entryLength, inFile ) != inEntry.entryLength )
		return -1;

	Str31 name;
	XP_MEMCPY(name, ioSpec->name, ioSpec->name[0] + 1);
	CInfoPBRec pb;
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
	pb.hFileInfo.ioDirID = ioSpec->parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	err = PBGetCatInfoSync(&pb);
	if ( err != noErr )
		return -1;
#define YR_2000_SECONDS 3029572800
	pb.hFileInfo.ioFlCrDat = dates.create + 3029572800;
	pb.hFileInfo.ioFlMdDat = dates.modify + 3029572800;
	pb.hFileInfo.ioFlBkDat = dates.backup + 3029572800;
	/* Not sure if mac has the last access time */
	
	XP_MEMCPY(name, ioSpec->name, ioSpec->name[0] + 1);
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
	pb.hFileInfo.ioDirID = ioSpec->parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	err = PBSetCatInfoSync(&pb);
	
	return err;
}


/* su_ProcessFinderInfo
 * Sets the Finder info
 */
static int 
su_ProcessFinderInfo( ASEntry inEntry, XP_File inFile, FSSpec * ioSpec )
{
	ASFinderInfo info;
	OSErr err;

	if (inEntry.entryLength != sizeof( ASFinderInfo ))
		return -1;

	if ( XP_FileSeek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
		return -1 ;

	if ( XP_FileRead( &info, sizeof(info), inFile) != inEntry.entryLength )
		return -1;
	
	err = FSpSetFInfo(ioSpec, &info.ioFlFndrInfo);
	if ( err != noErr )
		return -1;

	Str31 name;
	XP_MEMCPY(name, ioSpec->name, ioSpec->name[0] + 1);
	CInfoPBRec pb;
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
	pb.hFileInfo.ioDirID = ioSpec->parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	err = PBGetCatInfoSync(&pb);
	if ( err != noErr )
		return -1;
	
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
	pb.hFileInfo.ioDirID = ioSpec->parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	pb.hFileInfo.ioFlXFndrInfo = info.ioFlXFndrInfo;
	err = PBSetCatInfoSync(&pb);
	
	return err;
}

/* su_ProcessMacInfo
 * Sets the Finder info
 */
static int 
su_ProcessMacInfo( ASEntry inEntry, XP_File inFile, FSSpec * ioSpec )
{
	ASMacInfo info;
	OSErr err;

	if (inEntry.entryLength != sizeof( ASMacInfo ))
		return -1;

	if ( XP_FileSeek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
		return -1 ;

	if ( XP_FileRead( &info, sizeof(info), inFile) != inEntry.entryLength )
		return -1;
	
	Str31 name;
	XP_MEMCPY(name, ioSpec->name, ioSpec->name[0] + 1);
	CInfoPBRec pb;
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
	pb.hFileInfo.ioDirID = ioSpec->parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	err = PBGetCatInfoSync(&pb);
	if ( err != noErr )
		return -1;
	
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
	pb.hFileInfo.ioDirID = ioSpec->parID;
	pb.hFileInfo.ioFlAttrib = info.ioFlAttrib;
	err = PBSetCatInfoSync(&pb);
	return err;
}

static int 
su_ProcessASEntry( ASEntry inEntry, XP_File inFile, FSSpec * ioSpec )
{
	switch (inEntry.entryID)
	{
		case AS_DATA:
			return su_ProcessDataFork( inEntry, inFile, ioSpec );
			break;
		case AS_RESOURCE:
			return su_ProcessResourceFork( inEntry, inFile, ioSpec );
			break;
		case AS_REALNAME:
			su_ProcessRealName( inEntry, inFile, ioSpec );
			return 0;	// Ignore these errors in ASD
		case AS_COMMENT:
			return su_ProcessComment( inEntry, inFile, ioSpec );
			break;
		case AS_ICONBW:
//			return su_ProcessIconBW( inEntry, inFile, ioSpec );
			break;
		case AS_ICONCOLOR:
//			return su_ProcessIconColor( inEntry, inFile, ioSpec );
			break;
		case AS_FILEDATES:
			return su_ProcessFileDates( inEntry, inFile, ioSpec );
			break;
		case AS_FINDERINFO:
			return su_ProcessFinderInfo( inEntry, inFile, ioSpec );
			break;
		case AS_MACINFO:
			return su_ProcessMacInfo( inEntry, inFile, ioSpec );
			break;
		case AS_PRODOSINFO:
		case AS_MSDOSINFO:
		case AS_AFPNAME:
		case AS_AFPINFO:
		case AS_AFPDIRID:
		default:
			return 0;
	}
	return 0;
}

int 
SU_DecodeAppleSingle(const char * inSrc, char ** dst)
{
	XP_File inFile = NULL;
	FSSpec outFileSpec;
	size_t	bytesRead;
	ASHeader header;
	OSErr err;
	
	if ( (inSrc == NULL) || (dst == NULL))
		return -1;

	*dst = NULL;

	XP_MEMSET(&outFileSpec, sizeof(outFileSpec), 0);
	
	inFile = XP_FileOpen(inSrc, xpURL, XP_FILE_READ_BIN);
	if (inFile == 0)
		goto fail;
	
	/* Header validity check */
	{
		bytesRead = XP_FileRead(&header, sizeof(ASHeader),  inFile );
		if ( bytesRead != sizeof(ASHeader))
			goto fail;
		if ( header.magicNum != 0x00051600 )
			goto fail;
		if ( header.versionNum != 0x00020000 )
			goto fail;
		if ( header.numEntries == 0 )	/* nothing in this file ? */
			goto fail;
	}
	
	/* Create a new file spec, right next to the old file */
	{
		char * temp = WH_FileName( inSrc, xpURL );
		if ( temp == NULL )
			goto fail;
		c2pstr( temp );
		err = FSMakeFSSpec(0, 0, (unsigned char *) temp, &outFileSpec);
		if ( err != noErr )
			goto fail;
		XP_FREE(temp);

		err = CFileMgr::UniqueFileSpec( outFileSpec, "decode", outFileSpec);
		if ( err != noErr )
			goto fail;
		err = FSpCreate( &outFileSpec, 'MOSS', '????', 0);
		if ( err != noErr )
			goto fail;
	}
	
	/* Loop through the entries, processing each */
	/* Set the time/date stamps last, because otherwise they'll be destroyed
	   when we write */
	{
		Boolean hasDateEntry = FALSE;
		ASEntry dateEntry;
		for ( int i=0; i < header.numEntries; i++ )
		{
			ASEntry entry;
			size_t offset = sizeof( ASHeader ) + sizeof( ASEntry ) * i;
 			if ( XP_FileSeek( inFile, offset, SEEK_SET ) != 0 )
 				goto fail;
 			if ( XP_FileRead( &entry, sizeof( entry ), inFile ) != sizeof( entry ))
 				goto fail;
 			if ( entry.entryID == AS_FILEDATES )
 			{
 				hasDateEntry = TRUE;
 				dateEntry = entry;
 			}
 			else
	 			err = su_ProcessASEntry( entry, inFile, &outFileSpec );
 			if ( err != 0)
 				goto fail;
		}
		if ( hasDateEntry )
			err = su_ProcessASEntry( dateEntry, inFile, &outFileSpec );
		if ( err != 0)
			goto fail;
	}
	
	/* Return the new file specs in xpURL form */
	
	{
		char * temp = CFileMgr::GetURLFromFileSpec(outFileSpec);
		if ( temp == NULL )
			goto fail;
		*dst = XP_STRDUP(&temp[XP_STRLEN("file://")]);
		XP_FREE(temp);
		if (*dst == NULL)
			goto fail;
	}
	
	XP_FileClose( inFile);
	return 0;

fail:
	if ( inFile )
		XP_FileClose( inFile);
	
	FSpDelete(&outFileSpec);
	if (*dst)
		XP_FREE( *dst);
	*dst = NULL;
	return -218;  /* unique error code to help tracking problems */
}