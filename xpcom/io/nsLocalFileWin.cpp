/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Doug Turner <dougt@netscape.com>
 *   Dean Tessman <dean_tessman@hotmail.com>
 *   Brodie Thiesfield <brofield@jellycan.com>
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
 * ***** END LICENSE BLOCK ***** */


#include "nsCOMPtr.h"
#include "nsMemory.h"

#include "nsLocalFile.h"
#include "nsNativeCharsetUtils.h"

#include "nsISimpleEnumerator.h"
#include "nsIComponentManager.h"
#include "prtypes.h"
#include "prio.h"

#include "nsXPIDLString.h"
#include "nsReadableUtils.h"

#include <direct.h>
#include <windows.h>

#include "shellapi.h"
#include "shlguid.h"

#include  <io.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <mbstring.h>

#include "nsXPIDLString.h"
#include "prproces.h"
#include "nsITimelineService.h"

#include "nsAutoLock.h"
#include "SpecialSystemDirectory.h"

// _mbsstr isn't declared in w32api headers but it's there in the libs
#ifdef __MINGW32__
extern "C" {
unsigned char *_mbsstr( const unsigned char *str,
                        const unsigned char *substr );
}
#endif

class nsDriveEnumerator : public nsISimpleEnumerator
{
public:
    nsDriveEnumerator();
    virtual ~nsDriveEnumerator();
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR
    nsresult Init();
private:
    /* mDrives and mLetter share data
     * Init sets them.
     * HasMoreElements reads mLetter.
     * GetNext advances mLetter.
     */
    nsCString mDrives;
    const char *mLetter;
};

//----------------------------------------------------------------------------
// short cut resolver
//----------------------------------------------------------------------------

class ShortcutResolver
{
public:
    ShortcutResolver();
    // nonvirtual since we're not subclassed
    ~ShortcutResolver();

    nsresult Init();
    nsresult Resolve(const WCHAR* in, char* out);

private:
    PRLock*       mLock;
    IPersistFile* mPersistFile;
    IShellLink*   mShellLink;
};

ShortcutResolver::ShortcutResolver()
{
    mLock = nsnull;
    mPersistFile = nsnull;
    mShellLink   = nsnull;
}

ShortcutResolver::~ShortcutResolver()
{
    if (mLock)
        PR_DestroyLock(mLock);

    // Release the pointer to the IPersistFile interface.
    if (mPersistFile)
        mPersistFile->Release();

    // Release the pointer to the IShellLink interface.
    if(mShellLink)
        mShellLink->Release();

    CoUninitialize();
}

