/*
	File:		FSCopyObject.c
	
	Contains:	A Copy/Delete Files/Folders engine which uses the HFS+ API's.
				This code is a combination of MoreFilesX and MPFileCopy 
				with some added features.  This code will run on OS 9.1 and up
				and 10.1.x (Classic and Carbon)

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

	Copyright © 2002 Apple Computer, Inc., All Rights Reserved
*/

#include "FSCopyObject.h"
#include <UnicodeConverter.h>
#include <stddef.h>
#include <string.h>

/*
	
*/

#pragma mark ----- Tunable Parameters -----

// The following constants control the behavior of the copy engine.

enum {		// BufferSizeForThisVolumeSpeed
//	kDefaultCopyBufferSize =   2L * 1024 * 1024,			// Fast be not very responsive.
	kDefaultCopyBufferSize = 256L * 1024,					// Slower, but can still use machine.
	kMaximumCopyBufferSize =   2L * 1024 * 1024,
	kMinimumCopyBufferSize = 1024
};

enum {		// CalculateForksToCopy
	kExpectedForkCount     = 10				// Number of non-classic forks we expect.
};											// (i.e. non resource/data forks) 

enum {		// CheckForDestInsideSource
	errFSDestInsideSource = -1234
};

enum {		
			// for use with PBHGetDirAccess in IsDropBox
	kPrivilegesMask			= kioACAccessUserWriteMask | kioACAccessUserReadMask | kioACAccessUserSearchMask,

			// for use with FSGetCatalogInfo and FSPermissionInfo->mode
			// from sys/stat.h...  note -- sys/stat.h definitions are in octal
			//
			// You can use these values to adjust the users/groups permissions
			// on a file/folder with FSSetCatalogInfo and extracting the
			// kFSCatInfoPermissions field.  See code below for examples
	kRWXUserAccessMask		= 0x01C0,
	kReadAccessUser			= 0x0100,
	kWriteAccessUser		= 0x0080,
	kExecuteAccessUser		= 0x0040,

	kRWXGroupAccessMask		= 0x0038,
	kReadAccessGroup		= 0x0020,
	kWriteAccessGroup		= 0x0010,
	kExecuteAccessGroup		= 0x0008,

	kRWXOtherAccessMask		= 0x0007,
	kReadAccessOther		= 0x0004,
	kWriteAccessOther		= 0x0002,
	kExecuteAccessOther		= 0x0001,

	kDropFolderValue		= kWriteAccessOther | kExecuteAccessOther
};

#pragma mark ----- Struct Definitions -----

#define VolHasCopyFile(volParms) \
		(((volParms)->vMAttrib & (1L << bHasCopyFile)) != 0)

	// The CopyParams data structure holds the copy buffer used
	// when copying the forks over, as well as special case
	// info on the destination
struct CopyParams {
	UTCDateTime		magicBusyCreateDate;
	void 			*copyBuffer;
	ByteCount 		copyBufferSize;
	Boolean         copyingToDropFolder;
	Boolean			copyingToLocalVolume;
};
typedef struct CopyParams CopyParams;

	// The FilterParams data structure holds the date and info
	// that the caller wants passed into the Filter Proc, as well
	// as the Filter Proc Pointer itself
struct FilterParams {
	FSCatalogInfoBitmap				whichInfo;
	CopyObjectFilterProcPtr			filterProcPtr;
	FSSpec							fileSpec;
	FSSpec						   *fileSpecPtr;
	HFSUniStr255					fileName;
	HFSUniStr255				   *fileNamePtr;
	void						   *yourDataPtr;
};
typedef struct FilterParams FilterParams;

	// The ForkTracker data structure holds information about a specific fork,
	// specifically the name and the refnum.  We use this to build a list of
	// all the forks before we start copying them.  We need to do this because,
	// if we're copying into a drop folder, we must open all the forks before
	// we start copying data into any of them.
	// Plus it's a convenient way to keep track of all the forks...
struct ForkTracker {
	HFSUniStr255 	forkName;
	SInt64			forkSize;
	SInt16       	forkDestRefNum;
};
typedef struct ForkTracker ForkTracker;
typedef ForkTracker *ForkTrackerPtr;

	// The FSCopyObjectGlobals data structure holds information needed to do
	// the recursive copy of a directory.
struct FSCopyObjectGlobals
{
	FSCatalogInfo	catalogInfo;
	FSRef			ref;			/* FSRef to the source file/folder*/
	FSRef			destRef;		/* FSRef to the destination directory */
	CopyParams		*copyParams;	/* pointer to info needed to do the copy */
	FilterParams	*filterParams;	/* pointer to info needed for the optional filter proc */
	ItemCount		maxLevels;		/* maximum levels to iterate through */
	ItemCount		currentLevel;	/* the current level FSCopyFolderLevel is on */
	Boolean			quitFlag;		/* set to true if filter wants to kill interation */
	Boolean			containerChanged; /* temporary - set to true if the current container changed during iteration */
	OSErr			result;			/* result */
	ItemCount		actualObjects;	/* number of objects returned */
};
typedef struct FSCopyObjectGlobals FSCopyObjectGlobals;

	// The FSDeleteObjectGlobals data structure holds information needed to 
	// recursively delete a directory
struct FSDeleteObjectGlobals
{
	FSCatalogInfo					catalogInfo;	/* FSCatalogInfo */
	ItemCount						actualObjects;	/* number of objects returned */
	OSErr							result;			/* result */
};
typedef struct FSDeleteObjectGlobals FSDeleteObjectGlobals;

#pragma mark ----- Local Prototypes -----

static OSErr		FSCopyFile(	const FSRef		*source,
							 	const FSRef		*destDir,
							 	const HFSUniStr255 *destName,			/* can be NULL (no rename during copy) */
							 	CopyParams		*copyParams,
							 	FilterParams	*filterParams,
							 	FSRef			*newFile);				/* can be NULL */
							 	
static OSErr		CopyFile(	const FSRef			*source,
								FSCatalogInfo		*sourceCatInfo,
							 	const FSRef			*destDir,
							 	const HFSUniStr255 	*destName,
								CopyParams			*copyParams,
								FSRef 				*newRef);			/* can be NULL */
								
static	OSErr 		FSUsePBHCopyFile(	const FSRef		*srcFileRef,
										const FSRef 	*dstDirectoryRef,
										UniCharCount 	nameLength, 
										const UniChar 	*copyName,		/* can be NULL (no rename during copy) */
										TextEncoding 	textEncodingHint,
										FSRef 			*newRef);		/* can be NULL */
										
static OSErr		DoCopyFile(	const FSRef 		*source,
								FSCatalogInfo		*sourceCatInfo,
								const FSRef			*destDir,
								const HFSUniStr255 	*destName,
								CopyParams			*params, 
								FSRef				*newRef);			/* can be NULL */

static OSErr		FSCopyFolder(	const FSRef *source,
									const FSRef *destDir,
									const HFSUniStr255 *destName,		/* can be NULL (no rename during copy) */
									CopyParams* copyParams,
									FilterParams *filterParams,
									ItemCount maxLevels,
									FSRef* newDir);						/* can be NULL */

static OSErr 		FSCopyFolderLevel( FSCopyObjectGlobals *theGlobals, const HFSUniStr255 *destName );

static OSErr		CheckForDestInsideSource(	const FSRef *source,
												const FSRef *destDir);

static OSErr		CopyItemsForks(	const FSRef *source,
									const FSRef *dest,
									CopyParams  *params);

static OSErr		OpenAllForks(	const FSRef 			*dest,
									const ForkTrackerPtr 	dataFork,
									const ForkTrackerPtr 	rsrcFork,
									ForkTrackerPtr			otherForks,
									ItemCount				otherForksCount);
									
static OSErr		CopyFork(	const FSRef				*source,
								const FSRef 			*dest, 
								const ForkTrackerPtr 	sourceFork,
								const CopyParams 		*params);
								
static OSErr		CloseAllForks(	SInt16 			dataRefNum,
									SInt16			rsrcRefNum,
									ForkTrackerPtr	otherForks, 
									ItemCount		otherForksCount);
									
static OSErr		CalculateForksToCopy(	const FSRef				*source,
											const ForkTrackerPtr	dataFork,
											const ForkTrackerPtr	rsrcFork,
											ForkTrackerPtr			*otherForksParam,
											ItemCount     			*otherForksCountParam);
																				
static OSErr		CalculateBufferSize(	const FSRef *source,
											const FSRef *destDir,
											ByteCount * bufferSize);

static ByteCount	BufferSizeForThisVolume(FSVolumeRefNum vRefNum);

