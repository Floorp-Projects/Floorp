/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Methods to support the JVM Plugin API
////////////////////////////////////////////////////////////////////////////////

#include "nsJVMManager.h"
#include "npglue.h"
#include "prefapi.h"
#include "np.h"
#include "prio.h"
#include "prmem.h"
#include "prthread.h"
#include "pprthred.h"
#include "plstr.h"
#include "jni.h"
#ifdef MOCHA
#include "libmocha.h"
#endif

extern "C" void jvm_InitLCGlue(void);      // in lcglue.cpp

#include "xpgetstr.h"
extern "C" int XP_PROGRESS_STARTING_JAVA;
extern "C" int XP_PROGRESS_STARTING_JAVA_DONE;
extern "C" int XP_JAVA_NO_CLASSES;
extern "C" int XP_JAVA_GENERAL_FAILURE;
extern "C" int XP_JAVA_STARTUP_FAILED;
extern "C" int XP_JAVA_DEBUGGER_FAILED;

// FIXME -- need prototypes for these functions!!! XXX
#ifdef XP_MAC
extern "C" {
OSErr ConvertUnixPathToMacPath(const char *, char **);
void startAsyncCursors(void);
void stopAsyncCursors(void);
}
#endif // XP_MAC

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIJVMManagerIID, NS_IJVMMANAGER_IID);
static NS_DEFINE_IID(kIJVMPluginIID, NS_IJVMPLUGIN_IID);
static NS_DEFINE_IID(kISymantecDebugManagerIID, NS_ISYMANTECDEBUGMANAGER_IID);
static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_AGGREGATED(nsJVMManager);

NS_METHOD
nsJVMManager::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;   // XXX right error?
    nsJVMManager* jvmmgr = new nsJVMManager(outer);
    if (jvmmgr == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    jvmmgr->AddRef();
    *aInstancePtr = outer ? (void*)jvmmgr->GetInner() : (void*)jvmmgr;
    return NS_OK;
}

nsJVMManager::nsJVMManager(nsISupports* outer)
    : fJVM(NULL), fStatus(nsJVMStatus_Enabled),
      fRegisteredJavaPrefChanged(PR_FALSE), fDebugManager(NULL), fJSJavaVM(NULL),
      fClassPathAdditions(new nsVector())
{
    NS_INIT_AGGREGATED(outer);
}

nsJVMManager::~nsJVMManager()
{
    int count = fClassPathAdditions->GetSize();
    for (int i = 0; i < count; i++) {
        PR_Free((*fClassPathAdditions)[i]);
    }
    delete fClassPathAdditions;
    if (fJVM) {
        /*nsrefcnt c =*/ fJVM->Release();   // Release for QueryInterface in GetJVM
        // XXX unload plugin if c == 1 ? (should this be done inside Release?)
    }
}

NS_METHOD
nsJVMManager::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (aIID.Equals(kIJVMManagerIID)) {
        *aInstancePtr = this;
        AddRef();
        return NS_OK;
    }
#ifdef XP_PC
    // Aggregates...
    if (fDebugManager == NULL) {
        nsresult rslt =
            nsSymantecDebugManager::Create(this, kISupportsIID,
                                           (void**)&fDebugManager, this);
        if (rslt != NS_OK) return rslt;
    }
    return fDebugManager->QueryInterface(aIID, aInstancePtr);
#else
    return NS_NOINTERFACE;
#endif
}

////////////////////////////////////////////////////////////////////////////////

#if 0
static const char*
ConvertToPlatformPathList(const char* cp)
{
    const char* c = strdup(cp);
    if (c == NULL)
        return NULL;
#ifdef XP_MAC
    {
        const char* path;
        const char* strtok_path;

        char* result = (char*) malloc(1);
        if (result == NULL)
            return NULL;
        result[0] = '\0';
        path = c;
        strtok_path = path;
#ifndef NSPR20
        while ((path = XP_STRTOK(strtok_path, PATH_SEPARATOR_STR)))
#else
            while ((path = XP_STRTOK(strtok_path, PR_PATH_SEPARATOR_STR)))
#endif
            {
                const char* macPath;
                OSErr err = ConvertUnixPathToMacPath(path, &macPath);
                char* r = PR_smprintf("%s\r%s", result, macPath);
			
                strtok_path = NULL;

                if (r == NULL)
                    return result;	/* return what we could come up with */
                free(result);
                result = r;
            }
        free(c);
        return result;
    }
#else
    return c;
#endif
}
#endif

