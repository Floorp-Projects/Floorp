/*
	File:		MoreFilesX.h

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

		 <3>	 4/19/02	JL		[2853905]  Fixed #if test around header includes.
		 <2>	 4/19/02	JL		[2853901]  Updated standard disclaimer.
		 <1>	 1/25/02	JL		MoreFilesX 1.0
	
	Notes:
		What do those arrows in the documentation for each routine mean?
			
			--> The parameter is an input
			
			<-- The parameter is an output. The pointer to the variable
				where the output will be returned (must not be NULL).
			
			<** The parameter is an optional output. If it is not a
				NULL pointer, it points to the variable where the output
				will be returned. If it is a NULL pointer, the output will
				not be returned and will possibly let the routine and the
				File Manager do less work. If you don't need an optional output,
				don't ask for it.
			**> The parameter is an optional input. If it is not a
				NULL pointer, it points to the variable containing the
				input data. If it is a NULL pointer, the input is not used
				and will possibly let the routine and the File Manager
				do less work.
*/

#ifndef __MOREFILESX__
#define __MOREFILESX__

#ifndef __CARBON__
	#if defined(__MACH__)
		#include <Carbon/Carbon.h>
	#else
		#include <Carbon.h>
	#endif
#endif

#if PRAGMA_ONCE
#pragma once
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if PRAGMA_IMPORT
#pragma import on
#endif

#if PRAGMA_STRUCT_ALIGN
	#pragma options align=mac68k
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(push, 2)
#elif PRAGMA_STRUCT_PACK
	#pragma pack(2)
#endif

/*****************************************************************************/

#pragma mark ----- FinderInfo and ExtendedFinderInfo -----

/*
 *	FSGetFinderInfo and FSSetFinderInfo use these unions for Finder information.
 */

union FinderInfo
{
  FileInfo				file;
  FolderInfo			folder;
};
typedef union FinderInfo FinderInfo;

union ExtendedFinderInfo
{
  ExtendedFileInfo		file;
  ExtendedFolderInfo	folder;
};
typedef union ExtendedFinderInfo ExtendedFinderInfo;

/*****************************************************************************/

#pragma mark ----- GetVolParmsInfoBuffer Macros -----

/*
 *	Macros to get information out of GetVolParmsInfoBuffer.
 */

/* version 1 field getters */
#define GetVolParmsInfoVersion(volParms) \
		((volParms)->vMVersion)
#define GetVolParmsInfoAttrib(volParms) \
		((volParms)->vMAttrib)
#define GetVolParmsInfoLocalHand(volParms) \
		((volParms)->vMLocalHand)
#define GetVolParmsInfoServerAdr(volParms) \
		((volParms)->vMServerAdr)

/* version 2 field getters (assume zero result if version < 2) */
#define GetVolParmsInfoVolumeGrade(volParms) \
		(((volParms)->vMVersion >= 2) ? (volParms)->vMVolumeGrade : 0)
#define GetVolParmsInfoForeignPrivID(volParms) \
		(((volParms)->vMVersion >= 2) ? (volParms)->vMForeignPrivID : 0)

/* version 3 field getters (assume zero result if version < 3) */
#define GetVolParmsInfoExtendedAttributes(volParms) \
		(((volParms)->vMVersion >= 3) ? (volParms)->vMExtendedAttributes : 0)

/* attribute bits supported by all versions of GetVolParmsInfoBuffer */
#define VolIsNetworkVolume(volParms) \
		((volParms)->vMServerAdr != 0)
#define VolHasLimitFCBs(volParms) \
		(((volParms)->vMAttrib & (1L << bLimitFCBs)) != 0)
#define VolHasLocalWList(volParms) \
		(((volParms)->vMAttrib & (1L << bLocalWList)) != 0)
#define VolHasNoMiniFndr(volParms) \
		(((volParms)->vMAttrib & (1L << bNoMiniFndr)) != 0)
#define VolHasNoVNEdit(volParms) \
		(((volParms)->vMAttrib & (1L << bNoVNEdit)) != 0)
#define VolHasNoLclSync(volParms) \
		(((volParms)->vMAttrib & (1L << bNoLclSync)) != 0)
#define VolHasTrshOffLine(volParms) \
		(((volParms)->vMAttrib & (1L << bTrshOffLine)) != 0)
#define VolHasNoSwitchTo(volParms) \
		(((volParms)->vMAttrib & (1L << bNoSwitchTo)) != 0)
#define VolHasNoDeskItems(volParms) \
		(((volParms)->vMAttrib & (1L << bNoDeskItems)) != 0)
#define VolHasNoBootBlks(volParms) \
		(((volParms)->vMAttrib & (1L << bNoBootBlks)) != 0)
#define VolHasAccessCntl(volParms) \
		(((volParms)->vMAttrib & (1L << bAccessCntl)) != 0)
#define VolHasNoSysDir(volParms) \
		(((volParms)->vMAttrib & (1L << bNoSysDir)) != 0)
#define VolHasExtFSVol(volParms) \
		(((volParms)->vMAttrib & (1L << bHasExtFSVol)) != 0)
#define VolHasOpenDeny(volParms) \
		(((volParms)->vMAttrib & (1L << bHasOpenDeny)) != 0)
#define VolHasCopyFile(volParms) \
		(((volParms)->vMAttrib & (1L << bHasCopyFile)) != 0)
#define VolHasMoveRename(volParms) \
		(((volParms)->vMAttrib & (1L << bHasMoveRename)) != 0)
#define VolHasDesktopMgr(volParms) \
		(((volParms)->vMAttrib & (1L << bHasDesktopMgr)) != 0)
#define VolHasShortName(volParms) \
		(((volParms)->vMAttrib & (1L << bHasShortName)) != 0)
#define VolHasFolderLock(volParms) \
		(((volParms)->vMAttrib & (1L << bHasFolderLock)) != 0)
#define VolHasPersonalAccessPrivileges(volParms) \
		(((volParms)->vMAttrib & (1L << bHasPersonalAccessPrivileges)) != 0)
#define VolHasUserGroupList(volParms) \
		(((volParms)->vMAttrib & (1L << bHasUserGroupList)) != 0)
#define VolHasCatSearch(volParms) \
		(((volParms)->vMAttrib & (1L << bHasCatSearch)) != 0)
#define VolHasFileIDs(volParms) \
		(((volParms)->vMAttrib & (1L << bHasFileIDs)) != 0)
#define VolHasBTreeMgr(volParms) \
		(((volParms)->vMAttrib & (1L << bHasBTreeMgr)) != 0)
#define VolHasBlankAccessPrivileges(volParms) \
		(((volParms)->vMAttrib & (1L << bHasBlankAccessPrivileges)) != 0)
#define VolSupportsAsyncRequests(volParms) \
		(((volParms)->vMAttrib & (1L << bSupportsAsyncRequests)) != 0)
