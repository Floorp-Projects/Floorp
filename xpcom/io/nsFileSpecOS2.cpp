/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1998 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): Henry Sobotka <sobotka@axess.com>
 *                 00/01/06: general review and update against Win/Unix versions;
 *                           replaced nsFileSpec::Execute implementation with system() call
 *                           which properly launches OS/2 PM|VIO, WinOS2 and DOS programs
 * 
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 03/23/2000       IBM Corp.      Fixed bug where 2 char or less profile names treated as drive letters.
 */

// This file is #include-d by nsFileSpec.cpp and contains OS/2 specific
// routines.
//
// Let me try & summarise what's going on here:
//
//    nsFileSpec:   "C:\foo\bar\b az.ext"
//
//    nsFilePath:   "/C|/foo/bar/b az.ext"
//   
//    nsFileURL:    "file:///C|/foo/bar/b%20az.ext"
//
// We don't share the Windoze code 'cos it delves into the Win32 api.
// When things stabilize, we should push to merge some of the duplicated stuff.
//

#define INCL_DOSERRORS
#define INCL_DOS
#define INCL_WINWORKPLACE
#include <os2.h>

#ifdef XP_OS2_VACPP
#include <fcntl.h> /* for O_RDWR */
#include <direct.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <ctype.h>
#include <io.h>

// General helper routines --------------------------------------------------

// Takes a "unix path" in the nsFilePath sense, full or relative, and returns
// the os/2 equivalent.
//
// OS/2 [==dos]-style paths are unaffected.
void nsFileSpecHelpers::UnixToNative( nsSimpleCharString &ioPath)
{
   if( !ioPath.IsEmpty())
   {
      // Skip any leading slash
      char *c = (char*) ioPath;
      if( '/' == *c)
      {
         nsSimpleCharString tmp = c + 1;
         ioPath = tmp;
      }

      // flip slashes
      for( c = (char*)ioPath; *c; c++)
         if( *c == '/') *c = '\\';
   
      // do the drive portion
      if( ioPath[1] == '|')
         ioPath[1] = ':';
   
      // yucky specialcase for drive roots: "H:" is not valid, has to be "H:\"
      if(( ioPath[2] == '\0') && (ioPath[1] == ':')  && (ioPath[0] != '\0'))
      {
         char dl = ioPath[0];
         ioPath = " :\\";
         ioPath[0] = dl;
      }
   }
}

// Take a FQPN (os/2-style) and return a "unix path" in the nsFilePath sense
void nsFileSpecHelpers::NativeToUnix( nsSimpleCharString &ioPath)
{
   if( !ioPath.IsEmpty())
   {
      // unix path is one character longer
      nsSimpleCharString result( "/");
      result += ioPath;
   
      // flip slashes
      for( char *c = result; *c; c++)
         if( *c == '\\') *c = '/';
   
      // colon to pipe
      result[2] = '|';

      ioPath = result;
   }
}

// Canonify, make absolute, and check whether directories exist.
// The path given is a NATIVE path.
void nsFileSpecHelpers::Canonify( nsSimpleCharString &ioPath, PRBool inMakeDirs)
{
   PRUint32 lenstr = ioPath.Length();
   if( lenstr)
   {
      char &lastchar = ioPath[lenstr - 1];

      // Strip off any trailing backslash UNLESS it's the backslash that
      // comes after "X:".  Note also that "\" is valid.  Sheesh.
      //
      if( lastchar == '\\' && (lenstr != 3 || ioPath[1] != ':') && lenstr != 1)
         lastchar = '\0';

      // Canonify path: makes absolute and does the right thing with ".." etc.
      char full_native[ CCHMAXPATH] = "";
      DosQueryPathInfo( (char*) ioPath, FIL_QUERYFULLNAME,
                        full_native, CCHMAXPATH);
   
      ioPath = full_native;
   
      // if required, make directories.
      if( inMakeDirs)
      {
         nsSimpleCharString unix_path( ioPath);
         nsFileSpecHelpers::NativeToUnix( unix_path);
         nsFileSpecHelpers::MakeAllDirectories( unix_path, 0700 /* hmm */);
      }
   }
}

// nsFileSpec <-> nsFilePath ------------------------------------------------

// We assume that input is valid.  Garbage in, garbage out.

void nsFileSpec::operator = ( const nsFilePath &inPath)
{
   mPath = (const char*) inPath;
   nsFileSpecHelpers::UnixToNative( mPath);
   mError = NS_OK;
}

void nsFilePath::operator = ( const nsFileSpec &inSpec)
{
   mPath = inSpec.mPath;
   nsFileSpecHelpers::NativeToUnix(mPath);
}

// nsFilePath constructor
nsFilePath::nsFilePath( const nsFileSpec &inSpec)
{
   *this = inSpec;
}

// nsFileSpec implementation ------------------------------------------

nsFileSpec::nsFileSpec( const nsFilePath &inPath)
{
// NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
   *this = inPath;
}

