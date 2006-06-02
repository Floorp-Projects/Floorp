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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Jungshik Shin
 * <jshin@i18nl10n.com>.  Portions created by the Initial Developer
 * are Copyright (C) 2006 the Initial Developer. All Rights Reserved.
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

#include "nscore.h"
#include "nsString.h"
#include "nsNativeCharsetUtils.h"

#include "nsWinAPIs.h"

#include <windows.h>
#include <winerror.h>

#include <io.h>
#include <stdlib.h>
#include <stdio.h>
#include <direct.h>

#ifdef WINCE
char *_getdcwd(int drive, char *buffer, int maxlen)
{
  return _getcwd(buffer, maxlen);
}
#endif

static inline void ConvertArg(LPCWSTR str, nsCAutoString &cstr,
                              const char* (&ptr))
{
    if (str) {                                                
        NS_CopyUnicodeToNative(nsDependentString(str), cstr);
        ptr = cstr.get();
    }                                                        
    else                                                     
        ptr = NULL;                                          
}

template <class T>
static inline PRBool SetLengthAndCheck(T &aStr, DWORD aLen)
{
    aStr.SetLength(aLen);
    if (aStr.Length() != aLen) {
        SetLastError(ERROR_OUTOFMEMORY);
        return PR_FALSE;
    }
    return PR_TRUE;
}

//
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/sysinfo/base/getting_system_information.asp

BOOL WINAPI nsSHGetPathFromIDListW(LPCITEMIDLIST aIdList, LPWSTR aPathW)
{
    if (aPathW)  {
        char pathA[MAX_PATH];
        if (SHGetPathFromIDListA(aIdList, pathA)) {
            NS_ConvertAtoW(pathA, MAX_PATH, aPathW);
            return TRUE;
        }
    }

    return FALSE;
}

// GetSystemDirectory, GetWindowsDirectory, GetTempPath, GetEnvironmentVariable 
// return the required buffer size including the terminating null if the buffer 
// passed is too small to hold the result while they return the number of 
// characters stored in the buffer excluding the terminating null if it's 
// large enough. Therefore, just checking whether the return value is zero 
// is not sufficient. We also need to make sure that the return value  is 
// smaller than the buffer size passed to them.
UINT WINAPI nsGetSystemDirectoryW(LPWSTR aPathW, UINT aSize)
{
    char pathA[MAX_PATH];
    int len;

    if (aPathW && GetSystemDirectoryA(pathA, MAX_PATH) < MAX_PATH && 
        (len = NS_ConvertAtoW(pathA, aSize, aPathW)))
        return len - 1; // terminating null is not included
    else 
        return 0;
}

UINT WINAPI nsGetWindowsDirectoryW(LPWSTR aPathW, UINT aSize)
{
    char pathA[MAX_PATH];
    int len;

    if (aPathW && GetWindowsDirectoryA(pathA, MAX_PATH) < MAX_PATH && 
        (len = NS_ConvertAtoW(pathA, aSize, aPathW)))
        return len - 1; // terminating null is not included
    else 
        return 0;
}

DWORD WINAPI nsGetTempPathW(DWORD aLen, LPWSTR aPathW)
{
#ifdef WINCE
    char *pathA = "\\temp";
    DWORD len = NS_ConvertAtoW(pathA, aLen, aPathW);
    return len-1;
#else
    char pathA[MAX_PATH];
    DWORD len;
    if (aPathW && GetTempPathA(MAX_PATH, pathA) < MAX_PATH &&
        (len = NS_ConvertAtoW(pathA, aLen, aPathW)))
        return len - 1; // terminating null is not included
    return 0;
#endif
}


