/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 
//    This file is included by nsFileSpec.cpp, and includes the Unix-specific
//    implementations.

#include <sys/stat.h>
#include <sys/param.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#include <stdlib.h>
#include <limits.h>
#include "nsError.h"
#include "prio.h"   /* for PR_Rename */

#if defined(_SCO_DS)
#define _SVID3  /* for statvfs.h */
#endif

#ifdef HAVE_SYS_STATVFS_H
#include <sys/statvfs.h>
#endif

#ifdef HAVE_SYS_VFS_H
#include <sys/vfs.h>
#endif

#ifdef HAVE_SYS_STATFS_H
#include <sys/statfs.h>
#endif

#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#undef Free
#endif

#ifdef HAVE_STATVFS
#define STATFS	statvfs
#else
#define STATFS	statfs
#endif

#ifndef MAXPATHLEN
#define MAXPATHLEN	1024  /* Guessing this is okay.  Works for SCO. */
#endif
 
#if defined(__QNX__)
#include <unix.h>	/* for realpath */
#define f_bavail	f_bfree
extern "C" int truncate(const char *, off_t);
#endif

#if defined(SUNOS4)
extern "C" int statfs(char *, struct statfs *);
#endif

#if defined(OSF1)
extern "C" int statvfs(const char *, struct statvfs *);
#endif

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::Canonify(nsSimpleCharString& ioPath, PRBool inMakeDirs)
// Canonify, make absolute, and check whether directories exist
//----------------------------------------------------------------------------------------
{
    if (ioPath.IsEmpty())
        return;
    if (inMakeDirs)
    {
        const mode_t mode = 0755;
        nsFileSpecHelpers::MakeAllDirectories((const char*)ioPath, mode);
    }

    errno = 0;  // needed?

    if (ioPath[0] != '/')
    {
        // the ioPath that was passed in is relative.  We must cat it to the cwd.
        char buffer[MAXPATHLEN];

        (void) getcwd(buffer, MAXPATHLEN);

        strcat(buffer, "/");
        strcat(buffer, ioPath);

        ioPath = buffer;
    }
} // nsFileSpecHelpers::Canonify

//----------------------------------------------------------------------------------------
void nsFileSpec::SetLeafName(const char* inLeafName)
//----------------------------------------------------------------------------------------
{
    mPath.LeafReplace('/', inLeafName);
} // nsFileSpec::SetLeafName

//----------------------------------------------------------------------------------------
char* nsFileSpec::GetLeafName() const
//----------------------------------------------------------------------------------------
{
    return mPath.GetLeaf('/');
} // nsFileSpec::GetLeafName

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::Exists() const
//----------------------------------------------------------------------------------------
{
    struct stat st;
    return !mPath.IsEmpty() && 0 == stat(mPath, &st); 
} // nsFileSpec::Exists

//----------------------------------------------------------------------------------------
void nsFileSpec::GetModDate(TimeStamp& outStamp) const
//----------------------------------------------------------------------------------------
{
	struct stat st;
    if (!mPath.IsEmpty() && stat(mPath, &st) == 0) 
        outStamp = st.st_mtime; 
    else
        outStamp = 0;
} // nsFileSpec::GetModDate