static ByteCount	BufferSizeForThisVolumeSpeed(UInt32 volumeBytesPerSecond);

static OSErr		IsDropBox(	const FSRef* source,
								Boolean *isDropBox);

static OSErr		GetMagicBusyCreationDate( UTCDateTime *date );

static Boolean 		CompareHFSUniStr255(const HFSUniStr255 *lhs, 
										const HFSUniStr255 *rhs);

static OSErr		FSGetVRefNum(	const FSRef *ref,
									FSVolumeRefNum *vRefNum);

static OSErr		FSGetVolParms(	FSVolumeRefNum volRefNum,
										UInt32 bufferSize,
										GetVolParmsInfoBuffer *volParmsInfo,
										UInt32 *actualInfoSize);	/*	Can Be NULL	*/

static OSErr		UnicodeNameGetHFSName(	UniCharCount nameLength,
												const UniChar *name,
												TextEncoding textEncodingHint,
												Boolean isVolumeName,
												Str31 hfsName);

static OSErr		FSMakeFSRef(	FSVolumeRefNum volRefNum,
									SInt32 dirID,
									ConstStr255Param name,
									FSRef *ref);
									
static OSErr		FSDeleteFolder( const FSRef *container );

static void			FSDeleteFolderLevel(	const FSRef *container,
											FSDeleteObjectGlobals *theGlobals);

/*****************************************************************************/
/*****************************************************************************/
/*****************************************************************************/

#pragma mark ----- Copy Objects -----

	// This routine acts as the top level of the copy engine.  It exists
	// to a) present a nicer API than the various recursive routines, and
	// b) minimise the local variables in the recursive routines.
OSErr FSCopyObject(	const FSRef *source,
					const FSRef *destDir,
				 	UniCharCount nameLength,
				 	const UniChar *copyName,	// can be NULL (no rename during copy)
				 	ItemCount maxLevels,
				 	FSCatalogInfoBitmap whichInfo,
					Boolean wantFSSpec,
					Boolean wantName,
					CopyObjectFilterProcPtr filterProcPtr, // can be NULL
					void *yourDataPtr,		// can be NULL
					FSRef *newObject)		// can be NULL
{
	CopyParams   	copyParams;
	FilterParams	filterParams;
	HFSUniStr255 	destName;
	HFSUniStr255 	*destNamePtr;
	Boolean			isDirectory;
	OSErr			osErr = ( source != NULL && destDir != NULL ) ? noErr : paramErr;
  
	if (copyName)
	{
		if (nameLength <= 255)
		{
			BlockMoveData(copyName, destName.unicode, nameLength * sizeof(UniChar));
			destName.length = nameLength;
			destNamePtr = &destName;
		}
		else
			osErr = paramErr;
	}
	else
		destNamePtr = NULL;
  
		// we want the settable info no matter what the user asked for
	filterParams.whichInfo		= whichInfo | kFSCatInfoSettableInfo;
	filterParams.filterProcPtr	= filterProcPtr;
	filterParams.fileSpecPtr	= ( wantFSSpec ) ? &filterParams.fileSpec	: NULL;
	filterParams.fileNamePtr	= ( wantName   ) ? &filterParams.fileName	: NULL;
	filterParams.yourDataPtr	= yourDataPtr;

		// Calculate the optimal buffer size to copy the forks over
		// and create the buffer
	if( osErr == noErr )
		osErr = CalculateBufferSize( source, destDir, &copyParams.copyBufferSize);

	if( osErr == noErr )
	{
		copyParams.copyBuffer = NewPtr( copyParams.copyBufferSize );
		if( copyParams.copyBuffer == NULL )
			osErr = memFullErr;
	}

	if( osErr == noErr )
		osErr = GetMagicBusyCreationDate( &copyParams.magicBusyCreateDate ); 

	if( osErr == noErr )	// figure out if source is a file or folder
	{						//			  if it is on a local volume, 
							//			  if destination is a drop box 
		GetVolParmsInfoBuffer	volParms;
		FSCatalogInfo			tmpCatInfo;
		FSVolumeRefNum			destVRefNum;

			// to figure out if the souce is a folder or directory
		osErr = FSGetCatalogInfo(source, kFSCatInfoNodeFlags, &tmpCatInfo, NULL, NULL, NULL);
		if( osErr == noErr )
		{
			isDirectory = ((tmpCatInfo.nodeFlags & kFSNodeIsDirectoryMask) != 0);
				// are we copying to a drop folder?
			osErr = IsDropBox( destDir, &copyParams.copyingToDropFolder );
		}
		if( osErr == noErr )		
			osErr = FSGetVRefNum(destDir, &destVRefNum);
		if( osErr == noErr )
			osErr = FSGetVolParms( destVRefNum, sizeof(volParms), &volParms, NULL );
		if( osErr == noErr )	// volParms.vMServerAdr is non-zero for remote volumes
			copyParams.copyingToLocalVolume = (volParms.vMServerAdr == 0);
	}
	
		// now copy the file/folder...
	if( osErr == noErr )
	{		// is it a folder?
		if ( isDirectory )
		{		// yes
			osErr = CheckForDestInsideSource(source, destDir);
			if( osErr == noErr )
				osErr = FSCopyFolder( source, destDir, destNamePtr, &copyParams, &filterParams, maxLevels, newObject );
		}
		else	// no
			osErr = FSCopyFile(source, destDir, destNamePtr, &copyParams, &filterParams, newObject);
	}
	
	// Clean up for space and safety...  Who me?
	if( copyParams.copyBuffer != NULL )
		DisposePtr((char*)copyParams.copyBuffer);
	
	mycheck_noerr( osErr );	// put up debug assert in debug builds
	
	return osErr;
}				 

/*****************************************************************************/

#pragma mark ----- Copy Files -----

OSErr FSCopyFile(	const FSRef		*source,
			 		const FSRef		*destDir,
			 		const HFSUniStr255 *destName,
			 		CopyParams	*copyParams,
			 		FilterParams	*filterParams,
			 		FSRef			*newFile)
{
	FSCatalogInfo 	sourceCatInfo;
	FSRef			tmpRef;
	OSErr			osErr = ( source != NULL && destDir != NULL &&
							  copyParams != NULL && filterParams != NULL ) ? noErr : paramErr;
		
		// get needed info about the source file
	if ( osErr == noErr )
	{
		if (destName)
		{
			osErr = FSGetCatalogInfo(source, filterParams->whichInfo, &sourceCatInfo, NULL,  NULL, NULL);
			filterParams->fileName = *destName;
		}
		else
			osErr = FSGetCatalogInfo(source, filterParams->whichInfo, &sourceCatInfo, &filterParams->fileName,  NULL, NULL);
	}
	if( osErr == noErr )
		osErr = CopyFile(source, &sourceCatInfo, destDir, &filterParams->fileName, copyParams, &tmpRef);
	
		// Call the IterateFilterProc _after_ the new file was created
		// even if an error occured
	if( filterParams->filterProcPtr != NULL )
	{	
		(void) CallCopyObjectFilterProc(filterParams->filterProcPtr, false, 0, osErr, &sourceCatInfo,
										&tmpRef, filterParams->fileSpecPtr,
										filterParams->fileNamePtr, filterParams->yourDataPtr);
	}
	
	if( osErr == noErr && newFile != NULL )
		*newFile = tmpRef;
	
	mycheck_noerr(osErr);	// put up debug assert in debug builds

	return osErr;
}

/*****************************************************************************/

OSErr CopyFile(	const FSRef *source,
				FSCatalogInfo *sourceCatInfo,
			   	const FSRef *destDir,
			   	ConstHFSUniStr255Param destName,
			   	CopyParams *params,
			   	FSRef* newFile)
{
	OSErr		osErr = paramErr;

		// Clear the "inited" bit so that the Finder positions the icon for us.
	((FInfo *)(sourceCatInfo->finderInfo))->fdFlags &= ~kHasBeenInited;

		// if the destination is on a remote volume, try to use PBHCopyFile
	if( params->copyingToLocalVolume == 0 )
		osErr = FSUsePBHCopyFile( source, destDir, 0, NULL, kTextEncodingUnknown, newFile );
		
							// if PBHCopyFile didn't work or not supported,
	if( osErr != noErr )	// then try old school file transfer
		osErr = DoCopyFile(	source, sourceCatInfo, destDir, destName, params, newFile );		

	mycheck_noerr(osErr);	// put up debug assert in debug builds

	return osErr;
}

/*****************************************************************************/

