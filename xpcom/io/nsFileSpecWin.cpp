/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
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
 
//	This file is included by nsFileSpec.cp, and includes the Windows-specific
//	implementations.

#include <sys/stat.h>
#include <direct.h>
#include <limits.h>
#include <stdlib.h>
#include "prio.h"
#include "nsError.h"

#include "windows.h"

#ifdef UNICODE
#define CreateDirectoryW  CreateDirectory
#else
#define CreateDirectoryA  CreateDirectory
#endif 

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::Canonify(nsSimpleCharString& ioPath, PRBool inMakeDirs)
// Canonify, make absolute, and check whether directories exist. This
// takes a (possibly relative) native path and converts it into a
// fully qualified native path.
//----------------------------------------------------------------------------------------
{
    if (ioPath.IsEmpty())
        return;
  
    if (inMakeDirs)
    {
        const int mode = 0700;
        nsSimpleCharString unixStylePath = ioPath;
        nsFileSpecHelpers::NativeToUnix(unixStylePath);
        nsFileSpecHelpers::MakeAllDirectories((const char*)unixStylePath, mode);
    }
    char buffer[_MAX_PATH];
    errno = 0;
    *buffer = '\0';
    char* canonicalPath = _fullpath(buffer, ioPath, _MAX_PATH);

    NS_ASSERTION( canonicalPath[0] != '\0', "Uh oh...couldn't convert" );
    if (canonicalPath[0] == '\0')
        return;

    ioPath = canonicalPath;
} // nsFileSpecHelpers::Canonify

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::UnixToNative(nsSimpleCharString& ioPath)
// This just does string manipulation.  It doesn't check reality, or canonify, or
// anything
//----------------------------------------------------------------------------------------
{
	// Allow for relative or absolute.  We can do this in place, because the
	// native path is never longer.
	
	if (ioPath.IsEmpty())
		return;
		
    // Strip initial slash for an absolute path
	char* src = (char*)ioPath;
	if (*src == '/')
	{
		// Since it was an absolute path, check for the drive letter
		char* colonPointer = src + 2;
		if (strstr(src, "|/") == colonPointer)
	        *colonPointer = ':';
	    // allocate new string by copying from ioPath[1]
	    nsSimpleCharString temp = src + 1;
	    ioPath = temp;
	}

	src = (char*)ioPath;
		
	// Convert '/' to '\'.
	while (*++src)
    {
        if (*src == '/')
            *src = '\\';
    }
} // nsFileSpecHelpers::UnixToNative

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::NativeToUnix(nsSimpleCharString& ioPath)
// This just does string manipulation.  It doesn't check reality, or canonify, or
// anything.  The unix path is longer, so we can't do it in place.
//----------------------------------------------------------------------------------------
{
	if (ioPath.IsEmpty())
		return;
		
	// Convert the drive-letter separator, if present
	nsSimpleCharString temp("/");

	char* cp = (char*)ioPath + 1;
	if (strstr(cp, ":\\") == cp)
		*cp = '|';    // absolute path
    else
        temp[0] = '\0'; // relative path
	
	// Convert '\' to '/'
	for (; *cp; cp++)
    {
      if (*cp == '\\')
        *cp = '/';
    }
	// Add the slash in front.
	temp += ioPath;
	ioPath = temp;
}

//----------------------------------------------------------------------------------------
nsFileSpec::nsFileSpec(const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
	*this = inPath;
}

//----------------------------------------------------------------------------------------
void nsFileSpec::operator = (const nsFilePath& inPath)
//----------------------------------------------------------------------------------------
{
	mPath = (const char*)inPath;
	nsFileSpecHelpers::UnixToNative(mPath);
	mError = NS_OK;
} // nsFileSpec::operator =

//----------------------------------------------------------------------------------------
nsFilePath::nsFilePath(const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	*this = inSpec;
} // nsFilePath::nsFilePath

//----------------------------------------------------------------------------------------
void nsFilePath::operator = (const nsFileSpec& inSpec)
//----------------------------------------------------------------------------------------
{
	mPath = inSpec.mPath;
	nsFileSpecHelpers::NativeToUnix(mPath);
} // nsFilePath::operator =

//----------------------------------------------------------------------------------------
void nsFileSpec::SetLeafName(const char* inLeafName)
//----------------------------------------------------------------------------------------
{
	mPath.LeafReplace('\\', inLeafName);
} // nsFileSpec::SetLeafName

