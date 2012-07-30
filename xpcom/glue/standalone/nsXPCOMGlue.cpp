/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXPCOMGlue.h"
#include "nsGlueLinking.h"

#include "nspr.h"
#include "nsDebug.h"
#include "nsIServiceManager.h"
#include "nsXPCOMPrivate.h"
#include "nsCOMPtr.h"
#include <stdlib.h>
#include <stdio.h>

#ifdef XP_WIN
#include <windows.h>
#include <mbstring.h>
#include <malloc.h>
#define snprintf _snprintf
#endif

static XPCOMFunctions xpcomFunctions;
static bool do_preload = false;

extern "C"
void XPCOMGlueEnablePreload()
{
    do_preload = true;
}

extern "C"
nsresult XPCOMGlueStartup(const char* xpcomFile)
{
    xpcomFunctions.version = XPCOM_GLUE_VERSION;
    xpcomFunctions.size    = sizeof(XPCOMFunctions);

    GetFrozenFunctionsFunc func = nullptr;

    if (!xpcomFile)
        xpcomFile = XPCOM_DLL;
    
    nsresult rv = XPCOMGlueLoad(xpcomFile, &func);
    if (NS_FAILED(rv))
        return rv;

    rv = (*func)(&xpcomFunctions, nullptr);
    if (NS_FAILED(rv)) {
        XPCOMGlueUnload();
        return rv;
    }

    return NS_OK;
}

#if defined(XP_WIN)
#define READ_TEXTMODE L"rt"
#elif defined(XP_OS2)
#define READ_TEXTMODE "rt"
#else
#define READ_TEXTMODE "r"
#endif

#ifdef XP_WIN
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

void
XPCOMGlueLoadDependentLibs(const char *xpcomDir, DependentLibsCallback cb)
{
    char buffer[MAXPATHLEN];
    sprintf(buffer, "%s" XPCOM_FILE_PATH_SEPARATOR XPCOM_DEPENDENT_LIBS_LIST,
            xpcomDir);

    FILE *flist = TS_tfopen(buffer, READ_TEXTMODE);
    if (!flist)
        return;

    while (fgets(buffer, sizeof(buffer), flist)) {
        int l = strlen(buffer);

        // ignore empty lines and comments
        if (l == 0 || *buffer == '#')
            continue;

        // cut the trailing newline, if present
        if (buffer[l - 1] == '\n')
            buffer[l - 1] = '\0';

        char buffer2[MAXPATHLEN];
        snprintf(buffer2, sizeof(buffer2),
                 "%s" XPCOM_FILE_PATH_SEPARATOR "%s",
                 xpcomDir, buffer);
        cb(buffer2, do_preload);
    }

    fclose(flist);
}

extern "C"
nsresult XPCOMGlueShutdown()
{
    XPCOMGlueUnload();
    
    memset(&xpcomFunctions, 0, sizeof(xpcomFunctions));
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
                        PRUint32           aDataLength,
                        PRUint32           aFlags)
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

XPCOM_API(PRUint32)
NS_StringGetData(const nsAString &aStr, const PRUnichar **aBuf, bool *aTerm)
{
    if (!xpcomFunctions.stringGetData) {
        *aBuf = nullptr;
        return 0;
    }
    return xpcomFunctions.stringGetData(aStr, aBuf, aTerm);
}

XPCOM_API(PRUint32)
NS_StringGetMutableData(nsAString &aStr, PRUint32 aLen, PRUnichar **aBuf)
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
NS_StringSetData(nsAString &aStr, const PRUnichar *aBuf, PRUint32 aCount)
{
    if (!xpcomFunctions.stringSetData)
        return NS_ERROR_NOT_INITIALIZED;

    return xpcomFunctions.stringSetData(aStr, aBuf, aCount);
}

XPCOM_API(nsresult)
NS_StringSetDataRange(nsAString &aStr, PRUint32 aCutStart, PRUint32 aCutLength,
                      const PRUnichar *aBuf, PRUint32 aCount)
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
                         PRUint32           aDataLength,
                         PRUint32           aFlags)
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

XPCOM_API(PRUint32)
NS_CStringGetData(const nsACString &aStr, const char **aBuf, bool *aTerm)
{
    if (!xpcomFunctions.cstringGetData) {
        *aBuf = nullptr;
        return 0;
    }
    return xpcomFunctions.cstringGetData(aStr, aBuf, aTerm);
}

XPCOM_API(PRUint32)
NS_CStringGetMutableData(nsACString &aStr, PRUint32 aLen, char **aBuf)
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
NS_CStringSetData(nsACString &aStr, const char *aBuf, PRUint32 aCount)
{
    if (!xpcomFunctions.cstringSetData)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.cstringSetData(aStr, aBuf, aCount);
}

XPCOM_API(nsresult)
NS_CStringSetDataRange(nsACString &aStr, PRUint32 aCutStart, PRUint32 aCutLength,
                       const char *aBuf, PRUint32 aCount)
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
NS_Alloc(PRSize size)
{
    if (!xpcomFunctions.allocFunc)
        return nullptr;
    return xpcomFunctions.allocFunc(size);
}

XPCOM_API(void*)
NS_Realloc(void* ptr, PRSize size)
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
NS_DebugBreak(PRUint32 aSeverity, const char *aStr, const char *aExpr,
              const char *aFile, PRInt32 aLine)
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
             const char *aTypeName, PRUint32 aInstanceSize)
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
NS_LogCtor(void *aPtr, const char *aTypeName, PRUint32 aInstanceSize)
{
    if (xpcomFunctions.logCtorFunc)
        xpcomFunctions.logCtorFunc(aPtr, aTypeName, aInstanceSize);
}

XPCOM_API(void)
NS_LogDtor(void *aPtr, const char *aTypeName, PRUint32 aInstanceSize)
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
NS_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                 PRUint32 paramCount, nsXPTCVariant* params)
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
NS_CycleCollectorSuspect2(nsISupports* obj)
{
    if (!xpcomFunctions.cycleSuspect2Func)
        return nullptr;

    return xpcomFunctions.cycleSuspect2Func(obj);
}

XPCOM_API(bool)
NS_CycleCollectorForget2(nsPurpleBufferEntry* e)
{
    if (!xpcomFunctions.cycleForget2Func)
        return false;

    return xpcomFunctions.cycleForget2Func(e);
}