OSErr FSUsePBHCopyFile(	const FSRef *srcFileRef,
						const FSRef *dstDirectoryRef,
						UniCharCount nameLength,
						const UniChar *copyName,	/* can be NULL (no rename during copy) */
						TextEncoding textEncodingHint,
						FSRef *newRef)				/* can be NULL */
{
	FSSpec					srcFileSpec;
	FSCatalogInfo			catalogInfo;
	GetVolParmsInfoBuffer	volParmsInfo;
	HParamBlockRec			pb;
	Str31					hfsName;
	OSErr					osErr;
	
								// get source FSSpec from source FSRef
	osErr = FSGetCatalogInfo(srcFileRef, kFSCatInfoNone, NULL, NULL, &srcFileSpec, NULL);
	if( osErr == noErr )		// Make sure the volume supports CopyFile
		osErr = FSGetVolParms(	srcFileSpec.vRefNum, sizeof(GetVolParmsInfoBuffer), &volParmsInfo, NULL);
	if( osErr == noErr )
		osErr = VolHasCopyFile(&volParmsInfo) ? noErr : paramErr;
	if( osErr == noErr )		// get the destination vRefNum and dirID
		osErr = FSGetCatalogInfo(dstDirectoryRef, kFSCatInfoVolume | kFSCatInfoNodeID, &catalogInfo, NULL, NULL, NULL);
	if( osErr == noErr )		// gather all the info needed
	{
		pb.copyParam.ioVRefNum		= srcFileSpec.vRefNum;
		pb.copyParam.ioDirID		= srcFileSpec.parID;
		pb.copyParam.ioNamePtr		= (StringPtr)srcFileSpec.name;
		pb.copyParam.ioDstVRefNum	= catalogInfo.volume;
		pb.copyParam.ioNewDirID		= (long)catalogInfo.nodeID;
		pb.copyParam.ioNewName		= NULL;
		if( copyName != NULL )
			osErr = UnicodeNameGetHFSName(nameLength, copyName, textEncodingHint, false, hfsName);
		pb.copyParam.ioCopyName		= ( copyName != NULL && osErr == noErr ) ? hfsName : NULL;
	}
	if( osErr == noErr )		// tell the server to copy the object
		osErr = PBHCopyFileSync(&pb);
	
	if( osErr == noErr && newRef != NULL )
	{
		myverify_noerr(FSMakeFSRef(pb.copyParam.ioDstVRefNum, pb.copyParam.ioNewDirID,
								 pb.copyParam.ioCopyName, newRef));
	}

	if( osErr != paramErr )	// returning paramErr is ok, it means PBHCopyFileSync was not supported
		mycheck_noerr(osErr);	// put up debug assert in debug builds

	return osErr;
}

/*****************************************************************************/

	// Copies a file referenced by source to the directory referenced by
	// destDir.  destName is the name the file should be given in the
	// destination directory.  sourceCatInfo is the catalogue info of
	// the file, which is passed in as an optimization (we could get it
	// by doing a FSGetCatalogInfo but the caller has already done that
	// so we might as well take advantage of that).
	//
OSErr DoCopyFile(	const FSRef *source,
					FSCatalogInfo *sourceCatInfo,
					const FSRef *destDir,
					ConstHFSUniStr255Param destName,
					CopyParams *params,
					FSRef *newRef)
{
	FSRef	 			dest;
	FSPermissionInfo	originalPermissions;
	UTCDateTime			originalCreateDate;
	OSType				originalFileType;
	UInt16				originalNodeFlags;
	OSErr				osErr;

		// If we're copying to a drop folder, we won't be able to reset this
		// information once the copy is done, so we don't mess it up in
		// the first place.  We still clear the locked bit though; items dropped
		// into a drop folder always become unlocked.	
	if (!params->copyingToDropFolder)
	{
			// Remember to clear the file's type, so the Finder doesn't
			// look at the file until we're done.
		originalFileType = ((FInfo *) &sourceCatInfo->finderInfo)->fdType;
		((FInfo *) &sourceCatInfo->finderInfo)->fdType = kFirstMagicBusyFiletype;

			// Remember and clear the file's locked status, so that we can
			// actually write the forks we're about to create.		
		originalNodeFlags = sourceCatInfo->nodeFlags;

			// Set the file's creation date to kMagicBusyCreationDate,
			// remembering the old value for restoration later.
		originalCreateDate = sourceCatInfo->createDate;
		sourceCatInfo->createDate = params->magicBusyCreateDate;
	}
	sourceCatInfo->nodeFlags &= ~kFSNodeLockedMask;
	
		// we need to have user level read/write/execute access to the file we are going to create
		// otherwise FSCreateFileUnicode will return -5000 (afpAccessDenied),
		// and the FSRef returned will be invalid, yet the file is created (size 0k)... bug?
	originalPermissions = *((FSPermissionInfo*)sourceCatInfo->permissions);
	((FSPermissionInfo*)sourceCatInfo->permissions)->mode |= kRWXUserAccessMask;
	
		// Classic only supports 9.1 and higher, so we don't have to worry about 2397324
	osErr = FSCreateFileUnicode(destDir, destName->length, destName->unicode, kFSCatInfoSettableInfo, sourceCatInfo, &dest, NULL);
	if( osErr == noErr )	// Copy the forks over to the new file
		osErr = CopyItemsForks(source, &dest, params);

		// Restore the original file type, creation and modification dates,
		// locked status and permissions.
		// This is one of the places where we need to handle drop
		// folders as a special case because this FSSetCatalogInfo will fail for
		// an item in a drop folder, so we don't even attempt it.		
	if (osErr == noErr && !params->copyingToDropFolder)
	{
		((FInfo *) &sourceCatInfo->finderInfo)->fdType = originalFileType;
		sourceCatInfo->createDate = originalCreateDate;
		sourceCatInfo->nodeFlags  = originalNodeFlags;
		*((FSPermissionInfo*)sourceCatInfo->permissions) = originalPermissions;

		osErr = FSSetCatalogInfo(&dest, kFSCatInfoSettableInfo, sourceCatInfo);
	}
	
		// If we created the file and the copy failed, try to clean up by
		// deleting the file we created.  We do this because, while it's
		// possible for the copy to fail halfway through and the File Manager 
		// doesn't really clean up that well, we *really* don't wan't
		// any half-created files being left around.
		// if the file already existed, we don't want to delete it
		//
		// Note that there are cases where the assert can fire which are not
		// errors (for example, if the  destination is in a drop folder) but
		// I'll leave it in anyway because I'm interested in discovering those
		// cases.  Note that, if this fires and we're running MP, current versions
		// of MacsBug won't catch the exception and the MP task will terminate
		// with a kMPTaskAbortedErr error.
	if (osErr != noErr && osErr != dupFNErr )
		myverify_noerr( FSDeleteObjects(&dest) );
	else if( newRef != NULL )	// if everything was fine, then return the new file
		*newRef = dest;

	mycheck_noerr(osErr);	// put up debug assert in debug builds

	return osErr;
}

/*****************************************************************************/

#pragma mark ----- Copy Folders -----

OSErr FSCopyFolder( const FSRef *source, const FSRef *destDir, const HFSUniStr255 *destName,
                    CopyParams* copyParams,  FilterParams *filterParams, ItemCount maxLevels, FSRef* newDir)
{
	FSCopyObjectGlobals	theGlobals;

	theGlobals.ref				= *source;
	theGlobals.destRef			= *destDir;
	theGlobals.copyParams		= copyParams;
	theGlobals.filterParams		= filterParams;
	theGlobals.maxLevels		= maxLevels;
	theGlobals.currentLevel		= 0;
	theGlobals.quitFlag			= false;
	theGlobals.containerChanged = false;
	theGlobals.result			= ( source != NULL && destDir != NULL &&
									copyParams != NULL && filterParams != NULL ) ?
								  noErr : paramErr;
	theGlobals.actualObjects	= 0;
	
		// here we go into recursion land...
	if( theGlobals.result == noErr )
		theGlobals.result = FSCopyFolderLevel(&theGlobals, destName);

	if( theGlobals.result == noErr && newDir != NULL)
		*newDir = theGlobals.ref;

		// Call the IterateFilterProc _after_ the new folder is created
		// even if we failed...
	if( filterParams->filterProcPtr != NULL )
	{	
		(void) CallCopyObjectFilterProc(filterParams->filterProcPtr, theGlobals.containerChanged,
										theGlobals.currentLevel, theGlobals.result, &theGlobals.catalogInfo,
										&theGlobals.ref, filterParams->fileSpecPtr,
										filterParams->fileNamePtr, filterParams->yourDataPtr);
	}

	mycheck_noerr(theGlobals.result);	// put up debug assert in debug builds

	return ( theGlobals.result );
}