void nsFileSpec::SetLeafName( const char *inLeafName)
{
   mPath.LeafReplace( '\\', inLeafName);
}

char *nsFileSpec::GetLeafName() const
{
   return mPath.GetLeaf( '\\');
}

PRBool nsFileSpec::Exists() const
{
   struct stat st;
   return !mPath.IsEmpty() && 0 == stat(nsNSPRPath(*this), &st); 
}

// These stat tests are done somewhat verbosely 'cos the VACPP version
// of sys/stat.h is sadly lacking in #defines.

PRBool nsFileSpec::IsFile() const
{
   struct stat st;
   return (!mPath.IsEmpty() && 0 == stat(nsNSPRPath(*this), &st)) && (S_IFREG == (st.st_mode & S_IFREG));
}

PRBool nsFileSpec::IsDirectory() const
{
   struct stat st;
   return (!mPath.IsEmpty() && 0 == stat(nsNSPRPath(*this), &st)) && (S_IFDIR == (st.st_mode & S_IFDIR));
}

// Really should factor out DosQPI() call to an internal GetFS3() method and then use
// here, in IsDirectory(), IsFile(), GetModDate(), GetFileSize() [and a future IsReadOnly()]
// and lose the clumsy stat() calls.  Exists() too.

PRBool nsFileSpec::IsHidden() const
{
   FILESTATUS3 fs3;
   APIRET rc;
   PRBool bHidden = PR_FALSE;

   if (!mPath.IsEmpty()) {
     rc = DosQueryPathInfo( mPath, FIL_STANDARD, &fs3, sizeof fs3);
     if(!rc)
       bHidden = fs3.attrFile & FILE_HIDDEN ? PR_TRUE : PR_FALSE;
   }
   return bHidden; 
}

// On FAT or HPFS there's no such thing as a symlink; it's possible that JFS
// (new with Warp Server for e-business) does know what they are.  Someone
// with a recent toolkit should check it out, but this will be OK for now.
PRBool nsFileSpec::IsSymlink() const
{
    return PR_FALSE;
} 

nsresult nsFileSpec::ResolveSymlink(PRBool& wasAliased)
{
    return NS_OK;
} 

void nsFileSpec::GetModDate( TimeStamp& outStamp) const
{
   struct stat st;
   if(!mPath.IsEmpty() && stat(nsNSPRPath(*this), &st) == 0) 
      outStamp = st.st_mtime; 
   else
      outStamp = 0;
}

PRUint32 nsFileSpec::GetFileSize() const
{
   struct stat st;
   PRUint32 size = 0;
   if(!mPath.IsEmpty() && stat(nsNSPRPath(*this), &st) == 0) 
      size = (PRUint32) st.st_size;
   return size;
}

// Okay, this is a really weird place to put this method!
// And it ought to return a PRInt64.
//
PRInt64 nsFileSpec::GetDiskSpaceAvailable() const
{
   ULONG      ulDriveNo = toupper(mPath[0]) + 1 - 'A';
   FSALLOCATE fsAllocate = { 0 };
   APIRET     rc = NO_ERROR;
   PRUint32   cbAvail = UINT_MAX; // XXX copy windows...

   rc = DosQueryFSInfo( ulDriveNo, FSIL_ALLOC,
                        &fsAllocate, sizeof fsAllocate);

   if( NO_ERROR == rc)
   {
      // XXX check for overflows and do UINT_MAX if necessary
      cbAvail = fsAllocate.cUnitAvail  *
                fsAllocate.cSectorUnit *
                fsAllocate.cbSector;
   }
   
   PRInt64 space64;

   LL_I2L(space64 , cbAvail);

   return space64;
}

void nsFileSpec::GetParent( nsFileSpec &outSpec) const
{
   outSpec.mPath = mPath;
   char *slash = strrchr( (char *) outSpec.mPath, '\\'); // XXX wrong for dbcs
   if( slash)
      *slash = '\0';
}

void nsFileSpec::operator += ( const char *inRelativePath)
{
   if( !inRelativePath || mPath.IsEmpty())
      return;

   if( mPath[mPath.Length() - 1] == '\\')
      mPath += "x";
   else
      mPath +="\\x";
	
   // If it's a (unix) relative path, make it native
   nsSimpleCharString relPath = inRelativePath;
   nsFileSpecHelpers::UnixToNative( relPath);
   SetLeafName( relPath);
}

void nsFileSpec::CreateDirectory( int mode)
{
   // Note that mPath is canonical.
   // This means that all directories above us are meant to exist.
  // mode is ignored
  if (!mPath.IsEmpty())
    PR_MkDir(nsNSPRPath(*this), PR_CREATE_FILE);
}

