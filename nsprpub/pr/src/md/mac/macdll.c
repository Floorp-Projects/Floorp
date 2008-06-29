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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include <string.h>

#include <Files.h>
#include <Errors.h>
#include <Folders.h>
#include <CodeFragments.h>
#include <Aliases.h>
#include <Resources.h>

#include "IterateDirectory.h"		/* MoreFiles */

#include "MacErrorHandling.h"
#include "macdll.h"
#include "mdmac.h"
#include "macio.h"

#include "primpl.h"
#include "plstr.h"

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

OSErr
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
		return (cfragNoLibraryErr);
	
	tempErr = cfragNoLibraryErr;
	
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
				tempErr = cfragNoLibraryErr;
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
			tempErr = GetDiskFragment(&fragSpec, codeOffset, codeLength, fragSpec.name, kLoadCFrag, &pFilterData->outID, &pFilterData->outAddress, errName);
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
	short					refNum;
	CFragResourceHandle		hCfrg;
	CFragResourceMember*	pCurItem;
	UInt32					curLibIndex;
	Boolean					found;
	
	// asume we didn't find it
	found = false;
	
	// open the resource fork, if we can't bail
	refNum = FSpOpenResFile(inSpec, fsRdPerm);
	require(-1 != refNum, Exit);
	
	// grab out the alias record, if it's not there bail
	hCfrg = (CFragResourceHandle) Get1Resource(kCFragResourceType, kCFragResourceID);
	require(NULL != hCfrg, CloseResourceAndExit);
	
	HLock((Handle)hCfrg);
	
	// get ptr to first item
	pCurItem = &(*hCfrg)->firstMember;
	for (curLibIndex = 0; curLibIndex < (*hCfrg)->memberCount; curLibIndex++)
	{
		// is this our library?
		if ((pCurItem->name[0] == inName[0]) &&
			(strncmp((char*) inName + 1, (char*) pCurItem->name + 1, PR_MIN(pCurItem->name[0], inName[0])) == 0))
		{
			*outCodeOffset = pCurItem->offset;
			*outCodeLength = pCurItem->length;
			found = true;
		}
		
		// skip to next one
		pCurItem = (CFragResourceMember*) ((char*) pCurItem + pCurItem->memberSize);						
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

OSErr
NSFindSymbol(CFragConnectionID inID, Str255 inSymName, Ptr*	outMainAddr, CFragSymbolClass *outSymClass)
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
				err = cfragNoSymbolErr;
		}	
	}
	else
	{	
		err = FindSymbol(inID, inSymName, outMainAddr, outSymClass);
	}
	
	return (err);
}


#pragma mark -


/*-----------------------------------------------------------------

	GetNamedFragmentOffsets

	Get the offsets into the data fork of the named fragment,
	by reading the 'cfrg' resoruce.

-----------------------------------------------------------------*/
OSErr GetNamedFragmentOffsets(const FSSpec *fileSpec, const char* fragmentName,
							UInt32 *outOffset, UInt32 *outLength)
{
	CFragResourceHandle		cFragHandle;
	short									fileRefNum;
	OSErr									err = noErr;
	
	fileRefNum = FSpOpenResFile(fileSpec, fsRdPerm);
	err = ResError();
	if (err != noErr) return err;

	cFragHandle = (CFragResourceHandle)Get1Resource(kCFragResourceType, kCFragResourceID);	
	if (!cFragHandle)
	{
		err = resNotFound;
		goto done;
	}
	
	/* nothing here moves memory, so no need to lock the handle */
	
	err = cfragNoLibraryErr;			/* in case of failure */
	*outOffset = 0;
	*outLength = 0;
	
	/* Now look for the named fragment */
	if ((**cFragHandle).memberCount > 0)
	{
		CFragResourceMemberPtr	memberPtr;
		UInt16									i;
		
		for (	i = 0, memberPtr = &(**cFragHandle).firstMember;
					i < (**cFragHandle).memberCount;
					i ++, memberPtr = (CFragResourceMemberPtr)((char *)memberPtr + memberPtr->memberSize))
		{
			char		memberName[256];
			UInt16	nameLen = PR_MIN(memberPtr->name[0], 255);
		
			// avoid malloc here for speed
			strncpy(memberName, (char *)&memberPtr->name[1], nameLen);
			memberName[nameLen] = '\0';
		
			// fragment names are case insensitive, so act like the system
			if (PL_strcasecmp(memberName, fragmentName) == 0)
			{
				*outOffset = memberPtr->offset;
				*outLength = memberPtr->length;
				err = noErr;
				break;
			}
		}
	}
	
	/* Resource handle will go away when the res fork is closed */
	
done:
	CloseResFile(fileRefNum);
	return err;
}


