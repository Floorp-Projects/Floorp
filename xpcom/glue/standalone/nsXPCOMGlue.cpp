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

#ifdef DEBUG_dougt
#define XPCOM_GLUE_FLUSH_HEAP
#endif

// seawood tells me there isn't a better way...
#ifdef XP_PC
#define XPCOM_DLL  "xpcom32.dll"
#else
#ifdef XP_MAC
#define XPCOM_DLL "XPCOM_DLL"
#else
#define XPCOM_DLL  "libxpcom.so"
#endif
#endif

extern "C"
nsresult NS_COM XPCOMGlueStartup(const char* xpcomFile)
{
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

    rv = (*function)(xpcomFunctions);
    if (NS_FAILED(rv)) {
        free(xpcomFunctions);
        xpcomFunctions = nsnull;  
        PR_UnloadLibrary(xpcomLib);
        xpcomLib = nsnull;
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}
#ifdef XPCOM_GLUE_FLUSH_HEAP
static void FlushHeap()
{
#if defined(XP_WIN32)
    // Heap compaction and shrink working set now 
#ifdef DEBUG_dougt
    PRIntervalTime start = PR_IntervalNow();
    int ret = 
#endif
        _heapmin();
#ifdef DEBUG_dougt
    printf("DEBUG: HeapCompact() %s - %d ms\n", (!ret ? "success" : "FAILED"),
           PR_IntervalToMilliseconds(PR_IntervalNow()-start));
#endif
    
    // shrink working set if we can
    // This function call is available only on winnt and above.
    typedef BOOL WINAPI SetProcessWorkingSetProc(HANDLE hProcess, SIZE_T dwMinimumWorkingSetSize,
                                                 SIZE_T dwMaximumWorkingSetSize);
    SetProcessWorkingSetProc *setProcessWorkingSetSizeP = NULL;
    
    HMODULE kernel = GetModuleHandle("kernel32.dll");
    if (kernel) {
        setProcessWorkingSetSizeP = (SetProcessWorkingSetProc *)
            GetProcAddress(kernel, "SetProcessWorkingSetSize");
    }
    
    if (setProcessWorkingSetSizeP) {
        // shrink working set
#ifdef DEBUG_dougt
        start = PR_IntervalNow();
#endif
        (*setProcessWorkingSetSizeP)(GetCurrentProcess(), -1, -1);
#ifdef DEBUG_dougt
        printf("DEBUG: Honey! I shrunk the resident-set! - %d ms\n",
               PR_IntervalToMilliseconds(PR_IntervalNow() - start));
#endif
    }
#endif 
}
#endif 

extern "C"
nsresult NS_COM XPCOMGlueShutdown()
{
    if (xpcomFunctions) {
        free ((void*)xpcomFunctions);
        xpcomFunctions = nsnull;
    }

    if (xpcomLib) {
        PR_UnloadLibrary(xpcomLib);
        xpcomLib = nsnull;
    }
#ifdef XPCOM_GLUE_FLUSH_HEAP
    // should the application do this instead of us?
    FlushHeap();
#endif 

    return NS_OK;
}

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

#if DEBUG_dougt

struct nsTraceRefcntStats {
  nsrefcnt mAddRefs;
  nsrefcnt mReleases;
  nsrefcnt mCreates;
  nsrefcnt mDestroys;
  double mRefsOutstandingTotal;
  double mRefsOutstandingSquared;
  double mObjsOutstandingTotal;
  double mObjsOutstandingSquared;
};

// Function type used by GatherStatistics. For each type that data has
// been gathered for, this function is called with the counts of the
// various operations that have been logged. The function can return
// PR_FALSE if the gathering should stop.
//
// aCurrentStats is the current value of the counters. aPrevStats is
// the previous value of the counters which is established by the
// nsTraceRefcnt::SnapshotStatistics call.
typedef PRBool (PR_CALLBACK *nsTraceRefcntStatFunc)
  (const char* aTypeName,
   PRUint32 aInstanceSize,
   nsTraceRefcntStats* aCurrentStats,
   nsTraceRefcntStats* aPrevStats,
   void *aClosure);

class nsTraceRefcnt {
public:
  static NS_EXPORT void Startup(){};

  static NS_EXPORT void Shutdown(){};

  static NS_EXPORT void LogAddRef(void* aPtr,
                               nsrefcnt aNewRefCnt,
                               const char* aTypeName,
                               PRUint32 aInstanceSize){};

  static NS_EXPORT void LogRelease(void* aPtr,
                                nsrefcnt aNewRefCnt,
                                const char* aTypeName){};

  static NS_EXPORT void LogNewXPCOM(void* aPtr,
                                 const char* aTypeName,
                                 PRUint32 aInstanceSize,
                                 const char* aFile,
                                 int aLine){};

  static NS_EXPORT void LogDeleteXPCOM(void* aPtr,
                                    const char* aFile,
                                    int aLine){};

  static NS_EXPORT nsrefcnt LogAddRefCall(void* aPtr,
                                       nsrefcnt aNewRefcnt,
                                       const char* aFile,
                                       int aLine){return 0;};

  static NS_EXPORT nsrefcnt LogReleaseCall(void* aPtr,
                                        nsrefcnt aNewRefcnt,
                                        const char* aFile,
                                        int aLine){return 0;};

  static NS_EXPORT void LogCtor(void* aPtr, const char* aTypeName,
                             PRUint32 aInstanceSize){};

  static NS_EXPORT void LogDtor(void* aPtr, const char* aTypeName,
                             PRUint32 aInstanceSize){};

  static NS_EXPORT void LogAddCOMPtr(void *aCOMPtr, nsISupports *aObject){};

  static NS_EXPORT void LogReleaseCOMPtr(void *aCOMPtr, nsISupports *aObject){};

  enum StatisticsType {
    ALL_STATS,
    NEW_STATS
  };

  static NS_EXPORT nsresult DumpStatistics(StatisticsType type = ALL_STATS,
                                        FILE* out = 0){return NS_ERROR_NOT_IMPLEMENTED;};
  
  static NS_EXPORT void ResetStatistics(void){};

  static NS_EXPORT void GatherStatistics(nsTraceRefcntStatFunc aFunc,
                                      void* aClosure){};

  static NS_EXPORT void LoadLibrarySymbols(const char* aLibraryName,
                                        void* aLibrayHandle){};

  static NS_EXPORT void DemangleSymbol(const char * aSymbol, 
                                    char * aBuffer,
                                    int aBufLen){};

  static NS_EXPORT void WalkTheStack(FILE* aStream){};
  
  static NS_EXPORT void SetPrefServiceAvailability(PRBool avail){};

  /**
   * Tell nsTraceRefcnt whether refcounting, allocation, and destruction
   * activity is legal.  This is used to trigger assertions for any such
   * activity that occurs because of static constructors or destructors.
   */
  static NS_EXPORT void SetActivityIsLegal(PRBool aLegal){};
};
#endif