void nsFileSpec::Delete( PRBool inRecursive) const
{
   // Un-readonly ourselves
   chmod( mPath, S_IREAD | S_IWRITE);
   if( IsDirectory())
   {
      if( inRecursive)
      {
         for( nsDirectoryIterator i(*this, PR_FALSE); i.Exists(); i++)
         {
            nsFileSpec &child = (nsFileSpec &) i;
            child.Delete( inRecursive);
         }
      }
      PR_RmDir(nsNSPRPath(*this));
   }
   else if (!mPath.IsEmpty())
   {
      remove(nsNSPRPath(*this));
   }
}

// XXX what format is this string?  Who knows!
nsresult nsFileSpec::Rename( const char *inNewName)
{
   nsresult rc = NS_FILE_FAILURE;

   if( inNewName == nsnull)
      rc = NS_ERROR_NULL_POINTER;
   else if( !strchr( inNewName, '/') && !strchr( inNewName, '\\')) // XXX DBCS
   {
      // PR_Rename just calls DosMove() on what you give it.

      char *oldPath = nsCRT::strdup(mPath);
      SetLeafName( inNewName);
      APIRET ret = DosMove( oldPath, (const char*) mPath);
      if( NO_ERROR == ret)
	rc = NS_OK;
      else
      {
#ifdef DEBUG
         printf( "DosMove %s %s returned %ld\n", (char*)mPath, oldPath, ret);
#endif
         mPath = oldPath;
      }
      nsCRT::free(oldPath);
   }
   return rc;
}

nsresult nsFileSpec::CopyToDir( const nsFileSpec &inParentDirectory) const
{
   // Copy the file this filespec represents into the given directory.
   nsresult rc = NS_FILE_FAILURE;

   if( !IsDirectory() && inParentDirectory.IsDirectory())
   {
      char *myLeaf = GetLeafName();
      nsSimpleCharString copyTo( inParentDirectory.GetCString());
      copyTo += "\\";
      copyTo += myLeaf;
      nsCRT::free(myLeaf);

      APIRET ret = DosCopy( (const char*) mPath, (const char*) copyTo, DCPY_EXISTING);

      if( NO_ERROR == ret)
         rc = NS_OK;
      else
	{
#ifdef DEBUG
	  printf( "DosCopy %s %s returned %ld\n",
		  (const char*) mPath, (const char*) copyTo, ret);
#endif
	  rc = NS_FILE_FAILURE;
	}
   }

   return rc;
}

// XXX not sure about the semantics of this method...
nsresult nsFileSpec::MoveToDir( const nsFileSpec &inNewParentDirectory)
{
   // Copy first & then delete self to avoid drive-clashes
   nsresult rc = CopyToDir( inNewParentDirectory);
   if( NS_SUCCEEDED(rc))
   {
      Delete( PR_FALSE); // XXX why no return code
      *this = inNewParentDirectory + GetLeafName();
   }
   return rc;
}

nsresult nsFileSpec::Execute(const char *inArgs) const
{
    nsresult result = NS_FILE_FAILURE;
    
    if (!mPath.IsEmpty() && !IsDirectory())
    {
       nsSimpleCharString fileNameWithArgs = mPath + " " + inArgs;
       result = NS_FILE_RESULT(system(fileNameWithArgs));
    } 
    return result;
}

// nsDirectoryIterator ------------------------------------------------------

nsDirectoryIterator::nsDirectoryIterator(	const nsFileSpec &aDirectory,
                                            PRBool resolveSymlinks)
: mCurrent( aDirectory), 
  mExists(PR_FALSE), 
  mResoveSymLinks(resolveSymlinks), 
  mStarting( aDirectory),
  mDir( nsnull)
{
   mDir = PR_OpenDir( aDirectory);
   mCurrent += "dummy";
   mStarting += "dummy";
   ++(*this);
}

nsDirectoryIterator::~nsDirectoryIterator()
{
   if( mDir)
      PR_CloseDir( mDir);
}

nsDirectoryIterator &nsDirectoryIterator::operator ++ ()
{
   mExists = PR_FALSE;
   if( !mDir)
      return *this;
   PRDirEntry *entry = PR_ReadDir( mDir, PR_SKIP_BOTH);
   if( entry)
   {
      mExists = PR_TRUE;
      mCurrent = mStarting;
      mCurrent.SetLeafName( entry->name);
      if (mResoveSymLinks)
      {   
          PRBool ignore;
          mCurrent.ResolveSymlink(ignore);
      }
   }
   return *this;
}

nsDirectoryIterator& nsDirectoryIterator::operator -- ()
{
   return ++(*this); // can't go backwards without much pain & suffering.
}

void nsFileSpec::RecursiveCopy(nsFileSpec newDir) const
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
}

nsresult nsFileSpec::Truncate(PRInt32 offset) const
{
    char* Path = nsCRT::strdup(mPath);
    int rv = 0;

#ifdef XP_OS2_VACPP
    int fh = open(Path, O_RDWR);

    if (fh != -1)
      rv = _chsize(fh, offset);
#else
    truncate(Path, offset);
#endif

    nsCRT::free(Path);

    if(!rv) 
        return NS_OK;
    else
        return NS_ERROR_FAILURE;
}