#define VolSupportsTrashVolumeCache(volParms) \
		(((volParms)->vMAttrib & (1L << bSupportsTrashVolumeCache)) != 0)

/* attribute bits supported by version 3 and greater versions of GetVolParmsInfoBuffer */
#define VolIsEjectable(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bIsEjectable)) != 0)
#define VolSupportsHFSPlusAPIs(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bSupportsHFSPlusAPIs)) != 0)
#define VolSupportsFSCatalogSearch(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bSupportsFSCatalogSearch)) != 0)
#define VolSupportsFSExchangeObjects(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bSupportsFSExchangeObjects)) != 0)
#define VolSupports2TBFiles(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bSupports2TBFiles)) != 0)
#define VolSupportsLongNames(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bSupportsLongNames)) != 0)
#define VolSupportsMultiScriptNames(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bSupportsMultiScriptNames)) != 0)
#define VolSupportsNamedForks(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bSupportsNamedForks)) != 0)
#define VolSupportsSubtreeIterators(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bSupportsSubtreeIterators)) != 0)
#define VolL2PCanMapFileBlocks(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bL2PCanMapFileBlocks)) != 0)
#define VolParentModDateChanges(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bParentModDateChanges)) != 0)
#define VolAncestorModDateChanges(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bAncestorModDateChanges)) != 0)
#define VolSupportsSymbolicLinks(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bSupportsSymbolicLinks)) != 0)
#define VolIsAutoMounted(volParms) \
		((GetVolParmsInfoExtendedAttributes(volParms) & (1L << bIsAutoMounted)) != 0)

/*****************************************************************************/

#pragma mark ----- userPrivileges Bit Masks and Macros -----

/*
 *	Bit masks and macros to get common information out of userPrivileges byte
 *	returned by FSGetCatalogInfo.
 *
 *	Note:	The userPrivileges byte is the same as the ioACUser byte returned
 *			by PBGetCatInfo, and is the 1's complement of the user's privileges
 *			byte returned in ioACAccess by PBHGetDirAccess. That's where the
 *			ioACUser names came from.
 *
 *			The userPrivileges are user's effective privileges based on the
 *			user ID and the groups that user belongs to, and the owner, group,
 *			and everyone privileges for the given directory.
 */

enum
{
	/* mask for just the access restriction bits */
	kioACUserAccessMask		= (kioACUserNoSeeFolderMask +
							   kioACUserNoSeeFilesMask +
							   kioACUserNoMakeChangesMask),
	/* common access privilege settings */
	kioACUserFull			= 0x00, /* no access restiction bits on */
	kioACUserNone			= kioACUserAccessMask, /* all access restiction bits on */
	kioACUserDropBox		= (kioACUserNoSeeFolderMask +
							   kioACUserNoSeeFilesMask), /* make changes, but not see files or folders */
	kioACUserBulletinBoard	= kioACUserNoMakeChangesMask /* see files and folders, but not make changes */
};


/* Macros for testing ioACUser bits. */

#define UserIsOwner(userPrivileges) \
		(((userPrivileges) & kioACUserNotOwnerMask) == 0)
#define UserHasFullAccess(userPrivileges)	\
		(((userPrivileges) & (kioACUserAccessMask)) == kioACUserFull)
#define UserHasDropBoxAccess(userPrivileges)	\
		(((userPrivileges) & kioACUserAccessMask) == kioACUserDropBox)
#define UserHasBulletinBoard(userPrivileges)	\
		(((userPrivileges) & kioACUserAccessMask) == kioACUserBulletinBoard)
#define UserHasNoAccess(userPrivileges)		\
		(((userPrivileges) & kioACUserAccessMask) == kioACUserNone)

/*****************************************************************************/

#pragma mark ----- File Access Routines -----

/*****************************************************************************/

#pragma mark FSCopyFork

OSErr
FSCopyFork(
	SInt16 srcRefNum,
	SInt16 dstRefNum,
	void *copyBufferPtr,
	ByteCount copyBufferSize);

/*
	The FSCopyFork function copies all data from the source fork to the
	destination fork of open file forks and makes sure the destination EOF
	is equal to the source EOF.

	srcRefNum			--> The source file reference number.
	dstRefNum			--> The destination file reference number.
	copyBufferPtr		--> Pointer to buffer to use during copy. The
							buffer should be at least 4K-bytes minimum.
							The larger the buffer, the faster the copy
							(up to a point).
	copyBufferSize		--> The size of the copy buffer.
*/

/*****************************************************************************/

#pragma mark ----- Volume Access Routines -----

/*****************************************************************************/

#pragma mark FSGetVolParms

OSErr
FSGetVolParms(
	FSVolumeRefNum volRefNum,
	UInt32 bufferSize,
	GetVolParmsInfoBuffer *volParmsInfo,
	UInt32 *actualInfoSize);

/*
	The FSGetVolParms function returns information about the characteristics
	of a volume. A result of paramErr usually just means the volume doesn't
	support GetVolParms and the feature you were going to check
	for isn't available.
	
	volRefNum			--> Volume specification.
	bufferSize			--> Size of buffer pointed to by volParmsInfo.
	volParmsInfo		<-- A GetVolParmsInfoBuffer record where the volume
							attributes information is returned.
	actualInfoSize		<-- The number of bytes actually returned
							in volParmsInfo.
	
	__________
	
	Also see:	The GetVolParmsInfoBuffer Macros for checking attribute bits
				in this file
*/

/*****************************************************************************/

#pragma mark FSGetVRefNum

OSErr
FSGetVRefNum(
	const FSRef *ref,
	FSVolumeRefNum *vRefNum);

/*
	The FSGetVRefNum function determines the volume reference
	number of a volume from a FSRef.

	ref					--> The FSRef.
	vRefNum				<-- The volume reference number.
*/

/*****************************************************************************/

#pragma mark FSGetVInfo

OSErr
FSGetVInfo(
	FSVolumeRefNum volume,
	HFSUniStr255 *volumeName,	/* can be NULL */
	UInt64 *freeBytes,			/* can be NULL */
	UInt64 *totalBytes);		/* can be NULL */

/*
	The FSGetVInfo function returns the name, available space (in bytes),
	and total space (in bytes) for the specified volume.

	volume				--> The volume reference number.
	volumeName			<** An optional pointer to a HFSUniStr255.
							If not NULL, the volume name will be returned in
							the HFSUniStr255.
	freeBytes			<** An optional pointer to a UInt64.
							If not NULL, the number of free bytes on the
							volume will be returned in the UInt64.
	totalBytes			<** An optional pointer to a UInt64.
							If not NULL, the total number of bytes on the
							volume will be returned in the UInt64.
*/

/*****************************************************************************/

#pragma mark FSGetVolFileSystemID

