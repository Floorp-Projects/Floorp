/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
//	  This file is included by nsFile.cp, and includes the Macintosh-specific
//	  implementations.


#include <string.h>

#include "prtypes.h"
#include "nscore.h"

#ifndef RHAPSODY
#include "FullPath.h"
#include "FileCopy.h"
#include "MoreFilesExtras.h"
#endif

#include <Aliases.h>
#include <Folders.h>
#include <Errors.h>
#include <Math64.h>
#include <TextUtils.h>
#include <Processes.h>
#include <limits.h>		// ULONG_MAX

#include "nsFileSpec.h"
#include "nsEscape.h"
#include "nsXPIDLString.h"


#ifndef RHAPSODY
const unsigned char* kAliasHavenFolderName = "\pnsAliasHaven";

//========================================================================================
namespace MacFileHelpers
//========================================================================================
{
	inline void						PLstrcpy(Str255 dst, ConstStr255Param src)
									{
										memcpy(dst, src, 1 + src[0]);
									}

	void							PLstrcpy(Str255 dst, const char* src, int inMaxLen=255);
	void							PLstrncpy(Str255 dst, const char* src, int inMaxLen);

	void							SwapSlashColon(char * s);
	OSErr							FSSpecFromUnixPath(
										const char * unixPath,
										FSSpec& ioSpec,
										Boolean hexDecode,
										Boolean resolveAlias,
										Boolean allowPartial = false,
										Boolean createDirs = false);
	char*							MacPathFromUnixPath(
										const char* unixPath,
										Boolean hexDecode);
	char*							EncodeMacPath(
										char* inPath, // NOT const - gets clobbered
										Boolean prependSlash,
										Boolean doEscape );
	OSErr							FSSpecFromPathname(
										const char* inPathNamePtr,
										FSSpec& ioSpec,
										Boolean inCreateDirs);
	char*							PathNameFromFSSpec(
										const FSSpec& inSpec );
	OSErr							CreateFolderInFolder(
										short				refNum,		// Parent directory/volume
										long				dirID,
										ConstStr255Param	folderName,	   // Name of the new folder
										short&				outRefNum,	  // Volume of the created folder
										long&				outDirID);	  // 

	// Some routines to support an "alias haven" directory.	 Aliases in this directory
	// are never resolved.	There is a ResolveAlias here that respects that.  This is
	// to support attaching of aliases in mail.
	void							EnsureAliasHaven();
	void							SetNoResolve(Boolean inResolve);
	PRBool							IsAliasSafe(const FSSpec& inSpec);
	OSErr							MakeAliasSafe(FSSpec& inOutSpec);
	OSErr							ResolveAliasFile(FSSpec& inOutSpec, Boolean& wasAliased);

	Boolean							sNoResolve = false;
	long							sAliasHavenDirID = 0;
	short							sAliasHavenVRefNum = 0;
} // namespace MacFileHelpers

//----------------------------------------------------------------------------------------
void MacFileHelpers::PLstrcpy(Str255 dst, const char* src, int inMax)
//----------------------------------------------------------------------------------------
{
	int srcLength = strlen(src);
	NS_ASSERTION(srcLength <= inMax, "Oops, string is too long!");
	if (srcLength > inMax)
		srcLength = inMax;
	dst[0] = srcLength;
	memcpy(&dst[1], src, srcLength);
}

//----------------------------------------------------------------------------------------
void MacFileHelpers::PLstrncpy(Str255 dst, const char* src, int inMax)
//----------------------------------------------------------------------------------------
{
	int srcLength = strlen(src);
	if (srcLength > inMax)
		srcLength = inMax;
	dst[0] = srcLength;
	memcpy(&dst[1], src, srcLength);
}

//-----------------------------------
void MacFileHelpers::SwapSlashColon(char * s)
//-----------------------------------

{
	while (*s)
	{
		if (*s == '/')
			*s++ = ':';
		else if (*s == ':')
			*s++ = '/';
		else
			*s++;
	}
} // MacFileHelpers::SwapSlashColon

//-----------------------------------
char* MacFileHelpers::EncodeMacPath(
	char* inPath, // NOT const, gets clobbered
	Boolean prependSlash,
	Boolean doEscape )
//	  Transforms Macintosh style path into Unix one
//	  Method: Swap ':' and '/', hex escape the result
//-----------------------------------
{
	if (inPath == nsnull)
		return nsnull;
	int pathSize = strlen(inPath);
	
	// XP code sometimes chokes if there's a final slash in the unix path.
	// Since correct mac paths to folders and volumes will end in ':', strip this
	// first.
	char* c = inPath + pathSize - 1;
	if (*c == ':')
	{
		*c = 0;
		pathSize--;
	}

	char * newPath = nsnull;
	char * finalPath = nsnull;
	
	if (prependSlash)
	{
		newPath = new char[pathSize + 2];
		newPath[0] = ':';	 // It will be converted to '/'
		memcpy(&newPath[1], inPath, pathSize + 1);
	}
	else
	{
		newPath = new char[pathSize + 1];
		strcpy(newPath, inPath);
	}
	if (newPath)
	{
		SwapSlashColon( newPath );
		if (doEscape)
		{
			finalPath = nsEscape(newPath, url_Path);
			delete [] newPath;
		}
		else
			finalPath = newPath;
	}
	delete [] inPath;
	return finalPath;
} // MacFileHelpers::EncodeMacPath

//----------------------------------------------------------------------------------------
inline void MacFileHelpers::SetNoResolve(Boolean inResolve)
//----------------------------------------------------------------------------------------
{
	sNoResolve = inResolve;
} // MacFileHelpers::SetNoResolve