//----------------------------------------------------------------------------------------
char* nsFileSpec::GetLeafName() const
//----------------------------------------------------------------------------------------
{
    return mPath.GetLeaf('\\');
} // nsFileSpec::GetLeafName

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::Exists() const
//----------------------------------------------------------------------------------------
{
	struct stat st;
	return 0 == stat(nsprPath(*this), &st); 
} // nsFileSpec::Exists

//----------------------------------------------------------------------------------------
void nsFileSpec::GetModDate(TimeStamp& outStamp) const
//----------------------------------------------------------------------------------------
{
	struct stat st;
    if (stat(nsprPath(*this), &st) == 0) 
        outStamp = st.st_mtime; 
    else
        outStamp = 0;
} // nsFileSpec::GetModDate

//----------------------------------------------------------------------------------------
PRUint32 nsFileSpec::GetFileSize() const
//----------------------------------------------------------------------------------------
{
	struct stat st;
    if (stat(nsprPath(*this), &st) == 0) 
        return (PRUint32)st.st_size; 
    return 0;
} // nsFileSpec::GetFileSize

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsFile() const
//----------------------------------------------------------------------------------------
{
  struct stat st;
  return 0 == stat(nsprPath(*this), &st) && (_S_IFREG & st.st_mode); 
} // nsFileSpec::IsFile

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsDirectory() const
//----------------------------------------------------------------------------------------
{
	struct stat st;
	return 0 == stat(nsprPath(*this), &st) && (_S_IFDIR & st.st_mode); 
} // nsFileSpec::IsDirectory

//----------------------------------------------------------------------------------------
void nsFileSpec::GetParent(nsFileSpec& outSpec) const
//----------------------------------------------------------------------------------------
{
	outSpec.mPath = mPath;
	char* cp = strrchr(outSpec.mPath, '\\');
	if (cp++)
		*cp = '\0';
} // nsFileSpec::GetParent

//----------------------------------------------------------------------------------------
void nsFileSpec::operator += (const char* inRelativePath)
//----------------------------------------------------------------------------------------
{
	if (!inRelativePath || mPath.IsEmpty())
		return;
	
	if (mPath[mPath.Length() - 1] == '\\')
		mPath += "x";
	else
		mPath += "\\x";
	
	// If it's a (unix) relative path, make it native
	nsSimpleCharString dosPath = inRelativePath;
	nsFileSpecHelpers::UnixToNative(dosPath);
	SetLeafName(dosPath);
} // nsFileSpec::operator +=

//----------------------------------------------------------------------------------------
void nsFileSpec::CreateDirectory(int /*mode*/)
//----------------------------------------------------------------------------------------
{
	// Note that mPath is canonical!
	mkdir(nsprPath(*this));
} // nsFileSpec::CreateDirectory

//----------------------------------------------------------------------------------------
void nsFileSpec::Delete(PRBool inRecursive) const
//----------------------------------------------------------------------------------------
{
    if (IsDirectory())
    {
	    if (inRecursive)
        {
            for (nsDirectoryIterator i(*this); i.Exists(); i++)
                {
                    nsFileSpec& child = (nsFileSpec&)i;
                    child.Delete(inRecursive);
                }		
        }
	    rmdir(nsprPath(*this));
    }
	else
    {
        remove(nsprPath(*this));
    }
} // nsFileSpec::Delete


