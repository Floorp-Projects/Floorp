/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * ***** END LICENSE BLOCK *****
 *
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date         Modified by     Description of modification
 * 03/27/2000   IBM Corp.       Added PR_CALLBACK for Optlink
 *                               use in OS2
 */

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Methods to support the JVM Plugin API
////////////////////////////////////////////////////////////////////////////////

#include "nscore.h"
#include "nsJVMManager.h"
#include "nspr.h"
#include "ProxyJNI.h"
#include "nsIPluginHost.h"
#include "nsIServiceManager.h"
#include "nsIEventQueueService.h"

// All these interfaces are necessary just to get the damn
// nsIWebBrowserChrome to send the "Starting Java" message to the status
// bar.
#include "nsIWindowWatcher.h"
#include "nsIDOMWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIPresContext.h"
#include "nsIPresContext.h"
#include "nsIDocShell.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowserChrome.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#include "nsIStringBundle.h"

#include "nsIPref.h"
#include "lcglue.h"

#include "nspr.h"
#include "plstr.h"
#include "nsCOMPtr.h"
#include "nsIPrincipal.h"
#include "nsIScriptSecurityManager.h"
#include "nsISignatureVerifier.h"


extern "C" int XP_PROGRESS_STARTING_JAVA;
extern "C" int XP_PROGRESS_STARTING_JAVA_DONE;
extern "C" int XP_JAVA_NO_CLASSES;
extern "C" int XP_JAVA_GENERAL_FAILURE;
extern "C" int XP_JAVA_STARTUP_FAILED;
extern "C" int XP_JAVA_DEBUGGER_FAILED;

static NS_DEFINE_CID(kStringBundleServiceCID, NS_STRINGBUNDLESERVICE_CID);

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);
static NS_DEFINE_CID(kJVMManagerCID, NS_JVMMANAGER_CID);

static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);

static NS_DEFINE_CID(kEventQueueServiceCID, NS_EVENTQUEUESERVICE_CID);

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
static NS_DEFINE_IID(kIThreadManagerIID, NS_ITHREADMANAGER_IID);
static NS_DEFINE_IID(kILiveConnectManagerIID, NS_ILIVECONNECTMANAGER_IID);
static NS_DEFINE_IID(kIJVMPluginIID, NS_IJVMPLUGIN_IID);

#define PLUGIN_REGIONAL_URL "chrome://global-region/locale/region.properties"


////////////////////////////////////////////////////////////////////////////////

NS_IMPL_AGGREGATED(nsJVMManager)

extern "C" {
static nsIJVMPlugin* GetRunningJVM(void);
static nsIJVMPlugin*
GetRunningJVM()
{
    nsIJVMPlugin* jvm = NULL;
    nsresult rv;
    nsCOMPtr<nsIJVMManager> managerService = do_GetService(kJVMManagerCID, &rv);
    if (NS_FAILED(rv)) return jvm;
    nsJVMManager* jvmMgr = (nsJVMManager *)managerService.get();
    if (jvmMgr) {
        nsJVMStatus status = jvmMgr->GetJVMStatus();
        if (status == nsJVMStatus_Enabled)
            status = jvmMgr->StartupJVM();
        if (status == nsJVMStatus_Running) {
            jvm = jvmMgr->GetJVMPlugin();
        }
    }
    return jvm;
}
}

NS_METHOD
nsJVMManager::CreateProxyJNI(nsISecureEnv* inSecureEnv, JNIEnv** outProxyEnv)
{
	JVMContext* context = GetJVMContext();
	if (context->proxyEnv != NULL) {
		*outProxyEnv = context->proxyEnv;
		return NS_OK;
	}
    nsIJVMPlugin* jvmPlugin = GetRunningJVM();
	if (jvmPlugin != NULL) {
		*outProxyEnv = context->proxyEnv = ::CreateProxyJNI(jvmPlugin, inSecureEnv);
		return NS_OK;
	}
	return NS_ERROR_FAILURE;
}

NS_METHOD
nsJVMManager::GetProxyJNI(JNIEnv** outProxyEnv)
{
        // Get proxy JNI, if not created yet, create it.
        *outProxyEnv = JVM_GetJNIEnv();
	return NS_OK;
}

NS_METHOD
nsJVMManager::GetJavaEnabled(PRBool* outEnabled)
{
	nsJVMStatus status = GetJVMStatus();
	*outEnabled = (status == nsJVMStatus_Enabled || status == nsJVMStatus_Running);
	return NS_OK;
}