//----------------------------------------------------------------------------------------
OSErr MacFileHelpers::MakeAliasSafe(FSSpec& inOutSpec)
// Pass in the spec of an alias.  This copies the file to the safe haven folder, and
// returns the spec of the copy to the caller
//----------------------------------------------------------------------------------------
{
	EnsureAliasHaven();
	nsFileSpec dstDirSpec(sAliasHavenVRefNum, sAliasHavenDirID, "\p");

	// Make sure its name is unique
	nsFileSpec havenSpec(sAliasHavenVRefNum, sAliasHavenDirID, "\pG'day");
	if (havenSpec.Valid())
		havenSpec.MakeUnique(inOutSpec.name);
	// Copy the file into the haven directory
	if (havenSpec.Valid())
	{
		OSErr err = ::FSpFileCopy(
						&inOutSpec,
						dstDirSpec,
						havenSpec.GetLeafPName(),
						nil, 0, true);
		// Return the spec of the copy to the caller.
		if (err != noErr)
			return err;
		inOutSpec = havenSpec;
	}
	return noErr;
} // MacFileHelpers::MakeAliasSafe

//----------------------------------------------------------------------------------------
char* MacFileHelpers::MacPathFromUnixPath(const char* unixPath, Boolean hexDecode)
//----------------------------------------------------------------------------------------
{
	// Relying on the fact that the unix path is always longer than the mac path:
	size_t len = strlen(unixPath);
	char* result = new char[len + 2]; // ... but allow for the initial colon in a partial name
	// REMEMBER: at the end we call SwapSlashColon, so bear that in mind when you see
	// this code using '/' as a separator in what is supposed to be a Macintosh path!
	if (result)
	{
		char* dst = result;
		const char* src = unixPath;
		if (*src == '/')			 // * full path
			src++;
		else if (strchr(src, '/') && *src != '.')
		{
			// * partial path, and not just a leaf name. The '.' test is there because
			// the loop below will add sufficient colons in that case.
			*dst++ = '/';
		}
		// Copy src to dst, but watch out for .. and .
		char c = '/';
		do
		{
			char cprev = c; // remember the previous char (initially /)
			c = *src++;
			if (c == '.' && cprev == '/')
			{
				char* dstSaved = dst;
				// Special cases: "." and "..". Convert to ':' and '::'
				*dst++ = '/';  // . becomes :
				c = *src++;
				if (c == '.')
				{
					*dst++ = '/'; // .. becomes ::
					c = *src++;
				}
				if (c == '/')
				{
					// ../	  becomes	:: so just skip the slash
					// ./	  becomes	:  "  "	   "	"	"
					src++;
				}
				else if (c)
				{
					// Oh. A file called ".foo" or "..foo"
					// Back up and just do the normal thing.
					src -= (dst - dstSaved);
					dst = dstSaved;
					// Since c is not '/', we won't do this stuff on the
					// next iteration.
				}
				continue;
			}
			else if (c == '/' && cprev == '/')
			{
				// Hmm. A 'run on' path with two slashes right next to each other.
				// This is an illegal path, but, hey, we'll be tough and try to
				// deal with it (especially since '::' has loaded semantics in
				// a Mac path)
				continue;
			}
			*dst++ = c;
		} while (c);
		if (hexDecode)
			nsUnescape(result);	   // Hex Decode
		MacFileHelpers::SwapSlashColon(result);
	}
	return result;
} // MacFileHelpers::MacPathFromUnixPath

//----------------------------------------------------------------------------------------
OSErr MacFileHelpers::FSSpecFromPathname(
	const char* inPathNamePtr,
	FSSpec& ioSpec, // used as in-parameter for a relative path.
	Boolean inCreateDirs)
// FSSpecFromPathname reverses PathNameFromFSSpec.
// It returns a FSSpec given a c string which is a mac pathname.
//----------------------------------------------------------------------------------------
{
	OSErr err;
	// Simplify this routine to use FSMakeFSSpec if length < 255. Otherwise use the MoreFiles
	// routine FSpLocationFromFullPath, which allocates memory, to handle longer pathnames. 
	
	short inVRefNum = ioSpec.vRefNum;
	long inParID = ioSpec.parID;
	
	size_t inLength = strlen(inPathNamePtr);
	bool isRelative = (strchr(inPathNamePtr, ':') == 0 || *inPathNamePtr == ':');
#ifdef NS_DEBUG
	// Attempt to catch people sending unix paths in to routines that expect native ones.
	NS_ASSERTION(strchr(inPathNamePtr, '/') == 0,
			"Possible unix path where native path is required");
#endif
	if (inLength < 255)
	{
		Str255 pascalpath;
		MacFileHelpers::PLstrcpy(pascalpath, inPathNamePtr);
		if (isRelative)
			err = ::FSMakeFSSpec(inVRefNum, inParID, pascalpath, &ioSpec);
		else
			err = ::FSMakeFSSpec(0, 0, pascalpath, &ioSpec);
	}
	else if (!isRelative)
		err = FSpLocationFromFullPath(inLength, inPathNamePtr, &ioSpec);
	else
		err = bdNamErr;

	if ((err == dirNFErr || err == bdNamErr) && inCreateDirs)
	{
		const char* path = inPathNamePtr;
		if (isRelative)
		{
			ioSpec.vRefNum = inVRefNum;
			ioSpec.parID = inParID;
		}
		else
		{
			ioSpec.vRefNum = 0;
			ioSpec.parID = 0;
		}
		do {
			// Locate the colon that terminates the node.
			// But if we've a partial path (starting with a colon), find the second one.
			const char* nextColon = strchr(path + (*path == ':'), ':');
			// Well, if there are no more colons, point to the end of the string.
			if (!nextColon)
				nextColon = path + strlen(path);

			// Make a pascal string out of this node.  Include initial
			// and final colon, if any!
			Str255 ppath;
			MacFileHelpers::PLstrncpy(ppath, path, nextColon - path + 1);
			
			// Use this string as a relative path using the directory created
			// on the previous round (or directory 0,0 on the first round).
			err = ::FSMakeFSSpec(ioSpec.vRefNum, ioSpec.parID, ppath, &ioSpec);

			// If this was the leaf node, then we are done.
			if (!*nextColon)
				break;

			// Since there's more to go, we have to get the directory ID, which becomes
			// the parID for the next round.
			if (err == noErr)
			{
				// The directory (or perhaps a file) exists. Find its dirID.
				long dirID;
				Boolean isDirectory;
				err = ::FSpGetDirectoryID(&ioSpec, &dirID, &isDirectory);
				if (!isDirectory)
					return dupFNErr; // oops! a file exists with that name.
				if (err)
					return err;
				ioSpec.parID = dirID;
			}
			else if (err == fnfErr)
			{
				// If we got "file not found", then
				// we need to create a directory.
				err = ::FSpDirCreate(&ioSpec, smCurrentScript, &ioSpec.parID);
				// For some reason, this usually returns fnfErr, even though it works.
				if (err == fnfErr)
					err = noErr;
			}
			if (err != noErr)
				return err;
			path = nextColon; // next round
		} while (1);
	}
	return err;
} // MacFileHelpers::FSSpecFromPathname

