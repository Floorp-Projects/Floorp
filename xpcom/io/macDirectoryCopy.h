/*
**	Apple Macintosh Developer Technical Support
**
**	DirectoryCopy: A robust, general purpose directory copy routine.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		DirectoryCopy.h
**
**	Copyright © 1992-1998 Apple Computer, Inc.
**	All rights reserved.
**
**	You may incorporate this sample code into your applications without
**	restriction, though the sample code has been provided "AS IS" and the
**	responsibility for its operation is 100% yours.  However, what you are
**	not permitted to do is to redistribute the source as "DSC Sample Code"
**	after having made changes. If you're going to re-distribute the source,
**	we require that you make it clear in the source that the code was
**	descended from Apple Sample Code, but that you've made changes.
*/

// Modified to allow renaming the destination folder

#ifndef __MACDIRECTORYCOPY__
#define __MACDIRECTORYCOPY__

#include <Types.h>
#include <Files.h>

#include "Optimization.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef	pascal	Boolean	(*CopyErrProcPtr) (OSErr error,
										   short failedOperation,
										   short srcVRefNum,
										   long srcDirID,
										   ConstStr255Param srcName,
										   short dstVRefNum,
										   long dstDirID,
										   ConstStr255Param dstName);
/*	¦ Prototype for the CopyErrProc function DirectoryCopy calls.
	This is the prototype for the CopyErrProc function DirectoryCopy
	calls if an error condition is detected sometime during the copy.  If
	CopyErrProc returns false, then DirectoryCopy attempts to continue with
	the directory copy operation.  If CopyErrProc returns true, then
	DirectoryCopy stops the directory copy operation.

	error			input:	The error result code that caused CopyErrProc to
							be called.
	failedOperation	input:	The operation that returned an error to
							DirectoryCopy.
	srcVRefNum		input:	Source volume specification.
	srcDirID		input:	Source directory ID.
	srcName			input:	Source file or directory name, or nil if
							srcDirID specifies the directory.
	dstVRefNum		input:	Destination volume specification.
	dstDirID		input:	Destination directory ID.
	dstName			input:	Destination file or directory name, or nil if
							dstDirID specifies the directory.

	__________
	
	Also see:	FilteredDirectoryCopy, FSpFilteredDirectoryCopy, DirectoryCopy, FSpDirectoryCopy
*/

pascal	OSErr	MacFSpDirectoryCopyRename(const FSSpec *srcSpec,
								 const FSSpec *dstSpec,
								 ConstStr255Param newName,
								 void *copyBufferPtr,
								 long copyBufferSize,
								 Boolean preflight,
								 CopyErrProcPtr copyErrHandler);
/*	¦ Make a copy of a directory structure in a new location.
	The FSpDirectoryCopy function makes a copy of a directory structure in a
	new location. If copyBufferPtr <> NIL, it points to a buffer of
	copyBufferSize that is used to copy files data.  The larger the
	supplied buffer, the faster the copy.  If copyBufferPtr = NIL, then this
	routine allocates a buffer in the application heap. If you pass a
	copy buffer to this routine, make its size a multiple of 512
	($200) bytes for optimum performance.
	
	srcSpec			input:	An FSSpec record specifying the directory to copy.
	dstSpec			input:	An FSSpec record specifying destination directory
							of the copy.
	copyBufferPtr	input:	Points to a buffer of copyBufferSize that
							is used the i/o buffer for the copy or
							nil if you want DirectoryCopy to allocate its
							own buffer in the application heap.
	copyBufferSize	input:	The size of the buffer pointed to
							by copyBufferPtr.
	preflight		input:	If true, FSpDirectoryCopy makes sure there are
							enough allocation blocks on the destination
							volume to hold the directory's files before
							starting the copy.
	copyErrHandler	input:	A pointer to the routine you want called if an
							error condition is detected during the copy, or
							nil if you don't want to handle error conditions.
							If you don't handle error conditions, the first
							error will cause the copy to quit and
							DirectoryCopy will return the error.
							Error handling is recommended...
	
	Result Codes
		noErr				0		No error
		readErr				Ð19		Driver does not respond to read requests
		writErr				Ð20		Driver does not respond to write requests
		badUnitErr			Ð21		Driver reference number does not
									match unit table
		unitEmptyErr		Ð22		Driver reference number specifies a
									nil handle in unit table
		abortErr			Ð27		Request aborted by KillIO
		notOpenErr			Ð28		Driver not open
		dskFulErr			-34		Destination volume is full
		nsvErr				-35		No such volume
		ioErr				-36		I/O error
		bdNamErr			-37		Bad filename
		tmfoErr				-42		Too many files open
		fnfErr				-43		Source file not found, or destination
									directory does not exist
		wPrErr				-44		Volume locked by hardware
		fLckdErr			-45		File is locked
		vLckdErr	 		-46		Destination volume is read-only
		fBsyErr	 			-47		The source or destination file could
									not be opened with the correct access
									modes
		dupFNErr			-48		Destination file already exists
		opWrErr				-49		File already open for writing
		paramErr			-50		No default volume or function not
									supported by volume
		permErr	 			-54		File is already open and cannot be opened using specified deny modes
		memFullErr			-108	Copy buffer could not be allocated
		dirNFErr			-120	Directory not found or incomplete pathname
		wrgVolTypErr		-123	Function not supported by volume
		afpAccessDenied		-5000	User does not have the correct access
		afpDenyConflict		-5006	The source or destination file could
									not be opened with the correct access
									modes
		afpObjectTypeErr	-5025	Source is a directory, directory not found
									or incomplete pathname
	
	__________
	
	Also see:	CopyErrProcPtr, DirectoryCopy, FilteredDirectoryCopy,
				FSpFilteredDirectoryCopy, FileCopy, FSpFileCopy
*/

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#include "OptimizationEnd.h"

#endif	/* __DIRECTORYCOPY__ */
