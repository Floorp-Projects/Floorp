/*
**	Apple Macintosh Developer Technical Support
**
**	DirectoryCopy: A robust, general purpose directory copy routine.
**
**	by Jim Luther, Apple Developer Technical Support Emeritus
**
**	File:		DirectoryCopy.c
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

#include <Types.h>
#include <Errors.h>
#include <Memory.h>
#include <Files.h>
#include <Script.h>

#define	__COMPILINGMOREFILES

#include "MoreFiles.h"
#include "MoreFilesExtras.h"
#include "MoreDesktopMgr.h"
#include "FileCopy.h"
#include "MacDirectoryCopy.h"
#include <string.h>


/*****************************************************************************/

enum
{
	getNextItemOp			= 1,	/* couldn't access items in this directory - no access privileges */
	copyDirCommentOp		= 2,	/* couldn't copy directory's Finder comment */
	copyDirAccessPrivsOp	= 3,	/* couldn't copy directory's AFP access privileges */
	copyDirFMAttributesOp	= 4,	/* couldn't copy directory's File Manager attributes */
	dirCreateOp				= 5,	/* couldn't create destination directory */
	fileCopyOp				= 6		/* couldn't copy file */
};

/*****************************************************************************/



#define CallCopyErrProc(userRoutine, error, failedOperation, srcVRefNum, srcDirID, srcName, dstVRefNum, dstDirID, dstName) \
		(*(userRoutine))((error), (failedOperation), (srcVRefNum), (srcDirID), (srcName), (dstVRefNum), (dstDirID), (dstName))

/*****************************************************************************/

typedef	pascal	Boolean	(*CopyFilterProcPtr) (const CInfoPBRec * const cpbPtr);

/*	¦ Prototype for the CopyFilterProc function.
	This is the prototype for the CopyFilterProc function called by
	FilteredDirectoryCopy and GetLevelSize. If true is returned,
	the file/folder is included in the copy, otherwise it is excluded.
	
	pb	input:	Points to the CInfoPBRec for the item under consideration.

	__________
	
	Also see:	FilteredDirectoryCopy, FSpFilteredDirectoryCopy
*/

#define CallCopyFilterProc(userRoutine, cpbPtr) (*(userRoutine))((cpbPtr))



/*****************************************************************************/

/* local constants */

enum
{
	dirCopyBigCopyBuffSize  = 0x00004000,
	dirCopyMinCopyBuffSize  = 0x00000200
};


/*****************************************************************************/

/* local data structures */

/* The EnumerateGlobals structure is used to minimize the amount of
** stack space used when recursively calling CopyLevel and to hold
** global information that might be needed at any time. */

#if PRAGMA_STRUCT_ALIGN
#pragma options align=mac68k
#endif
struct EnumerateGlobals
{
	Ptr			copyBuffer;			/* pointer to buffer used for file copy operations */
	long		bufferSize;			/* the size of the copy buffer */
	CopyErrProcPtr errorHandler;	/* pointer to error handling function */
	CopyFilterProcPtr copyFilterProc; /* pointer to filter function */
	OSErr		error;				/* temporary holder of results - saves 2 bytes of stack each level */
	Boolean		bailout;			/* set to true to by error handling function if fatal error */
	short		destinationVRefNum;	/* the destination vRefNum */
	Str63		itemName;			/* the name of the current item */
	CInfoPBRec	myCPB;				/* the parameter block used for PBGetCatInfo calls */
};
#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

typedef struct EnumerateGlobals EnumerateGlobals;
typedef EnumerateGlobals *EnumerateGlobalsPtr;


/* The PreflightGlobals structure is used to minimize the amount of
** stack space used when recursively calling GetLevelSize and to hold
** global information that might be needed at any time. */

