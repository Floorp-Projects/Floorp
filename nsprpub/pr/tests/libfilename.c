/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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

int main(int argc, char **argv)
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