OSErr
FSGetVolFileSystemID(
	FSVolumeRefNum volume,
	UInt16 *fileSystemID,	/* can be NULL */
	UInt16 *signature);		/* can be NULL */

/*
	The FSGetVolFileSystemID function returns the file system ID and signature
	of a mounted volume. The file system ID identifies the file system
	that handles requests to a particular volume. The signature identifies the
	volume type of the volume (for example, FSID 0 is Macintosh HFS Plus, HFS
	or MFS, where a signature of 0x4244 identifies the volume as HFS).
	Here's a partial list of file system ID numbers (only Apple's file systems
	are listed):
		FSID	File System
		-----	-----------------------------------------------------
		$0000	Macintosh HFS Plus, HFS or MFS
		$0100	ProDOS File System
		$0101	PowerTalk Mail Enclosures
		$4147	ISO 9660 File Access (through Foreign File Access)
		$4242	High Sierra File Access (through Foreign File Access)
		$464D	QuickTake File System (through Foreign File Access)
		$4953	Macintosh PC Exchange (MS-DOS)
		$4A48	Audio CD Access (through Foreign File Access)
		$4D4B	Apple Photo Access (through Foreign File Access)
		$6173	AppleShare (later versions of AppleShare only)
	
	See the Technical Note "FL 35 - Determining Which File System
	Is Active" and the "Guide to the File System Manager" for more
	information.
	
	volume				--> The volume reference number.
	fileSystemID		<** An optional pointer to a UInt16.
							If not NULL, the volume's file system ID will
							be returned in the UInt16.
	signature			<** An optional pointer to a UInt16.
							If not NULL, the volume's signature will
							be returned in the UInt16.
*/

/*****************************************************************************/

#pragma mark FSGetMountedVolumes

OSErr
FSGetMountedVolumes(
	FSRef ***volumeRefsHandle,	/* pointer to handle of FSRefs */
	ItemCount *numVolumes);

/*
	The FSGetMountedVolumes function returns the list of volumes currently
	mounted in an array of FSRef records. The array of FSRef records is
	returned in a Handle, volumeRefsHandle, which is allocated by
	FSGetMountedVolumes. The caller is responsible for disposing of
	volumeRefsHandle if the FSGetMountedVolumes returns noErr.
		
	volumeRefsHandle	<-- Pointer to an FSRef Handle where the array of
							FSRefs is to be returned.
	numVolumes			<-- The number of volumes returned in the array.
*/

/*****************************************************************************/

#pragma mark ----- FSRef/FSpec/Path/Name Conversion Routines -----

/*****************************************************************************/

#pragma mark FSRefMakeFSSpec

OSErr
FSRefMakeFSSpec(
	const FSRef *ref,
	FSSpec *spec);

/*
	The FSRefMakeFSSpec function returns an FSSpec for the file or
	directory specified by the ref parameter.

	ref					--> An FSRef specifying the file or directory.
	spec				<-- The FSSpec.
*/

/*****************************************************************************/

#pragma mark FSMakeFSRef

OSErr
FSMakeFSRef(
	FSVolumeRefNum volRefNum,
	SInt32 dirID,
	ConstStr255Param name,
	FSRef *ref);

/*
	The FSMakeFSRef function creates an FSRef from the traditional
	volume reference number, directory ID and pathname inputs. It is
	functionally equivalent to FSMakeFSSpec followed by FSpMakeFSRef.
	
	volRefNum			--> Volume specification.
	dirID				--> Directory specification.
	name				--> The file or directory name, or NULL.
	ref					<-- The FSRef.
*/

/*****************************************************************************/

#pragma mark FSMakePath

OSStatus
FSMakePath(
	SInt16 vRefNum,
	SInt32 dirID,
	ConstStr255Param name,
	UInt8 *path,
	UInt32 maxPathSize);

/*
	The FSMakePath function creates a pathname from the traditional volume reference
	number, directory ID, and pathname inputs. It is functionally equivalent to
	FSMakeFSSpec, FSpMakeFSRef, FSRefMakePath.
	
	volRefNum			--> Volume specification.
	dirID				--> Directory specification.
	name				--> The file or directory name, or NULL.
	path				<-- A pointer to a buffer which FSMakePath will
							fill with a C string representing the pathname
							to the file or directory specified. The format of
							the pathname returned can be determined with the
							Gestalt selector gestaltFSAttr's
							gestaltFSUsesPOSIXPathsForConversion bit.
							If the gestaltFSUsesPOSIXPathsForConversion bit is
							clear, the pathname is a Mac OS File Manager full
							pathname in a C string, and file or directory names
							in the pathname may be mangled as returned by
							the File Manager. If the
							gestaltFSUsesPOSIXPathsForConversion bit is set,
							the pathname is a UTF8 encoded POSIX absolute
							pathname in a C string. In either case, the
							pathname returned can be passed back to
							FSPathMakeRef to create an FSRef to the file or
							directory, or FSPathMakeFSSpec to craete an FSSpec
							to the file or directory.
	maxPathSize			--> The size of the path buffer in bytes. If the path
							buffer is too small for the pathname string,
							FSMakePath returns pathTooLongErr or
							buffersTooSmall.
*/

/*****************************************************************************/

#pragma mark FSPathMakeFSSpec

OSStatus
FSPathMakeFSSpec(
	const UInt8 *path,
	FSSpec *spec,
	Boolean *isDirectory);	/* can be NULL */

/*
	The FSPathMakeFSSpec function converts a pathname to an FSSpec.
	
	path				--> A pointer to a C String that is the pathname. The
							format of the pathname you must supply can be
							determined with the Gestalt selector gestaltFSAttr's
							gestaltFSUsesPOSIXPathsForConversion bit.
							If the gestaltFSUsesPOSIXPathsForConversion bit is
							clear, the pathname must be a Mac OS File Manager
							full pathname in a C string. If the
							gestaltFSUsesPOSIXPathsForConversion bit is set,
							the pathname must be a UTF8 encoded POSIX absolute
							pathname in a C string.
	spec				<-- The FSSpec.
	isDirectory			<** An optional pointer to a Boolean.
							If not NULL, true will be returned in the Boolean
							if the specified path is a directory, or false will
							be returned in the Boolean if the specified path is
							a file.
*/

/*****************************************************************************/

#pragma mark UnicodeNameGetHFSName

OSErr
UnicodeNameGetHFSName(
	UniCharCount nameLength,
	const UniChar *name,
	TextEncoding textEncodingHint,
	Boolean isVolumeName,
	Str31 hfsName);

