/*
	File:		MoreFilesX.c

	Contains:	A collection of useful high-level File Manager routines
				which use the HFS Plus APIs wherever possible.

	Version:	MoreFilesX 1.0.1

	Copyright:	© 1992-2002 by Apple Computer, Inc., all rights reserved.

	Disclaimer:	IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc.
				("Apple") in consideration of your agreement to the following terms, and your
				use, installation, modification or redistribution of this Apple software
				constitutes acceptance of these terms.  If you do not agree with these terms,
				please do not use, install, modify or redistribute this Apple software.

				In consideration of your agreement to abide by the following terms, and subject
				to these terms, Apple grants you a personal, non-exclusive license, under Apple’s
				copyrights in this original Apple software (the "Apple Software"), to use,
				reproduce, modify and redistribute the Apple Software, with or without
				modifications, in source and/or binary forms; provided that if you redistribute
				the Apple Software in its entirety and without modifications, you must retain
				this notice and the following text and disclaimers in all such redistributions of
				the Apple Software.  Neither the name, trademarks, service marks or logos of
				Apple Computer, Inc. may be used to endorse or promote products derived from the
				Apple Software without specific prior written permission from Apple.  Except as
				expressly stated in this notice, no other rights or licenses, express or implied,
				are granted by Apple herein, including but not limited to any patent rights that
				may be infringed by your derivative works or by other works in which the Apple
				Software may be incorporated.

				The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO
				WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED
				WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS FOR A PARTICULAR
				PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND OPERATION ALONE OR IN
				COMBINATION WITH YOUR PRODUCTS.

				IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR
				CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
				GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
				ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION
				OF THE APPLE SOFTWARE, HOWEVER CAUSED AND WHETHER UNDER THEORY OF CONTRACT, TORT
				(INCLUDING NEGLIGENCE), STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN
				ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

	File Ownership:

		DRI:				Apple Macintosh Developer Technical Support

		Other Contact:		For bug reports, consult the following page on
							the World Wide Web:
								http://developer.apple.com/bugreporter/

		Technology:			DTS Sample Code

	Writers:

		(JL)	Jim Luther

	Change History (most recent first):

		 <4>	 8/22/02	JL		[3016251]  Changed FSMoveRenameObjectUnicode to not use
		 							the Temporary folder because it isn't available on
		 							NFS volumes.
		 <3>	 4/19/02	JL		[2853905]  Fixed #if test around header includes.
		 <2>	 4/19/02	JL		[2850624]  Fixed C++ compile errors and Project Builder
		 							warnings.
		 <2>	 4/19/02	JL		[2853901]  Updated standard disclaimer.
		 <1>	 1/25/02	JL		MoreFilesX 1.0
*/

#if defined(__MACH__)
	#include <Carbon/Carbon.h>
	#include <string.h>
#else
	#include <Carbon.h>
	#include <string.h>
#endif

#include "MoreFilesX.h"

/* Set BuildingMoreFilesXForMacOS9 to 1 if building for Mac OS 9 */
#ifndef BuildingMoreFilesXForMacOS9
	#define BuildingMoreFilesXForMacOS9 0
#endif

/*****************************************************************************/

#pragma mark ----- Local type definitions -----

struct FSIterateContainerGlobals
{
	IterateContainerFilterProcPtr	iterateFilter;	/* pointer to IterateFilterProc */
	FSCatalogInfoBitmap				whichInfo;		/* fields of the CatalogInfo to get */
	FSCatalogInfo					catalogInfo;	/* FSCatalogInfo */
	FSRef							ref;			/* FSRef */
	FSSpec							spec;			/* FSSpec */
	FSSpec							*specPtr;		/* pointer to spec field, or NULL */
	HFSUniStr255					name;			/* HFSUniStr255 */
	HFSUniStr255					*namePtr;		/* pointer to name field, or NULL */
	void							*yourDataPtr;	/* a pointer to caller supplied data the filter may need to access */
	ItemCount						maxLevels;		/* maximum levels to iterate through */
	ItemCount						currentLevel;	/* the current level FSIterateContainerLevel is on */
	Boolean							quitFlag;		/* set to true if filter wants to kill interation */
	Boolean							containerChanged; /* temporary - set to true if the current container changed during iteration */
	OSErr							result;			/* result */
	ItemCount						actualObjects;	/* number of objects returned */
};
typedef struct FSIterateContainerGlobals FSIterateContainerGlobals;

struct FSDeleteContainerGlobals
{
	OSErr							result;			/* result */
	ItemCount						actualObjects;	/* number of objects returned */
	FSCatalogInfo					catalogInfo;	/* FSCatalogInfo */
};
typedef struct FSDeleteContainerGlobals FSDeleteContainerGlobals;

/*****************************************************************************/

#pragma mark ----- Local prototypes -----

static
void
FSDeleteContainerLevel(
	const FSRef *container,
	FSDeleteContainerGlobals *theGlobals);

static
void
FSIterateContainerLevel(
	FSIterateContainerGlobals *theGlobals);

static
OSErr
GenerateUniqueHFSUniStr(
	long *startSeed,
	const FSRef *dir1,
	const FSRef *dir2,
	HFSUniStr255 *uniqueName);

/*****************************************************************************/

#pragma mark ----- File Access Routines -----

/*****************************************************************************/

OSErr
FSCopyFork(
	SInt16 srcRefNum,
	SInt16 dstRefNum,
	void *copyBufferPtr,
	ByteCount copyBufferSize)
{
	OSErr		srcResult;
	OSErr		dstResult;
	OSErr		result;
	SInt64		forkSize;
	ByteCount	readActualCount;
	
	/* check input parameters */
	require_action((NULL != copyBufferPtr) && (0 != copyBufferSize), BadParameter, result = paramErr);
	
	/* get source fork size */
	result = FSGetForkSize(srcRefNum, &forkSize);
	require_noerr(result, SourceFSGetForkSizeFailed);
	
	/* allocate disk space for destination fork */
	result = FSSetForkSize(dstRefNum, fsFromStart, forkSize);
	require_noerr(result, DestinationFSSetForkSizeFailed);
	
	/* reset source fork's position to 0 */
	result = FSSetForkPosition(srcRefNum, fsFromStart, 0);
	require_noerr(result, SourceFSSetForkPositionFailed);
	
	/* reset destination fork's position to 0 */
	result = FSSetForkPosition(dstRefNum, fsFromStart, 0);
	require_noerr(result, DestinationFSSetForkPositionFailed);

	/* If copyBufferSize is greater than 4K bytes, make it a multiple of 4k bytes */
	/* This will make writes on local volumes faster */
	if ( (copyBufferSize >= 0x00001000) && ((copyBufferSize & 0x00000fff) != 0) )
	{
		copyBufferSize &= ~(0x00001000 - 1);
	}
	
	/* copy source to destination */
	srcResult = dstResult = noErr;
	while ( (noErr == srcResult) && (noErr == dstResult) )
	{
		srcResult = FSReadFork(srcRefNum, fsAtMark + noCacheMask, 0, copyBufferSize, copyBufferPtr, &readActualCount);
		dstResult = FSWriteFork(dstRefNum, fsAtMark + noCacheMask, 0, readActualCount, copyBufferPtr, NULL);
	}
	
	/* make sure there were no errors at the destination */
	require_noerr_action(dstResult, DestinationFSWriteForkFailed, result = dstResult);
	
	/* make sure the error at the source was eofErr */
	require_action(eofErr == srcResult, SourceResultNotEofErr, result = srcResult);
	
	/* everything went as expected */
	result = noErr;

SourceResultNotEofErr:
DestinationFSWriteForkFailed:
DestinationFSSetForkPositionFailed:
SourceFSSetForkPositionFailed:
DestinationFSSetForkSizeFailed:
SourceFSGetForkSizeFailed:
BadParameter:

	return ( result );
}

/*****************************************************************************/

#pragma mark ----- Volume Access Routines -----

/*****************************************************************************/ 

