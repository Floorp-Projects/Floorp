/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sw=4 et cindent: */
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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXPCOMGlue.h"

#include "nspr.h"
#include "nsMemory.h"
#include "nsGREDirServiceProvider.h"
#include "nsXPCOMPrivate.h"
#include <stdlib.h>

#if XP_WIN32
#include <windows.h>
#endif

void GRE_AddGREToEnvironment();

// functions provided by nsMemory.cpp and nsDebug.cpp
nsresult GlueStartupMemory();
void GlueShutdownMemory();
nsresult GlueStartupDebug();
void GlueShutdownDebug();

static PRLibrary *xpcomLib;
static XPCOMFunctions xpcomFunctions;

extern "C"
nsresult XPCOMGlueStartup(const char* xpcomFile)
{
#ifdef XPCOM_GLUE_NO_DYNAMIC_LOADING
    return NS_OK;
#else
    nsresult rv;
    GetFrozenFunctionsFunc function = nsnull;

    xpcomFunctions.version = XPCOM_GLUE_VERSION;
    xpcomFunctions.size    = sizeof(XPCOMFunctions);

    //
    // if xpcomFile == ".", then we assume xpcom is already loaded, and we'll
    // use NSPR to find NS_GetFrozenFunctions from the list of already loaded
    // libraries.
    //
    // otherwise, we try to load xpcom and then look for NS_GetFrozenFunctions.
    // if xpcomFile == NULL, then we try to load xpcom by name w/o a fully
    // qualified path.
    //

    if (xpcomFile && (xpcomFile[0] == '.' && xpcomFile[1] == '\0')) {
        function = (GetFrozenFunctionsFunc)
                PR_FindSymbolAndLibrary("NS_GetFrozenFunctions", &xpcomLib);
        if (!function) {
            // The symbol was not found, so failover to loading XPCOM_DLL,
            // and look for the symbol there.  See bug 240986 for details.
            xpcomFile = nsnull;
        }
        else {
            char *libPath = PR_GetLibraryFilePathname(XPCOM_DLL, (PRFuncPtr) function);
            if (!libPath)
                rv = NS_ERROR_FAILURE;
            else {
                rv = (*function)(&xpcomFunctions, libPath);
                PR_Free(libPath);
            }
        }
    }

    if (!function) {
        PRLibSpec libSpec;

        libSpec.type = PR_LibSpec_Pathname;
        if (!xpcomFile)
            libSpec.value.pathname = XPCOM_DLL;
        else
            libSpec.value.pathname = xpcomFile;

        xpcomLib = PR_LoadLibraryWithFlags(libSpec, PR_LD_LAZY|PR_LD_GLOBAL);
        if (!xpcomLib)
            return NS_ERROR_FAILURE;

        function = (GetFrozenFunctionsFunc) PR_FindSymbol(xpcomLib, "NS_GetFrozenFunctions");

        if (!function)
            rv = NS_ERROR_FAILURE;
        else
            rv = (*function)(&xpcomFunctions, libSpec.value.pathname);
    }

    if (NS_FAILED(rv))
        goto bail;

    rv = GlueStartupDebug();
    if (NS_FAILED(rv))
        goto bail;

    // startup the nsMemory
    rv = GlueStartupMemory();
    if (NS_FAILED(rv)) {
        GlueShutdownDebug();
        goto bail;
    }

    GRE_AddGREToEnvironment();
    return NS_OK;

bail:
    PR_UnloadLibrary(xpcomLib);
    xpcomLib = nsnull;
    memset(&xpcomFunctions, 0, sizeof(xpcomFunctions));
    return NS_ERROR_FAILURE;
#endif
}

extern "C"
nsresult XPCOMGlueShutdown()
{
#ifdef XPCOM_GLUE_NO_DYNAMIC_LOADING
    return NS_OK;
#else

    GlueShutdownMemory();

    GlueShutdownDebug();

    if (xpcomLib) {
        PR_UnloadLibrary(xpcomLib);
        xpcomLib = nsnull;
    }
    
    memset(&xpcomFunctions, 0, sizeof(xpcomFunctions));
    return NS_OK;
#endif
}

#ifndef XPCOM_GLUE_NO_DYNAMIC_LOADING
extern "C" NS_COM nsresult
NS_InitXPCOM2(nsIServiceManager* *result, 
              nsIFile* binDirectory,
              nsIDirectoryServiceProvider* appFileLocationProvider)
{
    if (!xpcomFunctions.init)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.init(result, binDirectory, appFileLocationProvider);
}

extern "C" NS_COM nsresult
NS_ShutdownXPCOM(nsIServiceManager* servMgr)
{
    if (!xpcomFunctions.shutdown)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.shutdown(servMgr);
}