/*
	The UnicodeNameGetHFSName function converts a Unicode string
	to a Pascal Str31 (or Str27) string using an algorithm similar to that used
	by the File Manager. Note that if the name is too long or cannot be converted
	using the given text encoding hint, you will get an error instead of the
	mangled name that the File Manager would return.
	
	nameLength			--> Number of UniChar in name parameter.
	name				--> The Unicode string to convert.
	textEncodingHint	--> The text encoding hint used for the conversion.
							You can pass kTextEncodingUnknown to use the
							"default" textEncodingHint.
	isVolumeName		--> If true, the output name will be limited to
							27 characters (kHFSMaxVolumeNameChars). If false,
							the output name will be limited to 31 characters
							(kHFSMaxFileNameChars).
	hfsName				<-- The hfsName as a Pascal string.
	
	__________
	
	Also see:	HFSNameGetUnicodeName
*/

/*****************************************************************************/

#pragma mark HFSNameGetUnicodeName

OSErr
HFSNameGetUnicodeName(
	ConstStr31Param hfsName,
	TextEncoding textEncodingHint,
	HFSUniStr255 *unicodeName);

/*
	The HFSNameGetUnicodeName function converts a Pascal Str31 string to an
	Unicode HFSUniStr255 string using the same routines as the File Manager.
	
	hfsName				--> The Pascal string to convert.
	textEncodingHint	--> The text encoding hint used for the conversion.
							You can pass kTextEncodingUnknown to use the
							"default" textEncodingHint.
	unicodeName			<-- The Unicode string.
	
	__________
	
	Also see:	UnicodeNameGetHFSName
*/

/*****************************************************************************/

#pragma mark ----- File/Directory Manipulation Routines -----

/*****************************************************************************/

#pragma mark FSRefValid

Boolean FSRefValid(const FSRef *ref);

/*
	The FSRefValid function determines if an FSRef is valid. If the result is
	true, then the FSRef refers to an existing file or directory.
	
	ref					--> FSRef to a file or directory.
*/

/*****************************************************************************/

#pragma mark FSGetParentRef

OSErr
FSGetParentRef(
	const FSRef *ref,
	FSRef *parentRef);

/*
	The FSGetParentRef function gets the parent directory FSRef of the
	specified object.
	
	Note: FSRefs always point to real file system objects. So, there cannot
	be a FSRef to the parent of volume root directories. If you call
	FSGetParentRef with a ref to the root directory of a volume, the
	function result will be noErr and the parentRef will be invalid (using it
	for other file system requests will fail).

	ref					--> FSRef to a file or directory.
	parentRef			<-- The parent directory's FSRef.
*/

/*****************************************************************************/

#pragma mark FSGetFileDirName

OSErr
FSGetFileDirName(
	const FSRef *ref,
	HFSUniStr255 *outName);

/*
	The FSGetFileDirName function gets the name of the file or directory
	specified.

	ref					--> FSRef to a file or directory.
	outName				<-- The file or directory name.
*/

/*****************************************************************************/

#pragma mark FSGetNodeID

OSErr
FSGetNodeID(
	const FSRef *ref,
	long *nodeID,			/* can be NULL */
	Boolean *isDirectory);	/* can be NULL */

/*
	The GetNodeIDFromFSRef function gets the node ID number of the
	file or directory specified (note: the node ID is the directory ID
	for directories).

	ref					--> FSRef to a file or directory.
	nodeID				<** An optional pointer to a long.
							If not NULL, the node ID will be returned in
							the long.
	isDirectory			<** An optional pointer to a Boolean.
							If not NULL, true will be returned in the Boolean
							if the object is a directory, or false will be
							returned in the Boolean if object is a file.
*/

/*****************************************************************************/

#pragma mark FSGetUserPrivilegesPermissions

OSErr
FSGetUserPrivilegesPermissions(
	const FSRef *ref,
	UInt8 *userPrivileges,		/* can be NULL */
	UInt32 permissions[4]); 	/* can be NULL */

/*
	The FSGetUserPrivilegesPermissions function gets the userPrivileges and/or
	permissions of the file or directory specified.

	ref					--> FSRef to a file or directory.
	userPrivileges		<** An optional pointer to a UInt8.
							If not NULL, the userPrivileges will be returned
							in the UInt8.
	permissions			<** An optional pointer to an UInt32[4] array.
							If not NULL, the permissions will be returned
							in the UInt32[4] array.
*/

/*****************************************************************************/

#pragma mark FSCheckLock

OSErr
FSCheckLock(
	const FSRef *ref);

/*
	The FSCheckLock function determines if a file or directory is locked.
	If FSCheckLock returns noErr, then the file or directory is not locked
	and the volume it is on is not locked either. If FSCheckLock returns
	fLckdErr, then the file or directory is locked. If FSCheckLock returns
	wPrErr, then the volume is locked by hardware (i.e., locked tab on
	removable media). If FSCheckLock returns vLckdErr, then the volume is
	locked by software.
	
	ref					--> FSRef to a file or directory.
*/

/*****************************************************************************/

#pragma mark FSGetForkSizes

OSErr
FSGetForkSizes(
	const FSRef *ref,
	UInt64 *dataLogicalSize,	/* can be NULL */
	UInt64 *rsrcLogicalSize);	/* can be NULL */

/*
	The FSGetForkSizes returns the size of the data and/or resource fork for
	the specified file.
	
	ref					--> FSRef to a file or directory.
	dataLogicalSize		<** An optional pointer to a UInt64.
							If not NULL, the data fork's size will be
							returned in the UInt64.
	rsrcLogicalSize		<** An optional pointer to a UInt64.
							If not NULL, the resource fork's size will be
							returned in the UInt64.
	
	__________
	
	Also see:	FSGetTotalForkSizes
*/

/*****************************************************************************/

#pragma mark FSGetTotalForkSizes

OSErr
FSGetTotalForkSizes(
	const FSRef *ref,
	UInt64 *totalLogicalSize,	/* can be NULL */
	UInt64 *totalPhysicalSize,	/* can be NULL */
	ItemCount *forkCount);		/* can be NULL */

/*
	The FSGetTotalForkSizes returns the total logical size and/or the total
	physical size of the specified file (i.e., it adds the sizes of all file
	forks). It optionally returns the number of file forks.
	
	ref					--> FSRef to a file or directory.
	totalLogicalSize	<** An optional pointer to a UInt64.
							If not NULL, the sum of all fork logical sizes
							will be returned in the UInt64.
	totalPhysicalSize	<** An optional pointer to a UInt64.
							If not NULL, the sum of all fork physical sizes
							will be returned in the UInt64.
	forkCount			<** An optional pointer to a ItemCount.
							If not NULL, the number of file forks
							will be returned in the ItemCount.
	
	__________
	
	Also see:	FSGetForkSizes
*/

/*****************************************************************************/

#pragma mark FSBumpDate

OSErr
FSBumpDate(
	const FSRef *ref);

/*
	The FSBumpDate function changes the content modification date of a file
	or directory to the current date/time. If the content modification date
	is already equal to the current date/time, then add one second to the
	content modification date.

	ref					--> FSRef to a file or directory.
*/

/*****************************************************************************/

#pragma mark FSGetFinderInfo