#if PRAGMA_STRUCT_ALIGN
#pragma options align=mac68k
#endif
struct PreflightGlobals
{
	OSErr			result;				/* temporary holder of results - saves 2 bytes of stack each level */
	Str63			itemName;			/* the name of the current item */
	CInfoPBRec		myCPB;				/* the parameter block used for PBGetCatInfo calls */

	unsigned long	dstBlksPerAllocBlk;	/* the number of 512 byte blocks per allocation block on destination */
										
	unsigned long	allocBlksNeeded;	/* the total number of allocation blocks needed  */

	unsigned long	tempBlocks;			/* temporary storage for calculations (save some stack space)  */
	CopyFilterProcPtr copyFilterProc;	/* pointer to filter function */
};
#if PRAGMA_STRUCT_ALIGN
#pragma options align=reset
#endif

typedef struct PreflightGlobals PreflightGlobals;
typedef PreflightGlobals *PreflightGlobalsPtr;

/*****************************************************************************/

/* static prototypes */

static	void	GetLevelSize(long currentDirID,
							 PreflightGlobals *theGlobals);

static	OSErr	PreflightDirectoryCopySpace(short srcVRefNum,
											long srcDirID,
											short dstVRefNum,
											CopyFilterProcPtr copyFilterProc,
											Boolean *spaceOK);

static	void	CopyLevel(long sourceDirID,
						  long dstDirID,
						  EnumerateGlobals *theGlobals);
						  
/*****************************************************************************/

static	void	GetLevelSize(long currentDirID,
							 PreflightGlobals *theGlobals)
{
	short	index = 1;
	
	do
	{
		theGlobals->myCPB.dirInfo.ioFDirIndex = index;
		theGlobals->myCPB.dirInfo.ioDrDirID = currentDirID;	/* we need to do this every time */
															/* through, since GetCatInfo  */
															/* returns ioFlNum in this field */
		theGlobals->result = PBGetCatInfoSync(&theGlobals->myCPB);
		if ( theGlobals->result == noErr )
		{
			if ( (theGlobals->copyFilterProc == NULL) ||
				 CallCopyFilterProc(theGlobals->copyFilterProc, &theGlobals->myCPB) ) /* filter if filter proc was supplied */
			{
				/* Either there's no filter proc OR the filter proc says to use this item */
				if ( (theGlobals->myCPB.dirInfo.ioFlAttrib & ioDirMask) != 0 )
				{
					/* we have a directory */
					
					GetLevelSize(theGlobals->myCPB.dirInfo.ioDrDirID, theGlobals); /* recurse */
					theGlobals->result = noErr; /* clear error return on way back */
				}
				else
				{
					/* We have a file - add its allocation blocks to allocBlksNeeded. */
					/* Since space on Mac OS disks is always allocated in allocation blocks, */
					/* this takes into account rounding up to the end of an allocation block. */
					
					/* get number of 512-byte blocks needed for data fork */
					if ( ((unsigned long)theGlobals->myCPB.hFileInfo.ioFlLgLen & 0x000001ff) != 0 )
					{
						theGlobals->tempBlocks = ((unsigned long)theGlobals->myCPB.hFileInfo.ioFlLgLen >> 9) + 1;
					}
					else
					{
						theGlobals->tempBlocks = (unsigned long)theGlobals->myCPB.hFileInfo.ioFlLgLen >> 9;
					}
					/* now, calculate number of new allocation blocks needed for the data fork and add it to the total */
					if ( theGlobals->tempBlocks % theGlobals->dstBlksPerAllocBlk )
					{
						theGlobals->allocBlksNeeded += (theGlobals->tempBlocks / theGlobals->dstBlksPerAllocBlk) + 1;
					}
					else
					{
						theGlobals->allocBlksNeeded += theGlobals->tempBlocks / theGlobals->dstBlksPerAllocBlk;
					}
					
					/* get number of 512-byte blocks needed for resource fork */
					if ( ((unsigned long)theGlobals->myCPB.hFileInfo.ioFlRLgLen & 0x000001ff) != 0 )
					{
						theGlobals->tempBlocks = ((unsigned long)theGlobals->myCPB.hFileInfo.ioFlRLgLen >> 9) + 1;
					}
					else
					{
						theGlobals->tempBlocks = (unsigned long)theGlobals->myCPB.hFileInfo.ioFlRLgLen >> 9;
					}
					/* now, calculate number of new allocation blocks needed for the resource  fork and add it to the total */
					if ( theGlobals->tempBlocks % theGlobals->dstBlksPerAllocBlk )
					{
						theGlobals->allocBlksNeeded += (theGlobals->tempBlocks / theGlobals->dstBlksPerAllocBlk) + 1;
					}
					else
					{
						theGlobals->allocBlksNeeded += theGlobals->tempBlocks / theGlobals->dstBlksPerAllocBlk;
					}
				}
			}
		}
		++index;
	} while ( theGlobals->result == noErr );
}

