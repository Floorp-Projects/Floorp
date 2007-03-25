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
 *   Jungshik Shin <jshin@i18nl10n.com>
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
#include "nsIDirectoryEnumerator.h"
#include "nsNativeCharsetUtils.h"

#include "nsISimpleEnumerator.h"
#include "nsIComponentManager.h"
#include "prtypes.h"
#include "prio.h"
#include "private/pprio.h"  // To get PR_ImportFile
#include "prprf.h"
#include "prmem.h"
#include "nsHashKeys.h"

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

#include "nsTraceRefcntImpl.h"

#define CHECK_mWorkingPath()                    \
    PR_BEGIN_MACRO                              \
        if (mWorkingPath.IsEmpty())             \
            return NS_ERROR_NOT_INITIALIZED;    \
    PR_END_MACRO

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
#ifndef WINCE
class ShortcutResolver
{
public:
    ShortcutResolver();
    // nonvirtual since we're not subclassed
    ~ShortcutResolver();

    nsresult Init();
    nsresult Resolve(const WCHAR* in, WCHAR* out);

private:
    PRLock*       mLock;
    IPersistFile* mPersistFile;
    // Win 95 and 98 don't have IShellLinkW
    IShellLinkW*  mShellLink;
};

ShortcutResolver::ShortcutResolver()
{
    mLock = nsnull;
    mPersistFile = nsnull;
    mShellLink  = nsnull;
}