OSErr
FSGetFinderInfo(
	const FSRef *ref,
	FinderInfo *info,					/* can be NULL */
	ExtendedFinderInfo *extendedInfo,	/* can be NULL */
	Boolean *isDirectory);				/* can be NULL */

/*
	The FSGetFinderInfo function gets the finder information for a file or
	directory.

	ref					--> FSRef to a file or directory.
	info				<** An optional pointer to a FinderInfo.
							If not NULL, the FileInfo (if ref is a file) or
							the FolderInfo (if ref is a folder) will be
							returned in the FinderInfo.
	extendedInfo		<** An optional pointer to a ExtendedFinderInfo.
							If not NULL, the ExtendedFileInfo (if ref is a file)
							or the ExtendedFolderInfo (if ref is a folder) will
							be returned in the ExtendedFinderInfo.
	isDirectory			<** An optional pointer to a Boolean.
							If not NULL, true will be returned in the Boolean
							if the object is a directory, or false will be
							returned in the Boolean if object is a file.

	__________

	Also see:	FSSetFinderInfo
*/

/*****************************************************************************/

#pragma mark FSSetFinderInfo

OSErr
FSSetFinderInfo(
	const FSRef *ref,
	const FinderInfo *info,						/* can be NULL */
	const ExtendedFinderInfo *extendedInfo);	/* can be NULL */

/*
	The FSSetFinderInfo function sets the finder information for a file or
	directory.

	ref					--> FSRef to a file or directory.
	info				**> A pointer to a FinderInfo record with the new
							FileInfo (if ref is a file) or new FolderInfo
							(if ref is a folder), or NULL if the FinderInfo
							is not to be changed.
	extendedInfo		**> A pointer to a FinderInfo record with the new
							ExtendedFileInfo (if ref is a file) or new
							ExtendedFolderInfo (if ref is a folder), or NULL
							if the ExtendedFinderInfo is not to be changed.
		
	__________

	Also see:	FSGetFinderInfo
*/

/*****************************************************************************/

#pragma mark FSChangeCreatorType

OSErr
FSChangeCreatorType(
	const FSRef *ref,
	OSType fileCreator,
	OSType fileType);

/*
	The FSChangeCreatorType function changes the creator and/or file type of a file.

	ref					--> FSRef to a file.
	creator				--> The new creator type or 0x00000000 to leave
							the creator type alone.
	fileType			--> The new file type or 0x00000000 to leave the
							file type alone.
*/

/*****************************************************************************/

#pragma mark FSChangeFinderFlags

OSErr
FSChangeFinderFlags(
	const FSRef *ref,
	Boolean setBits,
	UInt16 flagBits);

/*
	The FSChangeFinderFlags function sets or clears flag bits in
	the finderFlags field of a file's FileInfo record or a
	directory's FolderInfo record.

	ref					--> FSRef to a file or directory.
	setBits				--> If true, then set the bits specified in flagBits.
							If false, then clear the bits specified in flagBits.
	flagBits			--> The flagBits parameter specifies which Finder Flag
							bits to set or clear. If a bit in flagBits is set,
							then the same bit in fdFlags is either set or
							cleared depending on the state of the setBits
							parameter.
*/

/*****************************************************************************/

#pragma mark FSSetInvisible

OSErr
FSSetInvisible(
	const FSRef *ref);

#pragma mark FSClearInvisible

OSErr
FSClearInvisible(
	const FSRef *ref);

/*
	The FSSetInvisible and FSClearInvisible functions set or clear the
	kIsInvisible bit in the finderFlags field of the specified file or
	directory's finder information.

	ref					--> FSRef to a file or directory.
*/

/*****************************************************************************/

#pragma mark FSSetNameLocked

OSErr
FSSetNameLocked(
	const FSRef *ref);

#pragma mark FSClearNameLocked

OSErr
FSClearNameLocked(
	const FSRef *ref);

/*
	The FSSetNameLocked and FSClearNameLocked functions set or clear the
	kNameLocked bit bit in the finderFlags field of the specified file or
	directory's finder information.

	ref					--> FSRef to a file or directory.
*/

/*****************************************************************************/

#pragma mark FSSetIsStationery

OSErr
FSSetIsStationery(
	const FSRef *ref);

#pragma mark FSClearIsStationery

OSErr
FSClearIsStationery(
	const FSRef *ref);

/*
	The FSSetIsStationery and FSClearIsStationery functions set or clear the
	kIsStationery bit bit in the finderFlags field of the specified file or
	directory's finder information.

	ref					--> FSRef to a file or directory.
*/

/*****************************************************************************/

#pragma mark FSSetHasCustomIcon

OSErr
FSSetHasCustomIcon(
	const FSRef *ref);

#pragma mark FSClearHasCustomIcon

OSErr
FSClearHasCustomIcon(
	const FSRef *ref);

/*
	The FSSetHasCustomIcon and FSClearHasCustomIcon functions set or clear the
	kHasCustomIcon bit bit in the finderFlags field of the specified file or
	directory's finder information.

	ref					--> FSRef to a file or directory.
*/

/*****************************************************************************/

#pragma mark FSClearHasBeenInited

OSErr
FSClearHasBeenInited(
	const FSRef *ref);

/*
	The FSClearHasBeenInited function clears the kHasBeenInited bit in the
	finderFlags field of the specified file or directory's finder information.
	
	Note:	There is no FSSetHasBeenInited function because ONLY the Finder
			should set the kHasBeenInited bit.

	ref					--> FSRef to a file or directory.
*/

/*****************************************************************************/

#pragma mark FSCopyFileMgrAttributes

OSErr
FSCopyFileMgrAttributes(
	const FSRef *sourceRef,
	const FSRef *destinationRef,
	Boolean copyLockBit);

/*
	The CopyFileMgrAttributes function copies all File Manager attributes
	from the source file or directory to the destination file or directory.
	If copyLockBit is true, then set the locked state of the destination
	to match the source.

	sourceRef			--> FSRef to a file or directory.
	destinationRef		--> FSRef to a file or directory.
	copyLockBit			--> If true, set the locked state of the destination
							to match the source.
*/

/*****************************************************************************/

#pragma mark FSMoveRenameObjectUnicode

OSErr
FSMoveRenameObjectUnicode(
	const FSRef *ref,
	const FSRef *destDirectory,
	UniCharCount nameLength,
	const UniChar *name,			/* can be NULL (no rename during move) */
	TextEncoding textEncodingHint,
	FSRef *newRef);					/* if function fails along the way, newRef is final location of file */

