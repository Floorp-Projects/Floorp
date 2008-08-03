/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Netscape Portable Runtime (NSPR).
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
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

/***********************************************************************
**
** Name: libfilename.c
**
** Description: test PR_GetLibraryFilePathname.
**
***********************************************************************/

#include "nspr.h"
#include "pprio.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PRBool debug_mode = PR_FALSE;

static PRStatus RunTest(const char *name, PRFuncPtr addr)
{
    char *pathname;
    PRFileDesc *fd;

    pathname = PR_GetLibraryFilePathname(name, addr);
    if (pathname == NULL) {
        fprintf(stderr, "PR_GetLibraryFilePathname failed\n");
        /* we let this test pass if this function is not implemented */
        if (PR_GetError() == PR_NOT_IMPLEMENTED_ERROR) {
            return PR_SUCCESS;
        }
        return PR_FAILURE;
    }

    if (debug_mode) printf("Pathname is %s\n", pathname);
    fd = PR_OpenFile(pathname, PR_RDONLY, 0);
    if (fd == NULL) {
        fprintf(stderr, "PR_Open failed: %d\n", (int)PR_GetError());
        return PR_FAILURE;
    }
    if (PR_Close(fd) == PR_FAILURE) {
        fprintf(stderr, "PR_Close failed: %d\n", (int)PR_GetError());
        return PR_FAILURE;
    }
    PR_Free(pathname);
    return PR_SUCCESS;
}

int main(int argc, char *argv[])
{
    char *name;
    PRFuncPtr addr;
    PRLibrary *lib;
    PRBool failed = PR_FALSE;

    if (argc >= 2 && strcmp(argv[1], "-d") == 0) {
        debug_mode = PR_TRUE;
    }

    /* First test a library that is implicitly linked. */
#ifdef WINNT
    name = PR_Malloc(strlen("libnspr4.dll")+1);
    strcpy(name, "libnspr4.dll");
#else
    name = PR_GetLibraryName(NULL, "nspr4");
#endif
    addr = (PRFuncPtr)PR_GetTCPMethods()->close;
    if (RunTest(name, addr) == PR_FAILURE) {
        failed = PR_TRUE;
    }
    PR_FreeLibraryName(name);

    /* Next test a library that is dynamically loaded. */
    name = PR_GetLibraryName("dll", "my");
    if (debug_mode) printf("Loading library %s\n", name);
    lib = PR_LoadLibrary(name);
    if (!lib) {
        fprintf(stderr, "PR_LoadLibrary failed\n");
        exit(1);
    }
    PR_FreeLibraryName(name);
    name = PR_GetLibraryName(NULL, "my");
    addr = PR_FindFunctionSymbol(lib, "My_GetValue");
    if (RunTest(name, addr) == PR_FAILURE) {
        failed = PR_TRUE;
    }
    PR_FreeLibraryName(name);
    PR_UnloadLibrary(lib);
    if (failed) {
        printf("FAIL\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