DWORD WINAPI nsGetEnvironmentVariableW(LPCWSTR aName, LPWSTR aValue, DWORD aSize)
{

    if (!aName || !aValue || aSize <= 0)
        return 0;

    nsCAutoString nameA;
    NS_CopyUnicodeToNative(nsDependentString(aName), nameA); 

    const DWORD maxValueLen = 32767; // the maximum length of an env. variable
    nsCAutoString value;
    DWORD sizeA = NS_MIN(maxValueLen, aSize * 2);
    if (!SetLengthAndCheck(value, sizeA))
        return 0;
    char *buf = value.BeginWriting();
    DWORD len;
    if (!(len = GetEnvironmentVariableA((char*)nameA.get(), buf, sizeA)))
        return 0;

    if (len > sizeA) {
        if (sizeA < maxValueLen) {
            if (!SetLengthAndCheck(value, len))
                return 0;
            buf = value.BeginWriting();
            len = GetEnvironmentVariableA((char*)nameA.get(), buf, len);
        }
        else 
            // sizeA >= maxValueLen and still failed. 
            return 0;
    }

    // terminating null is not included in the length
    return (len = NS_ConvertAtoW(buf, aSize, aValue)) ? len - 1 : 0;
}

BOOL WINAPI nsCreateDirectoryW(LPCWSTR aPath, LPSECURITY_ATTRIBUTES aSecAttr)
{
    if (!aPath) {
        SetLastError(ERROR_BAD_ARGUMENTS);
        return FALSE;
    }

    nsCAutoString pathA;
    NS_CopyUnicodeToNative(nsDependentString(aPath), pathA);
    return CreateDirectoryA(pathA.get(), aSecAttr);
}

HANDLE WINAPI nsCreateFileW(LPCWSTR aPath, DWORD aAccess,
                            DWORD aMode, LPSECURITY_ATTRIBUTES aSecAttr,
                            DWORD aCreat, DWORD aFlags, HANDLE aTemplate)
{
    if (!aPath) {
        SetLastError(ERROR_BAD_ARGUMENTS);
        return INVALID_HANDLE_VALUE;
    }

    nsCAutoString pathA;
    NS_CopyUnicodeToNative(nsDependentString(aPath), pathA);
    return CreateFileA(pathA.get(), aAccess, aMode, aSecAttr, aCreat, 
                       aFlags, aTemplate);
}

HINSTANCE WINAPI nsShellExecuteW(HWND aWin, LPCWSTR aOp, LPCWSTR aFile,
                                 LPCWSTR aParam, LPCWSTR aDir, INT aShowCmd)
{
    nsCAutoString op, file, param, dir;
    const char  *pOp, *pFile, *pParam, *pDir;

    ConvertArg(aOp, op, pOp);
    ConvertArg(aFile, file, pFile);
    ConvertArg(aParam, param, pParam);
    ConvertArg(aDir, dir, pDir);

    return ShellExecuteA(aWin, pOp, pFile, pParam, pDir, aShowCmd);
}

BOOL WINAPI nsCopyFileW(LPCWSTR aSrc, LPCWSTR aDest, BOOL aNoOverwrite)
{
    nsCAutoString src, dest;
    const char *pSrc, *pDest;

    ConvertArg(aSrc, src, pSrc);
    ConvertArg(aDest, dest, pDest);

    return CopyFileA(pSrc, pDest, aNoOverwrite);
}

BOOL WINAPI nsMoveFileW(LPCWSTR aSrc, LPCWSTR aDest)
{
    nsCAutoString src, dest;
    const char *pSrc, *pDest;

    ConvertArg(aSrc, src, pSrc);
    ConvertArg(aDest, dest, pDest);

    return MoveFileA(pSrc, pDest);
}

// To work around the problem with misconfigured tinderboxes (bug 331433)
#ifndef WINCE
#if defined(_MSC_VER) && _MSC_VER <= 0x1200
#define GetFileVersionInfoA(aPath, aHandle, aLen, aData) \
  GetFileVersionInfoA((LPSTR)aPath, aHandle, aLen, aData)
#define GetFileVersionInfoSizeA(aPath, aHandle) \
  GetFileVersionInfoSizeA((LPSTR)aPath, aHandle)
#define GetFileVersionInfoW ((nsGetFileVersionInfo)GetFileVersionInfoW)
#define GetFileVersionInfoSizeW ((nsGetFileVersionInfoSize)GetFileVersionInfoSizeW)
#endif
#endif

BOOL WINAPI nsGetFileVersionInfoW(LPCWSTR aPath, DWORD aHandle, DWORD aLen,
                                  LPVOID aData)
{
    nsCAutoString path;
    const char *pPath;
    ConvertArg(aPath, path, pPath);
    return GetFileVersionInfoA(pPath, aHandle, aLen, aData);
}

