/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Steve Dagley <sdagley@netscape.com>
 *   John R. McMullen
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


#include "nsCOMPtr.h"
#include "nsMemory.h"
#include "nsXPIDLString.h" 

#include "nsLocalFile.h"
#include "nsNativeCharsetUtils.h"
#include "nsISimpleEnumerator.h"
#include "nsIComponentManager.h"
#include "nsIInternetConfigService.h"
#include "nsIMIMEInfo.h"
#include "prtypes.h"
#include "prerror.h"

#include "nsReadableUtils.h"
#include "nsITimelineService.h"

#ifdef XP_MACOSX
#include "nsXPIDLString.h"

#include "private/pprio.h"
#else
#include "pprio.h" // Include this rather than prio.h so we get def of PR_ImportFile
#endif
#include "prmem.h"
#include "plbase64.h"

#include "FullPath.h"
#include "FileCopy.h"
#include "MoreFilesExtras.h"
#include "DirectoryCopy.h"
#include <Script.h>
#include <Processes.h>
#include <StringCompare.h>
#include <Resources.h>

#include <AppleEvents.h>
#include <AEDataModel.h>
#include <AERegistry.h>
#include <Gestalt.h>

#include <Math64.h>
#include <Aliases.h>
#include <Folders.h>
#include <Gestalt.h>
#include "macDirectoryCopy.h"

#include <limits.h>

// Stupid @#$% header looks like its got extern mojo but it doesn't really
extern "C"
{
#ifndef XP_MACOSX
// BADPINK - this MSL header doesn't exist under macosx :-(
#include <FSp_fopen.h>
#endif
}

#if TARGET_CARBON
#include <CodeFragments.h>  // Needed for definition of kUnresolvedCFragSymbolAddress
#include <LaunchServices.h>
#endif

#pragma mark [Constants]

const OSType kDefaultCreator = 'MOSS';

#pragma mark -
#pragma mark [nsPathParser]

class nsPathParser
{
public:
    nsPathParser(const nsACString &path);
    
    ~nsPathParser()
    {
        if (mAllocatedBuffer)
            nsMemory::Free(mAllocatedBuffer);
    }
    
    const char* First()
    {
        return nsCRT::strtok(mBuffer, ":", &mNewString);
    }
    const char* Next()
    {
        return nsCRT::strtok(mNewString, ":", &mNewString);
    }
    const char* Remainder()
    {
        return mNewString;
    }

private:    
    char        mAutoBuffer[512];
    char        *mAllocatedBuffer;
    char        *mBuffer, *mNewString;
};

nsPathParser::nsPathParser(const nsACString &inPath) :
    mAllocatedBuffer(nsnull), mNewString(nsnull)
{
    PRUint32 inPathLen = inPath.Length();
    if (inPathLen >= sizeof(mAutoBuffer)) {
        mAllocatedBuffer = (char *)nsMemory::Alloc(inPathLen + 1);
        mBuffer = mAllocatedBuffer;
    }
    else
    	mBuffer = mAutoBuffer;
    
    // copy inPath into mBuffer	
    nsACString::const_iterator start, end;
    inPath.BeginReading(start);
    inPath.EndReading(end);
	
	PRUint32 size, offset = 0;
    for ( ; start != end; start.advance(size)) {
        const char* buf = start.get();
        size = start.size_forward();
        memcpy(mBuffer + offset, buf, size);
        offset += size;
    }
    mBuffer[offset] = '\0';
}

#pragma mark -
#pragma mark [static util funcs]

static inline void ClearFSSpec(FSSpec& aSpec)
{
    aSpec.vRefNum = 0;
    aSpec.parID = 0;
    aSpec.name[0] = 0;
}


// Simple func to map Mac OS errors into nsresults
static nsresult MacErrorMapper(OSErr inErr)
{
    nsresult outErr;
    
    switch (inErr)
    {
        case noErr:
            outErr = NS_OK;
            break;

        case fnfErr:
            outErr = NS_ERROR_FILE_NOT_FOUND;
            break;

        case dupFNErr:
            outErr = NS_ERROR_FILE_ALREADY_EXISTS;
            break;
        
        case dskFulErr:
            outErr = NS_ERROR_FILE_DISK_FULL;
            break;
        
        case fLckdErr:
            outErr = NS_ERROR_FILE_IS_LOCKED;
            break;
        
        // Can't find good map for some
        case bdNamErr:
            outErr = NS_ERROR_FAILURE;
            break;

        default:    
            outErr = NS_ERROR_FAILURE;
            break;
    }
    return outErr;
}



/*----------------------------------------------------------------------------
    IsEqualFSSpec 
    
    Compare two canonical FSSpec records.
            
    Entry:  file1 = pointer to first FSSpec record.
            file2 = pointer to second FSSpec record.
    
    Exit:   function result = true if the FSSpec records are equal.
----------------------------------------------------------------------------*/

static PRBool IsEqualFSSpec(const FSSpec& file1, const FSSpec& file2)
{
    return
        file1.vRefNum == file2.vRefNum &&
        file1.parID == file2.parID &&
        EqualString(file1.name, file2.name, false, true);
}


/*----------------------------------------------------------------------------
    GetParentFolderSpec
    
    Given an FSSpec to a (possibly non-existent) file, get an FSSpec for its
    parent directory.
    
----------------------------------------------------------------------------*/

static OSErr GetParentFolderSpec(const FSSpec& fileSpec, FSSpec& parentDirSpec)
{
    CInfoPBRec  pBlock = {0};
    OSErr       err = noErr;
    
    parentDirSpec.name[0] = 0;
    
    pBlock.dirInfo.ioVRefNum = fileSpec.vRefNum;
    pBlock.dirInfo.ioDrDirID = fileSpec.parID;
    pBlock.dirInfo.ioNamePtr = (StringPtr)parentDirSpec.name;
    pBlock.dirInfo.ioFDirIndex = -1;        //get info on parID
    err = PBGetCatInfoSync(&pBlock);
    if (err != noErr) return err;
    
    parentDirSpec.vRefNum = fileSpec.vRefNum;
    parentDirSpec.parID = pBlock.dirInfo.ioDrParID;
    
    return err;
}


/*----------------------------------------------------------------------------
    VolHasDesktopDB 
    
    Check to see if a volume supports the new desktop database.
    
    Entry:  vRefNum = vol ref num of volumn
            
    Exit:   function result = error code.
            *hasDesktop = true if volume has the new desktop database.
----------------------------------------------------------------------------*/

static OSErr VolHasDesktopDB (short vRefNum, Boolean *hasDesktop)
{
    HParamBlockRec      pb;
    GetVolParmsInfoBuffer   info;
    OSErr               err = noErr;
    
    pb.ioParam.ioCompletion = nil;
    pb.ioParam.ioNamePtr = nil;
    pb.ioParam.ioVRefNum = vRefNum;
    pb.ioParam.ioBuffer = (Ptr)&info;
    pb.ioParam.ioReqCount = sizeof(info);
    err = PBHGetVolParmsSync(&pb);
    *hasDesktop = err == noErr && (info.vMAttrib & (1L << bHasDesktopMgr)) != 0;
    return err;
}


/*----------------------------------------------------------------------------
    GetLastModDateTime
    
    Get the last mod date and time of a file.
    
    Entry:  fSpec = pointer to file spec.
    
    Exit:   function result = error code.
            *lastModDateTime = last mod date and time.
----------------------------------------------------------------------------*/

static OSErr GetLastModDateTime(const FSSpec *fSpec, unsigned long *lastModDateTime)
{
    CInfoPBRec  pBlock;
    OSErr       err = noErr;
    
    pBlock.hFileInfo.ioNamePtr = (StringPtr)fSpec->name;
    pBlock.hFileInfo.ioVRefNum = fSpec->vRefNum;
    pBlock.hFileInfo.ioFDirIndex = 0;
    pBlock.hFileInfo.ioDirID = fSpec->parID;
    err = PBGetCatInfoSync(&pBlock);
    if (err != noErr) return err;
    *lastModDateTime = pBlock.hFileInfo.ioFlMdDat;
    return noErr;
}


/*----------------------------------------------------------------------------
    FindAppOnVolume 
    
    Find an application on a volume.
    
    Entry:  sig = application signature.
            vRefNum = vol ref num
            
    Exit:   function result = error code
                = afpItemNotFound if app not found on vol.
            *file = file spec for application on volume.
----------------------------------------------------------------------------*/

static OSErr FindAppOnVolume (OSType sig, short vRefNum, FSSpec *file)
{
    DTPBRec     pb;
    OSErr       err = noErr;
    short       ioDTRefNum, i;
    FInfo       fInfo;
    FSSpec      candidate;
    unsigned long lastModDateTime, maxLastModDateTime;

    memset(&pb, 0, sizeof(DTPBRec));
    pb.ioCompletion = nil;
    pb.ioVRefNum = vRefNum;
    pb.ioNamePtr = nil;
    err = PBDTGetPath(&pb);
    if (err != noErr) return err;
    ioDTRefNum = pb.ioDTRefNum;

    memset(&pb, 0, sizeof(DTPBRec));
    pb.ioCompletion = nil;
    pb.ioIndex = 0;
    pb.ioFileCreator = sig;
    pb.ioNamePtr = file->name;
    pb.ioDTRefNum = ioDTRefNum;
    err = PBDTGetAPPLSync(&pb);
    
    if (err == fnfErr || err == paramErr) return afpItemNotFound;
    if (err != noErr) return err;

    file->vRefNum = vRefNum;
    file->parID = pb.ioAPPLParID;
    
    err = FSpGetFInfo(file, &fInfo);
    if (err == noErr) return noErr;
    
    i = 1;
    maxLastModDateTime = 0;
    while (true)
    {
        memset(&pb, 0, sizeof(DTPBRec)); 
        pb.ioCompletion = nil;
        pb.ioIndex = i;
        pb.ioFileCreator = sig;
        pb.ioNamePtr = candidate.name;
        pb.ioDTRefNum = ioDTRefNum;
        err = PBDTGetAPPLSync(&pb);
        if (err != noErr) break;
        candidate.vRefNum = vRefNum;
        candidate.parID = pb.ioAPPLParID;
        err = GetLastModDateTime(file, &lastModDateTime);
        if (err == noErr) {
            if (lastModDateTime > maxLastModDateTime) {
                maxLastModDateTime = lastModDateTime;
                *file = candidate;
            }
        }
        i++;
    }
    
    return maxLastModDateTime > 0 ? noErr : afpItemNotFound;
}


/*----------------------------------------------------------------------------
    GetIndVolume 
    
    Get a volume reference number by volume index.
    
    Entry:  index = volume index
            
    Exit:   function result = error code.
            *vRefNum = vol ref num of indexed volume.
----------------------------------------------------------------------------*/

static OSErr GetIndVolume(short index, short *vRefNum)
{
    HParamBlockRec pb;
    Str63 volumeName;
    OSErr err = noErr;
    
    pb.volumeParam.ioCompletion = nil;
    pb.volumeParam.ioNamePtr = volumeName;
    pb.volumeParam.ioVolIndex = index;
    
    err = PBHGetVInfoSync(&pb);
    
    *vRefNum = pb.volumeParam.ioVRefNum;
    return err;
}


// Private NSPR functions
static unsigned long gJanuaryFirst1970Seconds = 0;
/*
 * The geographic location and time zone information of a Mac
 * are stored in extended parameter RAM.  The ReadLocation
 * produdure uses the geographic location record, MachineLocation,
 * to read the geographic location and time zone information in
 * extended parameter RAM.
 *
 * Because serial port and SLIP conflict with ReadXPram calls,
 * we cache the call here.
 *
 * Caveat: this caching will give the wrong result if a session
 * extend across the DST changeover time.
 */

static void MyReadLocation(MachineLocation *loc)
{
    static MachineLocation storedLoc;
    static Boolean didReadLocation = false;
    
    if (!didReadLocation) {
        ReadLocation(&storedLoc);
        didReadLocation = true;
    }
    *loc = storedLoc;
}

static long GMTDelta(void)
{
    MachineLocation loc;
    long gmtDelta;

    MyReadLocation(&loc);
    gmtDelta = loc.u.gmtDelta & 0x00ffffff;
    if (gmtDelta & 0x00800000) {    /* test sign extend bit */
        gmtDelta |= 0xff000000;
    }
    return gmtDelta;
}

static void MacintoshInitializeTime(void)
{
    /*
     * The NSPR epoch is midnight, Jan. 1, 1970 GMT.
     *
     * At midnight Jan. 1, 1970 GMT, the local time was
     *     midnight Jan. 1, 1970 + GMTDelta().
     *
     * Midnight Jan. 1, 1970 is 86400 * (365 * (1970 - 1904) + 17)
     *     = 2082844800 seconds since the Mac epoch.
     * (There were 17 leap years from 1904 to 1970.)
     *
     * So the NSPR epoch is 2082844800 + GMTDelta() seconds since
     * the Mac epoch.  Whew! :-)
     */
    gJanuaryFirst1970Seconds = 2082844800 + GMTDelta();
}

static nsresult ConvertMacTimeToMilliseconds( PRInt64* aLastModifiedTime, PRUint32 timestamp )
{
    if ( gJanuaryFirst1970Seconds == 0)
        MacintoshInitializeTime();
    timestamp -= gJanuaryFirst1970Seconds;
    PRTime usecPerSec, dateInMicroSeconds;
    LL_I2L(dateInMicroSeconds, timestamp);
    LL_I2L(usecPerSec, PR_MSEC_PER_SEC);
    LL_MUL(*aLastModifiedTime, usecPerSec, dateInMicroSeconds);
    return NS_OK;
}