//----------------------------------------------------------------------------------------
OSErr MacFileHelpers::CreateFolderInFolder(
	short				 refNum,	 // Parent directory/volume
	long				dirID,
	ConstStr255Param	folderName,	   // Name of the new folder
	short&				  outRefNum,	// Volume of the created folder
	long&				 outDirID)	  // 
// Creates a folder named 'folderName' inside a folder.
// The errors returned are same as PBDirCreate
//----------------------------------------------------------------------------------------
{
	HFileParam hpb;
	hpb.ioVRefNum = refNum;
	hpb.ioDirID = dirID;
	hpb.ioNamePtr = (StringPtr)&folderName;

	OSErr err = PBDirCreateSync((HParmBlkPtr)&hpb);
	if (err == noErr)
	{
		outRefNum = hpb.ioVRefNum;
		outDirID = hpb.ioDirID;
	}
	else
	{
		outRefNum = 0;
		outDirID = 0;
	}
	return err;
} // MacFileHelpers::CreateFolderInFolder

//----------------------------------------------------------------------------------------
void MacFileHelpers::EnsureAliasHaven()
//----------------------------------------------------------------------------------------
{
	// Alias Haven is a directory in which we never resolve aliases.
	if (sAliasHavenVRefNum != 0)
		return;

	
	FSSpec temp;
	if (FindFolder(0, kTemporaryFolderType, true, & temp.vRefNum, &temp.parID) == noErr)
	{
		CreateFolderInFolder(
			temp.vRefNum,				  // Parent directory/volume
			temp.parID,
			kAliasHavenFolderName,		  // Name of the new folder
			sAliasHavenVRefNum,		   // Volume of the created folder
			sAliasHavenDirID);		  
	}
} // MacFileHelpers::EnsureAliasHaven

//----------------------------------------------------------------------------------------
PRBool MacFileHelpers::IsAliasSafe(const FSSpec& inSpec)
// Returns true if the alias is in the alias haven directory, or if alias resolution
// has been turned off.
//----------------------------------------------------------------------------------------
{
	return sNoResolve
		|| (inSpec.parID == sAliasHavenDirID && inSpec.vRefNum == sAliasHavenVRefNum);
} // MacFileHelpers::IsAliasSafe

//----------------------------------------------------------------------------------------
OSErr MacFileHelpers::ResolveAliasFile(FSSpec& inOutSpec, Boolean& wasAliased)
//----------------------------------------------------------------------------------------
{
	wasAliased = false;
	if (IsAliasSafe(inOutSpec))
		return noErr;
	Boolean dummy;	  
	return ::ResolveAliasFile(&inOutSpec, TRUE, &dummy, &wasAliased);
} // MacFileHelpers::ResolveAliasFile

//-----------------------------------
OSErr MacFileHelpers::FSSpecFromUnixPath(
	const char * unixPath,
	FSSpec& ioSpec,
	Boolean hexDecode,
	Boolean resolveAlias,
	Boolean allowPartial,
	Boolean createDirs)
// File spec from URL. Reverses GetURLFromFileSpec 
// Its input is only the <path> part of the URL
// JRM 97/01/08 changed this so that if it's a partial path (doesn't start with '/'),
// then it is combined with inOutSpec's vRefNum and parID to form a new spec.
//-----------------------------------
{
	if (unixPath == nsnull)
		return badFidErr;
	char* macPath = MacPathFromUnixPath(unixPath, hexDecode);
	if (!macPath)
		return memFullErr;

	OSErr err = noErr;
	if (!allowPartial)
	{
		NS_ASSERTION(*unixPath == '/' /*full path*/, "Not a full Unix path!");
	}
	err = FSSpecFromPathname(macPath, ioSpec, createDirs);
	if (err == fnfErr)
		err = noErr;
	Boolean dummy;	  
	if (err == noErr && resolveAlias)	 // Added 
		err = MacFileHelpers::ResolveAliasFile(ioSpec, dummy);
	delete [] macPath;
	NS_ASSERTION(err==noErr||err==fnfErr||err==dirNFErr||err==nsvErr, "Not a path!");
	return err;
} // MacFileHelpers::FSSpecFromLocalUnixPath