void
nsJVMManager::ReportJVMError(nsresult err)
{
    MWContext* cx = XP_FindSomeContext();
    char *s = NULL;
    switch (err) {
      case NS_JVM_ERROR_NO_CLASSES: {
          s = PR_smprintf(XP_GetString(XP_JAVA_NO_CLASSES));
          break;
      }

      case NS_JVM_ERROR_JAVA_ERROR: {
          nsIJVMPlugin* plugin = GetJVMPlugin();
          PR_ASSERT(plugin != NULL);
          if (plugin == NULL) break;
          JNIEnv* env;
          plugin->GetJNIEnv(&env);
          const char* msg = GetJavaErrorString(env);
          plugin->ReleaseJNIEnv(env);
#ifdef DEBUG
          env->ExceptionDescribe();
#endif
          s = PR_smprintf(XP_GetString(XP_JAVA_STARTUP_FAILED), 
                          (msg ? msg : ""));
          if (msg) free((void*)msg);
          break;
      }

      case NS_JVM_ERROR_NO_DEBUGGER: {
          s = PR_smprintf(XP_GetString(XP_JAVA_DEBUGGER_FAILED));
          break;
      }

      default: {
          s = PR_smprintf(XP_GetString(XP_JAVA_GENERAL_FAILURE), err);
          break;
      }
    }
    if (s) {
        FE_Alert(cx, s);
        free(s);
    }
}

/* stolen from java_lang_Object.h (jri version) */
#define classname_java_lang_Object	"java/lang/Object"
#define name_java_lang_Object_toString	"toString"
#define sig_java_lang_Object_toString 	"()Ljava/lang/String;"

const char*
nsJVMManager::GetJavaErrorString(JNIEnv* env)
{
    jthrowable exc = env->ExceptionOccurred();
    if (exc == NULL) {
        return strdup("");	/* XXX better "no error" message? */
    }

    /* Got to do this before trying to find a class (like Object). 
       This is because the runtime refuses to do this with a pending 
       exception! I think it's broken. */

    env->ExceptionClear();

    jclass classObject = env->FindClass(classname_java_lang_Object);
    jmethodID toString = env->GetMethodID(classObject, name_java_lang_Object_toString, sig_java_lang_Object_toString);
    jstring excString = (jstring) env->CallObjectMethod(exc, toString);

    jboolean isCopy;
    const char* msg = env->GetStringUTFChars(excString, &isCopy);
    if (msg != NULL) {
        const char* dupmsg = (msg == NULL ? NULL : strdup(msg));
    	env->ReleaseStringUTFChars(excString, msg);
    	msg = dupmsg;
    }
    return msg;
}

////////////////////////////////////////////////////////////////////////////////

PRLogModuleInfo* NSJAVA = NULL;

