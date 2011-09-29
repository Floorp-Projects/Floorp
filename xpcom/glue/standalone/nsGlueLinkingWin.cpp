/* -*- Mode: C++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla XPCOM.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsGlueLinking.h"
#include "nsXPCOMGlue.h"

#include "nsStringAPI.h"
#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <tchar.h>

#define MOZ_LOADLIBRARY_FLAGS LOAD_WITH_ALTERED_SEARCH_PATH

struct DependentLib
{
    HINSTANCE     libHandle;
    DependentLib *next;
};

static DependentLib *sTop;
HINSTANCE sXULLibrary;

static void
AppendDependentLib(HINSTANCE libHandle)
{
    DependentLib *d = new DependentLib;
    if (!d)
        return;

    d->next = sTop;
    d->libHandle = libHandle;

    sTop = d;
}

static void
preload(LPCWSTR dll)
{
    HANDLE fd = CreateFileW(dll, GENERIC_READ, FILE_SHARE_READ,
                            NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    char buf[64 * 1024];

    if (fd == INVALID_HANDLE_VALUE)
        return;
  
    DWORD dwBytesRead;
    // Do dummy reads to trigger kernel-side readhead via FILE_FLAG_SEQUENTIAL_SCAN.
    // Abort when underfilling because during testing the buffers are read fully
    // A buffer that's not keeping up would imply that readahead isn't working right
    while (ReadFile(fd, buf, sizeof(buf), &dwBytesRead, NULL) && dwBytesRead == sizeof(buf))
        /* Nothing */;
  
    CloseHandle(fd);
}

static void
ReadDependentCB(const char *aDependentLib, bool do_preload)
{
    wchar_t wideDependentLib[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, aDependentLib, -1, wideDependentLib, MAX_PATH);

    if (do_preload)
        preload(wideDependentLib);

    HINSTANCE h =
        LoadLibraryExW(wideDependentLib, NULL, MOZ_LOADLIBRARY_FLAGS);

    if (!h)
        return;

    AppendDependentLib(h);
}

// like strpbrk but finds the *last* char, not the first
static char*
ns_strrpbrk(char *string, const char *strCharSet)
{
    char *found = NULL;
    for (; *string; ++string) {
        for (const char *search = strCharSet; *search; ++search) {
            if (*search == *string) {
                found = string;
                // Since we're looking for the last char, we save "found"
                // until we're at the end of the string.
            }
        }
    }

    return found;
}
// like strpbrk but finds the *last* char, not the first
static wchar_t*
ns_wcspbrk(wchar_t *string, const wchar_t *strCharSet)
{
    wchar_t *found = NULL;
    for (; *string; ++string) {
        for (const wchar_t *search = strCharSet; *search; ++search) {
            if (*search == *string) {
                found = string;
                // Since we're looking for the last char, we save "found"
                // until we're at the end of the string.
            }
        }
    }

    return found;
}

bool ns_isRelPath(wchar_t* path)
{
    return !(path[1] == ':');
}

nsresult
XPCOMGlueLoad(const char *aXpcomFile, GetFrozenFunctionsFunc *func)
{
    wchar_t xpcomFile[MAXPATHLEN];
    MultiByteToWideChar(CP_UTF8, 0, aXpcomFile,-1,
                        xpcomFile, MAXPATHLEN);
   
    
    if (xpcomFile[0] == '.' && xpcomFile[1] == '\0') {
        wcscpy(xpcomFile, LXPCOM_DLL);
    }
    else {
        wchar_t xpcomDir[MAXPATHLEN];
        
        if (ns_isRelPath(xpcomFile))
        {
            _wfullpath(xpcomDir, xpcomFile, sizeof(xpcomDir)/sizeof(wchar_t));
        } 
        else 
        {
            wcscpy(xpcomDir, xpcomFile);
        }
        wchar_t *lastSlash = ns_wcspbrk(xpcomDir, L"/\\");
        if (lastSlash) {
            *lastSlash = '\0';
            char xpcomDir_narrow[MAXPATHLEN];
            WideCharToMultiByte(CP_UTF8, 0, xpcomDir,-1,
                                xpcomDir_narrow, MAX_PATH, NULL, NULL);

            XPCOMGlueLoadDependentLibs(xpcomDir_narrow, ReadDependentCB);
            
            _snwprintf(lastSlash, MAXPATHLEN - wcslen(xpcomDir), L"\\" LXUL_DLL);
            sXULLibrary =
                LoadLibraryExW(xpcomDir, NULL, MOZ_LOADLIBRARY_FLAGS);

            if (!sXULLibrary) 
            {
                DWORD err = GetLastError();
#ifdef DEBUG
                LPVOID lpMsgBuf;
                FormatMessage(
                              FORMAT_MESSAGE_ALLOCATE_BUFFER |
                              FORMAT_MESSAGE_FROM_SYSTEM |
                              FORMAT_MESSAGE_IGNORE_INSERTS,
                              NULL,
                              GetLastError(),
                              MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                              (LPTSTR) &lpMsgBuf,
                              0,
                              NULL
                              );
                wprintf(L"Error loading %s: %s\n", xpcomDir, lpMsgBuf);
                LocalFree(lpMsgBuf);
#endif //DEBUG
                return (err == ERROR_NOT_ENOUGH_MEMORY || err == ERROR_OUTOFMEMORY)
                    ? NS_ERROR_OUT_OF_MEMORY : NS_ERROR_FAILURE;
            }
        }
    }
    HINSTANCE h =
        LoadLibraryExW(xpcomFile, NULL, MOZ_LOADLIBRARY_FLAGS);

    if (!h) 
    {
        DWORD err = GetLastError();
#ifdef DEBUG
        LPVOID lpMsgBuf;
        FormatMessage(
                      FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      NULL,
                      err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR) &lpMsgBuf,
                      0,
                      NULL
                      );
        wprintf(L"Error loading %s: %s\n", xpcomFile, lpMsgBuf);
        LocalFree(lpMsgBuf);
#endif        
        return (err == ERROR_NOT_ENOUGH_MEMORY || err == ERROR_OUTOFMEMORY)
            ? NS_ERROR_OUT_OF_MEMORY : NS_ERROR_FAILURE;
    }

    AppendDependentLib(h);

    GetFrozenFunctionsFunc sym =
        (GetFrozenFunctionsFunc) GetProcAddress(h, "NS_GetFrozenFunctions");

    if (!sym) { // No symbol found.
        XPCOMGlueUnload();
        return NS_ERROR_NOT_AVAILABLE;
    }

    *func = sym;

    return NS_OK;
}

void
XPCOMGlueUnload()
{
    while (sTop) {
        FreeLibrary(sTop->libHandle);

        DependentLib *temp = sTop;
        sTop = sTop->next;

        delete temp;
    }

    if (sXULLibrary) {
        FreeLibrary(sXULLibrary);
        sXULLibrary = nsnull;
    }
}

nsresult
XPCOMGlueLoadXULFunctions(const nsDynamicFunctionLoad *symbols)
{
    if (!sXULLibrary)
        return NS_ERROR_NOT_INITIALIZED;

    nsresult rv = NS_OK;
    while (symbols->functionName) {
        *symbols->function = 
            (NSFuncPtr) GetProcAddress(sXULLibrary, symbols->functionName);
        if (!*symbols->function)
            rv = NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;

        ++symbols;
    }

    return rv;
}
