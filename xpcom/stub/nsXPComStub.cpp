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
 * The Original Code is Mozilla.
 *
 * The Initial Developer of the Original Code is IBM Corporation.
 * Portions created by IBM Corporation are Copyright (C) 2004
 * IBM Corporation. All Rights Reserved.
 *
 * Contributor(s):
 *   Darin Fisher <darin@meer.net>
 *   Benjamin Smedberg <bsmedberg@covad.net>
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

#include "nsXPCOM.h"
#include "nsXPCOMPrivate.h"
#include "nsStringAPI.h"

#include <string.h>

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
    &NS_RegisterXPCOMExitRoutine_P,
    &NS_UnregisterXPCOMExitRoutine_P,

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
    &NS_CStringContainerInit2_P
};  

extern "C" NS_EXPORT nsresult
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
extern "C" NS_EXPORT nsresult
NS_InitXPCOM2(nsIServiceManager **result,
              nsIFile *binDirectory,
              nsIDirectoryServiceProvider *dirProvider)
{
  return NS_InitXPCOM2_P(result, binDirectory, dirProvider);
}

#undef NS_ShutdownXPCOM
extern "C" NS_EXPORT nsresult
NS_ShutdownXPCOM(nsIServiceManager *svcMgr)
{
  return NS_ShutdownXPCOM_P(svcMgr);
}

#undef NS_GetServiceManager
extern "C" NS_EXPORT nsresult
NS_GetServiceManager(nsIServiceManager* *result)
{
  return NS_GetServiceManager_P(result);
}

#undef NS_GetComponentManager
extern "C" NS_EXPORT nsresult
NS_GetComponentManager(nsIComponentManager* *result)
{
  return NS_GetComponentManager_P(result);
}

#undef NS_GetComponentRegistrar
extern "C" NS_EXPORT nsresult
NS_GetComponentRegistrar(nsIComponentRegistrar* *result)
{
  return NS_GetComponentRegistrar_P(result);
}

#undef NS_GetMemoryManager
extern "C" NS_EXPORT nsresult
NS_GetMemoryManager(nsIMemory* *result)
{
  return NS_GetMemoryManager_P(result);
}

#undef NS_NewLocalFile
extern "C" NS_EXPORT nsresult
NS_NewLocalFile(const nsAString &path,
                PRBool followLinks,
                nsILocalFile **result)
{
  return NS_NewLocalFile_P(path, followLinks, result);
}

#undef NS_NewNativeLocalFile
extern "C" NS_EXPORT nsresult
NS_NewNativeLocalFile(const nsACString &path,
                      PRBool followLinks,
                      nsILocalFile **result)
{
  return NS_NewNativeLocalFile_P(path, followLinks, result);
}

#undef NS_GetDebug
extern "C" NS_EXPORT nsresult
NS_GetDebug(nsIDebug **result)
{
  return NS_GetDebug_P(result);
}

#undef NS_GetTraceRefcnt
extern "C" NS_EXPORT nsresult
NS_GetTraceRefcnt(nsITraceRefcnt **result)
{
  return NS_GetTraceRefcnt_P(result);
}

#undef NS_Alloc
extern "C" NS_EXPORT void*
NS_Alloc(PRSize size)
{
  return NS_Alloc_P(size);
}

#undef NS_Realloc
extern "C" NS_EXPORT void*
NS_Realloc(void* ptr, PRSize size)
{
  return NS_Realloc_P(ptr, size);
}

#undef NS_Free
extern "C" NS_EXPORT void
NS_Free(void* ptr)
{
  NS_Free_P(ptr);
}

/*
 * Stubs for nsXPCOMPrivate.h
 */

#undef NS_RegisterXPCOMExitRoutine
extern "C" NS_EXPORT nsresult
NS_RegisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine, PRUint32 priority)
{
  return NS_RegisterXPCOMExitRoutine_P(exitRoutine, priority);
}

#undef NS_UnregisterXPCOMExitRoutine
extern "C" NS_EXPORT nsresult
NS_UnregisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine)
{
  return NS_UnregisterXPCOMExitRoutine_P(exitRoutine);
}

/*
 * Stubs for nsStringAPI.h
 */

#undef NS_StringContainerInit
extern "C" NS_EXPORT nsresult
NS_StringContainerInit(nsStringContainer &aStr)
{
  return NS_StringContainerInit_P(aStr);
}

