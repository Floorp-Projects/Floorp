/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"
#include "nsXPCOMStrings.h"
#include "xptcall.h"

#include <string.h>

/**
 * Private Method to register an exit routine.  This method
 * used to allow you to setup a callback that will be called from 
 * the NS_ShutdownXPCOM function after all services and 
 * components have gone away. It was fatally flawed in that the component
 * DLL could be released before the exit function was called; it is now a
 * stub implementation that does nothing.
 */
XPCOM_API(nsresult)
NS_RegisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine, PRUint32 priority);

XPCOM_API(nsresult)
NS_UnregisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine);

static const XPCOMFunctions kFrozenFunctions = {
    XPCOM_GLUE_VERSION,
    sizeof(XPCOMFunctions),
    &NS_InitXPCOM2_P,
    &NS_ShutdownXPCOM_P,
    &NS_GetServiceManager_P,
    &NS_GetComponentManager_P,
    &NS_GetComponentRegistrar_P,
    &NS_GetMemoryManager_P,
    &NS_NewLocalFile_P,
    &NS_NewNativeLocalFile_P,
    &NS_RegisterXPCOMExitRoutine,
    &NS_UnregisterXPCOMExitRoutine,

    // these functions were added post 1.4
    &NS_GetDebug_P,
    &NS_GetTraceRefcnt_P,

    // these functions were added post 1.6
    &NS_StringContainerInit_P,
    &NS_StringContainerFinish_P,
    &NS_StringGetData_P,
    &NS_StringSetData_P,
    &NS_StringSetDataRange_P,
    &NS_StringCopy_P,
    &NS_CStringContainerInit_P,
    &NS_CStringContainerFinish_P,
    &NS_CStringGetData_P,
    &NS_CStringSetData_P,
    &NS_CStringSetDataRange_P,
    &NS_CStringCopy_P,
    &NS_CStringToUTF16_P,
    &NS_UTF16ToCString_P,
    &NS_StringCloneData_P,
    &NS_CStringCloneData_P,

    // these functions were added post 1.7 (post Firefox 1.0)
    &NS_Alloc_P,
    &NS_Realloc_P,
    &NS_Free_P,
    &NS_StringContainerInit2_P,
    &NS_CStringContainerInit2_P,
    &NS_StringGetMutableData_P,
    &NS_CStringGetMutableData_P,
    NULL,

    // these functions were added post 1.8
    &NS_DebugBreak_P,
    &NS_LogInit_P,
    &NS_LogTerm_P,
    &NS_LogAddRef_P,
    &NS_LogRelease_P,
    &NS_LogCtor_P,
    &NS_LogDtor_P,
    &NS_LogCOMPtrAddRef_P,
    &NS_LogCOMPtrRelease_P,
    &NS_GetXPTCallStub_P,
    &NS_DestroyXPTCallStub_P,
    &NS_InvokeByIndex_P,
    &NS_CycleCollectorSuspect_P,
    &NS_CycleCollectorForget_P,
    &NS_StringSetIsVoid_P,
    &NS_StringGetIsVoid_P,
    &NS_CStringSetIsVoid_P,
    &NS_CStringGetIsVoid_P,

    // these functions were added post 1.9
    &NS_CycleCollectorSuspect2_P,
    &NS_CycleCollectorForget2_P
};

EXPORT_XPCOM_API(nsresult)
NS_GetFrozenFunctions(XPCOMFunctions *functions, const char* /* libraryPath */)
{
    if (!functions)
        return NS_ERROR_OUT_OF_MEMORY;

    if (functions->version != XPCOM_GLUE_VERSION)
        return NS_ERROR_FAILURE;

    PRUint32 size = functions->size;
    if (size > sizeof(XPCOMFunctions))
        size = sizeof(XPCOMFunctions);

    size -= offsetof(XPCOMFunctions, init);

    memcpy(&functions->init, &kFrozenFunctions.init, size);

    return NS_OK;
}

/*
 * Stubs for nsXPCOM.h
 */

#undef NS_InitXPCOM2
EXPORT_XPCOM_API(nsresult)
NS_InitXPCOM2(nsIServiceManager **result,
              nsIFile *binDirectory,
              nsIDirectoryServiceProvider *dirProvider)
{
  return NS_InitXPCOM2_P(result, binDirectory, dirProvider);
}

