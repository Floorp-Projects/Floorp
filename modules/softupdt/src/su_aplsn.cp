/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

/* Implements a simple AppleSingle decoder, as described in RFC1740 */
/* http://andrew2.andrew.cmu.edu/rfc/rfc1740.html */


#include "su_aplsn.h"

#include "xp_file.h"
#include "xp_mcom.h"

/* Mac */
#include "ufilemgr.h"

/* MoreFiles */
#include "MoreDesktopMgr.h"

/* struct definitions from RFC1740 */

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=mac68k
#endif

typedef struct ASHeader /* header portion of AppleSingle */
{
    /* AppleSingle = 0x00051600; AppleDouble = 0x00051607 */
       UInt32 magicNum; /* internal file type tag */
       UInt32 versionNum; /* format version: 2 = 0x00020000 */
       UInt8 filler[16]; /* filler, currently all bits 0 */
       UInt16 numEntries; /* number of entries which follow */
} ASHeader ; /* ASHeader */

typedef struct ASEntry /* one AppleSingle entry descriptor */
{
        UInt32 entryID; /* entry type: see list, 0 invalid */
        UInt32 entryOffset; /* offset, in octets, from beginning */
                                  /* of file to this entry's data */
        UInt32 entryLength; /* length of data in octets */
} ASEntry; /* ASEntry */

typedef struct ASFinderInfo
{
        FInfo ioFlFndrInfo; /* PBGetFileInfo() or PBGetCatInfo() */
        FXInfo ioFlXFndrInfo; /* PBGetCatInfo() (HFS only) */
} ASFinderInfo; /* ASFinderInfo */

typedef struct ASMacInfo        /* entry ID 10, Macintosh file information */
{
       UInt8 filler[3]; /* filler, currently all bits 0 */
       UInt8 ioFlAttrib; /* PBGetFileInfo() or PBGetCatInfo() */
} ASMacInfo;

typedef struct ASFileDates      /* entry ID 8, file dates info */
{
        SInt32 create; /* file creation date/time */
        SInt32 modify; /* last modification date/time */
        SInt32 backup; /* last backup date/time */
        SInt32 access; /* last access date/time */
} ASFileDates; /* ASFileDates */

#if PRAGMA_ALIGN_SUPPORTED
#pragma options align=reset
#endif

/* Prototypes */
OSErr su_decodeFileDates( ASEntry inEntry, FILE * inFile, FSSpec * ioSpec );
OSErr su_decodeRealName( ASEntry inEntry, FILE * inFile, FSSpec * ioSpec );
OSErr su_encodeRealName(FILE * outfp, const FSSpec * inSpec );
OSErr su_encodeComment(FILE * outfp, Str255 comment );
OSErr su_encodeFileDates(FILE * outfp, const CInfoPBRec * pb);
OSErr su_encodeFinderInfo( FILE * outfp, const FSSpec * inSpec, const CInfoPBRec * pb);
OSErr su_encodeMacInfo( FILE * outfp, const CInfoPBRec * pb );
OSErr su_macFileToFileStream( FILE * outfp, SInt16 refNum, UInt32 bytesExpected);
OSErr su_encodeDataFork( FILE * outfp, const FSSpec * inFile, UInt32 bytesExpected);
OSErr su_encodeResourceFork( FILE * outfp, const FSSpec * inFile, UInt32 bytesExpected);
OSErr su_decodeAppleSingle(const char * inFile, FSSpec * outSpec, long wantedEntries);
OSErr su_encodeAppleSingle(const FSSpec * inSpec, const char * outFile, long wantedEntries);

/* su_asEntryToMacFile
 * Blasts the bytes specified in the entry to already opened Mac file
 */
