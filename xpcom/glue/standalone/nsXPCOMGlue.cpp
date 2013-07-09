/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXPCOMGlue.h"

#include "nspr.h"
#include "nsDebug.h"
#include "nsIServiceManager.h"
#include "nsXPCOMPrivate.h"
#include "nsCOMPtr.h"
#include <stdlib.h>
#include <stdio.h>

#include "mozilla/FileUtils.h"

using namespace mozilla;

#define XPCOM_DEPENDENT_LIBS_LIST "dependentlibs.list"

static XPCOMFunctions xpcomFunctions;
static bool do_preload = false;

#if defined(XP_WIN)
#define READ_TEXTMODE L"rt"
#elif defined(XP_OS2)
#define READ_TEXTMODE "rt"
#else
#define READ_TEXTMODE "r"
#endif

#if defined(SUNOS4) || defined(NEXTSTEP) || \
    defined(XP_DARWIN) || defined(XP_OS2) || \
    (defined(OPENBSD) || defined(NETBSD)) && !defined(__ELF__)
#define LEADING_UNDERSCORE "_"
#else
#define LEADING_UNDERSCORE
#endif

#if defined(XP_WIN)
#include <windows.h>
#include <mbstring.h>
#define snprintf _snprintf

typedef HINSTANCE LibHandleType;

static LibHandleType
GetLibHandle(pathstr_t aDependentLib)
{
    LibHandleType libHandle =
        LoadLibraryExW(aDependentLib, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);

    if (!libHandle) {
        DWORD err = GetLastError();
#ifdef DEBUG
        LPVOID lpMsgBuf;
        FormatMessage(
                      FORMAT_MESSAGE_ALLOCATE_BUFFER |
                      FORMAT_MESSAGE_FROM_SYSTEM |
                      FORMAT_MESSAGE_IGNORE_INSERTS,
                      nullptr,
                      err,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                      (LPTSTR) &lpMsgBuf,
                      0,
                      nullptr
                      );
        wprintf(L"Error loading %ls: %s\n", aDependentLib, lpMsgBuf);
        LocalFree(lpMsgBuf);
#endif
    }

    return libHandle;
}

static NSFuncPtr
GetSymbol(LibHandleType aLibHandle, const char *aSymbol)
{
    return (NSFuncPtr) GetProcAddress(aLibHandle, aSymbol);
}

static void
CloseLibHandle(LibHandleType aLibHandle)
{
    FreeLibrary(aLibHandle);
}

#elif defined(XP_OS2)
#define INCL_DOS
#define INCL_DOSERRORS
#include <os2.h>

typedef HMODULE LibHandleType;

static LibHandleType
GetLibHandle(pathstr_t aDependentLib)
{
    CHAR pszError[_MAX_PATH];
    ULONG ulrc = NO_ERROR;
    LibHandleType libHandle;
    ulrc = DosLoadModule(pszError, _MAX_PATH, aDependentLib, &libHandle);
    return (ulrc == NO_ERROR) ? libHandle : nullptr;
}

static NSFuncPtr
GetSymbol(LibHandleType aLibHandle, const char *aSymbol)
{
    ULONG ulrc = NO_ERROR;
    GetFrozenFunctionsFunc sym;
    ulrc = DosQueryProcAddr(aLibHandle, 0, aSymbol, (PFN*)&sym);
    return (ulrc == NO_ERROR) ? sym : nullptr;
}

static void
CloseLibHandle(LibHandleType aLibHandle)
{
    DosFreeModule(aLibHandle);
}

#elif defined(XP_MACOSX)
#include <mach-o/dyld.h>

typedef const mach_header *LibHandleType;

