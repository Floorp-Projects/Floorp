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

#include "MoreFilesX.h"

#ifdef STANDALONE_ASD
#include <iostream>
using namespace std;
#undef MOZILLA_CLIENT
#endif

#ifdef MOZILLA_CLIENT
  #include "nsISupportsUtils.h"
#endif

/*----------------------------------------------------------------------*
 *   Constructors/Destructor
 *----------------------------------------------------------------------*/
#ifdef MOZILLA_CLIENT
  MOZ_DECL_CTOR_COUNTER(nsAppleSingleDecoder)
#endif

nsAppleSingleDecoder::nsAppleSingleDecoder(const FSRef *inRef, FSRef *outRef)
: mInRef(NULL), 
  mOutRef(NULL),
  mInRefNum(0),
  mRenameReqd(false)
{
#ifdef MOZILLA_CLIENT
    MOZ_COUNT_CTOR(nsAppleSingleDecoder);
#endif

  if (inRef && outRef)
  {
    /* merely point to FSRefs, not own 'em */
    mInRef = inRef;
    mOutRef = outRef;
  }
}

nsAppleSingleDecoder::nsAppleSingleDecoder()
: mInRef(NULL), 
  mOutRef(NULL),
  mInRefNum(0),
  mRenameReqd(false)
{
#ifdef MOZILLA_CLIENT
  MOZ_COUNT_CTOR(nsAppleSingleDecoder);
#endif
}

nsAppleSingleDecoder::~nsAppleSingleDecoder()
{
  /* not freeing FSRefs since we don't own 'em */

#ifdef MOZILLA_CLIENT
  MOZ_COUNT_DTOR(nsAppleSingleDecoder);
#endif
}

#pragma mark -

/*----------------------------------------------------------------------*
 *   Public methods
 *----------------------------------------------------------------------*/