/*
	The FSMoveRenameObjectUnicode function moves a file or directory and
	optionally renames it.	The source and destination locations must be on
	the same volume.
	
	Note:	If the input ref parameter is invalid, this call will fail and
			newRef, like ref, will be invalid.

	ref					--> FSRef to a file or directory.
	destDirectory		--> FSRef to the destination directory.
	nameLength			--> Number of UniChar in name parameter.
	name				--> An Unicode string with the new name for the
							moved object, or NULL if no rename is wanted.
	textEncodingHint	--> The text encoding hint used for the rename.
							You can pass kTextEncodingUnknown to use the
							"default" textEncodingHint.
	newRef				<-- The new FSRef of the object moved. Note that if
							this function fails at any step along the way,
							newRef is still then final location of the object.
*/

/*****************************************************************************/

#pragma mark FSDeleteContainerContents

OSErr
FSDeleteContainerContents(
	const FSRef *container);

/*
	The FSDeleteContainerContents function deletes the contents of a container
	directory. All files and subdirectories in the specified container are
	deleted. If a locked file or directory is encountered, it is unlocked and
	then deleted. If any unexpected errors are encountered,
	FSDeleteContainerContents quits and returns to the caller.
	
	container			--> FSRef to a directory.
	
	__________
	
	Also see:	FSDeleteContainer
*/

/*****************************************************************************/

#pragma mark FSDeleteContainer

OSErr
FSDeleteContainer(
	const FSRef *container);

/*
	The FSDeleteContainer function deletes a container directory and its contents.
	All files and subdirectories in the specified container are deleted.
	If a locked file or directory is encountered, it is unlocked and then
	deleted.  After deleting the container's contents, the container is
	deleted. If any unexpected errors are encountered, FSDeleteContainer
	quits and returns to the caller.
	
	container			--> FSRef to a directory.
	
	__________
	
	Also see:	FSDeleteContainerContents
*/

/*****************************************************************************/

#pragma mark IterateContainerFilterProcPtr

typedef CALLBACK_API( Boolean , IterateContainerFilterProcPtr ) (
	Boolean containerChanged,
	ItemCount currentLevel,
	const FSCatalogInfo *catalogInfo,
	const FSRef *ref,
	const FSSpec *spec,
	const HFSUniStr255 *name,
	void *yourDataPtr);

/*
	This is the prototype for the IterateContainerFilterProc function which
	is called once for each file and directory found by FSIterateContainer.
	The IterateContainerFilterProc can use the read-only data it receives for
	whatever it wants.

	The result of the IterateContainerFilterProc function indicates if
	iteration should be stopped. To stop iteration, return true; to continue
	iteration, return false.

	The yourDataPtr parameter can point to whatever data structure you might
	want to access from within the IterateContainerFilterProc.

	containerChanged	--> Set to true if the container's contents changed
							during iteration.
	currentLevel		--> The current recursion level into the container.
							1 = the container, 2 = the container's immediate
							subdirectories, etc.
	catalogInfo			--> The catalog information for the current object.
							Only the fields requested by the whichInfo
							parameter passed to FSIterateContainer are valid.
	ref					--> The FSRef to the current object.
	spec				--> The FSSpec to the current object if the wantFSSpec
							parameter passed to FSIterateContainer is true.
	name				--> The name of the current object if the wantName
							parameter passed to FSIterateContainer is true.
	yourDataPtr			--> An optional pointer to whatever data structure you
							might want to access from within the
							IterateFilterProc.
	result				<-- To stop iteration, return true; to continue
							iteration, return false.

	__________

	Also see:	FSIterateContainer
*/

/*****************************************************************************/

#pragma mark CallIterateContainerFilterProc

#define CallIterateContainerFilterProc(userRoutine, containerChanged, currentLevel, catalogInfo, ref, spec, name, yourDataPtr) \
	(*(userRoutine))((containerChanged), (currentLevel), (catalogInfo), (ref), (spec), (name), (yourDataPtr))

/*****************************************************************************/

#pragma mark FSIterateContainer

OSErr
FSIterateContainer(
	const FSRef *container,
	ItemCount maxLevels,
	FSCatalogInfoBitmap whichInfo,
	Boolean wantFSSpec,
	Boolean wantName,
	IterateContainerFilterProcPtr iterateFilter,
	void *yourDataPtr);

/*
	The FSIterateContainer function performs a recursive iteration (scan) of the
	specified container directory and calls your IterateContainerFilterProc
	function once for each file and directory found.

	The maxLevels parameter lets you control how deep the recursion goes.
	If maxLevels is 1, FSIterateContainer only scans the specified directory;
	if maxLevels is 2, FSIterateContainer scans the specified directory and
	one subdirectory below the specified directory; etc. Set maxLevels to
	zero to scan all levels.

	The yourDataPtr parameter can point to whatever data structure you might
	want to access from within your IterateContainerFilterProc.

	container			--> The FSRef to the container directory to iterate.
	maxLevels			--> Maximum number of directory levels to scan or
							zero to scan all directory levels.
	whichInfo			--> The fields of the FSCatalogInfo you wish to get.
	wantFSSpec			--> Set to true if you want the FSSpec to each
							object passed to your IterateContainerFilterProc.
	wantName			--> Set to true if you want the name of each
							object passed to your IterateContainerFilterProc.
	iterateFilter		--> A pointer to the IterateContainerFilterProc you
							want called once for each file and directory found
							by FSIterateContainer.
	yourDataPtr			--> An optional pointer to whatever data structure you
							might want to access from within the
							IterateFilterProc.
*/

/*****************************************************************************/

#pragma mark FSGetDirectoryItems

OSErr
FSGetDirectoryItems(
	const FSRef *container,
	FSRef ***refsHandle,	/* pointer to handle of FSRefs */
	ItemCount *numRefs,
	Boolean *containerChanged);

/*
	The FSGetDirectoryItems function returns the list of items in the specified
	container. The array of FSRef records is returned in a Handle, refsHandle,
	which is allocated by FSGetDirectoryItems. The caller is responsible for
	disposing of refsHandle if the FSGetDirectoryItems returns noErr.
		
	container			--> FSRef to a directory.
	refsHandle			<-- Pointer to an FSRef Handle where the array of
							FSRefs is to be returned.
	numRefs				<-- The number of FSRefs returned in the array.
	containerChanged	<-- Set to true if the container changes while the
							list of items is being obtained.
*/

/*****************************************************************************/

#pragma mark FSExchangeObjectsCompat

OSErr
FSExchangeObjectsCompat(
	const FSRef *sourceRef,
	const FSRef *destRef,
	FSRef *newSourceRef,
	FSRef *newDestRef);

/*
	The FSExchangeObjectsCompat function exchanges the data between two files.
	
	The FSExchangeObjectsCompat function is an enhanced version of
	FSExchangeObjects function. The two enhancements FSExchangeObjectsCompat
	provides are:
	
	1,	FSExchangeObjectsCompat will work on volumes which do not support
		FSExchangeObjects. FSExchangeObjectsCompat does this by emulating
		FSExchangeObjects through a series of File Manager operations. If
		there is a failure at any step along the way, FSExchangeObjectsCompat
		attempts to undo any steps already taken to leave the files in their
		original state in their original locations.
		
	2.	FSExchangeObjectsCompat returns new FSRefs to the source and
		destination files. Note that if this function fails at any step along
		the way, newSourceRef and newDestRef still give you access to the final
		locations of the files being exchanged -- even if they are renamed or
		not in their original locations.

	sourceRef			--> FSRef to the source file.
	destRef				--> FSRef to the destination file.
	newSourceRef		<-- The new FSRef to the source file.
	newDestRef			<-- The new FSRef to the destination file.
*/