DWORD WINAPI nsGetFileVersionInfoSizeW(LPCWSTR aPath, LPDWORD aHandle)
{
    nsCAutoString path;
    const char *pPath;
    ConvertArg(aPath, path, pPath);
    return GetFileVersionInfoSizeA(pPath, aHandle);
}

DWORD WINAPI nsGetFileAttributesW(LPCWSTR aPath)
{
    nsCAutoString path;
    const char *pPath;
    ConvertArg(aPath, path, pPath);
    return GetFileAttributesA(pPath);
}

BOOL WINAPI nsGetFileAttributesExW(LPCWSTR aPath, GET_FILEEX_INFO_LEVELS aLevel,
                                   LPVOID aInfo)
{
    NS_ASSERTION(aLevel == GetFileExInfoStandard, "invalid level specified");
#ifndef WINCE
    NS_ASSERTION(nsWinAPIs::mCopyFile != CopyFileW, 
                 "Win APIs pointers are reset to the stubs of 'W' APIs");
#endif

    nsCAutoString path;
    const char *pPath;
    ConvertArg(aPath, path, pPath);

    // present on Win 98/ME
    if (nsWinAPIs::mGetFileAttributesExA)
#ifdef DEBUG_jungshik
    {
        BOOL rv = nsWinAPIs::mGetFileAttributesExA(pPath, aLevel, aInfo);
        if (!rv) {
            printf("nsGetFileAttributesExW with file=%s failed\n", pPath);
            printf("Last error is %d\n", GetLastError());
        }
        return rv;
    }
#else
        return nsWinAPIs::mGetFileAttributesExA(pPath, aLevel, aInfo);
#endif
    // fallback for Win95 without IE4 or later where GetFileAttributesEx 
    // is missing. 
    
    if (aLevel != GetFileExInfoStandard) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return FALSE;
    }

    // FindFirstFile expands wildcard characters so that we need to make
    // sure that the path contains no wildcard.
    if (NULL != wcspbrk(aPath, L"?*")) 
        return FALSE;

    WIN32_FIND_DATAA fdata;
    HANDLE handle = FindFirstFileA(pPath, &fdata);
    if (handle == INVALID_HANDLE_VALUE)
        return FALSE;
    // XXX : FindFirstFile doesn't work with a root directory
    // or a path ending with a slash/back slash.
    // We can work around it as in NSPR (w95io.c), but don't bother
    // because Windows 95 without IE4 or later is a rare beast so that
    // it's not worth duplicating ~40 lines of the NSPR code here to 
    // support a path ending with a slash/back slash or a root path. 
    // Note that even MS's emulation of GetFileAttributesEx for Win95
    // (included in Visual C++) doesn't, either.

    WIN32_FILE_ATTRIBUTE_DATA *fadata= 
        (WIN32_FILE_ATTRIBUTE_DATA *) aInfo;

    fadata->dwFileAttributes = fdata.dwFileAttributes;
    fadata->ftCreationTime = fdata.ftCreationTime;
    fadata->ftLastAccessTime = fdata.ftLastAccessTime;
    fadata->ftLastWriteTime = fdata.ftLastWriteTime;
    fadata->nFileSizeHigh = fdata.nFileSizeHigh;
    fadata->nFileSizeLow = fdata.nFileSizeLow;

    FindClose(handle);
    return TRUE;
}   