static LibHandleType
GetLibHandle(pathstr_t aDependentLib)
{
    LibHandleType libHandle = NSAddImage(aDependentLib,
                                         NSADDIMAGE_OPTION_RETURN_ON_ERROR |
                                         NSADDIMAGE_OPTION_MATCH_FILENAME_BY_INSTALLNAME);
    if (!libHandle) {
        NSLinkEditErrors linkEditError;
        int errorNum;
        const char *errorString;
        const char *fileName;
        NSLinkEditError(&linkEditError, &errorNum, &fileName, &errorString);
        fprintf(stderr, "XPCOMGlueLoad error %d:%d for file %s:\n%s\n",
                linkEditError, errorNum, fileName, errorString);
    }
    return libHandle;
}

static NSFuncPtr
GetSymbol(LibHandleType aLibHandle, const char *aSymbol)
{
    // Try to use |NSLookupSymbolInImage| since it is faster than searching
    // the global symbol table.  If we couldn't get a mach_header pointer
    // for the XPCOM dylib, then use |NSLookupAndBindSymbol| to search the
    // global symbol table (this shouldn't normally happen, unless the user
    // has called XPCOMGlueStartup(".") and relies on libxpcom.dylib
    // already being loaded).
    NSSymbol sym = nullptr;
    if (aLibHandle) {
        sym = NSLookupSymbolInImage(aLibHandle, aSymbol,
                                 NSLOOKUPSYMBOLINIMAGE_OPTION_BIND |
                                 NSLOOKUPSYMBOLINIMAGE_OPTION_RETURN_ON_ERROR);
    } else {
        if (NSIsSymbolNameDefined(aSymbol))
            sym = NSLookupAndBindSymbol(aSymbol);
    }
    if (!sym)
        return nullptr;

    return (NSFuncPtr) NSAddressOfSymbol(sym);
}

static void
CloseLibHandle(LibHandleType aLibHandle)
{
    // we cannot unload dylibs on OS X
}

#else
#include <dlfcn.h>

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

typedef void *LibHandleType;

static LibHandleType
GetLibHandle(pathstr_t aDependentLib)
{
    LibHandleType libHandle = dlopen(aDependentLib, RTLD_GLOBAL | RTLD_LAZY);
    if (!libHandle) {
        fprintf(stderr, "XPCOMGlueLoad error for file %s:\n%s\n", aDependentLib, dlerror());
    }
    return libHandle;
}

static NSFuncPtr
GetSymbol(LibHandleType aLibHandle, const char *aSymbol)
{
    return (NSFuncPtr) dlsym(aLibHandle, aSymbol);
}

static void
CloseLibHandle(LibHandleType aLibHandle)
{
    dlclose(aLibHandle);
}
#endif

struct DependentLib
{
    LibHandleType libHandle;
    DependentLib *next;
};

static DependentLib *sTop;

static void
AppendDependentLib(LibHandleType libHandle)
{
    DependentLib *d = new DependentLib;
    if (!d)
        return;

    d->next = sTop;
    d->libHandle = libHandle;

    sTop = d;
}

static bool
ReadDependentCB(pathstr_t aDependentLib, bool do_preload)
{
    if (do_preload) {
        ReadAheadLib(aDependentLib);
    }
    LibHandleType libHandle = GetLibHandle(aDependentLib);
    if (libHandle) {
        AppendDependentLib(libHandle);
    }

    return libHandle;
}

#ifdef XP_WIN
static bool
ReadDependentCB(const char *aDependentLib, bool do_preload)
{
    wchar_t wideDependentLib[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, aDependentLib, -1, wideDependentLib, MAX_PATH);
    return ReadDependentCB(wideDependentLib, do_preload);
}

inline FILE *TS_tfopen (const char *path, const wchar_t *mode)
{
    wchar_t wPath[MAX_PATH];
    MultiByteToWideChar(CP_UTF8, 0, path, -1, wPath, MAX_PATH);
    return _wfopen(wPath, mode);
}
#else
inline FILE *TS_tfopen (const char *path, const char *mode)
{
    return fopen(path, mode);
}
#endif

