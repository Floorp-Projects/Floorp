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


#ifndef _NS_APPLESINGLEDECODER_H_
	#include "nsAppleSingleDecoder.h"
#endif

#include "MoreFilesExtras.h"
#include "MoreDesktopMgr.h"
#include "IterateDirectory.h"
#ifdef MOZILLA_CLIENT
#include "nsISupportsUtils.h"
#endif

/*----------------------------------------------------------------------*
 *   Constructors/Destructor
 *----------------------------------------------------------------------*/
#ifdef MOZILLA_CLIENT
MOZ_DECL_CTOR_COUNTER(nsAppleSingleDecoder)
#endif

nsAppleSingleDecoder::nsAppleSingleDecoder(FSSpec *inSpec, FSSpec *outSpec)
: mInSpec(NULL), 
  mOutSpec(NULL),
  mInRefNum(0),
  mRenameReqd(false)
{
#ifdef MOZILLA_CLIENT
    MOZ_COUNT_CTOR(nsAppleSingleDecoder);
#endif

    if (inSpec && outSpec)
	{
		/* merely point to FSSpecs, not own 'em */
		mInSpec = inSpec;
		mOutSpec = outSpec;
	}
}

nsAppleSingleDecoder::nsAppleSingleDecoder()
: mInSpec(NULL), 
  mOutSpec(NULL),
  mInRefNum(0),
  mRenameReqd(false)
{
#ifdef MOZILLA_CLIENT
    MOZ_COUNT_CTOR(nsAppleSingleDecoder);
#endif
}

nsAppleSingleDecoder::~nsAppleSingleDecoder()
{
	/* not freeing FSSpecs since we don't own 'em */
#ifdef MOZILLA_CLIENT
    MOZ_COUNT_DTOR(nsAppleSingleDecoder);
#endif
}


/*----------------------------------------------------------------------*
 *   Public methods
 *----------------------------------------------------------------------*/
OSErr
nsAppleSingleDecoder::Decode()
{
	OSErr		err = noErr;
 	ASHeader 	header;
 	long		bytesRead = sizeof(header);
 	
 	// param check
 	if (!mInSpec || !mOutSpec)
 		return paramErr;
 		
 	// check for existence	
 	FSSpec tmp;
	err = FSMakeFSSpec(mInSpec->vRefNum, mInSpec->parID, mInSpec->name, &tmp);
	if (err == fnfErr)
		return err;
		
	MAC_ERR_CHECK(FSpOpenDF( mInSpec, fsRdPerm, &mInRefNum ));
	MAC_ERR_CHECK(FSRead( mInRefNum, &bytesRead, &header ));
	
	if ( (bytesRead != sizeof(header)) ||
		 (header.magicNum != 0x00051600) ||
		 (header.versionNum != 0x00020000) ||
		 (header.numEntries == 0) ) // empty file?
		 return -1;
	
	// create the outSpec which we'll rename correctly later
	err = FSMakeFSSpec( mInSpec->vRefNum, mInSpec->parID, "\pdecode", mOutSpec );
	if (err!=noErr && err!=fnfErr)
		return err;
	MAC_ERR_CHECK(FSMakeUnique( mOutSpec ));
	MAC_ERR_CHECK(FSpCreate( mOutSpec, 'MOZZ', '????', 0 ));
	
	
	/* Loop through the entries, processing each.
	** Set the time/date stamps last, because otherwise they'll 
	** be destroyed	when we write.
	*/	
	{
		Boolean hasDateEntry = false;
		ASEntry dateEntry;
		long offset;
		ASEntry entry;
		
		for ( int i=0; i < header.numEntries; i++ )
		{	
			offset = sizeof( ASHeader ) + sizeof( ASEntry ) * i;
			MAC_ERR_CHECK(SetFPos( mInRefNum, fsFromStart, offset ));
			
			bytesRead = sizeof(entry);
			MAC_ERR_CHECK(FSRead( mInRefNum, &bytesRead, &entry ));
			if (bytesRead != sizeof(entry))
				return -1;
				
 			if ( entry.entryID == AS_FILEDATES )
 			{
 				hasDateEntry = true;
 				dateEntry = entry;
 			}
 			else
	 			MAC_ERR_CHECK(ProcessASEntry( entry ));
		}
		if ( hasDateEntry )
			MAC_ERR_CHECK(ProcessASEntry( dateEntry ));
	}
	
	// close the inSpec
	FSClose( mInRefNum );
	
	// rename if need be
	if (mRenameReqd)
	{
		FSSpec old;		// delete old version of target file
		
		FSMakeFSSpec(mInSpec->vRefNum, mInSpec->parID, mInSpec->name, &old);
		MAC_ERR_CHECK(FSpDelete(&old));
		MAC_ERR_CHECK(FSpRename(mOutSpec, mInSpec->name));
		
		// reflect change in outSpec
		nsAppleSingleDecoder::PLstrncpy( mOutSpec->name, mInSpec->name, mInSpec->name[0] );
		mOutSpec->name[0] = mInSpec->name[0];
		mRenameReqd = false; // XXX redundant reinit?
	}
	
 	return err;
}