OSErr
nsAppleSingleDecoder::Decode()
{
  OSErr         err = noErr;
  ASHeader      header;
  UInt32        bytesRead = sizeof(header);
  HFSUniStr255  nameInRef;
   
  // param check
  if (!mInRef || !mOutRef)
    return paramErr;
     
  // check for existence (and get leaf name for later renaming of decoded file)
  err = FSGetCatalogInfo(mInRef, kFSCatInfoNone, NULL, &nameInRef, NULL, NULL);
  if (err == fnfErr)
    return err;
    
  HFSUniStr255 dataForkName;
  MAC_ERR_CHECK(FSGetDataForkName( &dataForkName ));
  MAC_ERR_CHECK(FSOpenFork( mInRef, dataForkName.length, dataForkName.unicode, 
    fsRdPerm, &mInRefNum ));
  MAC_ERR_CHECK(FSRead( mInRefNum, (long *)&bytesRead, &header ));
  
  if ( (bytesRead != sizeof(header)) ||
     (header.magicNum != APPLESINGLE_MAGIC) ||
     (header.versionNum != APPLESINGLE_VERSION) ||
     (header.numEntries == 0) ) // empty file?
     return -1;
  
  // create the outSpec which we'll rename correctly later
  FSRef parentRef;
  MAC_ERR_CHECK(FSGetParentRef( mInRef, &parentRef ));
  MAC_ERR_CHECK(FSMakeUnique( &parentRef, mOutRef ));
  MAC_ERR_CHECK(FSCreateFork( mOutRef, dataForkName.length, 
    dataForkName.unicode ));
  
  /* Loop through the entries, processing each.
  ** Set the time/date stamps last, because otherwise they'll 
  ** be destroyed  when we write.
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
      MAC_ERR_CHECK(FSRead( mInRefNum, (long *) &bytesRead, &entry ));
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
    // delete encoded version of target file
    MAC_ERR_CHECK(FSDeleteObject( mInRef ));
    MAC_ERR_CHECK(FSRenameUnicode( mOutRef, nameInRef.length, 
      nameInRef.unicode, kTextEncodingUnknown, mOutRef ));
    mRenameReqd = false; 
  }

  return err;
}

OSErr
nsAppleSingleDecoder::Decode(const FSRef *inRef, FSRef *outRef)
{
  OSErr   err = noErr;
  
  // param check
  if (inRef && outRef)
  {
    mInRef = inRef;    // reinit
    mOutRef = outRef;
    mRenameReqd = false; 
  }
  else
    return paramErr;
    
  err = Decode();
  
  return err;
}

Boolean 
DecodeDirIterateFilter(Boolean containerChanged, ItemCount currentLevel,
  const FSCatalogInfo *catalogInfo, const FSRef *ref, 
  const FSSpec *spec, const HFSUniStr255 *name, void *yourDataPtr)
{  
  FSRef                   outRef;
  nsAppleSingleDecoder   *thisObj;
  Boolean                 isDir;
  
  // param check
  if (!yourDataPtr || !ref)
    return false;
    
  // extract 'this' -- an nsAppleSingleDecoder instance
  thisObj = (nsAppleSingleDecoder*) yourDataPtr;
  
  isDir = nsAppleSingleDecoder::IsDirectory(ref);
  
  // if current FSRef is file
  if (!isDir)
  {
    // if file is in AppleSingle format
    if (nsAppleSingleDecoder::IsAppleSingleFile(ref))
    {
      // decode file
      thisObj->Decode(ref, &outRef);
    }
  }

  // else current FSRef is folder 
  else
  {
    thisObj->DecodeFolder(ref);
  }

  return false; // always continue iteration
}

OSErr 
nsAppleSingleDecoder::DecodeFolder(const FSRef *aFolder)
{
  OSErr  err;
  Boolean  isDir = false;
  
  // check that FSSpec is folder
  if (aFolder)
  {
    isDir = IsDirectory((const FSRef *) aFolder);
    if (!isDir)
      return dirNFErr;
  }
  
  // recursively enumerate contents of folder 
  // (maxLevels=0 means recurse all)
  err = FSIterateContainer(aFolder, 0, kFSCatInfoNone, false, 
    false, DecodeDirIterateFilter, (void*)this);
      
  // XXX do we really want to return err?
  return err;
}

Boolean
nsAppleSingleDecoder::IsAppleSingleFile(const FSRef *inRef)
{
  OSErr   err;
  SInt16  inRefNum;
  UInt32  magic;
  long    bytesRead = sizeof(magic);
  
  // param checks
  if (!inRef)
    return false;
    
  // check for existence
  err = FSGetCatalogInfo(inRef, kFSCatInfoNone, NULL, NULL, NULL, NULL);
  if (err!=noErr)
    return false;
  
  // open and read the magic number len bytes  
  HFSUniStr255 dataForkName;
  err = FSGetDataForkName( &dataForkName );
  if (err!=noErr)
    return false;

  err = FSOpenFork( inRef, dataForkName.length, dataForkName.unicode, 
          fsRdPerm, &inRefNum );
  if (err!=noErr)
    return false;

  err = FSRead( inRefNum, &bytesRead, &magic );
  if (err!=noErr)
    return false;
    
  FSClose(inRefNum);
  if (bytesRead != sizeof(magic))
    return false;

  // check if bytes read match magic number  
  return (magic == APPLESINGLE_MAGIC);
}

#pragma mark -

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
    case AS_FILEDATES:
      return ProcessFileDates( inEntry );
      break;
    case AS_FINDERINFO:
      return ProcessFinderInfo( inEntry );
      break;
    case AS_MACINFO:
    case AS_COMMENT:
    case AS_ICONBW:
    case AS_ICONCOLOR:
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
  OSErr  err = noErr;
  SInt16  refNum;
  
  /* Setup the files */
  HFSUniStr255 dataForkName;
  err = FSGetDataForkName( &dataForkName );
  if (err != noErr) 
    return err;

  err = FSOpenFork( mOutRef, dataForkName.length, dataForkName.unicode, 
          fsWrPerm, &refNum );

  if ( err == noErr )
    err = EntryToMacFile( inEntry, refNum );
  
  FSClose( refNum );
  return err;
}