/* RAII wrapper for FILE descriptors */
struct ScopedCloseFileTraits
{
    typedef FILE *type;
    static type empty() { return nullptr; }
    static void release(type f) {
        if (f) {
            fclose(f);
        }
    }
};
typedef Scoped<ScopedCloseFileTraits> ScopedCloseFile;

static void
XPCOMGlueUnload()
{
#if !defined(XP_WIN) && !defined(XP_OS2) && !defined(XP_MACOSX) \
  && defined(NS_TRACE_MALLOC)
    if (sTop) {
        _malloc = __libc_malloc;
        _calloc = __libc_calloc;
        _realloc = __libc_realloc;
        _free = __libc_free;
        _memalign = __libc_memalign;
        _valloc = __libc_valloc;
    }
#endif

    while (sTop) {
        CloseLibHandle(sTop->libHandle);

        DependentLib *temp = sTop;
        sTop = sTop->next;

        delete temp;
    }
}

#if defined(XP_WIN) || defined(XP_OS2)
// like strpbrk but finds the *last* char, not the first
static const char*
ns_strrpbrk(const char *string, const char *strCharSet)
{
    const char *found = nullptr;
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
#endif

static GetFrozenFunctionsFunc
XPCOMGlueLoad(const char *xpcomFile)
{
    char xpcomDir[MAXPATHLEN];
#if defined(XP_WIN) || defined(XP_OS2)
    const char *lastSlash = ns_strrpbrk(xpcomFile, "/\\");
#else
    const char *lastSlash = strrchr(xpcomFile, '/');
#endif
    char *cursor;
    if (lastSlash) {
        size_t len = size_t(lastSlash - xpcomFile);

        if (len > MAXPATHLEN - sizeof(XPCOM_FILE_PATH_SEPARATOR
                                      XPCOM_DEPENDENT_LIBS_LIST)) {
            return nullptr;
        }
        memcpy(xpcomDir, xpcomFile, len);
        strcpy(xpcomDir + len, XPCOM_FILE_PATH_SEPARATOR
                               XPCOM_DEPENDENT_LIBS_LIST);
        cursor = xpcomDir + len + 1;
    } else {
        strcpy(xpcomDir, XPCOM_DEPENDENT_LIBS_LIST);
        cursor = xpcomDir;
    }

    if (getenv("MOZ_RUN_GTEST")) {
        strcat(xpcomDir, ".gtest");
    }

    ScopedCloseFile flist;
    flist = TS_tfopen(xpcomDir, READ_TEXTMODE);
    if (!flist) {
        return nullptr;
    }

    *cursor = '\0';

    char buffer[MAXPATHLEN];

    while (fgets(buffer, sizeof(buffer), flist)) {
        int l = strlen(buffer);

        // ignore empty lines and comments
        if (l == 0 || *buffer == '#') {
            continue;
        }

        // cut the trailing newline, if present
        if (buffer[l - 1] == '\n')
            buffer[l - 1] = '\0';

        if (l + size_t(cursor - xpcomDir) > MAXPATHLEN) {
            return nullptr;
        }

        strcpy(cursor, buffer);
        if (!ReadDependentCB(xpcomDir, do_preload)) {
            XPCOMGlueUnload();
            return nullptr;
        }
    }

    GetFrozenFunctionsFunc sym =
        (GetFrozenFunctionsFunc) GetSymbol(sTop->libHandle, LEADING_UNDERSCORE "NS_GetFrozenFunctions");

    if (!sym) { // No symbol found.
        XPCOMGlueUnload();
        return nullptr;
    }

#if !defined(XP_WIN) && !defined(XP_OS2) && !defined(XP_MACOSX) \
  && defined(NS_TRACE_MALLOC)
    _malloc = (__ptr_t(*)(size_t)) GetSymbol(sTop->libHandle, "malloc");
    _calloc = (__ptr_t(*)(size_t, size_t)) GetSymbol(sTop->libHandle, "calloc");
    _realloc = (__ptr_t(*)(__ptr_t, size_t)) GetSymbol(sTop->libHandle, "realloc");
    _free = (void(*)(__ptr_t)) GetSymbol(sTop->libHandle, "free");
    _memalign = (__ptr_t(*)(size_t, size_t)) GetSymbol(sTop->libHandle, "memalign");
    _valloc = (__ptr_t(*)(size_t)) GetSymbol(sTop->libHandle, "valloc");
#endif

    return sym;
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

        *symbols->function = (NSFuncPtr) GetSymbol(sTop->libHandle, buffer);
        if (!*symbols->function)
            rv = NS_ERROR_LOSS_OF_SIGNIFICANT_DATA;

        ++symbols;
    }
    return rv;
}

