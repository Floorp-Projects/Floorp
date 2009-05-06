/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jim Mathies <jmathies@mozilla.com>.
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

/*
 * Test: 
 */

#include "../TestHarness.h"
#include "nsEmbedString.h"
#include "nsILocalFileWin.h"
#include <windows.h>

#define BUFFSIZE 512

BOOL IsXPOrGreater()
{
  OSVERSIONINFO osvi;

  ZeroMemory(&osvi, sizeof(OSVERSIONINFO));
  osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

  GetVersionEx(&osvi);

  return ( (osvi.dwMajorVersion > 5) ||
     ( (osvi.dwMajorVersion == 5) && (osvi.dwMinorVersion >= 1) ));
}

nsresult TestWinAttribs()
{
    if (!IsXPOrGreater())
      return NS_OK;
  
    printf("Is XP or greater, running tests...\n");

    nsresult rv;

    // File variables
    HANDLE hIndexed;
    nsCOMPtr<nsILocalFile> localFile;
    WCHAR filePath[MAX_PATH];

    // Create and open temporary file
    hIndexed = CreateFileW(L".\\indexbit.txt", 
                            GENERIC_READ | GENERIC_WRITE,
                            0, 
                            NULL,
                            CREATE_ALWAYS,        
                            FILE_ATTRIBUTE_NORMAL, //FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, not supported by cf
                            NULL);  

    if(hIndexed == INVALID_HANDLE_VALUE)
    {
        fail("Test Win Attribs: Creating Test File");
        return NS_ERROR_FAILURE;
    }

    CloseHandle(hIndexed);

    GetFullPathNameW((LPCWSTR)L".\\indexbit.txt", 
                        MAX_PATH, filePath, NULL);

    //wprintf(filePath);
    //wprintf(L"\n");

    rv = NS_NewLocalFile(nsEmbedString(filePath), PR_FALSE,
                         getter_AddRefs(localFile));
    if (NS_FAILED(rv))
    {
        fail("Test Win Attribs: Opening Test File");
        DeleteFileW(filePath);
        return rv;
    }

    nsCOMPtr<nsILocalFileWin> localFileWin(do_QueryInterface(localFile));

    DWORD dwAttrs = GetFileAttributesW(filePath);
    if (dwAttrs == INVALID_FILE_ATTRIBUTES)
    {
        fail("Test Win Attribs: GetFileAttributesW - couldn't find our temp file.");
        DeleteFileW(filePath);
        return NS_ERROR_FAILURE;
    }

    dwAttrs |= FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    SetFileAttributesW(filePath, dwAttrs);

    PRUint32 attribs = 0;
    rv = localFileWin->GetFileAttributesWin(&attribs);
    
    if (NS_FAILED(rv))
    {
        fail("Test Win Attribs: GetFileAttributesWin failed to GET attributes. (1)");
        DeleteFileW(filePath);
        return NS_ERROR_FAILURE;
    }
    
    if (attribs & nsILocalFileWin::WFA_SEARCH_INDEXED)
    {
        fail("Test Win Attribs: GetFileAttributesWin attributed did not match. (2)");
        DeleteFileW(filePath);
        return NS_ERROR_FAILURE;
    }

    dwAttrs &= ~FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    SetFileAttributesW(filePath, dwAttrs);

    rv = localFileWin->GetFileAttributesWin(&attribs);
    
    if (NS_FAILED(rv))
    {
        fail("Test Win Attribs: GetFileAttributesWin failed to GET attributes. (3)");
        DeleteFileW(filePath);
        return NS_ERROR_FAILURE;
    }

    if (!(attribs & nsILocalFileWin::WFA_SEARCH_INDEXED))
    {
        fail("Test Win Attribs: GetFileAttributesWin attributed did not match. (4)");
        DeleteFileW(filePath);
        return NS_ERROR_FAILURE;
    }

    dwAttrs &= ~FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    SetFileAttributesW(filePath, dwAttrs);

    attribs = nsILocalFileWin::WFA_SEARCH_INDEXED;
    rv = localFileWin->SetFileAttributesWin(attribs);

    dwAttrs = GetFileAttributesW(filePath);

    if (NS_FAILED(rv))
    {
        fail("Test Win Attribs: GetFileAttributesWin failed to SET attributes. (5)");
        DeleteFileW(filePath);
        return NS_ERROR_FAILURE;
    }

    if (dwAttrs & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED)
    {
        fail("Test Win Attribs: SetFileAttributesWin attributed did not match. (6)");
        DeleteFileW(filePath);
        return NS_ERROR_FAILURE;
    }
    
    dwAttrs |= FILE_ATTRIBUTE_NOT_CONTENT_INDEXED;
    SetFileAttributesW(filePath, dwAttrs);

    attribs = 0;
    rv = localFileWin->SetFileAttributesWin(attribs);

    dwAttrs = GetFileAttributesW(filePath);

    if (NS_FAILED(rv))
    {
        fail("Test Win Attribs: GetFileAttributesWin failed to SET attributes. (7)");
        DeleteFileW(filePath);
        return NS_ERROR_FAILURE;
    }

    if (!(dwAttrs & FILE_ATTRIBUTE_NOT_CONTENT_INDEXED))
    {
        fail("Test Win Attribs: SetFileAttributesWin attributed did not match. (8)");
        DeleteFileW(filePath);
        return NS_ERROR_FAILURE;
    }

    DeleteFileW(filePath);

    passed("Test Win Attribs: passed tests.");

    return NS_OK;
}

int main(int argc, char** argv)
{
    ScopedXPCOM xpcom("WinFileAttributes");
    if (xpcom.failed())
        return 1;

    int rv = 0;

    if(NS_FAILED(TestWinAttribs()))
        rv = 1;

    return rv;

}