//----------------------------------------------------------------------------------------
PRUint32 nsFileSpec::GetFileSize() const
//----------------------------------------------------------------------------------------
{
	struct stat st;
    if (!mPath.IsEmpty() && stat(mPath, &st) == 0) 
        return (PRUint32)st.st_size; 
    return 0;
} // nsFileSpec::GetFileSize

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsFile() const
//----------------------------------------------------------------------------------------
{
    struct stat st;
    return !mPath.IsEmpty() && stat(mPath, &st) == 0 && S_ISREG(st.st_mode); 
} // nsFileSpec::IsFile

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsDirectory() const
//----------------------------------------------------------------------------------------
{
    struct stat st;
    return !mPath.IsEmpty() && 0 == stat(mPath, &st) && S_ISDIR(st.st_mode); 
} // nsFileSpec::IsDirectory

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsHidden() const
//----------------------------------------------------------------------------------------
{
    PRBool hidden = PR_FALSE;
    char *leafname = GetLeafName();
    if (nsnull != leafname)
    {
	// rjc: don't return ".", "..", or any file/directory that begins with a "."

	/*        if ((!strcmp(leafname, ".")) || (!strcmp(leafname, "..")))	*/
	if (leafname[0] == '.')
        {
            hidden = PR_TRUE;
        }
        nsCRT::free(leafname);
    }
    return hidden;
} // nsFileSpec::IsHidden

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsSymlink() const
//----------------------------------------------------------------------------------------
{
    struct stat st;
    if (!mPath.IsEmpty() && stat(mPath, &st) == 0 && S_ISLNK(st.st_mode))
        return PR_TRUE;

    return PR_FALSE;
} // nsFileSpec::IsSymlink

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::ResolveSymlink(PRBool& wasAliased)
//----------------------------------------------------------------------------------------
{
    wasAliased = PR_FALSE;

    char resolvedPath[MAXPATHLEN];
    int charCount = readlink(mPath, (char*)&resolvedPath, MAXPATHLEN);
    if (0 < charCount)
    {
        if (MAXPATHLEN > charCount)
            resolvedPath[charCount] = '\0';
        
        wasAliased = PR_TRUE;

	/* if it's not an absolute path,
		replace the leaf with what got resolved */  
        if (resolvedPath[0] != '/') {
		SetLeafName(resolvedPath);
        }
        else {
        	mPath = (char*)&resolvedPath;
        }
	
	char* canonicalPath = realpath((const char *)mPath, resolvedPath);
	NS_ASSERTION(canonicalPath, "realpath failed");
	if (canonicalPath) {
		mPath = (char*)&resolvedPath;
	}
	else {
		return NS_ERROR_FAILURE;
	}
    }
    
    return NS_OK;
} // nsFileSpec::ResolveSymlink


//----------------------------------------------------------------------------------------
void nsFileSpec::GetParent(nsFileSpec& outSpec) const
//----------------------------------------------------------------------------------------
{
    outSpec.mPath = mPath;
	char* chars = (char*)outSpec.mPath;
	chars[outSpec.mPath.Length() - 1] = '\0'; // avoid trailing separator, if any
    char* cp = strrchr(chars, '/');
    if (cp++)
	    outSpec.mPath.SetLength(cp - chars); // truncate.
} // nsFileSpec::GetParent

//----------------------------------------------------------------------------------------
void nsFileSpec::operator += (const char* inRelativePath)
//----------------------------------------------------------------------------------------
{
	NS_ASSERTION(inRelativePath, "Attempt to do += with a null string");

    if (!inRelativePath || mPath.IsEmpty())
        return;
    
    char endChar = mPath[(int)(strlen(mPath) - 1)];
    if (endChar == '/')
        mPath += "x";
    else
        mPath += "/x";
    SetLeafName(inRelativePath);
} // nsFileSpec::operator +=

//----------------------------------------------------------------------------------------
void nsFileSpec::CreateDirectory(int mode)
//----------------------------------------------------------------------------------------
{
    // Note that mPath is canonical!
    if (mPath.IsEmpty())
        return;
    mkdir(mPath, mode);
} // nsFileSpec::CreateDirectory

