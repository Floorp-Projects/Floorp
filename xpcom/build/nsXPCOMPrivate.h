/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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

#ifndef nsXPComPrivate_h__
#define nsXPComPrivate_h__

#include "nscore.h"
#include "nsXPCOM.h"
#include "nsXPCOMStrings.h"
#include "xptcall.h"

class nsStringContainer;
class nsCStringContainer;
class nsIComponentLoader;

/**
 * During this shutdown notification all threads which run XPCOM code must
 * be joined.
 */
#define NS_XPCOM_SHUTDOWN_THREADS_OBSERVER_ID "xpcom-shutdown-threads"

/**
 * During this shutdown notification all module loaders must unload XPCOM
 * modules.
 */
#define NS_XPCOM_SHUTDOWN_LOADERS_OBSERVER_ID "xpcom-shutdown-loaders"

// PUBLIC
typedef nsresult   (* InitFunc)(nsIServiceManager* *result, nsIFile* binDirectory, nsIDirectoryServiceProvider* appFileLocationProvider);
typedef nsresult   (* ShutdownFunc)(nsIServiceManager* servMgr);
typedef nsresult   (* GetServiceManagerFunc)(nsIServiceManager* *result);
typedef nsresult   (* GetComponentManagerFunc)(nsIComponentManager* *result);
typedef nsresult   (* GetComponentRegistrarFunc)(nsIComponentRegistrar* *result);
typedef nsresult   (* GetMemoryManagerFunc)(nsIMemory* *result);
typedef nsresult   (* NewLocalFileFunc)(const nsAString &path, bool followLinks, nsILocalFile* *result);
typedef nsresult   (* NewNativeLocalFileFunc)(const nsACString &path, bool followLinks, nsILocalFile* *result);

typedef nsresult   (* GetDebugFunc)(nsIDebug* *result);
typedef nsresult   (* GetTraceRefcntFunc)(nsITraceRefcnt* *result);

typedef nsresult   (* StringContainerInitFunc)(nsStringContainer&);
typedef nsresult   (* StringContainerInit2Func)(nsStringContainer&, const PRUnichar *, PRUint32, PRUint32);
typedef void       (* StringContainerFinishFunc)(nsStringContainer&);
typedef PRUint32   (* StringGetDataFunc)(const nsAString&, const PRUnichar**, bool*);
typedef PRUint32   (* StringGetMutableDataFunc)(nsAString&, PRUint32, PRUnichar**);
typedef PRUnichar* (* StringCloneDataFunc)(const nsAString&);
typedef nsresult   (* StringSetDataFunc)(nsAString&, const PRUnichar*, PRUint32);
typedef nsresult   (* StringSetDataRangeFunc)(nsAString&, PRUint32, PRUint32, const PRUnichar*, PRUint32);
typedef nsresult   (* StringCopyFunc)(nsAString &, const nsAString &);
typedef void       (* StringSetIsVoidFunc)(nsAString &, const bool);
typedef bool       (* StringGetIsVoidFunc)(const nsAString &);

typedef nsresult   (* CStringContainerInitFunc)(nsCStringContainer&);
typedef nsresult   (* CStringContainerInit2Func)(nsCStringContainer&, const char *, PRUint32, PRUint32);
typedef void       (* CStringContainerFinishFunc)(nsCStringContainer&);
typedef PRUint32   (* CStringGetDataFunc)(const nsACString&, const char**, bool*);
typedef PRUint32   (* CStringGetMutableDataFunc)(nsACString&, PRUint32, char**);
typedef char*      (* CStringCloneDataFunc)(const nsACString&);
typedef nsresult   (* CStringSetDataFunc)(nsACString&, const char*, PRUint32);
typedef nsresult   (* CStringSetDataRangeFunc)(nsACString&, PRUint32, PRUint32, const char*, PRUint32);
typedef nsresult   (* CStringCopyFunc)(nsACString &, const nsACString &);
typedef void       (* CStringSetIsVoidFunc)(nsACString &, const bool);
typedef bool       (* CStringGetIsVoidFunc)(const nsACString &);

typedef nsresult   (* CStringToUTF16)(const nsACString &, nsCStringEncoding, nsAString &);
typedef nsresult   (* UTF16ToCString)(const nsAString &, nsCStringEncoding, nsACString &);