DWORD WINAPI nsGetShortPathNameW(LPCWSTR aLPath, LPWSTR aSPath, DWORD aLen)
{
    if (!aLPath || !aSPath) {
        SetLastError(ERROR_BAD_ARGUMENTS);
        return 0;
    }

    nsCAutoString lPath;
    NS_CopyUnicodeToNative(nsDependentString(aLPath), lPath);

    // MAX_PATH is the max path length including the terminating null.
    if (lPath.Length() >= MAX_PATH) {
        SetLastError(ERROR_BAD_ARGUMENTS);
        return 0;
    }

    nsCAutoString sPath;
    if (!SetLengthAndCheck(sPath, aLen))
        return 0;
    char *pSpath = sPath.BeginWriting();
    DWORD lenA = GetShortPathNameA(lPath.get(), pSpath, aLen);
    if (lenA > aLen) {
        if (!SetLengthAndCheck(sPath, lenA))
            return 0;
        pSpath = sPath.BeginWriting();
        lenA = GetShortPathNameA(lPath.get(), pSpath, lenA);
    }

    if (!lenA) 
        return 0;

    // see if |aSPath| is large enough to hold the result
    DWORD lenW = NS_ConvertAtoW(pSpath, 0, aSPath);
    if (lenW > aLen)
        // aSPath is too small. just return the required size.
        return lenW; 

    // terminating null is not included in the return value
    return (lenW = NS_ConvertAtoW(pSpath, aLen, aSPath)) ? lenW - 1 : 0;
}

BOOL WINAPI nsGetDiskFreeSpaceW(LPCWSTR aDir, LPDWORD aSpC, LPDWORD aBpS,
                                LPDWORD aNFC, LPDWORD aTNC)
{
    nsCAutoString dir;
    const char *pDir;
    ConvertArg(aDir, dir, pDir);
    return GetDiskFreeSpaceA(pDir, aSpC, aBpS, aNFC, aTNC);
}

typedef BOOL (WINAPI *fpGetDiskFreeSpaceExA)(LPCSTR lpDirectoryName,
                                             PULARGE_INTEGER lpFreeBytesAvailableToCaller,
                                             PULARGE_INTEGER lpTotalNumberOfBytes,
                                             PULARGE_INTEGER lpTotalNumberOfFreeBytes);

BOOL WINAPI nsGetDiskFreeSpaceExW(LPCWSTR aDir, PULARGE_INTEGER aFBA,
                                  PULARGE_INTEGER aTNB, PULARGE_INTEGER aTNFB)
{
    // Attempt to check disk space using the GetDiskFreeSpaceExA function. 
    // --- FROM MSDN ---
    // The GetDiskFreeSpaceEx function is available beginning with Windows 95 OEM Service 
    // Release 2 (OSR2). To determine whether GetDiskFreeSpaceEx is available, call 
    // GetModuleHandle to get the handle to Kernel32.dll. Then you can call GetProcAddress. 
    // It is not necessary to call LoadLibrary on Kernel32.dll because it is already loaded 
    // into every process address space.
    fpGetDiskFreeSpaceExA pGetDiskFreeSpaceExA = (fpGetDiskFreeSpaceExA)
        GetProcAddress(GetModuleHandle("kernel32.dll"), "GetDiskFreeSpaceExA");
    if (!pGetDiskFreeSpaceExA)
        return 0;

    nsCAutoString dir;
    const char *pDir;
    ConvertArg(aDir, dir, pDir);
    return pGetDiskFreeSpaceExA(pDir, aFBA, aTNB, aTNFB);
}

DWORD WINAPI nsGetModuleFileNameW(HMODULE aModule, LPWSTR aName, DWORD aSize)
{
    if (!aName || aSize <= 0) {
        SetLastError(ERROR_BAD_ARGUMENTS);
        return 0;
    }

    DWORD len;
    char buf[MAX_PATH + 1];
    if (!(len = GetModuleFileNameA(aModule, buf, MAX_PATH)))
        return 0;
    // just in case, null-terminate
    buf[MAX_PATH] = '\0';

    len = NS_ConvertAtoW(buf, 0, aName);
    if (len <= aSize) 
        return (len = NS_ConvertAtoW(buf, aSize, aName)) ? len - 1 : 0;

    // buffer is not large enough. just truncate the result to the first
    // |aSize| characters.
    nsAutoString temp;
    if (!SetLengthAndCheck(temp, len))
        return 0;
    PRUnichar *pTmp = temp.BeginWriting();
    NS_ConvertAtoW(buf, len, pTmp);
    memcpy(aName, pTmp, aSize * 2);
    SetLastError(ERROR_INSUFFICIENT_BUFFER);
    return aSize;
}