OSErr
nsAppleSingleDecoder::ProcessResourceFork(ASEntry inEntry)
{
  OSErr  err = noErr;
  SInt16  refNum;
    
  HFSUniStr255 rsrcForkName;
  err = FSGetResourceForkName( &rsrcForkName );
  if (err != noErr) 
    return err;

  err = FSOpenFork( mOutRef, rsrcForkName.length, rsrcForkName.unicode, 
          fsWrPerm, &refNum );

  if ( err == noErr )
    err = EntryToMacFile( inEntry, refNum );
  
  FSClose( refNum );
  return err;
}

OSErr
nsAppleSingleDecoder::ProcessRealName(ASEntry inEntry)
{
  OSErr         err = noErr;
  Str255        newName;
  HFSUniStr255  newNameUni;
  UInt32        bytesRead;
  FSRef         parentOfOutRef;
  
  MAC_ERR_CHECK(SetFPos(mInRefNum, fsFromStart, inEntry.entryOffset));
  
  bytesRead = inEntry.entryLength;
  MAC_ERR_CHECK(FSRead(mInRefNum, (long *) &bytesRead, &newName[1]));
  if (bytesRead != inEntry.entryLength)
    return -1;
    
  newName[0] = inEntry.entryLength;
  MAC_ERR_CHECK(FSGetParentRef(mOutRef, &parentOfOutRef));
  MAC_ERR_CHECK(HFSNameGetUnicodeName(newName, kTextEncodingUnknown, 
    &newNameUni));
  err = FSRenameUnicode(mOutRef, newNameUni.length, newNameUni.unicode, 
          kTextEncodingUnknown, mOutRef);
  if (err == dupFNErr)
  {
    HFSUniStr255 inRefName;
    MAC_ERR_CHECK(FSGetCatalogInfo(mInRef, kFSCatInfoNone, NULL, &inRefName, 
      NULL, NULL));

    // if we are trying to rename temp decode file to src name, rename later
    if (nsAppleSingleDecoder::UCstrcmp(&newNameUni, &inRefName))
    {
      mRenameReqd = true;
      return noErr;
    }
    
    // otherwise replace file in the way
    FSRef inTheWayRef;

    // delete file in the way
    MAC_ERR_CHECK(FSMakeFSRefUnicode( &parentOfOutRef, newNameUni.length, 
      newNameUni.unicode, kTextEncodingUnknown, &inTheWayRef ));
    MAC_ERR_CHECK(FSDeleteObject( &inTheWayRef ));

    // rename decoded file to ``newName''
    MAC_ERR_CHECK(FSRenameUnicode( mOutRef, newNameUni.length, 
      newNameUni.unicode, kTextEncodingUnknown, mOutRef ));
  }

  return err;
}

OSErr
nsAppleSingleDecoder::ProcessFileDates(ASEntry inEntry)
{
  OSErr         err = noErr;
  ASFileDates   dates;
  UInt32        bytesRead;
  FSCatalogInfo catInfo;

  if ( inEntry.entryLength != sizeof(dates) )  
    return -1;
  
  MAC_ERR_CHECK(SetFPos(mInRefNum, fsFromStart, inEntry.entryOffset));
  
  bytesRead = inEntry.entryLength;
  MAC_ERR_CHECK(FSRead(mInRefNum, (long *) &bytesRead, &dates));
  if (bytesRead != inEntry.entryLength)
    return -1;
    
#define YR_2000_SECONDS 3029529600
  LocalDateTime local = (LocalDateTime) {0, 0, 0};

  // set creation date
  local.lowSeconds = dates.create + YR_2000_SECONDS;
  ConvertLocalToUTCDateTime(&local, &catInfo.createDate);

  // set content modification date
  local.lowSeconds = dates.modify + YR_2000_SECONDS;
  ConvertLocalToUTCDateTime(&local, &catInfo.contentModDate);

  // set last access date
  local.lowSeconds = dates.access + YR_2000_SECONDS;
  ConvertLocalToUTCDateTime(&local, &catInfo.accessDate);

  // set backup date
  local.lowSeconds = dates.backup + YR_2000_SECONDS;
  ConvertLocalToUTCDateTime(&local, &catInfo.backupDate);

  // set attribute modification date
  GetUTCDateTime(&catInfo.attributeModDate, kUTCDefaultOptions);  

  MAC_ERR_CHECK(FSSetCatalogInfo(mOutRef, 
    kFSCatInfoCreateDate |
    kFSCatInfoContentMod |
    kFSCatInfoAttrMod    |
    kFSCatInfoAccessDate |
    kFSCatInfoBackupDate,
    &catInfo));

  return err;
}