OSErr
nsAppleSingleDecoder::Decode(FSSpec *inSpec, FSSpec *outSpec)
{
	OSErr 	err = noErr;
	
	// param check
	if (inSpec && outSpec)
	{
		mInSpec = inSpec;		// reinit
		mOutSpec = outSpec;
		mRenameReqd = false; 
	}
	else
		return paramErr;
		
	err = Decode();
	
	return err;
}

pascal void
DecodeDirIterateFilter(const CInfoPBRec * const cpbPtr, Boolean *quitFlag, void *yourDataPtr)
{	
	OSErr					err = noErr;
	FSSpec 					currFSp, outFSp;
	nsAppleSingleDecoder* 	thisObj = NULL;
	Boolean					isDir = false;
	long					dummy;
	
	// param check
	if (!yourDataPtr || !cpbPtr || !quitFlag)
		return;
		
	*quitFlag = false;
	
	// extract 'this' -- an nsAppleSingleDecoder instance
	thisObj = (nsAppleSingleDecoder*) yourDataPtr;
	
	// make an FSSpec from the CInfoPBRec*
	err = FSMakeFSSpec(cpbPtr->hFileInfo.ioVRefNum, cpbPtr->hFileInfo.ioFlParID, 
						cpbPtr->hFileInfo.ioNamePtr, &currFSp);
	if (err == noErr)
	{
		FSpGetDirectoryID(&currFSp, &dummy, &isDir);
		
		// if current FSSpec is file
		if (!isDir)
		{
			// if file is in AppleSingle format
			if (nsAppleSingleDecoder::IsAppleSingleFile(&currFSp))
			{
				// decode file
				thisObj->Decode(&currFSp, &outFSp);
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
nsAppleSingleDecoder::DecodeFolder(FSSpec *aFolder)
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
	FSpIterateDirectory(aFolder, 0, DecodeDirIterateFilter, (void*)this);
			
	return err;
}

Boolean
nsAppleSingleDecoder::IsAppleSingleFile(FSSpec *inSpec)
{
	OSErr	err;
	Boolean bAppleSingle = false;
	short	inRefNum;
	UInt32	magic;
	long 	bytesRead = sizeof(magic);
	
	// param checks
	if (!inSpec)
		return false;
		
	// check for existence
	FSSpec tmp;
	err = FSMakeFSSpec(inSpec->vRefNum, inSpec->parID, inSpec->name, &tmp);
	if (err!=noErr)
		return false;
	
	// open and read the magic number len bytes	
	err = FSpOpenDF( inSpec, fsRdPerm, &inRefNum );
	if (err!=noErr)
		return false;
		
	err = FSRead( inRefNum, &bytesRead, &magic );
	if (err!=noErr)
		return false;
		
	FSClose(inRefNum);
	if (bytesRead != sizeof(magic))
		return false;

	// check if bytes read match magic number	
	bAppleSingle = (magic == 0x00051600);
	return bAppleSingle;
}


/*----------------------------------------------------------------------*
 *   Private methods
 *----------------------------------------------------------------------*/
OSErr
nsAppleSingleDecoder::ProcessASEntry(ASEntry inEntry)
{
	switch (inEntry.entryID)
	{
		case AS_DATA:
			return ProcessDataFork( inEntry );
			break;
		case AS_RESOURCE:
			return ProcessResourceFork( inEntry );
			break;
		case AS_REALNAME:
			ProcessRealName( inEntry );
			break;
			// return 0;	// Ignore these errors in ASD   <--- XXX remove
		case AS_COMMENT:
//			return ProcessComment( inEntry );
			break;
		case AS_ICONBW:
//			return ProcessIconBW( inEntry );
			break;
		case AS_ICONCOLOR:
//			return ProcessIconColor( inEntry );
			break;
		case AS_FILEDATES:
			return ProcessFileDates( inEntry );
			break;
		case AS_FINDERINFO:
			return ProcessFinderInfo( inEntry );
			break;
		case AS_MACINFO:
			return ProcessMacInfo( inEntry );
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

OSErr
nsAppleSingleDecoder::ProcessDataFork(ASEntry inEntry)
{
	OSErr	err = noErr;
	SInt16	refNum;
	
	/* Setup the files */
	err = FSpOpenDF (mOutSpec, fsWrPerm,  &refNum);

	if ( err == noErr )
		err = EntryToMacFile( inEntry, refNum );
	
	FSClose( refNum );
	return err;
}

OSErr
nsAppleSingleDecoder::ProcessResourceFork(ASEntry inEntry)
{
	OSErr	err = noErr;
	SInt16	refNum;
		
	err = FSpOpenRF(mOutSpec, fsWrPerm,  &refNum);

	if ( err == noErr )
		err = EntryToMacFile( inEntry, refNum );
	
	FSClose( refNum );
	return err;
}

OSErr
nsAppleSingleDecoder::ProcessRealName(ASEntry inEntry)
{
	OSErr	err = noErr;
	Str255	newName;
	long	bytesRead;
	
	if ( inEntry.entryLength > 32 )	/* Max file name length for the Mac */
		return -1;

	MAC_ERR_CHECK(SetFPos(mInRefNum, fsFromStart, inEntry.entryOffset));
	
	bytesRead = inEntry.entryLength;
	MAC_ERR_CHECK(FSRead(mInRefNum, &bytesRead, &newName[1]));
	if (bytesRead != inEntry.entryLength)
		return -1;
		
	newName[0] = inEntry.entryLength;
	err = FSpRename(mOutSpec, newName);
	if (err == dupFNErr)
	{
		// if we are trying to rename temp decode file to src name, rename later
		if (nsAppleSingleDecoder::PLstrcmp(newName, mInSpec->name))
		{
			mRenameReqd = true;
			return noErr;
		}
		
		FSSpec old;		// delete old version of target file
		
		FSMakeFSSpec(mOutSpec->vRefNum, mOutSpec->parID, newName, &old);
		MAC_ERR_CHECK(FSpDelete(&old));
		MAC_ERR_CHECK(FSpRename(mOutSpec, newName));
	}
	nsAppleSingleDecoder::PLstrncpy( mOutSpec->name, newName, inEntry.entryLength );
	mOutSpec->name[0] = inEntry.entryLength;

	return err;
}

OSErr
nsAppleSingleDecoder::ProcessFileDates(ASEntry inEntry)
{
	OSErr		err = noErr;
	ASFileDates dates;
	long		bytesRead;
	
	if ( inEntry.entryLength != sizeof(dates) )	/* Max file name length for the Mac */
		return -1;
	
	MAC_ERR_CHECK(SetFPos(mInRefNum, fsFromStart, inEntry.entryOffset));
	
	bytesRead = inEntry.entryLength;
	MAC_ERR_CHECK(FSRead(mInRefNum, &bytesRead, &dates));
	if (bytesRead != inEntry.entryLength)
		return -1;
		
	Str31 name;
	nsAppleSingleDecoder::PLstrncpy(name, mOutSpec->name, mOutSpec->name[0]);
	name[0] = mOutSpec->name[0];
	CInfoPBRec pb;
	pb.hFileInfo.ioNamePtr = &name[0];
	pb.hFileInfo.ioVRefNum = mOutSpec->vRefNum;
	pb.hFileInfo.ioDirID = mOutSpec->parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	err = PBGetCatInfoSync(&pb);
	if ( err != noErr )
		return -1;
#define YR_2000_SECONDS 3029529600
	pb.hFileInfo.ioFlCrDat = dates.create + YR_2000_SECONDS;
	pb.hFileInfo.ioFlMdDat = dates.modify + YR_2000_SECONDS;
	pb.hFileInfo.ioFlBkDat = dates.backup + YR_2000_SECONDS;
	/* Not sure if mac has the last access time */
	
	nsAppleSingleDecoder::PLstrncpy(name, mOutSpec->name, mOutSpec->name[0]);
	name[0] = mOutSpec->name[0];
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = mOutSpec->vRefNum;
	pb.hFileInfo.ioDirID = mOutSpec->parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	err = PBSetCatInfo(&pb, false);
	
	return err;
}

OSErr
nsAppleSingleDecoder::ProcessFinderInfo(ASEntry inEntry)
{
	OSErr			err = noErr;
	ASFinderInfo 	info;
	long			bytesRead;
	
	if (inEntry.entryLength != sizeof( ASFinderInfo ))
		return -1;

	MAC_ERR_CHECK(SetFPos(mInRefNum, fsFromStart, inEntry.entryOffset));

	bytesRead = sizeof(info);
	MAC_ERR_CHECK(FSRead(mInRefNum, &bytesRead, &info));
	if (bytesRead != inEntry.entryLength)
		return -1;
		
	err = FSpSetFInfo(mOutSpec, &info.ioFlFndrInfo);
	if (err!=noErr && err!=fnfErr)
		return err;

	Str31 name;
	nsAppleSingleDecoder::PLstrncpy(name, mOutSpec->name, mOutSpec->name[0]);
	name[0] = mOutSpec->name[0];
	CInfoPBRec pb;
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = mOutSpec->vRefNum;
	pb.hFileInfo.ioDirID = mOutSpec->parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	MAC_ERR_CHECK(PBGetCatInfoSync(&pb));
	
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = mOutSpec->vRefNum;
	pb.hFileInfo.ioDirID = mOutSpec->parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	pb.hFileInfo.ioFlXFndrInfo = info.ioFlXFndrInfo;
	err = PBSetCatInfo(&pb, false);
    
    

    if (info.ioFlFndrInfo.fdType == 'APPL')
    {
        // need to register in desktop database or bad things will happen

        DTSetAPPL(  NULL, 
                    mOutSpec->vRefNum,
                    info.ioFlFndrInfo.fdCreator,
                    mOutSpec->parID,
                    mOutSpec->name );

    }


	return err;
}

OSErr
nsAppleSingleDecoder::ProcessMacInfo(ASEntry inEntry)
{
	OSErr		err = noErr;
	ASMacInfo 	info;
	long 		bytesRead;
	
	if (inEntry.entryLength != sizeof( ASMacInfo ))
		return -1;

	MAC_ERR_CHECK(SetFPos(mInRefNum, fsFromStart, inEntry.entryOffset));

	bytesRead = sizeof(info);
	MAC_ERR_CHECK(FSRead(mInRefNum, &bytesRead, &info));
	if (bytesRead != inEntry.entryLength)
		return -1;
	
	Str31 name;
	nsAppleSingleDecoder::PLstrncpy(name, mOutSpec->name, mOutSpec->name[0]);
	name[0] = mOutSpec->name[0];
	CInfoPBRec pb;
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = mOutSpec->vRefNum;
	pb.hFileInfo.ioDirID = mOutSpec->parID;
	pb.hFileInfo.ioFDirIndex = 0;	/* use ioNamePtr and ioDirID */
	MAC_ERR_CHECK(PBGetCatInfoSync(&pb));
	
	pb.hFileInfo.ioNamePtr = name;
	pb.hFileInfo.ioVRefNum = mOutSpec->vRefNum;
	pb.hFileInfo.ioDirID = mOutSpec->parID;
	pb.hFileInfo.ioFlAttrib = info.ioFlAttrib;
	err = PBSetCatInfo(&pb, false);
	
	return err;
}

OSErr
nsAppleSingleDecoder::EntryToMacFile(ASEntry inEntry, UInt16 inTargetSpecRefNum)
{
#define BUFFER_SIZE 8192

	OSErr	err = noErr;
	char 	buffer[BUFFER_SIZE];
	long 	totalRead = 0, bytesRead, bytesToWrite;

	MAC_ERR_CHECK(SetFPos( mInRefNum, fsFromStart, inEntry.entryOffset ));

	while ( totalRead < inEntry.entryLength )
	{
// Should we yield in here?
		bytesRead = BUFFER_SIZE;
		err = FSRead( mInRefNum, &bytesRead, buffer );
		if (err!=noErr && err!=eofErr)
			return err;
			
		if ( bytesRead <= 0 )
			return -1;
		bytesToWrite = totalRead + bytesRead > inEntry.entryLength ? 
									inEntry.entryLength - totalRead :
									bytesRead;

		totalRead += bytesRead;
		MAC_ERR_CHECK(FSWrite(inTargetSpecRefNum, &bytesToWrite, buffer));
	}	
	
	return err;
}

OSErr DTSetAPPL(Str255 volName,
                short vRefNum,
                OSType creator,
                long applParID,
                Str255 applName)
{
    OSErr err;
    DTPBRec *pb = NULL;
    short dtRefNum;
    short realVRefNum;
    Boolean newDTDatabase;
    /* get the real vRefnum */
    err = DetermineVRefNum(volName, vRefNum, &realVRefNum);
    if (err == noErr)
    {
        err = DTOpen(volName, vRefNum, &dtRefNum, &newDTDatabase);
        if (err == noErr && !newDTDatabase)
        {
            pb = (DTPBRec*) NewPtrClear( sizeof(DTPBRec) );
            
            if (pb==NULL) return -1;

            pb->ioNamePtr      =   applName;
            pb->ioDTRefNum     =   dtRefNum;
            pb->ioDirID        =   applParID;
            pb->ioFileCreator  =   creator;

            err = PBDTAddAPPLSync(pb);
            
            if (pb) DisposePtr((Ptr)pb);
        }        
    }
    return err;
}

OSErr
nsAppleSingleDecoder::FSMakeUnique(FSSpec *ioSpec)
{
	OSErr			err = noErr;
	Boolean 		bUnique = false;
	FSSpec			tmp;
	long				uniqueID = 0;
	Str255			name;
	short			i, j;
	unsigned char	puniqueID[16];
	char			*cuniqueIDPtr;

	// grab suggested name from in-out FSSpec
	nsAppleSingleDecoder::PLstrncpy(name, ioSpec->name, ioSpec->name[0]);
	name[0] = ioSpec->name[0];
	
	for (i=0; i<65536; i++)		// prevent infinite loop
	{	
		if (!bUnique)
		{
			err = FSMakeFSSpec( ioSpec->vRefNum, ioSpec->parID, name, &tmp );
			if (err == fnfErr)
			{
				bUnique = true;
				break;
			}
			else if (err == noErr)	// file already exists
			{
				// grab suggested name from in-out FSSpec
				nsAppleSingleDecoder::PLstrncpy(name, ioSpec->name, ioSpec->name[0]);
				name[0] = ioSpec->name[0];
				
				// attempt to create a new unique file name
				nsAppleSingleDecoder::PLstrncat( name, "\p-", 1 );
				
				// tack on digit(s)
				cuniqueIDPtr = nsAppleSingleDecoder::ltoa(uniqueID++);
				puniqueID[0] = strlen(cuniqueIDPtr);
				for (j=0; j<strlen(cuniqueIDPtr); j++)
				{
					puniqueID[j+1] = cuniqueIDPtr[j];
				}
				
				nsAppleSingleDecoder::PLstrncat( name, puniqueID, puniqueID[0] );
				DisposePtr((Ptr)cuniqueIDPtr);
			}
			else
				return err;
		}
	}
	
	// put back unique name into in-out FSSpec
	nsAppleSingleDecoder::PLstrncpy(ioSpec->name, name, name[0]);
	ioSpec->name[0] = name[0];
	
	return noErr;
}		

/*----------------------------------------------------------------------*
 *   Utilities
 *----------------------------------------------------------------------*/
char *
nsAppleSingleDecoder::ltoa(long n)
{
	char *s;
	int i, j, sign, tmp;
	
	/* check sign and convert to positive to stringify numbers */
	if ( (sign = n) < 0)
		n = -n;
	i = 0;
	s = (char*) malloc(sizeof(char));
	
	/* grow string as needed to add numbers from powers of 10 down till none left */
	do
	{
		s = (char*) realloc(s, (i+1)*sizeof(char));
		s[i++] = n % 10 + '0';  /* '0' or 30 is where ASCII numbers start from */
		s[i] = '\0';
	}
	while( (n /= 10) > 0);	
	
	/* tack on minus sign if we found earlier that this was negative */
	if (sign < 0)
	{
		s = (char*) realloc(s, (i+1)*sizeof(char));
		s[i++] = '-';
	}
	s[i] = '\0';
	
	/* pop numbers (and sign) off of string to push back into right direction */
	for (i = 0, j = strlen(s) - 1; i < j; i++, j--)
	{
		tmp = s[i];
		s[i] = s[j];
		s[j] = tmp;
	}
	
	return s;
}

StringPtr 	
nsAppleSingleDecoder::PLstrncpy(StringPtr dst, ConstStr255Param src, short max)
{
	int srcLen = src[0];
	if (srcLen > max)
		srcLen = max;
	dst[0] = srcLen;	
	memcpy(&dst[1], &src[1], srcLen);
	return dst;	
}

StringPtr 	
nsAppleSingleDecoder::PLstrncat(StringPtr dst, ConstStr255Param src, short max)
{
	int srcLen = src[0], dstLen = dst[0];
	if (srcLen > max)
		srcLen = max;
	dst[0] += srcLen;
	memcpy(&dst[dstLen+1], &src[1], srcLen);
	return dst;
}

Boolean
nsAppleSingleDecoder::PLstrcmp(StringPtr str1, StringPtr str2)
{
	Boolean	bEqual = true;
	
	// check for same length
	if (str1[0] == str2[0])
	{
		// verify mem blocks match
		if (0 != memcmp(&str1[1], &str2[1], str1[0]))
			bEqual = false;
	}
	else
		bEqual = false;
		
	return bEqual;
}