wchar_t* nsGetCwdW(wchar_t *aBuf, int aMaxLen)
{
    if (aMaxLen <= 0)
        return NULL;

    // One 16-bit unit in UTF-16 can take as many as 2 bytes in legacy Windows 
    // codepage used on Windows 9x/ME (note that we don't have to worry about
    // UTF-8 and GB18030 because they're not used on Windows 9x/ME)
    char *cwdA = _getcwd(NULL, aMaxLen * 2);
    if (!cwdA)
        return NULL;

    // see if we're given a buffer large enough to hold the result
    int len = NS_ConvertAtoW(cwdA, 0, aBuf);
    if ((len  > aMaxLen && aBuf) || len == 0) {
        free(cwdA);
        return NULL;
    }

    if (!aBuf)
        // errno = ENOMEM;
        aBuf = (wchar_t *) malloc(len * sizeof(wchar_t));

    if (!aBuf) {
        free(cwdA);
        return NULL;
    }

    len = NS_ConvertAtoW(cwdA, NS_MAX(len, aMaxLen), aBuf); 
    free(cwdA);
    return len ? aBuf : NULL;
}

wchar_t* nsGetDCwdW(int aDrive, wchar_t *aBuf, int aMaxLen)
{
    if (aMaxLen <= 0)
        return NULL;

    // One 16-bit unit in UTF-16 can take as many as 2 bytes in legacy Windows codepage
    // used on Windows 9x/ME (note that we don't have to worry about 
    // UTF-8 and GB18030 because they're not used on Windows 9x/ME)
    char *cwdA = _getdcwd(aDrive, NULL, aMaxLen * 2);
    if (!cwdA)
        return NULL;

    // see if we're given a buffer large enough to hold the result
    int len = NS_ConvertAtoW(cwdA, 0, aBuf);
    if ((len  > aMaxLen && aBuf) || len == 0) {
        //errno = ERANGE;
        free(cwdA);
        return NULL;
    }

    if (!aBuf)
        aBuf = (wchar_t *) malloc(len * sizeof(wchar_t));

    if (!aBuf) {
        free(cwdA);
        return NULL;
    }

    len = NS_ConvertAtoW(cwdA, NS_MAX(len, aMaxLen), aBuf); 
    free(cwdA);
    return len ? aBuf : NULL;
}

FILE* nsFopenW(const wchar_t *aPath, const wchar_t *aMode)
{
    if (!aPath) return NULL;

    nsCAutoString pathA;
    NS_CopyUnicodeToNative(nsDependentString(aPath), pathA);

    return fopen(pathA.get(),
                 NS_LossyConvertUTF16toASCII(aMode).get());
}

int nsRemoveW(const wchar_t *aPath)
{
    if (!aPath) return -1;

    nsCAutoString pathA;
    NS_CopyUnicodeToNative(nsDependentString(aPath), pathA);

    return remove(pathA.get());
}

int nsRmdirW(const wchar_t *aPath)
{
    if (!aPath) return -1;

    nsCAutoString pathA;
    NS_CopyUnicodeToNative(nsDependentString(aPath), pathA);

    return _rmdir(pathA.get());
}

int nsChmodW(const wchar_t *aPath, int aMode)
{
    if (!aPath) return -1;

    nsCAutoString pathA;
    NS_CopyUnicodeToNative(nsDependentString(aPath), pathA);

    return _chmod(pathA.get(), aMode);
}