//----------------------------------------------------------------------------------------
void nsFileSpec::Delete(PRBool inRecursive) const
// To check if this worked, call Exists() afterwards, see?
//----------------------------------------------------------------------------------------
{
    if (IsDirectory())
    {
        if (inRecursive)
        {
            for (nsDirectoryIterator i(*this, PR_FALSE); i.Exists(); i++)
            {
                nsFileSpec& child = (nsFileSpec&)i;
                child.Delete(inRecursive);
            }        
        }
        rmdir(mPath);
    }
    else if (!mPath.IsEmpty())
        remove(mPath);
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
    else if (!mPath.IsEmpty())
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
nsresult nsFileSpec::Truncate(PRInt32 offset) const
//----------------------------------------------------------------------------------------
{
    char* Path = nsCRT::strdup(mPath);

    int rv = truncate(Path, offset) ;

    nsCRT::free(Path) ;

    if(!rv) 
        return NS_OK ;
    else
        return NS_ERROR_FAILURE ;
} // nsFileSpec::Truncate

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Rename(const char* inNewName)
//----------------------------------------------------------------------------------------
{
	NS_ASSERTION(inNewName, "Attempt to Rename with a null string");

    // This function should not be used to move a file on disk. 
    if (mPath.IsEmpty() || strchr(inNewName, '/')) 
        return NS_FILE_FAILURE;

    char* oldPath = nsCRT::strdup(mPath);
    
    SetLeafName(inNewName); 

    if (PR_Rename(oldPath, mPath) != NS_OK)
    {
        // Could not rename, set back to the original.
        mPath = oldPath;
        return NS_FILE_FAILURE;
    }
    
    nsCRT::free(oldPath);

    return NS_OK;
} // nsFileSpec::Rename

//----------------------------------------------------------------------------------------
static int CrudeFileCopy(const char* in, const char* out)
//----------------------------------------------------------------------------------------
{
	struct stat in_stat;
	int stat_result = -1;

	char	buf [1024];
	FILE	*ifp, *ofp;
	int	rbytes, wbytes;

	if (!in || !out)
		return -1;

	stat_result = stat (in, &in_stat);

	ifp = fopen (in, "r");
	if (!ifp) 
	{
		return -1;
	}

	ofp = fopen (out, "w");
	if (!ofp)
	{
		fclose (ifp);
		return -1;
	}

	while ((rbytes = fread (buf, 1, sizeof(buf), ifp)) > 0)
	{
		while (rbytes > 0)
		{
			if ( (wbytes = fwrite (buf, 1, rbytes, ofp)) < 0 )
			{
				fclose (ofp);
				fclose (ifp);
				unlink(out);
				return -1;
			}
			rbytes -= wbytes;
		}
	}
	fclose (ofp);
	fclose (ifp);

	if (stat_result == 0)
		chmod (out, in_stat.st_mode & 0777);

	return 0;
} // nsFileSpec::Rename

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::CopyToDir(const nsFileSpec& inParentDirectory) const
//----------------------------------------------------------------------------------------
{
    // We can only copy into a directory, and (for now) can not copy entire directories
    nsresult result = NS_FILE_FAILURE;

    if (inParentDirectory.IsDirectory() && (! IsDirectory() ) )
    {
        char *leafname = GetLeafName();
        nsSimpleCharString destPath(inParentDirectory.GetCString());
        destPath += "/";
        destPath += leafname;
        nsCRT::free(leafname);
        result = NS_FILE_RESULT(CrudeFileCopy(GetCString(), destPath));
    }
    return result;
} // nsFileSpec::CopyToDir

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::MoveToDir(const nsFileSpec& inNewParentDirectory)
//----------------------------------------------------------------------------------------
{
    // We can only copy into a directory, and (for now) can not copy entire directories
    nsresult result = NS_FILE_FAILURE;

    if (inNewParentDirectory.IsDirectory() && !IsDirectory())
    {
        char *leafname = GetLeafName();
        nsSimpleCharString destPath(inNewParentDirectory.GetCString());
        destPath += "/";
        destPath += leafname;
        nsCRT::free(leafname);

        result = NS_FILE_RESULT(CrudeFileCopy(GetCString(), (const char*)destPath));
        if (result == NS_OK)
        {
            // cast to fix const-ness
		    ((nsFileSpec*)this)->Delete(PR_FALSE);
        
            *this = inNewParentDirectory + GetLeafName(); 
    	}
    }
    return result;
} 

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Execute(const char* inArgs ) const
//----------------------------------------------------------------------------------------
{
    nsresult result = NS_FILE_FAILURE;
    
    if (!mPath.IsEmpty() && !IsDirectory())
    {
        nsSimpleCharString fileNameWithArgs = mPath + " " + inArgs;
        result = NS_FILE_RESULT(system(fileNameWithArgs));
    } 

    return result;

} // nsFileSpec::Execute

//----------------------------------------------------------------------------------------
PRInt64 nsFileSpec::GetDiskSpaceAvailable() const
//----------------------------------------------------------------------------------------
{
    PRInt64 bytes; /* XXX dougt needs to fix this */
    LL_I2L(bytes , LONG_MAX); // initialize to all the space in the world?


#if defined(HAVE_SYS_STATFS_H) || defined(HAVE_SYS_STATVFS_H)

    char curdir [MAXPATHLEN];
    if (mPath.IsEmpty())
    {
        (void) getcwd(curdir, MAXPATHLEN);
        if (!curdir)
            return bytes;  /* hope for the best as we did in cheddar */
    }
    else
        sprintf(curdir, "%.200s", (const char*)mPath);
 
    struct STATFS fs_buf;
#if defined(__QNX__) && !defined(HAVE_STATVFS) /* Maybe this should be handled differently? */
    if (STATFS(curdir, &fs_buf, 0, 0) < 0)
#else
    if (STATFS(curdir, &fs_buf) < 0)
#endif
        return bytes; /* hope for the best as we did in cheddar */
 
#ifdef DEBUG_DISK_SPACE
    printf("DiskSpaceAvailable: %d bytes\n", 
       fs_buf.f_bsize * (fs_buf.f_bavail - 1));
#endif

    PRInt64 bsize,bavail;
    LL_I2L( bsize,  fs_buf.f_bsize );
    LL_I2L( bavail, fs_buf.f_bavail - 1 );
    LL_MUL( bytes, bsize, bavail );
    return bytes;

#else 
    /*
    ** This platform doesn't have statfs or statvfs, so we don't have much
    ** choice but to "hope for the best as we did in cheddar".
    */
    return bytes;
#endif /* HAVE_SYS_STATFS_H or HAVE_SYS_STATVFS_H */

} // nsFileSpec::GetDiskSpace()

//========================================================================================
//                                nsDirectoryIterator
//========================================================================================

//----------------------------------------------------------------------------------------
nsDirectoryIterator::nsDirectoryIterator(const nsFileSpec& inDirectory, PRBool resolveSymLinks)
//----------------------------------------------------------------------------------------
    : mCurrent(inDirectory)
    , mExists(PR_FALSE)
    , mResoveSymLinks(resolveSymLinks)
    , mStarting(inDirectory)
    , mDir(nsnull)

{
    mStarting += "sysygy"; // save off the starting directory 
    mCurrent += "sysygy"; // prepare the path for SetLeafName
    mDir = opendir((const char*)nsFilePath(inDirectory));
    ++(*this);
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
nsDirectoryIterator::~nsDirectoryIterator()
//----------------------------------------------------------------------------------------
{
    if (mDir)
        closedir(mDir);
} // nsDirectoryIterator::nsDirectoryIterator

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator ++ ()
//----------------------------------------------------------------------------------------
{
    mExists = PR_FALSE;
    if (!mDir)
        return *this;
    char* dot    = ".";
    char* dotdot = "..";
    struct dirent* entry = readdir(mDir);
    if (entry && strcmp(entry->d_name, dot) == 0)
        entry = readdir(mDir);
    if (entry && strcmp(entry->d_name, dotdot) == 0)
        entry = readdir(mDir);
    if (entry)
    {
        mExists = PR_TRUE;
	mCurrent = mStarting;	// restore mCurrent to be the starting directory.  ResolveSymlink() may have taken us to another directory
        mCurrent.SetLeafName(entry->d_name);
        if (mResoveSymLinks)
        {   
            PRBool ignore;
            mCurrent.ResolveSymlink(ignore);
        }
    }
    return *this;
} // nsDirectoryIterator::operator ++

//----------------------------------------------------------------------------------------
nsDirectoryIterator& nsDirectoryIterator::operator -- ()
//----------------------------------------------------------------------------------------
{
    return ++(*this); // can't do it backwards.
} // nsDirectoryIterator::operator --