nsresult
ShortcutResolver::Init()
{
    CoInitialize(NULL);  // FIX: we should probably move somewhere higher up during startup

    mLock = PR_NewLock();
    if (!mLock)
        return NS_ERROR_FAILURE;

    HRESULT hres = CoCreateInstance(CLSID_ShellLink,
                                    NULL,
                                    CLSCTX_INPROC_SERVER,
                                    IID_IShellLink,
                                    (void**)&mShellLink);
    if (SUCCEEDED(hres))
    {
        // Get a pointer to the IPersistFile interface.
        hres = mShellLink->QueryInterface(IID_IPersistFile, (void**)&mPersistFile);
    }

    if (mPersistFile == nsnull || mShellLink == nsnull)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

// |out| must be an allocated buffer of size MAX_PATH
nsresult
ShortcutResolver::Resolve(const WCHAR* in, char* out)
{
    nsAutoLock lock(mLock);

    // see if we can Load the path.
    HRESULT hres = mPersistFile->Load(in, STGM_READ);

    if (FAILED(hres))
        return NS_ERROR_FAILURE;

    // Resolve the link.
    hres = mShellLink->Resolve(nsnull, SLR_NO_UI );

    if (FAILED(hres))
        return NS_ERROR_FAILURE;

    WIN32_FIND_DATA wfd;
    // Get the path to the link target.
    hres = mShellLink->GetPath( out, MAX_PATH, &wfd, SLGP_UNCPRIORITY );
    if (FAILED(hres))
        return NS_ERROR_FAILURE;
    return NS_OK;
}

static ShortcutResolver * gResolver = nsnull;

static nsresult NS_CreateShortcutResolver()
{
    gResolver = new ShortcutResolver();
    if (!gResolver)
        return NS_ERROR_OUT_OF_MEMORY;

    return gResolver->Init();
}

static void NS_DestroyShortcutResolver()
{
    delete gResolver;
    gResolver = nsnull;
}


//-----------------------------------------------------------------------------
// static helper functions
//-----------------------------------------------------------------------------

// certainly not all the error that can be
// encountered, but many of them common ones
static nsresult ConvertWinError(DWORD winErr)
{
    nsresult rv;

    switch (winErr)
    {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_DRIVE:
            rv = NS_ERROR_FILE_NOT_FOUND;
            break;
        case ERROR_ACCESS_DENIED:
        case ERROR_NOT_SAME_DEVICE:
            rv = NS_ERROR_FILE_ACCESS_DENIED;
            break;
        case ERROR_NOT_ENOUGH_MEMORY:
        case ERROR_INVALID_BLOCK:
        case ERROR_INVALID_HANDLE:
        case ERROR_ARENA_TRASHED:
            rv = NS_ERROR_OUT_OF_MEMORY;
            break;
        case ERROR_CURRENT_DIRECTORY:
            rv = NS_ERROR_FILE_DIR_NOT_EMPTY;
            break;
        case ERROR_WRITE_PROTECT:
            rv = NS_ERROR_FILE_READ_ONLY;
            break;
        case ERROR_HANDLE_DISK_FULL:
            rv = NS_ERROR_FILE_TOO_BIG;
            break;
        case ERROR_FILE_EXISTS:
        case ERROR_ALREADY_EXISTS:
        case ERROR_CANNOT_MAKE:
            rv = NS_ERROR_FILE_ALREADY_EXISTS;
            break;
        case 0:
            rv = NS_OK;
        default:
            rv = NS_ERROR_FAILURE;
    }
    return rv;
}

// definition of INVALID_SET_FILE_POINTER from VC.NET header files
// it doesn't appear to be defined by VC6
#ifndef INVALID_SET_FILE_POINTER
# define INVALID_SET_FILE_POINTER ((DWORD)-1)
#endif
// same goes for INVALID_FILE_ATTRIBUTES
#ifndef INVALID_FILE_ATTRIBUTES
# define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#endif

// as suggested in the MSDN documentation on SetFilePointer
static __int64 
MyFileSeek64(HANDLE aHandle, __int64 aDistance, DWORD aMoveMethod)
{
    LARGE_INTEGER li;

    li.QuadPart = aDistance;
    li.LowPart = SetFilePointer(aHandle, li.LowPart, &li.HighPart, aMoveMethod);
    if (li.LowPart == INVALID_SET_FILE_POINTER && GetLastError() != NO_ERROR)
    {
        li.QuadPart = -1;
    }

    return li.QuadPart;
}

static PRBool
IsShortcutPath(const char *path)
{
    // Under Windows, the shortcuts are just files with a ".lnk" extension. 
    // Note also that we don't resolve links in the middle of paths.
    // i.e. "c:\foo.lnk\bar.txt" is invalid.
    NS_ABORT_IF_FALSE(path, "don't pass nulls");
    const char * ext = (const char *) _mbsrchr((const unsigned char *)path, '.');
    if (!ext || 0 != stricmp(ext + 1, "lnk"))
        return PR_FALSE;
    return PR_TRUE;
}

//-----------------------------------------------------------------------------
// nsDirEnumerator
//-----------------------------------------------------------------------------

class nsDirEnumerator : public nsISimpleEnumerator
{
    public:

        NS_DECL_ISUPPORTS

        nsDirEnumerator() : mDir(nsnull)
        {
        }

        nsresult Init(nsILocalFile* parent)
        {
            nsCAutoString filepath;
            parent->GetNativeTarget(filepath);

            if (filepath.IsEmpty())
            {
                parent->GetNativePath(filepath);
            }

            if (filepath.IsEmpty())
            {
                return NS_ERROR_UNEXPECTED;
            }

            mDir = PR_OpenDir(filepath.get());
            if (mDir == nsnull)    // not a directory?
                return NS_ERROR_FAILURE;

            mParent = parent;
            return NS_OK;
        }

        NS_IMETHOD HasMoreElements(PRBool *result)
        {
            nsresult rv;
            if (mNext == nsnull && mDir)
            {
                PRDirEntry* entry = PR_ReadDir(mDir, PR_SKIP_BOTH);
                if (entry == nsnull)
                {
                    // end of dir entries

                    PRStatus status = PR_CloseDir(mDir);
                    if (status != PR_SUCCESS)
                        return NS_ERROR_FAILURE;
                    mDir = nsnull;

                    *result = PR_FALSE;
                    return NS_OK;
                }

                nsCOMPtr<nsIFile> file;
                rv = mParent->Clone(getter_AddRefs(file));
                if (NS_FAILED(rv))
                    return rv;

                rv = file->AppendNative(nsDependentCString(entry->name));
                if (NS_FAILED(rv))
                    return rv;

                // make sure the thing exists.  If it does, try the next one.
                PRBool exists;
                rv = file->Exists(&exists);
                if (NS_FAILED(rv) || !exists)
                {
                    return HasMoreElements(result);
                }

                mNext = do_QueryInterface(file);
            }
            *result = mNext != nsnull;
            return NS_OK;
        }

        NS_IMETHOD GetNext(nsISupports **result)
        {
            nsresult rv;
            PRBool hasMore;
            rv = HasMoreElements(&hasMore);
            if (NS_FAILED(rv)) return rv;

            *result = mNext;        // might return nsnull
            NS_IF_ADDREF(*result);

            mNext = nsnull;
            return NS_OK;
        }

        // dtor can be non-virtual since there are no subclasses, but must be
        // public to use the class on the stack.
        ~nsDirEnumerator()
        {
            if (mDir)
            {
                PRStatus status = PR_CloseDir(mDir);
                NS_ASSERTION(status == PR_SUCCESS, "close failed");
            }
        }

    protected:
        PRDir*                  mDir;
        nsCOMPtr<nsILocalFile>  mParent;
        nsCOMPtr<nsILocalFile>  mNext;
};

NS_IMPL_ISUPPORTS1(nsDirEnumerator, nsISimpleEnumerator)


//-----------------------------------------------------------------------------
// nsLocalFile <public>
//-----------------------------------------------------------------------------

nsLocalFile::nsLocalFile()
{
    mFollowSymlinks = PR_FALSE;
    MakeDirty();
}

NS_METHOD
nsLocalFile::nsLocalFileConstructor(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    NS_ENSURE_ARG_POINTER(aInstancePtr);
    NS_ENSURE_NO_AGGREGATION(outer);

    nsLocalFile* inst = new nsLocalFile();
    if (inst == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = inst->QueryInterface(aIID, aInstancePtr);
    if (NS_FAILED(rv))
    {
        delete inst;
        return rv;
    }
    return NS_OK;
}


//-----------------------------------------------------------------------------
// nsLocalFile::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_THREADSAFE_ISUPPORTS2(nsLocalFile, nsILocalFile, nsIFile)


//-----------------------------------------------------------------------------
// nsLocalFile <private>
//-----------------------------------------------------------------------------

nsLocalFile::nsLocalFile(const nsLocalFile& other)
  : mDirty(other.mDirty)
  , mFollowSymlinks(other.mFollowSymlinks)
  , mWorkingPath(other.mWorkingPath)
  , mResolvedPath(other.mResolvedPath)
  , mFileInfo64(other.mFileInfo64)
{
}

// Resolve the shortcut file from mWorkingPath and write the path 
// it points to into mResolvedPath.
nsresult
nsLocalFile::ResolveShortcut()
{
    // we can't do anything without the resolver
    if (!gResolver)
        return NS_ERROR_FAILURE;

    // allocate the memory for the result of the resolution
    nsAutoString ucsBuf;
    NS_CopyNativeToUnicode(mWorkingPath, ucsBuf);

    mResolvedPath.SetLength(MAX_PATH);
    char *resolvedPath = mResolvedPath.BeginWriting();

    // resolve this shortcut
    nsresult rv = gResolver->Resolve(ucsBuf.get(), resolvedPath);

    size_t len = NS_FAILED(rv) ? 0 : strlen(resolvedPath);
    mResolvedPath.SetLength(len);

    return rv;
}

// Resolve any shortcuts and stat the resolved path. After a successful return
// the path is guaranteed valid and the members of mFileInfo64 can be used.
nsresult
nsLocalFile::ResolveAndStat()
{
    // if we aren't dirty then we are already done
    if (!mDirty)
        return NS_OK;

    // we can't resolve/stat anything that isn't a valid NSPR addressable path
    if (mWorkingPath.IsEmpty())
        return NS_ERROR_FILE_INVALID_PATH;

    // this is usually correct
    mResolvedPath.Assign(mWorkingPath);

    // slutty hack designed to work around bug 134796 until it is fixed
    char temp[4];
    const char *nsprPath = mWorkingPath.get();
    if (mWorkingPath.Length() == 2 && mWorkingPath.CharAt(1) == ':') 
    {
        temp[0] = mWorkingPath[0];
        temp[1] = mWorkingPath[1];
        temp[2] = '\\';
        temp[3] = '\0';
        nsprPath = temp;
    }

    // first we will see if the working path exists. If it doesn't then
    // there is nothing more that can be done
    PRStatus status = PR_GetFileInfo64(nsprPath, &mFileInfo64);
    if (status != PR_SUCCESS)
        return NS_ERROR_FILE_NOT_FOUND;

    // if this isn't a shortcut file or we aren't following symlinks then we're done 
    if (!mFollowSymlinks 
        || mFileInfo64.type != PR_FILE_FILE 
        || !IsShortcutPath(mWorkingPath.get()))
    {
        mDirty = PR_FALSE;
        return NS_OK;
    }

    // we need to resolve this shortcut to what it points to, this will
    // set mResolvedPath. Even if it fails we need to have the resolved
    // path equal to working path for those functions that always use
    // the resolved path.
    nsresult rv = ResolveShortcut();
    if (NS_FAILED(rv))
    {
        mResolvedPath.Assign(mWorkingPath);
        return rv;
    }

    // get the details of the resolved path
    status = PR_GetFileInfo64(mResolvedPath.get(), &mFileInfo64);
    if (status != PR_SUCCESS)
        return NS_ERROR_FILE_NOT_FOUND;

    mDirty = PR_FALSE;
    return NS_OK;
}


//-----------------------------------------------------------------------------
// nsLocalFile::nsIFile,nsILocalFile
//-----------------------------------------------------------------------------

NS_IMETHODIMP
nsLocalFile::Clone(nsIFile **file)
{
    // Just copy-construct ourselves
    *file = new nsLocalFile(*this);
    if (!*file)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*file);
    
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::InitWithNativePath(const nsACString &filePath)
{
    MakeDirty();

    nsACString::const_iterator begin, end;
    filePath.BeginReading(begin);
    filePath.EndReading(end);

    // input string must not be empty
    if (begin == end)
        return NS_ERROR_FAILURE;

    char firstChar = *begin;
    char secondChar = *(++begin);

    // just do a sanity check.  if it has any forward slashes, it is not a Native path
    // on windows.  Also, it must have a colon at after the first char.

    char *path = nsnull;
    PRInt32 pathLen = 0;

    if ( ( (secondChar == ':') && !FindCharInReadable('/', begin, end) ) ||  // normal path
         ( (firstChar == '\\') && (secondChar == '\\') ) )  // network path
    {
        // This is a native path
        path = ToNewCString(filePath);
        pathLen = filePath.Length();
    }

    if (path == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    // kill any trailing '\' provided it isn't the second char of DBCS
    PRInt32 len = pathLen - 1;
    if (path[len] == '\\' &&
        (!::IsDBCSLeadByte(path[len-1]) ||
         _mbsrchr((const unsigned char *)path, '\\') == (const unsigned char *)path+len))
    {
        path[len] = '\0';
        pathLen = len;
    }

    mWorkingPath.Adopt(path, pathLen);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::OpenNSPRFileDesc(PRInt32 flags, PRInt32 mode, PRFileDesc **_retval)
{
    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv;

    *_retval = PR_Open(mResolvedPath.get(), flags, mode);
    if (*_retval)
        return NS_OK;

    return NS_ErrorAccordingToNSPR();
}


NS_IMETHODIMP
nsLocalFile::OpenANSIFileDesc(const char *mode, FILE * *_retval)
{
    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv;

    *_retval = fopen(mResolvedPath.get(), mode);
    if (*_retval)
        return NS_OK;

    return NS_ERROR_FAILURE;
}



NS_IMETHODIMP
nsLocalFile::Create(PRUint32 type, PRUint32 attributes)
{
    if (type != NORMAL_FILE_TYPE && type != DIRECTORY_TYPE)
        return NS_ERROR_FILE_UNKNOWN_TYPE;

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv;

    // create directories to target
    //
    // A given local file can be either one of these forms:
    //
    //   - normal:    X:\some\path\on\this\drive
    //                       ^--- start here
    //
    //   - UNC path:  \\machine\volume\some\path\on\this\drive
    //                                     ^--- start here
    //
    // Skip the first 'X:\' for the first form, and skip the first full
    // '\\machine\volume\' segment for the second form.

    const unsigned char* path = (const unsigned char*) mResolvedPath.get();
    if (path[0] == '\\' && path[1] == '\\')
    {
        // dealing with a UNC path here; skip past '\\machine\'
        path = _mbschr(path + 2, '\\');
        if (!path)
            return NS_ERROR_FILE_INVALID_PATH;
        ++path;
    }

    // search for first slash after the drive (or volume) name
    unsigned char* slash = _mbschr(path, '\\');

    if (slash)
    {
        // skip the first '\\'
        ++slash;
        slash = _mbschr(slash, '\\');

        while (slash)
        {
            *slash = '\0';

            if (!CreateDirectoryA(mResolvedPath.get(), NULL)) {
                rv = ConvertWinError(GetLastError());
                // perhaps the base path already exists, or perhaps we don't have
                // permissions to create the directory.  NOTE: access denied could
                // occur on a parent directory even though it exists.
                if (rv != NS_ERROR_FILE_ALREADY_EXISTS &&
                    rv != NS_ERROR_FILE_ACCESS_DENIED)
                    return rv;
            }
            *slash = '\\';
            ++slash;
            slash = _mbschr(slash, '\\');
        }
    }

    if (type == NORMAL_FILE_TYPE)
    {
        PRFileDesc* file = PR_Open(mResolvedPath.get(), PR_RDONLY | PR_CREATE_FILE | PR_APPEND | PR_EXCL, attributes);
        if (!file) return NS_ERROR_FILE_ALREADY_EXISTS;

        PR_Close(file);
        return NS_OK;
    }

    if (type == DIRECTORY_TYPE)
    {
        if (!CreateDirectoryA(mResolvedPath.get(), NULL))
            return ConvertWinError(GetLastError());
        else
            return NS_OK;
    }

    return NS_ERROR_FILE_UNKNOWN_TYPE;
}

NS_IMETHODIMP
nsLocalFile::AppendNative(const nsACString &node)
{
    // append this path, multiple components are not permitted
    return AppendNativeInternal(PromiseFlatCString(node), PR_FALSE);
}

NS_IMETHODIMP
nsLocalFile::AppendRelativeNativePath(const nsACString &node)
{
    // append this path, multiple components are permitted
    return AppendNativeInternal(PromiseFlatCString(node), PR_TRUE);
}

nsresult
nsLocalFile::AppendNativeInternal(const nsAFlatCString &node, PRBool multipleComponents)
{
    if (node.IsEmpty())
        return NS_OK;

    // check the relative path for validity
    const unsigned char * nodePath = (const unsigned char *) node.get();
    if (*nodePath == '\\'                                       // can't start with an '\'
        || _mbschr(nodePath, '/')                               // can't contain /
        || node.Equals(".."))                                   // can't be ..
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    if (multipleComponents)
    {
        // can't contain .. as a path component. Ensure that the valid components 
        // "foo..foo", "..foo", and "foo.." are not falsely detected, but the invalid
        // paths "..\", "foo\..", "foo\..\foo", "..\foo", etc are.
        unsigned char * doubleDot = _mbsstr(nodePath, (unsigned char *)"\\..");
        while (doubleDot)
        {
            doubleDot += 3;
            if (*doubleDot == '\0' || *doubleDot == '\\')
                return NS_ERROR_FILE_UNRECOGNIZED_PATH;
            doubleDot = _mbsstr(doubleDot, (unsigned char *)"\\..");
        }
        if (0 == _mbsncmp(nodePath, (unsigned char *)"..\\", 3))  // catches the remaining cases of prefixes 
            return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }
    else if (_mbschr(nodePath, '\\'))   // single components can't contain '\'
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    MakeDirty();
    
    mWorkingPath.Append(NS_LITERAL_CSTRING("\\") + node);
    
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Normalize()
{
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetNativeLeafName(nsACString &aLeafName)
{
    aLeafName.Truncate();

    const char* temp = mWorkingPath.get();
    if(temp == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    const char* leaf = (const char*) _mbsrchr((const unsigned char*) temp, '\\');

    // if the working path is just a node without any lashes.
    if (leaf == nsnull)
        leaf = temp;
    else
        leaf++;

    aLeafName.Assign(leaf);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetNativeLeafName(const nsACString &aLeafName)
{
    MakeDirty();

    const unsigned char* temp = (const unsigned char*) mWorkingPath.get();
    if(temp == nsnull)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    // cannot use nsCString::RFindChar() due to 0x5c problem
    PRInt32 offset = (PRInt32) (_mbsrchr(temp, '\\') - temp);
    if (offset)
    {
        mWorkingPath.Truncate(offset+1);
    }
    mWorkingPath.Append(aLeafName);

    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::GetNativePath(nsACString &_retval)
{
    _retval = mWorkingPath;
    return NS_OK;
}

nsresult
nsLocalFile::CopySingleFile(nsIFile *sourceFile, nsIFile *destParent, const nsACString &newName, 
                            PRBool followSymlinks, PRBool move)
{
    nsresult rv;
    nsCAutoString filePath;

    // get the path that we are going to copy to.
    // Since windows does not know how to auto
    // resolve shortcuts, we must work with the
    // target.
    nsCAutoString destPath;
    destParent->GetNativeTarget(destPath);

    destPath.Append("\\");

    if (newName.IsEmpty())
    {
        nsCAutoString aFileName;
        sourceFile->GetNativeLeafName(aFileName);
        destPath.Append(aFileName);
    }
    else
    {
        destPath.Append(newName);
    }


    if (followSymlinks)
    {
        rv = sourceFile->GetNativeTarget(filePath);
        if (filePath.IsEmpty())
            rv = sourceFile->GetNativePath(filePath);
    }
    else
    {
        rv = sourceFile->GetNativePath(filePath);
    }

    if (NS_FAILED(rv))
        return rv;

    int copyOK;

    if (!move)
        copyOK = CopyFile(filePath.get(), destPath.get(), PR_TRUE);
    else
    {
        // What we have to do is check to see if the destPath exists.  If it
        // does, we have to move it out of the say so that MoveFile will
        // succeed.  However, we don't want to just remove it since MoveFile
        // can fail leaving us without a file.

        nsCAutoString backup;
        PRFileInfo64 fileInfo64;
        PRStatus status = PR_GetFileInfo64(destPath.get(), &fileInfo64);
        if (status == PR_SUCCESS)
        {

            // the file exists.  Check to make sure it is not a directory,
            // then move it out of the way.
            if (fileInfo64.type == PR_FILE_FILE)
            {
                backup.Append(destPath);
                backup.Append(".moztmp");

                // remove any existing backup file that we may already have.
                // maybe we should be doing some kind of unique naming here,
                // but why bother.
                remove(backup.get());

                // move destination file to backup file
                copyOK = MoveFile(destPath.get(), backup.get());
                if (!copyOK)
                {
                    // I guess we can't do the backup copy, so return.
                    rv = ConvertWinError(GetLastError());
                    return rv;
                }
            }
        }
        // move source file to destination file
        copyOK = MoveFile(filePath.get(), destPath.get());

        if (!backup.IsEmpty())
        {
            if (copyOK)
            {
                // remove the backup copy.
                remove(backup.get());
            }
            else
            {
                // restore backup
                int backupOk = MoveFile(backup.get(), destPath.get());
                NS_ASSERTION(backupOk, "move backup failed");
            }
        }
    }
    if (!copyOK)  // CopyFile and MoveFile returns non-zero if succeeds (backward if you ask me).
        rv = ConvertWinError(GetLastError());

    return rv;
}


nsresult
nsLocalFile::CopyMove(nsIFile *aParentDir, const nsACString &newName, PRBool followSymlinks, PRBool move)
{
    nsCOMPtr<nsIFile> newParentDir = aParentDir;
    // check to see if this exists, otherwise return an error.
    // we will check this by resolving.  If the user wants us
    // to follow links, then we are talking about the target,
    // hence we can use the |followSymlinks| parameter.
    nsresult rv  = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    if (!newParentDir)
    {
        // no parent was specified.  We must rename.

        if (newName.IsEmpty())
            return NS_ERROR_INVALID_ARG;

        rv = GetParent(getter_AddRefs(newParentDir));
        if (NS_FAILED(rv))
            return rv;
    }

    if (!newParentDir)
        return NS_ERROR_FILE_DESTINATION_NOT_DIR;

    // make sure it exists and is a directory.  Create it if not there.
    PRBool exists;
    newParentDir->Exists(&exists);
    if (!exists)
    {
        rv = newParentDir->Create(DIRECTORY_TYPE, 0644);  // TODO, what permissions should we use
        if (NS_FAILED(rv))
            return rv;
    }
    else
    {
        PRBool isDir;
        newParentDir->IsDirectory(&isDir);
        if (isDir == PR_FALSE)
        {
            if (followSymlinks)
            {
                PRBool isLink;
                newParentDir->IsSymlink(&isLink);
                if (isLink)
                {
                    nsCAutoString target;
                    newParentDir->GetNativeTarget(target);

                    nsCOMPtr<nsILocalFile> realDest = new nsLocalFile();
                    if (realDest == nsnull)
                        return NS_ERROR_OUT_OF_MEMORY;

                    rv = realDest->InitWithNativePath(target);

                    if (NS_FAILED(rv))
                        return rv;

                    return CopyMove(realDest, newName, followSymlinks, move);
                }
            }
            else
            {
                return NS_ERROR_FILE_DESTINATION_NOT_DIR;
            }
        }
    }

    // check to see if we are a directory, if so enumerate it.

    PRBool isDir;
    IsDirectory(&isDir);
    PRBool isSymlink;
    IsSymlink(&isSymlink);

    if (!isDir || (isSymlink && !followSymlinks))
    {
        rv = CopySingleFile(this, newParentDir, newName, followSymlinks, move);
        if (NS_FAILED(rv))
            return rv;
    }
    else
    {
        // create a new target destination in the new parentDir;
        nsCOMPtr<nsIFile> target;
        rv = newParentDir->Clone(getter_AddRefs(target));

        if (NS_FAILED(rv))
            return rv;

        nsCAutoString allocatedNewName;
        if (newName.IsEmpty())
        {
            PRBool isLink;
            IsSymlink(&isLink);
            if (isLink)
            {
                nsCAutoString temp;
                GetNativeTarget(temp);
                const char* leaf = (const char*) _mbsrchr((const unsigned char*) temp.get(), '\\');
                if (leaf[0] == '\\')
                    leaf++;
                allocatedNewName = leaf;
            }
            else
            {
                GetNativeLeafName(allocatedNewName);// this should be the leaf name of the
            }
        }
        else
        {
            allocatedNewName = newName;
        }

        rv = target->AppendNative(allocatedNewName);
        if (NS_FAILED(rv))
            return rv;

        allocatedNewName.Truncate();

        // check if the destination directory already exists
        target->Exists(&exists);
        if (!exists)
        {
            // if the destination directory cannot be created, return an error
            rv = target->Create(DIRECTORY_TYPE, 0644);  // TODO, what permissions should we use
            if (NS_FAILED(rv))
                return rv;
        }
        else
        {
            // check if the destination directory is writable and empty
            PRBool isWritable;

            target->IsWritable(&isWritable);
            if (!isWritable)
                return NS_ERROR_FILE_ACCESS_DENIED;

            nsCOMPtr<nsISimpleEnumerator> targetIterator;
            rv = target->GetDirectoryEntries(getter_AddRefs(targetIterator));

            PRBool more;
            targetIterator->HasMoreElements(&more);
            // return error if target directory is not empty
            if (more)
                return NS_ERROR_FILE_DIR_NOT_EMPTY;
        }

        nsDirEnumerator dirEnum;

        rv = dirEnum.Init(this);
        if (NS_FAILED(rv)) {
            NS_WARNING("dirEnum initalization failed");
            return rv;
        }

        PRBool more;
        while (NS_SUCCEEDED(dirEnum.HasMoreElements(&more)) && more)
        {
            nsCOMPtr<nsISupports> item;
            nsCOMPtr<nsIFile> file;
            dirEnum.GetNext(getter_AddRefs(item));
            file = do_QueryInterface(item);
            if (file)
            {
                PRBool isDir, isLink;

                file->IsDirectory(&isDir);
                file->IsSymlink(&isLink);

                if (move)
                {
                    if (followSymlinks)
                        return NS_ERROR_FAILURE;

                    rv = file->MoveToNative(target, nsCString());
                    NS_ENSURE_SUCCESS(rv,rv);
                }
                else
                {
                    if (followSymlinks)
                        rv = file->CopyToFollowingLinksNative(target, nsCString());
                    else
                        rv = file->CopyToNative(target, nsCString());
                    NS_ENSURE_SUCCESS(rv,rv);
                }
            }
        }
        // we've finished moving all the children of this directory
        // in the new directory.  so now delete the directory
        // note, we don't need to do a recursive delete.
        // MoveTo() is recursive.  At this point,
        // we've already moved the children of the current folder
        // to the new location.  nothing should be left in the folder.
        if (move)
        {
          rv = Remove(PR_FALSE /* recursive */);
          NS_ENSURE_SUCCESS(rv,rv);
        }
    }


    // If we moved, we want to adjust this.
    if (move)
    {
        MakeDirty();

        nsCAutoString newParentPath;
        newParentDir->GetNativePath(newParentPath);

        if (newParentPath.IsEmpty())
            return NS_ERROR_FAILURE;

        if (newName.IsEmpty())
        {
            nsCAutoString aFileName;
            GetNativeLeafName(aFileName);

            InitWithNativePath(newParentPath);
            AppendNative(aFileName);
        }
        else
        {
            InitWithNativePath(newParentPath);
            AppendNative(newName);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::CopyToNative(nsIFile *newParentDir, const nsACString &newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE, PR_FALSE);
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinksNative(nsIFile *newParentDir, const nsACString &newName)
{
    return CopyMove(newParentDir, newName, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP
nsLocalFile::MoveToNative(nsIFile *newParentDir, const nsACString &newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE, PR_TRUE);
}

NS_IMETHODIMP
nsLocalFile::Load(PRLibrary * *_retval)
{
    PRBool isFile;
    nsresult rv = IsFile(&isFile);

    if (NS_FAILED(rv))
        return rv;

    if (! isFile)
        return NS_ERROR_FILE_IS_DIRECTORY;

    NS_TIMELINE_START_TIMER("PR_LoadLibrary");
    *_retval =  PR_LoadLibrary(mResolvedPath.get());
    NS_TIMELINE_STOP_TIMER("PR_LoadLibrary");
    NS_TIMELINE_MARK_TIMER1("PR_LoadLibrary", mResolvedPath.get());

    if (*_retval)
        return NS_OK;

    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsLocalFile::Remove(PRBool recursive)
{
    // NOTE:
    //
    // if the working path points to a shortcut, then we will only
    // delete the shortcut itself.  even if the shortcut points to
    // a directory, we will not recurse into that directory or 
    // delete that directory itself.  likewise, if the shortcut
    // points to a normal file, we will not delete the real file.
    // this is done to be consistent with the other platforms that
    // behave this way.  we do this even if the followLinks attribute
    // is set to true.  this helps protect against misuse that could
    // lead to security bugs (e.g., bug 210588).
    //
    // Since shortcut files are no longer permitted to be used as unix-like
    // symlinks interspersed in the path (e.g. "c:/file.lnk/foo/bar.txt")
    // this processing is a lot simpler. Even if the shortcut file is 
    // pointing to a directory, only the mWorkingPath value is used and so
    // only the shortcut file will be deleted.

    PRBool isDir, isLink;
    nsresult rv;
    
    isDir = PR_FALSE;
    rv = IsSymlink(&isLink);
    if (NS_FAILED(rv))
        return rv;

    // only check to see if we have a directory if it isn't a link
    if (!isLink)
    {
        rv = IsDirectory(&isDir);
        if (NS_FAILED(rv))
            return rv;
    }

    if (isDir)
    {
        if (recursive)
        {
            nsDirEnumerator dirEnum;

            rv = dirEnum.Init(this);
            if (NS_FAILED(rv))
                return rv;

            PRBool more;
            while (NS_SUCCEEDED(dirEnum.HasMoreElements(&more)) && more)
            {
                nsCOMPtr<nsISupports> item;
                dirEnum.GetNext(getter_AddRefs(item));
                nsCOMPtr<nsIFile> file = do_QueryInterface(item);
                if (file)
                    file->Remove(recursive);
            }
        }
        rv = rmdir(mWorkingPath.get());
    }
    else
    {
        rv = remove(mWorkingPath.get());
    }

    // fixup error code if necessary...
    if (rv == (nsresult)-1)
        rv = NSRESULT_FOR_ERRNO();
    
    MakeDirty();
    return rv;
}

NS_IMETHODIMP
nsLocalFile::GetLastModifiedTime(PRInt64 *aLastModifiedTime)
{
    NS_ENSURE_ARG(aLastModifiedTime);
 
    // get the modified time of the target as determined by mFollowSymlinks
    // If PR_TRUE, then this will be for the target of the shortcut file, 
    // otherwise it will be for the shortcut file itself (i.e. the same 
    // results as GetLastModifiedTimeOfLink)

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    // microseconds -> milliseconds
    PRInt64 usecPerMsec;
    LL_I2L(usecPerMsec, PR_USEC_PER_MSEC);
    LL_DIV(*aLastModifiedTime, mFileInfo64.modifyTime, usecPerMsec);
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::GetLastModifiedTimeOfLink(PRInt64 *aLastModifiedTime)
{
    NS_ENSURE_ARG(aLastModifiedTime);
 
    // The caller is assumed to have already called IsSymlink 
    // and to have found that this file is a link. 

    PRFileInfo64 info;
    if (PR_GetFileInfo64(mWorkingPath.get(), &info) != PR_SUCCESS)
        return NSRESULT_FOR_ERRNO();

    // microseconds -> milliseconds
    PRInt64 usecPerMsec;
    LL_I2L(usecPerMsec, PR_USEC_PER_MSEC);
    LL_DIV(*aLastModifiedTime, info.modifyTime, usecPerMsec);
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::SetLastModifiedTime(PRInt64 aLastModifiedTime)
{
    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    // set the modified time of the target as determined by mFollowSymlinks
    // If PR_TRUE, then this will be for the target of the shortcut file, 
    // otherwise it will be for the shortcut file itself (i.e. the same 
    // results as SetLastModifiedTimeOfLink)

    rv = SetModDate(aLastModifiedTime, mResolvedPath.get());
    if (NS_SUCCEEDED(rv))
        MakeDirty();

    return rv;
}


NS_IMETHODIMP
nsLocalFile::SetLastModifiedTimeOfLink(PRInt64 aLastModifiedTime)
{
    // The caller is assumed to have already called IsSymlink 
    // and to have found that this file is a link. 

    nsresult rv = SetModDate(aLastModifiedTime, mWorkingPath.get());
    if (NS_SUCCEEDED(rv))
        MakeDirty();

    return rv;
}

nsresult
nsLocalFile::SetModDate(PRInt64 aLastModifiedTime, const char *filePath)
{
    HANDLE file = CreateFile(filePath,          // pointer to name of the file
                             GENERIC_WRITE,     // access (write) mode
                             0,                 // share mode
                             NULL,              // pointer to security attributes
                             OPEN_EXISTING,     // how to create
                             0,                 // file attributes
                             NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        return ConvertWinError(GetLastError());
    }

    FILETIME lft, ft;
    SYSTEMTIME st;
    PRExplodedTime pret;

    // PR_ExplodeTime expects usecs...
    PR_ExplodeTime(aLastModifiedTime * PR_USEC_PER_MSEC, PR_LocalTimeParameters, &pret);
    st.wYear            = pret.tm_year;
    st.wMonth           = pret.tm_month + 1; // Convert start offset -- Win32: Jan=1; NSPR: Jan=0
    st.wDayOfWeek       = pret.tm_wday;
    st.wDay             = pret.tm_mday;
    st.wHour            = pret.tm_hour;
    st.wMinute          = pret.tm_min;
    st.wSecond          = pret.tm_sec;
    st.wMilliseconds    = pret.tm_usec/1000;

    nsresult rv = NS_OK;
    if (    0 != SystemTimeToFileTime(&st, &lft) 
         || 0 != LocalFileTimeToFileTime(&lft, &ft) 
         || 0 != SetFileTime(file, NULL, &ft, &ft) )
    {
        rv = ConvertWinError(GetLastError());
    }

    CloseHandle(file);
    return rv;
}

NS_IMETHODIMP
nsLocalFile::GetPermissions(PRUint32 *aPermissions)
{
    NS_ENSURE_ARG(aPermissions);

    // get the permissions of the target as determined by mFollowSymlinks
    // If PR_TRUE, then this will be for the target of the shortcut file, 
    // otherwise it will be for the shortcut file itself (i.e. the same 
    // results as GetPermissionsOfLink)
    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    PRBool isWritable, isExecutable;
    IsWritable(&isWritable);
    IsExecutable(&isExecutable);

    *aPermissions = PR_IRUSR|PR_IRGRP|PR_IROTH;         // all read
    if (isWritable)
        *aPermissions |= PR_IWUSR|PR_IWGRP|PR_IWOTH;    // all write
    if (isExecutable)
        *aPermissions |= PR_IXUSR|PR_IXGRP|PR_IXOTH;    // all execute

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetPermissionsOfLink(PRUint32 *aPermissions)
{
    NS_ENSURE_ARG(aPermissions);

    // The caller is assumed to have already called IsSymlink 
    // and to have found that this file is a link. It is not 
    // possible for a link file to be executable.

    DWORD word = GetFileAttributes(mWorkingPath.get());
    if (word == INVALID_FILE_ATTRIBUTES)
        return NS_ERROR_FILE_INVALID_PATH;

    PRBool isWritable = !(word & FILE_ATTRIBUTE_READONLY);
    *aPermissions = PR_IRUSR|PR_IRGRP|PR_IROTH;         // all read
    if (isWritable)
        *aPermissions |= PR_IWUSR|PR_IWGRP|PR_IWOTH;    // all write

    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::SetPermissions(PRUint32 aPermissions)
{
    // set the permissions of the target as determined by mFollowSymlinks
    // If PR_TRUE, then this will be for the target of the shortcut file, 
    // otherwise it will be for the shortcut file itself (i.e. the same 
    // results as SetPermissionsOfLink)
    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    // windows only knows about the following permissions
    int mode = 0;
    if (aPermissions & (PR_IRUSR|PR_IRGRP|PR_IROTH))    // any read
        mode |= _S_IREAD;
    if (aPermissions & (PR_IWUSR|PR_IWGRP|PR_IWOTH))    // any write
        mode |= _S_IWRITE;

    if (chmod(mResolvedPath.get(), mode) == -1)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetPermissionsOfLink(PRUint32 aPermissions)
{
    // The caller is assumed to have already called IsSymlink 
    // and to have found that this file is a link. 

    // windows only knows about the following permissions
    int mode = 0;
    if (aPermissions & (PR_IRUSR|PR_IRGRP|PR_IROTH))    // any read
        mode |= _S_IREAD;
    if (aPermissions & (PR_IWUSR|PR_IWGRP|PR_IWOTH))    // any write
        mode |= _S_IWRITE;

    if (chmod(mWorkingPath.get(), mode) == -1)
        return NS_ERROR_FAILURE;

    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::GetFileSize(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    *aFileSize = mFileInfo64.size;
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::GetFileSizeOfLink(PRInt64 *aFileSize)
{
    NS_ENSURE_ARG(aFileSize);

    // The caller is assumed to have already called IsSymlink 
    // and to have found that this file is a link. 

    PRFileInfo64 info;
    if (!PR_GetFileInfo64(mWorkingPath.get(), &info))
        return NS_ERROR_FILE_INVALID_PATH;

    *aFileSize = info.size;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetFileSize(PRInt64 aFileSize)
{
    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    HANDLE hFile = CreateFile(mResolvedPath.get(),      // pointer to name of the file
                              GENERIC_WRITE,            // access (write) mode
                              FILE_SHARE_READ,          // share mode
                              NULL,                     // pointer to security attributes
                              OPEN_EXISTING,            // how to create
                              FILE_ATTRIBUTE_NORMAL,    // file attributes
                              NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        return ConvertWinError(GetLastError());
    }

    // seek the file pointer to the new, desired end of file
    // and then truncate the file at that position
    rv = NS_ERROR_FAILURE;
    aFileSize = MyFileSeek64(hFile, aFileSize, FILE_BEGIN);
    if (aFileSize != -1 && SetEndOfFile(hFile))
    {
        MakeDirty();
        rv = NS_OK;
    }

    CloseHandle(hFile);
    return rv;
}

typedef BOOL (WINAPI *fpGetDiskFreeSpaceExA)(LPCTSTR lpDirectoryName,
                                             PULARGE_INTEGER lpFreeBytesAvailableToCaller,
                                             PULARGE_INTEGER lpTotalNumberOfBytes,
                                             PULARGE_INTEGER lpTotalNumberOfFreeBytes);

NS_IMETHODIMP
nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
    NS_ENSURE_ARG(aDiskSpaceAvailable);

    ResolveAndStat();

    // Attempt to check disk space using the GetDiskFreeSpaceExA function. 
    // --- FROM MSDN ---
    // The GetDiskFreeSpaceEx function is available beginning with Windows 95 OEM Service 
    // Release 2 (OSR2). To determine whether GetDiskFreeSpaceEx is available, call 
    // GetModuleHandle to get the handle to Kernel32.dll. Then you can call GetProcAddress. 
    // It is not necessary to call LoadLibrary on Kernel32.dll because it is already loaded 
    // into every process address space.
    fpGetDiskFreeSpaceExA pGetDiskFreeSpaceExA = (fpGetDiskFreeSpaceExA)
        GetProcAddress(GetModuleHandle("kernel32.dll"), "GetDiskFreeSpaceExA");
    if (pGetDiskFreeSpaceExA)
    {
        ULARGE_INTEGER liFreeBytesAvailableToCaller, liTotalNumberOfBytes;
        if (pGetDiskFreeSpaceExA(mResolvedPath.get(), &liFreeBytesAvailableToCaller, 
            &liTotalNumberOfBytes, NULL))
        {
            *aDiskSpaceAvailable = liFreeBytesAvailableToCaller.QuadPart;
            return NS_OK;
        }
    }

    // use the old method of getting available disk space
    char aDrive[_MAX_DRIVE + 2];
    _splitpath( mResolvedPath.get(), aDrive, NULL, NULL, NULL);
    strcat(aDrive, "\\");

    DWORD dwSecPerClus, dwBytesPerSec, dwFreeClus, dwTotalClus;
    if (GetDiskFreeSpace(aDrive, &dwSecPerClus, &dwBytesPerSec, &dwFreeClus, &dwTotalClus))
    {
        __int64 bytes = dwFreeClus;
        bytes *= dwSecPerClus;
        bytes *= dwBytesPerSec;

        *aDiskSpaceAvailable = bytes;
        return NS_OK;
    }

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::GetParent(nsIFile * *aParent)
{
    NS_ENSURE_ARG_POINTER(aParent);

    nsCAutoString parentPath(mWorkingPath);

    // cannot use nsCString::RFindChar() due to 0x5c problem
    PRInt32 offset = (PRInt32) (_mbsrchr((const unsigned char *) parentPath.get(), '\\')
                     - (const unsigned char *) parentPath.get());
    // adding this offset check that was removed in bug 241708 fixes mail
    // directories that aren't relative to/underneath the profile dir.
    // e.g., on a different drive. Before you remove them, please make
    // sure local mail directories that aren't underneath the profile dir work.
    if (offset < 0)
      return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    if (offset == 1 && parentPath[0] == '\\') {
        aParent = nsnull;
        return NS_OK;
    }
    if (offset > 0)
        parentPath.Truncate(offset);
    else
        parentPath.AssignLiteral("\\\\.");

    nsCOMPtr<nsILocalFile> localFile;
    nsresult rv = NS_NewNativeLocalFile(parentPath, mFollowSymlinks, getter_AddRefs(localFile));

    if(NS_SUCCEEDED(rv) && localFile)
    {
        return CallQueryInterface(localFile, aParent);
    }
    return rv;
}

NS_IMETHODIMP
nsLocalFile::Exists(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    MakeDirty();
    nsresult rv = ResolveAndStat();
    *_retval = NS_SUCCEEDED(rv);

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsWritable(PRBool *_retval)
{
    nsresult rv = HasFileAttribute(FILE_ATTRIBUTE_READONLY, _retval);
    if (NS_FAILED(rv))
        return rv;

    // writable if it doesn't have the readonly attribute
    *_retval = !*_retval;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsReadable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    *_retval = PR_TRUE;
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::IsExecutable(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;
    
    nsresult rv;

    // only files can be executables
    PRBool isFile;
    rv = IsFile(&isFile);
    if (NS_FAILED(rv))
        return rv;
    if (!isFile)
        return NS_OK;

    //TODO: shouldn't we be checking mFollowSymlinks here?
    PRBool symLink;
    rv = IsSymlink(&symLink);
    if (NS_FAILED(rv))
        return rv;

    nsCAutoString path;
    if (symLink)
        GetNativeTarget(path);
    else
        GetNativePath(path);

    // Get extension.
    char * ext = (char *) _mbsrchr((unsigned char *)path.BeginWriting(), '.');
    if ( ext ) {
        // Convert extension to lower case.
        for( unsigned char *p = (unsigned char *)ext; *p; p++ )
            *p = _mbctolower( *p );

        // Search for any of the set of executable extensions.
        const char * const executableExts[] = {
            ".ad",
            ".adp",
            ".asp",
            ".bas",
            ".bat",
            ".chm",
            ".cmd",
            ".com",
            ".cpl",
            ".crt",
            ".exe",
            ".hlp",
            ".hta",
            ".inf",
            ".ins",
            ".isp",
            ".js",
            ".jse",
            ".lnk",
            ".mdb",
            ".mde",
            ".msc",
            ".msi",
            ".msp",
            ".mst",
            ".pcd",
            ".pif",
            ".reg",
            ".scr",
            ".sct",
            ".shb",
            ".shs",
            ".url",
            ".vb",
            ".vbe",
            ".vbs",
            ".vsd",
            ".vss",
            ".vst",
            ".vsw",
            ".ws",
            ".wsc",
            ".wsf",
            ".wsh",
            0 };
        for ( int i = 0; executableExts[i]; i++ ) {
            if ( ::strcmp( executableExts[i], ext ) == 0 ) {
                // Found a match.  Set result and quit.
                *_retval = PR_TRUE;
                break;
            }
        }
    }

    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::IsDirectory(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    *_retval = (mFileInfo64.type == PR_FILE_DIRECTORY); 
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsFile(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    *_retval = (mFileInfo64.type == PR_FILE_FILE); 
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsHidden(PRBool *_retval)
{
    return HasFileAttribute(FILE_ATTRIBUTE_HIDDEN, _retval);
}

nsresult
nsLocalFile::HasFileAttribute(DWORD fileAttrib, PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    // get the file attributes for the correct item depending on following symlinks
    const char *filePath = mFollowSymlinks ? mResolvedPath.get() : mWorkingPath.get();
    DWORD word = GetFileAttributes(filePath);

    *_retval = ((word & fileAttrib) != 0);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSymlink(PRBool *_retval)
{
    NS_ENSURE_ARG(_retval);

    // unless it is a valid shortcut path it's not a symlink
    if (!IsShortcutPath(mWorkingPath.get()))
    {
        *_retval = PR_FALSE;
        return NS_OK;
    }

    // we need to know if this is a file or directory
    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    // it's only a shortcut if it is a file
    *_retval = (mFileInfo64.type == PR_FILE_FILE);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSpecial(PRBool *_retval)
{
    return HasFileAttribute(FILE_ATTRIBUTE_SYSTEM, _retval);
}

NS_IMETHODIMP
nsLocalFile::Equals(nsIFile *inFile, PRBool *_retval)
{
    NS_ENSURE_ARG(inFile);
    NS_ENSURE_ARG(_retval);

    nsCAutoString inFilePath;
    inFile->GetNativePath(inFilePath);

    *_retval = inFilePath.Equals(mWorkingPath);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Contains(nsIFile *inFile, PRBool recur, PRBool *_retval)
{
    *_retval = PR_FALSE;

    nsCAutoString myFilePath;
    if ( NS_FAILED(GetNativeTarget(myFilePath)))
        GetNativePath(myFilePath);

    PRInt32 myFilePathLen = myFilePath.Length();

    nsCAutoString inFilePath;
    if ( NS_FAILED(inFile->GetNativeTarget(inFilePath)))
        inFile->GetNativePath(inFilePath);

    if ( strnicmp( myFilePath.get(), inFilePath.get(), myFilePathLen) == 0)
    {
        // now make sure that the |inFile|'s path has a trailing
        // separator.

        if (inFilePath[myFilePathLen] == '\\')
        {
            *_retval = PR_TRUE;
        }

    }

    return NS_OK;
}



NS_IMETHODIMP
nsLocalFile::GetNativeTarget(nsACString &_retval)
{
    _retval.Truncate();
#if STRICT_FAKE_SYMLINKS
    PRBool symLink;

    nsresult rv = IsSymlink(&symLink);
    if (NS_FAILED(rv))
        return rv;

    if (!symLink)
    {
        return NS_ERROR_FILE_INVALID_PATH;
    }
#endif
    ResolveAndStat();

    _retval = mResolvedPath;
    return NS_OK;
}


/* attribute PRBool followLinks; */
NS_IMETHODIMP
nsLocalFile::GetFollowLinks(PRBool *aFollowLinks)
{
    *aFollowLinks = mFollowSymlinks;
    return NS_OK;
}
NS_IMETHODIMP
nsLocalFile::SetFollowLinks(PRBool aFollowLinks)
{
    MakeDirty();
    mFollowSymlinks = aFollowLinks;
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::GetDirectoryEntries(nsISimpleEnumerator * *entries)
{
    nsresult rv;

    *entries = nsnull;
    if (mWorkingPath.EqualsLiteral("\\\\.")) {
        nsDriveEnumerator *drives = new nsDriveEnumerator;
        if (!drives)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(drives);
        rv = drives->Init();
        if (NS_FAILED(rv)) {
            NS_RELEASE(drives);
            return rv;
        }
        *entries = drives;
        return NS_OK;
    }

    PRBool isDir;
    rv = IsDirectory(&isDir);
    if (NS_FAILED(rv))
        return rv;
    if (!isDir)
        return NS_ERROR_FILE_NOT_DIRECTORY;

    nsDirEnumerator* dirEnum = new nsDirEnumerator();
    if (dirEnum == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(dirEnum);
    rv = dirEnum->Init(this);
    if (NS_FAILED(rv))
    {
        NS_RELEASE(dirEnum);
        return rv;
    }

    *entries = dirEnum;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetPersistentDescriptor(nsACString &aPersistentDescriptor)
{
    return GetNativePath(aPersistentDescriptor);
}

NS_IMETHODIMP
nsLocalFile::SetPersistentDescriptor(const nsACString &aPersistentDescriptor)
{
   return InitWithNativePath(aPersistentDescriptor);
}

NS_IMETHODIMP
nsLocalFile::Reveal()
{
    // make sure mResolvedPath is set
    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv;

    // use the full path to explorer for security
    nsCOMPtr<nsILocalFile> winDir;
    rv = GetSpecialSystemDirectory(Win_WindowsDirectory, getter_AddRefs(winDir));
    NS_ENSURE_SUCCESS(rv, rv);
    nsCAutoString explorerPath;
    rv = winDir->GetNativePath(explorerPath);  
    NS_ENSURE_SUCCESS(rv, rv);
    explorerPath.Append("\\explorer.exe");

    // Always open a new window for files because Win2K doesn't appear to select
    // the file if a window showing that folder was already open. If the resolved 
    // path is a directory then instead of opening the parent and selecting it, 
    // we open the directory itself.
    nsCAutoString explorerParams;
    if (mFileInfo64.type != PR_FILE_DIRECTORY) // valid because we ResolveAndStat above
        explorerParams.Append("/n,/select,");
    explorerParams.Append('\"');
    explorerParams.Append(mResolvedPath);
    explorerParams.Append('\"');

    if (::ShellExecute(NULL, "open", explorerPath.get(), explorerParams.get(), 
            NULL, SW_SHOWNORMAL) <= (HINSTANCE) 32)
        return NS_ERROR_FAILURE;
 
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::Launch()
{
    const nsCString &path = mWorkingPath;

    // use the app registry name to launch a shell execute....
    LONG r = (LONG) ::ShellExecute( NULL, NULL, path.get(), NULL, NULL, SW_SHOWNORMAL);

    // if the file has no association, we launch windows' "what do you want to do" dialog
    if (r == SE_ERR_NOASSOC) {
        nsCAutoString shellArg;
        shellArg.Assign(NS_LITERAL_CSTRING("shell32.dll,OpenAs_RunDLL ") + path);
        r = (LONG) ::ShellExecute(NULL, NULL, "RUNDLL32.EXE",  shellArg.get(),
                                  NULL, SW_SHOWNORMAL);
    }
    if (r < 32) {
        switch (r) {
          case 0:
          case SE_ERR_OOM:
            return NS_ERROR_OUT_OF_MEMORY;
          case ERROR_FILE_NOT_FOUND:
            return NS_ERROR_FILE_NOT_FOUND;
          case ERROR_PATH_NOT_FOUND:
            return NS_ERROR_FILE_UNRECOGNIZED_PATH;
          case ERROR_BAD_FORMAT:
            return NS_ERROR_FILE_CORRUPTED;
          case SE_ERR_ACCESSDENIED:
            return NS_ERROR_FILE_ACCESS_DENIED;
          case SE_ERR_ASSOCINCOMPLETE:
          case SE_ERR_NOASSOC:
            return NS_ERROR_UNEXPECTED;
          case SE_ERR_DDEBUSY:
          case SE_ERR_DDEFAIL:
          case SE_ERR_DDETIMEOUT:
            return NS_ERROR_NOT_AVAILABLE;
          case SE_ERR_DLLNOTFOUND:
            return NS_ERROR_FAILURE;
          case SE_ERR_SHARE:
            return NS_ERROR_FILE_IS_LOCKED;
          default:
            return NS_ERROR_FILE_EXECUTION_FAILED;
        }
    }
    return NS_OK;
}

nsresult
NS_NewNativeLocalFile(const nsACString &path, PRBool followLinks, nsILocalFile* *result)
{
    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

    file->SetFollowLinks(followLinks);

    if (!path.IsEmpty()) {
        nsresult rv = file->InitWithNativePath(path);
        if (NS_FAILED(rv)) {
            NS_RELEASE(file);
            return rv;
        }
    }

    *result = file;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// UCS2 interface
//-----------------------------------------------------------------------------

nsresult
nsLocalFile::InitWithPath(const nsAString &filePath)
{
    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(filePath, tmp);
    if (NS_SUCCEEDED(rv))
        return InitWithNativePath(tmp);

    return rv;
}

nsresult
nsLocalFile::Append(const nsAString &node)
{
    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(node, tmp);
    if (NS_SUCCEEDED(rv))
        return AppendNative(tmp);

    return rv;
}

nsresult
nsLocalFile::AppendRelativePath(const nsAString &node)
{
    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(node, tmp);
    if (NS_SUCCEEDED(rv))
        return AppendRelativeNativePath(tmp);
    return rv;
}

nsresult
nsLocalFile::GetLeafName(nsAString &aLeafName)
{
    nsCAutoString tmp;
    nsresult rv = GetNativeLeafName(tmp);
    if (NS_SUCCEEDED(rv))
        rv = NS_CopyNativeToUnicode(tmp, aLeafName);

    return rv;
}

nsresult
nsLocalFile::SetLeafName(const nsAString &aLeafName)
{
    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(aLeafName, tmp);
    if (NS_SUCCEEDED(rv))
        return SetNativeLeafName(tmp);

    return rv;
}

nsresult
nsLocalFile::GetPath(nsAString &_retval)
{
    return NS_CopyNativeToUnicode(mWorkingPath, _retval);
}

nsresult
nsLocalFile::CopyTo(nsIFile *newParentDir, const nsAString &newName)
{
    if (newName.IsEmpty())
        return CopyToNative(newParentDir, nsCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(newName, tmp);
    if (NS_SUCCEEDED(rv))
        return CopyToNative(newParentDir, tmp);

    return rv;
}

nsresult
nsLocalFile::CopyToFollowingLinks(nsIFile *newParentDir, const nsAString &newName)
{
    if (newName.IsEmpty())
        return CopyToFollowingLinksNative(newParentDir, nsCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(newName, tmp);
    if (NS_SUCCEEDED(rv))
        return CopyToFollowingLinksNative(newParentDir, tmp);

    return rv;
}

nsresult
nsLocalFile::MoveTo(nsIFile *newParentDir, const nsAString &newName)
{
    if (newName.IsEmpty())
        return MoveToNative(newParentDir, nsCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(newName, tmp);
    if (NS_SUCCEEDED(rv))
        return MoveToNative(newParentDir, tmp);

    return rv;
}

nsresult
nsLocalFile::GetTarget(nsAString &_retval)
{
    nsCAutoString tmp;
    nsresult rv = GetNativeTarget(tmp);
    if (NS_SUCCEEDED(rv))
        rv = NS_CopyNativeToUnicode(tmp, _retval);

    return rv;
}

nsresult
NS_NewLocalFile(const nsAString &path, PRBool followLinks, nsILocalFile* *result)
{
    nsCAutoString buf;
    nsresult rv = NS_CopyUnicodeToNative(path, buf);
    if (NS_FAILED(rv)) {
        *result = nsnull;
        return rv;
    }
    return NS_NewNativeLocalFile(buf, followLinks, result);
}

//-----------------------------------------------------------------------------
// nsLocalFile <static members>
//-----------------------------------------------------------------------------

void
nsLocalFile::GlobalInit()
{
    nsresult rv = NS_CreateShortcutResolver();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Shortcut resolver could not be created");
}

void
nsLocalFile::GlobalShutdown()
{
    NS_DestroyShortcutResolver();
}

NS_IMPL_ISUPPORTS1(nsDriveEnumerator, nsISimpleEnumerator)

nsDriveEnumerator::nsDriveEnumerator()
 : mLetter(0)
{
}

nsDriveEnumerator::~nsDriveEnumerator()
{
}

nsresult nsDriveEnumerator::Init()
{
    /* If the length passed to GetLogicalDriveStrings is smaller
     * than the length of the string it would return, it returns
     * the length required for the string. */
    DWORD length = GetLogicalDriveStrings(0, 0);
    /* The string is null terminated */
    mDrives.SetLength(length+1);
    if (!GetLogicalDriveStrings(length, mDrives.BeginWriting()))
        return NS_ERROR_FAILURE;
    mLetter = mDrives.get();
    return NS_OK;
}

NS_IMETHODIMP nsDriveEnumerator::HasMoreElements(PRBool *aHasMore)
{
    *aHasMore = *mLetter != '\0';
    return NS_OK;
}

NS_IMETHODIMP nsDriveEnumerator::GetNext(nsISupports **aNext)
{
    /* GetLogicalDrives stored in mLetter is a concatenation
     * of null terminated strings, followed by a null terminator. */
    if (!*mLetter) {
        *aNext = nsnull;
        return NS_OK;
    }
    const char *drive = mLetter;
    mLetter += strlen(drive) + 1;
    nsILocalFile *file;
    nsresult rv = 
        NS_NewNativeLocalFile(nsDependentCString(drive), PR_FALSE, &file);

    *aNext = file;
    return rv;
}