NS_METHOD
nsJVMManager::ShowJavaConsole(void)
{
    nsresult rv;
    nsCOMPtr<nsIWebBrowserChrome> chrome;
    nsAutoString  msg; 
    
    if (!fStartupMessagePosted) {
        PRUnichar *messageUni;
        nsCOMPtr<nsIStringBundleService> strings(do_GetService(kStringBundleServiceCID));
        nsCOMPtr<nsIStringBundle> regionalBundle;
        
        rv = this->GetChrome(getter_AddRefs(chrome));
        if (NS_SUCCEEDED(rv) && chrome && strings) {
            rv = strings->CreateBundle(PLUGIN_REGIONAL_URL, 
                                       getter_AddRefs(regionalBundle));
            if (NS_SUCCEEDED(rv) && regionalBundle) {
                // pluginStartupMessage is something like "Starting
                // plugin for type"
                rv = regionalBundle->GetStringFromName(NS_LITERAL_STRING("pluginStartupMessage").get(), 
                                                       &messageUni);
                if (NS_SUCCEEDED(rv) && messageUni) {
                    msg = messageUni;
                    nsMemory::Free((void *)messageUni);
                    
                    msg.Append(PRUnichar(' '));
                    msg.AppendLiteral("application/x-java-vm");
                    chrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT, 
                                      msg.get());
                }
            }
        }
    } // !fStartupMessagePosted

    JVM_ShowConsole();
    // clear the startup message, if one was posted
    if (!fStartupMessagePosted && chrome) {
        msg.Truncate();
        chrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT, 
                          msg.get());
        fStartupMessagePosted = PR_TRUE;
    }
        
    return NS_OK;
}
    
// nsIThreadManager:

NS_METHOD
nsJVMManager::GetCurrentThread(PRThread* *threadID)
{
	*threadID = PR_GetCurrentThread();
	return NS_OK;
}

NS_METHOD
nsJVMManager::Sleep(PRUint32 milli)
{
	PRIntervalTime ticks = (milli > 0 ? PR_MillisecondsToInterval(milli) : PR_INTERVAL_NO_WAIT);
	return (PR_Sleep(ticks) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE);
}

NS_METHOD
nsJVMManager::EnterMonitor(void* address)
{
	return (PR_CEnterMonitor(address) != NULL ? NS_OK : NS_ERROR_FAILURE);
}

NS_METHOD
nsJVMManager::ExitMonitor(void* address)
{
	return (PR_CExitMonitor(address) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE);
}

NS_METHOD
nsJVMManager::Wait(void* address, PRUint32 milli)
{
	PRIntervalTime timeout = (milli > 0 ? PR_MillisecondsToInterval(milli) : PR_INTERVAL_NO_TIMEOUT);
	return (PR_CWait(address, timeout) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE);
}

NS_METHOD
nsJVMManager::Notify(void* address)
{
	return (PR_CNotify(address) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE);
}

NS_METHOD
nsJVMManager::NotifyAll(void* address)
{
	return (PR_CNotifyAll(address) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE);
}

static void PR_CALLBACK thread_starter(void* arg)
{
	nsIRunnable* runnable = (nsIRunnable*) arg;
	if (runnable != NULL) {
		runnable->Run();
	}
}

