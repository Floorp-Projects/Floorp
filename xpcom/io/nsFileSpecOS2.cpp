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
 * Contributor(s): 
 *
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
      if( ioPath[2] == '\0')
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
}

void nsFilePath::operator = ( const nsFileSpec &inSpec)
{
   mPath = inSpec.mPath;
   nsFileSpecHelpers::NativeToUnix(mPath);
}

// nsFilePath constructor
nsFilePath::nsFilePath( const nsFileSpec &inSpec)
           : mPath(0)
{
   *this = inSpec;
}

// nsFileSpec implementation ------------------------------------------

nsFileSpec::nsFileSpec( const nsFilePath &inPath) : mPath(0)
{
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
   return 0 == stat( mPath, &st); 
}

// These stat tests are done somewhat verbosely 'cos the VACPP version
// of sys/stat.h is sadly lacking in #defines.

PRBool nsFileSpec::IsFile() const
{
   struct stat st;
   return (0 == stat( mPath, &st)) && (S_IFREG == (st.st_mode & S_IFREG));
}

PRBool nsFileSpec::IsDirectory() const
{
   struct stat st;
   return (0 == stat( mPath, &st)) && (S_IFDIR == (st.st_mode & S_IFDIR));
}

// Really should factor out DosQPI() call to an internal GetFS3() method and then use
// here, in IsDirectory(), IsFile(), GetModDate(), GetFileSize() [and a future IsReadOnly()]
// and lose the clumsy stat() calls.  Exists() too.

PRBool nsFileSpec::IsHidden() const
{
   FILESTATUS3 fs3;
   APIRET rc;
   PRBool bHidden = PR_FALSE; // XXX how do I return an error?
   rc = DosQueryPathInfo( mPath, FIL_STANDARD, &fs3, sizeof fs3);
   if( !rc)
      bHidden = fs3.attrFile & FILE_HIDDEN ? PR_TRUE : PR_FALSE;

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
   if( stat( mPath, &st) == 0) 
      outStamp = st.st_mtime; 
   else
      outStamp = 0;
}

PRUint32 nsFileSpec::GetFileSize() const
{
   struct stat st;
   PRUint32 size = 0;
   if( stat( mPath, &st) == 0) 
      size = (PRUint32) st.st_size;
   return size;
}

// Okay, this is a really weird place to put this method!
// And it ought to return a PRUint64.
//
PRUint32 nsFileSpec::GetDiskSpaceAvailable() const
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

   return cbAvail;
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

void nsFileSpec::CreateDirectory( int /*mode*/)
{
   // Note that mPath is canonical.
   // This means that all directories above us are meant to exist.
   PR_MkDir( mPath, PR_CREATE_FILE);
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
      PR_RmDir( mPath);
   }
   else
   {
      PR_Delete( mPath);
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

      char *aLocal = PL_strdup( mPath);
      SetLeafName( inNewName);
      APIRET ret = DosMove( aLocal, (const char*) mPath);
      if( NO_ERROR == ret)
      {
         delete [] aLocal;
         rc = NS_OK;
      }
      else
      {
#ifdef DEBUG
         printf( "DosMove %s %s returned %d\n", (char*)mPath, aLocal, ret);
#endif
         mPath = aLocal;
      }
   }

   return rc;
}

nsresult nsFileSpec::Copy( const nsFileSpec &inParentDirectory) const
{
   // Copy the file this filespec represents into the given directory.
   nsresult rc = NS_FILE_FAILURE;

   if( !IsDirectory() && inParentDirectory.IsDirectory())
   {
      char *myLeaf = GetLeafName();
      nsSimpleCharString copyTo( inParentDirectory.GetCString());
      copyTo += "\\";
      copyTo += myLeaf;
      delete [] myLeaf;

      APIRET ret = DosCopy( (const char*) mPath, (const char*) copyTo, DCPY_EXISTING);

      if( NO_ERROR == ret)
         rc = NS_OK;
#ifdef DEBUG
      else
         printf( "DosCopy %s %s returned %dl\n",
                 (const char*) mPath, (const char*) copyTo, ret);
#endif
   }

   return rc;
}

// XXX not sure about the semantics of this method...
nsresult nsFileSpec::Move( const nsFileSpec &aParentDirectory)
{
   // Copy first & then delete self to avoid drive-clashes
   nsresult rc = Copy( aParentDirectory);
   if( NS_SUCCEEDED(rc))
   {
      Delete( PR_FALSE); // XXX why no return code ?
      *this = aParentDirectory + GetLeafName();
   }

   return rc;
}

nsresult nsFileSpec::Execute( const char *inArgs) const
{
   // Running arbitrary programs in OS/2 seems soooo hard: getting a single
   // routine which deals correctly with PM, VIO, DOS & various Win 3.1
   // types would be so nice.
   //
   // The method here looks quite elegant, but makes me shiver with
   // unease:  we create a new program object for the program and its
   // parameters, get the shell to open it, and then destroy the object.
   //
   // I can't get either DosStartSession() or WinStartApp() to open
   // all types of thing, including varieties of Win3.1 programs.
   //
   nsresult rc = NS_FILE_FAILURE;

   char setupstring[ CCHMAXPATH * 2];
   sprintf( setupstring, "EXENAME=%s;PARAMETERS=%s;CCVIEW=NO",
            (const char *)mPath, inArgs);

   HOBJECT hObject = WinCreateObject( "WPProgram", "Title", setupstring,
                                      "<WP_NOWHERE>", CO_UPDATEIFEXISTS);
   if( 0 != hObject)
   {
      // Doing this twice gives focus to the opened object.
      // (no, this doesn't alter the object's settings - there's rather
      //  unexpected hackery in wpSetup)
      WinSetObjectData( hObject, "OPEN=DEFAULT");
      WinSetObjectData( hObject, "OPEN=DEFAULT");
      WinDestroyObject( hObject);
      rc = NS_OK;
   }

   return rc;
}

// nsDirectoryIterator ------------------------------------------------------

nsDirectoryIterator::nsDirectoryIterator(	const nsFileSpec &aDirectory,
                                            PRBool resolveSymlinks)
: mCurrent( aDirectory), 
  mDir( nsnull), 
  mExists(PR_FALSE), 
  mResoveSymLinks(resolveSymlinks)
{
   mDir = PR_OpenDir( aDirectory);
   mCurrent += "dummy";
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
