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

#include "jvmmgr.h"
#include "npglue.h"
#include "xp.h"
#include "prefapi.h"
#include "xp_str.h"
#include "np.h"
#include "prio.h"
#include "prmem.h"
#include "prthread.h"
#include "pprthred.h"
#include "plstr.h"
#include "jni.h"
#include "jsjava.h"
#include "jsdbgapi.h"
#include "libmocha.h"
#include "libevent.h"
#include "nsCCapsManager.h"
#include "prinrval.h"
#include "ProxyJNI.h"
#include "prcmon.h"
#include "nsCSecurityContext.h"
#include "nsISecurityContext.h"
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

static PRUintn tlsIndex1_g = 0;
PRUintn tlsIndex2_g = 0;
PRUintn tlsIndex3_g = 0;

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIJVMManagerIID, NS_IJVMMANAGER_IID);
static NS_DEFINE_IID(kIThreadManagerIID, NS_ITHREADMANAGER_IID);
static NS_DEFINE_IID(kIJVMPluginIID, NS_IJVMPLUGIN_IID);
static NS_DEFINE_IID(kISymantecDebugManagerIID, NS_ISYMANTECDEBUGMANAGER_IID);
static NS_DEFINE_IID(kIJVMPluginInstanceIID, NS_IJVMPLUGININSTANCE_IID);
static NS_DEFINE_IID(kIJVMPluginTagInfoIID, NS_IJVMPLUGINTAGINFO_IID);
static NS_DEFINE_IID(kIPluginTagInfo2IID, NS_IPLUGINTAGINFO2_IID);
static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIJVMConsoleIID, NS_IJVMCONSOLE_IID);
static NS_DEFINE_IID(kISymantecDebuggerIID, NS_ISYMANTECDEBUGGER_IID);
static NS_DEFINE_IID(kISecurityContextIID, NS_ISECURITYCONTEXT_IID);

////////////////////////////////////////////////////////////////////////////////

NS_IMPL_AGGREGATED(nsJVMMgr);

extern "C" {
static nsIJVMPlugin* GetRunningJVM(void);
}

NS_METHOD
nsJVMMgr::CreateProxyJNI(nsISecureJNI2* inSecureEnv, JNIEnv** outProxyEnv)
{
    nsIJVMPlugin* jvmPlugin = GetRunningJVM();
	if (jvmPlugin != NULL) {
		*outProxyEnv = ::CreateProxyJNI(jvmPlugin, inSecureEnv);
		return NS_OK;
	}
	return NS_ERROR_FAILURE;
}

// nsIThreadManager:

NS_METHOD
nsJVMMgr::GetCurrentThread(PRUint32* threadID)
{
	*threadID = PRUint32(PR_GetCurrentThread());
	return NS_OK;
}

NS_METHOD
nsJVMMgr::Sleep(PRUint32 milli)
{
	PRIntervalTime ticks = (milli > 0 ? PR_MillisecondsToInterval(milli) : PR_INTERVAL_NO_WAIT);
	return (PR_Sleep(ticks) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE);
}

NS_METHOD
nsJVMMgr::EnterMonitor(void* address)
{
	return (PR_CEnterMonitor(address) != NULL ? NS_OK : NS_ERROR_FAILURE);
}

NS_METHOD
nsJVMMgr::ExitMonitor(void* address)
{
	return (PR_CExitMonitor(address) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE);
}

NS_METHOD
nsJVMMgr::Wait(void* address, PRUint32 milli)
{
	PRIntervalTime timeout = (milli > 0 ? PR_MillisecondsToInterval(milli) : PR_INTERVAL_NO_TIMEOUT);
	return (PR_CWait(address, timeout) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE);
}

NS_METHOD
nsJVMMgr::Notify(void* address)
{
	return (PR_CNotify(address) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE);
}

NS_METHOD
nsJVMMgr::NotifyAll(void* address)
{
	return (PR_CNotifyAll(address) == PR_SUCCESS ? NS_OK : NS_ERROR_FAILURE);
}

static void thread_starter(void* arg)
{
	nsIRunnable* runnable = (nsIRunnable*) arg;
	if (runnable != NULL) {
		runnable->Run();
	}
}

NS_METHOD
nsJVMMgr::CreateThread(PRUint32* outThreadID, nsIRunnable* runnable)
{
	PRThread* thread = PR_CreateThread(PR_USER_THREAD, &thread_starter, (void*) runnable,
									PR_PRIORITY_LOW, PR_GLOBAL_THREAD, PR_JOINABLE_THREAD, 0);
	*outThreadID = (PRUint32) thread;
	return (thread != NULL ?  NS_OK : NS_ERROR_FAILURE);
}

NS_METHOD
nsJVMMgr::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;   // XXX right error?
    nsJVMMgr* jvmmgr = new nsJVMMgr(outer);
    if (jvmmgr == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    jvmmgr->AddRef();
    *aInstancePtr = (outer != NULL ? (void*) jvmmgr->GetInner() : (void*) jvmmgr);
    return NS_OK;
}

nsJVMMgr::nsJVMMgr(nsISupports* outer)
    : fJVM(NULL), fStatus(nsJVMStatus_Enabled),
      fRegisteredJavaPrefChanged(PR_FALSE), fDebugManager(NULL), fJSJavaVM(NULL),
      fClassPathAdditions(new nsVector())
{
    NS_INIT_AGGREGATED(outer);
    PR_NewThreadPrivateIndex(&tlsIndex1_g, NULL);
    PR_NewThreadPrivateIndex(&tlsIndex2_g, NULL);
    PR_NewThreadPrivateIndex(&tlsIndex3_g, NULL);
}