/*****************************************************************************/

OSErr FSCopyFolderLevel( FSCopyObjectGlobals *theGlobals, const HFSUniStr255 *destName )
{
		// If maxLevels is zero, we aren't checking levels
		// If currentLevel < maxLevels, look at this level
	if ( (theGlobals->maxLevels == 0) ||
		 (theGlobals->currentLevel < theGlobals->maxLevels) )
	{
		FSRef				newDirRef;
		UTCDateTime			originalCreateDate;
		FSPermissionInfo	originalPermissions;
		FSIterator			iterator;
		FilterParams		*filterPtr = theGlobals->filterParams;

			// get the info we need on the source file...
		theGlobals->result = FSGetCatalogInfo(	&theGlobals->ref, filterPtr->whichInfo,
												&theGlobals->catalogInfo, &filterPtr->fileName, 
												NULL, NULL);

		if (theGlobals->currentLevel == 0 && destName)
			filterPtr->fileName = *destName;

			// Clear the "inited" bit so that the Finder positions the icon for us.
		((FInfo *)(theGlobals->catalogInfo.finderInfo))->fdFlags &= ~kHasBeenInited;

			// Set the folder's creation date to kMagicBusyCreationDate
			// so that the Finder doesn't mess with the folder while
			// it's copying.  We remember the old value for restoration
			// later.  We only do this if we're not copying to a drop
			// folder, because if we are copying to a drop folder we don't
			// have the opportunity to reset the information at the end of
			// this routine.
		if ( theGlobals->result == noErr && !theGlobals->copyParams->copyingToDropFolder)
		{
			originalCreateDate = theGlobals->catalogInfo.createDate;
			theGlobals->catalogInfo.createDate = theGlobals->copyParams->magicBusyCreateDate;
		}
		
			// we need to have user level read/write/execute access to the folder we are going to create,
			// otherwise FSCreateDirectoryUnicode will return -5000 (afpAccessDenied),
			// and the FSRef returned will be invalid, yet the folder is created...  bug?
		originalPermissions = *((FSPermissionInfo*)theGlobals->catalogInfo.permissions);
		((FSPermissionInfo*)theGlobals->catalogInfo.permissions)->mode |= kRWXUserAccessMask;
		
			// create the new directory
		if( theGlobals->result == noErr )
		{
			theGlobals->result = FSCreateDirectoryUnicode(	&theGlobals->destRef, filterPtr->fileName.length,
															filterPtr->fileName.unicode, kFSCatInfoSettableInfo,
															&theGlobals->catalogInfo, &newDirRef,
															&filterPtr->fileSpec, NULL);
		}

		++theGlobals->currentLevel; // setup to go to the next level

			// With the new APIs, folders can have forks as well as files.  Before
			// we start copying items in the folder, we	must copy over the forks
		if( theGlobals->result == noErr )
			theGlobals->result = CopyItemsForks(&theGlobals->ref, &newDirRef, theGlobals->copyParams);
		if( theGlobals->result == noErr )	// Open FSIterator for flat access to theGlobals->ref
			theGlobals->result = FSOpenIterator(&theGlobals->ref, kFSIterateFlat, &iterator);
		if( theGlobals->result == noErr )
		{
			OSErr		osErr;
		
				// Call FSGetCatalogInfoBulk in loop to get all items in the container 
			do
			{
				theGlobals->result = FSGetCatalogInfoBulk(	iterator, 1, &theGlobals->actualObjects,
															&theGlobals->containerChanged, filterPtr->whichInfo,
															&theGlobals->catalogInfo, &theGlobals->ref, 
															filterPtr->fileSpecPtr, &filterPtr->fileName);
				if ( ( (theGlobals->result == noErr) || (theGlobals->result == errFSNoMoreItems) ) &&
					 ( theGlobals->actualObjects != 0 ) )
				{
						// Any errors in here will be passed to the filter proc
						// we don't want an error in here to prematurely
						// cancel the recursive copy, leaving a half filled directory
						
						// is the new object a directory?
					if ( (theGlobals->catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) != 0 )
					{		// yes
						theGlobals->destRef	= newDirRef;				
						osErr = FSCopyFolderLevel(theGlobals, NULL);
						theGlobals->result = noErr;	// don't want one silly mistake to kill the party...
					}
					else	// no
					{
						osErr = CopyFile(	&theGlobals->ref, &theGlobals->catalogInfo, 
											&newDirRef, &filterPtr->fileName, 
											theGlobals->copyParams, &theGlobals->ref);
					}

						// Call the filter proc _after_ the file/folder was created completly
					if( filterPtr->filterProcPtr != NULL && !theGlobals->quitFlag )
					{
						theGlobals->quitFlag = CallCopyObjectFilterProc(filterPtr->filterProcPtr,
														theGlobals->containerChanged, theGlobals->currentLevel,
														osErr, &theGlobals->catalogInfo, 
														&theGlobals->ref, filterPtr->fileSpecPtr,
														filterPtr->fileNamePtr, filterPtr->yourDataPtr);
					}
				}
			} while ( ( theGlobals->result == noErr ) && ( !theGlobals->quitFlag ) );

				// Close the FSIterator (closing an open iterator should never fail)
			(void) FSCloseIterator(iterator);
		}
		
			// errFSNoMoreItems is OK - it only means we hit the end of this level
			// afpAccessDenied is OK, too - it only means we cannot see inside a directory
		if ( (theGlobals->result == errFSNoMoreItems) || (theGlobals->result == afpAccessDenied) )
			theGlobals->result = noErr;
		
			// store away the name, and an FSSpec and FSRef of the new directory
			// for use in filter proc one level up...
		if( theGlobals->result == noErr )
		{
			theGlobals->ref		= newDirRef;
			theGlobals->result	= FSGetCatalogInfo(&newDirRef, kFSCatInfoNone, NULL, 
												   &filterPtr->fileName, &filterPtr->fileSpec, NULL);
		}
		
			// Return to previous level as we leave 
		--theGlobals->currentLevel; 
		
			// Reset the modification dates and permissions, except when copying to a drop folder
			// where this won't work.
		if (theGlobals->result == noErr && ! theGlobals->copyParams->copyingToDropFolder)
		{
			theGlobals->catalogInfo.createDate = originalCreateDate;
			*((FSPermissionInfo*)theGlobals->catalogInfo.permissions) = originalPermissions;
			theGlobals->result = FSSetCatalogInfo(&newDirRef,  kFSCatInfoCreateDate
															 | kFSCatInfoAttrMod 
															 | kFSCatInfoContentMod
															 | kFSCatInfoPermissions, &theGlobals->catalogInfo);
		}
		
			// If we created the folder and the copy failed, try to clean up by
			// deleting the folder we created.  We do this because, while it's
			// possible for the copy to fail halfway through and the File Manager
			// doesn't really clean up that well, we *really* don't wan't any
			// half-created files/folders being left around.
			// if the file already existed, we don't want to delete it
		if( theGlobals->result != noErr && theGlobals->result != dupFNErr )
			myverify_noerr( FSDeleteObjects(&newDirRef) );
	}

	mycheck_noerr( theGlobals->result );	// put up debug assert in debug builds
	
	return theGlobals->result;
}

/*****************************************************************************/

	// Determines whether the destination directory is equal to the source
	// item, or whether it's nested inside the source item.  Returns a
	// errFSDestInsideSource if that's the case.  We do this to prevent
	// endless recursion while copying.
	//
OSErr CheckForDestInsideSource(const FSRef *source, const FSRef *destDir)
{
	FSRef			thisDir = *destDir;
	FSCatalogInfo	thisDirInfo;
	Boolean			done = false;
	OSErr			osErr;
	
	do
	{
		osErr = FSCompareFSRefs(source, &thisDir);
		if (osErr == noErr)
			osErr = errFSDestInsideSource;
		else if (osErr == diffVolErr)
		{
			osErr = noErr;
			done = true;
		} 
		else if (osErr == errFSRefsDifferent)
		{
			// This is somewhat tricky.  We can ask for the parent of thisDir
			// by setting the parentRef parameter to FSGetCatalogInfo but, if
			// thisDir is the volume's FSRef, this will give us back junk.
			// So we also ask for the parent's dir ID to be returned in the
			// FSCatalogInfo record, and then check that against the node
			// ID of the root's parent (ie 1).  If we match that, we've made
			// it to the top of the hierarchy without hitting source, so
			// we leave with no error.
			
			osErr = FSGetCatalogInfo(&thisDir, kFSCatInfoParentDirID, &thisDirInfo, NULL, NULL, &thisDir);
			if( ( osErr == noErr ) && ( thisDirInfo.parentDirID == fsRtParID ) )
				done = true;
		}
	} while ( osErr == noErr && ! done );
	
	mycheck_noerr( osErr );	// put up debug assert in debug builds

	return osErr;
}