/*****************************************************************************/

static	OSErr	PreflightDirectoryCopySpace(short srcVRefNum,
											long srcDirID,
											short dstVRefNum,
											CopyFilterProcPtr copyFilterProc,
											Boolean *spaceOK)
{
	XVolumeParam pb;
	OSErr error;
	unsigned long dstFreeBlocks;
	PreflightGlobals theGlobals;
	
	error = XGetVolumeInfoNoName(NULL, dstVRefNum, &pb);
	if ( error == noErr )
	{
		/* Convert freeBytes to free disk blocks (512-byte blocks) */
#if (UNIVERSAL_INTERFACES_VERSION >= 0x0330)
		dstFreeBlocks = (pb.ioVFreeBytes >> 9);
#else
		dstFreeBlocks = (pb.ioVFreeBytes.hi << 23) + (pb.ioVFreeBytes.lo >> 9);
#endif		
		/* get allocation block size (always multiple of 512) and divide by 512
		  to get number of 512-byte blocks per allocation block */
		theGlobals.dstBlksPerAllocBlk = ((unsigned long)pb.ioVAlBlkSiz >> 9);
		
		theGlobals.allocBlksNeeded = 0;

		theGlobals.myCPB.dirInfo.ioNamePtr = theGlobals.itemName;
		theGlobals.myCPB.dirInfo.ioVRefNum = srcVRefNum;
		
		theGlobals.copyFilterProc = copyFilterProc;
		
		GetLevelSize(srcDirID, &theGlobals);
		
		/* Is there enough room on the destination volume for the source file?					*/
		/* Note:	This will work because the largest number of disk blocks supported			*/
		/*			on a 2TB volume is 0xffffffff and (allocBlksNeeded * dstBlksPerAllocBlk)	*/
		/*			will always be less than 0xffffffff.										*/
		*spaceOK = ((theGlobals.allocBlksNeeded * theGlobals.dstBlksPerAllocBlk) <= dstFreeBlocks);
	}

	return ( error );
}

/*****************************************************************************/

