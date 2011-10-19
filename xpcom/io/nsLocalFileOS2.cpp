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
 * Portions created by the Initial Developer are Copyright (C) 1998-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Henry Sobotka <sobotka@axess.com>
 *   IBM Corp.
 *   Rich Walsh <dragtext@e-vertise.com>
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

// N.B. the ns* & pr* headers below will #include all
// of the standard library headers this file requires

#include "nsCOMPtr.h"
#include "nsMemory.h"

#include "nsLocalFile.h"
#include "nsNativeCharsetUtils.h"

#include "nsISimpleEnumerator.h"
#include "nsIDirectoryEnumerator.h"
#include "nsIComponentManager.h"
#include "prtypes.h"
#include "prio.h"

#include "nsReadableUtils.h"
#include "nsISupportsPrimitives.h"
#include "nsIMutableArray.h"
#include "nsTraceRefcntImpl.h"

#define CHECK_mWorkingPath()                    \
    PR_BEGIN_MACRO                              \
        if (mWorkingPath.IsEmpty())             \
            return NS_ERROR_NOT_INITIALIZED;    \
    PR_END_MACRO

//-----------------------------------------------------------------------------
// static helper functions
//-----------------------------------------------------------------------------

static nsresult ConvertOS2Error(int err)
{
    nsresult rv;

    switch (err)
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
        default:
            rv = NS_ERROR_FAILURE;
    }
    return rv;
}

static void
myLL_L2II(PRInt64 result, PRInt32 *hi, PRInt32 *lo )
{
    PRInt64 a64, b64;  // probably could have been done with
                       // only one PRInt64, but these are macros,
                       // and I am a wimp.

    // shift the hi word to the low word, then push it into a long.
    LL_SHR(a64, result, 32);
    LL_L2I(*hi, a64);

    // shift the low word to the hi word first, then shift it back.
    LL_SHL(b64, result, 32);
    LL_SHR(a64, b64, 32);
    LL_L2I(*lo, a64);
}

// Locates the first occurrence of charToSearchFor in the stringToSearch
static unsigned char*
_mbschr(const unsigned char* stringToSearch, int charToSearchFor)
{
    const unsigned char* p = stringToSearch;

    do {
        if (*p == charToSearchFor)
            break;
        p  = (const unsigned char*)WinNextChar(0,0,0,(char*)p);
    } while (*p);

    // Result is p or NULL
    return *p ? (unsigned char*)p : NULL;
}

// Locates the first occurrence of subString in the stringToSearch
static unsigned char*
_mbsstr(const unsigned char* stringToSearch, const unsigned char* subString)
{
    const unsigned char* pStr = stringToSearch;
    const unsigned char* pSub = subString;

    do {
        while (*pStr && *pStr != *pSub)
            pStr = (const unsigned char*)WinNextChar(0,0,0,(char*)pStr);

        if (!*pStr)
            break;

        const unsigned char* pNxt = pStr;
        do {
            pSub = (const unsigned char*)WinNextChar(0,0,0,(char*)pSub);
            pNxt = (const unsigned char*)WinNextChar(0,0,0,(char*)pNxt);
        } while (*pSub && *pSub == *pNxt);

        if (!*pSub)
            break;

        pSub = subString;
        pStr = (const unsigned char*)WinNextChar(0,0,0,(char*)pStr);

    } while (*pStr);

    // if we got to the end of pSub, we've found it
    return *pSub ? NULL : (unsigned char*)pStr;
}

// Locates last occurence of charToSearchFor in the stringToSearch
NS_EXPORT unsigned char*
_mbsrchr(const unsigned char* stringToSearch, int charToSearchFor)
{
    int length = strlen((const char*)stringToSearch);
    const unsigned char* p = stringToSearch+length;

    do {
        if (*p == charToSearchFor)
            break;
        p  = (const unsigned char*)WinPrevChar(0,0,0,(char*)stringToSearch,(char*)p);
    } while (p > stringToSearch);

    // Result is p or NULL
    return (*p == charToSearchFor) ? (unsigned char*)p : NULL;
}

// Implement equivalent of Win32 CreateDirectoryA
static nsresult
CreateDirectoryA(PSZ path, PEAOP2 ppEABuf)
{
    APIRET rc;
    nsresult rv;
    FILESTATUS3 pathInfo;

    rc = DosCreateDir(path, ppEABuf);
    if (rc != NO_ERROR)
    {
        rv = ConvertOS2Error(rc);

        // Check if directory already exists and if so,
        // reflect that in the return value
        rc = DosQueryPathInfo(path, FIL_STANDARD,
                              &pathInfo, sizeof(pathInfo));
        if (rc == NO_ERROR)
            rv = ERROR_FILE_EXISTS;
    }
    else
        rv = rc;

    return rv;
}

static int isleadbyte(int c)
{
    static BOOL bDBCSFilled = FALSE;
    // According to the Control Program Guide&Ref, 12 bytes is sufficient
    static BYTE DBCSInfo[12] = { 0 };
    BYTE *curr;
    BOOL retval = FALSE;

    if(!bDBCSFilled)
    {
        COUNTRYCODE ctrycodeInfo = { 0 };
        APIRET rc = NO_ERROR;
        ctrycodeInfo.country = 0;     // Current Country
        ctrycodeInfo.codepage = 0;    // Current Codepage

        rc = DosQueryDBCSEnv(sizeof(DBCSInfo), &ctrycodeInfo, DBCSInfo);
        // we had an error, do something?
        if (rc != NO_ERROR)
            return FALSE;

        bDBCSFilled=TRUE;
    }

    // DBCSInfo returned by DosQueryDBCSEnv is terminated
    // with two '0' bytes in a row
    curr = DBCSInfo;
    while(*curr != 0 && *(curr+1) != 0)
    {
        if(c >= *curr && c <= *(curr+1))
        {
            retval=TRUE;
            break;
        }
        curr+=2;
    }

    return retval;
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

        NS_IMETHOD HasMoreElements(bool *result)
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

                    *result = false;
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
                bool exists;
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
            bool hasMore;
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
            bool hasMore = false;
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
                PRStatus status = PR_CloseDir(mDir);
                NS_ASSERTION(status == PR_SUCCESS, "close failed");
                if (status != PR_SUCCESS)
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
        PRDir*                  mDir;
        nsCOMPtr<nsILocalFile>  mParent;
        nsCOMPtr<nsILocalFile>  mNext;
};

NS_IMPL_ISUPPORTS2(nsDirEnumerator, nsISimpleEnumerator, nsIDirectoryEnumerator)

//-----------------------------------------------------------------------------
// nsDriveEnumerator
//-----------------------------------------------------------------------------