static nsresult ConvertMillisecondsToMacTime(PRInt64 aTime, PRUint32 *aOutMacTime)
{
    NS_ENSURE_ARG( aOutMacTime );
        
    PRTime usecPerSec, dateInSeconds;
    dateInSeconds = LL_ZERO;
    
    LL_I2L(usecPerSec, PR_MSEC_PER_SEC);
    LL_DIV(dateInSeconds, aTime, usecPerSec); // dateInSeconds = aTime/1,000
    LL_L2UI(*aOutMacTime, dateInSeconds);
    *aOutMacTime += 2082844800; // date + Mac epoch

    return NS_OK;
}

static void myPLstrcpy(Str255 dst, const char* src)
{
    int srcLength = strlen(src);
    NS_ASSERTION(srcLength <= 255, "Oops, Str255 can't hold >255 chars");
    if (srcLength > 255)
        srcLength = 255;
    dst[0] = srcLength;
    memcpy(&dst[1], src, srcLength);
}

static void myPLstrncpy(Str255 dst, const char* src, int inMax)
{
    int srcLength = strlen(src);
    if (srcLength > inMax)
        srcLength = inMax;
    dst[0] = srcLength;
    memcpy(&dst[1], src, srcLength);
}

/*
    NS_TruncNodeName
    
    Utility routine to do a mid-trunc on a potential file name so that it is
    no longer than 31 characters.  Until we move to the HFS+ APIs we need this
    to come up with legal Mac file names.

    Entry:  aNode = initial file name
            outBuf = scratch buffer for the truncated name (MUST be >= 32 characters)

    Exit:   function result = pointer to truncated name.  Will be either aNode or outBuf.

*/
const char* NS_TruncNodeName(const char *aNode, char *outBuf)
{
    PRUint32 nodeLen;
    if ((nodeLen = strlen(aNode)) > 31)
    {
        static PRBool sInitialized = PR_FALSE;
        static CharByteTable sTable;
        // Init to "..." in case we fail to get the ellipsis token
        static char sEllipsisTokenStr[4] = { '.', '.', '.', 0 };
        static PRUint8 sEllipsisTokenLen = 3;
                
        if (!sInitialized)
        {
            // Entries in the table are:
            // 0 == 1 byte char
            // 1 == 2 byte char
            FillParseTable(sTable, smSystemScript);
            
            Handle itl4ResHandle = nsnull;
            long offset, len;
            ::GetIntlResourceTable(smSystemScript, smUnTokenTable, &itl4ResHandle, &offset, &len);
            if (itl4ResHandle)
            {
                UntokenTable *untokenTableRec = (UntokenTable *)(*itl4ResHandle + offset);
                if (untokenTableRec->lastToken >= tokenEllipsis)
                {
                    offset += untokenTableRec->index[tokenEllipsis];
                    char *tokenStr = (*itl4ResHandle + offset);
                    sEllipsisTokenLen = tokenStr[0];
                    memcpy(sEllipsisTokenStr, &tokenStr[1], sEllipsisTokenLen);
                }
                ::ReleaseResource(itl4ResHandle);
            }
            sInitialized = PR_TRUE;
        }

        PRInt32 halfLen = (31 - sEllipsisTokenLen) / 2;
        PRInt32 charSize = 0, srcPos, destPos;
        for (srcPos = 0; srcPos + charSize <= halfLen; srcPos += charSize)
            charSize = sTable[aNode[srcPos]] ? 2 : 1;
                    
        memcpy(outBuf, aNode, srcPos);
        memcpy(outBuf + srcPos, sEllipsisTokenStr, sEllipsisTokenLen);
        destPos = srcPos + sEllipsisTokenLen;
        
        for (; srcPos < nodeLen - halfLen; srcPos += charSize)
            charSize = sTable[aNode[srcPos]] ? 2 : 1;
            
        memcpy(outBuf + destPos, aNode + srcPos, nodeLen - srcPos);
        destPos += (nodeLen - srcPos);
        outBuf[destPos] = '\0';
        return outBuf;
    }
    return aNode;
}

/**
 *  HFSPlusGetRawPath returns the path for an FSSpec as a unicode string.
 *
 *  The reason for this routine instead of just calling FSRefMakePath is
 *  (1) inSpec does not have to exist
 *  (2) FSRefMakePath uses '/' as the separator under OSX and ':' under OS9
 */
static OSErr HFSPlusGetRawPath(const FSSpec& inSpec, nsAString& outStr)
{
    OSErr err;
    nsAutoString ucPathString;
    
    outStr.Truncate(0);
    
    FSRef nodeRef;
    FSCatalogInfo catalogInfo;
    catalogInfo.parentDirID = 0;
    err = ::FSpMakeFSRef(&inSpec, &nodeRef);
    
    if (err == fnfErr) {
        FSSpec parentDirSpec;               
        err = GetParentFolderSpec(inSpec, parentDirSpec);
        if (err == noErr) {
            const char *startPtr = (const char*)&inSpec.name[1];
            NS_CopyNativeToUnicode(Substring(startPtr, startPtr + PRUint32(inSpec.name[0])), outStr);
            err = ::FSpMakeFSRef(&parentDirSpec, &nodeRef);
        }
    }

    while (err == noErr && catalogInfo.parentDirID != fsRtParID) {
        HFSUniStr255 nodeName;
        FSRef parentRef;
        err = ::FSGetCatalogInfo(&nodeRef,
                                 kFSCatInfoNodeFlags + kFSCatInfoParentDirID,
                                 &catalogInfo,
                                 &nodeName,
                                 nsnull,
                                 &parentRef);
        if (err == noErr)
        {
            if (catalogInfo.nodeFlags & kFSNodeIsDirectoryMask)
                nodeName.unicode[nodeName.length++] = PRUnichar(':');
            const PRUnichar* nodeNameUni = (const PRUnichar*) nodeName.unicode;
            outStr.Insert(Substring(nodeNameUni, nodeNameUni + nodeName.length), 0);
            nodeRef = parentRef;
        }
    }
    return err;
}


// The R**co FSSpec resolver -
//  it slices, it dices, it juliannes fries and it even creates FSSpecs out of whatever you feed it
// This function will take a path and a starting FSSpec and generate a FSSpec to represent
// the target of the two.  If the intial FSSpec is null the path alone will be resolved
static OSErr ResolvePathAndSpec(const char * filePath, FSSpec *inSpec, PRBool createDirs, FSSpec *outSpec)
{
    OSErr   err = noErr;
    size_t  inLength = strlen(filePath);
    Boolean isRelative = (filePath && inSpec);
    FSSpec  tempSpec;
    Str255  ppath;
    Boolean isDirectory;
    
    if (isRelative && inSpec)
    {
        outSpec->vRefNum = inSpec->vRefNum;
        outSpec->parID = inSpec->parID;
        
        if (inSpec->name[0] != 0)
        {
            long theDirID;
            
            err = FSpGetDirectoryID(inSpec, &theDirID, &isDirectory);
        
            if (err == noErr  &&  isDirectory)
                outSpec->parID = theDirID;
            else if (err == fnfErr && createDirs)
            {
                err = FSpDirCreate(inSpec, smCurrentScript, &theDirID);
                if (err == noErr)
                    outSpec->parID = theDirID;
                else if (err == fnfErr)
                    err = dirNFErr; 
            }
        }
    }
    else
    {
        outSpec->vRefNum = 0;
        outSpec->parID = 0;
    }
    
    if (err)
        return err;
    
    // Try making an FSSpec from the path
    if (inLength < 255)
    {
        // Use tempSpec as dest because if FSMakeFSSpec returns dirNFErr, it
        // will reset the dest spec and we'll lose what we determined above.
        
        myPLstrcpy(ppath, filePath);
        err = ::FSMakeFSSpec(outSpec->vRefNum, outSpec->parID, ppath, &tempSpec);
        if (err == noErr || err == fnfErr)
            *outSpec = tempSpec;
    }
    else if (!isRelative)
    {
        err = ::FSpLocationFromFullPath(inLength, filePath, outSpec);
    }
    else
    {   // If the path is relative and >255 characters we need to manually walk the
        // path appending each node to the initial FSSpec so to reach that code we
        // set the err to bdNamErr and fall into the code below
        err = bdNamErr;
    }
    
    // If we successfully created a spec then leave
    if (err == noErr)
        return err;
    
    // We get here when the directory hierarchy needs to be created or we're resolving
    // a relative path >255 characters long
    if (err == dirNFErr || err == bdNamErr)
    {
        const char* path = filePath;
        
        if (!isRelative)    // If path is relative, we still need vRefNum & parID.
        {
            outSpec->vRefNum = 0;
            outSpec->parID = 0;
        }

        do
        {
            // Locate the colon that terminates the node.
            // But if we've a partial path (starting with a colon), find the second one.
            const char* nextColon = strchr(path + (*path == ':'), ':');
            // Well, if there are no more colons, point to the end of the string.
            if (!nextColon)
                nextColon = path + strlen(path);

            // Make a pascal string out of this node.  Include initial
            // and final colon, if any!
            myPLstrncpy(ppath, path, nextColon - path + 1);
            
            // Use this string as a relative path using the directory created
            // on the previous round (or directory 0,0 on the first round).
            err = ::FSMakeFSSpec(outSpec->vRefNum, outSpec->parID, ppath, outSpec);

            // If this was the leaf node, then we are done.
            if (!*nextColon)
                break;

            // Since there's more to go, we have to get the directory ID, which becomes
            // the parID for the next round.
            if (err == noErr)
            {
                // The directory (or perhaps a file) exists. Find its dirID.
                long dirID;
                err = ::FSpGetDirectoryID(outSpec, &dirID, &isDirectory);
                if (!isDirectory)
                    err = dupFNErr; // oops! a file exists with that name.
                if (err != noErr)
                    break;          // bail if we've got an error
                outSpec->parID = dirID;
            }
            else if ((err == fnfErr) && createDirs)
            {
                // If we got "file not found" and we're allowed to create directories 
                // then we need to create one
                err = ::FSpDirCreate(outSpec, smCurrentScript, &outSpec->parID);
                // For some reason, this usually returns fnfErr, even though it works.
                if (err == fnfErr)
                    err = noErr;
            }
            if (err != noErr)
                break;
            path = nextColon; // next round
        } while (true);
    }
    
    return err;
}


#pragma mark -
#pragma mark [StFollowLinksState]
class StFollowLinksState
{
  public:
    StFollowLinksState(nsILocalFile *aFile) :
        mFile(aFile)
    {
        NS_ASSERTION(mFile, "StFollowLinksState passed a NULL file.");
        if (mFile)
            mFile->GetFollowLinks(&mSavedState);
    }

    StFollowLinksState(nsILocalFile *aFile, PRBool followLinksState) :
        mFile(aFile)
    {
        NS_ASSERTION(mFile, "StFollowLinksState passed a NULL file.");
        if (mFile) {
            mFile->GetFollowLinks(&mSavedState);
            mFile->SetFollowLinks(followLinksState);
        }
    }

    ~StFollowLinksState()
    {
        if (mFile)
            mFile->SetFollowLinks(mSavedState);
    }
    
  private:
    nsCOMPtr<nsILocalFile> mFile;
    PRBool mSavedState;
};

#pragma mark -
#pragma mark [nsDirEnumerator]
class nsDirEnumerator : public nsISimpleEnumerator
{
    public:

        NS_DECL_ISUPPORTS

        nsDirEnumerator()
        {
        }

        nsresult Init(nsILocalFileMac* parent) 
        {
            NS_ENSURE_ARG(parent);
            nsresult rv;
            FSSpec fileSpec;

            rv = parent->GetFSSpec(&fileSpec);
            if (NS_FAILED(rv))
                return rv;

            OSErr err;
            Boolean isDirectory;
            
            err = ::FSpGetDirectoryID(&fileSpec, &mDirID, &isDirectory);
            if (err || !isDirectory)
                return NS_ERROR_FILE_NOT_DIRECTORY;
                            
            mCatInfo.hFileInfo.ioNamePtr = mItemName;
            mCatInfo.hFileInfo.ioVRefNum = fileSpec.vRefNum;
            mItemIndex = 1;
                    
            return NS_OK;
        }

        NS_IMETHOD HasMoreElements(PRBool *result) 
        {
            nsresult rv = NS_OK;
            if (mNext == nsnull) 
            {
                mItemName[0] = 0;
                mCatInfo.dirInfo.ioFDirIndex = mItemIndex;
                mCatInfo.dirInfo.ioDrDirID = mDirID;
                
                OSErr err = ::PBGetCatInfoSync(&mCatInfo);
                if (err == fnfErr)
                {
                    // end of dir entries
                    *result = PR_FALSE;
                    return  NS_OK;
                }
                
                // Make a new nsILocalFile for the new element
                FSSpec  tempSpec;
                tempSpec.vRefNum = mCatInfo.hFileInfo.ioVRefNum;
                tempSpec.parID = mDirID;
                ::BlockMoveData(mItemName, tempSpec.name, mItemName[0] + 1);
                
                rv = NS_NewLocalFileWithFSSpec(&tempSpec, PR_TRUE, getter_AddRefs(mNext));
                if (NS_FAILED(rv)) 
                    return rv;
            }
            *result = mNext != nsnull;
            return NS_OK;
        }

        NS_IMETHOD GetNext(nsISupports **result) 
        {
            NS_ENSURE_ARG_POINTER(result);
            *result = nsnull;

            nsresult rv;
            PRBool hasMore;
            rv = HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            *result = mNext;        // might return nsnull
            NS_IF_ADDREF(*result);

            mNext = nsnull;
            ++mItemIndex;
            return NS_OK;
        }

    private:
        ~nsDirEnumerator() {}

    protected:
        nsCOMPtr<nsILocalFileMac>   mNext;

        CInfoPBRec              mCatInfo;
        short                   mItemIndex;
        long                    mDirID;
        Str63                   mItemName;
};