//-----------------------------------
char* MacFileHelpers::PathNameFromFSSpec( const FSSpec& inSpec )
// Returns a full pathname to the given file
// Returned value is allocated with new [], and must be freed with delete []
// For consistency and to work under OS X this creates an nsILocalFileMac and has it do the work.
//-----------------------------------
{
	char* result = nil;
	nsresult rv;

    FSSpec nonConstSpec = inSpec;
    nsXPIDLCString path;	
    nsCOMPtr<nsILocalFileMac> macFile;
    
    rv = NS_NewLocalFileWithFSSpec(&nonConstSpec, PR_TRUE, getter_AddRefs(macFile));
    if (NS_FAILED(rv)) return nsnull;
    nsCOMPtr<nsILocalFile> localFile(do_QueryInterface(macFile, &rv));
    if (NS_FAILED(rv)) return nsnull;
    rv = localFile->GetPath(getter_Copies(path));
    if (NS_FAILED(rv)) return nsnull;
    PRInt32 strLen = path.Length();
    result = new char [strLen + 1];
    if (!result) return nsnull;
	memcpy(result, path.get(), strLen);
	result[ strLen ] = 0;

	return result;
} // MacFileHelpers::PathNameFromFSSpec

#endif

#pragma mark -

//========================================================================================
//					  Macintosh nsFileSpec implementation
//========================================================================================

#ifndef RHAPSODY
//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec()
//----------------------------------------------------------------------------------------
{
//    NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
	Clear();
}
#endif

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const FSSpec& inSpec, PRBool resolveAlias)
//----------------------------------------------------------------------------------------
: mSpec(inSpec)
, mError(NS_OK)
{
//    NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
	if (resolveAlias)
	{
		PRBool dummy;
		ResolveSymlink(dummy);
	}
}

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const FSSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	mSpec = inSpec;
	mError = NS_OK;
}

#ifndef RHAPSODY
//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
:	 mSpec(inSpec.mSpec)
,	 mError(inSpec.Error())
{
//    NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
}

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const char* inNativePathString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
{
//  NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
    Clear();		// this sets mError to NS_ERROR_NOT_INITIALIZED

	if (inNativePathString)
	{
		mError = NS_FILE_RESULT(MacFileHelpers::FSSpecFromPathname(
				  inNativePathString, mSpec, inCreateDirs));
		if (mError == NS_FILE_RESULT(fnfErr))
			mError = NS_OK;
	}

} // nsFileSpec::nsFileSpec

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsString& inNativePathString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
{
//    NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
	Clear();		// this sets mError to NS_ERROR_NOT_INITIALIZED

	mError = NS_FILE_RESULT(
		MacFileHelpers::FSSpecFromPathname(
			NS_LossyConvertUCS2toASCII(inNativePathString).get(),
			mSpec, inCreateDirs));
	if (mError == NS_FILE_RESULT(fnfErr))
		mError = NS_OK;

} // nsFileSpec::nsFileSpec

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(short vRefNum, long parID, ConstStr255Param fileName,  PRBool resolveAlias)
//----------------------------------------------------------------------------------------
{
//    NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
	mError = NS_FILE_RESULT(::FSMakeFSSpec(vRefNum, parID, fileName, &mSpec));
	if (mError == NS_FILE_RESULT(fnfErr))
		mError = NS_OK;
 
	if (resolveAlias)
	{
		PRBool dummy;
		ResolveSymlink(dummy);
	}
}

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
//    NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
	*this = inPath.GetFileSpec();
}

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
	Clear();		// this sets mError to NS_ERROR_NOT_INITIALIZED

	if (inString)
	{
		mError = NS_FILE_RESULT(MacFileHelpers::FSSpecFromPathname(inString, mSpec, true));
		if (mError == NS_FILE_RESULT(fnfErr))
			mError = NS_OK;
	}
	
} // nsFileSpec::operator =

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	mPath.SetToEmpty();
	mSpec.vRefNum = inSpec.mSpec.vRefNum;
	mSpec.parID = inSpec.mSpec.parID;
	memcpy(mSpec.name, inSpec.mSpec.name, inSpec.mSpec.name[0] + 1);
	mError = inSpec.Error();	// note that the error is propagated
} // nsFileSpec::operator =

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
	*this = inPath.GetFileSpec();
} // nsFileSpec::operator =

//----------------------------------------------------------------------------------------
inline void nsFileSpec::Clear()
//----------------------------------------------------------------------------------------
{
	mPath.SetToEmpty();
	mSpec.vRefNum = 0;
	mSpec.parID = 0;
	mSpec.name[0] = 0;
	mError = NS_ERROR_NOT_INITIALIZED;
}

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::Exists() const
//----------------------------------------------------------------------------------------
{
  if (NS_FAILED(mError)) return PR_FALSE;
	FSSpec temp;
	return ::FSMakeFSSpec(mSpec.vRefNum, mSpec.parID, mSpec.name, &temp) == noErr;
} // nsFileSpec::Exists()

//----------------------------------------------------------------------------------------
void nsFileSpec::GetModDate(TimeStamp& outStamp) const
//----------------------------------------------------------------------------------------
{
	CInfoPBRec pb;
	if (GetCatInfo(pb) == noErr)
		outStamp = ((DirInfo*)&pb)->ioDrMdDat; // The mod date is in the same spot for files and dirs.
	else
		outStamp = 0;
} // nsFileSpec::GetModDate

//----------------------------------------------------------------------------------------
PRUint32 nsFileSpec::GetFileSize() const
//----------------------------------------------------------------------------------------
{
	CInfoPBRec pb;
	if (noErr == GetCatInfo(pb))
		return (PRUint32)((HFileInfo*)&pb)->ioFlLgLen;
	return 0;
} // nsFileSpec::GetFileSize

//----------------------------------------------------------------------------------------
void nsFileSpec::SetLeafName(const char* inLeafName)
// In leaf name can actually be a partial path...
//----------------------------------------------------------------------------------------
{
	NS_ASSERTION(inLeafName, "Attempt to set leaf name with a null string");
	
	mPath.SetToEmpty();

	if (inLeafName)
	{
		// what about long relative paths?	Hmm?  We don't have a routine for this anywhere.
		Str255 partialPath;
		MacFileHelpers::PLstrcpy(partialPath, inLeafName);
		mError = NS_FILE_RESULT(
			::FSMakeFSSpec(mSpec.vRefNum, mSpec.parID, partialPath, &mSpec));
		if (mError == NS_FILE_RESULT(fnfErr))
			mError = NS_OK;
	}
	
} // nsFileSpec::SetLeafName

