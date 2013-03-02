/* -*- Mode: C++; tab-width: 6; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGlueLinking.h"
#include "nsXPCOMGlue.h"
#include "nscore.h"
#include "mozilla/FileUtils.h"

#if defined(LINUX) && !defined(ANDROID)
#define _GNU_SOURCE
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>
#endif

#include <errno.h>
#include <dlfcn.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(SUNOS4) || defined(NEXTSTEP) || \
    (defined(OPENBSD) || defined(NETBSD)) && !defined(__ELF__)
#define LEADING_UNDERSCORE "_"
#else
#define LEADING_UNDERSCORE
#endif

#if defined(MOZ_LINKER) && !defined(ANDROID)
extern "C" {
NS_HIDDEN __typeof(dlopen) __wrap_dlopen;
NS_HIDDEN __typeof(dlsym) __wrap_dlsym;
NS_HIDDEN __typeof(dlclose) __wrap_dlclose;
}

#define dlopen __wrap_dlopen
#define dlsym __wrap_dlsym
#define dlclose __wrap_dlclose
#endif

#ifdef NS_TRACE_MALLOC
extern "C" {
NS_EXPORT_(__ptr_t) __libc_malloc(size_t);
NS_EXPORT_(__ptr_t) __libc_calloc(size_t, size_t);
NS_EXPORT_(__ptr_t) __libc_realloc(__ptr_t, size_t);
NS_EXPORT_(void)    __libc_free(__ptr_t);
NS_EXPORT_(__ptr_t) __libc_memalign(size_t, size_t);
NS_EXPORT_(__ptr_t) __libc_valloc(size_t);
}

static __ptr_t (*_malloc)(size_t) = __libc_malloc;
static __ptr_t (*_calloc)(size_t, size_t) = __libc_calloc;
static __ptr_t (*_realloc)(__ptr_t, size_t) = __libc_realloc;
static void (*_free)(__ptr_t) = __libc_free;
static __ptr_t (*_memalign)(size_t, size_t) = __libc_memalign;
static __ptr_t (*_valloc)(size_t) = __libc_valloc;

NS_EXPORT_(__ptr_t) malloc(size_t size)
{
    return _malloc(size);
}

NS_EXPORT_(__ptr_t) calloc(size_t nmemb, size_t size)
{
    return _calloc(nmemb, size);
}

NS_EXPORT_(__ptr_t) realloc(__ptr_t ptr, size_t size)
{
    return _realloc(ptr, size);
}

NS_EXPORT_(void) free(__ptr_t ptr)
{
    _free(ptr);
}

NS_EXPORT_(void) cfree(__ptr_t ptr)
{
    _free(ptr);
}

NS_EXPORT_(__ptr_t) memalign(size_t boundary, size_t size)
{
    return _memalign(boundary, size);
}

NS_EXPORT_(int)
posix_memalign(void **memptr, size_t alignment, size_t size)
{
    __ptr_t ptr = _memalign(alignment, size);
    if (!ptr)
        return ENOMEM;
    *memptr = ptr;
    return 0;
}

NS_EXPORT_(__ptr_t) valloc(size_t size)
{
    return _valloc(size);
}
#endif /* NS_TRACE_MALLOC */

struct DependentLib
{
    void         *libHandle;
    DependentLib *next;
};

static DependentLib *sTop;
static void* sXULLibHandle;

static void
AppendDependentLib(void *libHandle)
{
    DependentLib *d = new DependentLib;
    if (!d)
        return;

    d->next = sTop;
    d->libHandle = libHandle;

    sTop = d;
}

static void
ReadDependentCB(const char *aDependentLib, bool do_preload)
{
#if defined(LINUX) && !defined(ANDROID)
    if (do_preload)
        mozilla::ReadAheadLib(aDependentLib);
#endif
    void *libHandle = dlopen(aDependentLib, RTLD_GLOBAL | RTLD_LAZY);
    if (!libHandle)
        return;

    AppendDependentLib(libHandle);
}

nsresult
XPCOMGlueLoad(const char *xpcomFile, GetFrozenFunctionsFunc *func)
{
    char xpcomDir[MAXPATHLEN];
    if (realpath(xpcomFile, xpcomDir)) {
        char *lastSlash = strrchr(xpcomDir, '/');
        if (lastSlash) {
            *lastSlash = '\0';

            XPCOMGlueLoadDependentLibs(xpcomDir, ReadDependentCB);

#ifdef __GLIBC__
            // XUL_DLL is already loaded by XPCOMGlueLoadDependentLibs, so
            // dlopening without the full path will return that one.
            sXULLibHandle = dlopen(XUL_DLL, RTLD_GLOBAL | RTLD_LAZY);
#else
            snprintf(lastSlash, MAXPATHLEN - strlen(xpcomDir), "/" XUL_DLL);

            sXULLibHandle = dlopen(xpcomDir, RTLD_GLOBAL | RTLD_LAZY);
#endif

#ifdef NS_TRACE_MALLOC
            _malloc = (__ptr_t(*)(size_t)) dlsym(sXULLibHandle, "malloc");
            _calloc = (__ptr_t(*)(size_t, size_t)) dlsym(sXULLibHandle, "calloc");
            _realloc = (__ptr_t(*)(__ptr_t, size_t)) dlsym(sXULLibHandle, "realloc");
            _free = (void(*)(__ptr_t)) dlsym(sXULLibHandle, "free");
            _memalign = (__ptr_t(*)(size_t, size_t)) dlsym(sXULLibHandle, "memalign");
            _valloc = (__ptr_t(*)(size_t)) dlsym(sXULLibHandle, "valloc");
#endif
        }
    }

    // RTLD_DEFAULT is not defined in non-GNU toolchains, and it is
    // (void*) 0 in any case.

    void *libHandle = nullptr;

    if (xpcomFile[0] != '.' || xpcomFile[1] != '\0') {
        libHandle = dlopen(xpcomFile, RTLD_GLOBAL | RTLD_LAZY);
        if (libHandle) {
            AppendDependentLib(libHandle);
        } else {
            fprintf(stderr, "XPCOMGlueLoad error for file %s:\n%s\n", xpcomFile,
                    dlerror());
        }
    }

    GetFrozenFunctionsFunc sym =
        (GetFrozenFunctionsFunc) dlsym(libHandle,
                                       LEADING_UNDERSCORE "NS_GetFrozenFunctions");

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
        dlclose(sTop->libHandle);

        DependentLib *temp = sTop;
        sTop = sTop->next;

        delete temp;
    }

    if (sXULLibHandle) {
#ifdef NS_TRACE_MALLOC
        _malloc = __libc_malloc;
        _calloc = __libc_calloc;
        _realloc = __libc_realloc;
        _free = __libc_free;
        _memalign = __libc_memalign;
        _valloc = __libc_valloc;
#endif
        dlclose(sXULLibHandle);
        sXULLibHandle = nullptr;
    }
}

nsresult
XPCOMGlueLoadXULFunctions(const nsDynamicFunctionLoad *symbols)
{
    // We don't null-check sXULLibHandle because this might work even
    // if it is null (same as RTLD_DEFAULT)

    nsresult rv = NS_OK;
    while (symbols->functionName) {
        char buffer[512];
        snprintf(buffer, sizeof(buffer),
                 LEADING_UNDERSCORE "%s", symbols->functionName);

        *symbols->function = (NSFuncPtr) dlsym(sXULLibHandle, buffer);
        if (!*symbols->function)
            rv = NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;

        ++symbols;
    }
    return rv;
}