//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Rename(const char* inNewName)
//----------------------------------------------------------------------------------------
{
    // This function should not be used to move a file on disk. 
    if (strchr(inNewName, '/')) 
        return NS_FILE_FAILURE;

    char* oldPath = PL_strdup(mPath);
    
    SetLeafName(inNewName);        

    if (PR_Rename(oldPath, mPath) != NS_OK)
    {
        // Could not rename, set back to the original.
        mPath = oldPath;
        return NS_FILE_FAILURE;
    }
    
    delete [] oldPath;
    
    return NS_OK;
} // nsFileSpec::Rename

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Copy(const nsFileSpec& inParentDirectory) const
//----------------------------------------------------------------------------------------
{
    // We can only copy into a directory, and (for now) can not copy entire directories
    if (inParentDirectory.IsDirectory() && (! IsDirectory() ) )
    {
        char *leafname = GetLeafName();
        nsSimpleCharString destPath(inParentDirectory.GetCString());
        destPath += "\\";
        destPath += leafname;
        delete [] leafname;
        
        // CopyFile returns non-zero if succeeds
        int copyOK = CopyFile(GetCString(), destPath, true);
        if (copyOK)
            return NS_OK;
    }
    return NS_FILE_FAILURE;
} // nsFileSpec::Copy

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Move(const nsFileSpec& inNewParentDirectory)
//----------------------------------------------------------------------------------------
{
    // We can only copy into a directory, and (for now) can not copy entire directories
    if (inNewParentDirectory.IsDirectory() && (! IsDirectory() ) )
    {
        char *leafname = GetLeafName();
        nsSimpleCharString destPath(inNewParentDirectory.GetCString());
        destPath += "\\";
        destPath += leafname;
        delete [] leafname;

        // MoveFile returns non-zero if succeeds
        int copyOK = MoveFile(GetCString(), destPath);

        if (copyOK)
        {
            *this = inNewParentDirectory + GetLeafName(); 
            return NS_OK;
        }
        
        delete [] destPath;
    }
    return NS_FILE_FAILURE;
} // nsFileSpec::Move

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Execute(const char* inArgs ) const
//----------------------------------------------------------------------------------------
{    
    if (!IsDirectory())
    {
	    nsSimpleCharString fileNameWithArgs = mPath + " " + inArgs;
        int execResult = WinExec( fileNameWithArgs, SW_NORMAL );     
        if (execResult > 31)
            return NS_OK;
    }
    return NS_FILE_FAILURE;
} // nsFileSpec::Execute

//----------------------------------------------------------------------------------------
PRUint32 nsFileSpec::GetDiskSpaceAvailable() const
//----------------------------------------------------------------------------------------
{
	char aDrive[_MAX_DRIVE + 2];
	_splitpath( (const char*)mPath, aDrive, NULL, NULL, NULL);

	if (aDrive[0] == '\0')
	{
        // The back end is always trying to pass us paths that look
        //   like /c|/netscape/mail.  See if we've got one of them
        if (mPath.Length() > 2 && mPath[0] == '/' && mPath[2] == '|')
        {
            aDrive[0] = mPath[1];
            aDrive[1] = ':';
            aDrive[2] = '\0';
        }
        else
        {
            // Return bogus large number and hope for the best
            return ULONG_MAX; 
        }
    }

	strcat(aDrive, "\\");

	DWORD dwSectorsPerCluster = 0;
	DWORD dwBytesPerSector = 0;
	DWORD dwFreeClusters = 0;
	DWORD dwTotalClusters = 0;
	if (!GetDiskFreeSpace(aDrive, 
						&dwSectorsPerCluster, 
						&dwBytesPerSector, 
						&dwFreeClusters, 
						&dwTotalClusters))
	{
		return ULONG_MAX; // Return bogus large number and hope for the best
	}

	//	We can now figure free disk space.
	return dwFreeClusters * dwSectorsPerCluster * dwBytesPerSector;
} // nsFileSpec::GetDiskSpaceAvailable()

//========================================================================================
//								nsDirectoryIterator
//========================================================================================

//----------------------------------------------------------------------------------------
nsDirectoryIterator::nsDirectoryIterator(
	const nsFileSpec& inDirectory
,	int inIterateDirection)
//----------------------------------------------------------------------------------------
	: mCurrent(inDirectory)
	, mDir(nsnull)
	, mExists(PR_FALSE)
{
    mDir = PR_OpenDir(inDirectory);
	mCurrent += "dummy";
    ++(*this);
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
nsDirectoryIterator::~nsDirectoryIterator()
//----------------------------------------------------------------------------------------
{
    if (mDir)
	    PR_CloseDir(mDir);
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator ++ ()
//----------------------------------------------------------------------------------------
{
	mExists = PR_FALSE;
	if (!mDir)
		return *this;
    PRDirEntry* entry = PR_ReadDir(mDir, PR_SKIP_BOTH); // Ignore '.' && '..'
	if (entry)
    {
      mExists = PR_TRUE;
      mCurrent.SetLeafName(entry->name);
    }
	return *this;
} // nsDirectoryIterator::operator ++

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator -- ()
//----------------------------------------------------------------------------------------
{
	return ++(*this); // can't do it backwards.
} // nsDirectoryIterator::operator --

