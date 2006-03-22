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

#ifndef nsWinAPIs_h__
#define nsWinAPIs_h__

#include "prtypes.h"
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>

typedef BOOL (WINAPI * nsSHGetPathFromIDList) (LPCITEMIDLIST pidList, LPWSTR pszPath);
typedef UINT (WINAPI * nsGetSystemDirectory) (LPWSTR lpBuffer, UINT usize);
typedef UINT (WINAPI * nsGetWindowsDirectory) (LPWSTR lpBuffer, UINT usize);
typedef DWORD (WINAPI * nsGetTempPath) (DWORD len, LPWSTR lpBuffer);
typedef DWORD (WINAPI * nsGetEnvironmentVariable) (LPCWSTR lpName, LPWSTR lpBuffer, DWORD nSize);
typedef BOOL (WINAPI * nsCreateDirectory) (LPCWSTR aDir,
                                           LPSECURITY_ATTRIBUTES aSecAttr);
typedef HANDLE (WINAPI * nsCreateFile) (LPCWSTR aPath, DWORD aAccess,
                                        DWORD aMode, 
                                        LPSECURITY_ATTRIBUTES aSecAttr,
                                        DWORD aCreat, DWORD aFlags,
                                        HANDLE aTemplate);
typedef HINSTANCE (WINAPI * nsShellExecute) (HWND hwnd, LPCWSTR lpOperation,
                                             LPCWSTR lpFile,
                                             LPCWSTR lpParameters,
                                             LPCWSTR lpDirectory,
                                             INT nShowCmd);
typedef BOOL (WINAPI * nsCopyFile) (LPCWSTR aSrc, LPCWSTR aDest, BOOL aNoOverwrite);
typedef BOOL (WINAPI * nsMoveFile) (LPCWSTR aSrc, LPCWSTR aDest);
typedef BOOL (WINAPI * nsGetFileVersionInfo) (LPCWSTR aPath, DWORD aHandle, DWORD aLen,
                                              LPVOID aData);
typedef DWORD (WINAPI * nsGetFileVersionInfoSize) (LPCWSTR aPath, LPDWORD aHandle);
typedef DWORD (WINAPI * nsGetFileAttributes) (LPCWSTR aPath);
typedef BOOL (WINAPI * nsGetFileAttributesEx) (LPCWSTR aPath, GET_FILEEX_INFO_LEVELS aLevel,
                                               LPVOID aInfo);
typedef BOOL (WINAPI * nsGetFileAttributesExA) (LPCSTR aPath, GET_FILEEX_INFO_LEVELS aLevel,
                                                LPVOID aInfo);
typedef DWORD (WINAPI * nsGetShortPathName) (LPCWSTR aLPATH, LPWSTR aSPATH, DWORD aLen);
typedef BOOL (WINAPI * nsGetDiskFreeSpace) (LPCWSTR aDir, LPDWORD aSpC, 
                                            LPDWORD aBpS, LPDWORD aNFC, LPDWORD aTNC);
typedef BOOL (WINAPI * nsGetDiskFreeSpaceEx) (LPCWSTR aDir, 
                                              PULARGE_INTEGER aFBA,
                                              PULARGE_INTEGER aTNB,
                                              PULARGE_INTEGER aTNFB);
typedef DWORD (WINAPI * nsGetModuleFileName) (HMODULE aModule, LPWSTR aName, DWORD aSize);

typedef wchar_t* (*nsGetCwd) (wchar_t *buffer, int maxlen);
typedef wchar_t* (*nsGetDCwd) (int aDrive, wchar_t *aPath, int aMaxLen);
typedef FILE* (*nsFopen) (const wchar_t *aName, const wchar_t *aMode);
typedef int (*nsRemove) (const wchar_t *aPath);
typedef int (*nsRmdir) (const wchar_t *aPath);
typedef int (*nsChmod) (const wchar_t *aName, int pmode);

class nsWinAPIs {

public: 
    static nsSHGetPathFromIDList      mSHGetPathFromIDList;
    static nsGetSystemDirectory       mGetSystemDirectory;
    static nsGetWindowsDirectory      mGetWindowsDirectory;
    static nsGetTempPath              mGetTempPath;
    static nsGetEnvironmentVariable   mGetEnvironmentVariable;
    static nsCreateDirectory          mCreateDirectory;
    static nsCreateFile               mCreateFile;
    static nsShellExecute             mShellExecute;
    static nsCopyFile                 mCopyFile;
    static nsMoveFile                 mMoveFile;
    static nsGetFileVersionInfo       mGetFileVersionInfo;
    static nsGetFileVersionInfoSize   mGetFileVersionInfoSize;
    static nsGetFileAttributes        mGetFileAttributes;
    static nsGetFileAttributesEx      mGetFileAttributesEx;
    static nsGetFileAttributesExA     mGetFileAttributesExA;
    static nsGetShortPathName         mGetShortPathName;    
    static nsGetDiskFreeSpace         mGetDiskFreeSpace;     
    static nsGetDiskFreeSpaceEx       mGetDiskFreeSpaceEx;     
    static nsGetModuleFileName        mGetModuleFileName;


    static nsGetCwd                   mGetCwd;
    static nsFopen                    mFopen;
    static nsGetDCwd                  mGetDCwd;
    static nsRemove                   mRemove;
    static nsRmdir                    mRmdir;
    static nsChmod                    mChmod;

private:
    nsWinAPIs() {}
    static PRBool GlobalInit(); 
    static PRBool sUseUnicode; 
    friend NS_COM PRBool NS_UseUnicode();
    friend NS_COM void NS_StartupWinAPIs();

    // a dummy variable to make sure that |GlobalInit| is invoked
    // before the first use |nsLocalFileWin| that relies on Windows APIs 
    // set up in |GlobalInit|
    static PRBool sDummy;
};

NS_COM PRBool NS_UseUnicode();

#endif