/*****************************************************************************/

#pragma mark ----- Copy Forks -----

OSErr CopyItemsForks(const FSRef *source, const FSRef *dest, CopyParams *params)
{
	ForkTracker		dataFork,
					rsrcFork;
	ForkTrackerPtr 	otherForks;
	ItemCount      	otherForksCount,
					thisForkIndex;
	OSErr			osErr;
	
	dataFork.forkDestRefNum = 0;
	rsrcFork.forkDestRefNum = 0;
	otherForks = NULL;
	otherForksCount = 0;
	
		// Get the constant names for the resource and data fork, which
		// we're going to need inside the copy engine.
	osErr = FSGetDataForkName(&dataFork.forkName);
	if( osErr == noErr )
		osErr = FSGetResourceForkName(&rsrcFork.forkName);
	if( osErr == noErr ) // First determine the list of forks that the source has.
		osErr = CalculateForksToCopy(source, &dataFork, &rsrcFork, &otherForks, &otherForksCount);
	if (osErr == noErr)
	{
		// If we're copying into a drop folder, open up all of those forks.
		// We have to do this because, once we've starting writing to a fork
		// in a drop folder, we can't open any more forks.
		//
		// We only do this if we're copying into a drop folder in order
		// to conserve FCBs in the more common, non-drop folder case.
		
		if (params->copyingToDropFolder)
			osErr = OpenAllForks(dest, &dataFork, &rsrcFork, otherForks, otherForksCount);
	
			// Copy each fork.
		if (osErr == noErr && (dataFork.forkSize != 0))		// copy data fork
			osErr = CopyFork(source, dest, &dataFork, params);
		if (osErr == noErr && (rsrcFork.forkSize != 0))		// copy resource fork
			osErr = CopyFork(source, dest, &rsrcFork, params);
		if (osErr == noErr) {								// copy other forks
			for (thisForkIndex = 0; thisForkIndex < otherForksCount && osErr == noErr; thisForkIndex++)
				osErr = CopyFork(source,dest, &otherForks[thisForkIndex], params);
		}

			// Close any forks that might be left open.  Note that we have to call
			// this regardless of an error.  Also note that this only closes forks
			// that were opened by OpenAllForks.  If we're not copying into a drop
			// folder, the forks are opened and closed by CopyFork.		
		{
			OSErr osErr2 = CloseAllForks(dataFork.forkDestRefNum, rsrcFork.forkDestRefNum, otherForks, otherForksCount);
			mycheck_noerr(osErr2);
			if (osErr == noErr)
				osErr = osErr2;
		}
	}

		// Clean up.	
	if (otherForks != NULL)
		DisposePtr((char*)otherForks);

	mycheck_noerr( osErr );	// put up debug assert in debug builds

	return osErr;
}

/*****************************************************************************/

	// Open all the forks of the file.  We need to do this when we're copying
	// into a drop folder, where you must open all the forks before starting
	// to write to any of them.
	//
	// IMPORTANT:  If it fails, this routine won't close forks that opened successfully.
	// You must call CloseAllForks regardless of whether this routine returns an error.
OSErr OpenAllForks(	const FSRef *dest,
					const ForkTrackerPtr dataFork,
					const ForkTrackerPtr rsrcFork,
					ForkTrackerPtr otherForks,
					ItemCount otherForksCount)
{
	ItemCount	thisForkIndex;
	OSErr		osErr			= noErr;

		// Open the resource and data forks as a special case, if they exist in this object
	if (dataFork->forkSize != 0)						// Data fork never needs to be created, so I don't have to FSCreateFork it here.
		osErr = FSOpenFork(dest, dataFork->forkName.length, dataFork->forkName.unicode, fsWrPerm, &dataFork->forkDestRefNum);
	if (osErr == noErr && rsrcFork->forkSize != 0)	// Resource fork never needs to be created, so I don't have to FSCreateFork it here.
		osErr = FSOpenFork(dest, rsrcFork->forkName.length, rsrcFork->forkName.unicode, fsWrPerm, &rsrcFork->forkDestRefNum);

	if (osErr == noErr && otherForks != NULL && otherForksCount > 0) // Open the other forks.
	{
		for (thisForkIndex = 0; thisForkIndex < otherForksCount && osErr == noErr; thisForkIndex++)
		{
				// Create the fork.  Swallow afpAccessDenied because this operation
				// causes the external file system compatibility shim in Mac OS 9 to
				// generate a GetCatInfo request to the AppleShare external file system,
				// which in turn causes an AFP GetFileDirParms request on the wire,
				// which the AFP server bounces with afpAccessDenied because the file
				// is in a drop folder.  As there's no native support for non-classic
				// forks in current AFP, there's no way I can decide how I should
				// handle this in a non-test case.  So I just swallow the error and
				// hope that when native AFP support arrives, the right thing will happen.
			osErr = FSCreateFork(dest, otherForks[thisForkIndex].forkName.length, otherForks[thisForkIndex].forkName.unicode);
			if (osErr == noErr || osErr == afpAccessDenied)
				osErr = noErr;
			
				// Previously I avoided opening up the fork if the fork if the
				// length was empty, but that confused CopyFork into thinking
				// this wasn't a drop folder copy, so I decided to simply avoid
				// this trivial optimization.  In drop folders, we always open
				// all forks.
			if (osErr == noErr)
				osErr = FSOpenFork(dest, otherForks[thisForkIndex].forkName.length, otherForks[thisForkIndex].forkName.unicode, fsWrPerm, &otherForks[thisForkIndex].forkDestRefNum);
		}
	}

	mycheck_noerr( osErr );	// put up debug assert in debug builds

	return osErr;
}

/*****************************************************************************/

	// Copies the fork whose name is forkName from source to dest.
	// A refnum for the destination fork may be supplied in forkDestRefNum.
	// If forkDestRefNum is 0, we must open the destination fork ourselves,
	// otherwise it has been opened for us and we shouldn't close it.