nsJVMMgr::~nsJVMMgr()
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
nsJVMMgr::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (aIID.Equals(kIJVMManagerIID)) {
        *aInstancePtr = this;
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kIThreadManagerIID)) {
        *aInstancePtr = (void*) (nsIThreadManager*) this;
        AddRef();
        return NS_OK;
    }
#ifdef XP_PC
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
// LiveConnect callbacks
////////////////////////////////////////////////////////////////////////////////

template <class T>
class ThreadLocalStorage {
public:
	ThreadLocalStorage(PRThreadPrivateDTOR dtor) : mIndex(0), mValid(PR_FALSE)
	{
		mValid = (PR_NewThreadPrivateIndex(&mIndex, dtor) == PR_SUCCESS);
	}
	
	void set(T value)
	{
		if (mValid) PR_SetThreadPrivate(mIndex, value);
	}
	
	T get()
	{
		return (T) (mValid ? PR_GetThreadPrivate(mIndex) : 0);
	}

private:
	PRUintn mIndex;
	PRBool mValid;
};

PR_BEGIN_EXTERN_C

#include "jscntxt.h"


static JSContext* PR_CALLBACK
map_jsj_thread_to_js_context_impl(JSJavaThreadState *jsj_env, JNIEnv *env, char **errp)
{
    JSContext *cx    = LM_GetCrippledContext();

    *errp = NULL;
#if 0    
    nsJVMMgr* pJVMMgr = JVM_GetJVMMgr();
    if (pJVMMgr != NULL) {
        nsIJVMPlugin* pJVMPI = pJVMMgr->GetJVMPlugin();
        jvmMochaPrefsEnabled = LM_GetMochaEnabled();
        if (pJVMPI != NULL) {
            nsIPluginInstance* pPIT;
            nsresult err = pJVMPI->GetPluginInstance(env, &pPIT);
            if ( (err == NS_OK) && (pPIT != NULL) ) {
                nsIJVMPluginInstance* pJVMPIT;
                if (pPIT->QueryInterface(kIJVMPluginInstanceIID,
                                         (void**)&pJVMPIT) == NS_OK) {
                    nsPluginInstancePeer* pPITP;
                    err = pJVMPIT->GetPeer((nsIPluginInstancePeer**)&pPITP); 
                    if ( (err == NS_OK) &&(pPITP != NULL) ) {
                        cx = pPITP->GetJSContext();
                        pPITP->Release();
                    }
                    pJVMPIT->Release();
                }
                pPIT->Release();
            }
            // pJVMPI->Release(); // GetJVMPlugin no longer calls AddRef
        }
        pJVMMgr->Release();
    }
    if (jvmMochaPrefsEnabled == PR_FALSE)
    {
        *errp = strdup("Java preference is disabled");
        return NULL;
    }

    if ( cx == NULL )
    {
        *errp = strdup("Java thread could not be associated with a JSContext");
        return NULL;
    }
#endif
    return cx;
}

static void PR_CALLBACK detach_jsjava_thread_state(void* env)
{
    JSJavaThreadState *jsj_env = (JSJavaThreadState*)env;
    JSJ_DetachCurrentThreadFromJava(jsj_env);
}

/*
** This callback is called to map a JSContext to a JSJavaThreadState which
** is a wrapper around JNIEnv. Hence this callback essentially maps a JSContext
** to a java thread. JSJ_AttachCurrentThreadToJava just calls AttachCurrentThread
** on the java vm.
*/
static JSJavaThreadState* PR_CALLBACK
map_js_context_to_jsj_thread_impl(JSContext *cx, char **errp)
{
	*errp = NULL;

	static ThreadLocalStorage<JSJavaThreadState*> localThreadState(&detach_jsjava_thread_state);
	JSJavaThreadState* jsj_env = localThreadState.get();
	if (jsj_env != NULL)
		return jsj_env;
    
	if (ET_InitMoja(0) != LM_MOJA_OK) {
		*errp = strdup("LiveConnect initialization failed.");
		return NULL;
	}

	JSJavaVM* js_jvm = NULL;
	nsJVMMgr* pJVMMgr = JVM_GetJVMMgr();
	if (pJVMMgr != NULL) {
		js_jvm = pJVMMgr->GetJSJavaVM();
		pJVMMgr->Release();
		if (js_jvm == NULL) {
			*errp = strdup("Failed to attach to a Java VM.");
			return NULL;
		}
	}

	jsj_env = JSJ_AttachCurrentThreadToJava(js_jvm, NULL, NULL);
	localThreadState.set(jsj_env);
 PR_SetThreadPrivate(tlsIndex3_g, (void *)cx);

	return jsj_env;
}