OSErr
FSGetVolParms(
	FSVolumeRefNum volRefNum,
	UInt32 bufferSize,
	GetVolParmsInfoBuffer *volParmsInfo,
	UInt32 *actualInfoSize)
{
	OSErr			result;
	HParamBlockRec	pb;
	
	/* check parameters */
	require_action((NULL != volParmsInfo) && (NULL != actualInfoSize),
		BadParameter, result = paramErr);
	
	pb.ioParam.ioNamePtr = NULL;
	pb.ioParam.ioVRefNum = volRefNum;
	pb.ioParam.ioBuffer = (Ptr)volParmsInfo;
	pb.ioParam.ioReqCount = (SInt32)bufferSize;
	result = PBHGetVolParmsSync(&pb);
	require_noerr(result, PBHGetVolParmsSync);
	
	/* return number of bytes the file system returned in volParmsInfo buffer */
	*actualInfoSize = (UInt32)pb.ioParam.ioActCount;
	
PBHGetVolParmsSync:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetVRefNum(
	const FSRef *ref,
	FSVolumeRefNum *vRefNum)
{
	OSErr			result;
	FSCatalogInfo	catalogInfo;
	
	/* check parameters */
	require_action(NULL != vRefNum, BadParameter, result = paramErr);
	
	/* get the volume refNum from the FSRef */
	result = FSGetCatalogInfo(ref, kFSCatInfoVolume, &catalogInfo, NULL, NULL, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	/* return volume refNum from catalogInfo */
	*vRefNum = catalogInfo.volume;
	
FSGetCatalogInfo:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetVInfo(
	FSVolumeRefNum volume,
	HFSUniStr255 *volumeName,	/* can be NULL */
	UInt64 *freeBytes,			/* can be NULL */
	UInt64 *totalBytes)			/* can be NULL */
{
	OSErr				result;
	FSVolumeInfo		info;
	
	/* ask for the volume's sizes only if needed */
	result = FSGetVolumeInfo(volume, 0, NULL,
		(((NULL != freeBytes) || (NULL != totalBytes)) ? kFSVolInfoSizes : kFSVolInfoNone),
		&info, volumeName, NULL);
	require_noerr(result, FSGetVolumeInfo);
	
	if ( NULL != freeBytes )
	{
		*freeBytes = info.freeBytes;
	}
	if ( NULL != totalBytes )
	{
		*totalBytes = info.totalBytes;
	}
	
FSGetVolumeInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetVolFileSystemID(
	FSVolumeRefNum volume,
	UInt16 *fileSystemID,	/* can be NULL */
	UInt16 *signature)		/* can be NULL */
{
	OSErr			result;
	FSVolumeInfo	info;
	
	result = FSGetVolumeInfo(volume, 0, NULL, kFSVolInfoFSInfo, &info, NULL, NULL);
	require_noerr(result, FSGetVolumeInfo);
	
	if ( NULL != fileSystemID )
	{
		*fileSystemID = info.filesystemID;
	}
	if ( NULL != signature )
	{
		*signature = info.signature;
	}
	
FSGetVolumeInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetMountedVolumes(
	FSRef ***volumeRefsHandle,	/* pointer to handle of FSRefs */
	ItemCount *numVolumes)
{
	OSErr		result;
	OSErr		memResult;
	ItemCount	volumeIndex;
	FSRef		ref;
	
	/* check parameters */
	require_action((NULL != volumeRefsHandle) && (NULL != numVolumes),
		BadParameter, result = paramErr);
	
	/* No volumes yet */
	*numVolumes = 0;
	
	/* Allocate a handle for the results */
	*volumeRefsHandle = (FSRef **)NewHandle(0);
	require_action(NULL != *volumeRefsHandle, NewHandle, result = memFullErr);
	
	/* Call FSGetVolumeInfo in loop to get all volumes starting with the first */
	volumeIndex = 1;
	do
	{
		result = FSGetVolumeInfo(0, volumeIndex, NULL, kFSVolInfoNone, NULL, NULL, &ref);
		if ( noErr == result )
		{
			/* concatenate the FSRef to the end of the handle */
			PtrAndHand(&ref, (Handle)*volumeRefsHandle, sizeof(FSRef));
			memResult = MemError();
			require_noerr_action(memResult, MemoryAllocationFailed, result = memResult);
			
			++(*numVolumes);	/* increment the volume count */
			++volumeIndex;		/* and the volumeIndex to get the next volume*/
		}
	} while ( noErr == result );
	
	/* nsvErr is OK -- it just means there are no more volumes */
	require(nsvErr == result, FSGetVolumeInfo);
		
	return ( noErr );
	
	/**********************/
	
MemoryAllocationFailed:
FSGetVolumeInfo:

	/* dispose of handle if already allocated and clear the outputs */
	if ( NULL != *volumeRefsHandle )
	{
		DisposeHandle((Handle)*volumeRefsHandle);
		*volumeRefsHandle = NULL;
	}
	*numVolumes = 0;
	
NewHandle:
BadParameter:

	return ( result );
}

/*****************************************************************************/

#pragma mark ----- FSRef/FSpec/Path/Name Conversion Routines -----

/*****************************************************************************/

OSErr
FSRefMakeFSSpec(
	const FSRef *ref,
	FSSpec *spec)
{
	OSErr	result;
	
	/* check parameters */
	require_action(NULL != spec, BadParameter, result = paramErr);
	
	result = FSGetCatalogInfo(ref, kFSCatInfoNone, NULL, NULL, spec, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
FSGetCatalogInfo:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSMakeFSRef(
	FSVolumeRefNum volRefNum,
	SInt32 dirID,
	ConstStr255Param name,
	FSRef *ref)
{
	OSErr		result;
	FSRefParam	pb;
	
	/* check parameters */
	require_action(NULL != ref, BadParameter, result = paramErr);
	
	pb.ioVRefNum = volRefNum;
	pb.ioDirID = dirID;
	pb.ioNamePtr = (StringPtr)name;
	pb.newRef = ref;
	result = PBMakeFSRefSync(&pb);
	require_noerr(result, PBMakeFSRefSync);
	
PBMakeFSRefSync:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSStatus
FSMakePath(
	SInt16 volRefNum,
	SInt32 dirID,
	ConstStr255Param name,
	UInt8 *path,
	UInt32 maxPathSize)
{
	OSStatus	result;
	FSRef		ref;
	
	/* check parameters */
	require_action(NULL != path, BadParameter, result = paramErr);
	
	/* convert the inputs to an FSRef */
	result = FSMakeFSRef(volRefNum, dirID, name, &ref);
	require_noerr(result, FSMakeFSRef);
	
	/* and then convert the FSRef to a path */
	result = FSRefMakePath(&ref, path, maxPathSize);
	require_noerr(result, FSRefMakePath);
	
FSRefMakePath:
FSMakeFSRef:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSStatus
FSPathMakeFSSpec(
	const UInt8 *path,
	FSSpec *spec,
	Boolean *isDirectory)	/* can be NULL */
{
	OSStatus	result;
	FSRef		ref;
	
	/* check parameters */
	require_action(NULL != spec, BadParameter, result = paramErr);
	
	/* convert the POSIX path to an FSRef */
	result = FSPathMakeRef(path, &ref, isDirectory);
	require_noerr(result, FSPathMakeRef);
	
	/* and then convert the FSRef to an FSSpec */
	result = FSGetCatalogInfo(&ref, kFSCatInfoNone, NULL, NULL, spec, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
FSGetCatalogInfo:
FSPathMakeRef:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
UnicodeNameGetHFSName(
	UniCharCount nameLength,
	const UniChar *name,
	TextEncoding textEncodingHint,
	Boolean isVolumeName,
	Str31 hfsName)
{
	OSStatus			result;
	ByteCount			unicodeByteLength;
	ByteCount			unicodeBytesConverted;
	ByteCount			actualPascalBytes;
	UnicodeMapping		uMapping;
	UnicodeToTextInfo	utInfo;
	
	/* check parameters */
	require_action(NULL != hfsName, BadParameter, result = paramErr);
	
	/* make sure output is valid in case we get errors or there's nothing to convert */
	hfsName[0] = 0;
	
	unicodeByteLength = nameLength * sizeof(UniChar);
	if ( 0 == unicodeByteLength )
	{
		/* do nothing */
		result = noErr;
	}
	else
	{
		/* if textEncodingHint is kTextEncodingUnknown, get a "default" textEncodingHint */
		if ( kTextEncodingUnknown == textEncodingHint )
		{
			ScriptCode			script;
			RegionCode			region;
			
			script = (ScriptCode)GetScriptManagerVariable(smSysScript);
			region = (RegionCode)GetScriptManagerVariable(smRegionCode);
			result = UpgradeScriptInfoToTextEncoding(script, kTextLanguageDontCare, region,
				NULL, &textEncodingHint );
			if ( paramErr == result )
			{
				/* ok, ignore the region and try again */
				result = UpgradeScriptInfoToTextEncoding(script, kTextLanguageDontCare,
					kTextRegionDontCare, NULL, &textEncodingHint );
			}
			if ( noErr != result )
			{
				/* ok... try something */
				textEncodingHint = kTextEncodingMacRoman;
			}
		}
		
		uMapping.unicodeEncoding = CreateTextEncoding(kTextEncodingUnicodeV2_0,
			kUnicodeCanonicalDecompVariant, kUnicode16BitFormat);
		uMapping.otherEncoding = GetTextEncodingBase(textEncodingHint);
		uMapping.mappingVersion = kUnicodeUseHFSPlusMapping;
	
		result = CreateUnicodeToTextInfo(&uMapping, &utInfo);
		require_noerr(result, CreateUnicodeToTextInfo);
		
		result = ConvertFromUnicodeToText(utInfo, unicodeByteLength, name, kUnicodeLooseMappingsMask,
			0, NULL, 0, NULL,	/* offsetCounts & offsetArrays */
			isVolumeName ? kHFSMaxVolumeNameChars : kHFSMaxFileNameChars,
			&unicodeBytesConverted, &actualPascalBytes, &hfsName[1]);
		require_noerr(result, ConvertFromUnicodeToText);
		
		hfsName[0] = (unsigned char)actualPascalBytes;	/* fill in length byte */

ConvertFromUnicodeToText:
		
		/* verify the result in debug builds -- there's really not anything you can do if it fails */
		verify_noerr(DisposeUnicodeToTextInfo(&utInfo));
	}
	
CreateUnicodeToTextInfo:	
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
HFSNameGetUnicodeName(
	ConstStr31Param hfsName,
	TextEncoding textEncodingHint,
	HFSUniStr255 *unicodeName)
{
	ByteCount			unicodeByteLength;
	OSStatus			result;
	UnicodeMapping		uMapping;
	TextToUnicodeInfo	tuInfo;
	ByteCount			pascalCharsRead;
	
	/* check parameters */
	require_action(NULL != unicodeName, BadParameter, result = paramErr);
	
	/* make sure output is valid in case we get errors or there's nothing to convert */
	unicodeName->length = 0;
	
	if ( 0 == StrLength(hfsName) )
	{
		result = noErr;
	}
	else
	{
		/* if textEncodingHint is kTextEncodingUnknown, get a "default" textEncodingHint */
		if ( kTextEncodingUnknown == textEncodingHint )
		{
			ScriptCode			script;
			RegionCode			region;
			
			script = GetScriptManagerVariable(smSysScript);
			region = GetScriptManagerVariable(smRegionCode);
			result = UpgradeScriptInfoToTextEncoding(script, kTextLanguageDontCare, region,
				NULL, &textEncodingHint);
			if ( paramErr == result )
			{
				/* ok, ignore the region and try again */
				result = UpgradeScriptInfoToTextEncoding(script, kTextLanguageDontCare,
					kTextRegionDontCare, NULL, &textEncodingHint);
			}
			if ( noErr != result )
			{
				/* ok... try something */
				textEncodingHint = kTextEncodingMacRoman;
			}
		}
		
		uMapping.unicodeEncoding = CreateTextEncoding(kTextEncodingUnicodeV2_0,
			kUnicodeCanonicalDecompVariant, kUnicode16BitFormat);
		uMapping.otherEncoding = GetTextEncodingBase(textEncodingHint);
		uMapping.mappingVersion = kUnicodeUseHFSPlusMapping;
	
		result = CreateTextToUnicodeInfo(&uMapping, &tuInfo);
		require_noerr(result, CreateTextToUnicodeInfo);
			
		result = ConvertFromTextToUnicode(tuInfo, hfsName[0], &hfsName[1],
			0,								/* no control flag bits */
			0, NULL, 0, NULL,				/* offsetCounts & offsetArrays */
			sizeof(unicodeName->unicode),	/* output buffer size in bytes */
			&pascalCharsRead, &unicodeByteLength, unicodeName->unicode);
		require_noerr(result, ConvertFromTextToUnicode);
		
		/* convert from byte count to char count */
		unicodeName->length = unicodeByteLength / sizeof(UniChar);

ConvertFromTextToUnicode:

		/* verify the result in debug builds -- there's really not anything you can do if it fails */
		verify_noerr(DisposeTextToUnicodeInfo(&tuInfo));
	}
	
CreateTextToUnicodeInfo:
BadParameter:

	return ( result );
}

/*****************************************************************************/

#pragma mark ----- File/Directory Manipulation Routines -----

/*****************************************************************************/

Boolean FSRefValid(const FSRef *ref)
{
	return ( noErr == FSGetCatalogInfo(ref, kFSCatInfoNone, NULL, NULL, NULL, NULL) );
}

/*****************************************************************************/

OSErr
FSGetParentRef(
	const FSRef *ref,
	FSRef *parentRef)
{
	OSErr	result;
	FSCatalogInfo	catalogInfo;
	
	/* check parameters */
	require_action(NULL != parentRef, BadParameter, result = paramErr);
	
	result = FSGetCatalogInfo(ref, kFSCatInfoNodeID, &catalogInfo, NULL, NULL, parentRef);
	require_noerr(result, FSGetCatalogInfo);
	
	/*
	 * Note: FSRefs always point to real file system objects. So, there cannot
	 * be a FSRef to the parent of volume root directories. Early versions of
	 * Mac OS X do not handle this case correctly and incorrectly return a
	 * FSRef for the parent of volume root directories instead of returning an
	 * invalid FSRef (a cleared FSRef is invalid). The next three lines of code
	 * ensure that you won't run into this bug. WW9D!
	 */
	if ( fsRtDirID == catalogInfo.nodeID )
	{
		/* clear parentRef and return noErr which is the proper behavior */
		memset(parentRef, 0, sizeof(FSRef));
	}

FSGetCatalogInfo:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetFileDirName(
	const FSRef *ref,
	HFSUniStr255 *outName)
{
	OSErr	result;
	
	/* check parameters */
	require_action(NULL != outName, BadParameter, result = paramErr);
	
	result = FSGetCatalogInfo(ref, kFSCatInfoNone, NULL, outName, NULL, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
FSGetCatalogInfo:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetNodeID(
	const FSRef *ref,
	long *nodeID,			/* can be NULL */
	Boolean *isDirectory)	/* can be NULL */
{
	OSErr				result;
	FSCatalogInfo		catalogInfo;
	FSCatalogInfoBitmap whichInfo;
	
	/* determine what catalog information to get */
	whichInfo = kFSCatInfoNone; /* start with none */
	if ( NULL != nodeID )
	{
		whichInfo |= kFSCatInfoNodeID;
	}
	if ( NULL != isDirectory )
	{
		whichInfo |= kFSCatInfoNodeFlags;
	}
	
	result = FSGetCatalogInfo(ref, whichInfo, &catalogInfo, NULL, NULL, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	if ( NULL != nodeID )
	{
		*nodeID = catalogInfo.nodeID;
	}
	if ( NULL != isDirectory )
	{
		*isDirectory = (0 != (kFSNodeIsDirectoryMask & catalogInfo.nodeFlags));
	}
	
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetUserPrivilegesPermissions(
	const FSRef *ref,
	UInt8 *userPrivileges,		/* can be NULL */
	UInt32 permissions[4])		/* can be NULL */
{
	OSErr			result;
	FSCatalogInfo	catalogInfo;
	FSCatalogInfoBitmap whichInfo;
	
	/* determine what catalog information to get */
	whichInfo = kFSCatInfoNone; /* start with none */
	if ( NULL != userPrivileges )
	{
		whichInfo |= kFSCatInfoUserPrivs;
	}
	if ( NULL != permissions )
	{
		whichInfo |= kFSCatInfoPermissions;
	}
	
	result = FSGetCatalogInfo(ref, whichInfo, &catalogInfo, NULL, NULL, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	if ( NULL != userPrivileges )
	{
		*userPrivileges = catalogInfo.userPrivileges;
	}
	if ( NULL != permissions )
	{
		BlockMoveData(&catalogInfo.permissions, permissions, sizeof(UInt32) * 4);
	}
	
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSCheckLock(
	const FSRef *ref)
{
	OSErr			result;
	FSCatalogInfo	catalogInfo;
	FSVolumeInfo	volumeInfo;
	
	/* get nodeFlags and vRefNum for container */
	result = FSGetCatalogInfo(ref, kFSCatInfoNodeFlags + kFSCatInfoVolume, &catalogInfo, NULL, NULL,NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	/* is file locked? */
	if ( 0 != (catalogInfo.nodeFlags & kFSNodeLockedMask) )
	{
		result = fLckdErr;	/* file is locked */
	}
	else
	{
		/* file isn't locked, but is volume locked? */
		
		/* get volume flags */
		result = FSGetVolumeInfo(catalogInfo.volume, 0, NULL, kFSVolInfoFlags, &volumeInfo, NULL, NULL);
		require_noerr(result, FSGetVolumeInfo);
		
		if ( 0 != (volumeInfo.flags & kFSVolFlagHardwareLockedMask) )
		{
			result = wPrErr;	/* volume locked by hardware */
		}
		else if ( 0 != (volumeInfo.flags & kFSVolFlagSoftwareLockedMask) )
		{
			result = vLckdErr;	/* volume locked by software */
		}
	}
	
FSGetVolumeInfo:
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetForkSizes(
	const FSRef *ref,
	UInt64 *dataLogicalSize,	/* can be NULL */
	UInt64 *rsrcLogicalSize)	/* can be NULL */
{
	OSErr				result;
	FSCatalogInfoBitmap whichInfo;
	FSCatalogInfo		catalogInfo;
	
	whichInfo = kFSCatInfoNodeFlags;
	if ( NULL != dataLogicalSize )
	{
		/* get data fork size */
		whichInfo |= kFSCatInfoDataSizes;
	}
	if ( NULL != rsrcLogicalSize )
	{
		/* get resource fork size */
		whichInfo |= kFSCatInfoRsrcSizes;
	}

	/* get nodeFlags and catalog info */
	result = FSGetCatalogInfo(ref, whichInfo, &catalogInfo, NULL, NULL,NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	/* make sure FSRef was to a file */
	require_action(0 == (catalogInfo.nodeFlags & kFSNodeIsDirectoryMask), FSRefNotFile, result = notAFileErr);
	
	if ( NULL != dataLogicalSize )
	{
		/* return data fork size */
		*dataLogicalSize = catalogInfo.dataLogicalSize;
	}
	if ( NULL != rsrcLogicalSize )
	{
		/* return resource fork size */
		*rsrcLogicalSize = catalogInfo.rsrcLogicalSize;
	}
	
FSRefNotFile:
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetTotalForkSizes(
	const FSRef *ref,
	UInt64 *totalLogicalSize,	/* can be NULL */
	UInt64 *totalPhysicalSize,	/* can be NULL */
	ItemCount *forkCount)		/* can be NULL */
{
	OSErr			result;
	CatPositionRec	forkIterator;
	SInt64			forkSize;
	SInt64			*forkSizePtr;
	UInt64			forkPhysicalSize;
	UInt64			*forkPhysicalSizePtr;
	
	/* Determine if forkSize needed */
	if ( NULL != totalLogicalSize)
	{
		*totalLogicalSize = 0;
		forkSizePtr = &forkSize;
	}
	else
	{
		forkSizePtr = NULL;
	}
	
	/* Determine if forkPhysicalSize is needed */
	if ( NULL != totalPhysicalSize )
	{
		*totalPhysicalSize = 0;
		forkPhysicalSizePtr = &forkPhysicalSize;
	}
	else
	{
		forkPhysicalSizePtr = NULL;
	}
	
	/* zero fork count if returning it */
	if ( NULL != forkCount )
	{
		*forkCount = 0;
	}
	
	/* Iterate through the forks to get the sizes */
	forkIterator.initialize = 0;
	do
	{
		result = FSIterateForks(ref, &forkIterator, NULL, forkSizePtr, forkPhysicalSizePtr);
		if ( noErr == result )
		{
			if ( NULL != totalLogicalSize )
			{
				*totalLogicalSize += forkSize;
			}
			
			if ( NULL != totalPhysicalSize )
			{
				*totalPhysicalSize += forkPhysicalSize;
			}
			
			if ( NULL != forkCount )
			{
				++*forkCount;
			}
		}
	} while ( noErr == result );
	
	/* any error result other than errFSNoMoreItems is serious */
	require(errFSNoMoreItems == result, FSIterateForks);
	
	/* Normal exit */
	result = noErr;

FSIterateForks:
	
	return ( result );
}

/*****************************************************************************/

OSErr
FSBumpDate(
	const FSRef *ref)
{
	OSStatus		result;
	FSCatalogInfo	catalogInfo;
	UTCDateTime		oldDateTime;
#if !BuildingMoreFilesXForMacOS9
	FSRef			parentRef;
	Boolean			notifyParent;
#endif

#if !BuildingMoreFilesXForMacOS9
	/* Get the node flags, the content modification date and time, and the parent ref */
	result = FSGetCatalogInfo(ref, kFSCatInfoNodeFlags + kFSCatInfoContentMod, &catalogInfo, NULL, NULL, &parentRef);
	require_noerr(result, FSGetCatalogInfo);
	
	/* Notify the parent if this is a file */
	notifyParent = (0 == (catalogInfo.nodeFlags & kFSNodeIsDirectoryMask));
#else
	/* Get the content modification date and time */
	result = FSGetCatalogInfo(ref, kFSCatInfoContentMod, &catalogInfo, NULL, NULL, NULL);
	require_noerr(result, FSGetCatalogInfo);
#endif
	
	oldDateTime = catalogInfo.contentModDate;

	/* Get the current date and time */
	result = GetUTCDateTime(&catalogInfo.contentModDate, kUTCDefaultOptions);
	require_noerr(result, GetUTCDateTime);
	
	/* if the old date and time is the the same as the current, bump the seconds by one */
	if ( (catalogInfo.contentModDate.fraction == oldDateTime.fraction) &&
		 (catalogInfo.contentModDate.lowSeconds == oldDateTime.lowSeconds) &&
		 (catalogInfo.contentModDate.highSeconds == oldDateTime.highSeconds) )
	{
		++catalogInfo.contentModDate.lowSeconds;
		if ( 0 == catalogInfo.contentModDate.lowSeconds )
		{
			++catalogInfo.contentModDate.highSeconds;
		}
	}
	
	/* Bump the content modification date and time */
	result = FSSetCatalogInfo(ref, kFSCatInfoContentMod, &catalogInfo);
	require_noerr(result, FSSetCatalogInfo);

#if !BuildingMoreFilesXForMacOS9
	/*
	 * The problem with FNNotify is that it is not available under Mac OS 9
	 * and there's no way to test for that except for looking for the symbol
	 * or something. So, I'll just conditionalize this for those who care
	 * to send a notification.
	 */
	
	/* Send a notification for the parent of the file, or for the directory */
	result = FNNotify(notifyParent ? &parentRef : ref, kFNDirectoryModifiedMessage, kNilOptions);
	require_noerr(result, FNNotify);
#endif

	/* ignore errors from FSSetCatalogInfo (volume might be write protected) and FNNotify */
FNNotify:
FSSetCatalogInfo:
	
	return ( noErr );
	
	/**********************/
	
GetUTCDateTime:
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetFinderInfo(
	const FSRef *ref,
	FinderInfo *info,					/* can be NULL */
	ExtendedFinderInfo *extendedInfo,	/* can be NULL */
	Boolean *isDirectory)				/* can be NULL */
{
	OSErr				result;
	FSCatalogInfo		catalogInfo;
	FSCatalogInfoBitmap whichInfo;
	
	/* determine what catalog information is really needed */
	whichInfo = kFSCatInfoNone;
	
	if ( NULL != info )
	{
		/* get FinderInfo */
		whichInfo |= kFSCatInfoFinderInfo;
	}
	
	if ( NULL != extendedInfo )
	{
		/* get ExtendedFinderInfo */
		whichInfo |= kFSCatInfoFinderXInfo;
	}
	
	if ( NULL != isDirectory )
	{
		whichInfo |= kFSCatInfoNodeFlags;
	}
	
	result = FSGetCatalogInfo(ref, whichInfo, &catalogInfo, NULL, NULL, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	/* return FinderInfo if requested */
	if ( NULL != info )
	{
		BlockMoveData(catalogInfo.finderInfo, info, sizeof(FinderInfo));
	}
	
	/* return ExtendedFinderInfo if requested */
	if ( NULL != extendedInfo)
	{
		BlockMoveData(catalogInfo.extFinderInfo, extendedInfo, sizeof(ExtendedFinderInfo));
	}
	
	/* set isDirectory Boolean if requested */
	if ( NULL != isDirectory)
	{
		*isDirectory = (0 != (kFSNodeIsDirectoryMask & catalogInfo.nodeFlags));
	}
	
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSSetFinderInfo(
	const FSRef *ref,
	const FinderInfo *info,
	const ExtendedFinderInfo *extendedInfo)
{
	OSErr				result;
	FSCatalogInfo		catalogInfo;
	FSCatalogInfoBitmap whichInfo;
	
	/* determine what catalog information will be set */
	whichInfo = kFSCatInfoNone; /* start with none */
	if ( NULL != info )
	{
		/* set FinderInfo */
		whichInfo |= kFSCatInfoFinderInfo;
		BlockMoveData(info, catalogInfo.finderInfo, sizeof(FinderInfo));
	}
	if ( NULL != extendedInfo )
	{
		/* set ExtendedFinderInfo */
		whichInfo |= kFSCatInfoFinderXInfo;
		BlockMoveData(extendedInfo, catalogInfo.extFinderInfo, sizeof(ExtendedFinderInfo));
	}
	
	result = FSSetCatalogInfo(ref, whichInfo, &catalogInfo);
	require_noerr(result, FSGetCatalogInfo);
	
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSChangeCreatorType(
	const FSRef *ref,
	OSType fileCreator,
	OSType fileType)
{
	OSErr			result;
	FSCatalogInfo	catalogInfo;
	FSRef			parentRef;
	
	/* get nodeFlags, finder info, and parent FSRef */
	result = FSGetCatalogInfo(ref, kFSCatInfoNodeFlags + kFSCatInfoFinderInfo, &catalogInfo , NULL, NULL, &parentRef);
	require_noerr(result, FSGetCatalogInfo);
	
	/* make sure FSRef was to a file */
	require_action(0 == (catalogInfo.nodeFlags & kFSNodeIsDirectoryMask), FSRefNotFile, result = notAFileErr);
	
	/* If fileType not 0x00000000, change fileType */
	if ( fileType != (OSType)0x00000000 )
	{
		((FileInfo *)&catalogInfo.finderInfo)->fileType = fileType;
	}
	
	/* If creator not 0x00000000, change creator */
	if ( fileCreator != (OSType)0x00000000 )
	{
		((FileInfo *)&catalogInfo.finderInfo)->fileCreator = fileCreator;
	}
	
	/* now, save the new information back to disk */
	result = FSSetCatalogInfo(ref, kFSCatInfoFinderInfo, &catalogInfo);
	require_noerr(result, FSSetCatalogInfo);
	
	/* and attempt to bump the parent directory's mod date to wake up */
	/* the Finder to the change we just made (ignore errors from this) */
	verify_noerr(FSBumpDate(&parentRef));
	
FSSetCatalogInfo:
FSRefNotFile:
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSChangeFinderFlags(
	const FSRef *ref,
	Boolean setBits,
	UInt16 flagBits)
{
	OSErr			result;
	FSCatalogInfo	catalogInfo;
	FSRef			parentRef;
	
	/* get the current finderInfo */
	result = FSGetCatalogInfo(ref, kFSCatInfoFinderInfo, &catalogInfo, NULL, NULL, &parentRef);
	require_noerr(result, FSGetCatalogInfo);
	
	/* set or clear the appropriate bits in the finderInfo.finderFlags */
	if ( setBits )
	{
		/* OR in the bits */
		((FileInfo *)&catalogInfo.finderInfo)->finderFlags |= flagBits;
	}
	else
	{
		/* AND out the bits */
		((FileInfo *)&catalogInfo.finderInfo)->finderFlags &= ~flagBits;
	}
	
	/* save the modified finderInfo */
	result = FSSetCatalogInfo(ref, kFSCatInfoFinderInfo, &catalogInfo);
	require_noerr(result, FSSetCatalogInfo);
	
	/* and attempt to bump the parent directory's mod date to wake up the Finder */
	/* to the change we just made (ignore errors from this) */
	verify_noerr(FSBumpDate(&parentRef));
	
FSSetCatalogInfo:
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSSetInvisible(
	const FSRef *ref)
{
	return ( FSChangeFinderFlags(ref, true, kIsInvisible) );
}

OSErr
FSClearInvisible(
	const FSRef *ref)
{
	return ( FSChangeFinderFlags(ref, false, kIsInvisible) );
}

/*****************************************************************************/

OSErr
FSSetNameLocked(
	const FSRef *ref)
{
	return ( FSChangeFinderFlags(ref, true, kNameLocked) );
}

OSErr
FSClearNameLocked(
	const FSRef *ref)
{
	return ( FSChangeFinderFlags(ref, false, kNameLocked) );
}

/*****************************************************************************/

OSErr
FSSetIsStationery(
	const FSRef *ref)
{
	return ( FSChangeFinderFlags(ref, true, kIsStationery) );
}

OSErr
FSClearIsStationery(
	const FSRef *ref)
{
	return ( FSChangeFinderFlags(ref, false, kIsStationery) );
}

/*****************************************************************************/

OSErr
FSSetHasCustomIcon(
	const FSRef *ref)
{
	return ( FSChangeFinderFlags(ref, true, kHasCustomIcon) );
}

OSErr
FSClearHasCustomIcon(
	const FSRef *ref)
{
	return ( FSChangeFinderFlags(ref, false, kHasCustomIcon) );
}

/*****************************************************************************/

OSErr
FSClearHasBeenInited(
	const FSRef *ref)
{
	return ( FSChangeFinderFlags(ref, false, kHasBeenInited) );
}

/*****************************************************************************/

OSErr
FSCopyFileMgrAttributes(
	const FSRef *sourceRef,
	const FSRef *destinationRef,
	Boolean copyLockBit)
{
	OSErr			result;
	FSCatalogInfo	catalogInfo;
	
	/* get the source information */
	result = FSGetCatalogInfo(sourceRef, kFSCatInfoSettableInfo, &catalogInfo, NULL, NULL, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	/* don't copy the hasBeenInited bit; clear it */
	((FileInfo *)&catalogInfo.finderInfo)->finderFlags &= ~kHasBeenInited;
	
	/* should the locked bit be copied? */
	if ( !copyLockBit )
	{
		/* no, make sure the locked bit is clear */
		catalogInfo.nodeFlags &= ~kFSNodeLockedMask;
	}
		
	/* set the destination information */
	result = FSSetCatalogInfo(destinationRef, kFSCatInfoSettableInfo, &catalogInfo);
	require_noerr(result, FSSetCatalogInfo);
	
FSSetCatalogInfo:
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSMoveRenameObjectUnicode(
	const FSRef *ref,
	const FSRef *destDirectory,
	UniCharCount nameLength,
	const UniChar *name,			/* can be NULL (no rename during move) */
	TextEncoding textEncodingHint,
	FSRef *newRef)					/* if function fails along the way, newRef is final location of file */
{
	OSErr			result;
	FSVolumeRefNum	vRefNum;
	FSCatalogInfo	catalogInfo;
	FSRef			originalDirectory;
	TextEncoding	originalTextEncodingHint;
	HFSUniStr255	originalName;
	HFSUniStr255	uniqueName;		/* unique name given to object while moving it to destination */
	long			theSeed;		/* the seed for generating unique names */
	
	/* check parameters */
	require_action(NULL != newRef, BadParameter, result = paramErr);
	
	/* newRef = input to start with */
	BlockMoveData(ref, newRef, sizeof(FSRef));
	
	/* get destDirectory's vRefNum */
	result = FSGetCatalogInfo(destDirectory, kFSCatInfoVolume, &catalogInfo, NULL, NULL, NULL);
	require_noerr(result, DestinationBad);
	
	/* save vRefNum */
	vRefNum = catalogInfo.volume;
	
	/* get ref's vRefNum, TextEncoding, name and parent directory*/
	result = FSGetCatalogInfo(ref, kFSCatInfoTextEncoding + kFSCatInfoVolume, &catalogInfo, &originalName, NULL, &originalDirectory);
	require_noerr(result, SourceBad);
	
	/* save TextEncoding */
	originalTextEncodingHint = catalogInfo.textEncodingHint;
	
	/* make sure ref and destDirectory are on same volume */
	require_action(vRefNum == catalogInfo.volume, NotSameVolume, result = diffVolErr);
	
	/* Skip a few steps if we're not renaming */
	if ( NULL != name )
	{
		/* generate a name that is unique in both directories */
		theSeed = 0x4a696d4c;	/* a fine unlikely filename */
		
		result = GenerateUniqueHFSUniStr(&theSeed, &originalDirectory, destDirectory, &uniqueName);
		require_noerr(result, GenerateUniqueHFSUniStrFailed);
		
		/* Rename the object to uniqueName */
		result = FSRenameUnicode(ref, uniqueName.length, uniqueName.unicode, kTextEncodingUnknown, newRef);
		require_noerr(result, FSRenameUnicodeBeforeMoveFailed);
		
		if ( FSCompareFSRefs(destDirectory, &originalDirectory) != noErr )
		{
		  /* Move object to its new home */
		  result = FSMoveObject(newRef, destDirectory, newRef);
		  require_noerr(result, FSMoveObjectAfterRenameFailed);
		}
		
		/* Rename the object to new name */
		result = FSRenameUnicode(ref, nameLength, name, textEncodingHint, newRef);
		require_noerr(result, FSRenameUnicodeAfterMoveFailed);
	}
	else
	{
		/* Move object to its new home */
		result = FSMoveObject(newRef, destDirectory, newRef);
		require_noerr(result, FSMoveObjectNoRenameFailed);
	}
	
	return ( result );
	
	/*************/

/*
 * failure handling code when renaming
 */

FSRenameUnicodeAfterMoveFailed:

	/* Error handling: move object back to original location - ignore errors */
	verify_noerr(FSMoveObject(newRef, &originalDirectory, newRef));
	
FSMoveObjectAfterRenameFailed:

	/* Error handling: rename object back to original name - ignore errors */
	verify_noerr(FSRenameUnicode(newRef, originalName.length, originalName.unicode, originalTextEncodingHint, newRef));
	
FSRenameUnicodeBeforeMoveFailed:
GenerateUniqueHFSUniStrFailed:

/*
 * failure handling code for renaming or not
 */
FSMoveObjectNoRenameFailed:
NotSameVolume:
SourceBad:
DestinationBad:
BadParameter:

	return ( result );
}

/*****************************************************************************/

/*
	The FSDeleteContainerLevel function deletes the contents of a container
	directory. All files and subdirectories in the specified container are
	deleted. If a locked file or directory is encountered, it is unlocked
	and then deleted. If any unexpected errors are encountered,
	FSDeleteContainerLevel quits and returns to the caller.
	
	container			--> FSRef to a directory.
	theGlobals			--> A pointer to a FSDeleteContainerGlobals struct
							which contains the variables that do not need to
							be allocated each time FSDeleteContainerLevel
							recurses. That lets FSDeleteContainerLevel use
							less stack space per recursion level.
*/

static
void
FSDeleteContainerLevel(
	const FSRef *container,
	FSDeleteContainerGlobals *theGlobals)
{
	/* level locals */
	FSIterator					iterator;
	FSRef						itemToDelete;
	UInt16						nodeFlags;
	
	/* Open FSIterator for flat access and give delete optimization hint */
	theGlobals->result = FSOpenIterator(container, kFSIterateFlat + kFSIterateDelete, &iterator);
	require_noerr(theGlobals->result, FSOpenIterator);
	
	/* delete the contents of the directory */
	do
	{
		/* get 1 item to delete */
		theGlobals->result = FSGetCatalogInfoBulk(iterator, 1, &theGlobals->actualObjects,
								NULL, kFSCatInfoNodeFlags, &theGlobals->catalogInfo,
								&itemToDelete, NULL, NULL);
		if ( (noErr == theGlobals->result) && (1 == theGlobals->actualObjects) )
		{
			/* save node flags in local in case we have to recurse */
			nodeFlags = theGlobals->catalogInfo.nodeFlags;
			
			/* is it a file or directory? */
			if ( 0 != (nodeFlags & kFSNodeIsDirectoryMask) )
			{
				/* it's a directory -- delete its contents before attempting to delete it */
				FSDeleteContainerLevel(&itemToDelete, theGlobals);
			}
			/* are we still OK to delete? */
			if ( noErr == theGlobals->result )
			{
				/* is item locked? */
				if ( 0 != (nodeFlags & kFSNodeLockedMask) )
				{
					/* then attempt to unlock it (ignore result since FSDeleteObject will set it correctly) */
					theGlobals->catalogInfo.nodeFlags = nodeFlags & ~kFSNodeLockedMask;
					(void) FSSetCatalogInfo(&itemToDelete, kFSCatInfoNodeFlags, &theGlobals->catalogInfo);
				}
				/* delete the item */
				theGlobals->result = FSDeleteObject(&itemToDelete);
			}
		}
	} while ( noErr == theGlobals->result );
	
	/* we found the end of the items normally, so return noErr */
	if ( errFSNoMoreItems == theGlobals->result )
	{
		theGlobals->result = noErr;
	}
	
	/* close the FSIterator (closing an open iterator should never fail) */
	verify_noerr(FSCloseIterator(iterator));

FSOpenIterator:

	return;
}

/*****************************************************************************/

OSErr
FSDeleteContainerContents(
	const FSRef *container)
{
	FSDeleteContainerGlobals	theGlobals;
	
	/* delete container's contents */
	FSDeleteContainerLevel(container, &theGlobals);
	
	return ( theGlobals.result );
}

/*****************************************************************************/

OSErr
FSDeleteContainer(
	const FSRef *container)
{
	OSErr			result;
	FSCatalogInfo	catalogInfo;
	
	/* get nodeFlags for container */
	result = FSGetCatalogInfo(container, kFSCatInfoNodeFlags, &catalogInfo, NULL, NULL,NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	/* make sure container is a directory */
	require_action(0 != (catalogInfo.nodeFlags & kFSNodeIsDirectoryMask), ContainerNotDirectory, result = dirNFErr);
	
	/* delete container's contents */
	result = FSDeleteContainerContents(container);
	require_noerr(result, FSDeleteContainerContents);
	
	/* is container locked? */
	if ( 0 != (catalogInfo.nodeFlags & kFSNodeLockedMask) )
	{
		/* then attempt to unlock container (ignore result since FSDeleteObject will set it correctly) */
		catalogInfo.nodeFlags &= ~kFSNodeLockedMask;
		(void) FSSetCatalogInfo(container, kFSCatInfoNodeFlags, &catalogInfo);
	}
	
	/* delete the container */
	result = FSDeleteObject(container);
	
FSDeleteContainerContents:
ContainerNotDirectory:
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

/*
	The FSIterateContainerLevel function iterates the contents of a container
	directory and calls a IterateContainerFilterProc function once for each
	file and directory found.
	
	theGlobals			--> A pointer to a FSIterateContainerGlobals struct
							which contains the variables needed globally by
							all recusion levels of FSIterateContainerLevel.
							That makes FSIterateContainer thread safe since
							each call to it uses its own global world.
							It also contains the variables that do not need
							to be allocated each time FSIterateContainerLevel
							recurses. That lets FSIterateContainerLevel use
							less stack space per recursion level.
*/

static
void
FSIterateContainerLevel(
	FSIterateContainerGlobals *theGlobals)
{	
	FSIterator	iterator;
	
	/* If maxLevels is zero, we aren't checking levels */
	/* If currentLevel < maxLevels, look at this level */
	if ( (theGlobals->maxLevels == 0) ||
		 (theGlobals->currentLevel < theGlobals->maxLevels) )
	{
		/* Open FSIterator for flat access to theGlobals->ref */
		theGlobals->result = FSOpenIterator(&theGlobals->ref, kFSIterateFlat, &iterator);
		require_noerr(theGlobals->result, FSOpenIterator);
		
		++theGlobals->currentLevel; /* Go to next level */
		
		/* Call FSGetCatalogInfoBulk in loop to get all items in the container */
		do
		{
			theGlobals->result = FSGetCatalogInfoBulk(iterator, 1, &theGlobals->actualObjects,
				&theGlobals->containerChanged, theGlobals->whichInfo, &theGlobals->catalogInfo,
				&theGlobals->ref, theGlobals->specPtr, theGlobals->namePtr);
			if ( (noErr == theGlobals->result || errFSNoMoreItems == theGlobals->result) &&
				(0 != theGlobals->actualObjects) )
			{
				/* Call the IterateFilterProc */
				theGlobals->quitFlag = CallIterateContainerFilterProc(theGlobals->iterateFilter,
					theGlobals->containerChanged, theGlobals->currentLevel,
					&theGlobals->catalogInfo, &theGlobals->ref,
					theGlobals->specPtr, theGlobals->namePtr, theGlobals->yourDataPtr);
				/* Is it a directory? */
				if ( 0 != (theGlobals->catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) )
				{
					/* Keep going? */
					if ( !theGlobals->quitFlag )
					{
						/* Dive again if the IterateFilterProc didn't say "quit" */
						FSIterateContainerLevel(theGlobals);
					}
				}
			}
			/* time to fall back a level? */
		} while ( (noErr == theGlobals->result) && (!theGlobals->quitFlag) );
		
		/* errFSNoMoreItems is OK - it only means we hit the end of this level */
		/* afpAccessDenied is OK, too - it only means we cannot see inside a directory */
		if ( (errFSNoMoreItems == theGlobals->result) ||
			 (afpAccessDenied == theGlobals->result) )
		{
			theGlobals->result = noErr;
		}
		
		--theGlobals->currentLevel; /* Return to previous level as we leave */
		
		/* Close the FSIterator (closing an open iterator should never fail) */
		verify_noerr(FSCloseIterator(iterator));
	}
	
FSOpenIterator:

	return;
}

/*****************************************************************************/

OSErr
FSIterateContainer(
	const FSRef *container,
	ItemCount maxLevels,
	FSCatalogInfoBitmap whichInfo,
	Boolean wantFSSpec,
	Boolean wantName,
	IterateContainerFilterProcPtr iterateFilter,
	void *yourDataPtr)
{
	OSErr						result;
	FSIterateContainerGlobals	theGlobals;
	
	/* make sure there is an iterateFilter */
	require_action(iterateFilter != NULL, NoIterateFilter, result = paramErr);
	
	/*
	 * set up the globals we need to access from the recursive routine
	 */
	theGlobals.iterateFilter = iterateFilter;
	/* we need the node flags no matter what was requested so we can detect files vs. directories */
	theGlobals.whichInfo = whichInfo | kFSCatInfoNodeFlags;
	/* start with input container -- the first OpenIterator will ensure it is a directory */
	theGlobals.ref = *container;
	if ( wantFSSpec )
	{
		theGlobals.specPtr = &theGlobals.spec;
	}
	else
	{
		theGlobals.specPtr = NULL;
	}
	if ( wantName )
	{
		theGlobals.namePtr = &theGlobals.name;
	}
	else
	{
		theGlobals.namePtr = NULL;
	}
	theGlobals.yourDataPtr = yourDataPtr;
	theGlobals.maxLevels = maxLevels;
	theGlobals.currentLevel = 0;
	theGlobals.quitFlag = false;
	theGlobals.containerChanged = false;
	theGlobals.result = noErr;
	theGlobals.actualObjects = 0;
	
	/* here we go into recursion land... */
	FSIterateContainerLevel(&theGlobals);
	result = theGlobals.result;
	require_noerr(result, FSIterateContainerLevel);
	
FSIterateContainerLevel:
NoIterateFilter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetDirectoryItems(
	const FSRef *container,
	FSRef ***refsHandle,	/* pointer to handle of FSRefs */
	ItemCount *numRefs,
	Boolean *containerChanged)
{
	/* Grab items 10 at a time. */
	enum { kMaxItemsPerBulkCall = 10 };
	
	OSErr		result;
	OSErr		memResult;
	FSIterator	iterator;
	FSRef		refs[kMaxItemsPerBulkCall];
	ItemCount	actualObjects;
	Boolean		changed;
	
	/* check parameters */
	require_action((NULL != refsHandle) && (NULL != numRefs) && (NULL != containerChanged),
		BadParameter, result = paramErr);
	
	*numRefs = 0;
	*containerChanged = false;
	*refsHandle = (FSRef **)NewHandle(0);
	require_action(NULL != *refsHandle, NewHandle, result = memFullErr);
	
	/* open an FSIterator */
	result = FSOpenIterator(container, kFSIterateFlat, &iterator);
	require_noerr(result, FSOpenIterator);
	
	/* Call FSGetCatalogInfoBulk in loop to get all items in the container */
	do
	{
		result = FSGetCatalogInfoBulk(iterator, kMaxItemsPerBulkCall, &actualObjects,
					&changed, kFSCatInfoNone,  NULL,  refs, NULL, NULL);
		
		/* if the container changed, set containerChanged for output, but keep going */
		if ( changed )
		{
			*containerChanged = changed;
		}
		
		/* any result other than noErr and errFSNoMoreItems is serious */
		require((noErr == result) || (errFSNoMoreItems == result), FSGetCatalogInfoBulk);
		
		/* add objects to output array and count */
		if ( 0 != actualObjects )
		{
			/* concatenate the FSRefs to the end of the	 handle */
			PtrAndHand(refs, (Handle)*refsHandle, actualObjects * sizeof(FSRef));
			memResult = MemError();
			require_noerr_action(memResult, MemoryAllocationFailed, result = memResult);
			
			*numRefs += actualObjects;
		}
	} while ( noErr == result );
	
	verify_noerr(FSCloseIterator(iterator)); /* closing an open iterator should never fail, but... */
	
	return ( noErr );
	
	/**********************/
	
MemoryAllocationFailed:
FSGetCatalogInfoBulk:

	/* close the iterator */
	verify_noerr(FSCloseIterator(iterator));

FSOpenIterator:
	/* dispose of handle if already allocated and clear the outputs */
	if ( NULL != *refsHandle )
	{
		DisposeHandle((Handle)*refsHandle);
		*refsHandle = NULL;
	}
	*numRefs = 0;
	
NewHandle:
BadParameter:

	return ( result );
}

/*****************************************************************************/

/*
	The GenerateUniqueName function generates a HFSUniStr255 name that is
	unique in both dir1 and dir2.
	
	startSeed			-->	A pointer to a long which is used to generate the
							unique name.
						<--	It is modified on output to a value which should
							be used to generate the next unique name.
	dir1				-->	The first directory.
	dir2				-->	The second directory.
	uniqueName			<--	A pointer to a HFSUniStr255 where the unique name
							is to be returned.
*/

static
OSErr
GenerateUniqueHFSUniStr(
	long *startSeed,
	const FSRef *dir1,
	const FSRef *dir2,
	HFSUniStr255 *uniqueName)
{
	OSErr			result;
	long			i;
	FSRefParam		pb;
	FSRef			newRef;
	unsigned char	hexStr[17] = "0123456789ABCDEF";
	
	/* set up the parameter block */
	pb.name = uniqueName->unicode;
	pb.nameLength = 8;	/* always 8 characters */
	pb.textEncodingHint = kTextEncodingUnknown;
	pb.newRef = &newRef;

	/* loop until we get fnfErr with a filename in both directories */
	result = noErr;
	while ( fnfErr != result )
	{
		/* convert startSeed to 8 character Unicode string */
		uniqueName->length = 8;
		for ( i = 0; i < 8; ++i )
		{
			uniqueName->unicode[i] = hexStr[((*startSeed >> ((7-i)*4)) & 0xf)];
		}
		
		/* try in dir1 */
		pb.ref = dir1;
		result = PBMakeFSRefUnicodeSync(&pb);
		if ( fnfErr == result )
		{
			/* try in dir2 */
			pb.ref = dir2;
			result = PBMakeFSRefUnicodeSync(&pb);
			if ( fnfErr != result )
			{
				/* exit if anything other than noErr or fnfErr */
				require_noerr(result, Dir2PBMakeFSRefUnicodeSyncFailed);
			}
		}
		else
		{
			/* exit if anything other than noErr or fnfErr */
			require_noerr(result, Dir1PBMakeFSRefUnicodeSyncFailed);
		}
		
		/* increment seed for next pass through loop, */
		/* or for next call to GenerateUniqueHFSUniStr */
		++(*startSeed);
	}
	
	/* we have a unique file name which doesn't exist in dir1 or dir2 */
	result = noErr;
	
Dir2PBMakeFSRefUnicodeSyncFailed:
Dir1PBMakeFSRefUnicodeSyncFailed:

	return ( result );
}

/*****************************************************************************/

OSErr
FSExchangeObjectsCompat(
	const FSRef *sourceRef,
	const FSRef *destRef,
	FSRef *newSourceRef,
	FSRef *newDestRef)
{
	enum
	{
		/* get all settable info except for mod dates, plus the volume refNum and parent directory ID */
		kGetCatInformationMask = (kFSCatInfoSettableInfo |
								  kFSCatInfoVolume |
								  kFSCatInfoParentDirID) &
								 ~(kFSCatInfoContentMod | kFSCatInfoAttrMod),
		/* set everything possible except for mod dates */
		kSetCatinformationMask = kFSCatInfoSettableInfo &
								 ~(kFSCatInfoContentMod | kFSCatInfoAttrMod)
	};
	
	OSErr					result;
	GetVolParmsInfoBuffer	volParmsInfo;
	UInt32					infoSize;
	FSCatalogInfo			sourceCatalogInfo;	/* source file's catalog information */
	FSCatalogInfo			destCatalogInfo;	/* destination file's catalog information */
	HFSUniStr255			sourceName;			/* source file's Unicode name */
	HFSUniStr255			destName;			/* destination file's Unicode name */
	FSRef					sourceCurrentRef;	/* FSRef to current location of source file throughout this function */
	FSRef					destCurrentRef;		/* FSRef to current location of destination file throughout this function */
	FSRef					sourceParentRef;	/* FSRef to parent directory of source file */
	FSRef					destParentRef;		/* FSRef to parent directory of destination file */
	HFSUniStr255			sourceUniqueName;	/* unique name given to source file while exchanging it with destination */
	HFSUniStr255			destUniqueName;		/* unique name given to destination file while exchanging it with source */
	long					theSeed;			/* the seed for generating unique names */
	Boolean					sameParentDirs;		/* true if source and destinatin parent directory is the same */
	
	/* check parameters */
	require_action((NULL != newSourceRef) && (NULL != newDestRef), BadParameter, result = paramErr);
	
	/* output refs and current refs = input refs to start with */
	BlockMoveData(sourceRef, newSourceRef, sizeof(FSRef));
	BlockMoveData(sourceRef, &sourceCurrentRef, sizeof(FSRef));
	
	BlockMoveData(destRef, newDestRef, sizeof(FSRef));
	BlockMoveData(destRef, &destCurrentRef, sizeof(FSRef));

	/* get source volume's vRefNum */
	result = FSGetCatalogInfo(&sourceCurrentRef, kFSCatInfoVolume, &sourceCatalogInfo, NULL, NULL, NULL);
	require_noerr(result, DetermineSourceVRefNumFailed);
	
	/* see if that volume supports FSExchangeObjects */
	result = FSGetVolParms(sourceCatalogInfo.volume, sizeof(GetVolParmsInfoBuffer),
		&volParmsInfo, &infoSize);
	if ( (noErr == result) && VolSupportsFSExchangeObjects(&volParmsInfo) )
	{
		/* yes - use FSExchangeObjects */
		result = FSExchangeObjects(sourceRef, destRef);
	}
	else
	{
		/* no - emulate FSExchangeObjects */
		
		/* Note: The compatibility case won't work for files with *Btree control blocks. */
		/* Right now the only *Btree files are created by the system. */
		
		/* get all catalog information and Unicode names for each file */
		result = FSGetCatalogInfo(&sourceCurrentRef, kGetCatInformationMask, &sourceCatalogInfo, &sourceName, NULL, &sourceParentRef);
		require_noerr(result, SourceFSGetCatalogInfoFailed);
		
		result = FSGetCatalogInfo(&destCurrentRef, kGetCatInformationMask, &destCatalogInfo, &destName, NULL, &destParentRef);
		require_noerr(result, DestFSGetCatalogInfoFailed);
		
		/* make sure source and destination are on same volume */
		require_action(sourceCatalogInfo.volume == destCatalogInfo.volume, NotSameVolume, result = diffVolErr);
		
		/* make sure both files are *really* files */
		require_action((0 == (sourceCatalogInfo.nodeFlags & kFSNodeIsDirectoryMask)) &&
					   (0 == (destCatalogInfo.nodeFlags & kFSNodeIsDirectoryMask)), NotAFile, result = notAFileErr);
		
		/* generate 2 names that are unique in both directories */
		theSeed = 0x4a696d4c;	/* a fine unlikely filename */
		
		result = GenerateUniqueHFSUniStr(&theSeed, &sourceParentRef, &destParentRef, &sourceUniqueName);
		require_noerr(result, GenerateUniqueHFSUniStr1Failed);
		
		result = GenerateUniqueHFSUniStr(&theSeed, &sourceParentRef, &destParentRef, &destUniqueName);
		require_noerr(result, GenerateUniqueHFSUniStr2Failed);

		/* rename sourceCurrentRef to sourceUniqueName */
		result = FSRenameUnicode(&sourceCurrentRef, sourceUniqueName.length, sourceUniqueName.unicode, kTextEncodingUnknown, newSourceRef);
		require_noerr(result, FSRenameUnicode1Failed);
		BlockMoveData(newSourceRef, &sourceCurrentRef, sizeof(FSRef));
		
		/* rename destCurrentRef to destUniqueName */
		result = FSRenameUnicode(&destCurrentRef, destUniqueName.length, destUniqueName.unicode, kTextEncodingUnknown, newDestRef);
		require_noerr(result, FSRenameUnicode2Failed);
		BlockMoveData(newDestRef, &destCurrentRef, sizeof(FSRef));
		
		/* are the source and destination parent directories the same? */
		sameParentDirs = ( sourceCatalogInfo.parentDirID == destCatalogInfo.parentDirID );
		if ( !sameParentDirs )
		{
			/* move source file to dest parent directory */
			result = FSMoveObject(&sourceCurrentRef, &destParentRef, newSourceRef);
			require_noerr(result, FSMoveObject1Failed);
			BlockMoveData(newSourceRef, &sourceCurrentRef, sizeof(FSRef));
			
			/* move dest file to source parent directory */
			result = FSMoveObject(&destCurrentRef, &sourceParentRef, newDestRef);
			require_noerr(result, FSMoveObject2Failed);
			BlockMoveData(newDestRef, &destCurrentRef, sizeof(FSRef));
		}
		
		/* At this point, the files are in their new locations (if they were moved). */
		/* The source file is named sourceUniqueName and is in the directory referred to */
		/* by destParentRef. The destination file is named destUniqueName and is in the */
		/* directory referred to by sourceParentRef. */
				
		/* give source file the dest file's catalog information except for mod dates */
		result = FSSetCatalogInfo(&sourceCurrentRef, kSetCatinformationMask, &destCatalogInfo);
		require_noerr(result, FSSetCatalogInfo1Failed);
		
		/* give dest file the source file's catalog information except for mod dates */
		result = FSSetCatalogInfo(&destCurrentRef, kSetCatinformationMask, &sourceCatalogInfo);
		require_noerr(result, FSSetCatalogInfo2Failed);
		
		/* rename source file with dest file's name */
		result = FSRenameUnicode(&sourceCurrentRef, destName.length, destName.unicode, destCatalogInfo.textEncodingHint, newSourceRef);
		require_noerr(result, FSRenameUnicode3Failed);
		BlockMoveData(newSourceRef, &sourceCurrentRef, sizeof(FSRef));
		
		/* rename dest file with source file's name */
		result = FSRenameUnicode(&destCurrentRef, sourceName.length, sourceName.unicode, sourceCatalogInfo.textEncodingHint, newDestRef);
		require_noerr(result, FSRenameUnicode4Failed);
		
		/* we're done with no errors, so swap newSourceRef and newDestRef */
		BlockMoveData(newDestRef, newSourceRef, sizeof(FSRef));
		BlockMoveData(&sourceCurrentRef, newDestRef, sizeof(FSRef));
	}
	
	return ( result );
	
	/**********************/

/* If there are any failures while emulating FSExchangeObjects, attempt to reverse any steps */
/* already taken. In any case, newSourceRef and newDestRef will refer to the files in whatever */
/* state and location they ended up in so that both files can be found by the calling code. */
	
FSRenameUnicode4Failed:

	/* attempt to rename source file to sourceUniqueName */
	if ( noErr == FSRenameUnicode(&sourceCurrentRef, sourceUniqueName.length, sourceUniqueName.unicode, kTextEncodingUnknown, newSourceRef) )
	{
		BlockMoveData(newSourceRef, &sourceCurrentRef, sizeof(FSRef));
	}

FSRenameUnicode3Failed:

	/* attempt to restore dest file's catalog information */
	verify_noerr(FSSetCatalogInfo(&destCurrentRef, kFSCatInfoSettableInfo, &destCatalogInfo));

FSSetCatalogInfo2Failed:

	/* attempt to restore source file's catalog information */
	verify_noerr(FSSetCatalogInfo(&sourceCurrentRef, kFSCatInfoSettableInfo, &sourceCatalogInfo));

FSSetCatalogInfo1Failed:

	if ( !sameParentDirs )
	{
		/* attempt to move dest file back to dest directory */
		if ( noErr == FSMoveObject(&destCurrentRef, &destParentRef, newDestRef) )
		{
			BlockMoveData(newDestRef, &destCurrentRef, sizeof(FSRef));
		}
	}

FSMoveObject2Failed:

	if ( !sameParentDirs )
	{
		/* attempt to move source file back to source directory */
		if ( noErr == FSMoveObject(&sourceCurrentRef, &sourceParentRef, newSourceRef) )
		{
			BlockMoveData(newSourceRef, &sourceCurrentRef, sizeof(FSRef));
		}
	}

FSMoveObject1Failed:

	/* attempt to rename dest file to original name */
	verify_noerr(FSRenameUnicode(&destCurrentRef, destName.length, destName.unicode, destCatalogInfo.textEncodingHint, newDestRef));

FSRenameUnicode2Failed:

	/* attempt to rename source file to original name */
	verify_noerr(FSRenameUnicode(&sourceCurrentRef, sourceName.length, sourceName.unicode, sourceCatalogInfo.textEncodingHint, newSourceRef));

FSRenameUnicode1Failed:
GenerateUniqueHFSUniStr2Failed:
GenerateUniqueHFSUniStr1Failed:
NotAFile:
NotSameVolume:
DestFSGetCatalogInfoFailed:
SourceFSGetCatalogInfoFailed:
DetermineSourceVRefNumFailed:	
BadParameter:

	return ( result );
}

/*****************************************************************************/

#pragma mark ----- Shared Environment Routines -----

/*****************************************************************************/

OSErr
FSLockRange(
	SInt16 refNum,
	SInt32 rangeLength,
	SInt32 rangeStart)
{
	OSErr			result;
	ParamBlockRec	pb;

	pb.ioParam.ioRefNum = refNum;
	pb.ioParam.ioReqCount = rangeLength;
	pb.ioParam.ioPosMode = fsFromStart;
	pb.ioParam.ioPosOffset = rangeStart;
	result = PBLockRangeSync(&pb);
	require_noerr(result, PBLockRangeSync);
	
PBLockRangeSync:

	return ( result );
}

/*****************************************************************************/

OSErr
FSUnlockRange(
	SInt16 refNum,
	SInt32 rangeLength,
	SInt32 rangeStart)
{
	OSErr			result;
	ParamBlockRec	pb;

	pb.ioParam.ioRefNum = refNum;
	pb.ioParam.ioReqCount = rangeLength;
	pb.ioParam.ioPosMode = fsFromStart;
	pb.ioParam.ioPosOffset = rangeStart;
	result = PBUnlockRangeSync(&pb);
	require_noerr(result, PBUnlockRangeSync);
	
PBUnlockRangeSync:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetDirAccess(
	const FSRef *ref,
	SInt32 *ownerID,		/* can be NULL */
	SInt32 *groupID,		/* can be NULL */
	SInt32 *accessRights)	/* can be NULL */
{
	OSErr			result;
	FSSpec			spec;
	HParamBlockRec	pb;
	
	/* get FSSpec from FSRef */
	result = FSGetCatalogInfo(ref, kFSCatInfoNone, NULL, NULL, &spec, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	/* get directory access info for FSSpec */
	pb.accessParam.ioNamePtr = (StringPtr)spec.name;
	pb.accessParam.ioVRefNum = spec.vRefNum;
	pb.fileParam.ioDirID = spec.parID;
	result = PBHGetDirAccessSync(&pb);
	require_noerr(result, PBHGetDirAccessSync);
	
	/* return the IDs and access rights */
	if ( NULL != ownerID )
	{
		*ownerID = pb.accessParam.ioACOwnerID;
	}
	if ( NULL != groupID )
	{
		*groupID = pb.accessParam.ioACGroupID;
	}
	if ( NULL != accessRights )
	{
		*accessRights = pb.accessParam.ioACAccess;
	}
	
PBHGetDirAccessSync:
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSSetDirAccess(
	const FSRef *ref,
	SInt32 ownerID,
	SInt32 groupID,
	SInt32 accessRights)
{
	OSErr			result;
	FSSpec			spec;
	HParamBlockRec	pb;

	enum
	{
		/* Just the bits that can be set */
		kSetDirAccessSettableMask = (kioACAccessBlankAccessMask +
			kioACAccessEveryoneWriteMask + kioACAccessEveryoneReadMask + kioACAccessEveryoneSearchMask +
			kioACAccessGroupWriteMask + kioACAccessGroupReadMask + kioACAccessGroupSearchMask +
			kioACAccessOwnerWriteMask + kioACAccessOwnerReadMask + kioACAccessOwnerSearchMask)
	};
	
	/* get FSSpec from FSRef */
	result = FSGetCatalogInfo(ref, kFSCatInfoNone, NULL, NULL, &spec, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	/* set directory access info for FSSpec */
	pb.accessParam.ioNamePtr = (StringPtr)spec.name;
	pb.accessParam.ioVRefNum = spec.vRefNum;
	pb.fileParam.ioDirID = spec.parID;
	pb.accessParam.ioACOwnerID = ownerID;
	pb.accessParam.ioACGroupID = groupID;
	pb.accessParam.ioACAccess = accessRights & kSetDirAccessSettableMask;
	result = PBHSetDirAccessSync(&pb);
	require_noerr(result, PBHSetDirAccessSync);
	
PBHSetDirAccessSync:
FSGetCatalogInfo:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetVolMountInfoSize(
	FSVolumeRefNum volRefNum,
	SInt16 *size)
{
	OSErr			result;
	ParamBlockRec	pb;

	/* check parameters */
	require_action(NULL != size, BadParameter, result = paramErr);
	
	pb.ioParam.ioNamePtr = NULL;
	pb.ioParam.ioVRefNum = volRefNum;
	pb.ioParam.ioBuffer = (Ptr)size;
	result = PBGetVolMountInfoSize(&pb);
	require_noerr(result, PBGetVolMountInfoSize);
	
PBGetVolMountInfoSize:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSGetVolMountInfo(
	FSVolumeRefNum volRefNum,
	void *volMountInfo)
{
	OSErr			result;
	ParamBlockRec	pb;

	/* check parameters */
	require_action(NULL != volMountInfo, BadParameter, result = paramErr);
	
	pb.ioParam.ioNamePtr = NULL;
	pb.ioParam.ioVRefNum = volRefNum;
	pb.ioParam.ioBuffer = (Ptr)volMountInfo;
	result = PBGetVolMountInfo(&pb);
	require_noerr(result, PBGetVolMountInfo);
	
PBGetVolMountInfo:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSVolumeMount(
	const void *volMountInfo,
	FSVolumeRefNum *volRefNum)
{
	OSErr			result;
	ParamBlockRec	pb;

	/* check parameters */
	require_action(NULL != volRefNum, BadParameter, result = paramErr);
	
	pb.ioParam.ioBuffer = (Ptr)volMountInfo;
	result = PBVolumeMount(&pb);
	require_noerr(result, PBVolumeMount);
	
	/* return the volume reference number */
	*volRefNum = pb.ioParam.ioVRefNum;

PBVolumeMount:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSMapID(
	FSVolumeRefNum volRefNum,
	SInt32 ugID,
	SInt16 objType,
	Str31 name)
{
	OSErr			result;
	HParamBlockRec	pb;

	/* check parameters */
	require_action(NULL != name, BadParameter, result = paramErr);
	
	pb.objParam.ioNamePtr = NULL;
	pb.objParam.ioVRefNum = volRefNum;
	pb.objParam.ioObjType = objType;
	pb.objParam.ioObjNamePtr = name;
	pb.objParam.ioObjID = ugID;
	result = PBHMapIDSync(&pb);
	require_noerr(result, PBHMapIDSync);
	
PBHMapIDSync:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSMapName(
	FSVolumeRefNum volRefNum,
	ConstStr255Param name,
	SInt16 objType,
	SInt32 *ugID)
{
	OSErr			result;
	HParamBlockRec	pb;

	/* check parameters */
	require_action(NULL != ugID, BadParameter, result = paramErr);
	
	pb.objParam.ioNamePtr = NULL;
	pb.objParam.ioVRefNum = volRefNum;
	pb.objParam.ioObjType = objType;
	pb.objParam.ioObjNamePtr = (StringPtr)name;
	result = PBHMapNameSync(&pb);
	require_noerr(result, PBHMapNameSync);
	
	/* return the user or group ID */
	*ugID = pb.objParam.ioObjID;
	
PBHMapNameSync:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSCopyFile(
	const FSRef *srcFileRef,
	const FSRef *dstDirectoryRef,
	UniCharCount nameLength,
	const UniChar *copyName,	/* can be NULL (no rename during copy) */
	TextEncoding textEncodingHint,
	FSRef *newRef)				/* can be NULL */
{
	OSErr					result;
	FSSpec					srcFileSpec;
	FSCatalogInfo			catalogInfo;
	HParamBlockRec			pb;
	Str31					hfsName;
	GetVolParmsInfoBuffer	volParmsInfo;
	UInt32					infoSize;
	
	/* get source FSSpec from source FSRef */
	result = FSGetCatalogInfo(srcFileRef, kFSCatInfoNone, NULL, NULL, &srcFileSpec, NULL);
	require_noerr(result, FSGetCatalogInfo_srcFileRef);
	
	/* Make sure the volume supports CopyFile */
	result = FSGetVolParms(srcFileSpec.vRefNum, sizeof(GetVolParmsInfoBuffer),
		&volParmsInfo, &infoSize);
	require_action((noErr == result) && VolHasCopyFile(&volParmsInfo),
		NoCopyFileSupport, result = paramErr);

	/* get destination volume reference number and destination directory ID from destination FSRef */
	result = FSGetCatalogInfo(dstDirectoryRef, kFSCatInfoVolume + kFSCatInfoNodeID,
		&catalogInfo, NULL, NULL, NULL);
	require_noerr(result, FSGetCatalogInfo_dstDirectoryRef);
	
	/* tell the server to copy the object */
	pb.copyParam.ioVRefNum = srcFileSpec.vRefNum;
	pb.copyParam.ioDirID = srcFileSpec.parID;
	pb.copyParam.ioNamePtr = (StringPtr)srcFileSpec.name;
	pb.copyParam.ioDstVRefNum = catalogInfo.volume;
	pb.copyParam.ioNewDirID = (long)catalogInfo.nodeID;
	pb.copyParam.ioNewName = NULL;
	if ( NULL != copyName )
	{
		result = UnicodeNameGetHFSName(nameLength, copyName, textEncodingHint, false, hfsName);
		require_noerr(result, UnicodeNameGetHFSName);
		
		pb.copyParam.ioCopyName = hfsName;
	}
	else
	{
		pb.copyParam.ioCopyName = NULL;
	}
	result = PBHCopyFileSync(&pb);
	require_noerr(result, PBHCopyFileSync);
	
	if ( NULL != newRef )
	{
		verify_noerr(FSMakeFSRef(pb.copyParam.ioDstVRefNum, pb.copyParam.ioNewDirID,
			pb.copyParam.ioCopyName, newRef));
	}
		
PBHCopyFileSync:
UnicodeNameGetHFSName:
FSGetCatalogInfo_dstDirectoryRef:
NoCopyFileSupport:
FSGetCatalogInfo_srcFileRef:

	return ( result );
}

/*****************************************************************************/

OSErr
FSMoveRename(
	const FSRef *srcFileRef,
	const FSRef *dstDirectoryRef,
	UniCharCount nameLength,
	const UniChar *moveName,	/* can be NULL (no rename during move) */
	TextEncoding textEncodingHint,
	FSRef *newRef)				/* can be NULL */
{
	OSErr					result;
	FSSpec					srcFileSpec;
	FSCatalogInfo			catalogInfo;
	HParamBlockRec			pb;
	Str31					hfsName;
	GetVolParmsInfoBuffer	volParmsInfo;
	UInt32					infoSize;
	
	/* get source FSSpec from source FSRef */
	result = FSGetCatalogInfo(srcFileRef, kFSCatInfoNone, NULL, NULL, &srcFileSpec, NULL);
	require_noerr(result, FSGetCatalogInfo_srcFileRef);
	
	/* Make sure the volume supports MoveRename */
	result = FSGetVolParms(srcFileSpec.vRefNum, sizeof(GetVolParmsInfoBuffer),
		&volParmsInfo, &infoSize);
	require_action((noErr == result) && VolHasMoveRename(&volParmsInfo),
		NoMoveRenameSupport, result = paramErr);

	/* get destination volume reference number and destination directory ID from destination FSRef */
	result = FSGetCatalogInfo(dstDirectoryRef, kFSCatInfoVolume + kFSCatInfoNodeID,
		&catalogInfo, NULL, NULL, NULL);
	require_noerr(result, FSGetCatalogInfo_dstDirectoryRef);
	
	/* make sure the source and destination are on the same volume */
	require_action(srcFileSpec.vRefNum == catalogInfo.volume, NotSameVolume, result = diffVolErr);
	
	/* tell the server to move and rename the object */
	pb.copyParam.ioVRefNum = srcFileSpec.vRefNum;
	pb.copyParam.ioDirID = srcFileSpec.parID;
	pb.copyParam.ioNamePtr = (StringPtr)srcFileSpec.name;
	pb.copyParam.ioNewDirID = (long)catalogInfo.nodeID;
	pb.copyParam.ioNewName = NULL;
	if ( NULL != moveName )
	{
		result = UnicodeNameGetHFSName(nameLength, moveName, textEncodingHint, false, hfsName);
		require_noerr(result, UnicodeNameGetHFSName);
		
		pb.copyParam.ioCopyName = hfsName;
	}
	else
	{
		pb.copyParam.ioCopyName = NULL;
	}
	result = PBHMoveRenameSync(&pb);
	require_noerr(result, PBHMoveRenameSync);
	
	if ( NULL != newRef )
	{
		verify_noerr(FSMakeFSRef(pb.copyParam.ioVRefNum, pb.copyParam.ioNewDirID,
			pb.copyParam.ioCopyName, newRef));
	}
	
PBHMoveRenameSync:
UnicodeNameGetHFSName:
NotSameVolume:
FSGetCatalogInfo_dstDirectoryRef:
NoMoveRenameSupport:
FSGetCatalogInfo_srcFileRef:

	return ( result );
}

/*****************************************************************************/

#pragma mark ----- File ID Routines -----

/*****************************************************************************/

OSErr
FSResolveFileIDRef(
	FSVolumeRefNum volRefNum,
	SInt32 fileID,
	FSRef *ref)
{
	OSErr		result;
	FIDParam	pb;
	Str255		tempStr;
	
	/* check parameters */
	require_action(NULL != ref, BadParameter, result = paramErr);
	
	/* resolve the file ID reference */
	tempStr[0] = 0;
	pb.ioNamePtr = tempStr;
	pb.ioVRefNum = volRefNum;
	pb.ioFileID = fileID;
	result = PBResolveFileIDRefSync((HParmBlkPtr)&pb);
	require_noerr(result, PBResolveFileIDRefSync);
	
	/* and then make an FSRef to the file */
	result = FSMakeFSRef(volRefNum, pb.ioSrcDirID, tempStr, ref);
	require_noerr(result, FSMakeFSRef);
	
FSMakeFSRef:
PBResolveFileIDRefSync:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSCreateFileIDRef(
	const FSRef *ref,
	SInt32 *fileID)
{
	OSErr		result;
	FSSpec		spec;
	FIDParam	pb;
	
	/* check parameters */
	require_action(NULL != fileID, BadParameter, result = paramErr);
	
	/* Get an FSSpec from the FSRef */
	result = FSGetCatalogInfo(ref, kFSCatInfoNone, NULL, NULL, &spec, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	/* Create (or get) the file ID reference using the FSSpec */
	pb.ioNamePtr = (StringPtr)spec.name;
	pb.ioVRefNum = spec.vRefNum;
	pb.ioSrcDirID = spec.parID;
	result = PBCreateFileIDRefSync((HParmBlkPtr)&pb);
	require((noErr == result) || (fidExists == result) || (afpIDExists == result),
		PBCreateFileIDRefSync);
	
	/* return the file ID reference */
	*fileID = pb.ioFileID;
	
PBCreateFileIDRefSync:
FSGetCatalogInfo:
BadParameter:

	return ( result );
}

/*****************************************************************************/

#pragma mark ----- Utility Routines -----

/*****************************************************************************/

Ptr
GetTempBuffer(
	ByteCount buffReqSize,
	ByteCount *buffActSize)
{
	enum
	{
		kSlopMemory = 0x00008000	/* 32K - Amount of free memory to leave when allocating buffers */
	};
	
	Ptr tempPtr;
	
	/* check parameters */
	require_action(NULL != buffActSize, BadParameter, tempPtr = NULL);
	
	/* Make request a multiple of 4K bytes */
	buffReqSize = buffReqSize & 0xfffff000;
	
	if ( buffReqSize < 0x00001000 )
	{
		/* Request was smaller than 4K bytes - make it 4K */
		buffReqSize = 0x00001000;
	}
	
	/* Attempt to allocate the memory */
	tempPtr = NewPtr(buffReqSize);
	
	/* If request failed, go to backup plan */
	if ( (tempPtr == NULL) && (buffReqSize > 0x00001000) )
	{
		/*
		**	Try to get largest 4K byte block available
		**	leaving some slop for the toolbox if possible
		*/
		long freeMemory = (FreeMem() - kSlopMemory) & 0xfffff000;
		
		buffReqSize = MaxBlock() & 0xfffff000;
		
		if ( buffReqSize > freeMemory )
		{
			buffReqSize = freeMemory;
		}
		
		if ( buffReqSize == 0 )
		{
			buffReqSize = 0x00001000;
		}
		
		tempPtr = NewPtr(buffReqSize);
	}
	
	/* Return bytes allocated */
	if ( tempPtr != NULL )
	{
		*buffActSize = buffReqSize;
	}
	else
	{
		*buffActSize = 0;
	}
	
BadParameter:

	return ( tempPtr );
}

/*****************************************************************************/

OSErr
FileRefNumGetFSRef(
	short refNum,
	FSRef *ref)
{
	return ( FSGetForkCBInfo(refNum, 0, NULL, NULL, NULL, ref, NULL) );
}

/*****************************************************************************/

OSErr
FSSetDefault(
	const FSRef *newDefault,
	FSRef *oldDefault)
{
	OSErr			result;
	FSVolumeRefNum	vRefNum;
	long			dirID;
	FSCatalogInfo	catalogInfo;
	
	/* check parameters */
	require_action((NULL != newDefault) && (NULL != oldDefault), BadParameter, result = paramErr);
	
	/* Get nodeFlags, vRefNum and dirID (nodeID) of newDefault */
	result = FSGetCatalogInfo(newDefault,
		kFSCatInfoNodeFlags + kFSCatInfoVolume + kFSCatInfoNodeID,
		&catalogInfo, NULL, NULL, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	/* Make sure newDefault is a directory */
	require_action(0 != (kFSNodeIsDirectoryMask & catalogInfo.nodeFlags), NewDefaultNotDirectory,
		result = dirNFErr);
	
	/* Get the current working directory. */
	result = HGetVol(NULL, &vRefNum, &dirID);
	require_noerr(result, HGetVol);
	
	/* Return the oldDefault FSRef */
	result = FSMakeFSRef(vRefNum, dirID, NULL, oldDefault);
	require_noerr(result, FSMakeFSRef);
	
	/* Set the new current working directory */
	result = HSetVol(NULL, catalogInfo.volume, catalogInfo.nodeID);
	require_noerr(result, HSetVol);

HSetVol:
FSMakeFSRef:
HGetVol:
NewDefaultNotDirectory:
FSGetCatalogInfo:
BadParameter:

	return ( result );
}

/*****************************************************************************/

OSErr
FSRestoreDefault(
	const FSRef *oldDefault)
{
	OSErr			result;
	FSCatalogInfo	catalogInfo;
	
	/* check parameters */
	require_action(NULL != oldDefault, BadParameter, result = paramErr);
	
	/* Get nodeFlags, vRefNum and dirID (nodeID) of oldDefault */
	result = FSGetCatalogInfo(oldDefault,
		kFSCatInfoNodeFlags + kFSCatInfoVolume + kFSCatInfoNodeID,
		&catalogInfo, NULL, NULL, NULL);
	require_noerr(result, FSGetCatalogInfo);
	
	/* Make sure oldDefault is a directory */
	require_action(0 != (kFSNodeIsDirectoryMask & catalogInfo.nodeFlags), OldDefaultNotDirectory,
		result = dirNFErr);
	
	/* Set the current working directory to oldDefault */
	result = HSetVol(NULL, catalogInfo.volume, catalogInfo.nodeID);
	require_noerr(result, HSetVol);

HSetVol:
OldDefaultNotDirectory:
FSGetCatalogInfo:
BadParameter:

	return ( result );
}

/*****************************************************************************/