static	void	CopyLevel(long sourceDirID,
						  long dstDirID,
						  EnumerateGlobals *theGlobals)
{
	long currentSrcDirID;
	long newDirID;
	short index = 1;
	
	do
	{	
		/* Get next source item at the current directory level */
		
		theGlobals->myCPB.dirInfo.ioFDirIndex = index;
		theGlobals->myCPB.dirInfo.ioDrDirID = sourceDirID;
		theGlobals->error = PBGetCatInfoSync(&theGlobals->myCPB);		

		if ( theGlobals->error == noErr )
		{
			if ( (theGlobals->copyFilterProc == NULL) ||
				 CallCopyFilterProc(theGlobals->copyFilterProc, &theGlobals->myCPB) ) /* filter if filter proc was supplied */
			{
				/* Either there's no filter proc OR the filter proc says to use this item */

				/* We have an item.  Is it a file or directory? */
				if ( (theGlobals->myCPB.hFileInfo.ioFlAttrib & ioDirMask) != 0 )
				{
					/* We have a directory */
					
					/* Create a new directory at the destination. No errors allowed! */
					theGlobals->error = DirCreate(theGlobals->destinationVRefNum, dstDirID, theGlobals->itemName, &newDirID);
					if ( theGlobals->error == noErr )
					{
						/* Save the current source directory ID where we can get it when we come back
						** from recursion land. */
						currentSrcDirID = theGlobals->myCPB.dirInfo.ioDrDirID;
						
						/* Dive again (copy the directory level we just found below this one) */
						CopyLevel(theGlobals->myCPB.dirInfo.ioDrDirID, newDirID, theGlobals);
						
						if ( !theGlobals->bailout )
						{
							/* Copy comment from old to new directory. */
							/* Ignore the result because we really don't care if it worked or not. */
							(void) DTCopyComment(theGlobals->myCPB.dirInfo.ioVRefNum, currentSrcDirID, NULL, theGlobals->destinationVRefNum, newDirID, NULL);
							
							/* Copy directory attributes (dates, etc.) to newDirID. */
							/* No errors allowed */
							theGlobals->error = CopyFileMgrAttributes(theGlobals->myCPB.dirInfo.ioVRefNum, currentSrcDirID, NULL, theGlobals->destinationVRefNum, newDirID, NULL, true);
							
							/* handle any errors from CopyFileMgrAttributes */
							if ( theGlobals->error != noErr )
							{
								if ( theGlobals->errorHandler != NULL )
								{
									theGlobals->bailout =  CallCopyErrProc(theGlobals->errorHandler, theGlobals->error, copyDirFMAttributesOp,
															theGlobals->myCPB.dirInfo.ioVRefNum, currentSrcDirID, NULL,
															theGlobals->destinationVRefNum, newDirID, NULL);
								}
								else
								{
									/* If you don't handle the errors with an error handler, */
									/* then the copy stops here. */
									theGlobals->bailout = true;
								}
							}
						}
					}
					else	/* error handling for DirCreate */
					{
						if ( theGlobals->errorHandler != NULL )
						{
							theGlobals->bailout = CallCopyErrProc(theGlobals->errorHandler, theGlobals->error, dirCreateOp,
														theGlobals->myCPB.dirInfo.ioVRefNum, currentSrcDirID, NULL,
														theGlobals->destinationVRefNum, dstDirID, theGlobals->itemName);
						}
						else
						{
							/* If you don't handle the errors with an error handler, */
							/* then the copy stops here. */
							theGlobals->bailout = true;
						}
					}
					
					if ( !theGlobals->bailout )
					{
						/* clear error return on way back if we aren't bailing out */
						theGlobals->error = noErr;
					}
				}
				else
				{
					/* We have a file, so copy it */
					
					theGlobals->error = FileCopy(theGlobals->myCPB.hFileInfo.ioVRefNum,
												 theGlobals->myCPB.hFileInfo.ioFlParID,
												 theGlobals->itemName,
												 theGlobals->destinationVRefNum,
												 dstDirID,
												 NULL,
												 NULL,
												 theGlobals->copyBuffer,
												 theGlobals->bufferSize,
												 false);
							
					/* handle any errors from FileCopy */
					if ( theGlobals->error != noErr )
					{
						if ( theGlobals->errorHandler != NULL )
						{
							theGlobals->bailout = CallCopyErrProc(theGlobals->errorHandler, theGlobals->error, fileCopyOp,
													theGlobals->myCPB.hFileInfo.ioVRefNum, theGlobals->myCPB.hFileInfo.ioFlParID, theGlobals->itemName,
													theGlobals->destinationVRefNum, dstDirID, NULL);
							if ( !theGlobals->bailout )
							{
								/* If the CopyErrProc handled the problem, clear the error here */
								theGlobals->error = noErr;
							}
						}
						else
						{
							/* If you don't handle the errors with an error handler, */
							/* then the copy stops here. */
							theGlobals->bailout = true;
						}
					}
				}
			}
		}
		else
		{	/* error handling for PBGetCatInfo */
			/* it's normal to get a fnfErr when indexing; that only means you've hit the end of the directory */
			if ( theGlobals->error != fnfErr )
			{
				if ( theGlobals->errorHandler != NULL )
				{ 
					theGlobals->bailout = CallCopyErrProc(theGlobals->errorHandler, theGlobals->error, getNextItemOp,
											theGlobals->myCPB.dirInfo.ioVRefNum, sourceDirID, NULL, 0, 0, NULL);
					if ( !theGlobals->bailout )
					{
						/* If the CopyErrProc handled the problem, clear the error here */
						theGlobals->error = noErr;
					}
				}
				else
				{
					/* If you don't handle the errors with an error handler, */
					/* then the copy stops here. */
					theGlobals->bailout = true;
				}
			}
		}
		++index; /* prepare to get next item */
	} while ( (theGlobals->error == noErr) && (!theGlobals->bailout) ); /* time to fall back a level? */
}