NS_METHOD
nsJVMManager::CreateThread(PRThread **outThread, nsIRunnable* runnable)
{
	PRThread* thread = PR_CreateThread(PR_USER_THREAD, &thread_starter, (void*) runnable,
									PR_PRIORITY_NORMAL, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
	*outThread = thread;
	return (thread != NULL ?  NS_OK : NS_ERROR_FAILURE);
}

struct JVMRunnableEvent : PLEvent {
	JVMRunnableEvent(nsIRunnable* runnable);
	~JVMRunnableEvent();

	nsIRunnable* mRunnable;	
};

static void PR_CALLBACK
handleRunnableEvent(JVMRunnableEvent* aEvent)
{
	aEvent->mRunnable->Run();
}

static void PR_CALLBACK
destroyRunnableEvent(JVMRunnableEvent* aEvent)
{
	delete aEvent;
}

JVMRunnableEvent::JVMRunnableEvent(nsIRunnable* runnable)
	:	mRunnable(runnable)
{
	NS_ADDREF(mRunnable);
	PL_InitEvent(this, nsnull, PLHandleEventProc(handleRunnableEvent), PLDestroyEventProc(&destroyRunnableEvent));
}

JVMRunnableEvent::~JVMRunnableEvent()
{
	NS_RELEASE(mRunnable);
}

NS_METHOD
nsJVMManager::PostEvent(PRThread* thread, nsIRunnable* runnable, PRBool async)
{
    nsresult rv;
    nsCOMPtr<nsIEventQueueService> eventService = 
             do_GetService(kEventQueueServiceCID, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIEventQueue> eventQueue = NULL;
    rv = eventService->GetThreadEventQueue(thread, getter_AddRefs(eventQueue));
    if (NS_FAILED(rv)) return rv;

    JVMRunnableEvent* runnableEvent = new JVMRunnableEvent(runnable);
    if (runnableEvent == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    if (async)
        eventQueue->PostEvent(runnableEvent);
    else
        eventQueue->PostSynchronousEvent(runnableEvent, nsnull);
    return rv;
}

NS_METHOD
nsJVMManager::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
	 if (!aInstancePtr)
		  return NS_ERROR_INVALID_POINTER;
	 *aInstancePtr = nsnull;

    if (outer && !aIID.Equals(kISupportsIID))
        return NS_ERROR_INVALID_ARG; 
    nsJVMManager* jvmmgr = new nsJVMManager(outer);
    if (jvmmgr == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

	 nsresult rv = jvmmgr->AggregatedQueryInterface(aIID, aInstancePtr);
	 if(NS_FAILED(rv))
		  delete jvmmgr;

	 return rv;
}

nsJVMManager::nsJVMManager(nsISupports* outer)
    : fJVM(NULL), fStatus(nsJVMStatus_Enabled),
      fRegisteredJavaPrefChanged(PR_FALSE), fDebugManager(NULL), fJSJavaVM(NULL),
      fClassPathAdditions(new nsVoidArray()), fClassPathAdditionsString(NULL),
      fStartupMessagePosted(PR_FALSE)
{
    NS_INIT_AGGREGATED(outer);
}

nsJVMManager::~nsJVMManager()
{
    int count = fClassPathAdditions->Count();
    for (int i = 0; i < count; i++) {
        PR_Free((*fClassPathAdditions)[i]);
    }
    delete fClassPathAdditions;
    if (fClassPathAdditionsString)
        PR_Free(fClassPathAdditionsString);
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
    if (aIID.Equals(kIThreadManagerIID)) {
        *aInstancePtr = (void*) NS_STATIC_CAST(nsIThreadManager*, this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kILiveConnectManagerIID)) {
        *aInstancePtr = (void*) NS_STATIC_CAST(nsILiveConnectManager*, this);
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = GetInner();
        NS_ADDREF((nsISupports*)*aInstancePtr);
        return NS_OK;
    }
#if defined(XP_WIN) || defined(XP_OS2)
    // Aggregates...
    if (fDebugManager == NULL) {
        nsresult rslt =
            nsSymantecDebugManager::Create((nsIJVMManager *)this, kISupportsIID,
                                           (void**)&fDebugManager, this);
        if (rslt != NS_OK) return rslt;
    }
    return fDebugManager->QueryInterface(aIID, aInstancePtr);
#else
    return NS_NOINTERFACE;
#endif
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD
nsJVMManager::InitLiveConnectClasses(JSContext* context, JSObject* globalObject)
{
	return (JSJ_InitJSContext(context, globalObject, NULL) ? NS_OK : NS_ERROR_FAILURE);
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD
nsJVMManager::WrapJavaObject(JSContext* context, jobject javaObject, JSObject* *outJSObject)
{
	if (NULL == outJSObject) {
		return NS_ERROR_NULL_POINTER; 
	}
	jsval val;
	if (JSJ_ConvertJavaObjectToJSValue(context, javaObject, &val)) {
		*outJSObject = JSVAL_TO_OBJECT(val);
		return NS_OK;
	}
	return NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////

NS_METHOD
nsJVMManager::GetClasspathAdditions(const char* *result)
{
    if (fClassPathAdditionsString != NULL)
        PR_Free(fClassPathAdditionsString);
    int count = fClassPathAdditions->Count();
    char* classpathAdditions = NULL;
    for (int i = 0; i < count; i++) {
        const char* path = (const char*)(*fClassPathAdditions)[i];
        char* oldPath = classpathAdditions;
        if (oldPath) {
            char sep = PR_GetPathSeparator();
            classpathAdditions = PR_smprintf("%s%c%s", oldPath, sep, path);
            PR_Free(oldPath);
        }
        else
            classpathAdditions = PL_strdup(path);
    }
    fClassPathAdditionsString = classpathAdditions;
    *result = classpathAdditions;  // XXX need to convert to PRUnichar*
    return classpathAdditions ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
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
        while ((path = XP_STRTOK(strtok_path, PR_PATH_SEPARATOR_STR)))
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
        const char* dupmsg = (msg == NULL ? (const char*)NULL : strdup(msg));
    	env->ReleaseStringUTFChars(excString, msg);
    	msg = dupmsg;
    }
    return msg;
}
#endif // 0

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

 /* TODO: Show the progress bar.
	MWContext* someRandomContext = XP_FindSomeContext();
	if (someRandomContext) {
		FE_Progress(someRandomContext, XP_GetString(XP_PROGRESS_STARTING_JAVA));
	}
 */

#ifdef MOZ_OJI_REQUIRE_THREAD_SAFE_ON_STARTUP    
    PRUintn uStatus=0;
    EnterMonitor(&uStatus);
#endif // MOZ_OJI_REQUIRE_THREAD_SAFE_ON_STARTUP    

    PR_ASSERT(fJVM == NULL);
    /*
    **TODO: amusil. Load the plugin by getting into Service manager.
    **              Right now there is no API to do this stuff. We need to
    **              add this to nsIPluginHost. We need a case where we just 
    **              load the plugin but do not instantiate any instance. 
    **              The code in there right now always creates a new instance.
    **              But for Java we may not create any instances and may need to
    **              do JNI calls via liveconnect.
    */

	// beard:  Now uses the nsIPluginHost to load the plugin factory for NS_JVM_MIME_TYPE.
    nsresult err;
    nsCOMPtr<nsIPluginHost> pluginHost = 
             do_GetService(kPluginManagerCID, &err);
    if (NS_FAILED(err)) {
        fStatus = nsJVMStatus_Failed;

#ifdef MOZ_OJI_REQUIRE_THREAD_SAFE_ON_STARTUP    
        ExitMonitor(&uStatus);
#endif // MOZ_OJI_REQUIRE_THREAD_SAFE_ON_STARTUP    

        return fStatus;
    }

    if (!pluginHost) {
        fStatus = nsJVMStatus_Failed;

#ifdef MOZ_OJI_REQUIRE_THREAD_SAFE_ON_STARTUP    
        ExitMonitor(&uStatus);
#endif // MOZ_OJI_REQUIRE_THREAD_SAFE_ON_STARTUP    

        return fStatus;
    }

    nsIPlugin* pluginFactory = NULL;
     // this code is the correct way to obtain pluggable JVM
    nsCOMPtr<nsIPlugin> f = 
             do_GetService(NS_INLINE_PLUGIN_CONTRACTID_PREFIX NS_JVM_MIME_TYPE, &err);
    if (NS_FAILED(err) || !f) {
        err = pluginHost->GetPluginFactory(NS_JVM_MIME_TYPE, &pluginFactory);
    } 
    else {
        pluginFactory  = f;
    }
    
    if (pluginFactory == NULL) {
        fStatus = nsJVMStatus_Failed;

#ifdef MOZ_OJI_REQUIRE_THREAD_SAFE_ON_STARTUP    
        ExitMonitor(&uStatus);
#endif // MOZ_OJI_REQUIRE_THREAD_SAFE_ON_STARTUP    

        return fStatus;
    }

    nsresult rslt = pluginFactory->QueryInterface(kIJVMPluginIID, (void**)&fJVM);
    if (rslt != NS_OK) {
        PR_ASSERT(fJVM == NULL);
        fStatus = nsJVMStatus_Failed;

#ifdef MOZ_OJI_REQUIRE_THREAD_SAFE_ON_STARTUP    
        ExitMonitor(&uStatus);
#endif // MOZ_OJI_REQUIRE_THREAD_SAFE_ON_STARTUP    

        return fStatus;
    }

    // beard: do we really need an explicit startup mechanim for the JVM?
    // since we obtained a working JVM plugin, assume it is running.
    fStatus = nsJVMStatus_Running;

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

#ifdef MOZ_OJI_REQUIRE_THREAD_SAFE_ON_STARTUP    
        ExitMonitor(&uStatus);
#endif // MOZ_OJI_REQUIRE_THREAD_SAFE_ON_STARTUP    

 /* TODO:

    if (someRandomContext) {
        FE_Progress(someRandomContext, XP_GetString(XP_PROGRESS_STARTING_JAVA_DONE));
    }
 */
    return fStatus;
}

nsJVMStatus
nsJVMManager::ShutdownJVM(PRBool fullShutdown)
{
    if (fStatus == nsJVMStatus_Running) {
        PR_ASSERT(fJVM != NULL);
        // XXX need to shutdown JVM via ServiceManager
//        nsresult err = fJVM->ShutdownJVM(fullShutdown);
//        if (err == NS_OK)
            fStatus = nsJVMStatus_Enabled;
//        else {
//            ReportJVMError(err);
//            fStatus = nsJVMStatus_Disabled;
//        }
        fJVM = NULL;
    }
    PR_ASSERT(fJVM == NULL);
    return fStatus;
}

////////////////////////////////////////////////////////////////////////////////

/**

 * This is called when the user changes the state of the
 * security.enable_java preference.  

 */

static int PR_CALLBACK
JavaPrefChanged(const char *prefStr, void* data)
{
    nsJVMManager* mgr = (nsJVMManager*)data;
    PRBool prefBool = PR_TRUE;
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID));
    nsresult rv;
    
    // check for success
    if (!prefs) {
        return 0;
    }
    rv = prefs->GetBoolPref("security.enable_java", &prefBool);
    if (NS_SUCCEEDED(rv)) {
        mgr->SetJVMEnabled(prefBool);
    }

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

/**

 * Called from GetJVMStatus. <P>

 * We only take action once per nsJVMManager instance.  

 */ 

void
nsJVMManager::EnsurePrefCallbackRegistered(void)
{
    if (fRegisteredJavaPrefChanged != PR_TRUE) {
        nsCOMPtr<nsIPref> prefs(do_GetService(kPrefCID));
        PRBool isJavaEnabled = PR_TRUE;
        nsresult rv;

        // check for success
        if (!prefs) {
            return;
        }

        // step one, register the callback for the pref changing.
        rv = prefs->RegisterCallback("security.enable_java", 
                                     JavaPrefChanged, this);
        if (NS_SUCCEEDED(rv)) {
            fRegisteredJavaPrefChanged = PR_TRUE;
        }

        // step two, update our fStatus ivar with the current value of the 
        // pref
        rv = prefs->GetBoolPref("security.enable_java", &isJavaEnabled);
        if (NS_SUCCEEDED(rv)) {
            if (!isJavaEnabled) {
                fStatus = nsJVMStatus_Disabled;
            }
            // else, we leave it with the value it had at construction
        }
    }
}

nsresult
nsJVMManager::GetChrome(nsIWebBrowserChrome **theChrome)
{
    *theChrome = nsnull;

    nsresult rv;
    nsCOMPtr<nsIWindowWatcher> windowWatcher =
        do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
        return rv;
    }
    nsCOMPtr<nsIDOMWindow> domWindow;
    windowWatcher->GetActiveWindow(getter_AddRefs(domWindow));
    nsCOMPtr<nsIScriptGlobalObject> scriptObject =
        do_QueryInterface(domWindow, &rv);
    if (!scriptObject) {
        return rv;
    }
    nsIDocShell *docShell = scriptObject->GetDocShell();
    if (!docShell) {
        return NS_OK;
    }
    nsCOMPtr<nsIPresContext> presContext;
    rv = docShell->GetPresContext(getter_AddRefs(presContext));
    if (!presContext) {
        return rv;
    }
    nsCOMPtr<nsISupports> container(presContext->GetContainer());
    nsCOMPtr<nsIDocShellTreeItem> treeItem = do_QueryInterface(container, &rv);
    if (!treeItem) {
        return rv;
    }
    nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
    treeItem->GetTreeOwner(getter_AddRefs(treeOwner));

    nsCOMPtr<nsIWebBrowserChrome> chrome = do_GetInterface(treeOwner, &rv);
    *theChrome = (nsIWebBrowserChrome *) chrome.get();
    NS_IF_ADDREF(*theChrome);
    return rv;
}

nsJVMStatus
nsJVMManager::GetJVMStatus(void)
{
    EnsurePrefCallbackRegistered();
    return fStatus;
}

extern "C" nsresult JSJ_RegisterLiveConnectFactory(void);

PRBool
nsJVMManager::MaybeStartupLiveConnect(void)
{
    if (fJSJavaVM)
        return PR_TRUE;

	do {
		static PRBool registeredLiveConnectFactory = NS_SUCCEEDED(JSJ_RegisterLiveConnectFactory());
        if (IsLiveConnectEnabled() && StartupJVM() == nsJVMStatus_Running) {
            JVM_InitLCGlue();
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
#if 0
// TODO: Get a replacement for LM_GetMochaEnabled. This is on Tom's list.
	if (LM_GetMochaEnabled()) {
		nsJVMStatus status = GetJVMStatus();
		return (status == nsJVMStatus_Enabled || status == nsJVMStatus_Running);
	}
#endif
	return PR_TRUE;
}

/*
 * Find out if a given signer has been granted all permissions. This
 * is a precondition to loading a signed applet in trusted mode.
 * The certificate from which the fingerprint and commonname have
 * be derived, should have been verified before this method is 
 * called.
 */

NS_METHOD
nsJVMManager::IsAllPermissionGranted(
    const char * lastFP,
    const char * lastCN, 
    const char * rootFP,
    const char * rootCN, 
    PRBool * isGranted)
{
    nsresult rv      = NS_OK;

    nsCOMPtr<nsIPrincipal> pIPrincipal;
  
    // Get the Script Security Manager.

    nsCOMPtr<nsIScriptSecurityManager> secMan = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv) || !secMan) return PR_FALSE;

    // Ask the Script Security Manager to make a Certificate Principal.
    // The fingerprint is a one way hash of this certificate. It is used
    // as the key to store the principal in the principal database.

    rv = secMan->GetCertificatePrincipal(lastFP, nsnull,
                                         getter_AddRefs(pIPrincipal));
    if (NS_FAILED(rv)) return PR_FALSE;

    // Set the common name.
    rv = pIPrincipal->SetCommonName(lastCN);

    PRInt16 ret;

    secMan->RequestCapability(pIPrincipal,"AllPermission",&ret);

    PR_ASSERT(isGranted);
    *isGranted = (ret!=0);

    return PR_TRUE;
}

NS_METHOD
nsJVMManager::IsAppletTrusted(
    const char* aRSABuf, 
    PRUint32    aRSABufLen, 
    const char* aPlaintext, 
    PRUint32    aPlaintextLen,    
    PRBool*     isTrusted,
    nsIPrincipal** pIPrincipal)
{
    nsresult rv      = NS_OK;

    //-- Get the signature verifier service
    nsCOMPtr<nsISignatureVerifier> verifier = 
             do_GetService(SIGNATURE_VERIFIER_CONTRACTID, &rv);
    if (NS_FAILED(rv)) // No signature verifier available
        return NS_OK;

    // Get the Script Security Manager.

    nsCOMPtr<nsIScriptSecurityManager> secMan = 
             do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv) || !secMan) return PR_FALSE;


    // Ask the Script Security Manager to make a Certificate Principal.
    // The fingerprint is a one way hash of this certificate. It is used
    // as the key to store the principal in the principal database.

    {
        PRInt32 ret;
        PR_ASSERT(pIPrincipal);
        rv = verifier->VerifySignature(aRSABuf,aRSABufLen,aPlaintext,aPlaintextLen,&ret,pIPrincipal);
        if (NS_FAILED(rv)) return PR_FALSE;
    }

    {
        PRInt16 ret = 0;
        secMan->RequestCapability(*pIPrincipal,"UniversalBrowserRead",&ret);
        PR_ASSERT(isTrusted);
        *isTrusted = (ret!=0);
    }

    return PR_TRUE;
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
            char sep = PR_GetDirectorySeparator();
            char* path = PR_smprintf("%s%c%s", dirPath, sep, PR_DirName(dirent));
			if (path != NULL) {
        		PRBool freePath = PR_TRUE;
	            if ((PR_GetFileInfo(path, &info) == PR_SUCCESS)
	                && (info.type == PR_FILE_FILE)) {
	                int len = PL_strlen(path);

	                /* Is it a zip or jar file? */
	                if ((len > 4) && 
	                    ((PL_strcasecmp(path+len-4, ".zip") == 0) || 
	                     (PL_strcasecmp(path+len-4, ".jar") == 0))) {
	                    fClassPathAdditions->AppendElement((void*)path);
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
    fClassPathAdditions->AppendElement((void*)dirPath);
    if (jvm) {
        jvm->AddToClassPath(dirPath);
        // jvm->Release(); // GetJVMPlugin no longer calls AddRef
    }
    return NS_OK;
}

