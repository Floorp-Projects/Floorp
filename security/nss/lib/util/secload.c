/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "secport.h"
#include "nspr.h"

#ifdef XP_UNIX
#include <unistd.h>
#define BL_MAXSYMLINKS 20

/*
 * If 'link' is a symbolic link, this function follows the symbolic links
 * and returns the pathname of the ultimate source of the symbolic links.
 * If 'link' is not a symbolic link, this function returns NULL.
 * The caller should call PR_Free to free the string returned by this
 * function.
 */
static char* loader_GetOriginalPathname(const char* link)
{
    char* resolved = NULL;
    char* input = NULL;
    PRUint32 iterations = 0;
    PRInt32 len = 0, retlen = 0;
    if (!link) {
        PR_SetError(PR_INVALID_ARGUMENT_ERROR, 0);
        return NULL;
    }
    len = PR_MAX(1024, strlen(link) + 1);
    resolved = PR_Malloc(len);
    input = PR_Malloc(len);
    if (!resolved || !input) {
        if (resolved) {
            PR_Free(resolved);
        }
        if (input) {
            PR_Free(input);
        }
        return NULL;
    }
    strcpy(input, link);
    while ( (iterations++ < BL_MAXSYMLINKS) &&
            ( (retlen = readlink(input, resolved, len - 1)) > 0) ) {
        char* tmp = input;
        resolved[retlen] = '\0'; /* NULL termination */
        input = resolved;
        resolved = tmp;
    }
    PR_Free(resolved);
    if (iterations == 1 && retlen < 0) {
        PR_Free(input);
        input = NULL;
    }
    return input;
}
#endif /* XP_UNIX */

/*
 * Load the library with the file name 'name' residing in the same
 * directory as the reference library, whose pathname is 'referencePath'.
 */
static PRLibrary *
loader_LoadLibInReferenceDir(const char *referencePath, const char *name)
{
    PRLibrary *dlh = NULL;
    char *fullName = NULL;
    char* c;
    PRLibSpec libSpec;

    /* Remove the trailing filename from referencePath and add the new one */
    c = strrchr(referencePath, PR_GetDirectorySeparator());
    if (c) {
        size_t referencePathSize = 1 + c - referencePath;
        fullName = (char*) PORT_Alloc(strlen(name) + referencePathSize + 1);
        if (fullName) {
            memcpy(fullName, referencePath, referencePathSize);
            strcpy(fullName + referencePathSize, name); 
#ifdef DEBUG_LOADER
            PR_fprintf(PR_STDOUT, "\nAttempting to load fully-qualified %s\n", 
                       fullName);
#endif
            libSpec.type = PR_LibSpec_Pathname;
            libSpec.value.pathname = fullName;
            dlh = PR_LoadLibraryWithFlags(libSpec, PR_LD_NOW | PR_LD_LOCAL
#ifdef PR_LD_ALT_SEARCH_PATH
            /* allow library's dependencies to be found in the same directory
             * on Windows even if PATH is not set. Requires NSPR 4.8.1 . */
                                          | PR_LD_ALT_SEARCH_PATH 
#endif
                                          );
            PORT_Free(fullName);
        }
    }
    return dlh;
}

/*
 * Load a shared library called "newShLibName" in the same directory as
 * a shared library that is already loaded, called existingShLibName.
 * A pointer to a static function in that shared library,
 * staticShLibFunc, is required.
 *
 * existingShLibName:
 *   The file name of the shared library that shall be used as the 
 *   "reference library". The loader will attempt to load the requested
 *   library from the same directory as the reference library.
 *
 * staticShLibFunc:
 *   Pointer to a static function in the "reference library".
 *
 * newShLibName:
 *   The simple file name of the new shared library to be loaded.
 *
 * We use PR_GetLibraryFilePathname to get the pathname of the loaded 
 * shared lib that contains this function, and then do a
 * PR_LoadLibraryWithFlags with an absolute pathname for the shared
 * library to be loaded.
 *
 * On Windows, the "alternate search path" strategy is employed, if available.
 * On Unix, if existingShLibName is a symbolic link, and no link exists for the
 * new library, the original link will be resolved, and the new library loaded
 * from the resolved location.
 *
 * If the new shared library is not found in the same location as the reference
 * library, it will then be loaded from the normal system library path.
 *
 */

PRLibrary *
PORT_LoadLibraryFromOrigin(const char* existingShLibName,
                 PRFuncPtr staticShLibFunc,
                 const char *newShLibName)
{
    PRLibrary *lib = NULL;
    char* fullPath = NULL;
    PRLibSpec libSpec;

    /* Get the pathname for existingShLibName, e.g. /usr/lib/libnss3.so
     * PR_GetLibraryFilePathname works with either the base library name or a
     * function pointer, depending on the platform.
     * We require the address of a function in the "reference library",
     * provided by the caller. To avoid getting the address of the stub/thunk
     * of an exported function by accident, use the address of a static
     * function rather than an exported function.
     */
    fullPath = PR_GetLibraryFilePathname(existingShLibName,
                                         staticShLibFunc);

    if (fullPath) {
        lib = loader_LoadLibInReferenceDir(fullPath, newShLibName);
#ifdef XP_UNIX
        if (!lib) {
            /*
             * If fullPath is a symbolic link, resolve the symbolic
             * link and try again.
             */
            char* originalfullPath = loader_GetOriginalPathname(fullPath);
            if (originalfullPath) {
                PR_Free(fullPath);
                fullPath = originalfullPath;
                lib = loader_LoadLibInReferenceDir(fullPath, newShLibName);
            }
        }
#endif
        PR_Free(fullPath);
    }
    if (!lib) {
#ifdef DEBUG_LOADER
        PR_fprintf(PR_STDOUT, "\nAttempting to load %s\n", newShLibName);
#endif
        libSpec.type = PR_LibSpec_Pathname;
        libSpec.value.pathname = newShLibName;
        lib = PR_LoadLibraryWithFlags(libSpec, PR_LD_NOW | PR_LD_LOCAL);
    }
    if (NULL == lib) {
#ifdef DEBUG_LOADER
        PR_fprintf(PR_STDOUT, "\nLoading failed : %s.\n", newShLibName);
#endif
    }
    return lib;
}