/*****************************************************************************/

pascal	OSErr	FilteredDirectoryCopy(short srcVRefNum,
									  long srcDirID,
									  ConstStr255Param srcName,
									  short dstVRefNum,
									  long dstDirID,
									  ConstStr255Param dstName,
									  void *copyBufferPtr,
									  long copyBufferSize,
									  Boolean preflight,
									  CopyErrProcPtr copyErrHandler,
									  CopyFilterProcPtr copyFilterProc, ConstStr255Param newName );
/*	¦ Make a copy of a directory structure in a new location with item filtering.
	The FilteredDirectoryCopy function makes a copy of a directory
	structure in a new location. If copyBufferPtr <> NIL, it points to
	a buffer of copyBufferSize that is used to copy files data. The
	larger the supplied buffer, the faster the copy. If
	copyBufferPtr = NIL, then this routine allocates a buffer in the
	application heap. If you pass a copy buffer to this routine, make
	its size a multiple of 512 ($200) bytes for optimum performance.
	
	The optional copyFilterProc parameter lets a routine you define
	decide what files or directories are copied to the destination.
	
	FilteredDirectoryCopy normally creates a new directory *in* the
	specified destination directory and copies the source directory's
	content into the new directory. However, if root parent directory
	(fsRtParID) is passed as the dstDirID parameter and NULL is
	passed as the dstName parameter, DirectoryCopy renames the
	destination volume to the source directory's name (truncating
	if the name is longer than 27 characters) and copies the source
	directory's content into the destination volume's root directory.
	This special case is supported by FilteredDirectoryCopy, but
	not by FSpFilteredDirectoryCopy since with FSpFilteredDirectoryCopy,
	the dstName parameter can not be NULL.
	
	srcVRefNum		input:	Source volume specification.
	srcDirID		input:	Source directory ID.
	srcName			input:	Source directory name, or nil if
							srcDirID specifies the directory.
	dstVRefNum		input:	Destination volume specification.
	dstDirID		input:	Destination directory ID.
	dstName			input:	Destination directory name, or nil if
							dstDirID specifies the directory.
	copyBufferPtr	input:	Points to a buffer of copyBufferSize that
							is used the i/o buffer for the copy or
							nil if you want DirectoryCopy to allocate its
							own buffer in the application heap.
	copyBufferSize	input:	The size of the buffer pointed to
							by copyBufferPtr.
	preflight		input:	If true, DirectoryCopy makes sure there are
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
	copyFilterProc	input:	A pointer to the filter routine you want called
							for each item in the source directory, or NULL
							if you don't want to filter.
	
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
	
	Also see:	CopyErrProcPtr, CopyFilterProcPtr, FSpFilteredDirectoryCopy,
				DirectoryCopy, FSpDirectoryCopy, FileCopy, FSpFileCopy
*/