/*
** This callback is called to map a applet,bean to its corresponding JSObject
** created on javascript side and then map that to a java wrapper JSObject class.
** This callback is called in JSObject.getWindow implementation to get
** a java wrapper JSObject class for a applet only once.
** Note that once a mapping between applet -> javascript JSObject -> Java wrapper JSObject 
** is made, all subsequent method calls via JSObject use the internal field
** to get to the javascript JSObject.
*/
static JSObject* PR_CALLBACK
map_java_object_to_js_object_impl(JNIEnv *env, void *pNSIPluginInstanceIn, char **errp)
{
    MWContext       *cx;
    JSObject        *window;
    MochaDecoder    *decoder; 
    PRBool           mayscript = PR_FALSE;
    PRBool           jvmMochaPrefsEnabled = PR_FALSE;
    nsresult         err = NS_OK;

    *errp = NULL;
    /* XXX assert JS is locked */

    if (!pNSIPluginInstanceIn) {
        env->ThrowNew(env->FindClass("java/lang/NullPointerException"),0);
        return 0;
    }

    nsJVMMgr* pJVMMgr = JVM_GetJVMMgr();
    if (pJVMMgr != NULL) {
        nsIJVMPlugin* pJVMPI = pJVMMgr->GetJVMPlugin();
        jvmMochaPrefsEnabled = LM_GetMochaEnabled();
        if (pJVMPI != NULL) {
            //jobject javaObject = applet;
            nsIPluginInstance* pPIT;
            //nsresult err = pJVMPI->GetPluginInstance(javaObject, &pPIT);
            pPIT = (nsIPluginInstance*)pNSIPluginInstanceIn;
            if ( (err == NS_OK) && (pPIT != NULL) ) {
                nsIJVMPluginInstance* pJVMPIT;
                if (pPIT->QueryInterface(kIJVMPluginInstanceIID,
                                         (void**)&pJVMPIT) == NS_OK) {
                    nsPluginInstancePeer* pPITP;
                    err = pJVMPIT->GetPeer((nsIPluginInstancePeer**)&pPITP); 
                    if ( (err == NS_OK) &&(pPITP != NULL) ) {
                        nsIJVMPluginTagInfo* pJVMTagInfo;
                        if (pPITP->QueryInterface(kIJVMPluginTagInfoIID,
                                                  (void**)&pJVMTagInfo) == NS_OK) {
                            err = pJVMTagInfo->GetMayScript(&mayscript);
                            PR_ASSERT(err != NS_OK ? mayscript == PR_FALSE : PR_TRUE);
                            pJVMTagInfo->Release();
                        }
                        cx = pPITP->GetMWContext();
                        pPITP->Release();
                    }
                    pJVMPIT->Release();
                }
                // pPIT->Release();	// pNSIPluginInstanceIn is passed-in.
            }
            // pJVMPI->Release(); // GetJVMPlugin no longer calls AddRef
        }
        pJVMMgr->Release();
    }

    if (  (mayscript            == PR_FALSE)
          ||(jvmMochaPrefsEnabled == PR_FALSE)
        )
    {
        *errp = strdup("JSObject.getWindow() requires mayscript attribute on this Applet or java preference is disabled");
        goto except;
    }


    if (!cx || (cx->type != MWContextBrowser && cx->type != MWContextPane))
    {
        *errp = strdup("JSObject.getWindow() can only be called in MWContextBrowser or MWContextPane");
        return 0;
    }

    decoder = LM_GetMochaDecoder(cx);

    /* if there is a decoder now, reflect the window */
    if (decoder && (jvmMochaPrefsEnabled == PR_TRUE)) {
        window = decoder->window_object;
    }

    LM_PutMochaDecoder(decoder);

    return window;
  except:
    return 0;
}

#if 0
static JavaVM* PR_CALLBACK
get_java_vm_impl(char **errp)
{
    *errp = NULL;
    JavaVM *pJavaVM = NULL;
    
    nsJVMMgr* pJVMMgr = JVM_GetJVMMgr();
    if (pJVMMgr != NULL) {
        nsIJVMPlugin* pJVMPI = pJVMMgr->GetJVMPlugin();
        if (pJVMPI != NULL) {
            nsresult err = pJVMPI->GetJavaVM(&pJavaVM);
            PR_ASSERT(err != NS_OK ? pJavaVM == NULL : PR_TRUE);
            // pJVMPI->Release(); // GetJVMPlugin no longer calls AddRef
        }
        pJVMMgr->Release();
    }
    if ( pJavaVM == NULL )
    {
        *errp = strdup("Could not find a JavaVM to attach to.");
    }
    return pJavaVM;
}
#endif

static JSPrincipals* PR_CALLBACK
get_JSPrincipals_from_java_caller_impl(JNIEnv *pJNIEnv, JSContext *pJSContext, void  **ppNSIPrincipalArrayIN, int numPrincipals, void *pNSISecurityContext)
{
    nsIPrincipal  **ppNSIPrincipalArray = (nsIPrincipal  **)ppNSIPrincipalArrayIN;
    PRInt32        length = 0;
    nsresult       err    = NS_OK;
    nsJVMMgr* pJVMMgr = JVM_GetJVMMgr();
    void          *pNSPrincipalArray = NULL;
    if (pJVMMgr != NULL) {
         if (ppNSIPrincipalArray != NULL) {
             nsIPluginManager *pNSIPluginManager = NULL;
             NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);
             err = pJVMMgr->QueryInterface(kIPluginManagerIID,
                                  (void**)&pNSIPluginManager);

             if(   (err == NS_OK) 
                && (pNSIPluginManager != NULL )
               )
             {
               nsCCapsManager *pNSCCapsManager = NULL;
               NS_DEFINE_IID(kICapsManagerIID, NS_ICAPSMANAGER_IID);
               err = pNSIPluginManager->QueryInterface(kICapsManagerIID, (void**)&pNSCCapsManager);
               if(   (err == NS_OK) 
                  && (pNSCCapsManager != NULL)
                 )
               {
                 PRInt32 i=0;
                 nsPrincipal *pNSPrincipal = NULL;
                 pNSPrincipalArray = nsCapsNewPrincipalArray(length);
                 if (pNSPrincipalArray != NULL) 
                 {
                   while( i<length )
                   {
                      err = pNSCCapsManager->GetNSPrincipal(ppNSIPrincipalArray[i], &pNSPrincipal);
                      nsCapsSetPrincipalArrayElement(pNSPrincipalArray, i, pNSPrincipal);
                      i++;
                   }
                 }
                 pNSCCapsManager->Release();
               }
               pNSIPluginManager->Release();
             }
         }
      pJVMMgr->Release();
    }
    if (  (pNSPrincipalArray != NULL)
        &&(length != 0)
       )
    {
        return LM_GetJSPrincipalsFromJavaCaller(pJSContext, pNSPrincipalArray, pNSISecurityContext);
    }

    return NULL;
}