//----------------------------------------------------------------------------------------
char* nsFileSpec::GetLeafName() const
// Result needs to be nsCRT::free()ed.
//----------------------------------------------------------------------------------------
{
	char leaf[64];
	memcpy(leaf, &mSpec.name[1], mSpec.name[0]);
	leaf[mSpec.name[0]] = '\0';
	return nsCRT::strdup(leaf);
} // nsFileSpec::GetLeafName

//----------------------------------------------------------------------------------------
void nsFileSpec::MakeAliasSafe()
//----------------------------------------------------------------------------------------
{
	mPath.SetToEmpty();
	mError = NS_FILE_RESULT(MacFileHelpers::MakeAliasSafe(mSpec));
} // nsFileSpec::MakeAliasSafe

//----------------------------------------------------------------------------------------
void nsFileSpec::MakeUnique(ConstStr255Param inSuggestedLeafName)
//----------------------------------------------------------------------------------------
{
	mPath.SetToEmpty();
	if (inSuggestedLeafName[0] > 0)
		MacFileHelpers::PLstrcpy(mSpec.name, inSuggestedLeafName);

	MakeUnique();
} // nsFileSpec::MakeUnique

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsFile() const
//----------------------------------------------------------------------------------------
{
	long dirID;
	Boolean isDirectory;
	return (noErr == ::FSpGetDirectoryID(&mSpec, &dirID, &isDirectory) && !isDirectory);
} // nsFileSpec::IsFile

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsDirectory() const
//----------------------------------------------------------------------------------------
{
	long dirID;
	Boolean isDirectory;
	return (noErr == ::FSpGetDirectoryID(&mSpec, &dirID, &isDirectory) && isDirectory);
} // nsFileSpec::IsDirectory

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsHidden() const
//----------------------------------------------------------------------------------------
{
	CInfoPBRec		cInfo;
	PRBool			hidden = PR_FALSE;

	if (noErr == GetCatInfo(cInfo))
		if (cInfo.hFileInfo.ioFlFndrInfo.fdFlags & kIsInvisible)
			hidden = PR_TRUE;
	
	return hidden;
} // nsFileSpec::IsHidden

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsSymlink() const
//----------------------------------------------------------------------------------------
{
	CInfoPBRec		cInfo;
	PRBool			hidden = PR_FALSE;

	if (noErr == GetCatInfo(cInfo))
		if (cInfo.hFileInfo.ioFlFndrInfo.fdFlags & kIsAlias)
			hidden = PR_TRUE;
	
	return hidden;
} // nsFileSpec::IsSymlink

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::ResolveSymlink(PRBool& wasAliased)
//----------------------------------------------------------------------------------------
{
	Boolean wasAliased2; // Type conversion Boolean <--> PRBool 
	OSErr err = MacFileHelpers::ResolveAliasFile(mSpec, wasAliased2); 
	if (wasAliased2) 
	{ 
		mError = NS_FILE_RESULT(err); 
		wasAliased = PR_TRUE; 
	} 
	else 
		wasAliased = PR_FALSE; 

	return mError;
} // nsFileSpec::ResolveSymlink

//----------------------------------------------------------------------------------------
void nsFileSpec::GetParent(nsFileSpec& outSpec) const
//----------------------------------------------------------------------------------------
{
	if (NS_SUCCEEDED(mError))
		outSpec.mError = NS_FILE_RESULT(::FSMakeFSSpec(mSpec.vRefNum, mSpec.parID, nsnull, outSpec));
} // nsFileSpec::GetParent

//----------------------------------------------------------------------------------------
void nsFileSpec::operator += (const char* inRelativePath)
//----------------------------------------------------------------------------------------
{
	NS_ASSERTION(inRelativePath, "Attempt to append relative path with null path");

	// Invalidate the path cache string, since we're changing ourselves.
	mPath.SetToEmpty();

	// if we are already bad, don't allow appendage
	if (NS_FAILED(Error()))
	{
	  NS_WARNING("trying to append to a bad nsFileSpec");
	  return;
	}
	
	// Find the dirID of the directory described by this spec
	long dirID;
	Boolean isDirectory;
	mError = NS_FILE_RESULT(::FSpGetDirectoryID(&mSpec, &dirID, &isDirectory));
	if (NS_FAILED(mError) || !isDirectory || !inRelativePath)
		return;
   // mSpec.vRefNum is already correct.
	mSpec.parID = dirID;

	// Next, determine if it is a UNIX or Mac style path. Now, Macintosh relative paths
	// are either leaf names (in which the distinction between unix and macintosh
	// relative paths disappears) or they start with a colon. If we find an initial colon,
	// then assume it's a macintosh path.
	// If it is a UNIX path (including just a leaf name), we will also look for ':' and
	// assert if we find one.
	if (*inRelativePath != ':')
	{
		// Looks like a UNIX path (including possibly just a leaf name)
		NS_ASSERTION(strchr(inRelativePath, ':') == nsnull, "Can not determine path type");
	   // Convert unix path (which is unencoded) to a spec
		mError = NS_FILE_RESULT(
			MacFileHelpers::FSSpecFromUnixPath(inRelativePath, mSpec, false, false, true, true));
	}
	else
	{
		// We must be a mac path!				 
		mError = NS_FILE_RESULT(MacFileHelpers::FSSpecFromPathname(inRelativePath, mSpec, true));
	}
	if (mError == NS_FILE_RESULT(fnfErr))
		mError = NS_OK;
	
} // nsFileSpec::operator +=

