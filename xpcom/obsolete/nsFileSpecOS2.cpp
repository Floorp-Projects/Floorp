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
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is
 * John Fairhurst, <john_fairhurst@iname.com>.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henry Sobotka <sobotka@axess.com>
 *   00/01/06: general review and update against Win/Unix versions;
 *   replaced nsFileSpec::Execute implementation with system() call
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
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
 * 06/20/2000       IBM Corp.      Make it more like Windows version.
 */

#define INCL_DOSERRORS
#define INCL_DOS
#define INCL_WINWORKPLACE
#include <os2.h>

#ifdef XP_OS2_VACPP
#include <direct.h>
#endif

#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <ctype.h>
#include <io.h>

//----------------------------------------------------------------------------------------
void nsFileSpecHelpers::Canonify(nsSimpleCharString& ioPath, PRBool inMakeDirs)
// Canonify, make absolute, and check whether directories exist. This
// takes a (possibly relative) native path and converts it into a
// fully qualified native path.
//----------------------------------------------------------------------------------------
{
    if (ioPath.IsEmpty())
        return;
  
    NS_ASSERTION(strchr((const char*)ioPath, '/') == 0,
		"This smells like a Unix path. Native path expected! "
		"Please fix.");
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
#ifdef XP_OS2
    PRBool removedBackslash = PR_FALSE;
    PRUint32 lenstr = ioPath.Length();
    char &lastchar = ioPath[lenstr -1];

    // Strip off any trailing backslash UNLESS it's the backslash that
    // comes after "X:".  Note also that "\" is valid.  Sheesh.
    //
    if( lastchar == '\\' && (lenstr != 3 || ioPath[1] != ':') && lenstr != 1)
    {
       lastchar = '\0';
       removedBackslash = PR_TRUE;
    }

    char canonicalPath[CCHMAXPATH] = "";

    DosQueryPathInfo( (char*) ioPath, 
                                          FIL_QUERYFULLNAME,
                                          canonicalPath, 
                                          CCHMAXPATH);
#else
    char* canonicalPath = _fullpath(buffer, ioPath, _MAX_PATH);
#endif

	if (canonicalPath)
	{
		NS_ASSERTION( canonicalPath[0] != '\0', "Uh oh...couldn't convert" );
		if (canonicalPath[0] == '\0')
			return;
#ifdef XP_OS2
                // If we removed that backslash, add it back onto the fullpath
                if (removedBackslash)
                   strcat( canonicalPath, "\\");
#endif
	}
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
  if (*src == '/') {
    if (!src[1]) {
      // allocate new string by copying from ioPath[1]
      nsSimpleCharString temp = src + 1;
      ioPath = temp;
      return;
    }
	  // Since it was an absolute path, check for the drive letter
		char* colonPointer = src + 2;
		if (strstr(src, "|/") == colonPointer)
	    *colonPointer = ':';
	  // allocate new string by copying from ioPath[1]
	  nsSimpleCharString temp = src + 1;
	  ioPath = temp;
	}

	src = (char*)ioPath;
		
    if (*src) {
	    // Convert '/' to '\'.
	    while (*++src)
        {
            if (*src == '/')
                *src = '\\';
        }
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
#ifdef XP_OS2
      // OS2TODO - implement equivalent of IsDBCSLeadByte
#else
      if(IsDBCSLeadByte(*cp) && *(cp+1) != nsnull)
      {
         cp++;
         continue;
      }
#endif
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
//    NS_ASSERTION(0, "nsFileSpec is unsupported - use nsIFile!");
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
	NS_ASSERTION(inLeafName, "Attempt to SetLeafName with a null string");
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
	return !mPath.IsEmpty() && 0 == stat(nsNSPRPath(*this), &st); 
} // nsFileSpec::Exists

//----------------------------------------------------------------------------------------
void nsFileSpec::GetModDate(TimeStamp& outStamp) const
//----------------------------------------------------------------------------------------
{
	struct stat st;
    if (!mPath.IsEmpty() && stat(nsNSPRPath(*this), &st) == 0) 
        outStamp = st.st_mtime; 
    else
        outStamp = 0;
} // nsFileSpec::GetModDate

//----------------------------------------------------------------------------------------
PRUint32 nsFileSpec::GetFileSize() const
//----------------------------------------------------------------------------------------
{
	struct stat st;
    if (!mPath.IsEmpty() && stat(nsNSPRPath(*this), &st) == 0) 
        return (PRUint32)st.st_size; 
    return 0;
} // nsFileSpec::GetFileSize

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsFile() const
//----------------------------------------------------------------------------------------
{
  struct stat st;
#ifdef XP_OS2
  return !mPath.IsEmpty() && 0 == stat(nsNSPRPath(*this), &st) && (  S_IFREG & st.st_mode);
#else
  return !mPath.IsEmpty() && 0 == stat(nsNSPRPath(*this), &st) && (_S_IFREG & st.st_mode);
#endif
} // nsFileSpec::IsFile

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsDirectory() const
//----------------------------------------------------------------------------------------
{
	struct stat st;
#ifdef XP_OS2
        return !mPath.IsEmpty() && 0 == stat(nsNSPRPath(*this), &st) && (  S_IFDIR & st.st_mode);
#else
	return !mPath.IsEmpty() && 0 == stat(nsNSPRPath(*this), &st) && (_S_IFDIR & st.st_mode);
#endif
} // nsFileSpec::IsDirectory

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsHidden() const
//----------------------------------------------------------------------------------------
{
    PRBool hidden = PR_FALSE;
    if (!mPath.IsEmpty())
    {
#ifdef XP_OS2
        FILESTATUS3 fs3;
        APIRET rc;

        rc = DosQueryPathInfo( mPath,
                                         FIL_STANDARD, 
                                         &fs3,
                                         sizeof fs3);
        if(!rc)
          hidden = fs3.attrFile & FILE_HIDDEN ? PR_TRUE : PR_FALSE;
#else
        DWORD attr = GetFileAttributes(mPath);
        if (FILE_ATTRIBUTE_HIDDEN & attr)
            hidden = PR_TRUE;
#endif
    }
    return hidden;
}
// nsFileSpec::IsHidden

//----------------------------------------------------------------------------------------
PRBool nsFileSpec::IsSymlink() const
//----------------------------------------------------------------------------------------
{
#ifdef XP_OS2
    return PR_FALSE;                // No symlinks on OS/2
#else
    HRESULT hres; 
    IShellLink* psl; 
    
    PRBool isSymlink = PR_FALSE;
    
    CoInitialize(NULL);
    // Get a pointer to the IShellLink interface. 
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl); 
    if (SUCCEEDED(hres)) 
    { 
        IPersistFile* ppf; 
        
        // Get a pointer to the IPersistFile interface. 
        hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf); 
        
        if (SUCCEEDED(hres)) 
        {
            WORD wsz[MAX_PATH]; 
            // Ensure that the string is Unicode. 
            MultiByteToWideChar(CP_ACP, 0, mPath, -1, wsz, MAX_PATH); 
 
            // Load the shortcut. 
            hres = ppf->Load(wsz, STGM_READ); 
            if (SUCCEEDED(hres)) 
            {
                isSymlink = PR_TRUE;
            }
            
            // Release the pointer to the IPersistFile interface. 
            ppf->Release(); 
        }
        
        // Release the pointer to the IShellLink interface. 
        psl->Release();
    }

    CoUninitialize();

    return isSymlink;
