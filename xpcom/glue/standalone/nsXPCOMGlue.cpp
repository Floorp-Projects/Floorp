/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nspr.h"
#include "prlink.h"
#include "nsMemory.h"
#include "nsXPCOMPrivate.h"

#if defined(XP_WIN32)
#include <windows.h>   // for SetProcessWorkingSetSize()
#include <malloc.h>    // for _heapmin()
#endif

static PRLibrary *xpcomLib = nsnull;
static XPCOMFunctions *xpcomFunctions = nsnull;
static nsIMemory* xpcomMemory = nsnull;

//#define XPCOM_GLUE_NO_DYNAMIC_LOADING

// seawood tells me there isn't a better way...
#ifdef XP_PC
#define XPCOM_DLL  "xpcom.dll"
#else
#ifdef XP_MAC
#define XPCOM_DLL "XPCOM_DLL"
#else
#define XPCOM_DLL "libxpcom"MOZ_DLL_SUFFIX
#endif
#endif



extern nsresult GlueStartupMemory();
extern void GlueShutdownMemory();

extern "C"
nsresult NS_COM XPCOMGlueStartup(const char* xpcomFile)
{
#ifdef XPCOM_GLUE_NO_DYNAMIC_LOADING
    return NS_OK;
#else
    nsresult rv;
    const char* libFile;
    if (!xpcomFile)
        libFile = XPCOM_DLL;
    else
        libFile = xpcomFile;
           
    xpcomLib = PR_LoadLibrary(libFile);
    if (!xpcomLib)
        return NS_ERROR_FAILURE;
    
    GetFrozenFunctionsFunc function = 
        (GetFrozenFunctionsFunc)PR_FindSymbol(xpcomLib, "NS_GetFrozenFunctions");
    
    if (!function) {
        PR_UnloadLibrary(xpcomLib);
        xpcomLib = nsnull;
        return NS_ERROR_FAILURE;
    }

    xpcomFunctions = (XPCOMFunctions*) calloc(1, sizeof(XPCOMFunctions));
    if (!xpcomFunctions){
        PR_UnloadLibrary(xpcomLib);
        xpcomLib = nsnull;
        return NS_ERROR_FAILURE;
    }   

    xpcomFunctions->version = XPCOM_GLUE_VERSION;
    xpcomFunctions->size    = sizeof(XPCOMFunctions);

    rv = (*function)(xpcomFunctions, libFile);
    if (NS_FAILED(rv)) {
        free(xpcomFunctions);
        xpcomFunctions = nsnull;  
        PR_UnloadLibrary(xpcomLib);
        xpcomLib = nsnull;
        return NS_ERROR_FAILURE;
    }

    // startup the nsMemory
    return GlueStartupMemory();
#endif
}

extern "C"
nsresult NS_COM XPCOMGlueShutdown()
{
#ifdef XPCOM_GLUE_NO_DYNAMIC_LOADING
    return NS_OK;
#else
    if (xpcomFunctions) {
        free ((void*)xpcomFunctions);
        xpcomFunctions = nsnull;
    }

    if (xpcomLib) {
        PR_UnloadLibrary(xpcomLib);
        xpcomLib = nsnull;
    }
    
    GlueShutdownMemory();
    return NS_OK;
#endif
}

#ifndef XPCOM_GLUE_NO_DYNAMIC_LOADING
extern "C" NS_COM nsresult
NS_InitXPCOM2(nsIServiceManager* *result, 
              nsIFile* binDirectory,
              nsIDirectoryServiceProvider* appFileLocationProvider)
{
    if (!xpcomFunctions)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions->init(result, binDirectory, appFileLocationProvider);
}

extern "C" NS_COM nsresult
NS_ShutdownXPCOM(nsIServiceManager* servMgr)
{
    if (!xpcomFunctions)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions->shutdown(servMgr);
}

extern "C" NS_COM nsresult
NS_GetServiceManager(nsIServiceManager* *result)
{
    if (!xpcomFunctions)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions->getServiceManager(result);
}

extern "C" NS_COM nsresult
NS_GetComponentManager(nsIComponentManager* *result)
{
    if (!xpcomFunctions)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions->getComponentManager(result);
}

extern "C" NS_COM nsresult
NS_GetComponentRegistrar(nsIComponentRegistrar* *result)
{
    if (!xpcomFunctions)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions->getComponentRegistrar(result);
}

extern "C" NS_COM nsresult
NS_GetMemoryManager(nsIMemory* *result)
{
    if (!xpcomFunctions)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions->getMemoryManager(result);
}

extern "C" NS_COM nsresult
NS_NewLocalFile(const nsAString &path, PRBool followLinks, nsILocalFile* *result)
{
    if (!xpcomFunctions)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions->newLocalFile(path, followLinks, result);
}

extern "C" NS_COM nsresult
NS_NewNativeLocalFile(const nsACString &path, PRBool followLinks, nsILocalFile* *result)
{
    if (!xpcomFunctions)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions->newNativeLocalFile(path, followLinks, result);
}

extern "C" NS_COM nsresult
NS_RegisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine, PRUint32 priority)
{
  if (!xpcomFunctions)
      return NS_ERROR_NOT_INITIALIZED;
  return xpcomFunctions->registerExitRoutine(exitRoutine, priority);
}

extern "C" NS_COM nsresult
NS_UnregisterXPCOMExitRoutine(XPCOMExitRoutine exitRoutine)
{
    if (!xpcomFunctions)
        return NS_ERROR_NOT_INITIALIZED;
    return xpcomFunctions->unregisterExitRoutine(exitRoutine);
}
#endif // #ifndef  XPCOM_GLUE_NO_DYNAMIC_LOADING