OSErr
nsAppleSingleDecoder::ProcessFinderInfo(ASEntry inEntry)
{
  OSErr         err = noErr;
  ASFinderInfo  info;
  UInt32        bytesRead;
  FSCatalogInfo catInfo;
  
  if (inEntry.entryLength != sizeof( ASFinderInfo ))
    return -1;

  MAC_ERR_CHECK(SetFPos(mInRefNum, fsFromStart, inEntry.entryOffset));

  bytesRead = sizeof(info);
  MAC_ERR_CHECK(FSRead(mInRefNum, (long *) &bytesRead, &info));
  if (bytesRead != inEntry.entryLength)
    return -1;
    
  MAC_ERR_CHECK(FSGetCatalogInfo(mOutRef, kFSCatInfoGettableInfo, &catInfo, 
    NULL, NULL, NULL));

  BlockMoveData((const void *) &info.ioFlFndrInfo, 
    (void *) &catInfo.finderInfo, sizeof(FInfo));
  BlockMoveData((const void *) &info.ioFlXFndrInfo, 
    (void *) &catInfo.extFinderInfo, sizeof(FXInfo));

  MAC_ERR_CHECK(FSSetCatalogInfo(mOutRef, 
    kFSCatInfoFinderInfo |
    kFSCatInfoFinderXInfo,
    &catInfo));
    
  return err;
}

OSErr
nsAppleSingleDecoder::EntryToMacFile(ASEntry inEntry, UInt16 inTargetSpecRefNum)
{
#define BUFFER_SIZE 8192

  OSErr  err = noErr;
  char   buffer[BUFFER_SIZE];
  UInt32 totalRead = 0, bytesRead, bytesToWrite;

  MAC_ERR_CHECK(SetFPos( mInRefNum, fsFromStart, inEntry.entryOffset ));

  while ( totalRead < inEntry.entryLength )
  {
// Should we yield in here?
    bytesRead = BUFFER_SIZE;
    err = FSRead( mInRefNum, (long *) &bytesRead, buffer );
    if (err!=noErr && err!=eofErr)
      return err;
      
    if ( bytesRead <= 0 )
      return -1;
    bytesToWrite = totalRead + bytesRead > inEntry.entryLength ? 
                  inEntry.entryLength - totalRead :
                  bytesRead;

    totalRead += bytesRead;
    MAC_ERR_CHECK(FSWrite(inTargetSpecRefNum, (long *) &bytesToWrite, 
      buffer));
  }  
  
  return err;
}

#pragma mark -

Boolean 
nsAppleSingleDecoder::IsDirectory(const FSRef *inRef)
{
  OSErr err;
  Boolean isDir;

  if (!inRef)
    return false;

  err = FSGetFinderInfo(inRef, NULL, NULL, &isDir);
  if (err != noErr)
    return false;
  
  return isDir;
}