OSErr CopyFork(	const FSRef *source, const FSRef *dest, const ForkTrackerPtr sourceFork, const CopyParams *params)
{
	UInt64		bytesRemaining;
	UInt64		bytesToReadThisTime;
	UInt64		bytesToWriteThisTime;
	SInt16		sourceRef;
	SInt16		destRef;
	OSErr		osErr = noErr;
	OSErr		osErr2 = noErr;
	
		// If we haven't been passed in a sourceFork->forkDestRefNum (which basically
		// means we're copying into a non-drop folder), create the destination
		// fork.  We have to do this regardless of whether sourceFork->forkSize is 
		// 0, because we want to preserve empty forks.
	if (sourceFork->forkDestRefNum == 0)
	{
		osErr = FSCreateFork(dest, sourceFork->forkName.length, sourceFork->forkName.unicode);
		
			// Mac OS 9.0 has a bug (in the AppleShare external file system,
			// I think) [2410374] that causes FSCreateFork to return an errFSForkExists
			// error even though the fork is empty.  The following code swallows
			// the error (which is harmless) in that case.
		if (osErr == errFSForkExists && !params->copyingToLocalVolume)
			osErr = noErr;
	}

	// The remainder of this code only applies if there is actual data
	// in the source fork.
	
	if (osErr == noErr && sourceFork->forkSize != 0) {

		// Prepare for failure.
		
		sourceRef = 0;
		destRef   = 0;

			// Open up the destination fork, if we're asked to, otherwise
			// just use the passed in sourceFork->forkDestRefNum.	
		if( sourceFork->forkDestRefNum == 0 )
			osErr = FSOpenFork(dest, sourceFork->forkName.length, sourceFork->forkName.unicode, fsWrPerm, &destRef);
		else
			destRef = sourceFork->forkDestRefNum;
		
			// Open up the source fork.
		if (osErr == noErr)
			osErr = FSOpenFork(source, sourceFork->forkName.length, sourceFork->forkName.unicode, fsRdPerm, &sourceRef);

			// Here we create space for the entire fork on the destination volume.
			// FSAllocateFork has the right semantics on both traditional Mac OS
			// and Mac OS X.  On traditional Mac OS it will allocate space for the
			// file in one hit without any other special action.  On Mac OS X,
			// FSAllocateFork is preferable to FSSetForkSize because it prevents
			// the system from zero filling the bytes that were added to the end
			// of the fork (which would be waste becasue we're about to write over
			// those bytes anyway.
		if( osErr == noErr )
			osErr = FSAllocateFork(destRef, kFSAllocNoRoundUpMask, fsFromStart, 0, sourceFork->forkSize, NULL);

			// Copy the file from the source to the destination in chunks of
			// no more than params->copyBufferSize bytes.  This is fairly
			// boring code except for the bytesToReadThisTime/bytesToWriteThisTime
			// distinction.  On the last chunk, we round bytesToWriteThisTime
			// up to the next 512 byte boundary and then, after we exit the loop,
			// we set the file's EOF back to the real location (if the fork size
			// is not a multiple of 512 bytes).
			// 
			// This technique works around a 'bug' in the traditional Mac OS File Manager,
			// where the File Manager will put the last 512-byte block of a large write into
			// the cache (even if we specifically request no caching) if that block is not
			// full. If the block goes into the cache it will eventually have to be
			// flushed, which causes sub-optimal disk performance.
			//
			// This is only done if the destination volume is local.  For a network
			// volume, it's better to just write the last bytes directly.
			//
			// This is extreme over-optimization given the other limits of this
			// sample, but I will hopefully get to the other limits eventually.
		bytesRemaining = sourceFork->forkSize;
		while (osErr == noErr && bytesRemaining != 0)
		{
			if (bytesRemaining > params->copyBufferSize)
			{
				bytesToReadThisTime  = 	params->copyBufferSize;
				bytesToWriteThisTime = 	bytesToReadThisTime;
			}
			else 
			{
				bytesToReadThisTime  = 	bytesRemaining;
				bytesToWriteThisTime =	(params->copyingToLocalVolume) 		?
										(bytesRemaining + 0x01FF) & ~0x01FF :
										bytesRemaining;
			}
			
			osErr = FSReadFork(sourceRef, fsAtMark + noCacheMask, 0, bytesToReadThisTime, params->copyBuffer, NULL);
			if (osErr == noErr)
				osErr = FSWriteFork(destRef, fsAtMark + noCacheMask, 0, bytesToWriteThisTime, params->copyBuffer, NULL);
			if (osErr == noErr)
				bytesRemaining -= bytesToReadThisTime;
		}
		
		if (osErr == noErr && (params->copyingToLocalVolume && ((sourceFork->forkSize & 0x01FF) != 0)) )
			osErr = FSSetForkSize(destRef, fsFromStart, sourceFork->forkSize);

			// Clean up.
		if (sourceRef != 0)
		{
			osErr2 = FSCloseFork(sourceRef);
			mycheck_noerr(osErr2);
			if (osErr == noErr)
				osErr = osErr2;
		}

			// Only close destRef if we were asked to open it (ie sourceFork->forkDestRefNum == 0) and
			// we actually managed to open it (ie destRef != 0).	
		if (sourceFork->forkDestRefNum == 0 && destRef != 0)
		{
			osErr2 = FSCloseFork(destRef);
			mycheck_noerr(osErr2);
			if (osErr == noErr)
				osErr = osErr2;
		}
	}

	mycheck_noerr( osErr );	// put up debug assert in debug builds

	return osErr;
}

/*****************************************************************************/

	// Close all the forks that might have been opened by OpenAllForks.
OSErr CloseAllForks(SInt16 dataRefNum, SInt16 rsrcRefNum, ForkTrackerPtr otherForks, ItemCount otherForksCount)
{
	ItemCount	thisForkIndex;
	OSErr		osErr = noErr,
				osErr2;
	
	if (dataRefNum != 0) 
	{
		osErr2 = FSCloseFork(dataRefNum);
		mycheck_noerr(osErr2);
		if (osErr == noErr)
			osErr = osErr2;
	}
	if (rsrcRefNum != 0)
	{
		osErr2 = FSCloseFork(rsrcRefNum);
		mycheck_noerr(osErr2);
		if (osErr == noErr)
			osErr = osErr2;
	}
	if( otherForks != NULL && otherForksCount > 0 )
	{
		for (thisForkIndex = 0; thisForkIndex < otherForksCount; thisForkIndex++)
		{
			if (otherForks[thisForkIndex].forkDestRefNum != 0)
			{
				osErr2 = FSCloseFork(otherForks[thisForkIndex].forkDestRefNum);
				mycheck_noerr(osErr2);
				if (osErr == noErr)
					osErr = osErr2;
			}
		}
	}
	
	mycheck_noerr( osErr );	// put up debug assert in debug builds

	return osErr;
}

/*****************************************************************************/

	// This routine determines the list of forks that a file has.
	// dataFork is populated if the file has a data fork.
	// rsrcFork is populated if the file has a resource fork.
	// otherForksParam is set to point to a memory block allocated with
	// NewPtr if the file has forks beyond the resource and data
	// forks.  You must free that block with DisposePtr.  otherForksCountParam
	// is set to the number of forks in the otherForksParam
	// array.  This count does *not* include the resource and data forks.
OSErr CalculateForksToCopy(	const FSRef *source,
							const ForkTrackerPtr dataFork,
							const ForkTrackerPtr rsrcFork,
							ForkTrackerPtr *otherForksParam,
							ItemCount      *otherForksCountParam)
{
	Boolean         done;
	CatPositionRec  iterator;
	HFSUniStr255 	thisForkName;
	SInt64          thisForkSize;
	ForkTrackerPtr  otherForks;
	ItemCount       otherForksCount;
	ItemCount       otherForksMemoryBlockCount;
	OSErr			osErr = ( (source != NULL) 	 && (dataFork != NULL) &&
							  (rsrcFork != NULL) && (otherForksParam != NULL) &&
							  (otherForksCountParam != NULL) ) ?
							noErr : paramErr;

	dataFork->forkSize = 0;
	rsrcFork->forkSize = 0;
	otherForks = NULL;
	otherForksCount = 0;
	iterator.initialize = 0;
	done = false;

	// Iterate through the list of forks, processing each fork name in turn.
	while (osErr == noErr && ! done)
	{
		osErr = FSIterateForks(source, &iterator, &thisForkName, &thisForkSize, NULL);
		if (osErr == errFSNoMoreItems)
		{
			osErr = noErr;
			done = true;
		}
		else if (osErr == noErr)
		{
			if ( CompareHFSUniStr255(&thisForkName, &dataFork->forkName) )
				dataFork->forkSize = thisForkSize;
			else if ( CompareHFSUniStr255(&thisForkName, &rsrcFork->forkName) )
				rsrcFork->forkSize = thisForkSize;
			else
			{
					// We've found a fork other than the resource and data forks.
					// We have to add it to the otherForks array.  But the array
					// a) may not have been created yet, and b) may not contain
					// enough elements to hold the new fork.
					
				if (otherForks == NULL)	// The array hasn't been allocated yet, allocate it.
				{
					otherForksMemoryBlockCount = kExpectedForkCount;
					otherForks = ( ForkTracker* ) NewPtr( sizeof(ForkTracker) * kExpectedForkCount );
					if (otherForks == NULL)
						osErr = memFullErr;
				} 	
				else if (otherForksCount == otherForksMemoryBlockCount)
				{	// If the array doesn't contain enough elements, grow it.
					ForkTrackerPtr newOtherForks;
					
					newOtherForks = (ForkTracker*)NewPtr(sizeof(ForkTracker) * (otherForksCount + kExpectedForkCount));
					if( newOtherForks != NULL)
					{
						BlockMoveData(otherForks, newOtherForks, sizeof(ForkTracker) * otherForksCount);
						otherForksMemoryBlockCount += kExpectedForkCount;
						DisposePtr((char*)otherForks);
						otherForks = newOtherForks;					
					}
					else
						osErr = memFullErr;
				}
				
				// If we have no error, we know we have space in the otherForks
				// array to place the new fork.  Put it there and increment the
				// count of forks.
				
				if (osErr == noErr)
				{
					BlockMoveData(&thisForkName, &otherForks[otherForksCount].forkName, sizeof(thisForkName));
					otherForks[otherForksCount].forkSize = thisForkSize;
					otherForks[otherForksCount].forkDestRefNum = 0;
					++otherForksCount;
				}
			}
		}
	}
	
	// Clean up.
	
	if (osErr != noErr)
	{
		if (otherForks != NULL)
		{
			DisposePtr((char*)otherForks);
			otherForks = NULL;
		}
		otherForksCount = 0;
	}
	
	*otherForksParam = otherForks;
	*otherForksCountParam = otherForksCount;

	mycheck_noerr( osErr );	// put up debug assert in debug builds

	return osErr;	
}

/*****************************************************************************/

#pragma mark ----- Calculate Buffer Size -----