ShortcutResolver::~ShortcutResolver()
{
    if (mLock)
        PR_DestroyLock(mLock);

    // Release the pointer to the IPersistFile interface.
    if (mPersistFile)
        mPersistFile->Release();

    // Release the pointer to the IShellLink interface.
    if (mShellLink)
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

    HRESULT hres; 
    hres = CoCreateInstance(CLSID_ShellLink,
                            NULL,
                            CLSCTX_INPROC_SERVER,
                            IID_IShellLinkW,
                            (void**)&(mShellLink));
    if (SUCCEEDED(hres))
    {
        // Get a pointer to the IPersistFile interface.
        hres = mShellLink->QueryInterface(IID_IPersistFile,
                                          (void**)&mPersistFile);
    }

    if (mPersistFile == nsnull || mShellLink == nsnull)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

// |out| must be an allocated buffer of size MAX_PATH
nsresult
ShortcutResolver::Resolve(const WCHAR* in, WCHAR* out)
{
    nsAutoLock lock(mLock);

    // see if we can Load the path.
    HRESULT hres = mPersistFile->Load(in, STGM_READ);

    if (FAILED(hres))
        return NS_ERROR_FAILURE;

    // Resolve the link.
    hres = mShellLink->Resolve(nsnull, SLR_NO_UI);

    if (FAILED(hres))
        return NS_ERROR_FAILURE;

    // Get the path to the link target.
    hres = mShellLink->GetPath(out, MAX_PATH, NULL, SLGP_UNCPRIORITY);

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
#endif


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
        case ERROR_FILENAME_EXCED_RANGE:
            rv = NS_ERROR_FILE_NAME_TOO_LONG;
            break;
        case 0:
            rv = NS_OK;
            break;
        default:
            rv = NS_ERROR_FAILURE;
            break;
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
IsShortcutPath(const nsAString &path)
{
    // Under Windows, the shortcuts are just files with a ".lnk" extension. 
    // Note also that we don't resolve links in the middle of paths.
    // i.e. "c:\foo.lnk\bar.txt" is invalid.
    NS_ABORT_IF_FALSE(!path.IsEmpty(), "don't pass an empty string");
    PRInt32 len = path.Length();
    return (StringTail(path, 4).LowerCaseEqualsASCII(".lnk"));
}

//-----------------------------------------------------------------------------
// We need the following three definitions to make |OpenFile| convert a file 
// handle to an NSPR file descriptor correctly when |O_APPEND| flag is
// specified. It is defined in a private header of NSPR (primpl.h) we can't
// include. As a temporary workaround until we decide how to extend
// |PR_ImportFile|, we define it here. Currently, |_PR_HAVE_PEEK_BUFFER|
// and |PR_STRICT_ADDR_LEN| are not defined for the 'w95'-dependent portion
// of NSPR so that fields of |PRFilePrivate| #ifdef'd by them are not copied.
// Similarly, |_MDFileDesc| is taken from nsprpub/pr/include/md/_win95.h.
// In an unlikely case we switch to 'NT'-dependent NSPR AND this temporary 
// workaround last beyond the switch, |PRFilePrivate| and |_MDFileDesc| 
// need to be changed to match the definitions for WinNT.
//-----------------------------------------------------------------------------
typedef enum {
    _PR_TRI_TRUE = 1,
    _PR_TRI_FALSE = 0,
    _PR_TRI_UNKNOWN = -1
} _PRTriStateBool;

struct _MDFileDesc {
    PROsfd osfd;
};

struct PRFilePrivate {
    PRInt32 state;
    PRBool nonblocking;
    _PRTriStateBool inheritable;
    PRFileDesc *next;
    PRIntn lockCount;   /*   0: not locked
                         *  -1: a native lockfile call is in progress
                         * > 0: # times the file is locked */
    PRBool  appendMode; 
    _MDFileDesc md;
};

//-----------------------------------------------------------------------------
// Six static methods defined below (OpenFile,  FileTimeToPRTime, GetFileInfo,
// OpenDir, CloseDir, ReadDir) should go away once the corresponding 
// UTF-16 APIs are implemented on all the supported platforms (or at least 
// Windows 9x/ME) in NSPR. Currently, they're only implemented on 
// Windows NT4 or later. (bug 330665)
//-----------------------------------------------------------------------------

// copied from nsprpub/pr/src/{io/prfile.c | md/windows/w95io.c} : 
// PR_Open and _PR_MD_OPEN
static nsresult
OpenFile(const nsAFlatString &name, PRIntn osflags, PRIntn mode,
         PRFileDesc **fd)
{
    // XXX : 'mode' is not translated !!!
    PRInt32 access = 0;
    PRInt32 flags = 0;
    PRInt32 flag6 = 0;

    if (osflags & PR_SYNC) flag6 = FILE_FLAG_WRITE_THROUGH;
 
    if (osflags & PR_RDONLY || osflags & PR_RDWR)
        access |= GENERIC_READ;
    if (osflags & PR_WRONLY || osflags & PR_RDWR)
        access |= GENERIC_WRITE;

    if ( osflags & PR_CREATE_FILE && osflags & PR_EXCL )
        flags = CREATE_NEW;
    else if (osflags & PR_CREATE_FILE) {
        if (osflags & PR_TRUNCATE)
            flags = CREATE_ALWAYS;
        else
            flags = OPEN_ALWAYS;
    } else {
        if (osflags & PR_TRUNCATE)
            flags = TRUNCATE_EXISTING;
        else
            flags = OPEN_EXISTING;
    }

    HANDLE file = ::CreateFileW(name.get(), access,
                                FILE_SHARE_READ|FILE_SHARE_WRITE,
                                NULL, flags, flag6, NULL);

    if (file == INVALID_HANDLE_VALUE) { 
        *fd = nsnull;
        return ConvertWinError(GetLastError());
    }

    *fd = PR_ImportFile((PROsfd) file); 
    if (*fd) {
        // On Windows, _PR_HAVE_O_APPEND is not defined so that we have to
        // add it manually. (see |PR_Open| in nsprpub/pr/src/io/prfile.c)
        (*fd)->secret->appendMode = (PR_APPEND & osflags) ? PR_TRUE : PR_FALSE;
        return NS_OK;
    }

    nsresult rv = NS_ErrorAccordingToNSPR();

    CloseHandle(file);

    return rv;
}

// copied from nsprpub/pr/src/{io/prfile.c | md/windows/w95io.c} :
// PR_FileTimeToPRTime and _PR_FileTimeToPRTime
static
void FileTimeToPRTime(const FILETIME *filetime, PRTime *prtm)
{
#ifdef __GNUC__
    const PRTime _pr_filetime_offset = 116444736000000000LL;
#else
    const PRTime _pr_filetime_offset = 116444736000000000i64;
#endif

    PR_ASSERT(sizeof(FILETIME) == sizeof(PRTime));
    ::CopyMemory(prtm, filetime, sizeof(PRTime));
#ifdef __GNUC__
    *prtm = (*prtm - _pr_filetime_offset) / 10LL;
#else
    *prtm = (*prtm - _pr_filetime_offset) / 10i64;
#endif
}

// copied from nsprpub/pr/src/{io/prfile.c | md/windows/w95io.c} with some
// changes : PR_GetFileInfo64, _PR_MD_GETFILEINFO64
static nsresult
GetFileInfo(const nsAFlatString &name, PRFileInfo64 *info)
{
    WIN32_FILE_ATTRIBUTE_DATA fileData;

    if (name.IsEmpty() || name.FindCharInSet(L"?*") != kNotFound)
        return NS_ERROR_INVALID_ARG;

    if (!::GetFileAttributesExW(name.get(), GetFileExInfoStandard, &fileData))
        return ConvertWinError(GetLastError());

    if (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        info->type = PR_FILE_DIRECTORY;
    } else {
        info->type = PR_FILE_FILE;
    }

    info->size = fileData.nFileSizeHigh;
    info->size = (info->size << 32) + fileData.nFileSizeLow;

    FileTimeToPRTime(&fileData.ftLastWriteTime, &info->modifyTime);

    if (0 == fileData.ftCreationTime.dwLowDateTime &&
            0 == fileData.ftCreationTime.dwHighDateTime) {
        info->creationTime = info->modifyTime;
    } else {
        FileTimeToPRTime(&fileData.ftCreationTime, &info->creationTime);
    }

    return NS_OK;
}

struct nsDir
{
    HANDLE   handle; 
    WIN32_FIND_DATAW data;
    PRBool   firstEntry;
};

static nsresult
OpenDir(const nsAFlatString &name, nsDir * *dir)
{
    NS_ENSURE_ARG_POINTER(dir);

    *dir = nsnull;
    if (name.Length() + 3 >= MAX_PATH)
        return NS_ERROR_FILE_NAME_TOO_LONG;

    nsDir *d  = PR_NEW(nsDir);
    if (!d)
        return NS_ERROR_OUT_OF_MEMORY;

    nsAutoString filename(name);

     //If 'name' ends in a slash or backslash, do not append
     //another backslash.
    if (filename.Last() == L'/' || filename.Last() == L'\\')
        filename.AppendASCII("*");
    else 
        filename.AppendASCII("\\*");

    filename.ReplaceChar(L'/', L'\\');

    d->handle = ::FindFirstFileW(filename.get(), &(d->data) );

    if ( d->handle == INVALID_HANDLE_VALUE )
    {
        PR_Free(d);
        return ConvertWinError(GetLastError());
    }
    d->firstEntry = PR_TRUE;

    *dir = d;
    return NS_OK;
}

static nsresult
ReadDir(nsDir *dir, PRDirFlags flags, nsString& name)
{
    name.Truncate();
    NS_ENSURE_ARG(dir);

    while (1) {
        BOOL rv;
        if (dir->firstEntry)
        {
            dir->firstEntry = PR_FALSE;
            rv = 1;
        } else
            rv = ::FindNextFileW(dir->handle, &(dir->data));

        if (rv == 0)
            break;

        const PRUnichar *fileName;
        nsString tmp;
        fileName = (dir)->data.cFileName;

        if ((flags & PR_SKIP_DOT) &&
            (fileName[0] == L'.') && (fileName[1] == L'\0'))
            continue;
        if ((flags & PR_SKIP_DOT_DOT) &&
            (fileName[0] == L'.') && (fileName[1] == L'.') &&
            (fileName[2] == L'\0'))
            continue;

        DWORD attrib =  dir->data.dwFileAttributes;
        if ((flags & PR_SKIP_HIDDEN) && (attrib & FILE_ATTRIBUTE_HIDDEN))
            continue;

        if (fileName == tmp.get())
            name = tmp;
        else 
            name = fileName;
        return NS_OK;
    }

    DWORD err = GetLastError();
    return err == ERROR_NO_MORE_FILES ? NS_OK : ConvertWinError(err);
}

static nsresult
CloseDir(nsDir *d)
{
    NS_ENSURE_ARG(d);

    BOOL isOk = FindClose(d->handle);
    PR_DELETE(d);
    return isOk ? NS_OK : ConvertWinError(GetLastError());
}

//-----------------------------------------------------------------------------
// nsDirEnumerator
//-----------------------------------------------------------------------------

class nsDirEnumerator : public nsISimpleEnumerator,
                        public nsIDirectoryEnumerator
{
    public:

        NS_DECL_ISUPPORTS

        nsDirEnumerator() : mDir(nsnull)
        {
        }

        nsresult Init(nsILocalFile* parent)
        {
            nsAutoString filepath;
            parent->GetTarget(filepath);

            if (filepath.IsEmpty())
            {
                parent->GetPath(filepath);
            }

            if (filepath.IsEmpty())
            {
                return NS_ERROR_UNEXPECTED;
            }

            nsresult rv = OpenDir(filepath, &mDir);
            if (NS_FAILED(rv))
                return rv;

            mParent = parent;
            return NS_OK;
        }

        NS_IMETHOD HasMoreElements(PRBool *result)
        {
            nsresult rv;
            if (mNext == nsnull && mDir)
            {
                nsString name;
                rv = ReadDir(mDir, PR_SKIP_BOTH, name);
                if (NS_FAILED(rv))
                    return rv;
                if (name.IsEmpty()) 
                {
                    // end of dir entries
                    if (NS_FAILED(CloseDir(mDir)))
                        return NS_ERROR_FAILURE;

                    mDir = nsnull;

                    *result = PR_FALSE;
                    return NS_OK;
                }

                nsCOMPtr<nsIFile> file;
                rv = mParent->Clone(getter_AddRefs(file));
                if (NS_FAILED(rv))
                    return rv;

                rv = file->Append(name);
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
            if (!*result) 
                Close();
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

        NS_IMETHOD GetNextFile(nsIFile **result)
        {
            *result = nsnull;
            PRBool hasMore = PR_FALSE;
            nsresult rv = HasMoreElements(&hasMore);
            if (NS_FAILED(rv) || !hasMore)
                return rv;
            *result = mNext;
            NS_IF_ADDREF(*result);
            mNext = nsnull;
            return NS_OK;
        }

        NS_IMETHOD Close()
        {
            if (mDir)
            {
                nsresult rv = CloseDir(mDir);
                NS_ASSERTION(NS_SUCCEEDED(rv), "close failed");
                if (NS_FAILED(rv))
                    return NS_ERROR_FAILURE;
                mDir = nsnull;
            }
            return NS_OK;
        }

        // dtor can be non-virtual since there are no subclasses, but must be
        // public to use the class on the stack.
        ~nsDirEnumerator()
        {
            Close();
        }

    protected:
        nsDir*                  mDir;
        nsCOMPtr<nsILocalFile>  mParent;
        nsCOMPtr<nsILocalFile>  mNext;
};

NS_IMPL_ISUPPORTS2(nsDirEnumerator, nsISimpleEnumerator, nsIDirectoryEnumerator)


//-----------------------------------------------------------------------------
// nsLocalFile <public>
//-----------------------------------------------------------------------------

nsLocalFile::nsLocalFile()
  : mDirty(PR_TRUE)
  , mFollowSymlinks(PR_FALSE)
{
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

NS_IMPL_THREADSAFE_ISUPPORTS4(nsLocalFile,
                              nsILocalFile,
                              nsIFile,
                              nsILocalFileWin,
                              nsIHashable)


//-----------------------------------------------------------------------------
// nsLocalFile <private>
//-----------------------------------------------------------------------------

nsLocalFile::nsLocalFile(const nsLocalFile& other)
  : mDirty(PR_TRUE)
  , mFollowSymlinks(other.mFollowSymlinks)
  , mWorkingPath(other.mWorkingPath)
{
}

// Resolve the shortcut file from mWorkingPath and write the path 
// it points to into mResolvedPath.
nsresult
nsLocalFile::ResolveShortcut()
{
#ifndef WINCE
    // we can't do anything without the resolver
    if (!gResolver)
        return NS_ERROR_FAILURE;

    mResolvedPath.SetLength(MAX_PATH);
    if (mResolvedPath.Length() != MAX_PATH)
        return NS_ERROR_OUT_OF_MEMORY;

    PRUnichar *resolvedPath = mResolvedPath.BeginWriting();

    // resolve this shortcut
    nsresult rv = gResolver->Resolve(mWorkingPath.get(), resolvedPath);

    size_t len = NS_FAILED(rv) ? 0 : wcslen(resolvedPath);
    mResolvedPath.SetLength(len);

    return rv;
#else
    return NS_OK;
#endif
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
    nsAutoString nsprPath(mWorkingPath.get());
    if (mWorkingPath.Length() == 2 && mWorkingPath.CharAt(1) == L':') 
        nsprPath.AppendASCII("\\");

    // first we will see if the working path exists. If it doesn't then
    // there is nothing more that can be done
    if (NS_FAILED(GetFileInfo(nsprPath, &mFileInfo64)))
        return NS_ERROR_FILE_NOT_FOUND;

    // if this isn't a shortcut file or we aren't following symlinks then we're done 
    if (!mFollowSymlinks 
        || mFileInfo64.type != PR_FILE_FILE 
        || !IsShortcutPath(mWorkingPath))
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
    if (NS_FAILED(GetFileInfo(mResolvedPath, &mFileInfo64)))
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
nsLocalFile::InitWithFile(nsILocalFile *aFile)
{
    NS_ENSURE_ARG(aFile);
    
    nsAutoString path;
    aFile->GetPath(path);
    if (path.IsEmpty())
        return NS_ERROR_INVALID_ARG;
    return InitWithPath(path); 
}

NS_IMETHODIMP
nsLocalFile::InitWithPath(const nsAString &filePath)
{
    MakeDirty();

    nsAString::const_iterator begin, end;
    filePath.BeginReading(begin);
    filePath.EndReading(end);

    // input string must not be empty
    if (begin == end)
        return NS_ERROR_FAILURE;

    PRUnichar firstChar = *begin;
    PRUnichar secondChar = *(++begin);

    // just do a sanity check.  if it has any forward slashes, it is not a Native path
    // on windows.  Also, it must have a colon at after the first char.

    PRUnichar *path = nsnull;
    PRInt32 pathLen = 0;

    if ( ( (secondChar == L':') && !FindCharInReadable(L'/', begin, end) ) ||  // normal path
#ifdef WINCE
         ( (firstChar == L'\\') )   // wince absolute path or network path
#else
         ( (firstChar == L'\\') && (secondChar == L'\\') )   // network path
#endif
         )
    {
        // This is a native path
        path = ToNewUnicode(filePath);
        pathLen = filePath.Length();
    }

    if (path == nsnull) {
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }

    // kill any trailing '\'
    PRInt32 len = pathLen - 1;
    if (path[len] == L'\\')
    {
        path[len] = L'\0';
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

    return OpenFile(mResolvedPath, flags, mode, _retval);
}


NS_IMETHODIMP
nsLocalFile::OpenANSIFileDesc(const char *mode, FILE * *_retval)
{
    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv;

    *_retval = _wfopen(mResolvedPath.get(), NS_ConvertASCIItoUTF16(mode).get());
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

    PRUnichar* path = mResolvedPath.BeginWriting();

    if (path[0] == L'\\' && path[1] == L'\\')
    {
#ifdef WINCE
        ++path;
#else
        // dealing with a UNC path here; skip past '\\machine\'
        path = wcschr(path + 2, L'\\');
        if (!path)
            return NS_ERROR_FILE_INVALID_PATH;
        ++path;
#endif
    }

    // search for first slash after the drive (or volume) name
    PRUnichar* slash = wcschr(path, L'\\');

    if (slash)
    {
        // skip the first '\\'
        ++slash;
        slash = wcschr(slash, L'\\');

        while (slash)
        {
            *slash = L'\0';

            if (!::CreateDirectoryW(mResolvedPath.get(), NULL)) {
                rv = ConvertWinError(GetLastError());
                // perhaps the base path already exists, or perhaps we don't have
                // permissions to create the directory.  NOTE: access denied could
                // occur on a parent directory even though it exists.
                if (rv != NS_ERROR_FILE_ALREADY_EXISTS &&
                    rv != NS_ERROR_FILE_ACCESS_DENIED)
                    return rv;
            }
            *slash = L'\\';
            ++slash;
            slash = wcschr(slash, L'\\');
        }
    }

    if (type == NORMAL_FILE_TYPE)
    {
        PRFileDesc* file;
        OpenFile(mResolvedPath,
                 PR_RDONLY | PR_CREATE_FILE | PR_APPEND | PR_EXCL, attributes,
                 &file);
        if (!file) return NS_ERROR_FILE_ALREADY_EXISTS;

        PR_Close(file);
        return NS_OK;
    }

    if (type == DIRECTORY_TYPE)
    {
        if (!::CreateDirectoryW(mResolvedPath.get(), NULL))
            return ConvertWinError(GetLastError());
        else
            return NS_OK;
    }

    return NS_ERROR_FILE_UNKNOWN_TYPE;
}


NS_IMETHODIMP
nsLocalFile::Append(const nsAString &node)
{
    // append this path, multiple components are not permitted
    return AppendInternal(PromiseFlatString(node), PR_FALSE);
}

NS_IMETHODIMP
nsLocalFile::AppendRelativePath(const nsAString &node)
{
    // append this path, multiple components are permitted
    return AppendInternal(PromiseFlatString(node), PR_TRUE);
}


nsresult
nsLocalFile::AppendInternal(const nsAFlatString &node, PRBool multipleComponents)
{
    if (node.IsEmpty())
        return NS_OK;

    // check the relative path for validity
    if (node.First() == L'\\'                                   // can't start with an '\'
        || node.FindChar(L'/') != kNotFound                     // can't contain /
        || node.EqualsASCII(".."))                              // can't be ..
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

#ifndef WINCE  // who cares?
    if (multipleComponents)
    {
        // can't contain .. as a path component. Ensure that the valid components
        // "foo..foo", "..foo", and "foo.." are not falsely detected,
        // but the invalid paths "..\", "foo\..", "foo\..\foo", 
        // "..\foo", etc are.
        NS_NAMED_LITERAL_STRING(doubleDot, "\\.."); 
        nsAString::const_iterator start, end, offset;
        node.BeginReading(start);
        node.EndReading(end);
        offset = end; 
        while (FindInReadable(doubleDot, start, offset))
        {
            if (offset == end || *offset == L'\\')
                return NS_ERROR_FILE_UNRECOGNIZED_PATH;
            start = offset;
            offset = end;
        }
        
        // catches the remaining cases of prefixes 
        if (StringBeginsWith(node, NS_LITERAL_STRING("..\\")))
            return NS_ERROR_FILE_UNRECOGNIZED_PATH;
    }
    // single components can't contain '\'
    else if (node.FindChar(L'\\') != kNotFound)
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;
#endif

    MakeDirty();
    
    mWorkingPath.Append(NS_LITERAL_STRING("\\") + node);
    
    return NS_OK;
}

#define TOUPPER(u) (((u) >= L'a' && (u) <= L'z') ? \
                    (u) - (L'a' - L'A') : (u))

NS_IMETHODIMP
nsLocalFile::Normalize()
{
#ifndef WINCE
    // XXX See bug 187957 comment 18 for possible problems with this implementation.
    
    if (mWorkingPath.IsEmpty())
        return NS_OK;

    nsAutoString path(mWorkingPath);

    // find the index of the root backslash for the path. Everything before 
    // this is considered fully normalized and cannot be ascended beyond 
    // using ".."  For a local drive this is the first slash (e.g. "c:\").
    // For a UNC path it is the slash following the share name 
    // (e.g. "\\server\share\").
    PRInt32 rootIdx = 2;        // default to local drive
    if (path.First() == L'\\')   // if a share then calculate the rootIdx
    {
        rootIdx = path.FindChar(L'\\', 2);   // skip \\ in front of the server
        if (rootIdx == kNotFound)
            return NS_OK;                   // already normalized
        rootIdx = path.FindChar(L'\\', rootIdx+1);
        if (rootIdx == kNotFound)
            return NS_OK;                   // already normalized
    }
    else if (path.CharAt(rootIdx) != L'\\')
    {
        // The path has been specified relative to the current working directory 
        // for that drive. To normalize it, the current working directory for 
        // that drive needs to be inserted before the supplied relative path
        // which will provide an absolute path (and the rootIdx will still be 2).
        WCHAR cwd[MAX_PATH];
        WCHAR * pcwd = cwd;
        int drive = TOUPPER(path.First()) - 'A' + 1;
        if (!_wgetdcwd(drive, pcwd, MAX_PATH))
            pcwd = _wgetdcwd(drive, 0, 0);
        if (!pcwd)
            return NS_ERROR_OUT_OF_MEMORY;

        nsAutoString currentDir(pcwd);
        if (pcwd != cwd)
            free(pcwd);

        if (currentDir.Last() == '\\')
            path.Replace(0, 2, currentDir);
        else
            path.Replace(0, 2, currentDir + NS_LITERAL_STRING("\\"));
    }
    NS_POSTCONDITION(0 < rootIdx && rootIdx < (PRInt32)path.Length(), "rootIdx is invalid");
    NS_POSTCONDITION(path.CharAt(rootIdx) == '\\', "rootIdx is invalid");

    // if there is nothing following the root path then it is already normalized
    if (rootIdx + 1 == (PRInt32)path.Length())
        return NS_OK;

    // assign the root
    const PRUnichar * pathBuffer = path.get();  // simplify access to the buffer
    mWorkingPath.SetCapacity(path.Length()); // it won't ever grow longer
    mWorkingPath.Assign(pathBuffer, rootIdx);

    // Normalize the path components. The actions taken are:
    //
    //  "\\"    condense to single backslash
    //  "."     remove from path
    //  ".."    up a directory
    //  "..."   remove from path (any number of dots > 2)
    //
    // The last form is something that Windows 95 and 98 supported and 
    // is a shortcut for changing up multiple directories. Windows XP
    // and ilk ignore it in a path, as is done here.
    PRInt32 len, begin, end = rootIdx;
    while (end < (PRInt32)path.Length())
    {
        // find the current segment (text between the backslashes) to 
        // be examined, this will set the following variables:
        //  begin == index of first char in segment
        //  end   == index 1 char after last char in segment
        //  len   == length of segment 
        begin = end + 1;
        end = path.FindChar('\\', begin);
        if (end == kNotFound)
            end = path.Length();
        len = end - begin;
        
        // ignore double backslashes
        if (len == 0)
            continue;
        
        // len != 0, and interesting paths always begin with a dot
        if (pathBuffer[begin] == '.')
        {
            // ignore single dots
            if (len == 1)
                continue;   

            // handle multiple dots
            if (len >= 2 && pathBuffer[begin+1] == L'.')
            {
                // back up a path component on double dot
                if (len == 2)
                {
                    PRInt32 prev = mWorkingPath.RFindChar('\\');
                    if (prev >= rootIdx)
                        mWorkingPath.Truncate(prev);
                    continue;
                }

                // length is > 2 and the first two characters are dots. 
                // if the rest of the string is dots, then ignore it.
                int idx = len - 1;
                for (; idx >= 2; --idx) 
                {
                    if (pathBuffer[begin+idx] != L'.')
                        break;
                }

                // this is true if the loop above didn't break
                // and all characters in this segment are dots.
                if (idx < 2) 
                    continue;
            }
        }

        // add the current component to the path, including the preceding backslash
        mWorkingPath.Append(pathBuffer + begin - 1, len + 1);
    }

    // kill trailing dots and spaces.
    PRInt32 filePathLen = mWorkingPath.Length() - 1;
    while(filePathLen > 0 && (mWorkingPath[filePathLen] == L' ' ||
          mWorkingPath[filePathLen] == L'.'))
    {
        mWorkingPath.Truncate(filePathLen--);
    } 

    MakeDirty();
#else // WINCE
    // WINCE FIX
#endif 
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetLeafName(nsAString &aLeafName)
{
    aLeafName.Truncate();

    if(mWorkingPath.IsEmpty())
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    PRInt32 offset = mWorkingPath.RFindChar(L'\\');

    // if the working path is just a node without any lashes.
    if (offset == kNotFound)
        aLeafName = mWorkingPath;
    else
        aLeafName = Substring(mWorkingPath, offset + 1);

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetLeafName(const nsAString &aLeafName)
{
    MakeDirty();

    if(mWorkingPath.IsEmpty())
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    // cannot use nsCString::RFindChar() due to 0x5c problem
    PRInt32 offset = mWorkingPath.RFindChar(L'\\');
    if (offset)
    {
        mWorkingPath.Truncate(offset+1);
    }
    mWorkingPath.Append(aLeafName);

    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::GetPath(nsAString &_retval)
{
    _retval = mWorkingPath;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetCanonicalPath(nsAString &aResult)
{
    EnsureShortPath();
    aResult.Assign(mShortWorkingPath);
    return NS_OK;
}

typedef struct {
    WORD wLanguage;
    WORD wCodePage;
} LANGANDCODEPAGE;

NS_IMETHODIMP
nsLocalFile::GetVersionInfoField(const char* aField, nsAString& _retval)
{
    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    rv = NS_ERROR_FAILURE;

    // Cast away const-ness here because WinAPI functions don't understand it, 
    // the path is used for [in] parameters only however so it's safe. 
    WCHAR *path = NS_CONST_CAST(WCHAR*, mFollowSymlinks ? mResolvedPath.get() 
                                                        : mWorkingPath.get());

    DWORD dummy;
    DWORD size = ::GetFileVersionInfoSizeW(path, &dummy);
    if (!size)
        return rv;

    void* ver = calloc(size, 1);
    if (!ver)
        return NS_ERROR_OUT_OF_MEMORY;

    if (::GetFileVersionInfoW(path, 0, size, ver)) 
    {
        LANGANDCODEPAGE* translate = nsnull;
        UINT pageCount;
        BOOL queryResult = ::VerQueryValueW(ver, L"\\VarFileInfo\\Translation", 
                                            (void**)&translate, &pageCount);
        if (queryResult && translate) 
        {
            for (PRInt32 i = 0; i < 2; ++i) 
            { 
                PRUnichar subBlock[MAX_PATH];
                _snwprintf(subBlock, MAX_PATH,
                           L"\\StringFileInfo\\%04x%04x\\%s", 
                           (i == 0 ? translate[0].wLanguage 
                                   : ::GetUserDefaultLangID()),
                           translate[0].wCodePage,
                           NS_ConvertASCIItoUTF16(
                               nsDependentCString(aField)).get());
                subBlock[MAX_PATH - 1] = 0;
                LPVOID value = nsnull;
                UINT size;
                queryResult = ::VerQueryValueW(ver, subBlock, &value, &size);
                if (queryResult && value)
                {
                    _retval.Assign(NS_STATIC_CAST(PRUnichar*, value));
                    if (!_retval.IsEmpty()) 
                    {
                        rv = NS_OK;
                        break;
                    }
                }
            }
        }
    }
    free(ver);
    
    return rv;
}

nsresult
nsLocalFile::CopySingleFile(nsIFile *sourceFile, nsIFile *destParent,
                            const nsAString &newName, 
                            PRBool followSymlinks, PRBool move)
{
    nsresult rv;
    nsAutoString filePath;

    // get the path that we are going to copy to.
    // Since windows does not know how to auto
    // resolve shortcuts, we must work with the
    // target.
    nsAutoString destPath;
    destParent->GetTarget(destPath);

    destPath.AppendASCII("\\");

    if (newName.IsEmpty())
    {
        nsAutoString aFileName;
        sourceFile->GetLeafName(aFileName);
        destPath.Append(aFileName);
    }
    else
    {
        destPath.Append(newName);
    }


    if (followSymlinks)
    {
        rv = sourceFile->GetTarget(filePath);
        if (filePath.IsEmpty())
            rv = sourceFile->GetPath(filePath);
    }
    else
    {
        rv = sourceFile->GetPath(filePath);
    }

    if (NS_FAILED(rv))
        return rv;

    int copyOK;

    if (!move)
        copyOK = ::CopyFileW(filePath.get(), destPath.get(), PR_TRUE);
    else
    {
        // What we have to do is check to see if the destPath exists.  If it
        // does, we have to move it out of the say so that MoveFile will
        // succeed.  However, we don't want to just remove it since MoveFile
        // can fail leaving us without a file.

        nsAutoString backup;
        PRFileInfo64 fileInfo64;
        if (NS_SUCCEEDED(GetFileInfo(destPath, &fileInfo64)))
        {

            // the file exists.  Check to make sure it is not a directory,
            // then move it out of the way.
            if (fileInfo64.type == PR_FILE_FILE)
            {
                backup.Append(destPath);
                backup.Append(L".moztmp");

                // we are about to remove the .moztmp file,
                // so attempt to make sure the file is writable
                // (meaning:  the "read only" attribute is not set)
                // _wchmod can silently fail (return -1) if 
                // the file doesn't exist but that's ok, because 
                // _wremove() will also silently fail if the file
                // doesn't exist.
               (void)_wchmod(backup.get(), _S_IREAD | _S_IWRITE);

                // remove any existing backup file that we may already have.
                // maybe we should be doing some kind of unique naming here,
                // but why bother.
               (void)_wremove(backup.get());

                // move destination file to backup file
                copyOK = ::MoveFileW(destPath.get(), backup.get());
                if (!copyOK)
                {
                    // I guess we can't do the backup copy, so return.
                    rv = ConvertWinError(GetLastError());
                    return rv;
                }
            }
        }
        // move source file to destination file
        copyOK = ::MoveFileW(filePath.get(), destPath.get());

        if (!backup.IsEmpty())
        {
            if (copyOK)
            {
                // remove the backup copy.
                _wremove(backup.get());
            }
            else
            {
                // restore backup
                int backupOk = ::MoveFileW(backup.get(), destPath.get());
                NS_ASSERTION(backupOk, "move backup failed");
            }
        }
    }
    if (!copyOK)  // CopyFile and MoveFile returns non-zero if succeeds (backward if you ask me).
        rv = ConvertWinError(GetLastError());

    return rv;
}

nsresult
nsLocalFile::CopyMove(nsIFile *aParentDir, const nsAString &newName, PRBool followSymlinks, PRBool move)
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
                    nsAutoString target;
                    newParentDir->GetTarget(target);

                    nsCOMPtr<nsILocalFile> realDest = new nsLocalFile();
                    if (realDest == nsnull)
                        return NS_ERROR_OUT_OF_MEMORY;

                    rv = realDest->InitWithPath(target);

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

    // Try different ways to move/copy files/directories
    PRBool done = PR_FALSE;
    PRBool isDir;
    IsDirectory(&isDir);
    PRBool isSymlink;
    IsSymlink(&isSymlink);

    // Try to move the file or directory, or try to copy a single file (or non-followed symlink)
    if (move || !isDir || (isSymlink && !followSymlinks))
    {
        // Copy/Move single file, or move a directory
        rv = CopySingleFile(this, newParentDir, newName, followSymlinks, move);
        done = NS_SUCCEEDED(rv);
        // If we are moving a directory and that fails, fallback on directory
        // enumeration.  See bug 231300 for details.
        if (!done && !(move && isDir))
            return rv;  
    }
    
    // Not able to copy or move directly, so enumerate it
    if (!done)
    {
        // create a new target destination in the new parentDir;
        nsCOMPtr<nsIFile> target;
        rv = newParentDir->Clone(getter_AddRefs(target));

        if (NS_FAILED(rv))
            return rv;

        nsAutoString allocatedNewName;
        if (newName.IsEmpty())
        {
            PRBool isLink;
            IsSymlink(&isLink);
            if (isLink)
            {
                nsAutoString temp;
                GetTarget(temp);
                PRInt32 offset = temp.RFindChar(L'\\'); 
                if (offset == kNotFound)
                    allocatedNewName = temp;
                else 
                    allocatedNewName = Substring(temp, offset + 1);
            }
            else
            {
                GetLeafName(allocatedNewName);// this should be the leaf name of the
            }
        }
        else
        {
            allocatedNewName = newName;
        }

        rv = target->Append(allocatedNewName);
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
            NS_WARNING("dirEnum initialization failed");
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

                    rv = file->MoveTo(target, EmptyString());
                    NS_ENSURE_SUCCESS(rv,rv);
                }
                else
                {
                    if (followSymlinks)
                        rv = file->CopyToFollowingLinks(target, EmptyString());
                    else
                        rv = file->CopyTo(target, EmptyString());
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

        nsAutoString newParentPath;
        newParentDir->GetPath(newParentPath);

        if (newParentPath.IsEmpty())
            return NS_ERROR_FAILURE;

        if (newName.IsEmpty())
        {
            nsAutoString aFileName;
            GetLeafName(aFileName);

            InitWithPath(newParentPath);
            Append(aFileName);
        }
        else
        {
            InitWithPath(newParentPath);
            Append(newName);
        }
    }

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::CopyTo(nsIFile *newParentDir, const nsAString &newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE, PR_FALSE);
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinks(nsIFile *newParentDir, const nsAString &newName)
{
    return CopyMove(newParentDir, newName, PR_TRUE, PR_FALSE);
}

NS_IMETHODIMP
nsLocalFile::MoveTo(nsIFile *newParentDir, const nsAString &newName)
{
    return CopyMove(newParentDir, newName, PR_FALSE, PR_TRUE);
}


NS_IMETHODIMP
nsLocalFile::Load(PRLibrary * *_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    PRBool isFile;
    nsresult rv = IsFile(&isFile);

    if (NS_FAILED(rv))
        return rv;

    if (! isFile)
        return NS_ERROR_FILE_IS_DIRECTORY;

    NS_TIMELINE_START_TIMER("PR_LoadLibraryWithFlags");

#ifdef NS_BUILD_REFCNT_LOGGING
    nsTraceRefcntImpl::SetActivityIsLegal(PR_FALSE);
#endif

    PRLibSpec libSpec;
    libSpec.value.pathname_u = mResolvedPath.get();
    libSpec.type = PR_LibSpec_PathnameU;
    *_retval =  PR_LoadLibraryWithFlags(libSpec, 0);

#ifdef NS_BUILD_REFCNT_LOGGING
    nsTraceRefcntImpl::SetActivityIsLegal(PR_TRUE);
#endif

    NS_TIMELINE_STOP_TIMER("PR_LoadLibraryWithFlags");
    NS_TIMELINE_MARK_TIMER1("PR_LoadLibraryWithFlags",
                            NS_ConvertUTF16toUTF8(mResolvedPath).get());

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

    // Check we are correctly initialized.
    CHECK_mWorkingPath();

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
        rv = _wrmdir(mWorkingPath.get());
    }
    else
    {
        rv = _wremove(mWorkingPath.get());
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
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

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
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG(aLastModifiedTime);
 
    // The caller is assumed to have already called IsSymlink 
    // and to have found that this file is a link. 

    PRFileInfo64 info;
    nsresult rv = GetFileInfo(mWorkingPath, &info);
    if (NS_FAILED(rv)) 
        return rv;

    // microseconds -> milliseconds
    PRInt64 usecPerMsec;
    LL_I2L(usecPerMsec, PR_USEC_PER_MSEC);
    LL_DIV(*aLastModifiedTime, info.modifyTime, usecPerMsec);
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::SetLastModifiedTime(PRInt64 aLastModifiedTime)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

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
nsLocalFile::SetModDate(PRInt64 aLastModifiedTime, const PRUnichar *filePath)
{
    HANDLE file = ::CreateFileW(filePath,          // pointer to name of the file
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
    // if at least one of these fails...
    if (!(SystemTimeToFileTime(&st, &lft) != 0 &&
          LocalFileTimeToFileTime(&lft, &ft) != 0 &&
          SetFileTime(file, NULL, &ft, &ft) != 0))
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
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG(aPermissions);

    // The caller is assumed to have already called IsSymlink 
    // and to have found that this file is a link. It is not 
    // possible for a link file to be executable.

    DWORD word = ::GetFileAttributesW(mWorkingPath.get());
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
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

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

    if (_wchmod(mResolvedPath.get(), mode) == -1)
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

    if (_wchmod(mWorkingPath.get(), mode) == -1)
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
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG(aFileSize);

    // The caller is assumed to have already called IsSymlink 
    // and to have found that this file is a link. 

    PRFileInfo64 info;
    if (NS_FAILED(GetFileInfo(mWorkingPath, &info)))
        return NS_ERROR_FILE_INVALID_PATH;

    *aFileSize = info.size;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetFileSize(PRInt64 aFileSize)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    nsresult rv = ResolveAndStat();
    if (NS_FAILED(rv))
        return rv;

    HANDLE hFile = ::CreateFileW(mResolvedPath.get(),// pointer to name of the file
                                 GENERIC_WRITE,      // access (write) mode
                                 FILE_SHARE_READ,    // share mode
                                 NULL,               // pointer to security attributes
                                 OPEN_EXISTING,          // how to create
                                 FILE_ATTRIBUTE_NORMAL,  // file attributes
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

NS_IMETHODIMP
nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

#ifndef WINCE
    NS_ENSURE_ARG(aDiskSpaceAvailable);

    ResolveAndStat();

    ULARGE_INTEGER liFreeBytesAvailableToCaller, liTotalNumberOfBytes;
    if (::GetDiskFreeSpaceExW(mResolvedPath.get(), &liFreeBytesAvailableToCaller, 
                              &liTotalNumberOfBytes, NULL))
    {
        *aDiskSpaceAvailable = liFreeBytesAvailableToCaller.QuadPart;
        return NS_OK;
    }
#endif
    // WINCE FIX
    *aDiskSpaceAvailable = 0;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetParent(nsIFile * *aParent)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG_POINTER(aParent);

    // A two-character path must be a drive such as C:, so it has no parent
    if (mWorkingPath.Length() == 2) {
        *aParent = nsnull;
        return NS_OK;
    }

    PRInt32 offset = mWorkingPath.RFindChar(PRUnichar('\\'));
    // adding this offset check that was removed in bug 241708 fixes mail
    // directories that aren't relative to/underneath the profile dir.
    // e.g., on a different drive. Before you remove them, please make
    // sure local mail directories that aren't underneath the profile dir work.
    if (offset == kNotFound)
      return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    // A path of the form \\NAME is a top-level path and has no parent
    if (offset == 1 && mWorkingPath[0] == L'\\') {
        *aParent = nsnull;
        return NS_OK;
    }

    nsAutoString parentPath(mWorkingPath);

    if (offset > 0)
        parentPath.Truncate(offset);
    else
        parentPath.AssignLiteral("\\\\.");

    nsCOMPtr<nsILocalFile> localFile;
    nsresult rv = NS_NewLocalFile(parentPath, mFollowSymlinks, getter_AddRefs(localFile));

    if(NS_SUCCEEDED(rv) && localFile)
    {
        return CallQueryInterface(localFile, aParent);
    }
    return rv;
}

NS_IMETHODIMP
nsLocalFile::Exists(PRBool *_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG(_retval);
    *_retval = PR_FALSE;

    MakeDirty();
    nsresult rv = ResolveAndStat();
    *_retval = NS_SUCCEEDED(rv);

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsWritable(PRBool *aIsWritable)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    //TODO: extend to support NTFS file permissions

    // The read-only attribute on a FAT directory only means that it can't 
    // be deleted. It is still possible to modify the contents of the directory.
    nsresult rv = IsDirectory(aIsWritable);
    if (NS_FAILED(rv))
        return rv;
    if (*aIsWritable)
        return NS_OK;

    // writable if the file doesn't have the readonly attribute
    rv = HasFileAttribute(FILE_ATTRIBUTE_READONLY, aIsWritable);
    if (NS_FAILED(rv))
        return rv;
    *aIsWritable = !*aIsWritable;

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsReadable(PRBool *_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

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
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

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

    nsAutoString path;
    if (symLink)
        GetTarget(path);
    else
        GetPath(path);

    // kill trailing dots and spaces.
    PRInt32 filePathLen = path.Length() - 1;
    while(filePathLen > 0 && (path[filePathLen] == L' ' || path[filePathLen] == L'.'))
    {
        path.Truncate(filePathLen--);
    } 

    // Get extension.
    PRInt32 dotIdx = path.RFindChar(PRUnichar('.'));
    if ( dotIdx != kNotFound ) {
        // Convert extension to lower case.
        PRUnichar *p = path.BeginWriting();
        for( p+= dotIdx + 1; *p; p++ )
            *p +=  (*p >= L'A' && *p <= L'Z') ? 'a' - 'A' : 0; 
        
        // Search for any of the set of executable extensions.
        const char * const executableExts[] = {
            "ad",
            "ade",         // access project extension
            "adp",
            "app",         // executable application
            "application", // from bug 348763
            "asp",
            "bas",
            "bat",
            "chm",
            "cmd",
            "com",
            "cpl",
            "crt",
            "exe",
            "fxp",         // FoxPro compiled app
            "hlp",
            "hta",
            "inf",
            "ins",
            "isp",
            "js",
            "jse",
            "lnk",
            "mad",         // Access Module Shortcut
            "maf",         // Access
            "mag",         // Access Diagram Shortcut
            "mam",         // Access Macro Shortcut
            "maq",         // Access Query Shortcut
            "mar",         // Access Report Shortcut
            "mas",         // Access Stored Procedure
            "mat",         // Access Table Shortcut
            "mau",         // Media Attachment Unit
            "mav",         // Access View Shortcut
            "maw",         // Access Data Access Page
            "mda",         // Access Add-in, MDA Access 2 Workgroup
            "mdb",
            "mde",
            "mdt",         // Access Add-in Data
            "mdw",         // Access Workgroup Information
            "mdz",         // Access Wizard Template
            "msc",
            "msh",         // Microsoft Shell
            "mshxml",      // Microsoft Shell
            "msi",
            "msp",
            "mst",
            "ops",         // Office Profile Settings
            "pcd",
            "pif",
            "plg",         // Developer Studio Build Log
            "prf",         // windows system file
            "prg",
            "pst",
            "reg",
            "scf",         // Windows explorer command
            "scr",
            "sct",
            "shb",
            "shs",
            "url",
            "vb",
            "vbe",
            "vbs",
            "vsd",
            "vsmacros",    // Visual Studio .NET Binary-based Macro Project
            "vss",
            "vst",
            "vsw",
            "ws",
            "wsc",
            "wsf",
            "wsh",
            0 };
        for ( int i = 0; executableExts[i]; i++ ) {
            if ( Substring(path, dotIdx + 1).EqualsASCII(executableExts[i])) {
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
    const PRUnichar *filePath = mFollowSymlinks ? 
                                mResolvedPath.get() : mWorkingPath.get();
    DWORD word = ::GetFileAttributesW(filePath);

    *_retval = ((word & fileAttrib) != 0);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSymlink(PRBool *_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG(_retval);

    // unless it is a valid shortcut path it's not a symlink
    if (!IsShortcutPath(mWorkingPath))
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

    EnsureShortPath();

    nsCOMPtr<nsILocalFileWin> lf(do_QueryInterface(inFile));
    if (!lf) {
        *_retval = PR_FALSE;
        return NS_OK;
    }

    nsAutoString inFilePath;
    lf->GetCanonicalPath(inFilePath);

    // Ok : Win9x
    *_retval = _wcsicmp(mShortWorkingPath.get(), inFilePath.get()) == 0; 

    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::Contains(nsIFile *inFile, PRBool recur, PRBool *_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    *_retval = PR_FALSE;

    nsAutoString myFilePath;
    if (NS_FAILED(GetTarget(myFilePath)))
        GetPath(myFilePath);

    PRUint32 myFilePathLen = myFilePath.Length();

    nsAutoString inFilePath;
    if (NS_FAILED(inFile->GetTarget(inFilePath)))
        inFile->GetPath(inFilePath);

    // make sure that the |inFile|'s path has a trailing separator.
    if (inFilePath.Length() >= myFilePathLen && inFilePath[myFilePathLen] == L'\\')
    {
        if (_wcsnicmp(myFilePath.get(), inFilePath.get(), myFilePathLen) == 0)
        {
            *_retval = PR_TRUE;
        }

    }

    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::GetTarget(nsAString &_retval)
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
    CopyUTF16toUTF8(mWorkingPath, aPersistentDescriptor);
    return NS_OK;
}   
    
NS_IMETHODIMP
nsLocalFile::SetPersistentDescriptor(const nsACString &aPersistentDescriptor)
{
    if (IsUTF8(aPersistentDescriptor))
        return InitWithPath(NS_ConvertUTF8toUTF16(aPersistentDescriptor));
    else
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
    nsAutoString explorerPath;
    rv = winDir->GetPath(explorerPath);  
    NS_ENSURE_SUCCESS(rv, rv);
    explorerPath.Append(L"\\explorer.exe");

    // Always open a new window for files because Win2K doesn't appear to select
    // the file if a window showing that folder was already open. If the resolved 
    // path is a directory then instead of opening the parent and selecting it, 
    // we open the directory itself.
    nsAutoString explorerParams;
    if (mFileInfo64.type != PR_FILE_DIRECTORY) // valid because we ResolveAndStat above
        explorerParams.Append(L"/n,/select,");
    explorerParams.Append(L'\"');
    explorerParams.Append(mResolvedPath);
    explorerParams.Append(L'\"');

    if (::ShellExecuteW(NULL, L"open", explorerPath.get(), explorerParams.get(),
                        NULL, SW_SHOWNORMAL) <= (HINSTANCE) 32)
        return NS_ERROR_FAILURE;

    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::Launch()
{
    const nsString &path = mWorkingPath;

    // use the app registry name to launch a shell execute....
    LONG r = (LONG) ::ShellExecuteW(NULL, NULL, path.get(), NULL, NULL,
                                    SW_SHOWNORMAL);

    // if the file has no association, we launch windows' "what do you want to do" dialog
    if (r == SE_ERR_NOASSOC) {
        nsAutoString shellArg;
        shellArg.Assign(NS_LITERAL_STRING("shell32.dll,OpenAs_RunDLL ") + path);
        r = (LONG) ::ShellExecuteW(NULL, NULL, L"RUNDLL32.EXE", shellArg.get(),
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
NS_NewLocalFile(const nsAString &path, PRBool followLinks, nsILocalFile* *result)
{
    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

    file->SetFollowLinks(followLinks);

    if (!path.IsEmpty()) {
        nsresult rv = file->InitWithPath(path);
        if (NS_FAILED(rv)) {
            NS_RELEASE(file);
            return rv;
        }
    }

    *result = file;
    return NS_OK;
}

//-----------------------------------------------------------------------------
// Native (lossy) interface
//-----------------------------------------------------------------------------
  
NS_IMETHODIMP
nsLocalFile::InitWithNativePath(const nsACString &filePath)
{
   nsAutoString tmp;
   nsresult rv = NS_CopyNativeToUnicode(filePath, tmp);
   if (NS_SUCCEEDED(rv))
       return InitWithPath(tmp);

   return rv;
}

NS_IMETHODIMP
nsLocalFile::AppendNative(const nsACString &node)
{
    nsAutoString tmp;
    nsresult rv = NS_CopyNativeToUnicode(node, tmp);
    if (NS_SUCCEEDED(rv))
        return Append(tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::AppendRelativeNativePath(const nsACString &node)
{
    nsAutoString tmp;
    nsresult rv = NS_CopyNativeToUnicode(node, tmp);
    if (NS_SUCCEEDED(rv))
        return AppendRelativePath(tmp);
    return rv;
}


NS_IMETHODIMP
nsLocalFile::GetNativeLeafName(nsACString &aLeafName)
{
    //NS_WARNING("This API is lossy. Use GetLeafName !");
    nsAutoString tmp;
    nsresult rv = GetLeafName(tmp);
    if (NS_SUCCEEDED(rv))
        rv = NS_CopyUnicodeToNative(tmp, aLeafName);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::SetNativeLeafName(const nsACString &aLeafName)
{
    nsAutoString tmp;
    nsresult rv = NS_CopyNativeToUnicode(aLeafName, tmp);
    if (NS_SUCCEEDED(rv))
        return SetLeafName(tmp);

    return rv;
}


NS_IMETHODIMP
nsLocalFile::GetNativePath(nsACString &_retval)
{
    //NS_WARNING("This API is lossy. Use GetPath !");
    nsAutoString tmp;
    nsresult rv = GetPath(tmp);
    if (NS_SUCCEEDED(rv))
        rv = NS_CopyUnicodeToNative(tmp, _retval);

    return rv;
}


NS_IMETHODIMP
nsLocalFile::GetNativeCanonicalPath(nsACString &aResult)
{
    NS_WARNING("This method is lossy. Use GetCanoincailPath !");
    EnsureShortPath();
    NS_CopyUnicodeToNative(mShortWorkingPath, aResult);
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::CopyToNative(nsIFile *newParentDir, const nsACString &newName)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    if (newName.IsEmpty())
        return CopyTo(newParentDir, EmptyString());

    nsAutoString tmp;
    nsresult rv = NS_CopyNativeToUnicode(newName, tmp);
    if (NS_SUCCEEDED(rv))
        return CopyTo(newParentDir, tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinksNative(nsIFile *newParentDir, const nsACString &newName)
{
    if (newName.IsEmpty())
        return CopyToFollowingLinks(newParentDir, EmptyString());

    nsAutoString tmp;
    nsresult rv = NS_CopyNativeToUnicode(newName, tmp);
    if (NS_SUCCEEDED(rv))
        return CopyToFollowingLinks(newParentDir, tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::MoveToNative(nsIFile *newParentDir, const nsACString &newName)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    if (newName.IsEmpty())
        return MoveTo(newParentDir, EmptyString());

    nsAutoString tmp;
    nsresult rv = NS_CopyNativeToUnicode(newName, tmp);
    if (NS_SUCCEEDED(rv))
        return MoveTo(newParentDir, tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::GetNativeTarget(nsACString &_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_WARNING("This API is lossy. Use GetTarget !");
    nsAutoString tmp;
    nsresult rv = GetTarget(tmp);
    if (NS_SUCCEEDED(rv))
        rv = NS_CopyUnicodeToNative(tmp, _retval);

    return rv;
}

nsresult
NS_NewNativeLocalFile(const nsACString &path, PRBool followLinks, nsILocalFile* *result)
{
    nsAutoString buf;
    nsresult rv = NS_CopyNativeToUnicode(path, buf);
    if (NS_FAILED(rv)) {
        *result = nsnull;
        return rv;
    }
    return NS_NewLocalFile(buf, followLinks, result);
}

void
nsLocalFile::EnsureShortPath()
{
    if (!mShortWorkingPath.IsEmpty())
        return;

    WCHAR thisshort[MAX_PATH];
    DWORD thisr = ::GetShortPathNameW(mWorkingPath.get(), thisshort,
                                      sizeof(thisshort));
    if (thisr < sizeof(thisshort))
        mShortWorkingPath.Assign(thisshort);
    else
        mShortWorkingPath.Assign(mWorkingPath);
}

// nsIHashable

NS_IMETHODIMP
nsLocalFile::Equals(nsIHashable* aOther, PRBool *aResult)
{
    nsCOMPtr<nsIFile> otherfile(do_QueryInterface(aOther));
    if (!otherfile) {
        *aResult = PR_FALSE;
        return NS_OK;
    }

    return Equals(otherfile, aResult);
}

NS_IMETHODIMP
nsLocalFile::GetHashCode(PRUint32 *aResult)
{
    // In order for short and long path names to hash to the same value we
    // always hash on the short pathname.
    EnsureShortPath();

    *aResult = HashString(mShortWorkingPath);
    return NS_OK;
}

//-----------------------------------------------------------------------------
// nsLocalFile <static members>
//-----------------------------------------------------------------------------

void
nsLocalFile::GlobalInit()
{
#ifndef WINCE
    nsresult rv = NS_CreateShortcutResolver();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Shortcut resolver could not be created");
#endif
}

void
nsLocalFile::GlobalShutdown()
{
#ifndef WINCE
    NS_DestroyShortcutResolver();
#endif
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
#ifdef WINCE
    return NS_OK;
#else
    /* If the length passed to GetLogicalDriveStrings is smaller
     * than the length of the string it would return, it returns
     * the length required for the string. */
    DWORD length = GetLogicalDriveStrings(0, 0);
    /* The string is null terminated */
    if (!EnsureStringLength(mDrives, length+1))
        return NS_ERROR_OUT_OF_MEMORY;
    if (!GetLogicalDriveStrings(length, mDrives.BeginWriting()))
        return NS_ERROR_FAILURE;
    mLetter = mDrives.get();
    return NS_OK;
#endif
}

NS_IMETHODIMP nsDriveEnumerator::HasMoreElements(PRBool *aHasMore)
{
#ifdef WINCE
    *aHasMore = FALSE;
#else
    *aHasMore = *mLetter != '\0';
#endif
    return NS_OK;
}

NS_IMETHODIMP nsDriveEnumerator::GetNext(nsISupports **aNext)
{
#ifdef WINCE
    nsILocalFile *file;
    nsresult rv = NS_NewLocalFile(NS_LITERAL_STRING("\\"), PR_FALSE, &file);
    *aNext = file;
#else
    /* GetLogicalDrives stored in mLetter is a concatenation
     * of null terminated strings, followed by a null terminator. */
    if (!*mLetter) {
        *aNext = nsnull;
        return NS_OK;
    }
    NS_ConvertASCIItoUTF16 drive(mLetter);
    mLetter += drive.Length() + 1;
    nsILocalFile *file;
    nsresult rv = 
        NS_NewLocalFile(drive, PR_FALSE, &file);

    *aNext = file;
#endif
    return rv;
}