#undef NS_ShutdownXPCOM
EXPORT_XPCOM_API(nsresult)
NS_ShutdownXPCOM(nsIServiceManager *svcMgr)
{
  return NS_ShutdownXPCOM_P(svcMgr);
}

#undef NS_GetServiceManager
EXPORT_XPCOM_API(nsresult)
NS_GetServiceManager(nsIServiceManager* *result)
{
  return NS_GetServiceManager_P(result);
}

#undef NS_GetComponentManager
EXPORT_XPCOM_API(nsresult)
NS_GetComponentManager(nsIComponentManager* *result)
{
  return NS_GetComponentManager_P(result);
}

#undef NS_GetComponentRegistrar
EXPORT_XPCOM_API(nsresult)
NS_GetComponentRegistrar(nsIComponentRegistrar** result)
{
  return NS_GetComponentRegistrar_P(result);
}

#undef NS_GetMemoryManager
EXPORT_XPCOM_API(nsresult)
NS_GetMemoryManager(nsIMemory* *result)
{
  return NS_GetMemoryManager_P(result);
}

#undef NS_NewLocalFile
EXPORT_XPCOM_API(nsresult)
NS_NewLocalFile(const nsAString &path,
                bool followLinks,
                nsIFile **result)
{
  return NS_NewLocalFile_P(path, followLinks, result);
}

#undef NS_NewNativeLocalFile
EXPORT_XPCOM_API(nsresult)
NS_NewNativeLocalFile(const nsACString &path,
                      bool followLinks,
                      nsIFile **result)
{
  return NS_NewNativeLocalFile_P(path, followLinks, result);
}

#undef NS_GetDebug
EXPORT_XPCOM_API(nsresult)
NS_GetDebug(nsIDebug **result)
{
  return NS_GetDebug_P(result);
}

#undef NS_GetTraceRefcnt
EXPORT_XPCOM_API(nsresult)
NS_GetTraceRefcnt(nsITraceRefcnt **result)
{
  return NS_GetTraceRefcnt_P(result);
}

#undef NS_Alloc
EXPORT_XPCOM_API(void*)
NS_Alloc(PRSize size)
{
  return NS_Alloc_P(size);
}

#undef NS_Realloc
EXPORT_XPCOM_API(void*)
NS_Realloc(void* ptr, PRSize size)
{
  return NS_Realloc_P(ptr, size);
}

#undef NS_Free
EXPORT_XPCOM_API(void)
NS_Free(void* ptr)
{
  NS_Free_P(ptr);
}

#undef NS_DebugBreak
EXPORT_XPCOM_API(void)
NS_DebugBreak(PRUint32 aSeverity, const char *aStr, const char *aExpr,
              const char *aFile, PRInt32 aLine)
{
  NS_DebugBreak_P(aSeverity, aStr, aExpr, aFile, aLine);
}

#undef NS_LogInit
EXPORT_XPCOM_API(void)
NS_LogInit()
{
  NS_LogInit_P();
}

#undef NS_LogTerm
EXPORT_XPCOM_API(void)
NS_LogTerm()
{
  NS_LogTerm_P();
}

#undef NS_LogAddRef
EXPORT_XPCOM_API(void)
NS_LogAddRef(void* aPtr, nsrefcnt aNewRefCnt,
             const char *aTypeName, PRUint32 aInstanceSize)
{
  NS_LogAddRef_P(aPtr, aNewRefCnt, aTypeName, aInstanceSize);
}

#undef NS_LogRelease
EXPORT_XPCOM_API(void)
NS_LogRelease(void* aPtr, nsrefcnt aNewRefCnt, const char *aTypeName)
{
  NS_LogRelease_P(aPtr, aNewRefCnt, aTypeName);
}

#undef NS_LogCtor
EXPORT_XPCOM_API(void)
NS_LogCtor(void *aPtr, const char *aTypeName, PRUint32 aInstanceSize)
{
  NS_LogCtor_P(aPtr, aTypeName, aInstanceSize);
}

#undef NS_LogDtor
EXPORT_XPCOM_API(void)
NS_LogDtor(void *aPtr, const char *aTypeName, PRUint32 aInstanceSize)
{
  NS_LogDtor_P(aPtr, aTypeName, aInstanceSize);
}

#undef NS_LogCOMPtrAddRef
EXPORT_XPCOM_API(void)
NS_LogCOMPtrAddRef(void *aCOMPtr, nsISupports* aObject)
{
  NS_LogCOMPtrAddRef_P(aCOMPtr, aObject);
}

