/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 * 
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 * 
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#undef OLDROUTINENAMES
#define OLDROUTINENAMES 1

#include <Files.h>
#include <Errors.h>
#include <Folders.h>
#include <CodeFragments.h>
#include <Aliases.h>

#include "MacErrorHandling.h"
#include "primpl.h"
#include "plstr.h"

typedef struct CfrgItem CfrgItem, *CfrgItemPtr;
struct CfrgItem 
{
        OSType          fArchType;
        UInt32          fUpdateLevel;
        UInt32     		fCurVersion;
        UInt32     		fOldDefVersion;
        UInt32          fAppStackSize;
        UInt16          fAppSubFolder;
        UInt8       	fUsage;
        UInt8           fLocation;
        UInt32          fCodeOffset;
        UInt32          fCodeLength;
        UInt32          fReserved1;
        UInt32          fReserved2;
        UInt16          fItemSize; // %4 == 0
        Str255          fName;
        // Only make the final p-string as long as needed, then align to
        // a longword boundary
};

typedef struct CfrgHeader CfrgHeader, *CfrgHeaderPtr, **CfrgHeaderHandle;
struct CfrgHeader 
{
        UInt32          fReserved1;
        UInt32          fReserved2;
        UInt32          fVersion;
        UInt32          fReserved3;
        UInt32          fReserved4;
        UInt32          fFiller1;
        UInt32          fFiller2;
        UInt32          fItemCount;
        CfrgItem        fCfrgItemArray[1];
};

/*
	turds used to iterate through the directories looking
	for the desired library.
*/

struct GetSharedLibraryFilterProcData
{
	Boolean				inRecursive;
	StringPtr			inName;
	
	Boolean				outFound;
	CFragConnectionID	outID;
	Ptr					outAddress;
	OSErr				outError;
};
typedef struct GetSharedLibraryFilterProcData GetSharedLibraryFilterProcData;

static pascal void
GetSharedLibraryFilterProc(const CInfoPBRec* const inCpb, Boolean* inWantQuit, void *inFilterData);


/*
	NSGetSharedLibrary
	
	Unfortunately CFM doesn't support user specified loader paths,
	so we emulate the behavior.  Effectively this is a GetSharedLibrary
	where the loader path is user defined.
*/

extern OSErr
NSGetSharedLibrary(Str255 inLibName, CFragConnectionID* outID, Ptr* outMainAddr)
{
	char*		curLibPath;
	char*		freeCurLibPath;
	OSErr		tempErr;
	Boolean		recursive;
	FSSpec		curFolder;	
	GetSharedLibraryFilterProcData filterData;
	char		*endCurLibPath;
	Boolean		done;
	
	filterData.outFound = false;
	filterData.outID = (CFragConnectionID)(-1);
	filterData.outAddress = NULL;
	filterData.inName = inLibName;
		
	freeCurLibPath = curLibPath = PR_GetLibraryPath();
	
	if (curLibPath == NULL)
		return (fragLibNotFound);
	
	tempErr = fragLibNotFound;
	
	do
	{
		endCurLibPath = PL_strchr(curLibPath, PR_PATH_SEPARATOR);
		done = (endCurLibPath == NULL);

#if 0
		// we overload the first character of a path if it's :
		// then we want to recursively search that path
		// see if path should be recursive
		if (*curLibPath == ':')
		{
			// ':' is an illegal character in the name of a file
			// if we start any path with this, we want to allow 
			// search recursively
			curLibPath++;
			recursive = true;
		}
		else
#endif
		{
			recursive = false;
		}
		
		if (!done)
			*endCurLibPath = '\0';	// NULL terminate the string
		
		// convert to FSSpec
		tempErr = ConvertUnixPathToFSSpec(curLibPath, &curFolder);	

		// now look in this directory
		if (noErr == tempErr)
		{
			filterData.inRecursive = recursive;
			FSpIterateDirectory(&curFolder, recursive ? 0 : 1, &GetSharedLibraryFilterProc, &filterData);
			
			if (filterData.outFound)
			{
				*outID = filterData.outID;
				*outMainAddr = filterData.outAddress;
				tempErr = noErr;
				break;
			}
			else 
			{
				tempErr = fragLibNotFound;
			}
		}
		
		curLibPath = endCurLibPath + 1;	// skip to next path (past the '\0');
	} while (!done);

	free(freeCurLibPath);
	return (tempErr);
}


static Boolean
LibInPefContainer(const FSSpec* inSpec, StringPtr inName, UInt32* outCodeOffset, UInt32* outCodeLength);


/*
	GetSharedLibraryFilterProc
	
	Callback to FSpIterateDirectory, finds a library with the name matching the
	data in inFilterData (of type GetSharedLibraryFilterProcData).  Forces a quit
	when a match is found.
*/

