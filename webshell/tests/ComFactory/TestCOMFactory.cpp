/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// Standard Windows/COM headers
#include <windows.h>
#include <ole2.h>

// NGLayout headers...
#include "nsIWebShell.h"

//
// Define GUIDS for the WebShell class and IWebshell interface...
//
static GUID kWebShell_CID  = NS_WEB_SHELL_CID;
static GUID kIWebShell_IID = NS_IWEB_SHELL_IID;


int main(int argc, char **argv)
{
    HRESULT hr;
    nsIWebShell* webShell;

    OleInitialize(NULL);

    hr = CoCreateInstance(kWebShell_CID, 
                          NULL, 
                          CLSCTX_INPROC_SERVER,
                          kIWebShell_IID,
                          (void**)&webShell);

    if (S_OK == hr) {
        printf("CoCreateInstance(...) succeeded.\n");

        webShell->Release();
    } else if (REGDB_E_CLASSNOTREG == hr) {
        printf("Error: No registry entry.  Try running REGSVR32 raptorweb.dll\n");
    } else {
        printf("Error: CoCreateInstance(...) failed.\n");
    }

    OleUninitialize();
    return 0;
}