static jobject PR_CALLBACK
get_java_wrapper_impl(JNIEnv *pJNIEnv, jint jsobject)
{
    nsresult       err    = NS_OK;
    jobject  pJSObjectWrapper = NULL;
    nsJVMMgr* pJVMMgr = JVM_GetJVMMgr();
    if (pJVMMgr != NULL) {
      nsIJVMPlugin* pJVMPI = pJVMMgr->GetJVMPlugin();
      if (pJVMPI != NULL) {
         err = pJVMPI->GetJavaWrapper(pJNIEnv, jsobject, &pJSObjectWrapper);
         //pJVMPI->Release();
      }
      pJVMMgr->Release();
    }
    if ( err != NS_OK )
    {
       return NULL;
    }
    return pJSObjectWrapper;
}

static JSBool PR_CALLBACK
enter_js_from_java_impl(JNIEnv *jEnv, char **errp,
                        void **pNSIPrincipaArray, int numPrincipals, void *pNSISecurityContext)
{
    MWContext *cx = XP_FindSomeContext();   /* XXXMLM */
    JSContext *pJSCX = (JSContext *)PR_GetThreadPrivate(tlsIndex3_g);
    if (pJSCX == NULL)
    {
       pJSCX = LM_GetCrippledContext();
    }

    // Setup tls to maintain a stack of security contexts.
    if(  (pNSIPrincipaArray   != NULL)
       &&(pNSISecurityContext != NULL)
      )
    {
      ThreadLocalStorageAtIndex1 *pSecInfoNew = NULL;
      pSecInfoNew = (ThreadLocalStorageAtIndex1 *)malloc(sizeof(ThreadLocalStorageAtIndex1));
      pSecInfoNew->pNSIPrincipaArray   = pNSIPrincipaArray;
      pSecInfoNew->numPrincipals       = numPrincipals;
      pSecInfoNew->pNSISecurityContext = pNSISecurityContext;
      pSecInfoNew->prev                = pSecInfoNew;
      pSecInfoNew->next                = pSecInfoNew;
      JSStackFrame *fp                 = NULL;
      pSecInfoNew->pJavaToJSSFrame            = JS_FrameIterator(pJSCX, &fp);
      ThreadLocalStorageAtIndex1 *pSecInfoTop = NULL;
      pSecInfoTop = (ThreadLocalStorageAtIndex1 *)PR_GetThreadPrivate(tlsIndex1_g);
      if (pSecInfoTop == NULL)
      {
        PR_SetThreadPrivate(tlsIndex1_g, (void *)pSecInfoNew);
      }
      else
      {
        pSecInfoTop->prev->next = pSecInfoNew;
        pSecInfoNew->prev       = pSecInfoTop->prev;
        pSecInfoNew->next       = pSecInfoTop;
        pSecInfoTop->prev       = pSecInfoNew;
      }
    }

    return LM_LockJS(cx, errp);
}

static void PR_CALLBACK
exit_js_impl(JNIEnv *jEnv)
{
    MWContext *cx = XP_FindSomeContext();   /* XXXMLM */

    LM_UnlockJS(cx);

    // Pop the security context stack
    ThreadLocalStorageAtIndex1 *pSecInfoTop = NULL;
    pSecInfoTop = (ThreadLocalStorageAtIndex1 *)PR_GetThreadPrivate(tlsIndex1_g);
    if (pSecInfoTop != NULL)
    {
      if(pSecInfoTop->next == pSecInfoTop)
      {
        PR_SetThreadPrivate(tlsIndex1_g, NULL);
        pSecInfoTop->next        = NULL;            
        pSecInfoTop->prev        = NULL;            
        free(pSecInfoTop);
      }
      else
      {
        ThreadLocalStorageAtIndex1 *tail = pSecInfoTop->prev;
        tail->next        = NULL;            
        pSecInfoTop->prev = tail->prev;        
        tail->prev->next  = pSecInfoTop;
        tail->prev        = NULL;
        free(tail);
      }
    }

    return;
}

static PRBool PR_CALLBACK
create_java_vm_impl(SystemJavaVM* *jvm, JNIEnv* *initialEnv, void* initargs)
{
    const char* classpath = (const char*)initargs;      // unused (should it be?)
    *jvm = (SystemJavaVM*)JVM_GetJVMMgr();              // unused in the browser
    *initialEnv = JVM_GetJNIEnv();
    return (*jvm != NULL && *initialEnv != NULL);
}

static PRBool PR_CALLBACK
destroy_java_vm_impl(SystemJavaVM* jvm, JNIEnv* initialEnv)
{
    JVM_ReleaseJNIEnv(initialEnv);
    // need to release jvm
    return PR_TRUE;
}

static JNIEnv* PR_CALLBACK
attach_current_thread_impl(SystemJavaVM* jvm)
{
    return JVM_GetJNIEnv();
}