#undef NS_LogCOMPtrRelease
EXPORT_XPCOM_API(void)
NS_LogCOMPtrRelease(void *aCOMPtr, nsISupports* aObject)
{
  NS_LogCOMPtrRelease_P(aCOMPtr, aObject);
}

#undef NS_GetXPTCallStub
EXPORT_XPCOM_API(nsresult)
NS_GetXPTCallStub(REFNSIID aIID, nsIXPTCProxy* aOuter,
                  nsISomeInterface* *aStub)
{
  return NS_GetXPTCallStub_P(aIID, aOuter, aStub);
}

#undef NS_DestroyXPTCallStub
EXPORT_XPCOM_API(void)
NS_DestroyXPTCallStub(nsISomeInterface* aStub)
{
  NS_DestroyXPTCallStub_P(aStub);
}

#undef NS_InvokeByIndex
EXPORT_XPCOM_API(nsresult)
NS_InvokeByIndex(nsISupports* that, PRUint32 methodIndex,
                 PRUint32 paramCount, nsXPTCVariant* params)
{
  return NS_InvokeByIndex_P(that, methodIndex, paramCount, params);
}

/*
 * Stubs for nsXPCOMPrivate.h
 */

EXPORT_XPCOM_API(nsresult)
NS_RegisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine, PRUint32 priority)
{
  return NS_OK;
}

EXPORT_XPCOM_API(nsresult)
NS_UnregisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine)
{
  return NS_OK;
}

/*
 * Stubs for nsStringAPI.h
 */

#undef NS_StringContainerInit
EXPORT_XPCOM_API(nsresult)
NS_StringContainerInit(nsStringContainer &aStr)
{
  return NS_StringContainerInit_P(aStr);
}

#undef NS_StringContainerInit2
EXPORT_XPCOM_API(nsresult)
NS_StringContainerInit2(nsStringContainer &aStr,
                        const PRUnichar   *aData,
                        PRUint32           aDataLength,
                        PRUint32           aFlags)
{   
  return NS_StringContainerInit2_P(aStr, aData, aDataLength, aFlags);
}

#undef NS_StringContainerFinish
EXPORT_XPCOM_API(void)
NS_StringContainerFinish(nsStringContainer &aStr)
{
  NS_StringContainerFinish_P(aStr);
}

#undef NS_StringGetData
EXPORT_XPCOM_API(PRUint32)
NS_StringGetData(const nsAString &aStr, const PRUnichar **aBuf, bool *aTerm)
{
  return NS_StringGetData_P(aStr, aBuf, aTerm);
}

#undef NS_StringGetMutableData
EXPORT_XPCOM_API(PRUint32)
NS_StringGetMutableData(nsAString &aStr, PRUint32 aLen, PRUnichar **aBuf)
{
  return NS_StringGetMutableData_P(aStr, aLen, aBuf);
}

#undef NS_StringCloneData
EXPORT_XPCOM_API(PRUnichar *)
NS_StringCloneData(const nsAString &aStr)
{
  return NS_StringCloneData_P(aStr);
}

#undef NS_StringSetData
EXPORT_XPCOM_API(nsresult)
NS_StringSetData(nsAString &aStr, const PRUnichar *aBuf, PRUint32 aCount)
{
  return NS_StringSetData_P(aStr, aBuf, aCount);
}

#undef NS_StringSetDataRange
EXPORT_XPCOM_API(nsresult)
NS_StringSetDataRange(nsAString &aStr, PRUint32 aCutStart, PRUint32 aCutLength,
                      const PRUnichar *aBuf, PRUint32 aCount)
{
  return NS_StringSetDataRange_P(aStr, aCutStart, aCutLength, aBuf, aCount);
}

#undef NS_StringCopy
EXPORT_XPCOM_API(nsresult)
NS_StringCopy(nsAString &aDest, const nsAString &aSrc)
{
  return NS_StringCopy_P(aDest, aSrc);
}

#undef NS_StringSetIsVoid
EXPORT_XPCOM_API(void)
NS_StringSetIsVoid(nsAString &aStr, const bool aIsVoid)
{
  NS_StringSetIsVoid_P(aStr, aIsVoid);
}

#undef NS_StringGetIsVoid
EXPORT_XPCOM_API(bool)
NS_StringGetIsVoid(const nsAString &aStr)
{
  return NS_StringGetIsVoid_P(aStr);
}