nsJVMStatus
nsJVMManager::StartupJVM(void)
{
    // Be sure to check the prefs first before asking java to startup.
    switch (GetJVMStatus()) {
      case nsJVMStatus_Disabled:
        return nsJVMStatus_Disabled;
      case nsJVMStatus_Running:
        return nsJVMStatus_Running;
      default:
        break;
    }

#ifdef DEBUG
    PRIntervalTime start = PR_IntervalNow();
    if (NSJAVA == NULL)
        NSJAVA = PR_NewLogModule("NSJAVA");
    PR_LOG(NSJAVA, PR_LOG_ALWAYS, ("Starting java..."));
#endif

    MWContext* someRandomContext = XP_FindSomeContext();
    if (someRandomContext) {
        FE_Progress(someRandomContext, XP_GetString(XP_PROGRESS_STARTING_JAVA));
    }

    PR_ASSERT(fJVM == NULL);
    nsIPlugin* plugin = NPL_LoadPluginByType(NPJVM_MIME_TYPE);
    if (plugin == NULL) {
        fStatus = nsJVMStatus_Failed;
        return fStatus;
    }

    nsresult rslt = plugin->QueryInterface(kIJVMPluginIID, (void**)&fJVM);
    if (rslt != NS_OK) {
        PR_ASSERT(fJVM == NULL);
        fStatus = nsJVMStatus_Failed;
        return fStatus;
    }
    
    nsJVMInitArgs initargs;
    int count = fClassPathAdditions->GetSize();
    char* classpathAdditions = NULL;
    for (int i = 0; i < count; i++) {
        const char* path = (const char*)(*fClassPathAdditions)[i];
        char* oldPath = classpathAdditions;
        if (oldPath) {
            classpathAdditions = PR_smprintf("%s%c%s", oldPath, PR_PATH_SEPARATOR, path);
            PR_Free(oldPath);
        }
        else
            classpathAdditions = PL_strdup(path);
    }
    initargs.version = nsJVMInitArgs_Version;
    initargs.classpathAdditions = classpathAdditions;

    nsresult err = fJVM->StartupJVM(&initargs);
    if (err == NS_OK) {
        /* assume the JVM is running. */
        fStatus = nsJVMStatus_Running;
    }
    else {
        ReportJVMError(err);
        fStatus = nsJVMStatus_Failed;
    }

#if 0
    JSContext* crippledContext = LM_GetCrippledContext();
    MaybeStartupLiveConnect(crippledContext, JS_GetGlobalObject(crippledContext));
#endif

    fJVM->Release();

#ifdef DEBUG
    PRIntervalTime end = PR_IntervalNow();
    PRInt32 d = PR_IntervalToMilliseconds(end - start);
    PR_LOG(NSJAVA, PR_LOG_ALWAYS,
           ("Starting java...%s (%ld ms)",
            (fStatus == nsJVMStatus_Running ? "done" : "failed"), d));
#endif

    if (someRandomContext) {
        FE_Progress(someRandomContext, XP_GetString(XP_PROGRESS_STARTING_JAVA_DONE));
    }
    return fStatus;
}

nsJVMStatus
nsJVMManager::ShutdownJVM(PRBool fullShutdown)
{
    if (fStatus == nsJVMStatus_Running) {
        PR_ASSERT(fJVM != NULL);
        (void)MaybeShutdownLiveConnect();
        nsresult err = fJVM->ShutdownJVM(fullShutdown);
        if (err == NS_OK)
            fStatus = nsJVMStatus_Enabled;
        else {
            ReportJVMError(err);
            fStatus = nsJVMStatus_Disabled;
        }
        fJVM = NULL;
    }
    PR_ASSERT(fJVM == NULL);
    return fStatus;
}

////////////////////////////////////////////////////////////////////////////////

static int PR_CALLBACK
JavaPrefChanged(const char *prefStr, void* data)
{
    nsJVMManager* mgr = (nsJVMManager*)data;
    XP_Bool prefBool;
#if defined(XP_MAC)
    // beard: under Mozilla, no way to enable this right now.
    prefBool = TRUE;
#else
    PREF_GetBoolPref(prefStr, &prefBool);
#endif
    mgr->SetJVMEnabled(prefBool);
    return 0;
}

void
nsJVMManager::SetJVMEnabled(PRBool enabled)
{
    if (enabled) {
        if (fStatus != nsJVMStatus_Running) 
            fStatus = nsJVMStatus_Enabled;
        // don't start the JVM here, do it lazily
    }
    else {
        if (fStatus == nsJVMStatus_Running) 
            (void)ShutdownJVM();
        fStatus = nsJVMStatus_Disabled;
    }
}

void
nsJVMManager::EnsurePrefCallbackRegistered(void)
{
    if (fRegisteredJavaPrefChanged != PR_TRUE) {
        fRegisteredJavaPrefChanged = PR_TRUE;
        PREF_RegisterCallback("security.enable_java", JavaPrefChanged, this);
        JavaPrefChanged("security.enable_java", this);
    }
}

nsJVMStatus
nsJVMManager::GetJVMStatus(void)
{
    EnsurePrefCallbackRegistered();
    return fStatus;
}

#ifdef XP_MAC
#define JSJDLL "LiveConnect"
#endif