static PRBool PR_CALLBACK
detach_current_thread_impl(SystemJavaVM* jvm, JNIEnv* env)
{
    JVM_ReleaseJNIEnv(env);
    return PR_TRUE;
}

static SystemJavaVM* PR_CALLBACK
get_java_vm_impl(JNIEnv* env)
{
    // only one SystemJavaVM for the whole browser, so it doesn't depend on env
    return (SystemJavaVM*)JVM_GetJVMMgr();
}

PR_END_EXTERN_C


/*
 * Callbacks for client-specific jsjava glue
 */
static JSJCallbacks jsj_callbacks = {
    map_jsj_thread_to_js_context_impl,
    map_js_context_to_jsj_thread_impl,
    map_java_object_to_js_object_impl,
    get_JSPrincipals_from_java_caller_impl,
    enter_js_from_java_impl,
    exit_js_impl,
    NULL,       // error_print
    get_java_wrapper_impl,
    create_java_vm_impl,
    destroy_java_vm_impl,
    attach_current_thread_impl,
    detach_current_thread_impl,
    get_java_vm_impl
};

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
#endif

void
nsJVMMgr::ReportJVMError(nsresult err)
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
nsJVMMgr::GetJavaErrorString(JNIEnv* env)
{
    /* XXX javah is a pain wrt mixing JRI and JDK native methods. 
       Since we need to call a method on Object, we'll do it the hard way 
       to avoid using javah for this.
       Maybe we need a new JRI entry point for toString. Yikes! */

#if 1 //def XP_MAC
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
#else
    const char* msg;
    struct java_lang_Class* classObject;
    JRIMethodID toString;
    struct java_lang_String* excString;
    struct java_lang_Throwable* exc = JRI_ExceptionOccurred(env);

    if (exc == NULL) {
        return strdup("");	/* XXX better "no error" message? */
    }

    /* Got to do this before trying to find a class (like Object). 
       This is because the runtime refuses to do this with a pending 
       exception! I think it's broken. */

    JRI_ExceptionClear(env);

    classObject = (struct java_lang_Class*)
        JRI_FindClass(env, classname_java_lang_Object);
    toString = JRI_GetMethodID(env, classObject,
                               name_java_lang_Object_toString,
                               sig_java_lang_Object_toString);
    excString = (struct java_lang_String *)
        JRI_CallMethod(env)(env, JRI_CallMethod_op,
                            (struct java_lang_Object*)exc, toString);

    /* XXX change to JRI_GetStringPlatformChars */
    msg = JRI_GetStringPlatformChars(env, excString, NULL, 0);
    if (msg == NULL) return NULL;
    return strdup(msg);
#endif
}

////////////////////////////////////////////////////////////////////////////////

PRLogModuleInfo* NSJAVA = NULL;

nsJVMStatus
nsJVMMgr::StartupJVM(void)
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
nsJVMMgr::ShutdownJVM(PRBool fullShutdown)
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
    nsJVMMgr* mgr = (nsJVMMgr*)data;
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
nsJVMMgr::SetJVMEnabled(PRBool enabled)
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
nsJVMMgr::EnsurePrefCallbackRegistered(void)
{
    if (fRegisteredJavaPrefChanged != PR_TRUE) {
        fRegisteredJavaPrefChanged = PR_TRUE;
        PREF_RegisterCallback("security.enable_java", JavaPrefChanged, this);
        JavaPrefChanged("security.enable_java", this);
    }
}

nsJVMStatus
nsJVMMgr::GetJVMStatus(void)
{
    EnsurePrefCallbackRegistered();
    return fStatus;
}

#ifdef XP_MAC
#define JSJDLL "LiveConnect"
#endif

PRBool
nsJVMMgr::MaybeStartupLiveConnect()
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
	        JSJ_Init(&jsj_callbacks);
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
nsJVMMgr::MaybeShutdownLiveConnect(void)
{
    if (fJSJavaVM) {
        JSJ_DisconnectFromJavaVM(fJSJavaVM);
        fJSJavaVM = NULL;
        return PR_TRUE; 
    }
    return PR_FALSE;
}

PRBool
nsJVMMgr::IsLiveConnectEnabled(void)
{
	if (LM_GetMochaEnabled()) {
		nsJVMStatus status = GetJVMStatus();
		return (status == nsJVMStatus_Enabled || status == nsJVMStatus_Running);
	}
	return PR_FALSE;
}

////////////////////////////////////////////////////////////////////////////////

nsresult
nsJVMMgr::AddToClassPath(const char* dirPath)
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
// nsJVMPluginTagInfo
////////////////////////////////////////////////////////////////////////////////

nsJVMPluginTagInfo::nsJVMPluginTagInfo(nsISupports* outer, nsIPluginTagInfo2* info)
    : fPluginTagInfo(info), fSimulatedCodebase(NULL), fSimulatedCode(NULL)
{
    NS_INIT_AGGREGATED(outer);
}

nsJVMPluginTagInfo::~nsJVMPluginTagInfo(void)
{
    if (fSimulatedCodebase)
        PL_strfree(fSimulatedCodebase);

    if (fSimulatedCode)
        PL_strfree(fSimulatedCode);
}

NS_IMPL_AGGREGATED(nsJVMPluginTagInfo);

NS_METHOD
nsJVMPluginTagInfo::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (aIID.Equals(kIJVMPluginTagInfoIID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = this;
        AddRef();
        return NS_OK;
    }
    return NS_NOINTERFACE;
}