#endif
}


//----------------------------------------------------------------------------------------
nsresult nsFileSpec::ResolveSymlink(PRBool& wasSymlink)
//----------------------------------------------------------------------------------------
{
#ifdef XP_OS2
    return NS_OK;           // no symlinks on OS/2
#else
    wasSymlink = PR_FALSE;  // assume failure

	if (Exists())
		return NS_OK;


    HRESULT hres; 
    IShellLink* psl; 

    CoInitialize(NULL);

    // Get a pointer to the IShellLink interface. 
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl); 
    if (SUCCEEDED(hres)) 
    { 
        IPersistFile* ppf; 
        
        // Get a pointer to the IPersistFile interface. 
        hres = psl->QueryInterface(IID_IPersistFile, (void**)&ppf); 
        
        if (SUCCEEDED(hres)) 
        {
            WORD wsz[MAX_PATH]; 
            // Ensure that the string is Unicode. 
            MultiByteToWideChar(CP_ACP, 0, mPath, -1, wsz, MAX_PATH); 
 
            // Load the shortcut. 
            hres = ppf->Load(wsz, STGM_READ); 
            if (SUCCEEDED(hres)) 
            {
                wasSymlink = PR_TRUE;

                // Resolve the link. 
                hres = psl->Resolve(nsnull, SLR_NO_UI ); 
                if (SUCCEEDED(hres)) 
                { 
                    char szGotPath[MAX_PATH]; 
                    WIN32_FIND_DATA wfd; 

                    // Get the path to the link target. 
                    hres = psl->GetPath( szGotPath, MAX_PATH, &wfd, SLGP_UNCPRIORITY ); 

                    if (SUCCEEDED(hres))
                    {
                        // Here we modify the nsFileSpec;
                        mPath = szGotPath;
                        mError = NS_OK;
                    }
                } 
            }
            else {
                // It wasn't a shortcut. Oh well. Leave it like it was.
                hres = 0;
            }

            // Release the pointer to the IPersistFile interface. 
            ppf->Release(); 
        }
        // Release the pointer to the IShellLink interface. 
        psl->Release();
    }

    CoUninitialize();

    if (SUCCEEDED(hres))
        return NS_OK;

    return NS_FILE_FAILURE;
