/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/ 
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License 
 * for the specific language governing rights and limitations under the 
 * License. 
 * 
 * The Original Code is Mozilla Communicator client code, released March
 * 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation. Portions created by Netscape are Copyright (C) 1999
 * Netscape Communications Corporation. All Rights Reserved.  
 * 
 * Contributors:
 *     Samir Gehani <sgehani@netscape.com>
 */


#ifndef _NS_APPLESINGLEENCODER_H_
	#include "nsAppleSingleEncoder.h"
#endif
 
#include <Files.h>
#include <OSUtils.h>
#include "nsAppleSingleDecoder.h" /* AppleSingle struct definitions */
#include "MoreFilesExtras.h"
#include "IterateDirectory.h"

nsAppleSingleEncoder::nsAppleSingleEncoder()
{
	mInFile = NULL;
	mOutFile = NULL;
}

nsAppleSingleEncoder::~nsAppleSingleEncoder()
{
}

Boolean
nsAppleSingleEncoder::HasResourceFork(FSSpecPtr aFile)
{
	OSErr	err = noErr;
	Boolean bHasResFork = false;
	short	refnum;
	long	bytes2Read = 1;
	char	buf[2];
	
	err = FSpOpenRF(aFile, fsRdPerm, &refnum);
	if (err == noErr)
	{
		err = FSRead(refnum, &bytes2Read, &buf[0]);
		if (err == noErr)	/* err==eofErr if all OK but no res fork */
			bHasResFork = true;
	}
	
	FSClose(refnum);
	
	return bHasResFork;
}

OSErr
nsAppleSingleEncoder::Encode(FSSpecPtr aFile)
{
	OSErr		err = noErr;
	FSSpec		transient, exists;
	
	// handle init in lieu of overloading 
	if (!mOutFile || !mInFile)
	{
		mInFile = aFile;
		mOutFile = aFile;
	}
	
	// param check
	if (!mInFile)
		return paramErr;
		
	// stomp old temp file
	mTransient = &transient;
	err = FSMakeFSSpec(	mInFile->vRefNum, mInFile->parID, kTransientName, 
						mTransient);
	if (err == noErr) 
		FSpDelete(mTransient);
		
	// write to transient file 
	ERR_CHECK( WriteHeader() );
	ERR_CHECK( WriteEntryDescs() );
	ERR_CHECK( WriteEntries() );
	
	// rename transient file to out file	
	err = FSMakeFSSpec( mOutFile->vRefNum, mOutFile->parID, mOutFile->name,
						&exists );
	if (err == noErr)
		FSpDelete(mOutFile);
	FSpRename(mTransient, mOutFile->name);

	return err;
}

OSErr
nsAppleSingleEncoder::Encode(FSSpecPtr aInFile, FSSpecPtr aOutFile)
{
	OSErr err = noErr;
	
	// param check
	if (!aInFile || !aOutFile)
		return paramErr;
		
	mInFile = aInFile;
	mOutFile = aOutFile;
	
	err = Encode(NULL);

	return err;
}

pascal void
EncodeDirIterateFilter(const CInfoPBRec * const cpbPtr, Boolean *quitFlag, void *yourDataPtr)
{	
	OSErr					err = noErr;
	FSSpec 					currFSp;
	nsAppleSingleEncoder* 	thisObj = NULL;
	Boolean					isDir = false;
	long					dummy;
	
	// param check
	if (!yourDataPtr || !cpbPtr || !quitFlag)
		return;
	
	*quitFlag = false;
		
	// extract 'this' -- an nsAppleSingleEncoder instance
	thisObj = (nsAppleSingleEncoder*) yourDataPtr;
	thisObj->ReInit();
	
	// make an FSSpec from the CInfoPBRec*
	err = FSMakeFSSpec(cpbPtr->hFileInfo.ioVRefNum, cpbPtr->hFileInfo.ioFlParID, 
						cpbPtr->hFileInfo.ioNamePtr, &currFSp);
	if (err == noErr)
	{
		FSpGetDirectoryID(&currFSp, &dummy, &isDir);
		
		// if current FSSpec is file
		if (!isDir)
		{
			// if file has res fork
			if (nsAppleSingleEncoder::HasResourceFork(&currFSp))
			{
				// encode file
				thisObj->Encode(&currFSp);
			}
		}
		else
		{
			// else if current FSSpec is folder ignore
			// XXX never reached?
			return;
		}
	}
}

OSErr
nsAppleSingleEncoder::EncodeFolder(FSSpecPtr aFolder)
{
	OSErr	err = noErr;
	long	dummy;
	Boolean	isDir = false;
	
	// check that FSSpec is folder
	if (aFolder)
	{
		FSpGetDirectoryID(aFolder, &dummy, &isDir);
		if (!isDir)
			return dirNFErr;
	}
	
	// recursively enumerate contents of folder (maxLevels=0 means recurse all)
	FSpIterateDirectory(aFolder, 0, EncodeDirIterateFilter, (void*)this);
			
	return err;
}