//----------------------------------------------------------------------------------------
void nsFileSpec::CreateDirectory(int /* unix mode */)
//----------------------------------------------------------------------------------------
{
	long ignoredDirID;
	OSErr	err = ::FSpDirCreate(&mSpec, smCurrentScript, &ignoredDirID);
	// it's OK if the dir already exists
	if (err != noErr && IsDirectory())
		err = noErr;
		
	mError = NS_FILE_RESULT(err);
	
} // nsFileSpec::CreateDirectory

//----------------------------------------------------------------------------------------
void nsFileSpec::Delete(PRBool inRecursive) const
//----------------------------------------------------------------------------------------
{
	OSErr anErr;

	nsresult& mutableError = const_cast<nsFileSpec*>(this)->mError;
	if (inRecursive)
	{
		// MoreFilesExtras
		anErr = ::DeleteDirectory(
					mSpec.vRefNum,
					mSpec.parID,
					const_cast<unsigned char*>(mSpec.name));
	}
	else
		anErr = ::FSpDelete(&mSpec);
	
	if (anErr == fnfErr) // deleting a file that doesn't exist isn't an error!
		anErr = noErr;
	
	mutableError = NS_FILE_RESULT(anErr);
   
} // nsFileSpec::Delete

//----------------------------------------------------------------------------------------
void nsFileSpec::RecursiveCopy(nsFileSpec newDir) const
//----------------------------------------------------------------------------------------
{
	if (IsDirectory())
	{
		if (!(newDir.Exists()))
		{
			newDir.CreateDirectory();
		}

		for (nsDirectoryIterator i(*this, PR_FALSE); i.Exists(); i++)
		{
			nsFileSpec& child = (nsFileSpec&)i;

			if (child.IsDirectory())
			{
				nsFileSpec tmpDirSpec(newDir);

				char *leafname = child.GetLeafName();
				tmpDirSpec += leafname;
				nsCRT::free(leafname);

				child.RecursiveCopy(tmpDirSpec);
			}
			else
			{
				child.RecursiveCopy(newDir);
			}
		}
	}
	else
	{
		nsFileSpec& filePath = (nsFileSpec&) *this;

		if (!(newDir.Exists()))
		{
			newDir.CreateDirectory();
		}

		filePath.CopyToDir(newDir);
	}
} // nsFileSpec::RecursiveCopy

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Truncate(PRInt32 aNewLength) const
//----------------------------------------------------------------------------------------
{
    short   refNum;
    OSErr   err;
        
    // First see if we have an internal error set
    if (NS_FAILED(mError))
        return mError;
        
    // Need to open the file to trunc
    if (::FSpOpenDF(&mSpec, fsWrPerm, &refNum) != noErr)
        return NS_FILE_FAILURE;

    err = ::SetEOF(refNum, aNewLength);
        
    // Close the file unless we got an error that it was already closed
    if (err != fnOpnErr)
        (void)::FSClose(refNum);
        
    if (err != noErr)
        return NS_FILE_FAILURE;
        
    return NS_OK;
} // nsFileSpec::Truncate

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Rename(const char* inNewName)
//----------------------------------------------------------------------------------------
{
	NS_ASSERTION(inNewName, "Attempt to rename with null new name");

	if (strchr(inNewName, '/'))
		return NS_FILE_FAILURE; // no relative paths here!
	
	Str255 pName;
	MacFileHelpers::PLstrcpy(pName, inNewName);
	if (::FSpRename(&mSpec, pName) != noErr)
		return NS_FILE_FAILURE;
	SetLeafName(inNewName);
	return NS_OK;
} // nsFileSpec::Rename

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::CopyToDir(const nsFileSpec& newParentDir) const
//----------------------------------------------------------------------------------------
{
	// We can only copy into a directory, and (for now) can not copy entire directories

	if (!newParentDir.IsDirectory() || (IsDirectory() ) )
		return NS_FILE_FAILURE;
	
	nsresult rv = NS_FILE_RESULT(::FSpFileCopy(&mSpec,
							&newParentDir.mSpec,
							const_cast<StringPtr>(GetLeafPName()),
							nsnull,
							0,
							true));

	return rv;

} // nsFileSpec::CopyToDir

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::MoveToDir(const nsFileSpec& newParentDir) 
//----------------------------------------------------------------------------------------
{
	// We can only move into a directory
	
	if (!newParentDir.IsDirectory())
		return NS_FILE_FAILURE;
 
	nsresult result = NS_FILE_RESULT(::FSpMoveRenameCompat(&mSpec,
									&newParentDir.mSpec,
									const_cast<StringPtr>(GetLeafPName())));

	if ( NS_SUCCEEDED(result) )
	{
		char* leafName = GetLeafName();
		*this = newParentDir + leafName;
		nsCRT::free(leafName);
	}
	return result;
} // nsFileSpec::MoveToDir

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Execute(const char* /*args - how can this be cross-platform?  problem! */ ) const
//----------------------------------------------------------------------------------------
{
	if (IsDirectory())
		return NS_FILE_FAILURE;

	LaunchParamBlockRec launchThis;
	launchThis.launchAppSpec = const_cast<FSSpec*>(&mSpec);
	launchThis.launchAppParameters = nsnull; // args;
	/* launch the thing */
	launchThis.launchBlockID	= extendedBlock;
	launchThis.launchEPBLength	= extendedBlockLen;
	launchThis.launchFileFlags	= nsnull;
	launchThis.launchControlFlags = launchContinue + launchNoFileFlags + launchUseMinimum;
	launchThis.launchControlFlags += launchDontSwitch;

	nsresult result = NS_FILE_RESULT(::LaunchApplication(&launchThis));
	return result;
  
} // nsFileSpec::Execute