class nsDriveEnumerator : public nsISimpleEnumerator
{
public:
    nsDriveEnumerator();
    virtual ~nsDriveEnumerator();
    NS_DECL_ISUPPORTS
    NS_DECL_NSISIMPLEENUMERATOR
    nsresult Init();

private:
    // mDrives is a bitmap representing the available drives
    // mLetter is incremented each time mDrives is shifted rightward
    PRUint32    mDrives;
    PRUint8     mLetter;
};

NS_IMPL_ISUPPORTS1(nsDriveEnumerator, nsISimpleEnumerator)

nsDriveEnumerator::nsDriveEnumerator()
 : mDrives(0), mLetter(0)
{
}

nsDriveEnumerator::~nsDriveEnumerator()
{
}

nsresult nsDriveEnumerator::Init()
{
    ULONG   ulCurrent;

    // bits 0-25 in mDrives represent each possible drive, A-Z
    DosError(FERR_DISABLEHARDERR);
    APIRET rc = DosQueryCurrentDisk(&ulCurrent, (PULONG)&mDrives);
    DosError(FERR_ENABLEHARDERR);
    if (rc)
        return NS_ERROR_FAILURE;

    mLetter = 'A';
    return NS_OK;
}

NS_IMETHODIMP nsDriveEnumerator::HasMoreElements(bool *aHasMore)
{
    // no more bits means no more drives
    *aHasMore = (mDrives != 0);
    return NS_OK;
}

NS_IMETHODIMP nsDriveEnumerator::GetNext(nsISupports **aNext)
{
    if (!mDrives)
    {
        *aNext = nsnull;
        return NS_OK;
    }

    // if bit 0 is off, advance to the next bit that's on
    while ((mDrives & 1) == 0)
    {
        mDrives >>= 1;
        mLetter++;
    }

    // format a drive string, then advance to the next possible drive
    char drive[4] = "x:\\";
    drive[0] = mLetter;
    mDrives >>= 1;
    mLetter++;

    nsILocalFile *file;
    nsresult rv = NS_NewNativeLocalFile(nsDependentCString(drive),
                                        false, &file);
    *aNext = file;

    return rv;
}

//-----------------------------------------------------------------------------
// class TypeEaEnumerator - a convenience for accessing
// a file's .TYPE extended attribute
//-----------------------------------------------------------------------------

// this struct describes the first entry for an MVMT or MVST EA;
// .TYPE is supposed to be MVMT but is sometimes malformed as MVST

typedef struct _MVHDR {
    USHORT  usEAType;
    USHORT  usCodePage;
    USHORT  usNumEntries;
    USHORT  usDataType;
    USHORT  usDataLth;
    char    data[1];
} MVHDR;

typedef MVHDR *PMVHDR;


class TypeEaEnumerator
{
public:
    TypeEaEnumerator() : mEaBuf(nsnull) { }
    ~TypeEaEnumerator() { if (mEaBuf) NS_Free(mEaBuf); }

    nsresult Init(nsLocalFile * aFile);
    char *   GetNext(PRUint32 *lth);

private:
    char *  mEaBuf;
    char *  mpCur;
    PMVHDR  mpMvh;
    USHORT  mLth;
    USHORT  mCtr;
};


nsresult TypeEaEnumerator::Init(nsLocalFile * aFile)
{
#define EABUFSIZE 512

    // provide a buffer for the results
    mEaBuf = (char*)NS_Alloc(EABUFSIZE);
    if (!mEaBuf)
        return NS_ERROR_OUT_OF_MEMORY;

    PFEA2LIST   pfea2list = (PFEA2LIST)mEaBuf;
    pfea2list->cbList = EABUFSIZE;

    // ask for the .TYPE extended attribute
    nsresult rv = aFile->GetEA(".TYPE", pfea2list);
    if (NS_FAILED(rv))
        return rv;

    // point at the data - it starts immediately after the EA's name;
    // then confirm the EA is MVMT (correct) or MVST (acceptable)
    mpMvh = (PMVHDR)&(pfea2list->list[0].szName[pfea2list->list[0].cbName+1]);
    if (mpMvh->usEAType != EAT_MVMT)
        if (mpMvh->usEAType != EAT_MVST || mpMvh->usDataType != EAT_ASCII)
            return NS_ERROR_FAILURE;

    // init the variables that tell us where we are in the lsit
    mLth = 0;
    mCtr = 0;
    mpCur = (char*)(mpMvh->usEAType == EAT_MVMT ?
                    &mpMvh->usDataType : &mpMvh->usDataLth);

    return NS_OK;
}


char *   TypeEaEnumerator::GetNext(PRUint32 *lth)
{
    char *  result = nsnull;

    // this is a loop so we can skip invalid entries if needed;
    // normally, it will break out on the first iteration
    while (mCtr++ < mpMvh->usNumEntries) {

        // advance to the next entry
        mpCur += mLth;

        // if MVMT, ensure the datatype is OK, then advance
        // to the length field present in both formats
        if (mpMvh->usEAType == EAT_MVMT) {
            if (*((PUSHORT)mpCur) != EAT_ASCII)
                continue;
            mpCur += sizeof(USHORT);
        }

        // get the data's length, point at the data itself, then exit
        mLth = *lth = *((PUSHORT)mpCur);
        mpCur += sizeof(USHORT);
        result = mpCur;
        break;
    }

    return result;
}

//-----------------------------------------------------------------------------
// nsLocalFile <public>
//-----------------------------------------------------------------------------

nsLocalFile::nsLocalFile()
  : mDirty(true)
{
}

nsresult
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
                              nsILocalFileOS2,
                              nsIHashable)


//-----------------------------------------------------------------------------
// nsLocalFile <private>
//-----------------------------------------------------------------------------

nsLocalFile::nsLocalFile(const nsLocalFile& other)
  : mDirty(true)
  , mWorkingPath(other.mWorkingPath)
{
}