NS_IMPL_ISUPPORTS1(nsDirEnumerator, nsISimpleEnumerator)

#pragma mark -

OSType nsLocalFile::sCurrentProcessSignature = 0;
PRBool nsLocalFile::sHasHFSPlusAPIs = PR_FALSE;
PRBool nsLocalFile::sRunningOSX = PR_FALSE;

#pragma mark [CTOR/DTOR]
nsLocalFile::nsLocalFile() :
    mFollowLinks(PR_TRUE),
    mFollowLinksDirty(PR_TRUE),
    mSpecDirty(PR_TRUE),
    mCatInfoDirty(PR_TRUE),
    mType('TEXT'),
    mCreator(kDefaultCreator)
{
    ClearFSSpec(mSpec);
    ClearFSSpec(mTargetSpec);
    
    InitClassStatics();
    if (sCurrentProcessSignature != 0)
        mCreator = sCurrentProcessSignature;
}

nsLocalFile::nsLocalFile(const nsLocalFile& srcFile)
{
    *this = srcFile;
}

nsLocalFile::nsLocalFile(const FSSpec& aSpec, const nsACString& aAppendedPath) :
    mFollowLinks(PR_TRUE),
    mFollowLinksDirty(PR_TRUE),
    mSpecDirty(PR_TRUE),
    mSpec(aSpec),
    mAppendedPath(aAppendedPath),
    mCatInfoDirty(PR_TRUE),
    mType('TEXT'),
    mCreator(kDefaultCreator)
{
    ClearFSSpec(mTargetSpec);
    
    InitClassStatics();
    if (sCurrentProcessSignature != 0)
        mCreator = sCurrentProcessSignature;
}

nsLocalFile& nsLocalFile::operator=(const nsLocalFile& rhs)
{
    mFollowLinks = rhs.mFollowLinks;
    mFollowLinksDirty = rhs.mFollowLinksDirty;
    mSpecDirty = rhs.mSpecDirty;
    mSpec = rhs.mSpec;
    mAppendedPath = rhs.mAppendedPath;
    mTargetSpec = rhs.mTargetSpec;
    mCatInfoDirty = rhs.mCatInfoDirty;
    mType = rhs.mType;
    mCreator = rhs.mCreator;

    if (!rhs.mCatInfoDirty)
        mCachedCatInfo = rhs.mCachedCatInfo;

    return *this;
}

#pragma mark -
#pragma mark [nsISupports interface implementation]

NS_IMPL_THREADSAFE_ISUPPORTS3(nsLocalFile,
                              nsILocalFileMac,
                              nsILocalFile,
                              nsIFile)

NS_METHOD
nsLocalFile::nsLocalFileConstructor(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_NO_AGGREGATION(outer);

    nsLocalFile* inst = new nsLocalFile();
    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    
    nsresult rv = inst->QueryInterface(aIID, aInstancePtr);
    if (NS_FAILED(rv))
    {
        delete inst;
        return rv;
    }
    return NS_OK;
}

// This function resets any cached information about the file.
void
nsLocalFile::MakeDirty()
{
    mSpecDirty = PR_TRUE;
    mFollowLinksDirty = PR_TRUE;
    mCatInfoDirty = PR_TRUE;
    
    ClearFSSpec(mTargetSpec);
}


/* attribute PRBool followLinks; */
NS_IMETHODIMP 
nsLocalFile::GetFollowLinks(PRBool *aFollowLinks)
{
    NS_ENSURE_ARG_POINTER(aFollowLinks);
    *aFollowLinks = mFollowLinks;
    return NS_OK;
}