#endif
}



//----------------------------------------------------------------------------------------
void nsFileSpec::GetParent(nsFileSpec& outSpec) const
//----------------------------------------------------------------------------------------
{
	outSpec.mPath = mPath;
	char* chars = (char*)outSpec.mPath;
	chars[outSpec.mPath.Length() - 1] = '\0'; // avoid trailing separator, if any
    char* cp = strrchr(chars, '\\');
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
	if (!mPath.IsEmpty())
#ifdef XP_OS2
	    // OS2TODO - vacpp complains about mkdir but PR_MkDir should be ok?
            PR_MkDir(nsNSPRPath(*this), PR_CREATE_FILE);
#else
	    mkdir(nsNSPRPath(*this));
#endif
} // nsFileSpec::CreateDirectory

//----------------------------------------------------------------------------------------
void nsFileSpec::Delete(PRBool inRecursive) const
//----------------------------------------------------------------------------------------
{
    if (IsDirectory())
    {
	    if (inRecursive)
        {
            for (nsDirectoryIterator i(*this, PR_FALSE); i.Exists(); i++)
                {
                    nsFileSpec& child = i.Spec();
                    child.Delete(inRecursive);
                }		
        }
#ifdef XP_OS2
            // OS2TODO - vacpp complains if use rmdir but PR_RmDir should be ok?
            PR_RmDir(nsNSPRPath(*this));
#else
	    rmdir(nsNSPRPath(*this));
#endif
    }
	else if (!mPath.IsEmpty())
    {
        remove(nsNSPRPath(*this));
    }
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
			nsFileSpec& child = i.Spec();

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
nsresult
nsFileSpec::Truncate(PRInt32 aNewFileLength) const
//----------------------------------------------------------------------------------------
{
#ifdef XP_OS2
    APIRET rc;
    HFILE hFile;
    ULONG actionTaken;

    rc = DosOpen(mPath,
                       &hFile,
                       &actionTaken,
                       0,                 
                       FILE_NORMAL,
                       OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                       OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE,
                       NULL);
                                 
    if (rc != NO_ERROR)
        return NS_FILE_FAILURE;

    rc = DosSetFileSize(hFile, aNewFileLength);

    if (rc == NO_ERROR) 
        DosClose(hFile);
    else
        goto error; 
#else
    DWORD status;
    HANDLE hFile;

    // Leave it to Microsoft to open an existing file with a function
    // named "CreateFile".
    hFile = CreateFile(mPath,
                       GENERIC_WRITE, 
                       FILE_SHARE_READ, 
                       NULL, 
                       OPEN_EXISTING, 
                       FILE_ATTRIBUTE_NORMAL, 
                       NULL); 
    if (hFile == INVALID_HANDLE_VALUE)
        return NS_FILE_FAILURE;

    // Seek to new, desired end of file
    status = SetFilePointer(hFile, aNewFileLength, NULL, FILE_BEGIN);
    if (status == 0xffffffff)
        goto error;

    // Truncate file at current cursor position
    if (!SetEndOfFile(hFile))
        goto error;

    if (!CloseHandle(hFile))
        return NS_FILE_FAILURE;
#endif

    return NS_OK;

 error:
#ifdef XP_OS2
    DosClose(hFile);
#else
    CloseHandle(hFile);
#endif
    return NS_FILE_FAILURE;

} // nsFileSpec::Truncate

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Rename(const char* inNewName)
//----------------------------------------------------------------------------------------
{
	NS_ASSERTION(inNewName, "Attempt to Rename with a null string");

    // This function should not be used to move a file on disk. 
    if (strchr(inNewName, '/')) 
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
nsresult nsFileSpec::CopyToDir(const nsFileSpec& inParentDirectory) const
//----------------------------------------------------------------------------------------
{
    // We can only copy into a directory, and (for now) can not copy entire directories
    if (inParentDirectory.IsDirectory() && (! IsDirectory() ) )
    {
        char *leafname = GetLeafName();
        nsSimpleCharString destPath(inParentDirectory.GetCString());
        destPath += "\\";
        destPath += leafname;
        nsCRT::free(leafname);
        
        // CopyFile returns non-zero if succeeds
#ifdef XP_OS2
        APIRET rc;
        PRBool copyOK;

        rc = DosCopy(GetCString(), (PSZ)destPath, DCPY_EXISTING);

        if (rc == NO_ERROR)
            copyOK = PR_TRUE;
        else
            copyOK = PR_FALSE;
#else
        int copyOK = CopyFile(GetCString(), destPath, PR_TRUE);
#endif
        if (copyOK)
            return NS_OK;
    }
    return NS_FILE_FAILURE;
} // nsFileSpec::CopyToDir

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::MoveToDir(const nsFileSpec& inNewParentDirectory)
//----------------------------------------------------------------------------------------
{
    // We can only copy into a directory, and (for now) can not copy entire directories
    if (inNewParentDirectory.IsDirectory() && (! IsDirectory() ) )
    {
        char *leafname = GetLeafName();
        nsSimpleCharString destPath(inNewParentDirectory.GetCString());
        destPath += "\\";
        destPath += leafname;
        nsCRT::free(leafname);

        if (DosMove(GetCString(), destPath) == NO_ERROR)
        {
            *this = inNewParentDirectory + GetLeafName(); 
            return NS_OK;
        }
        
    }
    return NS_FILE_FAILURE;
} // nsFileSpec::MoveToDir

//----------------------------------------------------------------------------------------
nsresult nsFileSpec::Execute(const char* inArgs ) const
//----------------------------------------------------------------------------------------
{    
    if (!IsDirectory())
    {
#ifdef XP_OS2
        nsresult result = NS_FILE_FAILURE;
    
        if (!mPath.IsEmpty())
        {
            nsSimpleCharString fileNameWithArgs = mPath + " " + inArgs;   
            result = NS_FILE_RESULT(system(fileNameWithArgs));
        } 
        return result;
#else
        nsSimpleCharString fileNameWithArgs = "\"";
        fileNameWithArgs += mPath + "\" " + inArgs;
        int execResult = WinExec( fileNameWithArgs, SW_NORMAL );     
        if (execResult > 31)
            return NS_OK;
#endif
    }
    return NS_FILE_FAILURE;
} // nsFileSpec::Execute


//----------------------------------------------------------------------------------------
PRInt64 nsFileSpec::GetDiskSpaceAvailable() const
//----------------------------------------------------------------------------------------
{
    PRInt64 nBytes = 0;
    ULONG ulDriveNo = toupper(mPath[0]) + 1 - 'A';
    FSALLOCATE fsAllocate;
    APIRET rc = DosQueryFSInfo(ulDriveNo,
                               FSIL_ALLOC,
                               &fsAllocate,
                               sizeof(fsAllocate));

    if (rc == NO_ERROR) {
       nBytes = fsAllocate.cUnitAvail;
       nBytes *= fsAllocate.cSectorUnit;
       nBytes *= fsAllocate.cbSector;
    }

    return nBytes;
}



//========================================================================================
//								nsDirectoryIterator
//========================================================================================

//----------------------------------------------------------------------------------------
nsDirectoryIterator::nsDirectoryIterator(const nsFileSpec& inDirectory, PRBool resolveSymlink)
//----------------------------------------------------------------------------------------
	: mCurrent(inDirectory)
	, mDir(nsnull)
    , mStarting(inDirectory)
	, mExists(PR_FALSE)
    , mResoveSymLinks(resolveSymlink)
{
    mDir = PR_OpenDir(inDirectory);
	mCurrent += "dummy";
    mStarting += "dummy";
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
      mCurrent = mStarting;
      mCurrent.SetLeafName(entry->name);
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