//----------------------------------------------------------------------------------------
OSErr nsFileSpec::GetCatInfo(CInfoPBRec& outInfo) const
//----------------------------------------------------------------------------------------
{
	DirInfo	   *dipb=(DirInfo *)&outInfo;
	dipb->ioCompletion = nsnull;
	dipb->ioFDirIndex = 0; // use dirID and name
	dipb->ioVRefNum = mSpec.vRefNum;
	dipb->ioDrDirID = mSpec.parID;
	dipb->ioNamePtr = const_cast<nsFileSpec*>(this)->mSpec.name;
	return PBGetCatInfoSync(&outInfo);
} // nsFileSpec::GetCatInfo()

//----------------------------------------------------------------------------------------
OSErr nsFileSpec::SetFileTypeAndCreator(OSType type, OSType creator)
//----------------------------------------------------------------------------------------
{
	FInfo info;
	OSErr err = ::FSpGetFInfo(&mSpec, &info);
	if (err != noErr)
		return err;
	info.fdType = type;
	info.fdCreator = creator;
	err = ::FSpSetFInfo(&mSpec, &info);
	return err;
}

//----------------------------------------------------------------------------------------
OSErr nsFileSpec::GetFileTypeAndCreator(OSType* type, OSType* creator)
//----------------------------------------------------------------------------------------
{
	FInfo info;
	OSErr err = ::FSpGetFInfo(&mSpec, &info);
	if (err != noErr)
		return err;
	*type = info.fdType;
	*creator = info.fdCreator;	
	return noErr;
}

//----------------------------------------------------------------------------------------
PRInt64 nsFileSpec::GetDiskSpaceAvailable() const
//----------------------------------------------------------------------------------------
{
	PRInt64 space64Bits;

	LL_I2L(space64Bits , LONG_MAX);

	XVolumeParam	pb;
	pb.ioCompletion = nsnull;
	pb.ioVolIndex = 0;
	pb.ioNamePtr = nsnull;
	pb.ioVRefNum = mSpec.vRefNum;
	
	// PBXGetVolInfoSync works on HFS+ volumes too!
	OSErr err = ::PBXGetVolInfoSync(&pb);
	
	if (err == noErr)
	{
#ifdef HAVE_LONG_LONG
		space64Bits = pb.ioVFreeBytes;
#else
		const UnsignedWide& freeBytes = UInt64ToUnsignedWide(pb.ioVFreeBytes);
		space64Bits.lo = freeBytes.lo;
		space64Bits.hi = freeBytes.hi;
#endif
	}
		
	return space64Bits;
} // nsFileSpec::GetDiskSpace()

//----------------------------------------------------------------------------------------
const char* nsFileSpec::GetCString() const
// This is the only conversion to const char* that is provided, and it allows the
// path to be "passed" to NSPR file routines.  This practice is VERY EVIL and should only
// be used to support legacy code.	Using it guarantees bugs on Macintosh. The string is
// cached and freed by the nsFileSpec destructor, so do not delete (or free) it.
//----------------------------------------------------------------------------------------
{
	if (mPath.IsEmpty())
	{
		char* path = MacFileHelpers::PathNameFromFSSpec(mSpec);
		if (path != NULL) {
			const_cast<nsFileSpec*>(this)->mPath = path;	// operator =() copies the string!!!
			delete[] path;
		} else {
			const_cast<nsFileSpec*>(this)->mError = NS_ERROR_OUT_OF_MEMORY;
		}
	}
	return mPath;
}

#pragma mark -

//========================================================================================
//					  Macintosh nsFilePath implementation
//========================================================================================

//----------------------------------------------------------------------------------------
static void AssignFromPath(nsFilePath& ioPath, const char* inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
{
	NS_ASSERTION(inString, "AssignFromPath called with null inString");
	NS_ASSERTION(strstr(inString, kFileURLPrefix) != inString, "URL passed as path");

	FSSpec spec;
	spec.vRefNum = 0;
	spec.parID = 0;
	MacFileHelpers::FSSpecFromUnixPath(
		inString,
		spec,
		false,
		true, // resolve alias
		true,
		inCreateDirs);
	// Now we have a spec,
	// Invoke operator = (const nsFileSpec&) to do the rest.
	// Why didn't we just say mPath = inString to get the path? Well, we want it to be
	// canonical and absolute.
	ioPath = spec;
}

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const char* inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
{
	AssignFromPath(*this, inString, inCreateDirs);
} //nsFilePath::nsFilePath

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsString& inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
{
	AssignFromPath(*this, NS_LossyConvertUCS2toASCII(inString).get(),
	               inCreateDirs);
}

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
	AssignFromPath(*this, inString, PR_FALSE);
}

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	*this = inSpec;
}

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
	*this = inOther;
}

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	char * path = MacFileHelpers::PathNameFromFSSpec(inSpec);
	path = MacFileHelpers::EncodeMacPath(path, true, false);
	mPath = path;
	nsCRT::free(path);
	mFileSpec = inSpec;
} // nsFilePath::operator =

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFileURL& inOther)
//----------------------------------------------------------------------------------------
{
	char * path = MacFileHelpers::PathNameFromFSSpec(inOther.mFileSpec);
	path = MacFileHelpers::EncodeMacPath(path, true, false);
	mPath = path;
	nsCRT::free(path);
	mFileSpec = inOther.GetFileSpec();
}

#pragma mark -