#ifdef WINCE
PRBool                     nsWinAPIs::sUseUnicode        = 0;
nsSHGetPathFromIDList      nsWinAPIs::mSHGetPathFromIDList = nsSHGetPathFromIDListW;
nsGetSystemDirectory       nsWinAPIs::mGetSystemDirectory = GetSystemDirectoryW;
nsGetWindowsDirectory      nsWinAPIs::mGetWindowsDirectory = GetWindowsDirectoryW;
nsGetTempPath              nsWinAPIs::mGetTempPath = GetTempPathW;
nsGetEnvironmentVariable   nsWinAPIs::mGetEnvironmentVariable = GetEnvironmentVariableW;
nsCreateDirectory          nsWinAPIs::mCreateDirectory = CreateDirectoryW;
nsCreateFile               nsWinAPIs::mCreateFile = CreateFileW;
nsShellExecute             nsWinAPIs::mShellExecute = ShellExecuteW;
nsCopyFile                 nsWinAPIs::mCopyFile = CopyFileW;
nsMoveFile                 nsWinAPIs::mMoveFile = MoveFileW;
nsGetFileVersionInfo       nsWinAPIs::mGetFileVersionInfo = nsGetFileVersionInfoW;
nsGetFileVersionInfoSize   nsWinAPIs::mGetFileVersionInfoSize = nsGetFileVersionInfoSizeW;
nsGetFileAttributes        nsWinAPIs::mGetFileAttributes = GetFileAttributesW;
nsGetFileAttributesEx      nsWinAPIs::mGetFileAttributesEx = nsGetFileAttributesExW;
nsGetFileAttributesExA     nsWinAPIs::mGetFileAttributesExA = NULL;
nsGetShortPathName         nsWinAPIs::mGetShortPathName = nsGetShortPathNameW;    
nsGetDiskFreeSpace         nsWinAPIs::mGetDiskFreeSpace = nsGetDiskFreeSpaceW;     
nsGetDiskFreeSpaceEx       nsWinAPIs::mGetDiskFreeSpaceEx = nsGetDiskFreeSpaceExW;
nsGetModuleFileName        nsWinAPIs::mGetModuleFileName = GetModuleFileNameW;
nsGetCwd                   nsWinAPIs::mGetCwd = nsGetCwdW; 
nsGetDCwd                  nsWinAPIs::mGetDCwd = nsGetDCwdW; 
nsFopen                    nsWinAPIs::mFopen = nsFopenW;
nsRemove                   nsWinAPIs::mRemove = nsRemoveW;
nsRmdir                    nsWinAPIs::mRmdir = nsRmdirW;
nsChmod                    nsWinAPIs::mChmod = nsChmodW;

#else
PRBool nsWinAPIs::sUseUnicode        = -1;  // not yet initialized

// All but three APIs are initialized to 'W' versions. 
// Exceptions are SHGetPathFromIDList, GetFileAttributesEx, GetFreeDiskSpaceEx 
// for which even stubs are not available on Win95 so that we set
// them to our own implementation initially. Yet another exception needs to be
// made for GetFileAttributesEx because even its 'A' version doesn't exist on
// Win95. The corresponding function pointer is set to NULL initially 
// and its existence is checked using GetProcAddress on Win 9x/ME.
nsSHGetPathFromIDList      nsWinAPIs::mSHGetPathFromIDList = nsSHGetPathFromIDListW;
nsGetSystemDirectory       nsWinAPIs::mGetSystemDirectory = GetSystemDirectoryW;
nsGetWindowsDirectory      nsWinAPIs::mGetWindowsDirectory = GetWindowsDirectoryW;
nsGetTempPath              nsWinAPIs::mGetTempPath = GetTempPathW;
nsGetEnvironmentVariable   nsWinAPIs::mGetEnvironmentVariable = GetEnvironmentVariableW;
nsCreateDirectory          nsWinAPIs::mCreateDirectory = CreateDirectoryW;
nsCreateFile               nsWinAPIs::mCreateFile = CreateFileW;
nsShellExecute             nsWinAPIs::mShellExecute = ShellExecuteW;
nsCopyFile                 nsWinAPIs::mCopyFile = CopyFileW;
nsMoveFile                 nsWinAPIs::mMoveFile = MoveFileW;
nsGetFileVersionInfo       nsWinAPIs::mGetFileVersionInfo = GetFileVersionInfoW;
nsGetFileVersionInfoSize   nsWinAPIs::mGetFileVersionInfoSize = GetFileVersionInfoSizeW;
nsGetFileAttributes        nsWinAPIs::mGetFileAttributes = GetFileAttributesW;
nsGetFileAttributesEx      nsWinAPIs::mGetFileAttributesEx = nsGetFileAttributesExW;
nsGetFileAttributesExA     nsWinAPIs::mGetFileAttributesExA = NULL;
nsGetShortPathName         nsWinAPIs::mGetShortPathName = GetShortPathNameW;    
nsGetDiskFreeSpace         nsWinAPIs::mGetDiskFreeSpace = GetDiskFreeSpaceW;     
nsGetDiskFreeSpaceEx       nsWinAPIs::mGetDiskFreeSpaceEx = nsGetDiskFreeSpaceExW;
nsGetModuleFileName        nsWinAPIs::mGetModuleFileName = GetModuleFileNameW;