typedef void*      (* AllocFunc)(PRSize size);
typedef void*      (* ReallocFunc)(void* ptr, PRSize size);
typedef void       (* FreeFunc)(void* ptr);

typedef void       (* DebugBreakFunc)(PRUint32 aSeverity,
                                      const char *aStr, const char *aExpr,
                                      const char *aFile, PRInt32 aLine);

typedef void       (* xpcomVoidFunc)();
typedef void       (* LogAddRefFunc)(void*, nsrefcnt, const char*, PRUint32);
typedef void       (* LogReleaseFunc)(void*, nsrefcnt, const char*);
typedef void       (* LogCtorFunc)(void*, const char*, PRUint32);
typedef void       (* LogCOMPtrFunc)(void*, nsISupports*);

typedef nsresult   (* GetXPTCallStubFunc)(REFNSIID, nsIXPTCProxy*, nsISomeInterface**);
typedef void       (* DestroyXPTCallStubFunc)(nsISomeInterface*);
typedef nsresult   (* InvokeByIndexFunc)(nsISupports*, PRUint32, PRUint32, nsXPTCVariant*);
typedef bool       (* CycleCollectorFunc)(nsISupports*);
typedef nsPurpleBufferEntry*
                   (* CycleCollectorSuspect2Func)(nsISupports*);
typedef bool       (* CycleCollectorForget2Func)(nsPurpleBufferEntry*);

// PRIVATE AND DEPRECATED
typedef NS_CALLBACK(XPCOMExitRoutine)(void);

typedef nsresult   (* RegisterXPCOMExitRoutineFunc)(XPCOMExitRoutine exitRoutine, PRUint32 priority);
typedef nsresult   (* UnregisterXPCOMExitRoutineFunc)(XPCOMExitRoutine exitRoutine);

typedef struct XPCOMFunctions{
    PRUint32 version;
    PRUint32 size;

    InitFunc init;
    ShutdownFunc shutdown;
    GetServiceManagerFunc getServiceManager;
    GetComponentManagerFunc getComponentManager;
    GetComponentRegistrarFunc getComponentRegistrar;
    GetMemoryManagerFunc getMemoryManager;
    NewLocalFileFunc newLocalFile;
    NewNativeLocalFileFunc newNativeLocalFile;

    RegisterXPCOMExitRoutineFunc registerExitRoutine;
    UnregisterXPCOMExitRoutineFunc unregisterExitRoutine;

    // Added for Mozilla 1.5
    GetDebugFunc getDebug;
    GetTraceRefcntFunc getTraceRefcnt;

    // Added for Mozilla 1.7
    StringContainerInitFunc stringContainerInit;
    StringContainerFinishFunc stringContainerFinish;
    StringGetDataFunc stringGetData;
    StringSetDataFunc stringSetData;
    StringSetDataRangeFunc stringSetDataRange;
    StringCopyFunc stringCopy;
    CStringContainerInitFunc cstringContainerInit;
    CStringContainerFinishFunc cstringContainerFinish;
    CStringGetDataFunc cstringGetData;
    CStringSetDataFunc cstringSetData;
    CStringSetDataRangeFunc cstringSetDataRange;
    CStringCopyFunc cstringCopy;
    CStringToUTF16 cstringToUTF16;
    UTF16ToCString utf16ToCString;
    StringCloneDataFunc stringCloneData;
    CStringCloneDataFunc cstringCloneData;

    // Added for Mozilla 1.8
    AllocFunc allocFunc;
    ReallocFunc reallocFunc;
    FreeFunc freeFunc;
    StringContainerInit2Func stringContainerInit2;
    CStringContainerInit2Func cstringContainerInit2;
    StringGetMutableDataFunc stringGetMutableData;
    CStringGetMutableDataFunc cstringGetMutableData;
    void* init3; // obsolete

    // Added for Mozilla 1.9
    DebugBreakFunc debugBreakFunc;
    xpcomVoidFunc logInitFunc;
    xpcomVoidFunc logTermFunc;
    LogAddRefFunc logAddRefFunc;
    LogReleaseFunc logReleaseFunc;
    LogCtorFunc logCtorFunc;
    LogCtorFunc logDtorFunc;
    LogCOMPtrFunc logCOMPtrAddRefFunc;
    LogCOMPtrFunc logCOMPtrReleaseFunc;
    GetXPTCallStubFunc getXPTCallStubFunc;
    DestroyXPTCallStubFunc destroyXPTCallStubFunc;
    InvokeByIndexFunc invokeByIndexFunc;
    CycleCollectorFunc cycleSuspectFunc;
    CycleCollectorFunc cycleForgetFunc;
    StringSetIsVoidFunc stringSetIsVoid;
    StringGetIsVoidFunc stringGetIsVoid;
    CStringSetIsVoidFunc cstringSetIsVoid;
    CStringGetIsVoidFunc cstringGetIsVoid;

    // Added for Mozilla 1.9.1
    CycleCollectorSuspect2Func cycleSuspect2Func;
    CycleCollectorForget2Func cycleForget2Func;

} XPCOMFunctions;