static void
oji_StandardizeCodeAttribute(char* buf)
{
    // strip off the ".class" suffix
    char* cp;

    if ((cp = PL_strrstr(buf, ".class")) != NULL)
        *cp = '\0';

    // Convert '/' to '.'
    cp = buf;
    while ((*cp) != '\0') {
        if ((*cp) == '/')
            (*cp) = '.';

        ++cp;
    }
}

NS_METHOD
nsJVMPluginTagInfo::GetCode(const char* *result)
{
    if (fSimulatedCode) {
        *result = fSimulatedCode;
        return NS_OK;
    }

    const char* code;
    nsresult err = fPluginTagInfo->GetAttribute("code", &code);
    if (err == NS_OK && code) {
        fSimulatedCode = PL_strdup(code);
        oji_StandardizeCodeAttribute(fSimulatedCode);
        *result = fSimulatedCode;
        return NS_OK;
    }

    const char* classid;
    err = fPluginTagInfo->GetAttribute("classid", &classid);
    if (err == NS_OK && classid && PL_strncasecmp(classid, "java:", 5) == 0) {
        fSimulatedCode = PL_strdup(classid + 5); // skip "java:"
        oji_StandardizeCodeAttribute(fSimulatedCode);
        *result = fSimulatedCode;
        return NS_OK;
    }

    // XXX what about "javaprogram:" and "javabean:"?
    return NS_ERROR_FAILURE;
}

NS_METHOD
nsJVMPluginTagInfo::GetCodeBase(const char* *result)
{
    // If we've already cached and computed the value, use it...
    if (fSimulatedCodebase) {
        *result = fSimulatedCodebase;
        return NS_OK;
    }

    // See if it's supplied as an attribute...
    const char* codebase;
    nsresult err = fPluginTagInfo->GetAttribute("codebase", &codebase);
    if (err == NS_OK && codebase != NULL) {
        *result = codebase;
        return NS_OK;
    }

    // Okay, we'll need to simulate it from the layout tag's base URL.
    const char* docBase;
    err = fPluginTagInfo->GetDocumentBase(&docBase);
    if (err != NS_OK) return err;
    PA_LOCK(codebase, const char*, docBase);

    if ((fSimulatedCodebase = PL_strdup(codebase)) != NULL) {
        char* lastSlash = PL_strrchr(fSimulatedCodebase, '/');

        // chop of the filename from the original document base URL to
        // generate the codebase.
        if (lastSlash != NULL)
            *(lastSlash + 1) = '\0';
    }
    
    PA_UNLOCK(docBase);
    *result = fSimulatedCodebase;
    return NS_OK;
}

NS_METHOD
nsJVMPluginTagInfo::GetArchive(const char* *result)
{
    return fPluginTagInfo->GetAttribute("archive", result);
}

NS_METHOD
nsJVMPluginTagInfo::GetName(const char* *result)
{
    const char* attrName;
    nsPluginTagType type;
    nsresult err = fPluginTagInfo->GetTagType(&type);
    if (err != NS_OK) return err;
    switch (type) {
      case nsPluginTagType_Applet:
        attrName = "name";
        break;
      default:
        attrName = "id";
        break;
    }
    return fPluginTagInfo->GetAttribute(attrName, result);
}

NS_METHOD
nsJVMPluginTagInfo::GetMayScript(PRBool *result)
{
    const char* attr;
    *result = PR_FALSE;

    nsresult err = fPluginTagInfo->GetAttribute("mayscript", &attr);
    if (err) return err;

    if (PL_strcasecmp(attr, "true") == 0)
    {
       *result = PR_TRUE;
    }
    return NS_OK;
}