OSErr
nsAppleSingleDecoder::FSMakeUnique(const FSRef *inParentRef, FSRef *outRef)
{
/*
** This function has descended from Apple Sample Code, but we have
** made changes.  Specifically, this function descends from the 
** GenerateUniqueHFSUniStr() function in MoreFilesX.c, version 1.0.1.
*/

  OSErr         result = noErr;
  long          i, startSeed = 0x4a696d4c; /* a fine unlikely filename */
  FSRefParam    pb;
  unsigned char hexStr[17] = "0123456789ABCDEF";
  HFSUniStr255  uniqueName;
  
  /* set up the parameter block */
  pb.name = uniqueName.unicode;
  pb.nameLength = 8;  /* always 8 characters */
  pb.textEncodingHint = kTextEncodingUnknown;
  pb.newRef = outRef;
  pb.ref = inParentRef;

  /* loop until we get fnfErr with a filename in both directories */
  result = noErr;
  while ( fnfErr != result )
  {
    /* convert startSeed to 8 character Unicode string */
    uniqueName.length = 8;
    for ( i = 0; i < 8; ++i )
    {
      uniqueName.unicode[i] = hexStr[((startSeed >> ((7-i)*4)) & 0xf)];
    }
    
    result = PBMakeFSRefUnicodeSync(&pb);
    if ( fnfErr == result )
    {
      OSErr err;
      MAC_ERR_CHECK(FSCreateFileUnicode(inParentRef, uniqueName.length,
        uniqueName.unicode, kFSCatInfoNone, NULL, outRef, NULL));

      return noErr;
    }
    else if ( noErr != result)
    {
      /* exit if anything other than noErr or fnfErr */
      return result;
    }
    
    /* increment seed for next pass through loop */
    ++(startSeed);
  }
  
  return result;
}    

/*----------------------------------------------------------------------*
 *   Utilities
 *----------------------------------------------------------------------*/
Boolean
nsAppleSingleDecoder::UCstrcmp(const HFSUniStr255 *str1, 
                               const HFSUniStr255 *str2)
{
  OSStatus          status;
  Boolean           bEqual; 

  status = UCCompareTextDefault(kUCCollateStandardOptions, str1->unicode, 
                                 str1->length, str2->unicode, str2->length, 
                                 &bEqual, NULL);
  if (status != noErr)
    return false;

  return bEqual;
}

#ifdef STANDALONE_ASD
int
main(int argc, char **argv)
{
  OSErr err;
  FSRef encoded, decoded;
  nsAppleSingleDecoder *decoder = new nsAppleSingleDecoder;
  Boolean isDir;

  if (argc < 2)
  {
    cout << "usage: " << argv[0] << " <file/folder>" << endl
         << "\t\tAppleSingle encoded file path" << endl 
         << "\t\tor" << endl
         << "\t\tfolder path containing AppleSingle encoded files." << endl;
    exit(-1);
  }

  err = FSPathMakeRef((const UInt8 *)argv[1], &encoded, &isDir);
  if (err != noErr)
    return 1;

  // handle AppleSingle encoded files
  if (!isDir)
  {
    Boolean isEncoded = nsAppleSingleDecoder::IsAppleSingleFile(&encoded);
    cout << "IsAppleSingleFile returned: " << (isEncoded ? "true" : "false")
         << endl;

    if (isEncoded)
    {
      err = decoder->Decode(&encoded, &decoded);
      cout << "Decode returned: " << err << endl;
    }
  }

  // handle folders containing AppleSingle encoded files
  else
  {
    err = decoder->DecodeFolder(&encoded);
    cout << "DecodeFolder returned: " << err << endl;
  }

  // verify out file (n/a for folders)
  if (!isDir)
  {
    err = FSGetCatalogInfo(&decoded, kFSCatInfoNone, NULL, NULL, NULL, NULL);
    if (err == noErr)
    {
      cout << "Decoded file appears to exist" << endl;
      char path[1024];
      FSRefMakePath(&decoded, (UInt8 *)path, 1024);
      cout << "Decoded file path: " << path << endl;
    }
    else
      cout << "Decoded file doesn't appear to exist: " << err << endl;
  }

  return 0;
}
#endif /* STANDALONE_ASD */