typedef nsresult (*GetFrozenFunctionsFunc)(XPCOMFunctions *entryPoints, const char* libraryPath);
XPCOM_API(nsresult)
NS_GetFrozenFunctions(XPCOMFunctions *entryPoints, const char* libraryPath);


namespace mozilla {

/**
 * Shutdown XPCOM. You must call this method after you are finished
 * using xpcom. 
 *
 * @param servMgr           The service manager which was returned by NS_InitXPCOM.
 *                          This will release servMgr.  You may pass null.
 *
 * @return NS_OK for success;
 *         other error codes indicate a failure during shutdown
 *
 */
nsresult
ShutdownXPCOM(nsIServiceManager* servMgr);

/**
 * C++ namespaced version of NS_LogTerm.
 */
void LogTerm();

} // namespace mozilla


// think hard before changing this
#define XPCOM_GLUE_VERSION 1


/* XPCOM Specific Defines
 *
 * XPCOM_DLL              - name of the loadable xpcom library on disk. 
 * XUL_DLL                - name of the loadable XUL library on disk
 * XPCOM_SEARCH_KEY       - name of the environment variable that can be 
 *                          modified to include additional search paths.
 * GRE_CONF_NAME          - Name of the GRE Configuration file
 */

#if defined(XP_WIN32) || defined(XP_OS2)

#define XPCOM_SEARCH_KEY  "PATH"
#define GRE_CONF_NAME     "gre.config"
#define GRE_WIN_REG_LOC   L"Software\\mozilla.org\\GRE"
#define XPCOM_DLL         "xpcom.dll"
#define LXPCOM_DLL        L"xpcom.dll"
#define XUL_DLL           "xul.dll"
#define LXUL_DLL          L"xul.dll"

#else // Unix
#include <limits.h> // for PATH_MAX

#define XPCOM_DLL "libxpcom" MOZ_DLL_SUFFIX

// you have to love apple..
#ifdef XP_MACOSX  
#define XPCOM_SEARCH_KEY  "DYLD_LIBRARY_PATH"
#define GRE_FRAMEWORK_NAME "XUL.framework"
#define XUL_DLL            "XUL"
#else
#define XPCOM_SEARCH_KEY  "LD_LIBRARY_PATH"
#define XUL_DLL   "libxul" MOZ_DLL_SUFFIX
#endif

#define GRE_CONF_NAME ".gre.config"
#define GRE_CONF_PATH "/etc/gre.conf"
#define GRE_CONF_DIR  "/etc/gre.d"
#define GRE_USER_CONF_DIR ".gre.d"
#endif

#if defined(XP_WIN) || defined(XP_OS2)
  #define XPCOM_FILE_PATH_SEPARATOR       "\\"
  #define XPCOM_ENV_PATH_SEPARATOR        ";"
#elif defined(XP_UNIX)
  #define XPCOM_FILE_PATH_SEPARATOR       "/"
  #define XPCOM_ENV_PATH_SEPARATOR        ":"
#else
  #error need_to_define_your_file_path_separator_and_illegal_characters
#endif

#ifdef AIX
#include <sys/param.h>
#endif

#ifndef MAXPATHLEN
#ifdef PATH_MAX
#define MAXPATHLEN PATH_MAX
#elif defined(_MAX_PATH)
#define MAXPATHLEN _MAX_PATH
#elif defined(CCHMAXPATH)
#define MAXPATHLEN CCHMAXPATH
#else
#define MAXPATHLEN 1024
#endif
#endif

extern bool gXPCOMShuttingDown;

namespace mozilla {
namespace services {

/** 
 * Clears service cache, sets gXPCOMShuttingDown
 */
void Shutdown();

} // namespace services
} // namespace mozilla

#endif