void
nsAppleSingleEncoder::ReInit()
{
	mInFile = NULL;
	mOutFile = NULL;
}

OSErr
nsAppleSingleEncoder::WriteHeader()
{
	OSErr 		err = noErr;
	ASHeader 	hdr;
	int			i;
	long		bytes2Wr;
	
	// init header info
	hdr.magicNum = kAppleSingleMagicNum; 
	hdr.versionNum = kAppleSingleVerNum;
	for (i=0; i<16; i++)
		hdr.filler[i] = 0;
	hdr.numEntries = kNumASEntries;		
		/*
		** Entries:
		**
		** 1> Real name
		** 2> Finder info
		** 3> File dates
		** 4> Resource fork
		** 5> Data fork
		** 6> Mac info
		*/
	
	bytes2Wr = sizeof(ASHeader);
	ERR_CHECK( FSpCreate(mTransient, 'MOZZ', '????', 0) );
	ERR_CHECK( FSpOpenDF(mTransient, fsRdWrPerm, &mTransRefNum) );
	// ERR_CHECK( SetFPos(mTransRefNum, fsFromStart, 0) ); // XXX revisit
	ERR_CHECK( FSWrite(mTransRefNum, &bytes2Wr, &hdr) );
	if (bytes2Wr != sizeof(ASHeader))
		err = -1;
		
	return err;
}

OSErr
nsAppleSingleEncoder::WriteEntryDescs()
{
	OSErr 		err = noErr;
	long 		offset = sizeof(ASHeader), bytes2Wr = kNumASEntries*sizeof(ASEntry);
	ASEntry 	entries[kNumASEntries];
	int 		i;
	CInfoPBRec	pb;
	
	ERR_CHECK( FSpGetCatInfo(&pb, mInFile) );
	
	// real name
	entries[0].entryOffset = sizeof(ASHeader) + (sizeof(ASEntry) * kNumASEntries);
	entries[0].entryID = AS_REALNAME;
	entries[0].entryLength = pb.hFileInfo.ioNamePtr[0];
		
	// finder info
	entries[1].entryID = AS_FINDERINFO;
	entries[1].entryLength = sizeof(FInfo) + sizeof(FXInfo);
		
	// file dates
	entries[2].entryID = AS_FILEDATES;
	entries[2].entryLength = sizeof(ASFileDates);
		
	// resource fork
	entries[3].entryID = AS_RESOURCE;
	entries[3].entryLength = pb.hFileInfo.ioFlRLgLen;

	// data fork
	entries[4].entryID = AS_DATA;
	entries[4].entryLength = pb.hFileInfo.ioFlLgLen;
	
	// mac info
	entries[5].entryID = AS_MACINFO;
	entries[5].entryLength = sizeof(ASMacInfo);
	
	for (i=1; i<kNumASEntries; i++) /* start at 1, not 0 */
	{
		entries[i].entryOffset = entries[i-1].entryOffset + entries[i-1].entryLength;
	}
	
	// ERR_CHECK( SetFPos(mTransRefNum, fsFromStart, sizeof(ASHeader)) ); // revisit
	ERR_CHECK( FSWrite(mTransRefNum, &bytes2Wr, &entries[0]) );
	if (bytes2Wr != (kNumASEntries * sizeof(ASEntry)))
		err = -1;
		
	return err;
}