nsGetCwd                   nsWinAPIs::mGetCwd = _wgetcwd; 
nsGetDCwd                  nsWinAPIs::mGetDCwd = _wgetdcwd; 
nsFopen                    nsWinAPIs::mFopen = _wfopen;
nsRemove                   nsWinAPIs::mRemove = _wremove;
nsRmdir                    nsWinAPIs::mRmdir = _wrmdir;
nsChmod                    nsWinAPIs::mChmod = _wchmod;

#endif

// This is a dummy variable to make sure that WinAPI is initialized 
// at the very start. Note that |sDummy| must be defined AFTER
// all the function pointers for Win APIs are defined. Otherwise,
// what's done in |GlobalInit| would have no effect.
PRBool nsWinAPIs::sDummy = nsWinAPIs::GlobalInit();

PRBool
nsWinAPIs::GlobalInit()
{
#ifndef WINCE
    // Find out if we are running on a unicode enabled version of Windows
    OSVERSIONINFOA osvi = {0};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    if (!GetVersionExA(&osvi)) {
      sUseUnicode = PR_FALSE;
    } else {
      sUseUnicode = (osvi.dwPlatformId >= VER_PLATFORM_WIN32_NT);
    }

#ifdef DEBUG
  // In debug builds, allow explicit use of ANSI methods for testing purposes.
   if (getenv("WINAPI_USE_ANSI"))
       sUseUnicode = PR_FALSE;
#endif

    HINSTANCE kernel32 = ::GetModuleHandle("kernel32.dll");

    // on Win 9x/ME, use our own implementations of 'W' APIs
    // which eventually calls 'A' APIs
    if (!sUseUnicode) {
        NS_WARNING("Windows 'A' APIs will be used !!");
        mGetSystemDirectory = nsGetSystemDirectoryW;
        mGetWindowsDirectory = nsGetWindowsDirectoryW;
        mGetTempPath = nsGetTempPathW;
        mGetEnvironmentVariable = nsGetEnvironmentVariableW;
        mCreateFile = nsCreateFileW;
        mCreateDirectory = nsCreateDirectoryW;
        mShellExecute = nsShellExecuteW;
        mCopyFile = nsCopyFileW;
        mMoveFile = nsMoveFileW;
        mGetFileVersionInfo = nsGetFileVersionInfoW;
        mGetFileVersionInfoSize = nsGetFileVersionInfoSizeW;
        mGetFileAttributes = nsGetFileAttributesW;
        mGetShortPathName = nsGetShortPathNameW;
        mGetDiskFreeSpace = nsGetDiskFreeSpaceW;
        mGetModuleFileName = nsGetModuleFileNameW;
       
        mGetCwd = nsGetCwdW;
        mGetDCwd = nsGetDCwdW;
        mFopen  = nsFopenW;
        mRemove  = nsRemoveW;
        mRmdir  = nsRmdirW;
        mChmod  = nsChmodW;

        // absent on Win 95. 
        mGetFileAttributesExA = (nsGetFileAttributesExA)
            GetProcAddress(kernel32, "GetFileAttributesExA");
    } 
    else {
        HINSTANCE shell32 = LoadLibrary("Shell32.dll");
        if (shell32) 
            mSHGetPathFromIDList = (nsSHGetPathFromIDList)
                GetProcAddress(shell32, "SHGetPathFromIDListW");
        mGetFileAttributesEx = (nsGetFileAttributesEx)
            GetProcAddress(kernel32, "GetFileAttributesExW");
        mGetDiskFreeSpaceEx = (nsGetDiskFreeSpaceEx)
            GetProcAddress(kernel32, "GetDiskFreeSpaceExW");
        NS_ASSERTION(mSHGetPathFromIDList && mGetFileAttributesEx &&
                     mGetDiskFreeSpaceEx, "failed to get proc. addresses");
    }

#endif // WINCE
    return PR_TRUE;
}

NS_COM PRBool 
NS_UseUnicode()
{
    return nsWinAPIs::sUseUnicode; 
}