/*****************************************************************************/

#pragma mark ----- Shared Environment Routines -----

/*****************************************************************************/

#pragma mark FSLockRange

OSErr
FSLockRange(
	SInt16 refNum,
	SInt32 rangeLength,
	SInt32 rangeStart);

/*
	The LockRange function locks (denies access to) a portion of a file
	that was opened with shared read/write permission.

	refNum				--> The file reference number of an open file.
	rangeLength			--> The number of bytes in the range.
	rangeStart			--> The starting byte in the range to lock.

	__________

	Also see:	UnlockRange
*/

/*****************************************************************************/

#pragma mark FSUnlockRange

OSErr
FSUnlockRange(
	SInt16 refNum,
	SInt32 rangeLength,
	SInt32 rangeStart);

/*
	The UnlockRange function unlocks (allows access to) a previously locked
	portion of a file that was opened with shared read/write permission.

	refNum				--> The file reference number of an open file.
	rangeLength			--> The number of bytes in the range.
	rangeStart			--> The starting byte in the range to unlock.

	__________

	Also see:	LockRange
*/

/*****************************************************************************/

#pragma mark FSGetDirAccess

OSErr
FSGetDirAccess(
	const FSRef *ref,
	SInt32 *ownerID,		/* can be NULL */
	SInt32 *groupID,		/* can be NULL */
	SInt32 *accessRights);	/* can be NULL */

/*
	The FSGetDirAccess function retrieves the directory access control
	information for a directory on a shared volume.

	ref					--> An FSRef specifying the directory.
	ownerID				<** An optional pointer to a SInt32.
							If not NULL, the directory's owner ID
							will be returned in the SInt32.
	groupID				<** An optional pointer to a SInt32.
							If not NULL, the directory's group ID, or 0
							if no group affiliation, will be returned in
							the SInt32.
	accessRights		<** An optional pointer to a SInt32.
							If not NULL, the directory's access rights
							will be returned in the SInt32.

	__________

	Also see:	FSSetDirAccess, FSMapID, FSMapName
*/

/*****************************************************************************/

#pragma mark FSSetDirAccess

OSErr
FSSetDirAccess(
	const FSRef *ref,
	SInt32 ownerID,
	SInt32 groupID,
	SInt32 accessRights);

/*
	The FSpSetDirAccess function changes the directory access control
	information for a directory on a shared volume. You must be the owner of
	a directory to change its access control information.
	
	ref					--> An FSRef specifying the directory.
	ownerID				--> The directory's owner ID.
	groupID				--> The directory's group ID or 0 if no group affiliation.
	accessRights		--> The directory's access rights.
	
	__________
	
	Also see:	FSGetDirAccess, FSMapID, FSMapName
*/

/*****************************************************************************/

#pragma mark FSGetVolMountInfoSize

OSErr
FSGetVolMountInfoSize(
	FSVolumeRefNum volRefNum,
	SInt16 *size);

/*
	The FSGetVolMountInfoSize function determines the how much space the
	program needs to allocate for a volume mounting information record.

	volRefNum			--> Volume specification.
	size				<-- The space needed (in bytes) of the volume
							mounting information record.

	__________

	Also see:	FSGetVolMountInfo, VolumeMount
*/

/*****************************************************************************/

#pragma mark FSGetVolMountInfo

OSErr
FSGetVolMountInfo(
	FSVolumeRefNum volRefNum,
	void *volMountInfo);

/*
	The FSGetVolMountInfo function retrieves a volume mounting information
	record containing all the information needed to mount the volume,
	except for passwords.

	volRefNum			--> Volume specification.
	volMountInfo		<-- The volume mounting information.

	__________

	Also see:	FSGetVolMountInfoSize, VolumeMount
*/

/*****************************************************************************/

#pragma mark FSVolumeMount

OSErr
FSVolumeMount(
	const void *volMountInfo,
	FSVolumeRefNum *volRefNum);

/*
	The VolumeMount function mounts a volume using a volume mounting
	information record.

	volMountInfo		--> A volume mounting information record.
	volRefNum			<-- The volume reference number.

	__________

	Also see:	FSGetVolMountInfoSize, FSGetVolMountInfo
*/

/*****************************************************************************/

#pragma mark FSMapID

OSErr
FSMapID(
	FSVolumeRefNum volRefNum,
	SInt32 ugID,
	SInt16 objType,
	Str31 name);

/*
	The FSMapID function determines the name of a user or group if you know
	the user or group ID.

	volRefNum			--> Volume specification.
	objType				--> The mapping function code:
							kOwnerID2Name to map a user ID to a user name
							kGroupID2Name to map a group ID to a group name
	name				<** An optional pointer to a buffer (minimum Str31).
							If not NULL, the user or group name
							will be returned in the buffer.

	__________

	Also see:	FSGetDirAccess, FSSetDirAccess, FSMapName
*/

/*****************************************************************************/

#pragma mark FSMapName

OSErr
FSMapName(
	FSVolumeRefNum volRefNum,
	ConstStr255Param name,
	SInt16 objType,
	SInt32 *ugID);

/*
	The FSMapName function determines the user or group ID if you know the
	user or group name.
	
	volRefNum			--> Volume specification.
	name				--> The user or group name.
	objType				--> The mapping function code:
							kOwnerName2ID to map a user name to a user ID
							kGroupName2ID to map a user name to a group ID
	ugID				<-- The user or group ID.

	__________
	
	Also see:	FSGetDirAccess, FSSetDirAccess, FSMapID
*/

/*****************************************************************************/

#pragma mark FSCopyFile

OSErr
FSCopyFile(
	const FSRef *srcFileRef,
	const FSRef *dstDirectoryRef,
	UniCharCount nameLength,
	const UniChar *copyName,	/* can be NULL (no rename during copy) */
	TextEncoding textEncodingHint,
	FSRef *newRef);				/* can be NULL */

/*
	The FSCopyFile function duplicates a file and optionally renames it.
	The source and destination volumes must be on the same file server.
	This function instructs the server to copy the file.
	
	srcFileRef			--> An FSRef specifying the source file.
	dstDirectoryRef		--> An FSRef specifying the destination directory.
	nameLength			--> Number of UniChar in copyName parameter (ignored
							if copyName is NULL).
	copyName			--> Points to the new file name if the file is to be
							renamed, or NULL if the file isn't to be renamed.
	textEncodingHint	--> The text encoding hint used for the rename.
							You can pass kTextEncodingUnknown to use the
							"default" textEncodingHint.
	newRef				<** An optional pointer to a FSRef.
							If not NULL, the FSRef of the duplicated file
							will be returned in the FSRef.
*/