void XPCOMGlueEnablePreload()
{
    do_preload = true;
}

nsresult XPCOMGlueStartup(const char* xpcomFile)
{
    xpcomFunctions.version = XPCOM_GLUE_VERSION;
    xpcomFunctions.size    = sizeof(XPCOMFunctions);

    if (!xpcomFile)
        xpcomFile = XPCOM_DLL;

    GetFrozenFunctionsFunc func = XPCOMGlueLoad(xpcomFile);
    if (!func)
        return NS_ERROR_NOT_AVAILABLE;

    nsresult rv = (*func)(&xpcomFunctions, nullptr);
    if (NS_FAILED(rv)) {
        XPCOMGlueUnload();
        return rv;
    }

    return NS_OK;
}

XPCOM_API(nsresult)
NS_InitXPCOM2(nsIServiceManager* *result, 
              nsIFile* binDirectory,
              nsIDirectoryServiceProvider* appFileLocationProvider)
{
    if (!xpcomFunctions.init)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.init(result, binDirectory, appFileLocationProvider);
}

XPCOM_API(nsresult)
NS_ShutdownXPCOM(nsIServiceManager* servMgr)
{
    if (!xpcomFunctions.shutdown)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.shutdown(servMgr);
}

XPCOM_API(nsresult)
NS_GetServiceManager(nsIServiceManager* *result)
{
    if (!xpcomFunctions.getServiceManager)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.getServiceManager(result);
}

XPCOM_API(nsresult)
NS_GetComponentManager(nsIComponentManager* *result)
{
    if (!xpcomFunctions.getComponentManager)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.getComponentManager(result);
}

XPCOM_API(nsresult)
NS_GetComponentRegistrar(nsIComponentRegistrar* *result)
{
    if (!xpcomFunctions.getComponentRegistrar)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.getComponentRegistrar(result);
}

XPCOM_API(nsresult)
NS_GetMemoryManager(nsIMemory* *result)
{
    if (!xpcomFunctions.getMemoryManager)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.getMemoryManager(result);
}

XPCOM_API(nsresult)
NS_NewLocalFile(const nsAString &path, bool followLinks, nsIFile* *result)
{
    if (!xpcomFunctions.newLocalFile)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.newLocalFile(path, followLinks, result);
}

XPCOM_API(nsresult)
NS_NewNativeLocalFile(const nsACString &path, bool followLinks, nsIFile* *result)
{
    if (!xpcomFunctions.newNativeLocalFile)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.newNativeLocalFile(path, followLinks, result);
}

XPCOM_API(nsresult)
NS_GetDebug(nsIDebug* *result)
{
    if (!xpcomFunctions.getDebug)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.getDebug(result);
}


XPCOM_API(nsresult)
NS_GetTraceRefcnt(nsITraceRefcnt* *result)
{
    if (!xpcomFunctions.getTraceRefcnt)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.getTraceRefcnt(result);
}


XPCOM_API(nsresult)
NS_StringContainerInit(nsStringContainer &aStr)
{
    if (!xpcomFunctions.stringContainerInit)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.stringContainerInit(aStr);
}

XPCOM_API(nsresult)
NS_StringContainerInit2(nsStringContainer &aStr,
                        const PRUnichar   *aData,
                        uint32_t           aDataLength,
                        uint32_t           aFlags)
{
    if (!xpcomFunctions.stringContainerInit2)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.stringContainerInit2(aStr, aData, aDataLength, aFlags);
}