/*-----------------------------------------------------------------

	GetIndexedFragmentOffsets
	
	Get the offsets into the data fork of the indexed fragment,
	by reading the 'cfrg' resoruce.

-----------------------------------------------------------------*/
OSErr GetIndexedFragmentOffsets(const FSSpec *fileSpec, UInt32 fragmentIndex,
							UInt32 *outOffset, UInt32 *outLength, char **outFragmentName)
{
	CFragResourceHandle		cFragHandle;
	short									fileRefNum;
	OSErr									err = noErr;
	
	fileRefNum = FSpOpenResFile(fileSpec, fsRdPerm);
	err = ResError();
	if (err != noErr) return err;

	cFragHandle = (CFragResourceHandle)Get1Resource(kCFragResourceType, kCFragResourceID);	
	if (!cFragHandle)
	{
		err = resNotFound;
		goto done;
	}
		
	err = cfragNoLibraryErr;			/* in case of failure */
	*outOffset = 0;
	*outLength = 0;
	*outFragmentName = NULL;
	
	/* the CStrFromPStr mallocs, so might move memory */
	HLock((Handle)cFragHandle);
	
	/* Now look for the named fragment */
	if ((**cFragHandle).memberCount > 0)
	{
		CFragResourceMemberPtr	memberPtr;
		UInt16									i;
		
		for (	i = 0, memberPtr = &(**cFragHandle).firstMember;
					i < (**cFragHandle).memberCount;
					i ++, memberPtr = (CFragResourceMemberPtr)((char *)memberPtr + memberPtr->memberSize))
		{
			
			if (i == fragmentIndex)
			{
				char	*fragmentStr;
				CStrFromPStr(memberPtr->name, &fragmentStr);
				if (!fragmentStr)		/* test for allocation failure */
				{
					err = memFullErr;
					break;
				}
				
				*outFragmentName = fragmentStr;
				*outOffset = memberPtr->offset;
				*outLength = memberPtr->length;
				err = noErr;
				break;
			}
		}
	}
	
	HUnlock((Handle)cFragHandle);
	
	/* Resource handle will go away when the res fork is closed */
	
done:
	CloseResFile(fileRefNum);
	return err;
}


/*-----------------------------------------------------------------

	NSLoadNamedFragment
	
	Load the named fragment from the specified file. Aliases must
	have been resolved by this point.

-----------------------------------------------------------------*/

OSErr NSLoadNamedFragment(const FSSpec *fileSpec, const char* fragmentName, CFragConnectionID *outConnectionID)
{
    UInt32      fragOffset, fragLength;
    short       fragNameLength;
    Ptr             main;
    Str255      fragName;
    Str255      errName;
    OSErr           err;
    
    err = GetNamedFragmentOffsets(fileSpec, fragmentName, &fragOffset, &fragLength);
    if (err != noErr) return err;
    
    // convert fragment name to pascal string
    fragNameLength = strlen(fragmentName);
    if (fragNameLength > 255)
        fragNameLength = 255;
    BlockMoveData(fragmentName, &fragName[1], fragNameLength);
    fragName[0] = fragNameLength;
    
    // Note that we pass the fragment name as the 4th param to GetDiskFragment.
    // This value affects the ability of debuggers, and the Talkback system,
    // to match code fragments with symbol files
    err = GetDiskFragment(fileSpec, fragOffset, fragLength, fragName, 
                    kLoadCFrag, outConnectionID, &main, errName);

    return err;
}


/*-----------------------------------------------------------------

	NSLoadIndexedFragment
	
	Load the indexed fragment from the specified file. Aliases must
	have been resolved by this point.
	
	*outFragName is a malloc'd block containing the fragment name,
	if returning noErr.

-----------------------------------------------------------------*/

OSErr NSLoadIndexedFragment(const FSSpec *fileSpec, PRUint32 fragmentIndex,
                            char** outFragName, CFragConnectionID *outConnectionID)
{
    UInt32      fragOffset, fragLength;
    char        *fragNameBlock = NULL;
    Ptr         main;
    Str255      fragName = "\p";
    Str255      errName;
    OSErr       err;

    *outFragName = NULL;
    
    err = GetIndexedFragmentOffsets(fileSpec, fragmentIndex, &fragOffset, &fragLength, &fragNameBlock);
    if (err != noErr) return err;
                
    if (fragNameBlock)
    {
        UInt32 nameLen = strlen(fragNameBlock);
        if (nameLen > 63)
            nameLen = 63;
        BlockMoveData(fragNameBlock, &fragName[1], nameLen);
        fragName[0] = nameLen;
    }

    // Note that we pass the fragment name as the 4th param to GetDiskFragment.
    // This value affects the ability of debuggers, and the Talkback system,
    // to match code fragments with symbol files
    err = GetDiskFragment(fileSpec, fragOffset, fragLength, fragName, 
                    kLoadCFrag, outConnectionID, &main, errName);
    if (err != noErr)
    {
        free(fragNameBlock);
        return err;
    }

    *outFragName = fragNameBlock;
    return noErr;
}