NS_IMETHODIMP 
nsLocalFile::SetFollowLinks(PRBool aFollowLinks)
{
    if (aFollowLinks != mFollowLinks)
    {
        mFollowLinks = aFollowLinks;
        mFollowLinksDirty = PR_TRUE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::ResolveAndStat()
{
    OSErr   err = noErr;
    FSSpec  resolvedSpec;
    
    // fnfErr means target spec is valid but doesn't exist.
    // If the end result is fnfErr, we're cleanly resolved.
    
    if (mSpecDirty)
    {
        if (mAppendedPath.Length())
        {
            err = ResolvePathAndSpec(mAppendedPath.get(), &mSpec, PR_FALSE, &resolvedSpec);
            if (err == noErr)
                mAppendedPath.Truncate(0);
        }
        else
            err = ::FSMakeFSSpec(mSpec.vRefNum, mSpec.parID, mSpec.name, &resolvedSpec);
	    if (err == noErr)
	    {
	        mSpec = resolvedSpec;
	        mSpecDirty = PR_FALSE;
	    }
        mFollowLinksDirty = PR_TRUE;
    }
    if (mFollowLinksDirty && (err == noErr))
    {
        if (mFollowLinks)
        {
            // Resolve the alias to the original file.
            resolvedSpec = mSpec;
            Boolean resolvedWasFolder, resolvedWasAlias;
            err = ::ResolveAliasFile(&resolvedSpec, TRUE, &resolvedWasFolder, &resolvedWasAlias);
            if (err == noErr || err == fnfErr) {
                err = noErr;
                mTargetSpec = resolvedSpec;
                mFollowLinksDirty = PR_FALSE;
            }
        }
        else
        {
            mTargetSpec = mSpec;
            mFollowLinksDirty = PR_FALSE;
        }
        mCatInfoDirty = PR_TRUE;
    }       
    
    return (MacErrorMapper(err));
}
    

NS_IMETHODIMP  
nsLocalFile::Clone(nsIFile **file)
{
    // Just copy-construct ourselves
    *file = new nsLocalFile(*this);
    if (!*file)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*file);
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::InitWithNativePath(const nsACString &filePath)
{
    // The incoming path must be a FULL path

	if (filePath.IsEmpty())
		return NS_ERROR_INVALID_ARG;
		
    MakeDirty();

    // If it starts with a colon, it's invalid
    if (filePath.First() == ':')
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    
    nsPathParser parser(filePath);
    OSErr err;
    Str255 pascalNode;
    FSSpec nodeSpec;
    
    const char *root = parser.First();
    if (root == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
        
    // The first component must be an existing volume
    myPLstrcpy(pascalNode, root);
    pascalNode[++pascalNode[0]] = ':';
    err = ::FSMakeFSSpec(0, 0, pascalNode, &nodeSpec);
    if (err)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    
    // Build as much of a spec as possible from the rest of the path
    // What doesn't exist will be left over in mAppendedPath
    const char *nextNode;    
    while ((nextNode = parser.Next()) != nsnull) {
        long dirID;
        Boolean isDir;
        err = ::FSpGetDirectoryID(&nodeSpec, &dirID, &isDir);
        if (err || !isDir)
            break;
        myPLstrcpy(pascalNode, nextNode);
        err = ::FSMakeFSSpec(nodeSpec.vRefNum, dirID, pascalNode, &nodeSpec);
        if (err == fnfErr)
            break;
    }
    mSpec = nodeSpec;
    mAppendedPath = parser.Remainder();
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::InitWithPath(const nsAString &filePath)
{
   nsresult rv;
   nsCAutoString fsStr;
   
   if (NS_SUCCEEDED(rv = NS_CopyUnicodeToNative(filePath, fsStr)))
     rv = InitWithNativePath(fsStr);

   return rv;
}

NS_IMETHODIMP  
nsLocalFile::InitWithFile(nsILocalFile *aFile)
{
    NS_ENSURE_ARG(aFile);
    nsLocalFile *asLocalFile = dynamic_cast<nsLocalFile*>(aFile);
    if (!asLocalFile)
        return NS_ERROR_NO_INTERFACE; // Well, sort of.
    *this = *asLocalFile;
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::OpenNSPRFileDesc(PRInt32 flags, PRInt32 mode, PRFileDesc **_retval)
{
// Macintosh doesn't really have mode bits, just drop them
#pragma unused (mode)

    NS_ENSURE_ARG(_retval);
    
    nsresult rv = NS_OK;
    FSSpec  spec;
    OSErr err = noErr;
    
    rv = ResolveAndStat();
    if (rv == NS_ERROR_FILE_NOT_FOUND && (flags & PR_CREATE_FILE))
        rv = NS_OK;
        
    if (flags & PR_CREATE_FILE) {
        rv = Create(nsIFile::NORMAL_FILE_TYPE, 0);
        /* If opening with the PR_EXCL flag the existence of the file prior to opening is an error */
        if ((flags & PR_EXCL) &&  (rv == NS_ERROR_FILE_ALREADY_EXISTS))
            return rv;
    }
    
    rv = GetFSSpec(&spec);
    if (NS_FAILED(rv))
        return rv;
    
    SInt8 perm;
    if (flags & PR_RDWR)
       perm = fsRdWrPerm;
    else if (flags & PR_WRONLY)
       perm = fsWrPerm;
    else
       perm = fsRdPerm;

    short refnum;
    err = ::FSpOpenDF(&spec, perm, &refnum);

    if (err == noErr && (flags & PR_TRUNCATE))
        err = ::SetEOF(refnum, 0);
    if (err == noErr && (flags & PR_APPEND))
        err = ::SetFPos(refnum, fsFromLEOF, 0);
    if (err != noErr)
        return MacErrorMapper(err);

    if ((*_retval = PR_ImportFile(refnum)) == 0)
        return NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_FILES,(PR_GetError() & 0xFFFF));
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::OpenANSIFileDesc(const char *mode, FILE * *_retval)
{
    NS_ENSURE_ARG(mode);
    NS_ENSURE_ARG_POINTER(_retval);
    
    nsresult rv; 
    FSSpec spec;

    rv = ResolveAndStat();
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv;
    
    if (mode[0] == 'w' || mode[0] == 'a') // Create if it doesn't exist   
    {
        if (rv == NS_ERROR_FILE_NOT_FOUND) {
            mType = (mode[1] == 'b') ? 'BiNA' : 'TEXT';
            rv = Create(nsIFile::NORMAL_FILE_TYPE, 0);
            if (NS_FAILED(rv))
                return rv;
        }
    }

    rv = GetFSSpec(&spec);
    if (NS_FAILED(rv))
        return rv;
                
#ifdef MACOSX
    // FSp_fopen() doesn't exist under macosx :-(
    nsXPIDLCString ourPath;
    rv = GetPath(getter_Copies(ourPath));
    if (NS_FAILED(rv))
        return rv;
    *_retval = fopen(ourPath, mode);
#else
    *_retval = FSp_fopen(&spec, mode);
#endif

    if (*_retval)
        return NS_OK;

    return NS_ERROR_FAILURE;
}


NS_IMETHODIMP  
nsLocalFile::Create(PRUint32 type, PRUint32 attributes)
{ 
    OSErr   err;

    if (type != NORMAL_FILE_TYPE && type != DIRECTORY_TYPE)
        return NS_ERROR_FILE_UNKNOWN_TYPE;

    FSSpec newSpec;

    if (mAppendedPath.Length())
    {   // We've got an FSSpec and an appended path so pass 'em both to ResolvePathAndSpec
        err = ResolvePathAndSpec(mAppendedPath.get(), &mSpec, PR_TRUE, &newSpec);
    }
    else
    {
        err = ::FSMakeFSSpec(mSpec.vRefNum, mSpec.parID, mSpec.name, &newSpec);
    }
    
    if (err != noErr && err != fnfErr)
        return (MacErrorMapper(err));

    switch (type)
    {
        case NORMAL_FILE_TYPE:
            SetOSTypeAndCreatorFromExtension();
            err = ::FSpCreate(&newSpec, mCreator, mType, smCurrentScript);
            break;

        case DIRECTORY_TYPE:
            {
                long newDirID;
                err = ::FSpDirCreate(&newSpec, smCurrentScript, &newDirID);
                // For some reason, this usually returns fnfErr, even though it works.
                if (err == fnfErr)
                    err = noErr;
            }
            break;
        
        default:
            return NS_ERROR_FILE_UNKNOWN_TYPE;
            break;
    }

    if (err == noErr)
    {
        mSpec = mTargetSpec = newSpec;
        mAppendedPath.Truncate(0);
    }
    
    return (MacErrorMapper(err));
}

NS_IMETHODIMP  
nsLocalFile::AppendNative(const nsACString &aNode)
{
    if (aNode.IsEmpty())
        return NS_OK;
    
    nsACString::const_iterator start, end;
    aNode.BeginReading(start);
    aNode.EndReading(end);
    if (FindCharInReadable(':', start, end))
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    
    MakeDirty();
    
    char truncBuffer[32];
    const char *node = NS_TruncNodeName(PromiseFlatCString(aNode).get(), truncBuffer);
    
    if (!mAppendedPath.Length())
    {
        OSErr   err;
        Boolean resolvedWasFolder, resolvedWasAlias;
        err = ::ResolveAliasFile(&mSpec, TRUE, &resolvedWasFolder, &resolvedWasAlias);
        if (err == noErr)
        {
            long    dirID;
            Boolean isDir;

            if (!resolvedWasFolder)
                return NS_ERROR_FILE_NOT_DIRECTORY;
            if ((err = ::FSpGetDirectoryID(&mSpec, &dirID, &isDir)) != noErr)
                return MacErrorMapper(err);
                
            FSSpec childSpec;    
            Str255 pascalNode;
            myPLstrcpy(pascalNode, node);
            err = ::FSMakeFSSpec(mSpec.vRefNum, dirID, pascalNode, &childSpec);
            if (err && err != fnfErr)
                return MacErrorMapper(err);
            mSpec = childSpec;
        }
        else if (err == fnfErr)
            mAppendedPath.Assign(node);
        else
            return MacErrorMapper(err);
    }
    else
    {
        if (mAppendedPath.First() != ':')
            mAppendedPath.Insert(':', 0);
        mAppendedPath.Append(":");
        mAppendedPath.Append(node);
    }
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::Append(const nsAString &node)
{
   nsresult rv;
   nsCAutoString fsStr;
   
   if (NS_SUCCEEDED(rv = NS_CopyUnicodeToNative(node, fsStr)))
     rv = AppendNative(fsStr);

   return rv;
}
    
NS_IMETHODIMP  
nsLocalFile::AppendRelativeNativePath(const nsACString &relPath)
{
    if (relPath.IsEmpty())
        return NS_ERROR_INVALID_ARG;
    
    nsresult        rv;
    nsPathParser    parser(relPath);
    const char*     node = parser.First();
    
    while (node)
    {
        if (NS_FAILED(rv = AppendNative(nsDependentCString(node))))
            return rv;
        node = parser.Next();
    }
        
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::AppendRelativePath(const nsAString &relPath)
{
   nsresult rv;
   nsCAutoString fsStr;
   
   if (NS_SUCCEEDED(rv = NS_CopyUnicodeToNative(relPath, fsStr)))
     rv = AppendRelativeNativePath(fsStr);

   return rv;
}

NS_IMETHODIMP  
nsLocalFile::GetNativeLeafName(nsACString &aLeafName)
{
    aLeafName.Truncate();
 
    // See if we've had a path appended
    if (mAppendedPath.Length())
    {
        const char* temp = mAppendedPath.get();
        if (temp == nsnull)
            return NS_ERROR_FILE_UNRECOGNIZED_PATH;

        const char* leaf = strrchr(temp, ':');
        
        // if the working path is just a node without any directory delimeters.
        if (leaf == nsnull)
            leaf = temp;
        else
            leaf++;

        aLeafName = leaf;
    }
    else
    {
        // We don't have an appended path so grab the leaf name from the FSSpec
        // Convert the Pascal string to a C string              
        PRInt32 len = mSpec.name[0];
        char* leafName = (char *)malloc(len + 1);
        if (!leafName) return NS_ERROR_OUT_OF_MEMORY;               
        ::BlockMoveData(&mSpec.name[1], leafName, len);
        leafName[len] = '\0';
        aLeafName = leafName;
        free(leafName);
    }
 
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetLeafName(nsAString &aLeafName)
{
   nsresult rv;
   nsCAutoString fsStr;
   
   if (NS_SUCCEEDED(rv = GetNativeLeafName(fsStr)))
     rv = NS_CopyNativeToUnicode(fsStr, aLeafName);
   return rv;
}

NS_IMETHODIMP  
nsLocalFile::SetNativeLeafName(const nsACString &aLeafName)
{
    if (aLeafName.IsEmpty())
        return NS_ERROR_INVALID_ARG;

    MakeDirty();

    char truncBuffer[32];
    const char *leafName = NS_TruncNodeName(PromiseFlatCString(aLeafName).get(), truncBuffer);

    if (mAppendedPath.Length())
    {   // Lop off the end of the appended path and replace it with the new leaf name
        PRInt32 offset = mAppendedPath.RFindChar(':');
        if (offset || ((!offset) && (1 < mAppendedPath.Length())))
        {
            mAppendedPath.Truncate(offset + 1);
        }
        mAppendedPath.Append(leafName);
    }
    else
    {
        // We don't have an appended path so directly modify the FSSpec
        myPLstrcpy(mSpec.name, leafName);
    }
    
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::SetLeafName(const nsAString &aLeafName)
{
   nsresult rv;
   nsCAutoString fsStr;
   
   if (NS_SUCCEEDED(rv = NS_CopyUnicodeToNative(aLeafName, fsStr)))
     rv = SetNativeLeafName(fsStr);

   return rv;
}

NS_IMETHODIMP  
nsLocalFile::GetNativePath(nsACString &_retval)
{
    _retval.Truncate();

    nsCAutoString fsCharSetPathStr;
                
#if TARGET_CARBON
    if (sHasHFSPlusAPIs) // should always be true under Carbon, but in case...
    {
        OSErr err;
        nsresult rv;
        nsAutoString ucPathString;
        
        if ((err = HFSPlusGetRawPath(mSpec, ucPathString)) != noErr)
            return MacErrorMapper(err);
        rv = NS_CopyUnicodeToNative(ucPathString, fsCharSetPathStr);
        if (NS_FAILED(rv))
            return rv;
    }
    else
#endif
    {
        // Now would be a good time to call the code that makes an FSSpec into a path
        short   fullPathLen;
        Handle  fullPathHandle;
        (void)::FSpGetFullPath(&mSpec, &fullPathLen, &fullPathHandle);
        if (!fullPathHandle)
            return NS_ERROR_OUT_OF_MEMORY;
        
        ::HLock(fullPathHandle);        
        fsCharSetPathStr.Assign(*fullPathHandle, fullPathLen);
        ::DisposeHandle(fullPathHandle);
    }

    // We need to make sure that even if we have a path to a
    // directory we don't return the trailing colon. It breaks
    // the component manager. (Bugzilla bug #26102) 
    if (fsCharSetPathStr.Last() == ':')
        fsCharSetPathStr.Truncate(fsCharSetPathStr.Length() - 1);
    
    // Now, tack on mAppendedPath. It never ends in a colon.
    if (mAppendedPath.Length())
    {
        if (mAppendedPath.First() != ':')
            fsCharSetPathStr.Append(":");
        fsCharSetPathStr.Append(mAppendedPath);
    }
    
    _retval = fsCharSetPathStr;
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetPath(nsAString &_retval)
{
    nsresult rv = NS_OK;
                
#if TARGET_CARBON
    if (sHasHFSPlusAPIs) // should always be true under Carbon, but in case...
    {
        OSErr err;
        nsAutoString ucPathString;
        
        if ((err = HFSPlusGetRawPath(mSpec, ucPathString)) != noErr)
            return MacErrorMapper(err);
            
        // We need to make sure that even if we have a path to a
        // directory we don't return the trailing colon. It breaks
        // the component manager. (Bugzilla bug #26102) 
        if (ucPathString.Last() == PRUnichar(':'))
            ucPathString.Truncate(ucPathString.Length() - 1);
        
        // Now, tack on mAppendedPath. It never ends in a colon.
        if (mAppendedPath.Length())
        {
            nsAutoString ucAppendage;
            if (mAppendedPath.First() != ':')
                ucPathString.Append(PRUnichar(':'));
            rv = NS_CopyNativeToUnicode(mAppendedPath, ucAppendage);
            if (NS_FAILED(rv))
                return rv;
            ucPathString.Append(ucAppendage);
        }
        
        _retval = ucPathString;
    }
    else
#endif
    {
        nsCAutoString fsStr;

        if (NS_SUCCEEDED(rv = GetNativePath(fsStr))) {
            rv = NS_CopyNativeToUnicode(fsStr, _retval);
        }
    }
    return rv;
}

nsresult nsLocalFile::MoveCopy( nsIFile* newParentDir, const nsACString &newName, PRBool isCopy, PRBool followLinks )
{
    OSErr macErr;
    FSSpec srcSpec;
    Str255 newPascalName;
    nsresult rv;
    
    StFollowLinksState srcFollowState(this, followLinks);
    rv = GetFSSpec(&srcSpec);
    if ( NS_FAILED( rv ) )
        return rv;  
    
    // If newParentDir == nsnull, it's a simple rename
    if ( !newParentDir )
    {
        myPLstrncpy( newPascalName, PromiseFlatCString(newName).get(), 255 );
        macErr = ::FSpRename( &srcSpec, newPascalName );
        return MacErrorMapper( macErr );
    }

    nsCOMPtr<nsILocalFileMac> destDir(do_QueryInterface( newParentDir ));
    StFollowLinksState destFollowState(destDir, followLinks);
    FSSpec destSpec;
    rv = destDir->GetFSSpec(&destSpec);
    if ( NS_FAILED( rv ) )
        return rv;
    
    long dirID;
    Boolean isDirectory;    
    macErr = ::FSpGetDirectoryID(&destSpec, &dirID, &isDirectory);      
    if ( macErr || !isDirectory )
        return NS_ERROR_FILE_DESTINATION_NOT_DIR;

    if ( !newName.IsEmpty() )
        myPLstrncpy( newPascalName, PromiseFlatCString(newName).get(), 255);
    else
        memcpy(newPascalName, srcSpec.name, srcSpec.name[0] + 1);
    if ( isCopy )
    {
        macErr = ::FSpGetDirectoryID(&srcSpec, &dirID, &isDirectory);       
        if (macErr == noErr)
        {
            const PRInt32   kCopyBufferSize = (1024 * 512);   // allocate our own buffer to speed file copies. Bug #103202
            OSErr     tempErr;
            Handle    copyBufferHand = ::TempNewHandle(kCopyBufferSize, &tempErr);
            void*     copyBuffer = nsnull;
            PRInt32   copyBufferSize = 0;

            // it's OK if the allocated failed; FSpFileCopy will just fall back on its own internal 16k buffer
            if (copyBufferHand) 
            {
              ::HLock(copyBufferHand);
              copyBuffer = *copyBufferHand;
              copyBufferSize = kCopyBufferSize;
            }

            if ( isDirectory )
                macErr = MacFSpDirectoryCopyRename( &srcSpec, &destSpec, newPascalName, copyBuffer, copyBufferSize, true, NULL );
            else
                macErr = ::FSpFileCopy( &srcSpec, &destSpec, newPascalName, copyBuffer, copyBufferSize, true );

            if (copyBufferHand)
              ::DisposeHandle(copyBufferHand);
        }
    }
    else
    {
        macErr= ::FSpMoveRenameCompat(&srcSpec, &destSpec, newPascalName);
        if ( macErr == diffVolErr)
        {
                // On a different Volume so go for Copy and then delete
                rv = CopyToNative( newParentDir, newName );
                if ( NS_FAILED ( rv ) )
                    return rv;
                return Remove( PR_TRUE );
        }
    }   
    return MacErrorMapper( macErr );
}

NS_IMETHODIMP  
nsLocalFile::CopyToNative(nsIFile *newParentDir, const nsACString &newName)
{
    return MoveCopy( newParentDir, newName, PR_TRUE, PR_FALSE );
}

NS_IMETHODIMP  
nsLocalFile::CopyTo(nsIFile *newParentDir, const nsAString &newName)
{
    if (newName.IsEmpty())
        return CopyToNative(newParentDir, nsCString());
        
    nsresult rv;
    nsCAutoString fsStr;
    if (NS_SUCCEEDED(rv = NS_CopyUnicodeToNative(newName, fsStr)))
        rv = CopyToNative(newParentDir, fsStr);
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::CopyToFollowingLinksNative(nsIFile *newParentDir, const nsACString &newName)
{
    return MoveCopy( newParentDir, newName, PR_TRUE, PR_TRUE );
}

NS_IMETHODIMP  
nsLocalFile::CopyToFollowingLinks(nsIFile *newParentDir, const nsAString &newName)
{
    if (newName.IsEmpty())
        return CopyToFollowingLinksNative(newParentDir, nsCString());

    nsresult rv;
    nsCAutoString fsStr;
    if (NS_SUCCEEDED(rv = NS_CopyUnicodeToNative(newName, fsStr)))
        rv = CopyToFollowingLinksNative(newParentDir, fsStr);
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::MoveToNative(nsIFile *newParentDir, const nsACString &newName)
{
    return MoveCopy( newParentDir, newName, PR_FALSE, PR_FALSE );
}

NS_IMETHODIMP  
nsLocalFile::MoveTo(nsIFile *newParentDir, const nsAString &newName)
{
    if (newName.IsEmpty())
        return MoveToNative(newParentDir, nsCString());
        
    nsresult rv;
    nsCAutoString fsStr;
    if (NS_SUCCEEDED(rv = NS_CopyUnicodeToNative(newName, fsStr)))
        rv = MoveToNative(newParentDir, fsStr);
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::Load(PRLibrary * *_retval)
{
    PRBool isFile;
    nsresult rv = IsFile(&isFile);

    if (NS_FAILED(rv))
        return rv;
    
    if (! isFile)
        return NS_ERROR_FILE_IS_DIRECTORY;

  NS_TIMELINE_START_TIMER("PR_LoadLibrary");

#if !TARGET_CARBON      
    // This call to SystemTask is here to give the OS time to grow its
    // FCB (file control block) list, which it seems to be unable to
    // do unless we yield some time to the OS. See bugs 64978 & 70543
    // for the whole story.
    ::SystemTask();
#endif

    // Use the new PR_LoadLibraryWithFlags which allows us to use a FSSpec
    PRLibSpec libSpec;
    libSpec.type = PR_LibSpec_MacIndexedFragment;
    libSpec.value.mac_indexed_fragment.fsspec = &mTargetSpec;
    libSpec.value.mac_indexed_fragment.index = 0;
    *_retval =  PR_LoadLibraryWithFlags(libSpec, 0);
    
  NS_TIMELINE_STOP_TIMER("PR_LoadLibrary");
  NS_TIMELINE_MARK_TIMER("PR_LoadLibrary");

    if (*_retval)
        return NS_OK;

    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP  
nsLocalFile::Remove(PRBool recursive)
{
    OSErr err;
    nsresult rv;
    FSSpec specToDelete;
    PRBool isDir;
    
    StFollowLinksState(this, PR_FALSE);
    
    rv = IsDirectory(&isDir); // Calls ResolveAndStat()
    if (NS_FAILED(rv))
        return rv;  
    rv = GetFSSpec(&specToDelete);
    if (NS_FAILED(rv))
        return rv;
    
    if (isDir && recursive)
        err = ::DeleteDirectory( specToDelete.vRefNum, specToDelete.parID, specToDelete.name );
    else
        err = ::HDelete( specToDelete.vRefNum, specToDelete.parID, specToDelete.name );
    
    return MacErrorMapper( err );
}

NS_IMETHODIMP  
nsLocalFile::GetLastModifiedTime(PRInt64 *aLastModifiedTime)
{
    NS_ENSURE_ARG(aLastModifiedTime);
    *aLastModifiedTime = 0;
    
    nsresult rv = ResolveAndStat();
    if ( NS_FAILED( rv ) )
        return rv;      
    rv = UpdateCachedCatInfo(PR_TRUE);
    if ( NS_FAILED( rv ) )
        return rv;
    
    // The mod date is in the same spot for files and dirs.
    return ConvertMacTimeToMilliseconds( aLastModifiedTime, mCachedCatInfo.hFileInfo.ioFlMdDat );
}

NS_IMETHODIMP  
nsLocalFile::SetLastModifiedTime(PRInt64 aLastModifiedTime)
{
    nsresult rv = ResolveAndStat();
    if ( NS_FAILED(rv) )
        return rv;
    
    PRUint32 macTime = 0;
    OSErr err = noErr;
    
    ConvertMillisecondsToMacTime(aLastModifiedTime, &macTime);
    
    if (NS_SUCCEEDED(rv = UpdateCachedCatInfo(PR_TRUE)))
    {
        if (mCachedCatInfo.hFileInfo.ioFlAttrib & ioDirMask)
        {
            mCachedCatInfo.dirInfo.ioDrMdDat = macTime; 
            mCachedCatInfo.dirInfo.ioDrParID = mFollowLinks ? mTargetSpec.parID : mSpec.parID;
        }
        else
        {
            mCachedCatInfo.hFileInfo.ioFlMdDat = macTime;
            mCachedCatInfo.hFileInfo.ioDirID = mFollowLinks ? mTargetSpec.parID : mSpec.parID;
        }   
            
        err = ::PBSetCatInfoSync(&mCachedCatInfo);
        if (err != noErr)
            return MacErrorMapper(err);
    }
    
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::GetLastModifiedTimeOfLink(PRInt64 *aLastModifiedTime)
{
    NS_ENSURE_ARG(aLastModifiedTime);
    
    nsresult rv;
    PRBool isLink;
    
    rv = IsSymlink(&isLink);
    if (NS_FAILED(rv))
        return rv;
    if (!isLink)
        return NS_ERROR_FAILURE;
    
    StFollowLinksState followState(this, PR_FALSE);
    return GetLastModifiedTime(aLastModifiedTime);
}

NS_IMETHODIMP  
nsLocalFile::SetLastModifiedTimeOfLink(PRInt64 aLastModifiedTime)
{
    nsresult rv;
    PRBool isLink;
    
    rv = IsSymlink(&isLink);
    if (NS_FAILED(rv))
        return rv;
    if (!isLink)
        return NS_ERROR_FAILURE;
    
    StFollowLinksState followState(this, PR_FALSE);
    return SetLastModifiedTime(aLastModifiedTime);
}


NS_IMETHODIMP  
nsLocalFile::GetFileSize(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);
    nsresult rv;
    
    *aFileSize = LL_Zero();
    
    if (NS_SUCCEEDED(rv = ResolveAndStat()) && NS_SUCCEEDED(rv = UpdateCachedCatInfo(PR_TRUE)))
    {
        if (!(mCachedCatInfo.hFileInfo.ioFlAttrib & ioDirMask))
        {
            long dataSize = mCachedCatInfo.hFileInfo.ioFlLgLen;
            long resSize = mCachedCatInfo.hFileInfo.ioFlRLgLen;
            
            // For now we've only got 32 bits of file size info
            PRInt64 dataInt64 = LL_Zero();
            PRInt64 resInt64 = LL_Zero();
            
            // WARNING!!!!!!
            // 
            // For now we do NOT add the data and resource fork sizes as there are several
            // assumptions in the code (notably in form submit) that only the data fork is
            // used. 
            // LL_I2L(resInt64, resSize);
            
            LL_I2L(dataInt64, dataSize);
            
            LL_ADD((*aFileSize), dataInt64, resInt64);
        }
        // leave size at zero for dirs
    }
            
    return rv;
}


NS_IMETHODIMP  
nsLocalFile::SetFileSize(PRInt64 aFileSize)
{
    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;
        
    short   refNum;
    OSErr   err;
    PRInt32 aNewLength;
    
    LL_L2I(aNewLength, aFileSize);
    
    // Need to open the file to set the size
    if (::FSpOpenDF(&mTargetSpec, fsWrPerm, &refNum) != noErr)
        return NS_ERROR_FILE_ACCESS_DENIED;

    err = ::SetEOF(refNum, aNewLength);
        
    // Close the file unless we got an error that it was already closed
    if (err != fnOpnErr)
        (void)::FSClose(refNum);
        
    if (err != noErr)
        return MacErrorMapper(err);
        
    return MacErrorMapper(err);
}

NS_IMETHODIMP  
nsLocalFile::GetFileSizeOfLink(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);

    StFollowLinksState followState(this, PR_FALSE);
    return GetFileSize(aFileSize);
}

NS_IMETHODIMP  
nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
    NS_ENSURE_ARG(aDiskSpaceAvailable);
        
    PRInt64 space64Bits;

    LL_I2L(space64Bits , LONG_MAX);
    
    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    XVolumeParam    pb;
    pb.ioCompletion = nsnull;
    pb.ioVolIndex = 0;
    pb.ioNamePtr = nsnull;
    pb.ioVRefNum = mFollowLinks ? mTargetSpec.vRefNum : mSpec.vRefNum;
    
    // we should check if this call is available
    OSErr err = ::PBXGetVolInfoSync(&pb);
    
    if (err == noErr)
    {
        const UnsignedWide& freeBytes = UInt64ToUnsignedWide(pb.ioVFreeBytes);
#ifdef HAVE_LONG_LONG
    space64Bits = UnsignedWideToUInt64(freeBytes);
#else
        space64Bits.lo = freeBytes.lo;
        space64Bits.hi = freeBytes.hi;
#endif
    }
        
    *aDiskSpaceAvailable = space64Bits;

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::GetParent(nsIFile * *aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);
    *aParent = nsnull;
    
    nsresult rv = NS_OK;
    PRInt32 offset;

    nsCOMPtr<nsILocalFileMac> localFile;
    PRInt32     appendedLen = mAppendedPath.Length();
    OSErr       err;
                
    if (!appendedLen || (appendedLen == 1 && mAppendedPath.CharAt(0) == ':'))
    {
        rv = ResolveAndStat();
        //if the file does not exist, does not mean that the parent does not.
        if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
            return rv;

        CInfoPBRec  pBlock = {0};
        FSSpec      parentFolderSpec;
        parentFolderSpec.name[0] = 0;

        pBlock.dirInfo.ioVRefNum = mSpec.vRefNum;
        pBlock.dirInfo.ioDrDirID = mSpec.parID;
        pBlock.dirInfo.ioNamePtr = (StringPtr)parentFolderSpec.name;
        pBlock.dirInfo.ioFDirIndex = -1;        //get info on parID
        err = PBGetCatInfoSync(&pBlock);
        if (err != noErr) 
            return MacErrorMapper(err);
        parentFolderSpec.vRefNum = mSpec.vRefNum;
        parentFolderSpec.parID = pBlock.dirInfo.ioDrParID;
        
        localFile = new nsLocalFile;
        if (!localFile)
            return NS_ERROR_OUT_OF_MEMORY;
        rv = localFile->InitWithFSSpec(&parentFolderSpec);
        if (NS_FAILED(rv))
            return rv;
    }
    else
    {
        // trim off the last component of the appended path
        // construct a new file from our spec + trimmed path

        nsCAutoString parentAppendage(mAppendedPath);
        
        if (parentAppendage.Last() == ':')
            parentAppendage.Truncate(appendedLen - 1);
        if ((offset = parentAppendage.RFindChar(':')) != -1)
            parentAppendage.Truncate(offset);
        else
            parentAppendage.Truncate(0);
            
        localFile = new nsLocalFile(mSpec, parentAppendage);
        if (!localFile)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    *aParent = localFile;
    NS_ADDREF(*aParent);

    return rv;
}


NS_IMETHODIMP  
nsLocalFile::Exists(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;
    
    nsresult rv = ResolveAndStat();
    if (NS_SUCCEEDED(rv)) {
        if (NS_SUCCEEDED(UpdateCachedCatInfo(PR_TRUE)))
            *_retval = PR_TRUE;
    }
        
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsPackage(PRBool *outIsPackage)
{
    NS_ENSURE_ARG(outIsPackage);
    *outIsPackage = PR_FALSE;

    // Note: IsDirectory() calls ResolveAndStat() & UpdateCachedCatInfo
    PRBool isDir;
    nsresult rv = IsDirectory(&isDir);
    if (NS_FAILED(rv)) return rv;

    *outIsPackage = ((mCachedCatInfo.dirInfo.ioFlAttrib & kioFlAttribDirMask) &&
                     (mCachedCatInfo.dirInfo.ioDrUsrWds.frFlags & kHasBundle));

    if ((!*outIsPackage) && isDir)
    {
        // Believe it or not, folders ending with ".app" are also considered
        // to be packages, even if the top-level folder doesn't have bundle set
        nsCAutoString name;
        if (NS_SUCCEEDED(rv = GetNativeLeafName(name)))
        {
            const char *extPtr = strrchr(name.get(), '.');
            if (extPtr)
            {
                if (!nsCRT::strcasecmp(extPtr, ".app"))
                {
                    *outIsPackage = PR_TRUE;
                }
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsWritable(PRBool *outIsWritable)
{
    NS_ENSURE_ARG(outIsWritable);
    *outIsWritable = PR_TRUE;

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv)) return rv;
    
    rv = UpdateCachedCatInfo(PR_TRUE);
    if (NS_FAILED(rv)) return rv;
    
    *outIsWritable = !(mCachedCatInfo.hFileInfo.ioFlAttrib & kioFlAttribLockedMask);
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsReadable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);

    // is it ever not readable on Mac?
    *_retval = PR_TRUE;
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::IsExecutable(PRBool *outIsExecutable)
{
    NS_ENSURE_ARG(outIsExecutable);
    *outIsExecutable = PR_FALSE;    // Assume failure

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv)) return rv;

#if TARGET_CARBON
    // If we're running under OS X ask LaunchServices if we're executable
    if (sRunningOSX)
    {
        if ( (UInt32)LSCopyItemInfoForRef != (UInt32)kUnresolvedCFragSymbolAddress )
        {
            FSRef   theRef;
            LSRequestedInfo theInfoRequest = kLSRequestAllInfo;
            LSItemInfoRecord theInfo;
            
            if (::FSpMakeFSRef(&mTargetSpec, &theRef) == noErr)
            {
                if (::LSCopyItemInfoForRef(&theRef, theInfoRequest, &theInfo) == noErr)
                {
                    if ((theInfo.flags & kLSItemInfoIsApplication) != 0)
                        *outIsExecutable = PR_TRUE;
                }
            }
        }
    }
    else
#endif
    {
    OSType fileType;
    rv = GetFileType(&fileType);
    if (NS_FAILED(rv)) return rv;
    
    *outIsExecutable = (fileType == 'APPL' || fileType == 'appe' || fileType == 'FNDR');
    }
    
    return NS_OK;
}


NS_IMETHODIMP  
nsLocalFile::IsDirectory(PRBool *outIsDir)
{
    NS_ENSURE_ARG(outIsDir);
    *outIsDir = PR_FALSE;

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv)) return rv;
    
    rv = UpdateCachedCatInfo(PR_FALSE);
    if (NS_FAILED(rv)) return rv;
    
    *outIsDir = (mCachedCatInfo.hFileInfo.ioFlAttrib & ioDirMask) != 0;
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsFile(PRBool *outIsFile)
{
    NS_ENSURE_ARG(outIsFile);
    *outIsFile = PR_FALSE;

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv)) return rv;
    
    rv = UpdateCachedCatInfo(PR_FALSE);
    if (NS_FAILED(rv)) return rv;

    *outIsFile = (mCachedCatInfo.hFileInfo.ioFlAttrib & ioDirMask) == 0;
    return NS_OK;
}

NS_IMETHODIMP  
nsLocalFile::IsHidden(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv)) return rv;

    rv = UpdateCachedCatInfo(PR_FALSE);
    if (NS_FAILED(rv)) return rv;
    
    *_retval = (mCachedCatInfo.hFileInfo.ioFlFndrInfo.fdFlags & kIsInvisible) != 0;

    if (sRunningOSX)
    {
        // on Mac OS X, also follow Unix "convention" where files
        // beginning with a period are considered to be hidden
        nsCAutoString name;
        if (NS_SUCCEEDED(rv = GetNativeLeafName(name)))
        {
            if (name.First() == '.')
            {
                *_retval = PR_TRUE;
            }
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSymlink(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv)) return rv;

    Boolean isAlias, isFolder;
    if (::IsAliasFile(&mSpec, &isAlias, &isFolder) == noErr)
        *_retval = isAlias;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Equals(nsIFile *inFile, PRBool *_retval)
{
    NS_ENSURE_ARG(inFile);
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    // Building paths is expensive. If we can get the FSSpecs of
    // both (they or their parents exist) just compare the specs.
    nsCOMPtr<nsILocalFileMac> inMacFile(do_QueryInterface(inFile));
    FSSpec fileSpec, inFileSpec;
    if (NS_SUCCEEDED(GetFSSpec(&fileSpec)) && inMacFile && NS_SUCCEEDED(inMacFile->GetFSSpec(&inFileSpec)))
        *_retval = IsEqualFSSpec(fileSpec, inFileSpec);
    else
    {
        nsCAutoString filePath;
        GetNativePath(filePath);
    
        nsXPIDLCString inFilePath;
        inFile->GetNativePath(inFilePath);
        
        if (nsCRT::strcasecmp(inFilePath.get(), filePath.get()) == 0)
            *_retval = PR_TRUE;
    }
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Contains(nsIFile *inFile, PRBool recur, PRBool *outContains)
{
  /* Note here that we make no attempt to deal with the problem
     of folder aliases. Doing a 'Contains' test and dealing with
     folder aliases is Hard. Think about it.
  */
    *outContains = PR_FALSE;

    PRBool isDir;
    nsresult rv = IsDirectory(&isDir);    // need to cache this
    if (NS_FAILED(rv)) return rv;
    if (!isDir) return NS_OK;   // must be a dir to contain someone

    nsCOMPtr<nsILocalFileMac> macFile(do_QueryInterface(inFile));
    if (!macFile) return NS_OK;     // trying to compare non-local with local file

    FSSpec  mySpec = mSpec;
    FSSpec  compareSpec;

    // NOTE: we're not resolving inFile if it was an alias
    StFollowLinksState followState(macFile, PR_FALSE);
    rv = macFile->GetFSSpec(&compareSpec);
    if (NS_FAILED(rv)) return rv;

    // if they are on different volumes, bail
    if (mSpec.vRefNum != compareSpec.vRefNum)
        return NS_OK;

    // if recur == true, test every parent, otherwise just test the first one
    // (yes, recur does not get set in this loop)
    OSErr   err = noErr;
    do
    {
        FSSpec  parentFolderSpec;
        err = GetParentFolderSpec(compareSpec, parentFolderSpec);
        if (err != noErr) break;                // we reached the top   

        if (IsEqualFSSpec(parentFolderSpec, mySpec))
        {
            *outContains = PR_TRUE;
            break;
        }
        
        compareSpec = parentFolderSpec;
    } while (recur);
      
    return NS_OK;
}



NS_IMETHODIMP
nsLocalFile::GetNativeTarget(nsACString &_retval)
{   
    _retval.Truncate();
    
    PRBool symLink;
    
    nsresult rv = IsSymlink(&symLink);
    if (NS_FAILED(rv))
        return rv;

    if (!symLink)
        return NS_ERROR_FILE_INVALID_PATH;
        
    StFollowLinksState followState(this, PR_TRUE);
    return GetNativePath(_retval);
}

NS_IMETHODIMP
nsLocalFile::GetTarget(nsAString &_retval)
{   
   nsresult rv;
   nsCAutoString fsStr;
   
   if (NS_SUCCEEDED(rv = GetNativeTarget(fsStr))) {
     rv = NS_CopyNativeToUnicode(fsStr, _retval);
   }
   return rv;
}

NS_IMETHODIMP
nsLocalFile::GetDirectoryEntries(nsISimpleEnumerator * *entries)
{
    nsresult rv;
    
    *entries = nsnull;

    PRBool isDir;
    rv = IsDirectory(&isDir);
    if (NS_FAILED(rv)) 
        return rv;
    if (!isDir)
        return NS_ERROR_FILE_NOT_DIRECTORY;

    nsDirEnumerator* dirEnum = new nsDirEnumerator();
    if (dirEnum == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(dirEnum);
    rv = dirEnum->Init(this);
    if (NS_FAILED(rv)) 
    {
        NS_RELEASE(dirEnum);
        return rv;
    }
    
    *entries = dirEnum;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetPersistentDescriptor(nsACString &aPersistentDescriptor)
{
   aPersistentDescriptor.Truncate();
   
   nsresult  rv = ResolveAndStat();
   if ( NS_FAILED( rv ) )
     return rv;

   AliasHandle    aliasH;
   OSErr err = ::NewAlias(nil, &mTargetSpec, &aliasH);
   if (err != noErr)
     return MacErrorMapper(err);

   PRUint32 bytes = ::GetHandleSize((Handle) aliasH);
   HLock((Handle) aliasH);
   char* buf = PL_Base64Encode((const char*)*aliasH, bytes, nsnull); // Passing nsnull for dest makes NULL-term string
   ::DisposeHandle((Handle) aliasH);
   NS_ENSURE_TRUE(buf, NS_ERROR_OUT_OF_MEMORY);

   aPersistentDescriptor = buf;
   PR_Free(buf);

   return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetPersistentDescriptor(const nsACString &aPersistentDescriptor)
{
   if (aPersistentDescriptor.IsEmpty())
      return NS_ERROR_INVALID_ARG;
   
   nsresult rv = NS_OK;

   PRUint32 dataSize = aPersistentDescriptor.Length();
   char* decodedData = PL_Base64Decode(PromiseFlatCString(aPersistentDescriptor).get(), dataSize, nsnull);
   // Cast to an alias record and resolve.
   AliasHandle aliasH = nsnull;
   if (::PtrToHand(decodedData, &(Handle)aliasH, (dataSize * 3) / 4) != noErr)
      rv = NS_ERROR_OUT_OF_MEMORY;
   PR_Free(decodedData);
   NS_ENSURE_SUCCESS(rv, rv);

   Boolean changed;
   FSSpec resolvedSpec;
   OSErr err;
   err = ::ResolveAlias(nsnull, aliasH, &resolvedSpec, &changed);
   if (err == fnfErr) // resolvedSpec is valid in this case
      err = noErr;
   rv = MacErrorMapper(err);
   DisposeHandle((Handle) aliasH);
   NS_ENSURE_SUCCESS(rv, rv);
 
   return InitWithFSSpec(&resolvedSpec);   
}

#pragma mark -

// a stack-based, exception safe class for an AEDesc

#pragma mark
class StAEDesc: public AEDesc
{
public:
                    StAEDesc()
                    {
                        descriptorType = typeNull;
                        dataHandle = nil;
                    }
                                        
                    ~StAEDesc()
                    {
                        ::AEDisposeDesc(this);
                    }

                    void Clear()
                    {
                        ::AEDisposeDesc(this);
                        descriptorType = typeNull;
                        dataHandle = nil;
                    }
                                        
private:
    // disallow copies and assigns
                            StAEDesc(const StAEDesc& rhs);      // copy constructor
    StAEDesc&   operator= (const StAEDesc&rhs);     // throws OSErrs

};

#pragma mark -
#pragma mark [Utility methods]


nsresult nsLocalFile::UpdateCachedCatInfo(PRBool forceUpdate) 
{
    if (!mCatInfoDirty && !forceUpdate)
        return NS_OK;
        
    FSSpec spectoUse = mFollowLinks ? mTargetSpec : mSpec;
    mCachedCatInfo.hFileInfo.ioCompletion = nsnull;
    mCachedCatInfo.hFileInfo.ioFDirIndex = 0; // use dirID and name
    mCachedCatInfo.hFileInfo.ioVRefNum = spectoUse.vRefNum;
    mCachedCatInfo.hFileInfo.ioDirID = spectoUse.parID;
    mCachedCatInfo.hFileInfo.ioNamePtr = spectoUse.name;

    OSErr err = ::PBGetCatInfoSync(&mCachedCatInfo);
    if (err == noErr)
    {
        mCatInfoDirty = PR_FALSE;
        return NS_OK;
    }
    return MacErrorMapper(err);
}


nsresult nsLocalFile::FindRunningAppBySignature (OSType aAppSig, FSSpec& outSpec, ProcessSerialNumber& outPsn)
{
    ProcessInfoRec  info;
    FSSpec                  tempFSSpec;
    OSErr                   err = noErr;
    
    outPsn.highLongOfPSN = 0;
    outPsn.lowLongOfPSN  = kNoProcess;

    while (PR_TRUE)
    {
        err = ::GetNextProcess(&outPsn);
        if (err == procNotFound) break;
        if (err != noErr) return NS_ERROR_FAILURE;
        info.processInfoLength = sizeof(ProcessInfoRec);
        info.processName = nil;
        info.processAppSpec = &tempFSSpec;
        err = ::GetProcessInformation(&outPsn, &info);
        if (err != noErr) return NS_ERROR_FAILURE;
        
        if (info.processSignature == aAppSig)
        {
            outSpec = tempFSSpec;
            return NS_OK;
        }
    }
    
    return NS_ERROR_FILE_NOT_FOUND;     // really process not found
}


nsresult nsLocalFile::FindRunningAppByFSSpec(const FSSpec& appSpec, ProcessSerialNumber& outPsn)
{
    ProcessInfoRec  info;
    FSSpec                  tempFSSpec;
    OSErr                   err = noErr;
    
    outPsn.highLongOfPSN = 0;
    outPsn.lowLongOfPSN  = kNoProcess;

    while (PR_TRUE)
    {
        err = ::GetNextProcess(&outPsn);
        if (err == procNotFound) break;     
        if (err != noErr) return NS_ERROR_FAILURE;
        info.processInfoLength = sizeof(ProcessInfoRec);
        info.processName = nil;
        info.processAppSpec = &tempFSSpec;
        err = ::GetProcessInformation(&outPsn, &info);
        if (err != noErr) return NS_ERROR_FAILURE;
        
        if (IsEqualFSSpec(appSpec, *info.processAppSpec))
        {
            return NS_OK;
        }
    }
    
    return NS_ERROR_FILE_NOT_FOUND;     // really process not found
}


nsresult nsLocalFile::FindAppOnLocalVolumes(OSType sig, FSSpec &outSpec)
{
    OSErr   err;
    
    // get the system volume
    long        systemFolderDirID;
    short       sysVRefNum;
    err = FindFolder(kOnSystemDisk, kSystemFolderType, false, &sysVRefNum, &systemFolderDirID);
    if (err != noErr) return NS_ERROR_FAILURE;

    short   vRefNum = sysVRefNum;
    short   index = 0;
    
    while (true)
    {
        if (index == 0 || vRefNum != sysVRefNum)
        {
            // should we avoid AppleShare volumes?
            
            Boolean hasDesktopDB;
            err = VolHasDesktopDB(vRefNum, &hasDesktopDB);
            if (err != noErr) return err;
            if (hasDesktopDB)
            {
                err = FindAppOnVolume(sig, vRefNum, &outSpec);
                if (err != afpItemNotFound) return err;
            }
        }
        index++;
        err = GetIndVolume(index, &vRefNum);
        if (err == nsvErr) return fnfErr;
        if (err != noErr) return err;
    }

    return NS_OK;
}

#define aeSelectionKeyword  'fsel'
#define kAEOpenSelection    'sope'
#define kAERevealSelection  'srev'
#define kFinderType         'FNDR'

NS_IMETHODIMP nsLocalFile::Launch()
{
  AppleEvent        aeEvent = {0, nil};
  AppleEvent        aeReply = {0, nil};
  StAEDesc          aeDirDesc, listElem, myAddressDesc, fileList;
  FSSpec                dirSpec, appSpec;
  AliasHandle           DirAlias, FileAlias;
  OSErr             errorResult = noErr;
  ProcessSerialNumber   process;
  
  // for launching a file, we'll use mTargetSpec (which is both a resolved spec and a resolved alias)
  ResolveAndStat();
    
#if TARGET_CARBON
    if (sRunningOSX)
    { // We're running under Mac OS X, LaunchServices here we come

        // First we make sure the LaunchServices routine we want is implemented
        if ( (UInt32)LSOpenFSRef != (UInt32)kUnresolvedCFragSymbolAddress )
        {
            FSRef   theRef;
            if (::FSpMakeFSRef(&mTargetSpec, &theRef) == noErr)
            {
                (void)::LSOpenFSRef(&theRef, NULL);
            }
        }
    }
    else
#endif
    { // We're running under Mac OS 8.x/9.x, use the Finder Luke
  nsresult rv = FindRunningAppBySignature ('MACS', appSpec, process);
  if (NS_SUCCEEDED(rv))
  { 
    errorResult = AECreateDesc(typeProcessSerialNumber, (Ptr)&process, sizeof(process), &myAddressDesc);
    if (errorResult == noErr)
    {
      /* Create the FinderEvent */
      errorResult = AECreateAppleEvent(kFinderType, kAEOpenSelection, &myAddressDesc, kAutoGenerateReturnID, kAnyTransactionID,
                                       &aeEvent);   
      if (errorResult == noErr) 
      {
        errorResult = FSMakeFSSpec(mTargetSpec.vRefNum, mTargetSpec.parID, nil, &dirSpec);
        NewAlias(nil, &dirSpec, &DirAlias);
        /* Create alias for file */
        NewAlias(nil, &mTargetSpec, &FileAlias);
        
        /* Create the file  list */
         errorResult = AECreateList(nil, 0, false, &fileList);
        /*  create the folder  descriptor */
        HLock((Handle)DirAlias);
        errorResult = AECreateDesc(typeAlias, (Ptr)*DirAlias, GetHandleSize((Handle)DirAlias), &aeDirDesc);
        HUnlock((Handle)DirAlias);
        if (errorResult == noErr)
        {
          errorResult = AEPutParamDesc(&aeEvent, keyDirectObject, &aeDirDesc);
          if ( errorResult == noErr) 
          {
            /*  create the file descriptor and add to aliasList */
            HLock((Handle)FileAlias);
            errorResult = AECreateDesc(typeAlias, (Ptr)*FileAlias, GetHandleSize((Handle)FileAlias), &listElem);
            HLock((Handle)FileAlias);
            if (errorResult == noErr)
            {
              errorResult = AEPutDesc(&fileList, 0, &listElem);
              if (errorResult == noErr)
              {
                /* Add the file alias list to the event */
                errorResult = AEPutParamDesc(&aeEvent, aeSelectionKeyword, &fileList);
                if (errorResult == noErr)
                    AESend(&aeEvent, &aeReply, kAEWaitReply + kAENeverInteract 
                                  + kAECanSwitchLayer, kAEHighPriority, kAEDefaultTimeout, nil, nil);
              }
            }
          }
        }
      }
    }
  }
    }
                      
    return NS_OK;
}

NS_IMETHODIMP nsLocalFile::Reveal()
{
  FSSpec            specToReveal;
  AppleEvent        aeEvent = {0, nil};
  AppleEvent        aeReply = {0, nil};
  StAEDesc          aeDirDesc, listElem, myAddressDesc, fileList;
  OSErr             errorResult = noErr;
  ProcessSerialNumber   process;
  FSSpec appSpec;
    
  nsresult rv = ResolveAndStat();
  if (NS_FAILED(rv))
    return rv;
  rv = GetFSSpec(&specToReveal); // Pay attention to followLinks
  if (NS_FAILED(rv))
    return rv;
  
  rv = FindRunningAppBySignature ('MACS', appSpec, process);
  if (NS_SUCCEEDED(rv))
  { 
    errorResult = AECreateDesc(typeProcessSerialNumber, (Ptr)&process, sizeof(process), &myAddressDesc);
    if (errorResult == noErr)
    {
      /* Create the FinderEvent */
#if TARGET_CARBON
    // The Finder under OS X uses a different event to reveal
    if (sRunningOSX)
      errorResult = AECreateAppleEvent(kAEMiscStandards, kAEMakeObjectsVisible, &myAddressDesc, kAutoGenerateReturnID, kAnyTransactionID,
                                       &aeEvent);   
    else
#endif
      errorResult = AECreateAppleEvent(kFinderType, kAERevealSelection, &myAddressDesc, kAutoGenerateReturnID, kAnyTransactionID,
                                       &aeEvent);   
      if (errorResult == noErr) 
      {
        /* Create the file  list */
        errorResult = AECreateList(nil, 0, false, &fileList);
        if (errorResult == noErr) 
        {
          errorResult = AEPutPtr(&fileList, 0, typeFSS, &specToReveal, sizeof(FSSpec));
          
          if (errorResult == noErr)
          {
#if TARGET_CARBON
            // When we're sending the event under OS X the FSSpec must be a keyDirectObject
            if (sRunningOSX)
              errorResult = AEPutParamDesc(&aeEvent, keyDirectObject, &fileList);
            else
#endif
            errorResult = AEPutParamDesc(&aeEvent,keySelection, &fileList);

            if (errorResult == noErr)
            {
              errorResult = AESend(&aeEvent, &aeReply, kAENoReply, kAENormalPriority, kAEDefaultTimeout, nil, nil);
              if (errorResult == noErr)
                SetFrontProcess(&process);
            }
          }
        }
      }
    }
  }
    
  return NS_OK;
}

nsresult nsLocalFile::MyLaunchAppWithDoc(const FSSpec& appSpec, const FSSpec* aDocToLoad, PRBool aLaunchInBackground)
{
    ProcessSerialNumber thePSN = {0};
    StAEDesc            target;
    StAEDesc            docDesc;
    StAEDesc            launchDesc;
    StAEDesc            docList;
    AppleEvent      theEvent = {0, nil};
    AppleEvent      theReply = {0, nil};
    OSErr               err = noErr;
    Boolean             autoParamValue = false;
    Boolean             running = false;
    nsresult            rv = NS_OK;
    
#if TARGET_CARBON
    if (sRunningOSX)
    { // Under Mac OS X we'll use LaunchServices
        
        // First we make sure the LaunchServices routine we want is implemented
        if ( (UInt32)LSOpenFromRefSpec != (UInt32)kUnresolvedCFragSymbolAddress )
        {
            FSRef   appRef;
            FSRef   docRef;
            LSLaunchFlags       theLaunchFlags = kLSLaunchDefaults;
            LSLaunchFSRefSpec   thelaunchSpec;
            
            if (::FSpMakeFSRef(&appSpec, &appRef) != noErr)
                return NS_ERROR_FAILURE;
            
            if (aDocToLoad)
                if (::FSpMakeFSRef(aDocToLoad, &docRef) != noErr)
                    return NS_ERROR_FAILURE;
            
            if (aLaunchInBackground)
                theLaunchFlags |= kLSLaunchDontSwitch;
                
            memset(&thelaunchSpec, 0, sizeof(LSLaunchFSRefSpec));
            
            thelaunchSpec.appRef = &appRef;
            if (aDocToLoad)
            {
                thelaunchSpec.numDocs = 1;
                thelaunchSpec.itemRefs = &docRef;
            }
            thelaunchSpec.launchFlags = theLaunchFlags;
            
            err = ::LSOpenFromRefSpec(&thelaunchSpec, NULL);
            NS_ASSERTION((err != noErr), "Error calling LSOpenFromRefSpec");
            if (err != noErr) return NS_ERROR_FAILURE;
        }
    }
    else
#endif
    { // The old fashioned way for Mac OS 8.x/9.x
    rv = FindRunningAppByFSSpec(appSpec, thePSN);
    running = NS_SUCCEEDED(rv);
    
    err = AECreateDesc(typeProcessSerialNumber, &thePSN, sizeof(thePSN), &target); 
    if (err != noErr) return NS_ERROR_FAILURE;
    
    err = AECreateAppleEvent(kCoreEventClass, aDocToLoad ? kAEOpenDocuments : kAEOpenApplication, &target,
                            kAutoGenerateReturnID, kAnyTransactionID, &theEvent);
    if (err != noErr) return NS_ERROR_FAILURE;
    
    if (aDocToLoad)
    {
        err = AECreateList(nil, 0, false, &docList);
        if (err != noErr) return NS_ERROR_FAILURE;
        
        err = AECreateDesc(typeFSS, aDocToLoad, sizeof(FSSpec), &docDesc);
        if (err != noErr) return NS_ERROR_FAILURE;
        
        err = AEPutDesc(&docList, 0, &docDesc);
        if (err != noErr) return NS_ERROR_FAILURE;
        
        err = AEPutParamDesc(&theEvent, keyDirectObject, &docList);
        if (err != noErr) return NS_ERROR_FAILURE;
    }
    
    if (running)
    {
        err = AESend(&theEvent, &theReply, kAENoReply, kAENormalPriority, kNoTimeOut, nil, nil);
        if (err != noErr) return NS_ERROR_FAILURE;
        
        if (!aLaunchInBackground)
        {
            err = ::SetFrontProcess(&thePSN);
            if (err != noErr) return NS_ERROR_FAILURE;
        }
    }
    else
    {
        LaunchParamBlockRec launchThis = {0};
        PRUint16                        launchControlFlags = (launchContinue | launchNoFileFlags);
        if (aLaunchInBackground)
            launchControlFlags |= launchDontSwitch;
        
        err = AECoerceDesc(&theEvent, typeAppParameters, &launchDesc);
        if (err != noErr) return NS_ERROR_FAILURE;

        launchThis.launchAppSpec = (FSSpecPtr)&appSpec;
#if TARGET_CARBON && ACCESSOR_CALLS_ARE_FUNCTIONS
        ::AEGetDescData(&launchDesc, &launchThis.launchAppParameters, sizeof(launchThis.launchAppParameters));
#else
        // no need to lock this handle.
            launchThis.launchAppParameters = (AppParametersPtr) *(launchDesc.dataHandle);
#endif
        launchThis.launchBlockID = extendedBlock;
        launchThis.launchEPBLength = extendedBlockLen;
        launchThis.launchFileFlags = 0;
        launchThis.launchControlFlags = launchControlFlags;
        err = ::LaunchApplication(&launchThis);
        if (err != noErr) return NS_ERROR_FAILURE;
        
        // let's be nice and wait until it's running
        const PRUint32  kMaxTimeToWait = 60;        // wait 1 sec max
        PRUint32                endTicks = ::TickCount() + kMaxTimeToWait;

        PRBool                  foundApp = PR_FALSE;
        
        do
        {
            EventRecord     theEvent;
            (void)WaitNextEvent(nullEvent, &theEvent, 1, NULL);

            ProcessSerialNumber     psn;
            foundApp = NS_SUCCEEDED(FindRunningAppByFSSpec(appSpec, psn));
        
        } while (!foundApp && (::TickCount() <= endTicks));

        NS_ASSERTION(foundApp, "Failed to find app after launching it");
    }
    
    if (theEvent.dataHandle != nil) AEDisposeDesc(&theEvent);
    if (theReply.dataHandle != nil) AEDisposeDesc(&theReply);
    }

    return NS_OK;
}


#pragma mark -
#pragma mark [Methods that will not be implemented on Mac]

NS_IMETHODIMP
nsLocalFile::Normalize()
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsLocalFile::GetPermissions(PRUint32 *aPermissions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsLocalFile::GetPermissionsOfLink(PRUint32 *aPermissionsOfLink)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP  
nsLocalFile::SetPermissions(PRUint32 aPermissions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsLocalFile::SetPermissionsOfLink(PRUint32 aPermissions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP  
nsLocalFile::IsSpecial(PRBool *_retval)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

#pragma mark -
#pragma mark [nsILocalFileMac]
// Implementation of Mac specific finctions from nsILocalFileMac


NS_IMETHODIMP nsLocalFile::InitWithCFURL(CFURLRef aCFURL)
{
    nsresult rv = NS_ERROR_FAILURE;
    
#if TARGET_CARBON
    NS_ENSURE_ARG(aCFURL);
    
    // CFURLGetFSRef can only succeed if the entire path exists.
    FSRef fsRef;
    if (::CFURLGetFSRef(aCFURL, &fsRef) == PR_TRUE)
        rv = InitWithFSRef(&fsRef);
    else
    {
      CFURLRef parentURL = ::CFURLCreateCopyDeletingLastPathComponent(NULL, aCFURL);
      if (!parentURL)
        return NS_ERROR_FAILURE;

      // Get the FSRef from the parent and the FSSpec from that
      FSRef parentFSRef;
      FSSpec parentFSSpec;
      if ((::CFURLGetFSRef(parentURL, &parentFSRef) == PR_TRUE) &&
          (::FSGetCatalogInfo(&parentFSRef, kFSCatInfoNone,
                             nsnull, nsnull, &parentFSSpec, nsnull) == noErr))
      {
        // Get the leaf name of the file and turn it into a string HFS can use.
        CFStringRef fileNameRef = ::CFURLCopyLastPathComponent(aCFURL);
        if (fileNameRef)
        {
          TextEncoding theEncoding;
          if (::UpgradeScriptInfoToTextEncoding(smSystemScript, 
                                                kTextLanguageDontCare,
                                                kTextRegionDontCare,
                                                NULL,
                                                &theEncoding) != noErr)
            theEncoding = kTextEncodingMacRoman;

          char  origName[256];
          char  truncBuf[32];
          if (::CFStringGetCString(fileNameRef, origName, sizeof(origName), theEncoding))
          {
            MakeDirty();
            mSpec = parentFSSpec;
            mAppendedPath = NS_TruncNodeName(origName, truncBuf);
            rv = NS_OK;
          }
          ::CFRelease(fileNameRef);
        }
      }
      ::CFRelease(parentURL);
    }
#endif

    return rv;
}


NS_IMETHODIMP nsLocalFile::InitWithFSRef(const FSRef * aFSRef)
{
    nsresult rv = NS_ERROR_FAILURE;

#if TARGET_CARBON
    NS_ENSURE_ARG(aFSRef);
    
    FSSpec fsSpec;
    OSErr err = ::FSGetCatalogInfo(aFSRef, kFSCatInfoNone, nsnull,
                                   nsnull, &fsSpec, nsnull);
    if (err == noErr)
        rv = InitWithFSSpec(&fsSpec);
    else
        rv = MacErrorMapper(err);
#endif

    return rv;
}


NS_IMETHODIMP nsLocalFile::InitWithFSSpec(const FSSpec *fileSpec)
{
    MakeDirty();
    mSpec          = *fileSpec;
    mTargetSpec    = *fileSpec;
    mAppendedPath  = "";
    return NS_OK;
}


NS_IMETHODIMP nsLocalFile::InitToAppWithCreatorCode(OSType aAppCreator)
{
    FSSpec              appSpec;
    ProcessSerialNumber psn;
    
#if TARGET_CARBON
    if (sRunningOSX)
    { // If we're running under OS X use LaunchServices to determine the app
      // corresponding to the creator code
        if ( (UInt32)LSFindApplicationForInfo != (UInt32)kUnresolvedCFragSymbolAddress )
        {
            FSRef    theRef;
            if (::LSFindApplicationForInfo(aAppCreator, NULL, NULL, &theRef, NULL) == noErr)
            {
                FSCatalogInfoBitmap   whichInfo = kFSCatInfoNone;
                
                if (::FSGetCatalogInfo(&theRef, whichInfo, NULL, NULL, &appSpec, NULL) == noErr)
                    return InitWithFSSpec(&appSpec);
            }
            
            // If we get here we didn't find an app
            return NS_ERROR_FILE_NOT_FOUND;
        }
    }
#endif

    // is the app running?
    nsresult rv = FindRunningAppBySignature(aAppCreator, appSpec, psn);
    if (rv == NS_ERROR_FILE_NOT_FOUND)
    {
        // we have to look on disk
        rv = FindAppOnLocalVolumes(aAppCreator, appSpec);
        if (NS_FAILED(rv)) return rv;
    }
    else if (NS_FAILED(rv))
        return rv;

    // init with the spec here
    return InitWithFSSpec(&appSpec);
}

NS_IMETHODIMP nsLocalFile::GetCFURL(CFURLRef *_retval)
{
    nsresult rv = NS_ERROR_FAILURE;

#if TARGET_CARBON
    NS_ENSURE_ARG_POINTER(_retval);
    *_retval = nsnull;
    
    PRBool exists;
    if (NS_SUCCEEDED(Exists(&exists)) && exists)
    {
        FSRef fsRef;
        FSSpec fsSpec = mFollowLinks ? mTargetSpec : mSpec;
        if (::FSpMakeFSRef(&fsSpec, &fsRef) == noErr)
        {
            *_retval = ::CFURLCreateFromFSRef(NULL, &fsRef);
            if (*_retval)
                return NS_OK;    
        }
    }
    else
    {
        nsCAutoString tempPath;
        if (NS_SUCCEEDED(GetNativePath(tempPath)))
        {
            CFStringRef pathStrRef = ::CFStringCreateWithCString(NULL, tempPath.get(), kCFStringEncodingMacRoman);
            if (!pathStrRef)
                return NS_ERROR_FAILURE;
            *_retval = ::CFURLCreateWithFileSystemPath(NULL, pathStrRef, kCFURLHFSPathStyle, false);
            ::CFRelease(pathStrRef);
            if (*_retval)
                return NS_OK;    
        }
    }
#endif

    return rv;
}

NS_IMETHODIMP nsLocalFile::GetFSRef(FSRef *_retval)
{
    nsresult rv = NS_ERROR_FAILURE;
  
#if TARGET_CARBON
    NS_ENSURE_ARG_POINTER(_retval);
    
    FSSpec fsSpec;
    rv = GetFSSpec(&fsSpec);
    if (NS_SUCCEEDED(rv))
        rv = MacErrorMapper(::FSpMakeFSRef(&fsSpec, _retval));
#endif

    return rv;
}

NS_IMETHODIMP nsLocalFile::GetFSSpec(FSSpec *fileSpec)
{
    NS_ENSURE_ARG(fileSpec);
    nsresult rv = ResolveAndStat();
    if (rv == NS_ERROR_FILE_NOT_FOUND)
        rv = NS_OK;
    if (NS_SUCCEEDED(rv))
        *fileSpec = mFollowLinks ? mTargetSpec : mSpec;
    
    return NS_OK;
}

NS_IMETHODIMP nsLocalFile::GetFileType(OSType *aFileType)
{
    NS_ENSURE_ARG(aFileType);

    FSSpec fileSpec;
    (void)GetFSSpec(&fileSpec); 
    
    FInfo info;
    OSErr err = ::FSpGetFInfo(&fileSpec, &info);
    if (err != noErr)
    {
        *aFileType = mType;
        return NS_ERROR_FILE_NOT_FOUND;
    }
    
    *aFileType = info.fdType;
    
    return NS_OK;
}

NS_IMETHODIMP nsLocalFile::SetFileType(OSType aFileType)
{
    mType = aFileType;

    FSSpec fileSpec;
    (void)GetFSSpec(&fileSpec); 
    
    FInfo info;
    OSErr err = ::FSpGetFInfo(&fileSpec, &info);
    if (err != noErr)
        return NS_ERROR_FILE_NOT_FOUND;
            
    info.fdType = aFileType;    
    err = ::FSpSetFInfo(&fileSpec, &info);
    if (err != noErr)
        return NS_ERROR_FILE_ACCESS_DENIED;
    
    return NS_OK;
}

NS_IMETHODIMP nsLocalFile::GetFileCreator(OSType *aCreator)
{
    NS_ENSURE_ARG(aCreator);
    
    FSSpec fileSpec;
    (void)GetFSSpec(&fileSpec); 
    
    FInfo info;
    OSErr err = ::FSpGetFInfo(&fileSpec, &info);
    if (err != noErr)
    {
        *aCreator = mCreator;
        return NS_ERROR_FILE_NOT_FOUND;
    }
    
    *aCreator = info.fdCreator; 
    
    return NS_OK;
}

NS_IMETHODIMP nsLocalFile::SetFileCreator(OSType aCreator)
{
    if (aCreator == CURRENT_PROCESS_CREATOR)
        aCreator = sCurrentProcessSignature;
        
    mCreator = aCreator;

    FSSpec fileSpec;
    (void)GetFSSpec(&fileSpec); 
    
    FInfo info;
    OSErr err = ::FSpGetFInfo(&fileSpec, &info);
    if (err != noErr)
        return NS_ERROR_FILE_NOT_FOUND;
            
    info.fdCreator = aCreator;  
    err = ::FSpSetFInfo(&fileSpec, &info);
    if (err != noErr)
        return NS_ERROR_FILE_ACCESS_DENIED;
    
    return NS_OK;
}

NS_IMETHODIMP nsLocalFile::SetFileTypeAndCreatorFromExtension(const char *aExtension)
{
    NS_ENSURE_ARG(aExtension);
    return SetOSTypeAndCreatorFromExtension(aExtension);
}

NS_IMETHODIMP nsLocalFile::SetFileTypeAndCreatorFromMIMEType(const char *aMIMEType)
{
    NS_ENSURE_ARG(aMIMEType);

    nsresult rv;
    nsCOMPtr<nsIInternetConfigService> icService(do_GetService
        (NS_INTERNETCONFIGSERVICE_CONTRACTID, &rv));
        
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIMIMEInfo> mimeInfo;
        PRUint32 fileType = 'TEXT';
        PRUint32 fileCreator = nsILocalFileMac::CURRENT_PROCESS_CREATOR;
    
        rv = icService->FillInMIMEInfo(aMIMEType, 
                nsnull, getter_AddRefs(mimeInfo));
        if (NS_SUCCEEDED(rv))
            rv = mimeInfo->GetMacType(&fileType);
        if (NS_SUCCEEDED(rv))
            rv = mimeInfo->GetMacCreator(&fileCreator);
        if (NS_SUCCEEDED(rv))
            rv = SetFileType(fileType);
        if (NS_SUCCEEDED(rv))
            rv  = SetFileCreator(fileCreator);    
    }
    
    return rv;
}

NS_IMETHODIMP  
nsLocalFile::GetFileSizeWithResFork(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);
    
    *aFileSize = LL_Zero();

    ResolveAndStat();
    
    long dataSize = 0;
    long resSize = 0;
    
    OSErr err = FSpGetFileSize(&mTargetSpec, &dataSize, &resSize);
                               
    if (err != noErr)
        return MacErrorMapper(err);
    
    // For now we've only got 32 bits of file size info
    PRInt64 dataInt64 = LL_Zero();
    PRInt64 resInt64 = LL_Zero();
    
    // Combine the size of the resource and data forks
    LL_I2L(resInt64, resSize);
    LL_I2L(dataInt64, dataSize);
    LL_ADD((*aFileSize), dataInt64, resInt64);
    
    return NS_OK;
}


// this nsLocalFile points to the app. We want to launch it, optionally with the document.
NS_IMETHODIMP
nsLocalFile::LaunchWithDoc(nsILocalFile* aDocToLoad, PRBool aLaunchInBackground)
{
    // are we launchable?
    PRBool isExecutable;
    nsresult rv = IsExecutable(&isExecutable);
    if (NS_FAILED(rv)) return rv;
    if (!isExecutable) return NS_ERROR_FILE_EXECUTION_FAILED;
    
    FSSpec          docSpec;
    FSSpecPtr       docSpecPtr = nsnull;
    
    nsCOMPtr<nsILocalFileMac> macDoc = do_QueryInterface(aDocToLoad);
    if (macDoc)
    {
        rv = macDoc->GetFSSpec(&docSpec); // XXX GetTargetFSSpec
        if (NS_FAILED(rv)) return rv;
        
        docSpecPtr = &docSpec;
    }
    
    FSSpec  appSpec;
    rv = GetFSSpec(&appSpec); // XXX GetResolvedFSSpec
    if (NS_FAILED(rv)) return rv;
    
    rv = MyLaunchAppWithDoc(appSpec, docSpecPtr, aLaunchInBackground);
    return rv;
}


NS_IMETHODIMP
nsLocalFile::OpenDocWithApp(nsILocalFile* aAppToOpenWith, PRBool aLaunchInBackground)
{
    // if aAppToOpenWith is nil, we have to find the app from the creator code
    // of the document
    nsresult rv = NS_OK;
    
    FSSpec      appSpec;
    
    if (aAppToOpenWith)
    {
        nsCOMPtr<nsILocalFileMac> appFileMac = do_QueryInterface(aAppToOpenWith, &rv);
        if (!appFileMac) return rv;
    
        rv = appFileMac->GetFSSpec(&appSpec); // XXX GetTargetFSSpec
        if (NS_FAILED(rv)) return rv;

        // is it launchable?
        PRBool isExecutable;
        rv = aAppToOpenWith->IsExecutable(&isExecutable);
        if (NS_FAILED(rv)) return rv;
        if (!isExecutable) return NS_ERROR_FILE_EXECUTION_FAILED;
    }
    else
    {
        // look for one
        OSType  fileCreator;
        rv = GetFileCreator(&fileCreator);
        if (NS_FAILED(rv)) return rv;
        
        // just make one on the stack
        nsLocalFile localAppFile;
        rv = localAppFile.InitToAppWithCreatorCode(fileCreator);
        if (NS_FAILED(rv)) return rv;
        
        rv = localAppFile.GetFSSpec(&appSpec); // GetTargetFSSpec
        if (NS_FAILED(rv)) return rv;
    }
    
    FSSpec      docSpec;
    rv = GetFSSpec(&docSpec); // XXX GetResolvedFSSpec
    if (NS_FAILED(rv)) return rv;

    rv = MyLaunchAppWithDoc(appSpec, &docSpec, aLaunchInBackground);
    return rv;
}

nsresult nsLocalFile::SetOSTypeAndCreatorFromExtension(const char* extension)
{
    nsresult rv;
    
    nsCAutoString localExtBuf;
    const char *extPtr;
    
    if (!extension)
    {
        rv = GetNativeLeafName(localExtBuf);
        extPtr = strrchr(localExtBuf.get(), '.');
        if (!extPtr)
            return NS_ERROR_FAILURE;
        ++extPtr;   
    }
    else
    {
        extPtr = extension;
        if (*extPtr == '.')
            ++extPtr;
    }
    
    nsCOMPtr<nsIInternetConfigService> icService = 
             do_GetService(NS_INTERNETCONFIGSERVICE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
        nsCOMPtr<nsIMIMEInfo> mimeInfo;
        rv = icService->GetMIMEInfoFromExtension(extPtr, getter_AddRefs(mimeInfo));
        if (NS_SUCCEEDED(rv))
        {
            PRUint32 osType;
            rv = mimeInfo->GetMacType(&osType);
            if (NS_SUCCEEDED(rv))
                mType = osType;
            PRBool skip;
            rv = ExtensionIsOnExceptionList(extPtr, &skip);
            if (NS_SUCCEEDED(rv) && !skip)
            {
                rv = mimeInfo->GetMacCreator(&osType);
                if (NS_SUCCEEDED(rv))
                    mCreator = osType;
            }
        }
    }
    return rv;
}

nsresult nsLocalFile::ExtensionIsOnExceptionList(const char *extension, PRBool *onList)
{
    // Probably want to make a global list somewhere in the future
    // for now, just check for "html" and "htm"
    
    *onList = PR_FALSE;
    
    if (!nsCRT::strcasecmp(extension, "html") ||
        !nsCRT::strcasecmp(extension, "htm"))
        *onList = PR_TRUE;
    return NS_OK;
}


void nsLocalFile::InitClassStatics()
{
    OSErr err;
    

    if (sCurrentProcessSignature == 0)
    {
        ProcessSerialNumber psn;
        ProcessInfoRec  info;
        
        psn.highLongOfPSN = 0;
        psn.lowLongOfPSN  = kCurrentProcess;

        info.processInfoLength = sizeof(ProcessInfoRec);
        info.processName = nil;
        info.processAppSpec = nil;
        err = ::GetProcessInformation(&psn, &info);
        if (err == noErr)
            sCurrentProcessSignature = info.processSignature;
        // Try again next time if error
    }
    
    static PRBool didHFSPlusCheck = PR_FALSE;
    if (!didHFSPlusCheck)
    {
        long response;
        err = ::Gestalt(gestaltFSAttr, &response);
        sHasHFSPlusAPIs = (err == noErr && (response & (1 << gestaltHasHFSPlusAPIs)) != 0);
        didHFSPlusCheck = PR_TRUE;
    }
    
    static PRBool didOSXCheck = PR_FALSE;
    if (!didOSXCheck)
    {
        long version;
        sRunningOSX = (::Gestalt(gestaltSystemVersion, &version) == noErr && version >= 0x00001000);
        didOSXCheck = PR_TRUE;
    }
}


#pragma mark -

// Handy dandy utility create routine for something or the other
nsresult 
NS_NewNativeLocalFile(const nsACString &path, PRBool followLinks, nsILocalFile* *result)
{
    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

    file->SetFollowLinks(followLinks);

    if (!path.IsEmpty()) {
        nsresult rv = file->InitWithNativePath(path);
        if (NS_FAILED(rv)) {
            NS_RELEASE(file);
            return rv;
        }
    }
    *result = file;
    return NS_OK;
}

nsresult 
NS_NewLocalFile(const nsAString &path, PRBool followLinks, nsILocalFile* *result)
{
    nsCAutoString fsCharSetStr;   
    nsresult rv = NS_CopyUnicodeToNative(path, fsCharSetStr);
    if (NS_FAILED(rv))
        return rv;
    return NS_NewNativeLocalFile(fsCharSetStr, followLinks, result);
}

nsresult 
NS_NewLocalFileWithFSSpec(const FSSpec* inSpec, PRBool followLinks, nsILocalFileMac* *result)
{
    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

    file->SetFollowLinks(followLinks);

    nsresult rv = file->InitWithFSSpec(inSpec);
    if (NS_FAILED(rv)) {
        NS_RELEASE(file);
        return rv;
    }
    *result = file;
    return NS_OK;
}

void
nsLocalFile::GlobalInit()
{
}

void
nsLocalFile::GlobalShutdown()
{
}