OSErr CalculateBufferSize( const FSRef *source, const FSRef *destDir,
						   ByteCount * bufferSize )
{
	FSVolumeRefNum	sourceVRefNum,
					destVRefNum;
	ByteCount		tmpBufferSize = 0;
	OSErr			osErr = ( source != NULL && destDir != NULL && bufferSize != NULL ) ?
							noErr : paramErr;
	
	if( osErr == noErr )
		osErr = FSGetVRefNum( source, &sourceVRefNum );
	if( osErr == noErr )
		osErr = FSGetVRefNum( destDir, &destVRefNum);
	if( osErr == noErr)
	{
		tmpBufferSize = BufferSizeForThisVolume(sourceVRefNum);
		if (destVRefNum != sourceVRefNum)
		{
			ByteCount tmp = BufferSizeForThisVolume(destVRefNum);
			if (tmp < tmpBufferSize)
				tmpBufferSize = tmp;
		}
	}
	
	*bufferSize = tmpBufferSize;

	mycheck_noerr( osErr );	// put up debug assert in debug builds

	return osErr;
}

/*****************************************************************************/

	// This routine calculates the appropriate buffer size for
	// the given vRefNum.  It's a simple composition of FSGetVolParms
	// BufferSizeForThisVolumeSpeed.
ByteCount BufferSizeForThisVolume(FSVolumeRefNum vRefNum)
{
	GetVolParmsInfoBuffer	volParms;
	ByteCount				volumeBytesPerSecond = 0;
	UInt32					actualSize;
	OSErr					osErr;
	
	osErr = FSGetVolParms( vRefNum, sizeof(volParms), &volParms, &actualSize );
	if( osErr == noErr )
	{
		// Version 1 of the GetVolParmsInfoBuffer included the vMAttrib
		// field, so we don't really need to test actualSize.  A noErr
		// result indicates that we have the info we need.  This is
		// just a paranoia check.
		
		mycheck(actualSize >= offsetof(GetVolParmsInfoBuffer, vMVolumeGrade));

		// On the other hand, vMVolumeGrade was not introduced until
		// version 2 of the GetVolParmsInfoBuffer, so we have to explicitly
		// test whether we got a useful value.
		
		if( ( actualSize >= offsetof(GetVolParmsInfoBuffer, vMForeignPrivID) ) &&
			( volParms.vMVolumeGrade <= 0 ) ) 
		{
			volumeBytesPerSecond = -volParms.vMVolumeGrade;
		}
	}

	mycheck_noerr( osErr );	// put up debug assert in debug builds

	return BufferSizeForThisVolumeSpeed(volumeBytesPerSecond);
}

/*****************************************************************************/

	// Calculate an appropriate copy buffer size based on the volumes
	// rated speed.  Our target is to use a buffer that takes 0.25
	// seconds to fill.  This is necessary because the volume might be
	// mounted over a very slow link (like ARA), and if we do a 256 KB
	// read over an ARA link we'll block the File Manager queue for
	// so long that other clients (who might have innocently just
	// called PBGetCatInfoSync) will block for a noticeable amount of time.
	//
	// Note that volumeBytesPerSecond might be 0, in which case we assume
	// some default value.
ByteCount BufferSizeForThisVolumeSpeed(UInt32 volumeBytesPerSecond)
{
	ByteCount bufferSize;
	
	if (volumeBytesPerSecond == 0)
		bufferSize = kDefaultCopyBufferSize;
	else
	{	// We want to issue a single read that takes 0.25 of a second,
		// so devide the bytes per second by 4.
		bufferSize = volumeBytesPerSecond / 4;
	}
	
		// Round bufferSize down to 512 byte boundary.
	bufferSize &= ~0x01FF;
	
		// Clip to sensible limits.
	if (bufferSize < kMinimumCopyBufferSize)
		bufferSize = kMinimumCopyBufferSize;
	else if (bufferSize > kMaximumCopyBufferSize)
		bufferSize = kMaximumCopyBufferSize;
		
	return bufferSize;
}

/*****************************************************************************/

#pragma mark ----- Delete Objects -----

OSErr FSDeleteObjects( const FSRef *source )
{
	FSCatalogInfo	catalogInfo;
	OSErr			osErr = ( source != NULL ) ? noErr : paramErr;
	
		// get nodeFlags for container
	if( osErr == noErr )
		osErr = FSGetCatalogInfo(source, kFSCatInfoNodeFlags, &catalogInfo, NULL, NULL,NULL);
	if( osErr == noErr && (catalogInfo.nodeFlags & kFSNodeIsDirectoryMask) != 0 )
	{		// its a directory, so delete its contents before we delete it
		osErr = FSDeleteFolder(source);
	}
	if( osErr == noErr && (catalogInfo.nodeFlags & kFSNodeLockedMask) != 0 )	// is object locked?
	{		// then attempt to unlock the object (ignore osErr since FSDeleteObject will set it correctly)
		catalogInfo.nodeFlags &= ~kFSNodeLockedMask;
		(void) FSSetCatalogInfo(source, kFSCatInfoNodeFlags, &catalogInfo);
	}		
	if( osErr == noErr )	// delete the object (if it was a directory it is now empty, so we can delete it)
		osErr = FSDeleteObject(source);

	mycheck_noerr( osErr );
	
	return ( osErr );
}

/*****************************************************************************/

#pragma mark ----- Delete Folders -----

OSErr FSDeleteFolder( const FSRef *container )
{
	FSDeleteObjectGlobals	theGlobals;
	
	theGlobals.result = ( container != NULL ) ? noErr : paramErr;
	
		// delete container's contents
	if( theGlobals.result == noErr )
		FSDeleteFolderLevel(container, &theGlobals);
	
	mycheck_noerr( theGlobals.result );
	
	return ( theGlobals.result );
}

/*****************************************************************************/

void FSDeleteFolderLevel(	const FSRef *container,
								FSDeleteObjectGlobals *theGlobals)
{
	FSIterator					iterator;
	FSRef						itemToDelete;
	UInt16						nodeFlags;

		// Open FSIterator for flat access and give delete optimization hint
	theGlobals->result = FSOpenIterator(container, kFSIterateFlat + kFSIterateDelete, &iterator);
	if ( theGlobals->result == noErr )
	{
		do 	// delete the contents of the directory
		{
				// get 1 item to delete
			theGlobals->result = FSGetCatalogInfoBulk(	iterator, 1, &theGlobals->actualObjects,
														NULL, kFSCatInfoNodeFlags, &theGlobals->catalogInfo,
														&itemToDelete, NULL, NULL);
			if ( (theGlobals->result == noErr) && (theGlobals->actualObjects == 1) )
			{
					// save node flags in local in case we have to recurse */
				nodeFlags = theGlobals->catalogInfo.nodeFlags;
				
					// is it a directory?
				if ( (nodeFlags & kFSNodeIsDirectoryMask) != 0 )
				{	// yes -- delete its contents before attempting to delete it */ 
					FSDeleteFolderLevel(&itemToDelete, theGlobals);
				}
				if ( theGlobals->result == noErr)			// are we still OK to delete?
				{	
					if ( (nodeFlags & kFSNodeLockedMask) != 0 )	// is item locked?
					{		// then attempt to unlock it (ignore result since FSDeleteObject will set it correctly)
						theGlobals->catalogInfo.nodeFlags = nodeFlags & ~kFSNodeLockedMask;
						(void) FSSetCatalogInfo(&itemToDelete, kFSCatInfoNodeFlags, &theGlobals->catalogInfo);
					}
						// delete the item
					theGlobals->result = FSDeleteObject(&itemToDelete);
				}
			}
		} while ( theGlobals->result == noErr );
			
			// we found the end of the items normally, so return noErr
		if ( theGlobals->result ==  errFSNoMoreItems )
			theGlobals->result = noErr;
			
			// close the FSIterator (closing an open iterator should never fail)
		myverify_noerr(FSCloseIterator(iterator));
	}

	mycheck_noerr( theGlobals->result );
	
	return;
}

/*****************************************************************************/

#pragma mark ----- Utilities -----

	// Figures out if the given directory is a drop box or not
	// if it is, the Copy Engine will behave slightly differently