// Stat the path. After a successful return the path is
// guaranteed valid and the members of mFileInfo64 can be used.
nsresult
nsLocalFile::Stat()
{
    // if we aren't dirty then we are already done
    if (!mDirty)
        return NS_OK;

    // we can't stat anything that isn't a valid NSPR addressable path
    if (mWorkingPath.IsEmpty())
        return NS_ERROR_FILE_INVALID_PATH;

    // hack designed to work around bug 134796 until it is fixed
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

    // see if the working path exists
    DosError(FERR_DISABLEHARDERR);
    PRStatus status = PR_GetFileInfo64(nsprPath, &mFileInfo64);
    DosError(FERR_ENABLEHARDERR);
    if (status != PR_SUCCESS)
        return NS_ERROR_FILE_NOT_FOUND;

    mDirty = false;
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

    // just do a sanity check.  if it has any forward slashes, it is not
    // a Native path.  Also, it must have a colon at after the first char.
    if (FindCharInReadable('/', begin, end))
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    if (secondChar != ':' && (secondChar != '\\' || firstChar != '\\'))
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    mWorkingPath = filePath;
    // kill any trailing '\' provided it isn't the second char of DBCS
    PRInt32 len = mWorkingPath.Length() - 1;
    if (mWorkingPath[len] == '\\' && !::isleadbyte(mWorkingPath[len - 1]))
        mWorkingPath.Truncate(len);

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::OpenNSPRFileDesc(PRInt32 flags, PRInt32 mode, PRFileDesc **_retval)
{
    nsresult rv = Stat();
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv;

    *_retval = PR_Open(mWorkingPath.get(), flags, mode);
    if (*_retval)
        return NS_OK;

    if (flags & DELETE_ON_CLOSE) {
        PR_Delete(mWorkingPath.get());
    }

    return NS_ErrorAccordingToNSPR();
}


NS_IMETHODIMP
nsLocalFile::OpenANSIFileDesc(const char *mode, FILE * *_retval)
{
    nsresult rv = Stat();
    if (NS_FAILED(rv) && rv != NS_ERROR_FILE_NOT_FOUND)
        return rv;

    *_retval = fopen(mWorkingPath.get(), mode);
    if (*_retval)
        return NS_OK;

    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsLocalFile::Create(PRUint32 type, PRUint32 attributes)
{
    if (type != NORMAL_FILE_TYPE && type != DIRECTORY_TYPE)
        return NS_ERROR_FILE_UNKNOWN_TYPE;

    nsresult rv = Stat();
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

    unsigned char* path = (unsigned char*) mWorkingPath.BeginWriting();

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

            rv = CreateDirectoryA(const_cast<char*>(mWorkingPath.get()), NULL);
            if (rv) {
                rv = ConvertOS2Error(rv);
                if (rv != NS_ERROR_FILE_ALREADY_EXISTS)
                    return rv;
            }
            *slash = '\\';
            ++slash;
            slash = _mbschr(slash, '\\');
        }
    }

    if (type == NORMAL_FILE_TYPE)
    {
        PRFileDesc* file = PR_Open(mWorkingPath.get(), PR_RDONLY | PR_CREATE_FILE | PR_APPEND | PR_EXCL, attributes);
        if (!file)
            return NS_ERROR_FILE_ALREADY_EXISTS;

        PR_Close(file);
        return NS_OK;
    }

    if (type == DIRECTORY_TYPE)
    {
        rv = CreateDirectoryA(const_cast<char*>(mWorkingPath.get()), NULL);
        if (rv)
            return ConvertOS2Error(rv);
        else
            return NS_OK;
    }

    return NS_ERROR_FILE_UNKNOWN_TYPE;
}


NS_IMETHODIMP
nsLocalFile::AppendNative(const nsACString &node)
{
    // append this path, multiple components are not permitted
    return AppendNativeInternal(PromiseFlatCString(node), false);
}

NS_IMETHODIMP
nsLocalFile::AppendRelativeNativePath(const nsACString &node)
{
    // append this path, multiple components are permitted
    return AppendNativeInternal(PromiseFlatCString(node), true);
}