XPCOM_API(void)
NS_StringContainerFinish(nsStringContainer &aStr)
{
    if (xpcomFunctions.stringContainerFinish)
        xpcomFunctions.stringContainerFinish(aStr);
}

XPCOM_API(uint32_t)
NS_StringGetData(const nsAString &aStr, const PRUnichar **aBuf, bool *aTerm)
{
    if (!xpcomFunctions.stringGetData) {
        *aBuf = nullptr;
        return 0;
    }
    return xpcomFunctions.stringGetData(aStr, aBuf, aTerm);
}

XPCOM_API(uint32_t)
NS_StringGetMutableData(nsAString &aStr, uint32_t aLen, PRUnichar **aBuf)
{
    if (!xpcomFunctions.stringGetMutableData) {
        *aBuf = nullptr;
        return 0;
    }
    return xpcomFunctions.stringGetMutableData(aStr, aLen, aBuf);
}

XPCOM_API(PRUnichar*)
NS_StringCloneData(const nsAString &aStr)
{
    if (!xpcomFunctions.stringCloneData)
        return nullptr;
    return xpcomFunctions.stringCloneData(aStr);
}

XPCOM_API(nsresult)
NS_StringSetData(nsAString &aStr, const PRUnichar *aBuf, uint32_t aCount)
{
    if (!xpcomFunctions.stringSetData)
        return NS_ERROR_NOT_INITIALIZED;

    return xpcomFunctions.stringSetData(aStr, aBuf, aCount);
}

XPCOM_API(nsresult)
NS_StringSetDataRange(nsAString &aStr, uint32_t aCutStart, uint32_t aCutLength,
                      const PRUnichar *aBuf, uint32_t aCount)
{
    if (!xpcomFunctions.stringSetDataRange)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.stringSetDataRange(aStr, aCutStart, aCutLength, aBuf, aCount);
}

XPCOM_API(nsresult)
NS_StringCopy(nsAString &aDest, const nsAString &aSrc)
{
    if (!xpcomFunctions.stringCopy)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.stringCopy(aDest, aSrc);
}

XPCOM_API(void)
NS_StringSetIsVoid(nsAString &aStr, const bool aIsVoid)
{
    if (xpcomFunctions.stringSetIsVoid)
        xpcomFunctions.stringSetIsVoid(aStr, aIsVoid);
}

XPCOM_API(bool)
NS_StringGetIsVoid(const nsAString &aStr)
{
    if (!xpcomFunctions.stringGetIsVoid)
        return false;
    return xpcomFunctions.stringGetIsVoid(aStr);
}

XPCOM_API(nsresult)
NS_CStringContainerInit(nsCStringContainer &aStr)
{
    if (!xpcomFunctions.cstringContainerInit)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.cstringContainerInit(aStr);
}

XPCOM_API(nsresult)
NS_CStringContainerInit2(nsCStringContainer &aStr,
                         const char         *aData,
                         uint32_t           aDataLength,
                         uint32_t           aFlags)
{
    if (!xpcomFunctions.cstringContainerInit2)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.cstringContainerInit2(aStr, aData, aDataLength, aFlags);
}

XPCOM_API(void)
NS_CStringContainerFinish(nsCStringContainer &aStr)
{
    if (xpcomFunctions.cstringContainerFinish)
        xpcomFunctions.cstringContainerFinish(aStr);
}

XPCOM_API(uint32_t)
NS_CStringGetData(const nsACString &aStr, const char **aBuf, bool *aTerm)
{
    if (!xpcomFunctions.cstringGetData) {
        *aBuf = nullptr;
        return 0;
    }
    return xpcomFunctions.cstringGetData(aStr, aBuf, aTerm);
}