#undef NS_CStringContainerInit
EXPORT_XPCOM_API(nsresult)
NS_CStringContainerInit(nsCStringContainer &aStr)
{
  return NS_CStringContainerInit_P(aStr);
}

#undef NS_CStringContainerInit2
EXPORT_XPCOM_API(nsresult)
NS_CStringContainerInit2(nsCStringContainer &aStr,
                         const char         *aData,
                         PRUint32            aDataLength,
                         PRUint32            aFlags)
{   
  return NS_CStringContainerInit2_P(aStr, aData, aDataLength, aFlags);
}

#undef NS_CStringContainerFinish
EXPORT_XPCOM_API(void)
NS_CStringContainerFinish(nsCStringContainer &aStr)
{
  NS_CStringContainerFinish_P(aStr);
}

#undef NS_CStringGetData
EXPORT_XPCOM_API(PRUint32)
NS_CStringGetData(const nsACString &aStr, const char **aBuf, bool *aTerm)
{
  return NS_CStringGetData_P(aStr, aBuf, aTerm);
}

#undef NS_CStringGetMutableData
EXPORT_XPCOM_API(PRUint32)
NS_CStringGetMutableData(nsACString &aStr, PRUint32 aLen, char **aBuf)
{
  return NS_CStringGetMutableData_P(aStr, aLen, aBuf);
}

#undef NS_CStringCloneData
EXPORT_XPCOM_API(char *)
NS_CStringCloneData(const nsACString &aStr)
{
  return NS_CStringCloneData_P(aStr);
}

#undef NS_CStringSetData
EXPORT_XPCOM_API(nsresult)
NS_CStringSetData(nsACString &aStr, const char *aBuf, PRUint32 aCount)
{
  return NS_CStringSetData_P(aStr, aBuf, aCount);
}

#undef NS_CStringSetDataRange
EXPORT_XPCOM_API(nsresult)
NS_CStringSetDataRange(nsACString &aStr, PRUint32 aCutStart, PRUint32 aCutLength,
                       const char *aBuf, PRUint32 aCount)
{
  return NS_CStringSetDataRange_P(aStr, aCutStart, aCutLength, aBuf, aCount);
}

#undef NS_CStringCopy
EXPORT_XPCOM_API(nsresult)
NS_CStringCopy(nsACString &aDest, const nsACString &aSrc)
{
  return NS_CStringCopy_P(aDest, aSrc);
}

#undef NS_CStringSetIsVoid
EXPORT_XPCOM_API(void)
NS_CStringSetIsVoid(nsACString &aStr, const bool aIsVoid)
{
  NS_CStringSetIsVoid_P(aStr, aIsVoid);
}

#undef NS_CStringGetIsVoid
EXPORT_XPCOM_API(bool)
NS_CStringGetIsVoid(const nsACString &aStr)
{
  return NS_CStringGetIsVoid_P(aStr);
}

#undef NS_CStringToUTF16
EXPORT_XPCOM_API(nsresult)
NS_CStringToUTF16(const nsACString &aSrc, nsCStringEncoding aSrcEncoding, nsAString &aDest)
{
  return NS_CStringToUTF16_P(aSrc, aSrcEncoding, aDest);
}

#undef NS_UTF16ToCString
EXPORT_XPCOM_API(nsresult)
NS_UTF16ToCString(const nsAString &aSrc, nsCStringEncoding aDestEncoding, nsACString &aDest)
{
  return NS_UTF16ToCString_P(aSrc, aDestEncoding, aDest);
}

#undef NS_CycleCollectorSuspect
EXPORT_XPCOM_API(bool)
NS_CycleCollectorSuspect(nsISupports* obj)
{
  return NS_CycleCollectorSuspect_P(obj);
}

#undef NS_CycleCollectorForget
EXPORT_XPCOM_API(bool)
NS_CycleCollectorForget(nsISupports* obj)
{
  return NS_CycleCollectorForget_P(obj);
}

#undef NS_CycleCollectorSuspect2
EXPORT_XPCOM_API(nsPurpleBufferEntry*)
NS_CycleCollectorSuspect2(nsISupports* obj)
{
  return NS_CycleCollectorSuspect2_P(obj);
}

#undef NS_CycleCollectorForget2
EXPORT_XPCOM_API(bool)
NS_CycleCollectorForget2(nsPurpleBufferEntry* e)
{
  return NS_CycleCollectorForget2_P(e);
}