/*****************************************************************************/

pascal	OSErr	FilteredDirectoryCopy(short srcVRefNum,
									  long srcDirID,
									  ConstStr255Param srcName,
									  short dstVRefNum,
									  long dstDirID,
									  ConstStr255Param dstName,
									  void *copyBufferPtr,
									  long copyBufferSize,
									  Boolean preflight,
									  CopyErrProcPtr copyErrHandler,
									  CopyFilterProcPtr copyFilterProc, ConstStr255Param newName)
{
	EnumerateGlobals theGlobals;
	Boolean	isDirectory;
	OSErr	error;
	Boolean ourCopyBuffer = false;
	Str63	srcDirName, oldDiskName;
	Boolean spaceOK;			
	
	/* Make sure a copy buffer is allocated. */
	if ( copyBufferPtr == NULL )
	{
		/* The caller didn't supply a copy buffer so grab one from the application heap.
		** Try to get a big copy buffer, if we can't, try for a 512-byte buffer.
		** If 512 bytes aren't available, we're in trouble. */
		copyBufferSize = dirCopyBigCopyBuffSize;
		copyBufferPtr = NewPtr(copyBufferSize);
		if ( copyBufferPtr == NULL )
		{
			copyBufferSize = dirCopyMinCopyBuffSize;
			copyBufferPtr = NewPtr(copyBufferSize);
			if ( copyBufferPtr == NULL )
			{
				return ( memFullErr );
			}
		}
		ourCopyBuffer = true;
	}
	
	/* Get the real dirID where we're copying from and make sure it is a directory. */
	error = GetDirectoryID(srcVRefNum, srcDirID, srcName, &srcDirID, &isDirectory);
	if ( error != noErr )
	{
		goto ErrorExit;
	}
	if ( !isDirectory )
	{
		error = dirNFErr;
		goto ErrorExit;
	}
	
	/* Special case destination if it is the root parent directory. */
	/* Since you can't create the root directory, this is needed if */
	/* you want to copy a directory's content to a disk's root directory. */
	if ( (dstDirID == fsRtParID) && (dstName == NULL) )
	{
		dstDirID = fsRtParID;
		isDirectory = true;
		error = noErr;
	}
	else
	{
		/*  Get the real dirID where we're going to put the copy and make sure it is a directory. */
		error = GetDirectoryID(dstVRefNum, dstDirID, dstName, &dstDirID, &isDirectory);
		if ( error != noErr )
		{
			goto ErrorExit;
		}
		if ( !isDirectory )
		{
			error =  dirNFErr;
			goto ErrorExit;
		}
	}
	
	/* Get the real vRefNum of both the source and destination */
	error = DetermineVRefNum(srcName, srcVRefNum, &srcVRefNum);
	if ( error != noErr )
	{
		goto ErrorExit;
	}
	error = DetermineVRefNum(dstName, dstVRefNum, &dstVRefNum);
	if ( error != noErr )
	{
		goto ErrorExit;
	}
	
	if ( preflight )
	{
		error = PreflightDirectoryCopySpace(srcVRefNum, srcDirID, dstVRefNum, copyFilterProc, &spaceOK);
		if ( error != noErr )
		{
			goto ErrorExit;
		}
		if ( !spaceOK )
		{
			error = dskFulErr; /* not enough room on destination */
			goto ErrorExit;
		}
	}

	/* Create the new directory in the destination directory with the */
	/* same name as the source directory. */
	/* 
		if newName is not empty use it rather than the original dir name.
	*/
	if ( newName[0] == 0 )
	{
		error = GetDirName(srcVRefNum, srcDirID, srcDirName);
		if ( error != noErr )
		{
			goto ErrorExit;
		}
	}
	else
	{
		memcpy(srcDirName, newName, 32 );
		
	}	
	/* Again, special case destination if the destination is the */
	/* root parent directory. This time, we'll rename the disk to */
	/* the source directory name. */
	if ( dstDirID == fsRtParID )
	{
		/* Get the current name of the destination disk */
		error = GetDirName(dstVRefNum, fsRtDirID, oldDiskName);
		if ( error == noErr )	
		{
			/* Shorten the name if it's too long to be the volume name */
			TruncPString(srcDirName, srcDirName, 27);
			
			/* Rename the disk */
			error = HRename(dstVRefNum, fsRtParID, oldDiskName, srcDirName);
			/* and copy to the root directory */
			dstDirID = fsRtDirID;
		}
	}
	else
	{
		error = DirCreate(dstVRefNum, dstDirID, srcDirName, &dstDirID);
	}
	if ( error != noErr )
	{
		/* handle any errors from DirCreate */
		if ( copyErrHandler != NULL )
		{
			if ( CallCopyErrProc(copyErrHandler, error, dirCreateOp,
													srcVRefNum, srcDirID, NULL,
													dstVRefNum, dstDirID, srcDirName) )
			{
				goto ErrorExit;
			}
			else
			{
				/* If the CopyErrProc handled the problem, clear the error here */
				/* and continue */
				error = noErr;
			}
		}
		else
		{
			/* If you don't handle the errors with an error handler, */
			/* then the copy stops here. */
			goto ErrorExit;
		}
	}
	
	/* dstDirID is now the newly created directory! */
		
	/* Set up the globals we need to access from the recursive routine. */
	theGlobals.copyBuffer = (Ptr)copyBufferPtr;
	theGlobals.bufferSize = copyBufferSize;
	theGlobals.destinationVRefNum = dstVRefNum; /* so we can get to it always */
	theGlobals.myCPB.hFileInfo.ioNamePtr = (StringPtr)&theGlobals.itemName;
	theGlobals.myCPB.hFileInfo.ioVRefNum = srcVRefNum;
	theGlobals.errorHandler = copyErrHandler;
	theGlobals.bailout = false;
	theGlobals.copyFilterProc =  copyFilterProc;
		
	/* Here we go into recursion land... */
	CopyLevel(srcDirID, dstDirID, &theGlobals);
	error = theGlobals.error;	/* get the result */
	
	if ( !theGlobals.bailout )
	{
		/* Copy comment from source to destination directory. */
		/* Ignore the result because we really don't care if it worked or not. */
		(void) DTCopyComment(srcVRefNum, srcDirID, NULL, dstVRefNum, dstDirID, NULL);
		
		/* Copy the File Manager attributes */
		error = CopyFileMgrAttributes(srcVRefNum, srcDirID, NULL,
					dstVRefNum, dstDirID, NULL, true);
		
		/* handle any errors from CopyFileMgrAttributes */
		if ( (error != noErr) && (copyErrHandler != NULL) )
		{
			theGlobals.bailout = CallCopyErrProc(copyErrHandler, error, copyDirFMAttributesOp,
												srcVRefNum, srcDirID, NULL,
												dstVRefNum, dstDirID, NULL);
		}
	}

ErrorExit:
	/* Get rid of the copy buffer if we allocated it. */
	if ( ourCopyBuffer )
	{
		DisposePtr((Ptr)copyBufferPtr);
	}

	return ( error );
}


/*****************************************************************************/


pascal	OSErr	MacFSpDirectoryCopyRename(const FSSpec *srcSpec,
								 const FSSpec *dstSpec,
								 ConstStr255Param newName,
								 void *copyBufferPtr,
								 long copyBufferSize,
								 Boolean preflight,
								 CopyErrProcPtr copyErrHandler)
{
	return ( FilteredDirectoryCopy(srcSpec->vRefNum, srcSpec->parID, srcSpec->name,
								   dstSpec->vRefNum, dstSpec->parID, dstSpec->name,
								   copyBufferPtr, copyBufferSize, preflight,
								   copyErrHandler, NULL, newName) );
}