OSErr	IsDropBox(const FSRef* source, Boolean *isDropBox)
{
	FSCatalogInfo			tmpCatInfo;
	FSSpec					sourceSpec;
	Boolean					isDrop = false;
	OSErr					osErr;
	
		// get info about the destination, and an FSSpec to it for PBHGetDirAccess
	osErr = FSGetCatalogInfo(source, kFSCatInfoNodeFlags | kFSCatInfoPermissions, &tmpCatInfo, NULL, &sourceSpec, NULL);
	if( osErr == noErr )	// make sure the source is a directory
		osErr = ((tmpCatInfo.nodeFlags & kFSNodeIsDirectoryMask) != 0) ? noErr : errFSNotAFolder;
	if( osErr == noErr )
	{
		HParamBlockRec	hPB;

		BlockZero(&hPB, sizeof( HParamBlockRec ));

		hPB.accessParam.ioNamePtr		= sourceSpec.name;
		hPB.accessParam.ioVRefNum		= sourceSpec.vRefNum;
		hPB.accessParam.ioDirID			= sourceSpec.parID;
		
			// This is the official way (reads: the way X Finder does it) to figure
			// out the current users access privileges to a given directory
		osErr = PBHGetDirAccessSync(&hPB);
		if( osErr == noErr )	// its a drop folder if the current user only has write access
			isDrop = (hPB.accessParam.ioACAccess & kPrivilegesMask) == kioACAccessUserWriteMask;
		else if ( osErr == paramErr )
		{
			// There is a bug (2908703) in the Classic File System (not OS 9.x or Carbon)
			// on 10.1.x where PBHGetDirAccessSync sometimes returns paramErr even when the
			// data passed in is correct.  This is a workaround/hack for that problem,
			// but is not as accurate.
			// Basically, if "Everyone" has only Write/Search access then its a drop folder
			// that is the most common case when its a drop folder
			FSPermissionInfo *tmpPerm = (FSPermissionInfo *)tmpCatInfo.permissions;			
			isDrop = ((tmpPerm->mode & kRWXOtherAccessMask) == kDropFolderValue);
			osErr = noErr;
		}
	}

	*isDropBox = isDrop;

	mycheck_noerr( osErr );
	
	return osErr;
}

	// The copy engine is going to set each item's creation date
	// to kMagicBusyCreationDate while it's copying the item.
	// But kMagicBusyCreationDate is an old-style 32-bit date/time,
	// while the HFS Plus APIs use the new 64-bit date/time.  So
	// we have to call a happy UTC utilities routine to convert from
	// the local time kMagicBusyCreationDate to a UTCDateTime
	// gMagicBusyCreationDate, which the File Manager will store
	// on disk and which the Finder we read back using the old
	// APIs, whereupon the File Manager will convert it back
	// to local time (and hopefully get the kMagicBusyCreationDate
	// back!).
OSErr	GetMagicBusyCreationDate( UTCDateTime *date )
{
	UTCDateTime		tmpDate = {0,0,0};
	OSErr			osErr = ( date != NULL ) ? noErr : paramErr;
				
	if( osErr == noErr )
		osErr = ConvertLocalTimeToUTC(kMagicBusyCreationDate, &tmpDate.lowSeconds);
	if( osErr == noErr )
		*date = tmpDate;
		
	mycheck_noerr( osErr );	// put up debug assert in debug builds

	return osErr;
}

	// compares two HFSUniStr255 for equality
	// return true if they are identical, false if not
Boolean CompareHFSUniStr255(const HFSUniStr255 *lhs, const HFSUniStr255 *rhs)
{
	return (lhs->length == rhs->length)
			&& (memcmp(lhs->unicode, rhs->unicode, lhs->length * sizeof(UniChar)) == 0);
}

/*****************************************************************************/

OSErr FSGetVRefNum(const FSRef *ref, FSVolumeRefNum *vRefNum)
{
	FSCatalogInfo	catalogInfo;
	OSErr			osErr = ( ref != NULL && vRefNum != NULL ) ? noErr : paramErr;

	if( osErr == noErr )	/* get the volume refNum from the FSRef */
		osErr = FSGetCatalogInfo(ref, kFSCatInfoVolume, &catalogInfo, NULL, NULL, NULL);
	if( osErr == noErr )
		*vRefNum = catalogInfo.volume;
		
	mycheck_noerr( osErr );

	return osErr;
}

/*****************************************************************************/ 

OSErr FSGetVolParms(	FSVolumeRefNum volRefNum,
						UInt32 bufferSize,
						GetVolParmsInfoBuffer *volParmsInfo,
						UInt32 *actualInfoSize)		/*	Can Be NULL	*/
{
	HParamBlockRec	pb;
	OSErr			osErr = ( volParmsInfo != NULL ) ? noErr : paramErr;
		
	if( osErr == noErr )
	{
		pb.ioParam.ioNamePtr = NULL;
		pb.ioParam.ioVRefNum = volRefNum;
		pb.ioParam.ioBuffer = (Ptr)volParmsInfo;
		pb.ioParam.ioReqCount = (SInt32)bufferSize;
		osErr = PBHGetVolParmsSync(&pb);
	}
		/* return number of bytes the file system returned in volParmsInfo buffer */
	if( osErr == noErr && actualInfoSize != NULL)
		*actualInfoSize = (UInt32)pb.ioParam.ioActCount;

	mycheck_noerr( osErr );	// put up debug assert in debug builds

	return ( osErr );
}

/*****************************************************************************/

OSErr UnicodeNameGetHFSName(	UniCharCount nameLength,
								const UniChar *name,
								TextEncoding textEncodingHint,
								Boolean isVolumeName,
								Str31 hfsName)
{
	UnicodeMapping		uMapping;
	UnicodeToTextInfo	utInfo;
	ByteCount			unicodeByteLength;
	ByteCount			unicodeBytesConverted;
	ByteCount			actualPascalBytes;
	OSErr				osErr = (hfsName != NULL && name != NULL ) ? noErr : paramErr;
		
		// make sure output is valid in case we get errors or there's nothing to convert
	hfsName[0] = 0;
	
	unicodeByteLength = nameLength * sizeof(UniChar);
	if ( unicodeByteLength == 0 )	
		osErr = noErr;		/* do nothing */
	else
	{
			// if textEncodingHint is kTextEncodingUnknown, get a "default" textEncodingHint
		if ( kTextEncodingUnknown == textEncodingHint )
		{
			ScriptCode			script;
			RegionCode			region;
			
			script = (ScriptCode)GetScriptManagerVariable(smSysScript);
			region = (RegionCode)GetScriptManagerVariable(smRegionCode);
			osErr = UpgradeScriptInfoToTextEncoding(script, kTextLanguageDontCare, 
													region, NULL, &textEncodingHint );
			if ( osErr == paramErr )
			{		// ok, ignore the region and try again
				osErr = UpgradeScriptInfoToTextEncoding(script, kTextLanguageDontCare,
														kTextRegionDontCare, NULL, 
														&textEncodingHint );
			}
			if ( osErr != noErr )			// ok... try something
				textEncodingHint = kTextEncodingMacRoman;
		}
		
		uMapping.unicodeEncoding	= CreateTextEncoding(	kTextEncodingUnicodeV2_0,
															kUnicodeCanonicalDecompVariant, 
															kUnicode16BitFormat);
		uMapping.otherEncoding		= GetTextEncodingBase(textEncodingHint);
		uMapping.mappingVersion		= kUnicodeUseHFSPlusMapping;
	
		osErr = CreateUnicodeToTextInfo(&uMapping, &utInfo);
		if( osErr == noErr )
		{
			osErr = ConvertFromUnicodeToText(	utInfo, unicodeByteLength, name, kUnicodeLooseMappingsMask,
												0, NULL, 0, NULL,	/* offsetCounts & offsetArrays */
												isVolumeName ? kHFSMaxVolumeNameChars : kHFSMaxFileNameChars,
												&unicodeBytesConverted, &actualPascalBytes, &hfsName[1]);
		}
		if( osErr == noErr )
			hfsName[0] = actualPascalBytes;
		
			// verify the result in debug builds -- there's really not anything you can do if it fails
		myverify_noerr(DisposeUnicodeToTextInfo(&utInfo));
	}

	mycheck_noerr( osErr );	// put up debug assert in debug builds
	
	return ( osErr );
}

/*****************************************************************************/

OSErr FSMakeFSRef(	FSVolumeRefNum volRefNum,
					SInt32 dirID,
					ConstStr255Param name,
					FSRef *ref)
{
	FSRefParam	pb;
	OSErr		osErr = ( ref != NULL ) ? noErr : paramErr;
	
	if( osErr == noErr )
	{
		pb.ioVRefNum = volRefNum;
		pb.ioDirID = dirID;
		pb.ioNamePtr = (StringPtr)name;
		pb.newRef = ref;
		osErr = PBMakeFSRefSync(&pb);
	}
	
	mycheck_noerr( osErr );	// put up debug assert in debug builds
		
	return ( osErr );
}