nsresult
nsLocalFile::AppendNativeInternal(const nsAFlatCString &node, bool multipleComponents)
{
    if (node.IsEmpty())
        return NS_OK;

    // check the relative path for validity
    const unsigned char * nodePath = (const unsigned char *) node.get();
    if (*nodePath == '\\'                           // can't start with an '\'
        || _mbschr(nodePath, '/')                   // can't contain /
        || node.Equals(".."))                       // can't be ..
        return NS_ERROR_FILE_UNRECOGNIZED_PATH;

    if (multipleComponents)
    {
        // can't contain .. as a path component. Ensure that the valid components
        // "foo..foo", "..foo", and "foo.." are not falsely detected, but the invalid
        // paths "..\", "foo\..", "foo\..\foo", "..\foo", etc are.
        const unsigned char * doubleDot = _mbsstr(nodePath, (const unsigned char *)"\\..");
        while (doubleDot)
        {
            doubleDot += 3;
            if (*doubleDot == '\0' || *doubleDot == '\\')
                return NS_ERROR_FILE_UNRECOGNIZED_PATH;
            doubleDot = _mbsstr(doubleDot, (unsigned char *)"\\..");
        }
        // catches the remaining cases of prefixes (i.e. '..\')
        // note: this is a substitute for Win32's _mbsncmp(nodePath, "..\\", 3)
        if (*nodePath == '.') {
            nodePath = (const unsigned char*)WinNextChar(0,0,0,(char*)nodePath);
            if (*nodePath == '.' &&
                *WinNextChar(0,0,0,(char*)nodePath) == '\\')
                return NS_ERROR_FILE_UNRECOGNIZED_PATH;
        }
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
    // XXX See bug 187957 comment 18 for possible problems with this implementation.

    if (mWorkingPath.IsEmpty())
        return NS_OK;

    // work in unicode for ease
    nsAutoString path;
    NS_CopyNativeToUnicode(mWorkingPath, path);

    // find the index of the root backslash for the path. Everything before
    // this is considered fully normalized and cannot be ascended beyond
    // using ".."  For a local drive this is the first slash (e.g. "c:\").
    // For a UNC path it is the slash following the share name
    // (e.g. "\\server\share\").
    PRInt32 rootIdx = 2;        // default to local drive
    if (path.First() == '\\')   // if a share then calculate the rootIdx
    {
        rootIdx = path.FindChar('\\', 2);   // skip \\ in front of the server
        if (rootIdx == kNotFound)
            return NS_OK;                   // already normalized
        rootIdx = path.FindChar('\\', rootIdx+1);
        if (rootIdx == kNotFound)
            return NS_OK;                   // already normalized
    }
    else if (path.CharAt(rootIdx) != '\\')
    {
        // The path has been specified relative to the current working directory
        // for that drive. To normalize it, the current working directory for
        // that drive needs to be inserted before the supplied relative path
        // which will provide an absolute path (and the rootIdx will still be 2).
        char drv[4] = "A:.";
        char cwd[CCHMAXPATH];

        drv[0] = mWorkingPath.First();
        if (DosQueryPathInfo(drv, FIL_QUERYFULLNAME, cwd, sizeof(cwd)))
            return NS_ERROR_FILE_NOT_FOUND;

        nsAutoString currentDir;
        NS_CopyNativeToUnicode(nsDependentCString(cwd), currentDir);

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
    nsAutoString normal;
    const PRUnichar * pathBuffer = path.get();  // simplify access to the buffer
    normal.SetCapacity(path.Length()); // it won't ever grow longer
    normal.Assign(pathBuffer, rootIdx);

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
            if (len >= 2 && pathBuffer[begin+1] == '.')
            {
                // back up a path component on double dot
                if (len == 2)
                {
                    PRInt32 prev = normal.RFindChar('\\');
                    if (prev >= rootIdx)
                        normal.Truncate(prev);
                    continue;
                }

                // length is > 2 and the first two characters are dots.
                // if the rest of the string is dots, then ignore it.
                int idx = len - 1;
                for (; idx >= 2; --idx)
                {
                    if (pathBuffer[begin+idx] != '.')
                        break;
                }

                // this is true if the loop above didn't break
                // and all characters in this segment are dots.
                if (idx < 2)
                    continue;
            }
        }

        // add the current component to the path, including the preceding backslash
        normal.Append(pathBuffer + begin - 1, len + 1);
    }

    // kill trailing dots and spaces.
    PRInt32 filePathLen = normal.Length() - 1;
    while(filePathLen > 0 && (normal[filePathLen] == ' ' || normal[filePathLen] == '.'))
    {
        normal.Truncate(filePathLen--);
    }

    NS_CopyUnicodeToNative(normal, mWorkingPath);
    MakeDirty();

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

//-----------------------------------------------------------------------------

// get any single extended attribute for the current file or directory

nsresult
nsLocalFile::GetEA(const char *eaName, PFEA2LIST pfea2list)
{
    // ensure we have an out-buffer whose length is specified
    if (!pfea2list || !pfea2list->cbList)
        return NS_ERROR_FAILURE;

    // the gea2list's name field is only 1 byte long;
    // this expands its allocation to hold a 33 byte name
    union {
        GEA2LIST    gea2list;
        char        dummy[sizeof(GEA2LIST)+32];
    };
    EAOP2       eaop2;

    eaop2.fpFEA2List = pfea2list;
    eaop2.fpGEA2List = &gea2list;

    // fill in the request structure
    dummy[sizeof(GEA2LIST)+31] = 0;
    gea2list.list[0].oNextEntryOffset = 0;
    strcpy(gea2list.list[0].szName, eaName);
    gea2list.list[0].cbName = strlen(gea2list.list[0].szName);
    gea2list.cbList = sizeof(GEA2LIST) + gea2list.list[0].cbName;

    // see what we get - this will succeed even if the EA doesn't exist
    APIRET rc = DosQueryPathInfo(mWorkingPath.get(), FIL_QUERYEASFROMLIST,
                                 &eaop2, sizeof(eaop2));
    if (rc)
        return ConvertOS2Error(rc);

    // if the data length is zero, requested EA doesn't exist
    if (!pfea2list->list[0].cbValue)
        return NS_ERROR_FAILURE;

    return NS_OK;
}


// return an array of file types or null if there are none

NS_IMETHODIMP
nsLocalFile::GetFileTypes(nsIArray **_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = 0;

    // fetch the .TYPE ea & prepare for enumeration
    TypeEaEnumerator typeEnum;
    nsresult rv = typeEnum.Init(this);
    if (NS_FAILED(rv))
        return rv;

    // create an array that's scriptable
    nsCOMPtr<nsIMutableArray> mutArray =
        do_CreateInstance(NS_ARRAY_CONTRACTID);
    NS_ENSURE_STATE(mutArray);

    PRInt32  cnt;
    PRUint32 lth;
    char *   ptr;

    // get each file type, convert to a CString, then add to the array
    for (cnt=0, ptr=typeEnum.GetNext(&lth); ptr; ptr=typeEnum.GetNext(&lth)) {
        nsCOMPtr<nsISupportsCString> typeString(
                    do_CreateInstance(NS_SUPPORTS_CSTRING_CONTRACTID, &rv));
        if (NS_SUCCEEDED(rv)) {
            nsCAutoString temp;
            temp.Assign(ptr, lth);
            typeString->SetData(temp);
            mutArray->AppendElement(typeString, false);
            cnt++;
        }
    }

    // if the array has any contents, addref & return it
    if (cnt) {
        *_retval = mutArray;
        NS_ADDREF(*_retval);
        rv = NS_OK;
    }
    else
        rv = NS_ERROR_FAILURE;

    return rv;
}


// see if the file is of the requested type

NS_IMETHODIMP
nsLocalFile::IsFileType(const nsACString& fileType, bool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = false;

    // fetch the .TYPE ea & prepare for enumeration
    TypeEaEnumerator typeEnum;
    nsresult rv = typeEnum.Init(this);
    if (NS_FAILED(rv))
        return rv;

    PRUint32 lth;
    char *   ptr;

    // compare each type to the request;  if there's a match, exit
    for (ptr = typeEnum.GetNext(&lth); ptr; ptr = typeEnum.GetNext(&lth))
        if (fileType.EqualsASCII(ptr, lth)) {
            *_retval = true;
            break;
        }

    return NS_OK;
}

//-----------------------------------------------------------------------------

// this struct combines an FEA2LIST, an FEA2, plus additional fields
// needed to write a .TYPE EA in the correct EAT_MVMT format
#pragma pack(1)
    typedef struct _TYPEEA {
        struct {
            ULONG   ulcbList;
            ULONG   uloNextEntryOffset;
            BYTE    bfEA;
            BYTE    bcbName;
            USHORT  uscbValue;
            char    chszName[sizeof(".TYPE")];
        } hdr;
        struct {
            USHORT  usEAType;
            USHORT  usCodePage;
            USHORT  usNumEntries;
            USHORT  usDataType;
            USHORT  usDataLth;
        } info;
        char    data[1];
    } TYPEEA;

    typedef struct _TYPEEA2 {
        USHORT  usDataType;
        USHORT  usDataLth;
    } TYPEEA2;
#pragma pack()

// writes one or more .TYPE extended attributes taken
// from a comma-delimited string
NS_IMETHODIMP
nsLocalFile::SetFileTypes(const nsACString& fileTypes)
{
    if (fileTypes.IsEmpty())
        return NS_ERROR_FAILURE;

    PRUint32 cnt = CountCharInReadable(fileTypes, ',');
    PRUint32 lth = fileTypes.Length() - cnt + (cnt * sizeof(TYPEEA2));
    PRUint32 size = sizeof(TYPEEA) + lth;

    char *pBuf = (char*)NS_Alloc(size);
    if (!pBuf)
        return NS_ERROR_OUT_OF_MEMORY;

    TYPEEA *pEA = (TYPEEA *)pBuf;

    // the buffer has an extra byte due to TYPEEA.data[1]
    pEA->hdr.ulcbList = size - 1;
    pEA->hdr.uloNextEntryOffset = 0;
    pEA->hdr.bfEA = 0;
    pEA->hdr.bcbName = sizeof(".TYPE") - 1;
    pEA->hdr.uscbValue = sizeof(pEA->info) + lth;
    strcpy(pEA->hdr.chszName, ".TYPE");

    pEA->info.usEAType = EAT_MVMT;
    pEA->info.usCodePage = 0;
    pEA->info.usNumEntries = ++cnt;

    nsACString::const_iterator begin, end, delim;
    fileTypes.BeginReading(begin);
    fileTypes.EndReading(end);
    delim = begin;

    // fill in type & length, copy the current type name (which
    // is not zero-terminated), then advance the ptr so the next
    // iteration can reuse the trailing members of the structure
    do {
        FindCharInReadable( ',', delim, end);
        lth = delim.get() - begin.get();
        pEA->info.usDataType = EAT_ASCII;
        pEA->info.usDataLth = lth;
        memcpy(pEA->data, begin.get(), lth);
        pEA = (TYPEEA *)((char*)pEA + lth + sizeof(TYPEEA2));
        begin = ++delim;
    } while (--cnt);

    // write the EA, then free the buffer
    EAOP2 eaop2;
    eaop2.fpGEA2List = 0;
    eaop2.fpFEA2List = (PFEA2LIST)pBuf;

    int rc = DosSetPathInfo(mWorkingPath.get(), FIL_QUERYEASIZE,
                            &eaop2, sizeof(eaop2), 0);
    NS_Free(pBuf);

    if (rc)
        return ConvertOS2Error(rc);

    return NS_OK;
}

//-----------------------------------------------------------------------------

// this struct combines an FEA2LIST, an FEA2, plus additional fields
// needed to write a .SUBJECT EA in the correct EAT_ASCII format;
#pragma pack(1)
    typedef struct _SUBJEA {
        struct {
            ULONG   ulcbList;
            ULONG   uloNextEntryOffset;
            BYTE    bfEA;
            BYTE    bcbName;
            USHORT  uscbValue;
            char    chszName[sizeof(".SUBJECT")];
        } hdr;
        struct {
            USHORT  usEAType;
            USHORT  usDataLth;
        } info;
        char    data[1];
    } SUBJEA;
#pragma pack()

// saves the file's source URI in its .SUBJECT extended attribute
NS_IMETHODIMP
nsLocalFile::SetFileSource(const nsACString& aURI)
{
    if (aURI.IsEmpty())
        return NS_ERROR_FAILURE;

    // this includes an extra character for the spec's trailing null
    PRUint32 lth = sizeof(SUBJEA) + aURI.Length();
    char *   pBuf = (char*)NS_Alloc(lth);
    if (!pBuf)
        return NS_ERROR_OUT_OF_MEMORY;

    SUBJEA *pEA = (SUBJEA *)pBuf;

    // the trailing null doesn't get saved, so don't include it in the size
    pEA->hdr.ulcbList = lth - 1;
    pEA->hdr.uloNextEntryOffset = 0;
    pEA->hdr.bfEA = 0;
    pEA->hdr.bcbName = sizeof(".SUBJECT") - 1;
    pEA->hdr.uscbValue = sizeof(pEA->info) + aURI.Length();
    strcpy(pEA->hdr.chszName, ".SUBJECT");
    pEA->info.usEAType = EAT_ASCII;
    pEA->info.usDataLth = aURI.Length();
    strcpy(pEA->data, PromiseFlatCString(aURI).get());

    // write the EA, then free the buffer
    EAOP2 eaop2;
    eaop2.fpGEA2List = 0;
    eaop2.fpFEA2List = (PFEA2LIST)pEA;

    int rc = DosSetPathInfo(mWorkingPath.get(), FIL_QUERYEASIZE,
                            &eaop2, sizeof(eaop2), 0);
    NS_Free(pBuf);

    if (rc)
        return ConvertOS2Error(rc);

    return NS_OK;
}

//-----------------------------------------------------------------------------

nsresult
nsLocalFile::CopySingleFile(nsIFile *sourceFile, nsIFile *destParent,
                            const nsACString &newName, bool move)
{
    nsresult rv;
    nsCAutoString filePath;

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

    rv = sourceFile->GetNativePath(filePath);

    if (NS_FAILED(rv))
        return rv;

    APIRET rc = NO_ERROR;

    if (move)
        rc = DosMove(filePath.get(), (PSZ)const_cast<char*>(destPath.get()));

    if (!move || rc == ERROR_NOT_SAME_DEVICE || rc == ERROR_ACCESS_DENIED)
    {
        // will get an error if the destination and source files aren't on
        // the same drive.  "MoveFile()" on Windows will go ahead and move
        // the file without error, so we need to do the same   IBM-AKR

        do {
            rc = DosCopy(filePath.get(), (PSZ)const_cast<char*>(destPath.get()), DCPY_EXISTING);
            if (rc == ERROR_TOO_MANY_OPEN_FILES) {
                ULONG CurMaxFH = 0;
                LONG ReqCount = 20;
                APIRET rc2;
                rc2 = DosSetRelMaxFH(&ReqCount, &CurMaxFH);
                if (rc2 != NO_ERROR)
                    break;
            }
        } while (rc == ERROR_TOO_MANY_OPEN_FILES);

        // WSOD2 HACK - NETWORK_ACCESS_DENIED
        if (rc == 65)
        {
            CHAR         achProgram[CCHMAXPATH];  // buffer for program name, parameters
            RESULTCODES  rescResults;             // buffer for results of dosexecpgm

            strcpy(achProgram, "CMD.EXE  /C ");
            strcat(achProgram, """COPY ");
            strcat(achProgram, filePath.get());
            strcat(achProgram, " ");
            strcat(achProgram, (PSZ)const_cast<char*>(destPath.get()));
            strcat(achProgram, """");
            achProgram[strlen(achProgram) + 1] = '\0';
            achProgram[7] = '\0';
            DosExecPgm(NULL, 0,
                       EXEC_SYNC, achProgram, (PSZ)NULL,
                       &rescResults, achProgram);
            rc = 0; // Assume it worked

        } // rc == 65

        // moving the file is supposed to act like a rename, so delete the
        // original file if we got this far without error
        if(move && (rc == NO_ERROR))
            DosDelete( filePath.get());

    } // !move or ERROR

    if (rc)
        rv = ConvertOS2Error(rc);

    return rv;
}


nsresult
nsLocalFile::CopyMove(nsIFile *aParentDir, const nsACString &newName, bool move)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    nsCOMPtr<nsIFile> newParentDir = aParentDir;

    nsresult rv  = Stat();
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
    bool exists;
    newParentDir->Exists(&exists);
    if (!exists)
    {
        rv = newParentDir->Create(DIRECTORY_TYPE, 0644);  // TODO, what permissions should we use
        if (NS_FAILED(rv))
            return rv;
    }
    else
    {
        bool isDir;
        newParentDir->IsDirectory(&isDir);
        if (!isDir)
        {
            return NS_ERROR_FILE_DESTINATION_NOT_DIR;
        }
    }

    // Try different ways to move/copy files/directories
    bool done = false;
    bool isDir;
    IsDirectory(&isDir);

    // Try to move the file or directory, or try to copy a single file
    if (move || !isDir)
    {
        // when moving things, first try to just MoveFile it,
        // even if it is a directory
        rv = CopySingleFile(this, newParentDir, newName, move);
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

        nsCAutoString allocatedNewName;
        if (newName.IsEmpty())
        {
            GetNativeLeafName(allocatedNewName);
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
            bool isWritable;

            target->IsWritable(&isWritable);
            if (!isWritable)
                return NS_ERROR_FILE_ACCESS_DENIED;

            nsCOMPtr<nsISimpleEnumerator> targetIterator;
            rv = target->GetDirectoryEntries(getter_AddRefs(targetIterator));
            if (NS_FAILED(rv))
                return rv;

            bool more;
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

        bool more;
        while (NS_SUCCEEDED(dirEnum.HasMoreElements(&more)) && more)
        {
            nsCOMPtr<nsISupports> item;
            nsCOMPtr<nsIFile> file;
            dirEnum.GetNext(getter_AddRefs(item));
            file = do_QueryInterface(item);
            if (file)
            {
                if (move)
                {
                    rv = file->MoveToNative(target, EmptyCString());
                    NS_ENSURE_SUCCESS(rv,rv);
                }
                else
                {
                    rv = file->CopyToNative(target, EmptyCString());
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
          rv = Remove(false); // recursive
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
    return CopyMove(newParentDir, newName, false);
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinksNative(nsIFile *newParentDir, const nsACString &newName)
{
    return CopyMove(newParentDir, newName, false);
}

NS_IMETHODIMP
nsLocalFile::MoveToNative(nsIFile *newParentDir, const nsACString &newName)
{
    return CopyMove(newParentDir, newName, true);
}

NS_IMETHODIMP
nsLocalFile::Load(PRLibrary * *_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    bool isFile;
    nsresult rv = IsFile(&isFile);

    if (NS_FAILED(rv))
        return rv;

    if (!isFile)
        return NS_ERROR_FILE_IS_DIRECTORY;

#ifdef NS_BUILD_REFCNT_LOGGING
    nsTraceRefcntImpl::SetActivityIsLegal(false);
#endif

    *_retval =  PR_LoadLibrary(mWorkingPath.get());

#ifdef NS_BUILD_REFCNT_LOGGING
    nsTraceRefcntImpl::SetActivityIsLegal(true);
#endif

    if (*_retval)
        return NS_OK;

    return NS_ERROR_NULL_POINTER;
}

NS_IMETHODIMP
nsLocalFile::Remove(bool recursive)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    bool isDir = false;

    nsresult rv = IsDirectory(&isDir);
    if (NS_FAILED(rv))
        return rv;

    if (isDir)
    {
        if (recursive)
        {
            nsDirEnumerator dirEnum;

            rv = dirEnum.Init(this);
            if (NS_FAILED(rv))
                return rv;

            bool more;
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
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG(aLastModifiedTime);

    *aLastModifiedTime = 0;
    nsresult rv = Stat();
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
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsLocalFile::SetLastModifiedTime(PRInt64 aLastModifiedTime)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    return nsLocalFile::SetModDate(aLastModifiedTime);
}


NS_IMETHODIMP
nsLocalFile::SetLastModifiedTimeOfLink(PRInt64 aLastModifiedTime)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
nsLocalFile::SetModDate(PRInt64 aLastModifiedTime)
{
    nsresult rv = Stat();

    if (NS_FAILED(rv))
        return rv;

    PRExplodedTime pret;
    FILESTATUS3 pathInfo;

    // Level 1 info
    rv = DosQueryPathInfo(mWorkingPath.get(), FIL_STANDARD,
                          &pathInfo, sizeof(pathInfo));

    if (NS_FAILED(rv))
    {
       rv = ConvertOS2Error(rv);
       return rv;
    }

    // PR_ExplodeTime expects usecs...
    PR_ExplodeTime(aLastModifiedTime * PR_USEC_PER_MSEC, PR_LocalTimeParameters, &pret);

    // fdateLastWrite.year is based off of 1980
    if (pret.tm_year >= 1980)
        pathInfo.fdateLastWrite.year    = pret.tm_year-1980;
    else
        pathInfo.fdateLastWrite.year    = pret.tm_year;

    // Convert start offset -- OS/2: Jan=1; NSPR: Jan=0
    pathInfo.fdateLastWrite.month       = pret.tm_month + 1;
    pathInfo.fdateLastWrite.day         = pret.tm_mday;
    pathInfo.ftimeLastWrite.hours       = pret.tm_hour;
    pathInfo.ftimeLastWrite.minutes     = pret.tm_min;
    pathInfo.ftimeLastWrite.twosecs     = pret.tm_sec / 2;

    rv = DosSetPathInfo(mWorkingPath.get(), FIL_STANDARD,
                        &pathInfo, sizeof(pathInfo), 0UL);

    if (NS_FAILED(rv))
       return rv;

    MakeDirty();
    return rv;
}

NS_IMETHODIMP
nsLocalFile::GetPermissions(PRUint32 *aPermissions)
{
    NS_ENSURE_ARG(aPermissions);

    nsresult rv = Stat();
    if (NS_FAILED(rv))
        return rv;

    bool isWritable, isExecutable;
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
nsLocalFile::GetPermissionsOfLink(PRUint32 *aPermissionsOfLink)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

// the only Unix-style permission OS/2 knows is whether a file is
// writable;  as a matter of policy, a browser shouldn't be able
// to change any of the other DOS-style attributes;  to enforce
// this, we use DosSetPathInfo() rather than chmod()
NS_IMETHODIMP
nsLocalFile::SetPermissions(PRUint32 aPermissions)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    nsresult rv = Stat();
    if (NS_FAILED(rv))
        return rv;

    APIRET rc;
    FILESTATUS3 pathInfo;

    // Level 1 info
    rc = DosQueryPathInfo(mWorkingPath.get(), FIL_STANDARD,
                          &pathInfo, sizeof(pathInfo));

    if (rc != NO_ERROR)
       return ConvertOS2Error(rc);

    ULONG attr = 0;
    if (!(aPermissions & (PR_IWUSR|PR_IWGRP|PR_IWOTH)))
        attr = FILE_READONLY;

    if (attr == (pathInfo.attrFile & FILE_READONLY))
        return NS_OK;

    pathInfo.attrFile ^= FILE_READONLY;

    rc = DosSetPathInfo(mWorkingPath.get(), FIL_STANDARD,
                        &pathInfo, sizeof(pathInfo), 0UL);

    if (rc != NO_ERROR)
       return ConvertOS2Error(rc);

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetPermissionsOfLink(PRUint32 aPermissions)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsLocalFile::GetFileSize(PRInt64 *aFileSize)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG(aFileSize);
    *aFileSize = 0;

    nsresult rv = Stat();
    if (NS_FAILED(rv))
        return rv;

    *aFileSize = mFileInfo64.size;
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::GetFileSizeOfLink(PRInt64 *aFileSize)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsLocalFile::SetFileSize(PRInt64 aFileSize)
{
    nsresult rv = Stat();
    if (NS_FAILED(rv))
        return rv;

    APIRET rc;
    HFILE hFile;
    ULONG actionTaken;

    rc = DosOpen(mWorkingPath.get(),
                 &hFile,
                 &actionTaken,
                 0,
                 FILE_NORMAL,
                 OPEN_ACTION_FAIL_IF_NEW | OPEN_ACTION_OPEN_IF_EXISTS,
                 OPEN_SHARE_DENYREADWRITE | OPEN_ACCESS_READWRITE,
                 NULL);

    if (rc != NO_ERROR)
    {
        MakeDirty();
        return NS_ERROR_FAILURE;
    }

    // Seek to new, desired end of file
    PRInt32 hi, lo;
    myLL_L2II(aFileSize, &hi, &lo );

    rc = DosSetFileSize(hFile, lo);
    DosClose(hFile);
    MakeDirty();

    if (rc != NO_ERROR)
        return NS_ERROR_FAILURE;

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetDiskSpaceAvailable(PRInt64 *aDiskSpaceAvailable)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG(aDiskSpaceAvailable);
    *aDiskSpaceAvailable = 0;

    nsresult rv = Stat();
    if (NS_FAILED(rv))
        return rv;

    ULONG ulDriveNo = toupper(mWorkingPath.CharAt(0)) + 1 - 'A';
    FSALLOCATE fsAllocate;
    APIRET rc = DosQueryFSInfo(ulDriveNo,
                               FSIL_ALLOC,
                               &fsAllocate,
                               sizeof(fsAllocate));

    if (rc != NO_ERROR)
       return NS_ERROR_FAILURE;

    *aDiskSpaceAvailable = fsAllocate.cUnitAvail;
    *aDiskSpaceAvailable *= fsAllocate.cSectorUnit;
    *aDiskSpaceAvailable *= fsAllocate.cbSector;

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetParent(nsIFile * *aParent)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

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
    nsresult rv = NS_NewNativeLocalFile(parentPath, false, getter_AddRefs(localFile));

    if(NS_SUCCEEDED(rv) && localFile)
    {
        return CallQueryInterface(localFile, aParent);
    }
    return rv;
}

NS_IMETHODIMP
nsLocalFile::Exists(bool *_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG(_retval);
    *_retval = false;

    MakeDirty();
    nsresult rv = Stat();

    *_retval = NS_SUCCEEDED(rv);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsWritable(bool *_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG(_retval);
    *_retval = false;

    nsresult rv = Stat();
    if (NS_FAILED(rv))
        return rv;

    APIRET rc;
    FILESTATUS3 pathInfo;

    // Level 1 info
    rc = DosQueryPathInfo(mWorkingPath.get(), FIL_STANDARD,
                          &pathInfo, sizeof(pathInfo));

    if (rc != NO_ERROR)
    {
       rc = ConvertOS2Error(rc);
       return rc;
    }

    // on OS/2, unlike Windows, directories on writable media
    // can not be assigned a readonly attribute
    *_retval = !((pathInfo.attrFile & FILE_READONLY) != 0);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsReadable(bool *_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG(_retval);
    *_retval = false;

    nsresult rv = Stat();
    if (NS_FAILED(rv))
        return rv;

    *_retval = true;
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::IsExecutable(bool *_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG(_retval);
    *_retval = false;

    nsresult rv = Stat();
    if (NS_FAILED(rv))
        return rv;

    // no need to bother if this isn't a file
    bool isFile;
    rv = IsFile(&isFile);
    if (NS_FAILED(rv) || !isFile)
        return rv;

    nsCAutoString path;
    GetNativeTarget(path);

    // get the filename, including the leading backslash
    const char* leaf = (const char*) _mbsrchr((const unsigned char*)path.get(), '\\');
    if (!leaf)
        return NS_OK;

    // eliminate trailing dots & spaces in a DBCS-aware manner
    // XXX shouldn't this have been done when the path was set?

    char*   pathEnd = WinPrevChar(0, 0, 0, leaf, strchr(leaf, 0));
    while (pathEnd > leaf)
    {
        if (*pathEnd != ' ' && *pathEnd != '.')
            break;
        *pathEnd = 0;
        pathEnd = WinPrevChar(0, 0, 0, leaf, pathEnd);
    }

    if (pathEnd == leaf)
        return NS_OK;

    // get the extension, including the dot
    char* ext = (char*) _mbsrchr((const unsigned char*)leaf, '.');
    if (!ext)
        return NS_OK;

    if (stricmp(ext, ".exe") == 0 ||
        stricmp(ext, ".cmd") == 0 ||
        stricmp(ext, ".com") == 0 ||
        stricmp(ext, ".bat") == 0)
        *_retval = true;

    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::IsDirectory(bool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = false;

    nsresult rv = Stat();
    if (NS_FAILED(rv))
        return rv;

    *_retval = (mFileInfo64.type == PR_FILE_DIRECTORY);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsFile(bool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = false;

    nsresult rv = Stat();
    if (NS_FAILED(rv))
        return rv;

    *_retval = (mFileInfo64.type == PR_FILE_FILE);
    return rv;
}

NS_IMETHODIMP
nsLocalFile::IsHidden(bool *_retval)
{
    NS_ENSURE_ARG(_retval);
    *_retval = false;

    nsresult rv = Stat();
    if (NS_FAILED(rv))
        return rv;

    APIRET rc;
    FILESTATUS3 pathInfo;

    // Level 1 info
    rc = DosQueryPathInfo(mWorkingPath.get(), FIL_STANDARD,
                          &pathInfo, sizeof(pathInfo));

    if (rc != NO_ERROR)
    {
       rc = ConvertOS2Error(rc);
       return rc;
    }

    *_retval = ((pathInfo.attrFile & FILE_HIDDEN) != 0);
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::IsSymlink(bool *_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    NS_ENSURE_ARG_POINTER(_retval);

    // No Symlinks on OS/2
    *_retval = false;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::IsSpecial(bool *_retval)
{
    NS_ENSURE_ARG(_retval);

    // when implemented, IsSpecial will be used for WPS objects
    *_retval = false;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Equals(nsIFile *inFile, bool *_retval)
{
    NS_ENSURE_ARG(inFile);
    NS_ENSURE_ARG(_retval);

    nsCAutoString inFilePath;
    inFile->GetNativePath(inFilePath);

    *_retval = inFilePath.Equals(mWorkingPath);
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::Contains(nsIFile *inFile, bool recur, bool *_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    *_retval = false;

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
            *_retval = true;
        }

    }

    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetNativeTarget(nsACString &_retval)
{
    // Check we are correctly initialized.
    CHECK_mWorkingPath();

    _retval = mWorkingPath;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetFollowLinks(bool *aFollowLinks)
{
    NS_ENSURE_ARG(aFollowLinks);
    *aFollowLinks = false;
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::SetFollowLinks(bool aFollowLinks)
{
    return NS_OK;
}

NS_IMETHODIMP
nsLocalFile::GetDirectoryEntries(nsISimpleEnumerator * *entries)
{
    NS_ENSURE_ARG(entries);
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

    bool isDir;
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

#ifndef OPEN_DEFAULT
#define OPEN_DEFAULT       0
#define OPEN_CONTENTS      1
#endif

NS_IMETHODIMP
nsLocalFile::Reveal()
{
    bool isDirectory = false;
    nsCAutoString path;

    IsDirectory(&isDirectory);
    if (isDirectory)
    {
        GetNativePath(path);
    }
    else
    {
        nsCOMPtr<nsIFile> parent;
        GetParent(getter_AddRefs(parent));
        if (parent)
            parent->GetNativePath(path);
    }

    HOBJECT hobject = WinQueryObject(path.get());
    WinSetFocus(HWND_DESKTOP, HWND_DESKTOP);
    WinOpenObject(hobject, OPEN_DEFAULT, TRUE);

    // we don't care if it succeeded or failed.
    return NS_OK;
}


NS_IMETHODIMP
nsLocalFile::Launch()
{
  HOBJECT hobject = WinQueryObject(mWorkingPath.get());
  WinSetFocus(HWND_DESKTOP, HWND_DESKTOP);
  WinOpenObject(hobject, OPEN_DEFAULT, TRUE);

  // we don't care if it succeeded or failed.
  return NS_OK;
}

nsresult
NS_NewNativeLocalFile(const nsACString &path, bool followLinks, nsILocalFile* *result)
{
    nsLocalFile* file = new nsLocalFile();
    if (file == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(file);

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

NS_IMETHODIMP
nsLocalFile::InitWithPath(const nsAString &filePath)
{
    if (filePath.IsEmpty())
        return InitWithNativePath(EmptyCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(filePath, tmp);
    if (NS_SUCCEEDED(rv))
        return InitWithNativePath(tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::Append(const nsAString &node)
{
    if (node.IsEmpty())
        return NS_OK;

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(node, tmp);
    if (NS_SUCCEEDED(rv))
        return AppendNative(tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::AppendRelativePath(const nsAString &node)
{
    if (node.IsEmpty())
        return NS_OK;

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(node, tmp);
    if (NS_SUCCEEDED(rv))
        return AppendRelativeNativePath(tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::GetLeafName(nsAString &aLeafName)
{
    nsCAutoString tmp;
    nsresult rv = GetNativeLeafName(tmp);
    if (NS_SUCCEEDED(rv))
        rv = NS_CopyNativeToUnicode(tmp, aLeafName);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::SetLeafName(const nsAString &aLeafName)
{
    if (aLeafName.IsEmpty())
        return SetNativeLeafName(EmptyCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(aLeafName, tmp);
    if (NS_SUCCEEDED(rv))
        return SetNativeLeafName(tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::GetPath(nsAString &_retval)
{
    return NS_CopyNativeToUnicode(mWorkingPath, _retval);
}

NS_IMETHODIMP
nsLocalFile::CopyTo(nsIFile *newParentDir, const nsAString &newName)
{
    if (newName.IsEmpty())
        return CopyToNative(newParentDir, EmptyCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(newName, tmp);
    if (NS_SUCCEEDED(rv))
        return CopyToNative(newParentDir, tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::CopyToFollowingLinks(nsIFile *newParentDir, const nsAString &newName)
{
    if (newName.IsEmpty())
        return CopyToFollowingLinksNative(newParentDir, EmptyCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(newName, tmp);
    if (NS_SUCCEEDED(rv))
        return CopyToFollowingLinksNative(newParentDir, tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::MoveTo(nsIFile *newParentDir, const nsAString &newName)
{
    if (newName.IsEmpty())
        return MoveToNative(newParentDir, EmptyCString());

    nsCAutoString tmp;
    nsresult rv = NS_CopyUnicodeToNative(newName, tmp);
    if (NS_SUCCEEDED(rv))
        return MoveToNative(newParentDir, tmp);

    return rv;
}

NS_IMETHODIMP
nsLocalFile::GetTarget(nsAString &_retval)
{
    nsCAutoString tmp;
    nsresult rv = GetNativeTarget(tmp);
    if (NS_SUCCEEDED(rv))
        rv = NS_CopyNativeToUnicode(tmp, _retval);

    return rv;
}

// nsIHashable

NS_IMETHODIMP
nsLocalFile::Equals(nsIHashable* aOther, bool *aResult)
{
    nsCOMPtr<nsIFile> otherfile(do_QueryInterface(aOther));
    if (!otherfile) {
        *aResult = false;
        return NS_OK;
    }

    return Equals(otherfile, aResult);
}

NS_IMETHODIMP
nsLocalFile::GetHashCode(PRUint32 *aResult)
{
    *aResult = nsCRT::HashCode(mWorkingPath.get());
    return NS_OK;
}

nsresult
NS_NewLocalFile(const nsAString &path, bool followLinks, nsILocalFile* *result)
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
}

void
nsLocalFile::GlobalShutdown()
{
}

//-----------------------------------------------------------------------------