static pascal void
GetSharedLibraryFilterProc(const CInfoPBRec* const inCpb, Boolean* inWantQuit, void *inFilterData)
{
	GetSharedLibraryFilterProcData* pFilterData = (GetSharedLibraryFilterProcData*) inFilterData;

	if ((inCpb->hFileInfo.ioFlAttrib & (1 << ioDirFlg)) == 0)
	{
		FSSpec	fragSpec;
		OSErr	tempErr;
		Str255	errName;
		Boolean	crap;
		UInt32	codeOffset;
		UInt32	codeLength;
		
		// it's a file
		
		// ¥ fix-me do we really want to allow all 'APPL's' for in which to find this library?
		switch (inCpb->hFileInfo.ioFlFndrInfo.fdType)
		{
			case kCFragLibraryFileType:
			case 'APPL':
				tempErr = FSMakeFSSpec(inCpb->hFileInfo.ioVRefNum, inCpb->hFileInfo.ioFlParID, inCpb->hFileInfo.ioNamePtr, &fragSpec);

				// this shouldn't fail
				if (noErr != tempErr)
				{
					return;
				}
				
				// resolve an alias if this was one
				tempErr = ResolveAliasFile(&fragSpec, true, &crap, &crap);

				// if got here we have a shlb (or app-like shlb)
				if (noErr != tempErr)
				{
					// probably couldn't resolve an alias
					return;
				}
		
				break;
			default:
				return;
		}
	
		// see if this symbol is in this fragment
		if (LibInPefContainer(&fragSpec, pFilterData->inName, &codeOffset, &codeLength))
			tempErr = GetDiskFragment(&fragSpec, codeOffset, codeLength, pFilterData->inName, kLoadLib, &pFilterData->outID, &pFilterData->outAddress, errName);
		else
			return;
				
		// stop if we found a library by that name
		if (noErr == tempErr)
		{			
			*inWantQuit = true;
			pFilterData->outFound = true;
			pFilterData->outError = tempErr;
		}
	}
	// FSpIterateDirectory will automagically call us for subsequent sub-dirs if necessary
}


/*
	LibInPefContainer
	
	Tell whether library inName is contained it the file pointed to by inSpec.
	Return the codeOffset and codeLength information, for a subsequent
	call to GetDiskFragment.
*/

static Boolean
LibInPefContainer(const FSSpec* inSpec, StringPtr inName, UInt32* outCodeOffset, UInt32* outCodeLength)
{
	short				refNum;
	CfrgHeaderHandle	hCfrg;
	CfrgItem*			pCurItem;
	UInt32				curLibIndex;
	Boolean				found;
	
	// asume we didn't find it
	found = false;
	
	// open the resource fork, if we can't bail
	refNum = FSpOpenResFile(inSpec, fsRdPerm);
	require(-1 != refNum, Exit);
	
	// grab out the alias record, if it's not there bail
	hCfrg = (CfrgHeaderHandle) Get1Resource(kCFragResourceType, kCFragResourceID);
	require(NULL != hCfrg, CloseResourceAndExit);
	
	HLock((Handle)hCfrg);
	
	// get ptr to first item
	pCurItem = &(*hCfrg)->fCfrgItemArray[0];
	for (curLibIndex = 0; curLibIndex < (*hCfrg)->fItemCount; curLibIndex++)
	{
		// is this our library?
		if ((pCurItem->fName[0] == inName[0]) &&
			(strncmp((char*) inName + 1, (char*) pCurItem->fName + 1, PR_MIN(pCurItem->fName[0], inName[0])) == 0))
		{
			*outCodeOffset = pCurItem->fCodeOffset;
			*outCodeLength = pCurItem->fCodeLength;
			found = true;
		}
		
		// skip to next one
		pCurItem = (CfrgItem*) ((char*) pCurItem + pCurItem->fItemSize);						
	}
	
	HUnlock((Handle)hCfrg);
	
CloseResourceAndExit:
	CloseResFile(refNum);
Exit:
	return (found);

}


/*
	NSFindSymbol
	
	Workaround bug in CFM FindSymbol (in at least 7.5.5) where symbols with lengths
	greater than 63 chars cause a "paramErr".  We iterate through all symbols
	in the library to find the desired symbol.
*/

extern OSErr
NSFindSymbol(CFragConnectionID inID, Str255 inSymName, Ptr*	outMainAddr, SymClass *outSymClass)
{
	OSErr	err;
	
	if (inSymName[0] > 63)
	{
		/* 
			if there are greater than 63 characters in the
			name, CFM FindSymbol fails, so let's iterate through all
			of the symbols in the fragment and grab it 
			that way.
		*/
		long 	symbolCount;
		Str255	curSymName;
		long	curIndex;
		Boolean found;
		
		found = false;
		err = CountSymbols(inID, &symbolCount);
		if (noErr == err)
		{
			/* now iterate through all the symbols in the library */
			/* per DTS the indices apparently go 0 to n-1 */
			for (curIndex = 0; (curIndex <= symbolCount - 1 && !found); curIndex++)
			{
				err = GetIndSymbol(inID, curIndex, curSymName, outMainAddr, outSymClass);
				if (noErr == err && curSymName[0] == inSymName[0] && !strncmp((char*)curSymName + 1, (char*)inSymName + 1, curSymName[0]))
				{
					/* found our symbol */
					found = true;
				}
			}
			
			/* if we didn't find it set the error code so below it won't take this symbol */
			if (!found)
				err = fragSymbolNotFound;
		}	
	}
	else
	{	
		err = FindSymbol(inID, inSymName, outMainAddr, outSymClass);
	}
	
	return (err);
}