XPCOM_API(uint32_t)
NS_CStringGetMutableData(nsACString &aStr, uint32_t aLen, char **aBuf)
{
    if (!xpcomFunctions.cstringGetMutableData) {
        *aBuf = nullptr;
        return 0;
    }
    return xpcomFunctions.cstringGetMutableData(aStr, aLen, aBuf);
}

XPCOM_API(char*)
NS_CStringCloneData(const nsACString &aStr)
{
    if (!xpcomFunctions.cstringCloneData)
        return nullptr;
    return xpcomFunctions.cstringCloneData(aStr);
}

XPCOM_API(nsresult)
NS_CStringSetData(nsACString &aStr, const char *aBuf, uint32_t aCount)
{
    if (!xpcomFunctions.cstringSetData)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.cstringSetData(aStr, aBuf, aCount);
}

XPCOM_API(nsresult)
NS_CStringSetDataRange(nsACString &aStr, uint32_t aCutStart, uint32_t aCutLength,
                       const char *aBuf, uint32_t aCount)
{
    if (!xpcomFunctions.cstringSetDataRange)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.cstringSetDataRange(aStr, aCutStart, aCutLength, aBuf, aCount);
}

XPCOM_API(nsresult)
NS_CStringCopy(nsACString &aDest, const nsACString &aSrc)
{
    if (!xpcomFunctions.cstringCopy)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.cstringCopy(aDest, aSrc);
}

XPCOM_API(void)
NS_CStringSetIsVoid(nsACString &aStr, const bool aIsVoid)
{
    if (xpcomFunctions.cstringSetIsVoid)
        xpcomFunctions.cstringSetIsVoid(aStr, aIsVoid);
}

XPCOM_API(bool)
NS_CStringGetIsVoid(const nsACString &aStr)
{
    if (!xpcomFunctions.cstringGetIsVoid)
        return false;
    return xpcomFunctions.cstringGetIsVoid(aStr);
}

XPCOM_API(nsresult)
NS_CStringToUTF16(const nsACString &aSrc, nsCStringEncoding aSrcEncoding, nsAString &aDest)
{
    if (!xpcomFunctions.cstringToUTF16)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.cstringToUTF16(aSrc, aSrcEncoding, aDest);
}

XPCOM_API(nsresult)
NS_UTF16ToCString(const nsAString &aSrc, nsCStringEncoding aDestEncoding, nsACString &aDest)
{
    if (!xpcomFunctions.utf16ToCString)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.utf16ToCString(aSrc, aDestEncoding, aDest);
}

XPCOM_API(void*)
NS_Alloc(size_t size)
{
    if (!xpcomFunctions.allocFunc)
        return nullptr;
    return xpcomFunctions.allocFunc(size);
}

XPCOM_API(void*)
NS_Realloc(void* ptr, size_t size)
{
    if (!xpcomFunctions.reallocFunc)
        return nullptr;
    return xpcomFunctions.reallocFunc(ptr, size);
}

XPCOM_API(void)
NS_Free(void* ptr)
{
    if (xpcomFunctions.freeFunc)
        xpcomFunctions.freeFunc(ptr);
}

XPCOM_API(void)
NS_DebugBreak(uint32_t aSeverity, const char *aStr, const char *aExpr,
              const char *aFile, int32_t aLine)
{
    if (xpcomFunctions.debugBreakFunc)
        xpcomFunctions.debugBreakFunc(aSeverity, aStr, aExpr, aFile, aLine);
}

XPCOM_API(void)
NS_LogInit()
{
    if (xpcomFunctions.logInitFunc)
        xpcomFunctions.logInitFunc();
}

XPCOM_API(void)
NS_LogTerm()
{
    if (xpcomFunctions.logTermFunc)
        xpcomFunctions.logTermFunc();
}

XPCOM_API(void)
NS_LogAddRef(void *aPtr, nsrefcnt aNewRefCnt,
             const char *aTypeName, uint32_t aInstanceSize)
{
    if (xpcomFunctions.logAddRefFunc)
        xpcomFunctions.logAddRefFunc(aPtr, aNewRefCnt,
                                     aTypeName, aInstanceSize);
}