OSErr
nsAppleSingleEncoder::WriteEntries()
{
	OSErr 			err = noErr;
	long			bytes2Wr;
	DateTimeRec		currTime;
	ASFileDates		asDates;
	unsigned long	currSecs;
	ASMacInfo 		asMacInfo;
	int				i;
	CInfoPBRec		pb;
	char 			name[32];
	
	FSpGetCatInfo(&pb, mInFile);
	
	// real name
	bytes2Wr = pb.hFileInfo.ioNamePtr[0];
	strncpy(name, (char*)(pb.hFileInfo.ioNamePtr+1), bytes2Wr);
	ERR_CHECK( FSWrite(mTransRefNum, &bytes2Wr, name) );
	FSpGetCatInfo(&pb, mInFile); // XXX refresh after write
	if (bytes2Wr != pb.hFileInfo.ioNamePtr[0])
	{
		err = -1;
		goto cleanup;
	}
		
	// finder info
	bytes2Wr = sizeof(FInfo);
	ERR_CHECK( FSWrite(mTransRefNum, &bytes2Wr, &pb.hFileInfo.ioFlFndrInfo) );
	FSpGetCatInfo(&pb, mInFile); // XXX refresh after write
	if (bytes2Wr != sizeof(FInfo))
	{
		err = -1;
		goto cleanup;
	}
	
	bytes2Wr = sizeof(FXInfo);
	ERR_CHECK( FSWrite(mTransRefNum, &bytes2Wr, &pb.hFileInfo.ioFlXFndrInfo) );
	FSpGetCatInfo(&pb, mInFile); // XXX refresh after write	
	if (bytes2Wr != sizeof(FXInfo))
	{
		err = -1;
		goto cleanup;
	}
		
	// file dates
	GetTime(&currTime);
	DateToSeconds(&currTime, &currSecs);
	FSpGetCatInfo(&pb, mInFile); // XXX refresh after sys calls
	asDates.create = pb.hFileInfo.ioFlCrDat + kConvertTime;
	asDates.modify = pb.hFileInfo.ioFlMdDat + kConvertTime;
	asDates.backup = pb.hFileInfo.ioFlBkDat + kConvertTime;
	asDates.access = currSecs + kConvertTime;
	bytes2Wr = sizeof(ASFileDates);
	ERR_CHECK( FSWrite(mTransRefNum, &bytes2Wr, &asDates) );
	FSpGetCatInfo(&pb, mInFile); // XXX refresh after write
	if (bytes2Wr != sizeof(ASFileDates))
	{
		err = -1;
		goto cleanup;
	}
		
	// resource fork
	ERR_CHECK( WriteResourceFork() );
	
	// data fork
	ERR_CHECK( WriteDataFork() );
	
	// mac info	
	for (i=0; i<3; i++)
		asMacInfo.filler[i];
	FSpGetCatInfo(&pb, mInFile); // XXX refresh
	asMacInfo.ioFlAttrib = pb.hFileInfo.ioFlAttrib;
	bytes2Wr = sizeof(ASMacInfo);
	ERR_CHECK( FSWrite(mTransRefNum, &bytes2Wr, &asMacInfo) );
	if (bytes2Wr != sizeof(ASMacInfo))
	{
		err = -1;
		goto cleanup;
	}
	
cleanup:
	FSClose(mTransRefNum);
	
	return err;
}

#define BUFSIZE 8192

OSErr
nsAppleSingleEncoder::WriteResourceFork()
{
	OSErr		err = noErr;
	short		refnum;
	long		bytes2Rd, bytes2Wr;
	char 		buf[BUFSIZE];
	
	// open infile's resource fork
	ERR_CHECK( FSpOpenRF(mInFile, fsRdPerm, &refnum) );
	
	while (err != eofErr)
	{
		// keep reading chunks till infile's eof encountered
		bytes2Rd = BUFSIZE;
		err = FSRead(refnum, &bytes2Rd, buf);
		if (bytes2Rd <= 0)
			goto cleanup;
		if (err == noErr || err == eofErr)
		{		
			// keep writing 
			bytes2Wr = bytes2Rd;
			err = FSWrite(mTransRefNum, &bytes2Wr, buf);
			if (err != noErr || bytes2Wr != bytes2Rd)
				goto cleanup;
		}	
	}
		
cleanup:
	// close infile
	FSClose(refnum);
		
	// eof is OK so translate		
	if (err == eofErr)
		err = noErr;

	return err;
}

OSErr
nsAppleSingleEncoder::WriteDataFork()
{
	OSErr		err = noErr;
	short		refnum;
	long		bytes2Rd, bytes2Wr;
	char 		buf[BUFSIZE];
	
	// open infile's data fork
	ERR_CHECK( FSpOpenDF(mInFile, fsRdPerm, &refnum) );
	
	while (err != eofErr)
	{
		// keep reading chunks till infile's eof encountered
		bytes2Rd = BUFSIZE;
		err = FSRead(refnum, &bytes2Rd, buf);
		if (bytes2Rd <= 0)
			goto cleanup;
		if (err == noErr || err == eofErr)
		{		
			// keep writing 
			bytes2Wr = bytes2Rd;
			err = FSWrite(mTransRefNum, &bytes2Wr, buf);
			if (err != noErr || bytes2Wr != bytes2Rd)
				goto cleanup;
		}	
	}
		
cleanup:
	// close infile
	FSClose(refnum);
	
	// eof is OK so translate		
	if (err == eofErr)
		err = noErr;
	
	return err;
}

OSErr
nsAppleSingleEncoder::FSpGetCatInfo(CInfoPBRec *pb, FSSpecPtr aFile)
{
	OSErr err = noErr;
	Str31 name;
	
	nsAppleSingleDecoder::PLstrncpy(name, aFile->name, aFile->name[0]);
	name[0] = aFile->name[0]; // XXX paranoia

	pb->hFileInfo.ioNamePtr = name;
	pb->hFileInfo.ioVRefNum = aFile->vRefNum;
	pb->hFileInfo.ioDirID = aFile->parID;
	pb->hFileInfo.ioFDirIndex = 0; // use ioNamePtr and ioDirID 
	ERR_CHECK( PBGetCatInfoSync(pb) );

	return err;
}