static OSErr
su_asEntryToMacFile( ASEntry inEntry, FILE * inFile, UInt16 inRefNum)
{
#define BUFFER_SIZE 8192

        char buffer[BUFFER_SIZE];
        size_t totalRead = 0, bytesRead;
        long bytesToWrite;
        OSErr err;

        if ( XP_FileSeek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
                return -1 ;

        while ( totalRead < inEntry.entryLength )
        {
// Should we yield in here?
                bytesRead = XP_FileRead( buffer, BUFFER_SIZE, inFile );
                if ( bytesRead <= 0 )
                        return ioErr;
                bytesToWrite = totalRead + bytesRead > inEntry.entryLength ?
                                                                        inEntry.entryLength - totalRead :
                                                                        bytesRead;

                totalRead += bytesRead;
                err = FSWrite(inRefNum, &bytesToWrite, buffer);
                if (err != noErr)
                        return err;
        }
        return 0;
}

static OSErr
su_decodeDataFork( ASEntry inEntry, FILE * inFile, const FSSpec * ioSpec )
{
        SInt16  refNum;
        OSErr err;

        /* Setup the files */
        err = FSpOpenDF (ioSpec, fsWrPerm,  &refNum);

        if ( err == noErr )
                err = su_asEntryToMacFile( inEntry, inFile, refNum );

        FSClose( refNum );
        return err;
}

static OSErr
su_decodeResourceFork( ASEntry inEntry, FILE * inFile, const FSSpec * ioSpec )
{
        SInt16  refNum;
        OSErr err;

        err = FSpOpenRF (ioSpec, fsWrPerm,  &refNum);

        if ( err == noErr )
                err = su_asEntryToMacFile( inEntry, inFile, refNum );

        FSClose( refNum );
        return err;
}

static OSErr
su_decodeComment( ASEntry inEntry, FILE * inFile, const FSSpec * ioSpec )
{
        Str255 newComment;
        if ( inEntry.entryLength > 32 ) /* Max file name length for the Mac */
                return -1;

        if ( XP_FileSeek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
                return -1 ;

        if ( XP_FileRead( &newComment[1], inEntry.entryLength, inFile ) != inEntry.entryLength )
                return -1;
        newComment[0] = inEntry.entryLength;

        return FSpDTSetComment(ioSpec, newComment);
}

static OSErr
su_decodeFinderInfo( ASEntry inEntry, FILE * inFile, const FSSpec * ioSpec )
{
        ASFinderInfo info;
        OSErr err;
        Str31 name;
        CInfoPBRec pb;

        if (inEntry.entryLength != sizeof( ASFinderInfo ))
                return -1;

        if ( XP_FileSeek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
                return -1 ;

        if ( XP_FileRead( &info, sizeof(info), inFile) != inEntry.entryLength )
                return -1;

        err = FSpSetFInfo(ioSpec, &info.ioFlFndrInfo);
        if ( err != noErr )
                return -1;

        memcpy(name, ioSpec->name, ioSpec->name[0] + 1);
        pb.hFileInfo.ioNamePtr = name;
        pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
        pb.hFileInfo.ioDirID = ioSpec->parID;
        pb.hFileInfo.ioFDirIndex = 0;   /* use ioNamePtr and ioDirID */
        err = PBGetCatInfoSync(&pb);
        if ( err != noErr )
                return err;

        pb.hFileInfo.ioNamePtr = name;
        pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
        pb.hFileInfo.ioDirID = ioSpec->parID;
        pb.hFileInfo.ioFDirIndex = 0;   /* use ioNamePtr and ioDirID */
        pb.hFileInfo.ioFlXFndrInfo = info.ioFlXFndrInfo;
        err = PBSetCatInfoSync(&pb);
        return err;
}

static OSErr
su_decodeMacInfo( ASEntry inEntry, FILE * inFile, const FSSpec * ioSpec )
{
        ASMacInfo info;
        OSErr err;
        Str31 name;
        CInfoPBRec pb;

        if (inEntry.entryLength != sizeof( ASMacInfo ))
                return -1;

        if ( XP_FileSeek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
                return -1 ;

        if ( XP_FileRead( &info, sizeof(info), inFile) != inEntry.entryLength )
                return -1;

        memcpy(name, ioSpec->name, ioSpec->name[0] + 1);
        pb.hFileInfo.ioNamePtr = name;
        pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
        pb.hFileInfo.ioDirID = ioSpec->parID;
        pb.hFileInfo.ioFDirIndex = 0;   /* use ioNamePtr and ioDirID */
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

OSErr
su_decodeFileDates( ASEntry inEntry, FILE * inFile, FSSpec * ioSpec )
{
        ASFileDates dates;
        OSErr err;
        Str31 name;
        CInfoPBRec pb;

        if ( inEntry.entryLength != sizeof(dates) )     /* Max file name length for the Mac */
                return -1;

        if ( XP_FileSeek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
                return -1 ;

        if ( XP_FileRead( &dates, inEntry.entryLength, inFile ) != inEntry.entryLength )
                return -1;

        memcpy(name, ioSpec->name, ioSpec->name[0] + 1);
        pb.hFileInfo.ioNamePtr = name;
        pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
        pb.hFileInfo.ioDirID = ioSpec->parID;
        pb.hFileInfo.ioFDirIndex = 0;   /* use ioNamePtr and ioDirID */
        err = PBGetCatInfoSync(&pb);
        if ( err != noErr )
                return err;
#define YR_2000_SECONDS 3029572800
        pb.hFileInfo.ioFlCrDat = dates.create + YR_2000_SECONDS;
        pb.hFileInfo.ioFlMdDat = dates.modify + YR_2000_SECONDS;
        pb.hFileInfo.ioFlBkDat = dates.backup + YR_2000_SECONDS;
        /* Not sure if mac has the last access time dates.access*/

        memcpy(name, ioSpec->name, ioSpec->name[0] + 1);
        pb.hFileInfo.ioNamePtr = name;
        pb.hFileInfo.ioVRefNum = ioSpec->vRefNum;
        pb.hFileInfo.ioDirID = ioSpec->parID;
        pb.hFileInfo.ioFDirIndex = 0;   /* use ioNamePtr and ioDirID */
        err = PBSetCatInfoSync(&pb);

        return err;
}

OSErr
su_decodeRealName( ASEntry inEntry, FILE * inFile, FSSpec * ioSpec )
{
        Str255 newName;
        OSErr err;

        if ( inEntry.entryLength > 32 ) /* Max file name length for the Mac */
                return -1;

        if ( XP_FileSeek( inFile, inEntry.entryOffset, SEEK_SET) != 0 )
                return -1 ;

        if ( XP_FileRead( &newName[1], inEntry.entryLength, inFile ) != inEntry.entryLength )
                return -1;

        newName[0] = inEntry.entryLength;
        err =  FSpRename(ioSpec, newName);

        if (err == noErr)
                memcpy( ioSpec->name, newName, 32 );
        return err;

}

static OSErr
su_processASEntry( ASEntry inEntry, FILE * inFile, const FSSpec * ioSpec )
{
        switch (inEntry.entryID)
        {
                case AS_DATA:
                        return su_decodeDataFork( inEntry, inFile, ioSpec );
                        break;
                case AS_RESOURCE:
                        return su_decodeResourceFork( inEntry, inFile, ioSpec );
                        break;
                case AS_REALNAME:
//                      return su_decodeRealName( inEntry, inFile, ioSpec );
                        break;
                case AS_COMMENT:
                        return su_decodeComment( inEntry, inFile, ioSpec );
                        break;
                case AS_ICONBW:
//                      return su_decodeIconBW( inEntry, inFile, ioSpec );
                        break;
                case AS_ICONCOLOR:
//                      return su_decodeIconColor( inEntry, inFile, ioSpec );
                        break;
                case AS_FILEDATES:
//                      return su_decodeFileDates( inEntry, inFile, ioSpec );
                        break;
                case AS_FINDERINFO:
                        return su_decodeFinderInfo( inEntry, inFile, ioSpec );
                        break;
                case AS_MACINFO:
                        return su_decodeMacInfo( inEntry, inFile, ioSpec );
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


/* Decodes
 * Arguments:
 * inFile - name of the AppleSingle file
 * outSpec - destination. If destination is renamed (as part of decoding of realName)
 *                      the outSpec is modified to represent the new name
 */
OSErr
su_decodeAppleSingle(const char * inFile, FSSpec * outSpec, long wantedEntries)
{
        FILE * in;
        size_t  bytesRead;
        ASHeader header;
        OSErr err;
        int i;

        in = XP_FileOpen( inFile, xpURL, XP_FILE_READ_BIN);
        if ( in == NULL )
                return fnfErr;

        /* Read in the header */
        {
                bytesRead = XP_FileRead(&header, sizeof(ASHeader),  in );
                if ( bytesRead != sizeof(ASHeader))
                        goto fail;
                if ( header.magicNum != AS_MAGIC_NUM )
                        goto fail;
                if ( header.versionNum != 0x00020000 )
                        goto fail;
                if ( header.numEntries == 0 )   /* nothing in this file ? */
                        goto fail;
        }
        /* Create the output file */
        FSpDelete( outSpec );   /* Preventive delete, not sure if we need it */
        err = FSpCreate( outSpec, AS_DEFAULT_CREATOR, AS_DEFAULT_TYPE, 0);
        if ( err != noErr )
                goto fail;

        /* Loop through the entries, processing each */
        /* Set the time/date stamps last, because otherwise they'll be destroyed
           when we write */
        {
                Boolean hasDateEntry = false;
                ASEntry dateEntry;
                for ( i=0; i < header.numEntries; i++ )
                {
                        ASEntry entry;
                        size_t offset = sizeof( ASHeader ) + sizeof( ASEntry ) * i;
                        if ( XP_FileSeek( in, offset, SEEK_SET ) != 0 )
                                goto fail;
                        if ( XP_FileRead( &entry, sizeof( entry ), in ) != sizeof( entry ))
                                goto fail;
                        if ( wantedEntries & ( ((UInt32)1) << (entry.entryID - 1 )))
                                switch (entry.entryID)
                                {
                                        case AS_DATA:
                                                err = su_decodeDataFork( entry, in, outSpec );
                                                break;
                                        case AS_RESOURCE:
                                                err = su_decodeResourceFork( entry, in, outSpec );
                                                break;
                                        case AS_REALNAME:
                                        /* Ignore: ASD will rename later, and we get errors */
                                        /*      err = su_decodeRealName( entry, in, outSpec ); */
                                                break;
                                        case AS_COMMENT:
                                                err = su_decodeComment( entry, in, outSpec );
                                                break;
                                        case AS_FILEDATES:
                                        /* Save it for postprocessing */
                                                hasDateEntry = true;
                                                dateEntry = entry;
                                                break;
                                        case AS_FINDERINFO:
                                                err = su_decodeFinderInfo( entry, in, outSpec );
                                                break;
                                        case AS_MACINFO:
                                                err = su_decodeMacInfo( entry, in, outSpec );
                                                break;
                                        case AS_ICONBW:
                                        case AS_ICONCOLOR:
                                                XP_TRACE(("Can't decode AS_ICONBW..."));
                                                break;
                                        case AS_PRODOSINFO:
                                        case AS_MSDOSINFO:
                                        case AS_AFPNAME:
                                        case AS_AFPINFO:
                                        case AS_AFPDIRID:
                                        default:
                                                break;
                                }
                        if ( err != 0)
                                break;
                }
                if ( hasDateEntry )
                        err = su_processASEntry( dateEntry, in, outSpec );
        }
        XP_FileClose(in);
        in = NULL;

        if ( err == noErr )
                return err;
        // else fall through failure
fail:
        if (in)
                XP_FileClose(in);
        FSpDelete( outSpec);
        XP_TRACE(("AppleSingle decoding has failed: %s"));
        return ioErr;
}

/* wrapper for su_decodeAppleSingle - this works with URL strings instead of filespecs*/
int
SU_DecodeAppleSingle(const char * inSrc, char ** dst)
{
	if ( (inSrc == NULL) || (dst == NULL))
		return -1;

	XP_File inFile = NULL;
	FSSpec outFileSpec;
	char * inFilePath = NULL;
	OSErr err;	
	
	XP_MEMSET(&outFileSpec, sizeof(outFileSpec), 0);
	*dst = NULL;

	{
		/* Create a new file spec, right next to the old file */
		char * inFilePath = WH_FileName( inSrc, xpURL );
		if ( inFilePath == NULL )
			goto fail;
		c2pstr( inFilePath );
		err = FSMakeFSSpec(0, 0, (unsigned char *) inFilePath, &outFileSpec);
		if ( err != noErr )
			goto fail;

		err = CFileMgr::UniqueFileSpec( outFileSpec, "decode", outFileSpec);
		if ( err != noErr )
			goto fail;
		err = FSpCreate( &outFileSpec, 'MOSS', '????', 0);
		if ( err != noErr )
			goto fail;
			
		p2cstr( (unsigned char *)inFilePath );  // not sure if we need this
		
		err = su_decodeAppleSingle(inSrc, &outFileSpec, AS_ALLENTRIES ); // FIXME May not want all entries - may want to ignore date, etc.
		if (err != noErr)
			goto fail;
					
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

		if ( inFilePath )
			XP_FREE( inFilePath);
		
		return noErr;				
	}
	
fail:
	{
		if ( inFilePath )
			XP_FREE( inFilePath);
		
		FSpDelete(&outFileSpec);
		if (*dst)
			XP_FREE( *dst);
		*dst = NULL;
		if (err != noErr)
			return err;
		else
			return -1;
	}
}


OSErr
su_encodeRealName(FILE * outfp, const FSSpec * inSpec )
{
        if ( XP_FileWrite( &(inSpec->name[1]), inSpec->name[0], outfp) < 0 )
                return ioErr;
        return noErr;
}

OSErr
su_encodeComment(FILE * outfp, Str255 comment )
{
        if ( XP_FileWrite( &(comment[1]), comment[0], outfp) < 0 )
                return ioErr;
        return noErr;
}

OSErr
su_encodeFileDates(FILE * outfp, const CInfoPBRec * pb)
{
        ASFileDates dates;
#define YR_2000_SECONDS 3029572800
        dates.create = pb->hFileInfo.ioFlCrDat - YR_2000_SECONDS;
        dates.modify = pb->hFileInfo.ioFlMdDat - YR_2000_SECONDS;
        dates.backup = pb->hFileInfo.ioFlBkDat - YR_2000_SECONDS;
        dates.access = 0;       /* Unknown on the mac */
        if ( XP_FileWrite( &dates, sizeof(dates), outfp) < 0 )
                return ioErr;
        return noErr;
}

OSErr
su_encodeFinderInfo( FILE * outfp, const FSSpec * inSpec, const CInfoPBRec * pb)
{
        ASFinderInfo info;
        OSErr err;
        err = FSpGetFInfo(inSpec, &info.ioFlFndrInfo);
        if (err != noErr )
                return err;
        info.ioFlXFndrInfo = pb->hFileInfo.ioFlXFndrInfo;
        if ( XP_FileWrite( &info, sizeof(info), outfp) < 0 )
                return ioErr;
        return noErr;
}

OSErr
su_encodeMacInfo( FILE * outfp, const CInfoPBRec * pb )
{
        ASMacInfo info;
        memset( &info, 0, sizeof(info));
        info.ioFlAttrib = pb->hFileInfo.ioFlAttrib;
        if ( XP_FileWrite( &info, sizeof(info), outfp) < 0 )
                return ioErr;
        return noErr;
}

OSErr
su_macFileToFileStream( FILE * outfp, SInt16 refNum, UInt32 bytesExpected)
{
#define BUFFER_SIZE 8192

        char buffer[BUFFER_SIZE];
        UInt32 totalRead = 0;
        SInt32 currentRead;
        OSErr err;
        while ( totalRead < bytesExpected )
        {
                currentRead = BUFFER_SIZE;
                err = FSRead( refNum, &currentRead, buffer);
                totalRead += currentRead;
                if ( err != noErr && ( totalRead < bytesExpected))
                        return err;
                if ( XP_FileWrite( buffer, currentRead, outfp) < 0)
                        return -1;
        }
        return noErr;
}

OSErr
su_encodeDataFork( FILE * outfp, const FSSpec * inFile, UInt32 bytesExpected)
{
        short refNum;
        OSErr err = noErr;
        err = FSpOpenDF (inFile, fsRdPerm,  &refNum);
        if (err != noErr )
                return err;
        err = su_macFileToFileStream( outfp, refNum, bytesExpected );
        FSClose( refNum );
        return err;
}

OSErr
su_encodeResourceFork( FILE * outfp, const FSSpec * inFile, UInt32 bytesExpected)
{
        short refNum;
        OSErr err = noErr;
        err = FSpOpenRF (inFile, fsRdPerm,  &refNum);
        if (err != noErr )
                return err;
        err = su_macFileToFileStream( outfp, refNum, bytesExpected );
        FSClose( refNum );
        return err;
}

/* Encodes the file as applesingle
 *
 * These are the possible parts that can be encoded as AppleSingle:
      Data Fork              1 Data fork
      Resource Fork          2 Resource fork
      Real Name              3 File's name as created on home file system
      Comment                4 Standard Macintosh comment
      File Dates Info        8 File creation date, modification date,
                                      and so on
      Finder Info            9 Standard Macintosh Finder information
      Macintosh File Info   10 Macintosh file information, attributes  and so on
  * This routine will encode all parts that are relevant (ex no data fork encoding if data fork length is 0)
  */
OSErr
su_encodeAppleSingle(const FSSpec * inSpec, const char * outFile, long wantedEntries)
{
        OSErr err;
        CInfoPBRec cbrec;
        //FInfo fileinfo;
        Boolean needDataFork, needResourceFork, needRealName, needComment, needFileDates, needFinderInfo, needMacInfo;
        ASHeader header;
        ASEntry entry;
        UInt16 numEntries;
        FILE * outfp;
        Str255 comment;
        size_t availableOffset;
        Str255 temp;
        needDataFork = needResourceFork = needRealName = needComment
        = needFileDates = needFinderInfo = needMacInfo = false;

        /* Figure out which parts of will we need to encode */

        memcpy( temp, inSpec->name, inSpec->name[0] + 1);
        cbrec.hFileInfo.ioNamePtr =     temp;
        cbrec.hFileInfo.ioDirID = inSpec->parID;
        cbrec.hFileInfo.ioVRefNum = inSpec->vRefNum;
        cbrec.hFileInfo.ioFDirIndex     = 0;

        err = PBGetCatInfoSync(&cbrec);
        if(err != noErr)
                goto fail;

        if ( FSpDTGetComment( inSpec,comment) != noErr)
                comment[0] = 0;

        needDataFork = (cbrec.hFileInfo.ioFlLgLen > 0)
                                        && ( AS_DATA_BIT & wantedEntries) ; /* Data fork? */
        needResourceFork = (cbrec.hFileInfo.ioFlRLgLen > 0)
                                        && ( AS_RESOURCE_BIT & wantedEntries ); /* Resource fork? */
        needComment = comment[0] != 0
                                        && ( AS_COMMENT_BIT & wantedEntries );
        needRealName = (AS_REALNAME_BIT & wantedEntries) != 0;
        needFileDates = ( AS_FILEDATES_BIT & wantedEntries) != 0;
        needFinderInfo = ( AS_FINDERINFO_BIT & wantedEntries) != 0;
        needMacInfo = ( AS_MACINFO_BIT & wantedEntries) != 0;

        /* The header */
        memset(&header, 0, sizeof(ASHeader));   /* for the filler bits */
        header.magicNum = AS_MAGIC_NUM;
        header.versionNum = 0x00020000;
        numEntries = 0;
        if ( needDataFork ) numEntries++;
        if ( needResourceFork ) numEntries++;
        if ( needRealName ) numEntries++;
        if ( needComment ) numEntries++;
        if ( needFileDates ) numEntries++;
        if ( needFinderInfo ) numEntries++;
        if ( needMacInfo ) numEntries++;
        header.numEntries = numEntries;

        outfp = XP_FileOpen(outFile, xpURL, XP_FILE_WRITE_BIN);
        if ( outfp == NULL)
                goto fail;

        /* write header */
        if ( XP_FileWrite( &header, sizeof(ASHeader), outfp) < 0 )
                goto fail;

        /* write out the entry headers */
        availableOffset = sizeof(ASHeader) + numEntries * sizeof(ASEntry);

        if ( needRealName )
        {
                entry.entryID = AS_REALNAME;
                entry.entryOffset = availableOffset;
                entry.entryLength = inSpec->name[0];
                if ( XP_FileWrite( &entry, sizeof(ASEntry), outfp) < 0 )
                        goto fail;
                availableOffset += entry.entryLength;
        }
        if ( needComment )
        {
                entry.entryID = AS_COMMENT;
                entry.entryOffset = availableOffset;
                entry.entryLength = comment[0];
                if ( XP_FileWrite( &entry, sizeof(ASEntry), outfp) < 0 )
                        goto fail;
                availableOffset += entry.entryLength;
        }
        if ( needFileDates )
        {
                entry.entryID = AS_FILEDATES;
                entry.entryOffset = availableOffset;
                entry.entryLength = sizeof (ASFileDates );
                if ( XP_FileWrite( &entry, sizeof(ASEntry), outfp) < 0 )
                        goto fail;
                availableOffset += entry.entryLength;
        }
        if ( needFinderInfo )
        {
                entry.entryID = AS_FINDERINFO;
                entry.entryOffset = availableOffset;
                entry.entryLength = sizeof (ASFinderInfo );
                if ( XP_FileWrite( &entry, sizeof(ASEntry), outfp) < 0 )
                        goto fail;
                availableOffset += entry.entryLength;
        }
        if ( needMacInfo )
        {
                entry.entryID = AS_MACINFO;
                entry.entryOffset = availableOffset;
                entry.entryLength = sizeof (ASMacInfo );
                if ( XP_FileWrite( &entry, sizeof(ASEntry), outfp) < 0 )
                        goto fail;
                availableOffset += entry.entryLength;
        }
        if ( needDataFork )
        {
                entry.entryID = AS_DATA;
                entry.entryOffset = availableOffset;
                entry.entryLength = cbrec.hFileInfo.ioFlLgLen;
                if ( XP_FileWrite( &entry, sizeof(ASEntry), outfp) < 0 )
                        goto fail;
                availableOffset += entry.entryLength;
        }
        if ( needResourceFork )
        {
                entry.entryID = AS_RESOURCE;
                entry.entryOffset = availableOffset;
                entry.entryLength = cbrec.hFileInfo.ioFlRLgLen;
                if ( XP_FileWrite( &entry, sizeof(ASEntry), outfp) < 0 )
                        goto fail;
                availableOffset += entry.entryLength;
        }

        /* write out the entry data */
        if ( needRealName )
                if ( su_encodeRealName(outfp, inSpec) != noErr )
                        goto fail;
        if ( needComment )
                if ( su_encodeComment(outfp, comment) != noErr )
                        goto fail;
        if ( needFileDates )
                if ( su_encodeFileDates(outfp, &cbrec) != noErr )
                        goto fail;
        if ( needFinderInfo )
                if ( su_encodeFinderInfo(outfp, inSpec, &cbrec) != noErr )
                        goto fail;
        if ( needMacInfo )
                if ( su_encodeMacInfo( outfp, &cbrec ) != noErr )
                        goto fail;
        if ( needDataFork )
                if ( su_encodeDataFork( outfp, inSpec, cbrec.hFileInfo.ioFlLgLen) != noErr )
                        goto fail;
        if ( needResourceFork )
                if ( su_encodeResourceFork( outfp, inSpec, cbrec.hFileInfo.ioFlRLgLen ) != noErr )
                        goto fail;

        fclose(outfp);
        outfp = NULL;
        /* All done! */
        return noErr;

fail:
        if ( outfp )
                fclose(outfp);
        remove( outFile);
        if (err == noErr)
                err = ioErr;
        XP_TRACE(("Unexpected AppleSingle encoding error %s\n", outFile ));
        return err;
}

/* wrapper for su_encodeAppleSingle which accepts xpURL fileNames instead of fileSpecs */
int
SU_EncodeAppleSingle(const char * inSrc, const char * outFile)
{
	FSSpec	inSpec; 
	OSErr	err = fnfErr;
	
	if (inSrc == NULL || outFile == NULL)
		goto fail;

	XP_MEMSET(&inSpec, sizeof(inSpec), 0);
		
	char *macFilePath =  WH_FileName( inSrc, xpURL );
	
	if (macFilePath == NULL)
		goto fail;


	err = CFileMgr::FSSpecFromLocalUnixPath(inSrc, &inSpec, true);
	if (err != noErr)
		goto fail;

	XP_FREE(macFilePath);
	
	// Encoding must match what the diff tool is doing!
	
	err = su_encodeAppleSingle(&inSpec, outFile, AS_ALLENTRIES );  

	if (err != noErr)
		goto fail;
		
	return err;
	
	fail:
	{
		outFile = NULL;
		if (macFilePath)
			XP_FREE(macFilePath);
		
		if (err != noErr)
			return err;
		else
			return fnfErr;
	
	}
		
}