extern "C" NS_COM nsresult
NS_GetServiceManager(nsIServiceManager* *result)
{
    if (!xpcomFunctions.getServiceManager)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.getServiceManager(result);
}

extern "C" NS_COM nsresult
NS_GetComponentManager(nsIComponentManager* *result)
{
    if (!xpcomFunctions.getComponentManager)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.getComponentManager(result);
}

extern "C" NS_COM nsresult
NS_GetComponentRegistrar(nsIComponentRegistrar* *result)
{
    if (!xpcomFunctions.getComponentRegistrar)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.getComponentRegistrar(result);
}

extern "C" NS_COM nsresult
NS_GetMemoryManager(nsIMemory* *result)
{
    if (!xpcomFunctions.getMemoryManager)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.getMemoryManager(result);
}

extern "C" NS_COM nsresult
NS_NewLocalFile(const nsAString &path, PRBool followLinks, nsILocalFile* *result)
{
    if (!xpcomFunctions.newLocalFile)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.newLocalFile(path, followLinks, result);
}

extern "C" NS_COM nsresult
NS_NewNativeLocalFile(const nsACString &path, PRBool followLinks, nsILocalFile* *result)
{
    if (!xpcomFunctions.newNativeLocalFile)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.newNativeLocalFile(path, followLinks, result);
}

extern "C" NS_COM nsresult
NS_RegisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine, PRUint32 priority)
{
    if (!xpcomFunctions.registerExitRoutine)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.registerExitRoutine(exitRoutine, priority);
}

extern "C" NS_COM nsresult
NS_UnregisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine)
{
    if (!xpcomFunctions.unregisterExitRoutine)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.unregisterExitRoutine(exitRoutine);
}

extern "C" NS_COM nsresult
NS_GetDebug(nsIDebug* *result)
{
    if (!xpcomFunctions.getDebug)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.getDebug(result);
}


extern "C" NS_COM nsresult
NS_GetTraceRefcnt(nsITraceRefcnt* *result)
{
    if (!xpcomFunctions.getTraceRefcnt)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.getTraceRefcnt(result);
}


extern "C" NS_COM nsresult
NS_StringContainerInit(nsStringContainer &aStr)
{
    if (!xpcomFunctions.stringContainerInit)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.stringContainerInit(aStr);
}

extern "C" NS_COM void
NS_StringContainerFinish(nsStringContainer &aStr)
{
    if (xpcomFunctions.stringContainerFinish)
        xpcomFunctions.stringContainerFinish(aStr);
}

extern "C" NS_COM PRUint32
NS_StringGetData(const nsAString &aStr, const PRUnichar **aBuf, PRBool *aTerm)
{
    if (!xpcomFunctions.stringGetData) {
        *aBuf = nsnull;
        return 0;
    }
    return xpcomFunctions.stringGetData(aStr, aBuf, aTerm);
}

extern "C" NS_COM PRUnichar *
NS_StringCloneData(const nsAString &aStr)
{
    if (!xpcomFunctions.stringCloneData)
        return nsnull;
    return xpcomFunctions.stringCloneData(aStr);
}

extern "C" NS_COM nsresult
NS_StringSetData(nsAString &aStr, const PRUnichar *aBuf, PRUint32 aCount)
{
    if (!xpcomFunctions.stringSetData)
        return NS_ERROR_NOT_INITIALIZED;

    return xpcomFunctions.stringSetData(aStr, aBuf, aCount);
}

extern "C" NS_COM nsresult
NS_StringSetDataRange(nsAString &aStr, PRUint32 aCutStart, PRUint32 aCutLength,
                      const PRUnichar *aBuf, PRUint32 aCount)
{
    if (!xpcomFunctions.stringSetDataRange)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.stringSetDataRange(aStr, aCutStart, aCutLength, aBuf, aCount);
}

extern "C" NS_COM nsresult
NS_StringCopy(nsAString &aDest, const nsAString &aSrc)
{
    if (!xpcomFunctions.stringCopy)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.stringCopy(aDest, aSrc);
}


extern "C" NS_COM nsresult
NS_CStringContainerInit(nsCStringContainer &aStr)
{
    if (!xpcomFunctions.cstringContainerInit)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.cstringContainerInit(aStr);
}

extern "C" NS_COM void
NS_CStringContainerFinish(nsCStringContainer &aStr)
{
    if (xpcomFunctions.cstringContainerFinish)
        xpcomFunctions.cstringContainerFinish(aStr);
}

extern "C" NS_COM PRUint32
NS_CStringGetData(const nsACString &aStr, const char **aBuf, PRBool *aTerm)
{
    if (!xpcomFunctions.cstringGetData) {
        *aBuf = nsnull;
        return 0;
    }
    return xpcomFunctions.cstringGetData(aStr, aBuf, aTerm);
}

