/* -*- Mode: C++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlueLinking.h"
#include "nsXPCOMGlue.h"
#include "mozilla/FileUtils.h"

#include <mach-o/dyld.h>
#include <sys/param.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <limits.h>

static const mach_header* sXULLibImage;

static void
ReadDependentCB(const char *aDependentLib, bool do_preload)
{
    if (do_preload)
        mozilla::ReadAheadLib(aDependentLib);
    (void) NSAddImage(aDependentLib,
                      NSADDIMAGE_OPTION_RETURN_ON_ERROR |
                      NSADDIMAGE_OPTION_MATCH_FILENAME_BY_INSTALLNAME);
}

static void*
LookupSymbol(const mach_header* aLib, const char* aSymbolName)
{
    // Try to use |NSLookupSymbolInImage| since it is faster than searching
    // the global symbol table.  If we couldn't get a mach_header pointer
    // for the XPCOM dylib, then use |NSLookupAndBindSymbol| to search the
    // global symbol table (this shouldn't normally happen, unless the user
    // has called XPCOMGlueStartup(".") and relies on libxpcom.dylib
    // already being loaded).
    NSSymbol sym = nullptr;
    if (aLib) {
        sym = NSLookupSymbolInImage(aLib, aSymbolName,
                                 NSLOOKUPSYMBOLINIMAGE_OPTION_BIND |
                                 NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
    } else {
        if (NSIsSymbolNameDefined(aSymbolName))
            sym = NSLookupAndBindSymbol(aSymbolName);
    }
    if (!sym)
        return nullptr;

    return NSAddressOfSymbol(sym);
}

nsresult
XPCOMGlueLoad(const char *xpcomFile, GetFrozenFunctionsFunc *func)
{
    const mach_header* lib = nullptr;

    if (xpcomFile[0] != '.' || xpcomFile[1] != '\0') {
        char xpcomDir[PATH_MAX];
        if (realpath(xpcomFile, xpcomDir)) {
            char *lastSlash = strrchr(xpcomDir, '/');
            if (lastSlash) {
                *lastSlash = '\0';

                XPCOMGlueLoadDependentLibs(xpcomDir, ReadDependentCB);

                snprintf(lastSlash, PATH_MAX - strlen(xpcomDir), "/" XUL_DLL);

                sXULLibImage = NSAddImage(xpcomDir,
                              NSADDIMAGE_OPTION_RETURN_ON_ERROR |
                              NSADDIMAGE_OPTION_WITH_SEARCHING |
                              NSADDIMAGE_OPTION_MATCH_FILENAME_BY_INSTALLNAME);
            }
        }

        lib = NSAddImage(xpcomFile,
                         NSADDIMAGE_OPTION_RETURN_ON_ERROR |
                         NSADDIMAGE_OPTION_WITH_SEARCHING |
                         NSADDIMAGE_OPTION_MATCH_FILENAME_BY_INSTALLNAME);

        if (!lib) {
            NSLinkEditErrors linkEditError;
            int errorNum;
            const char *errorString;
            const char *fileName;
            NSLinkEditError(&linkEditError, &errorNum, &fileName, &errorString);
            fprintf(stderr, "XPCOMGlueLoad error %d:%d for file %s:\n%s\n",
                    linkEditError, errorNum, fileName, errorString);
        }
    }

    *func = (GetFrozenFunctionsFunc) LookupSymbol(lib, "_NS_GetFrozenFunctions");

    return *func ? NS_OK : NS_ERROR_NOT_AVAILABLE;
}

void
XPCOMGlueUnload()
{
  // nothing to do, since we cannot unload dylibs on OS X
}

nsresult
XPCOMGlueLoadXULFunctions(const nsDynamicFunctionLoad *symbols)
{
    nsresult rv = NS_OK;
    while (symbols->functionName) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer), "_%s", symbols->functionName);

        *symbols->function = (NSFuncPtr) LookupSymbol(sXULLibImage, buffer);
        if (!*symbols->function)
            rv = NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;

        ++symbols;
    }

    return rv;
}