/*****************************************************************************/

#pragma mark FSMoveRename

OSErr
FSMoveRename(
	const FSRef *srcFileRef,
	const FSRef *dstDirectoryRef,
	UniCharCount nameLength,
	const UniChar *moveName,	/* can be NULL (no rename during move) */
	TextEncoding textEncodingHint,
	FSRef *newRef);				/* can be NULL */

/*
	The FSMoveRename function moves a file or directory (object), and
	optionally renames it. The source and destination locations must be on
	the same shared volume.
	
	srcFileRef			--> An FSRef specifying the source file.
	dstDirectoryRef		--> An FSRef specifying the destination directory.
	nameLength			--> Number of UniChar in moveName parameter (ignored
							if copyName is NULL)
	moveName			--> Points to the new object name if the object is to be
							renamed, or NULL if the object isn't to be renamed.
	textEncodingHint	--> The text encoding hint used for the rename.
							You can pass kTextEncodingUnknown to use the
							"default" textEncodingHint.
	newRef				<** An optional pointer to a FSRef.
							If not NULL, the FSRef of the moved object
							will be returned in the FSRef.
*/

/*****************************************************************************/

#pragma mark ----- File ID Routines -----

/*****************************************************************************/

#pragma mark FSResolveFileIDRef

OSErr
FSResolveFileIDRef(
	FSVolumeRefNum volRefNum,
	SInt32 fileID,
	FSRef *ref);

/*
	The FSResolveFileIDRef function returns an FSRef for the file with the
	specified file ID reference.

	volRefNum			--> Volume specification.
	fileID				--> The file ID reference.
	ref					<-- The FSRef for the file ID reference.

	__________

	Also see:	FSCreateFileIDRef, FSDeleteFileIDRef
*/

/*****************************************************************************/

#pragma mark FSCreateFileIDRef

OSErr
FSCreateFileIDRef(
	const FSRef *ref,
	SInt32 *fileID);

/*
	The FSCreateFileIDRef function creates a file ID reference for the
	specified file, or if a file ID reference already exists, supplies
	the file ID reference and returns the result code fidExists or afpIDExists.

	ref					--> The FSRef for the file.
	fileID				<-- The file ID reference (if result is noErr,
							fidExists, or afpIDExists).

	__________

	Also see:	GetFSRefFromFileIDRef, FSDeleteFileIDRef
*/

/*****************************************************************************/

#pragma mark FSDeleteFileIDRef

/*
	Why is there no FSDeleteFileIDRef routine? There are two reasons:
	
	1.	Since Mac OS 8.1, PBDeleteFileIDRef hasn't deleted file ID references.
		On HFS volumes, deleting a file ID reference breaks aliases (which
		use file ID references to track files as they are moved around on a
		volume) and file ID references are automatically deleted when the file
		they refer to is deleted. On HFS Plus volumes, file ID references are
		always created when a file is created, deleted when the file is deleted,
		and cannot be deleted at any other time.
		
	2.	PBDeleteFileIDRef causes a memory access fault under Mac OS X 10.0
		through 10.1.x. While this will be fixed in a future release, the
		implementation, like the Mac OS 8/9 implementation, does not delete
		file ID references.
		
	__________

	Also see:	GetFSRefFromFileIDRef, FSCreateFileIDRef
*/

/*****************************************************************************/

#pragma mark ----- Utility Routines -----

/*****************************************************************************/

#pragma mark GetTempBuffer

Ptr
GetTempBuffer(
	ByteCount buffReqSize,
	ByteCount *buffActSize);

/*
	The GetTempBuffer function allocates a temporary buffer for file system
	operations which is at least 4K bytes and a multiple of 4K bytes.
	
	buffReqSize			--> Size you'd like the buffer to be.
	buffActSize			<-- The size of the buffer allocated.
	function result		<-- Pointer to memory allocated, or NULL if no memory
							was available. The caller is responsible for
							disposing of this buffer with DisposePtr.
*/

/*****************************************************************************/

#pragma mark FileRefNumGetFSRef

OSErr
FileRefNumGetFSRef(
	short refNum,
	FSRef *ref);

/*
	The FileRefNumGetFSRef function gets the FSRef of an open file.

	refNum				--> The file reference number of an open file.
	ref					<-- The FSRef to the open file.
*/

/*****************************************************************************/

#pragma mark FSSetDefault

OSErr
FSSetDefault(
	const FSRef *newDefault,
	FSRef *oldDefault);

/*
	The FSSetDefault function sets the current working directory to the
	directory specified by newDefault. The previous current working directory
	is returned in oldDefault and must be used to restore the current working
	directory to its previous state with the FSRestoreDefault function.
	These two functions are designed to be used as a wrapper around
	Standard I/O routines where the location of the file is implied to be the
	current working directory. This is how you should use these functions:
	
		result = FSSetDefault(&newDefault, &oldDefault);
		if ( noErr == result )
		{
			// call the Stdio functions like remove, rename,
			// fopen, freopen, etc here!

			result = FSRestoreDefault(&oldDefault);
		}
	
	newDefault			--> An FSRef that specifies the new current working
							directory.
	oldDefault			<-- The previous current working directory's FSRef.
	
	__________
	
	Also see:	FSRestoreDefault
*/

/*****************************************************************************/

#pragma mark FSRestoreDefault

OSErr
FSRestoreDefault(
	const FSRef *oldDefault);

/*
	The FSRestoreDefault function restores the current working directory
	to the directory specified by oldDefault. The oldDefault parameter was
	previously obtained from the FSSetDefault function.
	These two functions are designed to be used as a wrapper around
	Standard I/O routines where the location of the file is implied to be the
	current working directory. This is how you should use these functions:
	
		result = FSSetDefault(&newDefault, &oldDefault);
		if ( noErr == result )
		{
			// call the Stdio functions like remove, rename,
			// fopen, freopen, etc here!

			result = FSRestoreDefault(&oldDefault);
		}
		
	oldDefault			--> The FSRef of the location to restore.
	
	__________
	
	Also see:	FSSetDefault
*/

/*****************************************************************************/

#if PRAGMA_STRUCT_ALIGN
	#pragma options align=reset
#elif PRAGMA_STRUCT_PACKPUSH
	#pragma pack(pop)
#elif PRAGMA_STRUCT_PACK
	#pragma pack()
#endif

#ifdef PRAGMA_IMPORT_OFF
#pragma import off
#elif PRAGMA_IMPORT
#pragma import reset
#endif

#ifdef __cplusplus
}
#endif

#endif /* __MOREFILESX__ */