extern "C" NS_COM char *
NS_CStringCloneData(const nsACString &aStr)
{
    if (!xpcomFunctions.cstringCloneData)
        return nsnull;
    return xpcomFunctions.cstringCloneData(aStr);
}

extern "C" NS_COM nsresult
NS_CStringSetData(nsACString &aStr, const char *aBuf, PRUint32 aCount)
{
    if (!xpcomFunctions.cstringSetData)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.cstringSetData(aStr, aBuf, aCount);
}

extern "C" NS_COM nsresult
NS_CStringSetDataRange(nsACString &aStr, PRUint32 aCutStart, PRUint32 aCutLength,
                       const char *aBuf, PRUint32 aCount)
{
    if (!xpcomFunctions.cstringSetDataRange)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.cstringSetDataRange(aStr, aCutStart, aCutLength, aBuf, aCount);
}

extern "C" NS_COM nsresult
NS_CStringCopy(nsACString &aDest, const nsACString &aSrc)
{
    if (!xpcomFunctions.cstringCopy)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.cstringCopy(aDest, aSrc);
}

extern "C" NS_COM nsresult
NS_CStringToUTF16(const nsACString &aSrc, PRUint32 aSrcEncoding, nsAString &aDest)
{
    if (!xpcomFunctions.cstringToUTF16)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.cstringToUTF16(aSrc, aSrcEncoding, aDest);
}

extern "C" NS_COM nsresult
NS_UTF16ToCString(const nsAString &aSrc, PRUint32 aDestEncoding, nsACString &aDest)
{
    if (!xpcomFunctions.utf16ToCString)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions.utf16ToCString(aSrc, aDestEncoding, aDest);
}

#endif // #ifndef  XPCOM_GLUE_NO_DYNAMIC_LOADING


static char  sEnvString[MAXPATHLEN*10];
static char* spEnvString = 0;

void
GRE_AddGREToEnvironment()
{
  const char* grePath = GRE_GetGREPath();
  if (!grePath)
    return;

  const char* path = PR_GetEnv(XPCOM_SEARCH_KEY);
  if (!path) {
    path = "";
  }

  if (spEnvString) PR_smprintf_free(spEnvString);

  /**
   * if the PATH string is longer than our static buffer, allocate a
   * buffer for the environment string. This buffer will be leaked at shutdown!
   */
  if (strlen(grePath) + strlen(path) +
      sizeof(XPCOM_SEARCH_KEY) + sizeof(XPCOM_ENV_PATH_SEPARATOR) > MAXPATHLEN*10) {
      if (PR_smprintf(XPCOM_SEARCH_KEY "=%s" XPCOM_ENV_PATH_SEPARATOR "%s",
                      grePath,
                      path)) {
          PR_SetEnv(spEnvString);
      }
  } else {
      if (sprintf(sEnvString,
                  XPCOM_SEARCH_KEY "=%s" XPCOM_ENV_PATH_SEPARATOR "%s",
                  grePath,
                  path) > 0) {
          PR_SetEnv(sEnvString);
      }
  }
                 
#if XP_WIN32
  // On windows, the current directory is searched before the 
  // PATH environment variable.  This is a very bad thing 
  // since libraries in the cwd will be picked up before
  // any that are in either the application or GRE directory.

  if (grePath) {
    SetCurrentDirectory(grePath);
  }
#endif
}


// Default GRE startup/shutdown code

extern "C"
nsresult GRE_Startup()
{
    const char* xpcomLocation = GRE_GetXPCOMPath();

    // Startup the XPCOM Glue that links us up with XPCOM.
    nsresult rv = XPCOMGlueStartup(xpcomLocation);
    
    if (NS_FAILED(rv)) {
        NS_WARNING("gre: XPCOMGlueStartup failed");
        return rv;
    }

    nsGREDirServiceProvider *provider = new nsGREDirServiceProvider();
    if ( !provider ) {
        NS_WARNING("GRE_Startup failed");
        XPCOMGlueShutdown();
        return NS_ERROR_OUT_OF_MEMORY;
    }

    nsCOMPtr<nsIServiceManager> servMan;
    NS_ADDREF( provider );
    rv = NS_InitXPCOM2(getter_AddRefs(servMan), nsnull, provider);
    NS_RELEASE(provider);

    if ( NS_FAILED(rv) || !servMan) {
        NS_WARNING("gre: NS_InitXPCOM failed");
        XPCOMGlueShutdown();
        return rv;
    }

    return NS_OK;
}

extern "C"
nsresult GRE_Shutdown()
{
    NS_ShutdownXPCOM(nsnull);
    XPCOMGlueShutdown();
    return NS_OK;
}
