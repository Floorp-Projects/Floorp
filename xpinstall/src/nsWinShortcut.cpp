/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code,
 * released March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Sean Su <ssu@netscape.com>
 */

/* This is a hack for vc5.0.  It needs to be set *before* any shell
 * include files. The INITGUID definition turns off the inclusion
 * of shlguid.h in shlobj.h so it has to be done explicitly.
 */
#if (_MSC_VER == 1100)
#define INITGUID
#include "objbase.h"
DEFINE_OLEGUID(IID_IPersistFile, 0x0000010BL, 0, 0);
#endif

#include <shlobj.h>
#include <shlguid.h>
#include "nsWinShortcut.h"

HRESULT CreateALink(LPCSTR lpszPathObj, LPCSTR lpszPathLink, LPCSTR lpszDesc, LPCSTR lpszWorkingPath, LPCSTR lpszArgs, LPCSTR lpszIconFullPath, int iIcon)
{ 
    HRESULT    hres; 
    IShellLink *psl;
    char       lpszFullPath[MAX_BUF];

    lstrcpy(lpszFullPath, lpszPathLink);
    lstrcat(lpszFullPath, "\\");
    lstrcat(lpszFullPath, lpszDesc);
    lstrcat(lpszFullPath, ".lnk");

    CreateDirectory(lpszPathLink, NULL);
    CoInitialize(NULL);

    // Get a pointer to the IShellLink interface. 
    hres = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID *)&psl); 
    if(SUCCEEDED(hres))
    { 
        IPersistFile* ppf; 
 
        // Set the path to the shortcut target, and add the 
        // description. 
        psl->SetPath(lpszPathObj);

        // Do not set the description at this time.  We need to fix this
        // parameter so it can be passed in independent of the shortcut name
        // itself.  Comment this code out for now until a real fix can be done.
        // psl->SetDescription(lpszDesc);

        if(lpszWorkingPath)
            psl->SetWorkingDirectory(lpszWorkingPath);
        if(lpszArgs)
            psl->SetArguments(lpszArgs);
        psl->SetIconLocation(lpszIconFullPath, iIcon);
 
        // Query IShellLink for the IPersistFile interface for saving the 
        // shortcut in persistent storage. 
        hres = psl->QueryInterface(IID_IPersistFile, (LPVOID FAR *)&ppf); 
        if(SUCCEEDED(hres))
        { 
            WORD wsz[MAX_BUF]; 
 
            // Ensure that the string is ANSI. 
            MultiByteToWideChar(CP_ACP, 0, lpszFullPath, -1, (wchar_t *)wsz, MAX_BUF);
 
            // Save the link by calling IPersistFile::Save. 
            hres = ppf->Save((wchar_t *)wsz, TRUE); 
            ppf->Release(); 
        } 
        psl->Release(); 
    } 
    CoUninitialize();

    return hres;
}