PRBool
nsJVMManager::MaybeStartupLiveConnect(void)
{
    if (fJSJavaVM)
        return PR_TRUE;

    do {
        static PRBool registeredLiveConnectFactory = PR_FALSE;
        if (!registeredLiveConnectFactory) {
            NS_DEFINE_CID(kCLiveconnectCID, NS_CLIVECONNECT_CID);
            registeredLiveConnectFactory = 
                (nsRepository::RegisterFactory(kCLiveconnectCID, (const char *)JSJDLL,
                                               PR_FALSE, PR_FALSE) == NS_OK);
        }
        if (IsLiveConnectEnabled() && StartupJVM() == nsJVMStatus_Running) {
            jvm_InitLCGlue();
            nsIJVMPlugin* plugin = GetJVMPlugin();
            if (plugin) {
#if 0
                const char* classpath = NULL;
                nsresult err = plugin->GetClassPath(&classpath);
                if (err != NS_OK) break;

                JavaVM* javaVM = NULL;
                err = plugin->GetJavaVM(&javaVM);
                if (err != NS_OK) break;
#endif
                fJSJavaVM = JSJ_ConnectToJavaVM(NULL, NULL);
                if (fJSJavaVM != NULL)
                    return PR_TRUE;
                // plugin->Release(); // GetJVMPlugin no longer calls AddRef
            }
        }
    } while (0);
    return PR_FALSE;
}

PRBool
nsJVMManager::MaybeShutdownLiveConnect(void)
{
    if (fJSJavaVM) {
        JSJ_DisconnectFromJavaVM(fJSJavaVM);
        fJSJavaVM = NULL;
        return PR_TRUE; 
    }
    return PR_FALSE;
}

PRBool
nsJVMManager::IsLiveConnectEnabled(void)
{
    if (LM_GetMochaEnabled()) {
        nsJVMStatus status = GetJVMStatus();
        return (status == nsJVMStatus_Enabled || status == nsJVMStatus_Running);
    }
    return PR_FALSE;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsJVMManager::AddToClassPath(const char* dirPath)
{
    nsIJVMPlugin* jvm = GetJVMPlugin();

    /* Add any zip or jar files in this directory to the classpath: */
    PRDir* dir = PR_OpenDir(dirPath);
    if (dir != NULL) {
        PRDirEntry* dirent;
        while ((dirent = PR_ReadDir(dir, PR_SKIP_BOTH)) != NULL) {
            PRFileInfo info;
            char* path = PR_smprintf("%s%c%s", dirPath, PR_DIRECTORY_SEPARATOR, PR_DirName(dirent));
            if (path != NULL) {
                PRBool freePath = PR_TRUE;
                if ((PR_GetFileInfo(path, &info) == PR_SUCCESS)
                    && (info.type == PR_FILE_FILE)) {
                    int len = PL_strlen(path);

                    /* Is it a zip or jar file? */
                    if ((len > 4) && 
                        ((PL_strcasecmp(path+len-4, ".zip") == 0) || 
                         (PL_strcasecmp(path+len-4, ".jar") == 0))) {
                        fClassPathAdditions->Add((void*)path);
                        if (jvm) {
                            /* Add this path to the classpath: */
                            jvm->AddToClassPath(path);
                        }
                        freePath = PR_FALSE;
                    }
                }
	            
                // Don't leak the path!
                if (freePath)
                    PR_smprintf_free(path);
            }
        }
        PR_CloseDir(dir);
    }

    /* Also add the directory to the classpath: */
    fClassPathAdditions->Add((void*)dirPath);
    if (jvm) {
        jvm->AddToClassPath(dirPath);
        // jvm->Release(); // GetJVMPlugin no longer calls AddRef
    }
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

class nsJVMFactory : public nsIFactory {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              REFNSIID aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

    nsJVMFactory(void);
    virtual ~nsJVMFactory(void);
};

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsJVMFactory, kIFactoryIID);

nsJVMFactory::nsJVMFactory(void)
{
}

nsJVMFactory::~nsJVMFactory(void)
{
}

NS_METHOD
nsJVMFactory::CreateInstance(nsISupports *aOuter,
                             REFNSIID aIID,
                             void **aResult)
{
    return nsJVMManager::Create(aOuter, aIID, aResult);
}

NS_METHOD
nsJVMFactory::LockFactory(PRBool aLock)
{
    // nothing to do here since we're not a DLL (yet)
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Bogus stuff that will go away when
// a) we have a persistent registry for CLSID -> Factory mappings, and
// b) when we ship a browser where all this JVM management code is in a DLL.

static NS_DEFINE_CID(kJVMManagerCID, NS_JVMMANAGER_CID);

extern "C" void
jvm_RegisterJVMMgr(void)
{
    nsRepository::RegisterFactory(kJVMManagerCID,
                                  new nsJVMFactory(),
                                  PR_TRUE);
}

////////////////////////////////////////////////////////////////////////////////