XPCOM_API(void)
NS_LogRelease(void *aPtr, nsrefcnt aNewRefCnt, const char *aTypeName)
{
    if (xpcomFunctions.logReleaseFunc)
        xpcomFunctions.logReleaseFunc(aPtr, aNewRefCnt, aTypeName);
}

XPCOM_API(void)
NS_LogCtor(void *aPtr, const char *aTypeName, uint32_t aInstanceSize)
{
    if (xpcomFunctions.logCtorFunc)
        xpcomFunctions.logCtorFunc(aPtr, aTypeName, aInstanceSize);
}

XPCOM_API(void)
NS_LogDtor(void *aPtr, const char *aTypeName, uint32_t aInstanceSize)
{
    if (xpcomFunctions.logDtorFunc)
        xpcomFunctions.logDtorFunc(aPtr, aTypeName, aInstanceSize);
}

XPCOM_API(void)
NS_LogCOMPtrAddRef(void *aCOMPtr, nsISupports *aObject)
{
    if (xpcomFunctions.logCOMPtrAddRefFunc)
        xpcomFunctions.logCOMPtrAddRefFunc(aCOMPtr, aObject);
}

XPCOM_API(void)
NS_LogCOMPtrRelease(void *aCOMPtr, nsISupports *aObject)
{
    if (xpcomFunctions.logCOMPtrReleaseFunc)
        xpcomFunctions.logCOMPtrReleaseFunc(aCOMPtr, aObject);
}

XPCOM_API(nsresult)
NS_GetXPTCallStub(REFNSIID aIID, nsIXPTCProxy* aOuter,
                  nsISomeInterface* *aStub)
{
    if (!xpcomFunctions.getXPTCallStubFunc)
        return NS_ERROR_NOT_INITIALIZED;

    return xpcomFunctions.getXPTCallStubFunc(aIID, aOuter, aStub);
}

XPCOM_API(void)
NS_DestroyXPTCallStub(nsISomeInterface* aStub)
{
    if (xpcomFunctions.destroyXPTCallStubFunc)
        xpcomFunctions.destroyXPTCallStubFunc(aStub);
}

XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, uint32_t methodIndex,
                 uint32_t paramCount, nsXPTCVariant* params)
{
    if (!xpcomFunctions.invokeByIndexFunc)
        return NS_ERROR_NOT_INITIALIZED;

    return xpcomFunctions.invokeByIndexFunc(that, methodIndex,
                                            paramCount, params);
}

XPCOM_API(bool)
NS_CycleCollectorSuspect(nsISupports* obj)
{
    if (!xpcomFunctions.cycleSuspectFunc)
        return false;

    return xpcomFunctions.cycleSuspectFunc(obj);
}

XPCOM_API(bool)
NS_CycleCollectorForget(nsISupports* obj)
{
    if (!xpcomFunctions.cycleForgetFunc)
        return false;

    return xpcomFunctions.cycleForgetFunc(obj);
}

XPCOM_API(nsPurpleBufferEntry*)
NS_CycleCollectorSuspect2(void* obj, nsCycleCollectionParticipant *p)
{
    if (!xpcomFunctions.cycleSuspect2Func)
        return nullptr;

    return xpcomFunctions.cycleSuspect2Func(obj, p);
}

XPCOM_API(void)
NS_CycleCollectorSuspect3(void* obj, nsCycleCollectionParticipant *p,
                          nsCycleCollectingAutoRefCnt* aRefCnt,
                          bool* aShouldDelete)
{
    if (xpcomFunctions.cycleSuspect3Func)
        xpcomFunctions.cycleSuspect3Func(obj, p, aRefCnt, aShouldDelete);
}

XPCOM_API(bool)
NS_CycleCollectorForget2(nsPurpleBufferEntry* e)
{
    if (!xpcomFunctions.cycleForget2Func)
        return false;

    return xpcomFunctions.cycleForget2Func(e);
}