#undef NS_StringContainerInit2
extern "C" NS_EXPORT nsresult
NS_StringContainerInit2(nsStringContainer &aStr,
                        const PRUnichar   *aData,
                        PRUint32           aDataLength,
                        PRUint32           aFlags)
{   
  return NS_StringContainerInit2_P(aStr, aData, aDataLength, aFlags);
}

#undef NS_StringContainerFinish
extern "C" NS_EXPORT void
NS_StringContainerFinish(nsStringContainer &aStr)
{
  NS_StringContainerFinish_P(aStr);
}

#undef NS_StringGetData
extern "C" NS_EXPORT PRUint32
NS_StringGetData(const nsAString &aStr, const PRUnichar **aBuf, PRBool *aTerm)
{
  return NS_StringGetData_P(aStr, aBuf, aTerm);
}

#undef NS_StringCloneData
extern "C" NS_EXPORT PRUnichar *
NS_StringCloneData(const nsAString &aStr)
{
  return NS_StringCloneData_P(aStr);
}

#undef NS_StringSetData
extern "C" NS_EXPORT nsresult
NS_StringSetData(nsAString &aStr, const PRUnichar *aBuf, PRUint32 aCount)
{
  return NS_StringSetData_P(aStr, aBuf, aCount);
}

#undef NS_StringSetDataRange
extern "C" NS_EXPORT nsresult
NS_StringSetDataRange(nsAString &aStr, PRUint32 aCutStart, PRUint32 aCutLength,
                      const PRUnichar *aBuf, PRUint32 aCount)
{
  return NS_StringSetDataRange_P(aStr, aCutStart, aCutLength, aBuf, aCount);
}

#undef NS_StringCopy
extern "C" NS_EXPORT nsresult
NS_StringCopy(nsAString &aDest, const nsAString &aSrc)
{
  return NS_StringCopy_P(aDest, aSrc);
}

#undef NS_CStringContainerInit
extern "C" NS_EXPORT nsresult
NS_CStringContainerInit(nsCStringContainer &aStr)
{
  return NS_CStringContainerInit_P(aStr);
}

#undef NS_CStringContainerInit2
extern "C" NS_EXPORT nsresult
NS_CStringContainerInit2(nsCStringContainer &aStr,
                         const char         *aData,
                         PRUint32            aDataLength,
                         PRUint32            aFlags)
{   
  return NS_CStringContainerInit2_P(aStr, aData, aDataLength, aFlags);
}

#undef NS_CStringContainerFinish
extern "C" NS_EXPORT void
NS_CStringContainerFinish(nsCStringContainer &aStr)
{
  NS_CStringContainerFinish_P(aStr);
}

#undef NS_CStringGetData
extern "C" NS_EXPORT PRUint32
NS_CStringGetData(const nsACString &aStr, const char **aBuf, PRBool *aTerm)
{
  return NS_CStringGetData_P(aStr, aBuf, aTerm);
}

#undef NS_CStringCloneData
extern "C" NS_EXPORT char *
NS_CStringCloneData(const nsACString &aStr)
{
  return NS_CStringCloneData_P(aStr);
}

#undef NS_CStringSetData
extern "C" NS_EXPORT nsresult
NS_CStringSetData(nsACString &aStr, const char *aBuf, PRUint32 aCount)
{
  return NS_CStringSetData_P(aStr, aBuf, aCount);
}

#undef NS_CStringSetDataRange
extern "C" NS_EXPORT nsresult
NS_CStringSetDataRange(nsACString &aStr, PRUint32 aCutStart, PRUint32 aCutLength,
                       const char *aBuf, PRUint32 aCount)
{
  return NS_CStringSetDataRange_P(aStr, aCutStart, aCutLength, aBuf, aCount);
}

#undef NS_CStringCopy
extern "C" NS_EXPORT nsresult
NS_CStringCopy(nsACString &aDest, const nsACString &aSrc)
{
  return NS_CStringCopy_P(aDest, aSrc);
}

#undef NS_CStringToUTF16
extern "C" NS_EXPORT nsresult
NS_CStringToUTF16(const nsACString &aSrc, nsCStringEncoding aSrcEncoding, nsAString &aDest)
{
  return NS_CStringToUTF16_P(aSrc, aSrcEncoding, aDest);
}

#undef NS_UTF16ToCString
extern "C" NS_EXPORT nsresult
NS_UTF16ToCString(const nsAString &aSrc, nsCStringEncoding aDestEncoding, nsACString &aDest)
{
  return NS_UTF16ToCString_P(aSrc, aDestEncoding, aDest);
}