NS_METHOD
nsJVMPluginTagInfo::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr,
                           nsIPluginTagInfo2* info)
{
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;   // XXX right error?

    nsJVMPluginTagInfo* jvmTagInfo = new nsJVMPluginTagInfo(outer, info);
    if (jvmTagInfo == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    jvmTagInfo->AddRef();
    *aInstancePtr = jvmTagInfo->GetInner();

    nsresult result = outer->QueryInterface(kIPluginTagInfo2IID,
                                            (void**)&jvmTagInfo->fPluginTagInfo);
    if (result != NS_OK) goto error;
    outer->Release();   // no need to AddRef outer
    return result;

  error:
    delete jvmTagInfo;
    return result;
}

////////////////////////////////////////////////////////////////////////////////
// Convenience Routines

PR_BEGIN_EXTERN_C

extern nsPluginManager* thePluginManager;

PR_IMPLEMENT(nsJVMMgr*)
JVM_GetJVMMgr(void)
{
	nsresult result = NS_OK;
    if (thePluginManager == NULL) {
        result = nsPluginManager::Create(NULL, kIPluginManagerIID, (void**)&thePluginManager);
		if (result != NS_OK)
			return NULL;
    }
    nsJVMMgr* mgr = NULL;
    result = thePluginManager->QueryInterface(kIJVMManagerIID, (void**)&mgr);
    if (result != NS_OK)
        return NULL;
    return mgr;
}

static nsIJVMPlugin*
GetRunningJVM()
{
    nsIJVMPlugin* jvm = NULL;
    nsJVMMgr* jvmMgr = JVM_GetJVMMgr();
    if (jvmMgr) {
        nsJVMStatus status = jvmMgr->GetJVMStatus();
        if (status == nsJVMStatus_Enabled)
            status = jvmMgr->StartupJVM();
        if (status == nsJVMStatus_Running) {
            jvm = jvmMgr->GetJVMPlugin();
        }
        jvmMgr->Release();
    }
    return jvm;
}

PR_IMPLEMENT(nsJVMStatus)
JVM_GetJVMStatus(void)
{
    nsJVMStatus status = nsJVMStatus_Disabled;
    nsJVMMgr* mgr = JVM_GetJVMMgr();
    if (mgr) {
        status = mgr->GetJVMStatus();
        mgr->Release();
    }
    return status;
}

PR_IMPLEMENT(PRBool)
JVM_AddToClassPath(const char* dirPath)
{
    nsresult err = NS_ERROR_FAILURE;
    nsJVMMgr* mgr = JVM_GetJVMMgr();
    if (mgr) {
        err = mgr->AddToClassPath(dirPath);
        mgr->Release();
    }
    return err == NS_OK;
}

// This will get the JVMConsole if one is available. You have to Release it 
// when you're done with it.
static nsIJVMConsole*
GetConsole(void)
{
    nsIJVMConsole* console = NULL;
    nsIJVMPlugin* jvm = GetRunningJVM();
    if (jvm) {
        jvm->QueryInterface(kIJVMConsoleIID, (void**)&console);
        // jvm->Release(); // GetRunningJVM no longer calls AddRef
    }
    return console;
}

PR_IMPLEMENT(void)
JVM_ShowConsole(void)
{
    nsIJVMConsole* console = GetConsole();
    if (console) {
        console->ShowConsole();
        console->Release();
    }
}

PR_IMPLEMENT(void)
JVM_HideConsole(void)
{
    nsJVMStatus status = JVM_GetJVMStatus();
    if (status == nsJVMStatus_Running) {
        nsIJVMConsole* console = GetConsole();
        if (console) {
            console->HideConsole();
            console->Release();
        }
    }
}

PR_IMPLEMENT(PRBool)
JVM_IsConsoleVisible(void)
{
    PRBool result = PR_FALSE;
    nsJVMStatus status = JVM_GetJVMStatus();
    if (status == nsJVMStatus_Running) {
        nsIJVMConsole* console = GetConsole();
        if (console) {
            nsresult err = console->IsConsoleVisible(&result);
            PR_ASSERT(err != NS_OK ? result == PR_FALSE : PR_TRUE);
            console->Release();
        }
    }
    return result;
}

PR_IMPLEMENT(void)
JVM_ToggleConsole(void)
{
    nsIJVMConsole* console = GetConsole();
    if (console) {
        PRBool visible = PR_FALSE;
        nsresult err = console->IsConsoleVisible(&visible);
        PR_ASSERT(err != NS_OK ? visible == PR_FALSE : PR_TRUE);
        if (visible)
            console->HideConsole();
        else
            console->ShowConsole();
        console->Release();
    }
}

PR_IMPLEMENT(void)
JVM_PrintToConsole(const char* msg)
{
    nsJVMStatus status = JVM_GetJVMStatus();
    if (status != nsJVMStatus_Running)
        return;
    nsIJVMConsole* console = GetConsole();
    if (console) {
        console->Print(msg);
        console->Release();
    }
}

PR_IMPLEMENT(void)
JVM_StartDebugger(void)
{
    nsIJVMPlugin* jvm = GetRunningJVM();
    if (jvm) {
        nsISymantecDebugger* debugger;
        if (jvm->QueryInterface(kISymantecDebuggerIID, (void**)&debugger) == NS_OK) {
            // XXX should we make sure the vm is started first?
            debugger->StartDebugger(nsSymantecDebugPort_SharedMemory);
            debugger->Release();
        }
        // jvm->Release(); // GetRunningJVM no longer calls AddRef
    }
}

static void PR_CALLBACK detach_ProxyJNI(void* env)
{
	JNIEnv* proxyEnv = (JNIEnv*)env;
	if (proxyEnv != NULL)
		DeleteProxyJNI(proxyEnv);
}

PR_IMPLEMENT(JNIEnv*)
JVM_GetJNIEnv(void)
{
	/* Use NSPR thread private data to manage the per-thread JNIEnv* association. */
	static ThreadLocalStorage<JNIEnv*> localEnv(&detach_ProxyJNI);

    JNIEnv* env = localEnv.get();
	if (env != NULL)
		return env;

#if 0
    nsIJVMPlugin* jvm = GetRunningJVM();
    if (jvm) {
        (void)jvm->GetJNIEnv(&env);
        // jvm->Release(); // GetRunningJVM no longer calls AddRef
    }
#else
	// Create a Proxy JNI to associate with this NSPR thread.
    nsIJVMPlugin* jvmPlugin = GetRunningJVM();
	if (jvmPlugin != NULL)
		env = CreateProxyJNI(jvmPlugin);
#endif

	/* Associate the JNIEnv with the current thread. */
	localEnv.set(env);

    return env;
}

PR_IMPLEMENT(void)
JVM_ReleaseJNIEnv(JNIEnv* env)
{
// this is now done when the NSPR thread is shutdown.
#if 0
    nsIJVMPlugin* jvm = GetRunningJVM();
    if (jvm) {
        (void)jvm->ReleaseJNIEnv(env);
        // jvm->Release(); // GetRunningJVM no longer calls AddRef
    }
#endif
}

PR_IMPLEMENT(nsresult)
JVM_SpendTime(PRUint32 timeMillis)
{
#ifdef XP_MAC
	nsresult result = NS_ERROR_NOT_INITIALIZED;
    nsIJVMPlugin* jvm = GetRunningJVM();
    if (jvm != NULL)
		result = jvm->SpendTime(timeMillis);
	return result;
#else
	return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

PR_IMPLEMENT(PRBool)
JVM_MaybeStartupLiveConnect()
{
    PRBool result = PR_FALSE;
    nsJVMMgr* mgr = JVM_GetJVMMgr();
    if (mgr) {
        result = mgr->MaybeStartupLiveConnect();
        mgr->Release();
    }
    return result;
}


PR_IMPLEMENT(PRBool)
JVM_MaybeShutdownLiveConnect(void)
{
    PRBool result = PR_FALSE;
    nsJVMMgr* mgr = JVM_GetJVMMgr();
    if (mgr) {
        result = mgr->MaybeShutdownLiveConnect();
        mgr->Release();
    }
    return result;
}

PR_IMPLEMENT(PRBool)
JVM_IsLiveConnectEnabled(void)
{
    PRBool result = PR_FALSE;
    nsJVMMgr* mgr = JVM_GetJVMMgr();
    if (mgr) {
        result = mgr->IsLiveConnectEnabled();
        mgr->Release();
    }
    return result;
}

PR_IMPLEMENT(nsJVMStatus)
JVM_ShutdownJVM(void)
{
    nsJVMStatus status = nsJVMStatus_Failed;
    nsJVMMgr* mgr = JVM_GetJVMMgr();
    if (mgr) {
        status = mgr->ShutdownJVM();
        mgr->Release();
    }
    return status;
}

ThreadLocalStorageAtIndex1 *
findPrevNode(JSStackFrame  *pCurrentFrame)
{
    ThreadLocalStorageAtIndex1 *pSecInfoTop = NULL;
    pSecInfoTop = (ThreadLocalStorageAtIndex1 *)PR_GetThreadPrivate(tlsIndex1_g);
    if (pSecInfoTop == NULL)
    {
       return NULL;
    }

    ThreadLocalStorageAtIndex1 *pSecInfoTail = pSecInfoTop->prev;
    if (pCurrentFrame == NULL)
    {
       return pSecInfoTail;
    }
    if ( pSecInfoTop->pJavaToJSSFrame == pCurrentFrame )
    {
       return NULL;
    }
    ThreadLocalStorageAtIndex1 *pTempSecNode = pSecInfoTail;

    while( pTempSecNode->pJavaToJSSFrame != pCurrentFrame )
    {
       pTempSecNode = pTempSecNode->prev;
       if ( pTempSecNode == pSecInfoTail )
       {
          break;
       }
    }
    if( pTempSecNode->pJavaToJSSFrame == pCurrentFrame )
    {
      return pTempSecNode;
    }
    return NULL;
}

PR_IMPLEMENT(PRBool)
JVM_NSISecurityContextImplies(JSStackFrame  *pCurrentFrame, const char* target, const char* action)
{
    ThreadLocalStorageAtIndex1 *pSecInfo = findPrevNode(pCurrentFrame);

    if (pSecInfo == NULL)
    {
       return PR_FALSE;
    }

    nsISecurityContext *pNSISecurityContext = (nsISecurityContext *)pSecInfo->pNSISecurityContext;
    PRBool bAllowedAccess = PR_FALSE;
    if (pNSISecurityContext != NULL)
    {
       pNSISecurityContext->Implies(target, action, &bAllowedAccess);
    }
    return bAllowedAccess;
}

PR_IMPLEMENT(JSPrincipals*)
JVM_GetJavaPrincipalsFromStack(JSStackFrame  *pCurrentFrame)
{
    ThreadLocalStorageAtIndex1 *pSecInfo = findPrevNode(pCurrentFrame);

    if (pSecInfo == NULL)
    {
       return NULL;
    }

    JSPrincipals  *principals = NULL;
    JSContext *pJSCX = (JSContext *)PR_GetThreadPrivate(tlsIndex3_g);
    if (pJSCX == NULL)
    {
       pJSCX = LM_GetCrippledContext();
    }

    principals = get_JSPrincipals_from_java_caller_impl(NULL, pJSCX, pSecInfo->pNSIPrincipaArray, 
                                   pSecInfo->numPrincipals, pSecInfo->pNSISecurityContext);
    return principals;
}

PR_IMPLEMENT(JSStackFrame*)
JVM_GetEndJSFrameFromParallelStack(JSStackFrame  *pCurrentFrame)
{
    ThreadLocalStorageAtIndex1 *pSecInfo = findPrevNode(pCurrentFrame);

    if (pSecInfo == NULL)
    {
       return NULL;
    }
    return pSecInfo->pJavaToJSSFrame;
}

PR_IMPLEMENT(JSStackFrame*)
JVM_GetStartJSFrameFromParallelStack()
{
    JSStackFrame *pJSStartFrame = (JSStackFrame *)PR_GetThreadPrivate(tlsIndex2_g);
    return pJSStartFrame;
}

PR_IMPLEMENT(nsISecurityContext*) 
JVM_GetJSSecurityContext()
{
   nsCSecurityContext *pNSCSecurityContext = new nsCSecurityContext();
   nsISecurityContext *pNSISecurityContext = NULL;
   nsresult err = NS_OK;
   err = pNSCSecurityContext->QueryInterface(kISecurityContextIID,
                                             (void**)&pNSISecurityContext);
   if(err != NS_OK)
   {
     return NULL; 
   }
   return pNSISecurityContext;
}





PR_END_EXTERN_C

////////////////////////////////////////////////////////////////////////////////