//========================================================================================
//								  nsFileURL implementation
//========================================================================================

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const char* inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:	 mURL(inString)
{	 
	NS_ASSERTION(inString, "nsFileURL constructed with null inString");
	NS_ASSERTION(strstr(inString, kFileURLPrefix) == inString, "Not a URL!");
	mFileSpec.mError = NS_FILE_RESULT(MacFileHelpers::FSSpecFromUnixPath(
		inString + kFileURLPrefixLength,
		mFileSpec.mSpec,
		true, // need to decode
		false, // resolve alias
		false, // must be a full path
		inCreateDirs));
	if (mFileSpec.mError == NS_FILE_RESULT(fnfErr))
		mFileSpec.mError = NS_OK;
} // nsFileURL::nsFileURL

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsString& inString, PRBool inCreateDirs)
//----------------------------------------------------------------------------------------
:	 mURL(nsnull)
{
	NS_LossyConvertUCS2toASCII cstring(inString);
	mURL = cstring.get();
	NS_ASSERTION(strstr(cstring.get(), kFileURLPrefix) == cstring.get(),
	             "Not a URL!");
	mFileSpec.mError = NS_FILE_RESULT(MacFileHelpers::FSSpecFromUnixPath(
		cstring.get() + kFileURLPrefixLength,
		mFileSpec.mSpec,
		true, // need to decode
		false, // resolve alias
		false, // must be a full path
		inCreateDirs));
	if (mFileSpec.mError == NS_FILE_RESULT(fnfErr))
		mFileSpec.mError = NS_OK;
} // nsFileURL::nsFileURL

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
	*this = inOther.GetFileSpec();
} // nsFileURL::nsFileURL

//----------------------------------------------------------------------------------------
nsFileURL::nsFileURL(const nsFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
	*this = inOther;
} // nsFileURL::nsFileURL

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFilePath& inOther)
//----------------------------------------------------------------------------------------
{
	*this = inOther.GetFileSpec();
} // nsFileURL::operator =

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const nsFileSpec& inOther)
//----------------------------------------------------------------------------------------
{
	mFileSpec  = inOther;
	char* path = MacFileHelpers::PathNameFromFSSpec( mFileSpec );
	char* encodedPath = MacFileHelpers::EncodeMacPath(path, true, true);
	nsSimpleCharString encodedURL(kFileURLPrefix);
	encodedURL += encodedPath;
	nsCRT::free(encodedPath);
	mURL = encodedURL;
	if (encodedURL[encodedURL.Length() - 1] != '/' && inOther.IsDirectory())
		mURL += "/";
} // nsFileURL::operator =

//----------------------------------------------------------------------------------------
void nsFileURL::operator = (const char* inString)
//----------------------------------------------------------------------------------------
{
	NS_ASSERTION(inString, "nsFileURL operator= constructed with null inString");

	mURL = inString;
	NS_ASSERTION(strstr(inString, kFileURLPrefix) == inString, "Not a URL!");
	mFileSpec.mError = NS_FILE_RESULT(MacFileHelpers::FSSpecFromUnixPath(
		inString + kFileURLPrefixLength,
		mFileSpec.mSpec,
		true, // need to decode
		true, // resolve alias
		false, // must be a full path
		false)); // don't create dirs.
	if (mFileSpec.mError == NS_FILE_RESULT(fnfErr))
		mFileSpec.mError = NS_OK;
} // nsFileURL::operator =

#pragma mark -

//========================================================================================
//								  nsDirectoryIterator
//========================================================================================

//----------------------------------------------------------------------------------------
nsDirectoryIterator::nsDirectoryIterator(
	const nsFileSpec& inDirectory
,	PRBool resolveSymLinks)
//----------------------------------------------------------------------------------------
	: mCurrent(inDirectory)
	, mExists(false)
	, mIndex(-1)
	, mResoveSymLinks(resolveSymLinks)
{
	CInfoPBRec pb;
	OSErr err = inDirectory.GetCatInfo(pb);
	
	// test that we have got a directory back, not a file
	DirInfo* dipb = (DirInfo*)&pb;
	if (err != noErr  || !( dipb->ioFlAttrib & 0x0010))
		return;
	// Sorry about this, there seems to be a bug in CWPro 4:
	FSSpec& currentSpec = mCurrent.nsFileSpec::operator FSSpec&();
	mVRefNum = currentSpec.vRefNum;
	mParID = dipb->ioDrDirID;
	mMaxIndex = pb.dirInfo.ioDrNmFls;
	mIndex = 0; // ready to increment
	++(*this); // the pre-increment operator
	
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
OSErr nsDirectoryIterator::SetToIndex()
//----------------------------------------------------------------------------------------
{
	CInfoPBRec cipb;
	DirInfo	   *dipb=(DirInfo *)&cipb;
	Str255 objectName;
	dipb->ioCompletion = nsnull;
	dipb->ioFDirIndex = mIndex;
	// Sorry about this, there seems to be a bug in CWPro 4:
	FSSpec& currentSpec = mCurrent.nsFileSpec::operator FSSpec&();
	dipb->ioVRefNum = mVRefNum; /* Might need to use vRefNum, not sure*/
	dipb->ioDrDirID = mParID;
	dipb->ioNamePtr = objectName;
	OSErr err = PBGetCatInfoSync(&cipb);
	FSSpec temp;
	if (err == noErr)
		err = FSMakeFSSpec(mVRefNum, mParID, objectName, &temp);
	mCurrent = temp; // use the operator: it clears the string cache.
	mExists = err == noErr;

	if (mExists && mResoveSymLinks)
	{	
		PRBool ignore;
		mCurrent.ResolveSymlink(ignore);
	}
	return err;
} // nsDirectoryIterator::SetToIndex()

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator -- ()
//----------------------------------------------------------------------------------------
{
	mExists = false;
	while (--mIndex > 0)
	{
		OSErr err = SetToIndex();
		if (err == noErr)
			break;
	}
	return *this;
} // nsDirectoryIterator::operator --

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator ++ ()
//----------------------------------------------------------------------------------------
{
	mExists = false;
	if (mIndex >= 0) // probably trying to use a file as a directory!
		while (++mIndex <= mMaxIndex)
		{
			OSErr err = SetToIndex();
			if (err == noErr)
				break;
		}
	return *this;
} // nsDirectoryIterator::operator ++

#endif
