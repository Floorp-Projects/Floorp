/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

#include "prthread.h"
#include "nsJVMManager.h"
#include "nsIPluginInstancePeer2.h"
#include "ProxyJNI.h"
#include "lcglue.h"
#include "nscore.h"
#include "nsIScriptContext.h"
#include "nsISecurityContext.h"
#include "nsCSecurityContext.h"
#include "nsCRT.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIServiceManager.h"
#include "nsIScriptSecurityManager.h"
#include "nsNetUtil.h"

static NS_DEFINE_CID(kJVMManagerCID, NS_JVMMANAGER_CID);

extern "C" int XP_PROGRESS_STARTING_JAVA;
extern "C" int XP_PROGRESS_STARTING_JAVA_DONE;
extern "C" int XP_JAVA_NO_CLASSES;
extern "C" int XP_JAVA_GENERAL_FAILURE;
extern "C" int XP_JAVA_STARTUP_FAILED;
extern "C" int XP_JAVA_DEBUGGER_FAILED;

/**
 * Template based Thread Local Storage.
 */
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


static void PR_CALLBACK detach_JVMContext(void* storage)
{
	JVMContext* context = NS_REINTERPRET_CAST(JVMContext*, storage);
	
	JNIEnv* proxyEnv = context->proxyEnv;
	if (proxyEnv != NULL) {
		DeleteProxyJNI(proxyEnv);
		context->proxyEnv = NULL;
	}
	
	delete context;
}

JVMContext* GetJVMContext()
{
	/* Use NSPR thread private data to manage the per-thread JNIEnv* association. */
	static ThreadLocalStorage<JVMContext*> localContext((PRThreadPrivateDTOR)&detach_JVMContext);
	JVMContext* context = localContext.get();
	if (context == NULL) {
		context = new JVMContext;
		context->proxyEnv = NULL;
		context->securityStack = NULL;
		context->jsj_env = NULL;
		context->js_context = NULL;
		context->js_startframe = NULL;
		context->java_applet_obj = NULL;
		localContext.set(context);
	}
	return context;
}

////////////////////////////////////////////////////////////////////////////////
// LiveConnect callbacks
////////////////////////////////////////////////////////////////////////////////

JS_BEGIN_EXTERN_C

#include "jscntxt.h"

JS_STATIC_DLL_CALLBACK(JSContext*)
map_jsj_thread_to_js_context_impl(JSJavaThreadState *jsj_env, void* java_applet_obj, JNIEnv *env, char **errp)
{
#if 0
	JVMContext* context = GetJVMContext();
	JSContext *cx = context->js_context;

    /*
    ** This callback is called for spontaneous calls only. Either create a new JSContext
    ** or return the crippled context.
    ** TODO: Get to some kind of script manager via service manager and then get to script context 
    **       and then to get to the native context.
    */
    //JSContext *cx    = LM_GetCrippledContext();
    //JSContext *cx    = NULL;

    *errp = NULL;
    return cx;
#else
	// Guess what? This design is totally invalid under Gecko, because there isn't a 1 to 1 mapping
	// between NSPR threads and JSContexts. We have to ask the plugin instance peer what JSContext
	// it lives in to make any sense of all this.
	JSContext* context = NULL;
	if (java_applet_obj != NULL) {
		nsIPluginInstance* pluginInstance = NS_REINTERPRET_CAST(nsIPluginInstance*, java_applet_obj);
	        nsIPluginInstancePeer* pluginPeer = NULL;
		if (pluginInstance->GetPeer(&pluginPeer) == NS_OK) {
			nsIPluginInstancePeer2* pluginPeer2 = NULL;
			if (pluginPeer->QueryInterface(NS_GET_IID(nsIPluginInstancePeer2), (void**) &pluginPeer2) == NS_OK) {
				pluginPeer2->GetJSContext(&context);
				NS_RELEASE(pluginPeer2);
			}
			NS_RELEASE(pluginPeer);
		}
	}
	return context;
#endif
}

JS_STATIC_DLL_CALLBACK(void) detach_jsjava_thread_state(void* env)
{
    JSJavaThreadState *jsj_env = NS_REINTERPRET_CAST(JSJavaThreadState*, env);
    JSJ_DetachCurrentThreadFromJava(jsj_env);
}

/*
** This callback is called to map a JSContext to a JSJavaThreadState which
** is a wrapper around JNIEnv. Hence this callback essentially maps a JSContext
** to a java thread. JSJ_AttachCurrentThreadToJava just calls AttachCurrentThread
** on the java vm.
*/
JS_STATIC_DLL_CALLBACK(JSJavaThreadState*)
map_js_context_to_jsj_thread_impl(JSContext *cx, char **errp)
{
	*errp = NULL;

	JVMContext* context = GetJVMContext();
	JSJavaThreadState* jsj_env = context->jsj_env;
	if (jsj_env != NULL)
		return jsj_env;
 /*/TODO: Figure out if moja intiailzation went ok.   
	if (ET_InitMoja(0) != LM_MOJA_OK) {
		*errp = strdup("LiveConnect initialization failed.");
		return NULL;
	}
*/
	JSJavaVM* js_jvm = NULL;
	nsresult rv;
	nsCOMPtr<nsIJVMManager> managerService = do_GetService(kJVMManagerCID, &rv);
	if (NS_FAILED(rv)) return NULL;
	nsJVMManager* pJVMMgr = (nsJVMManager *)managerService.get();  
	if (pJVMMgr != NULL) {
		js_jvm = pJVMMgr->GetJSJavaVM();
		if (js_jvm == NULL) {
			*errp = strdup("Failed to attach to a Java VM.");
			return NULL;
		}
	}

	jsj_env = JSJ_AttachCurrentThreadToJava(js_jvm, NULL, NULL);
	context->jsj_env = jsj_env;
	context->js_context = cx;

	return jsj_env;
}

/*
** This callback is called in JSObject.getWindow implementation to get

** a java wrapper JSObject class for a applet only once.
** Note that once a mapping between applet -> javascript JSObject -> Java wrapper JSObject 
** is made, all subsequent method calls via JSObject use the internal field
** to get to the javascript JSObject.
*/

JS_STATIC_DLL_CALLBACK(JSObject*)
map_java_object_to_js_object_impl(JNIEnv *env, void *pluginInstancePtr, char* *errp)
{
	JSObject        *window = NULL;
	PRBool           mayscript = PR_FALSE;
	PRBool           jvmMochaPrefsEnabled = PR_TRUE;
	nsresult         err = NS_OK;

	*errp = NULL;

	if (pluginInstancePtr == NULL) {
		env->ThrowNew(env->FindClass("java/lang/NullPointerException"), "plugin instance is NULL");
		return NULL;
	}

	//TODO: Check if Mocha is enabled. To get to any mocha api, we should use service 
	//      manager and get to the appropriate service.
	// jvmMochaPrefsEnabled = LM_GetMochaEnabled();
	if (!jvmMochaPrefsEnabled) {
		*errp = strdup("JSObject.getWindow() failed: java preference is disabled");
		return NULL;
	}

	/*
	 * Check for the mayscript tag.
	 */
	nsIPluginInstance* pluginInstance = NS_REINTERPRET_CAST(nsIPluginInstance*, pluginInstancePtr);
	nsIPluginInstancePeer* pluginPeer;
	if (pluginInstance->GetPeer(&pluginPeer) == NS_OK) {
		nsIJVMPluginTagInfo* tagInfo;
		if (pluginPeer->QueryInterface(NS_GET_IID(nsIJVMPluginTagInfo), (void**) &tagInfo) == NS_OK) {
			err = tagInfo->GetMayScript(&mayscript);
			// PR_ASSERT(err != NS_OK ? mayscript == PR_FALSE : PR_TRUE);
			NS_RELEASE(tagInfo);
		}
		if ( !mayscript ) {
			*errp = strdup("JSObject.getWindow() requires mayscript attribute on this Applet");
		} else {
			nsIPluginInstancePeer2* pluginPeer2 = nsnull;
			if (pluginPeer->QueryInterface(NS_GET_IID(nsIPluginInstancePeer2),
			                              (void**) &pluginPeer2) == NS_OK) {
				err = pluginPeer2->GetJSWindow(&window);
				NS_RELEASE(pluginPeer2);
			}
		}
		NS_RELEASE(pluginPeer);
	}

	//TODO: Get to the window object using DOM.
	// window = getDOMWindow().getScriptOwner().getJSObject().
	return window;
}


#if 0 
// This needs to be modified at least temporarily.  Caps is undergoing some rearchitecture
// nsPrincipal doesn't exist anymore, we're trying to move towards using nsIPrincipal and a PrincipalTools.h
// which includes the definitions for arrays.
// TODO: Need raman's help. This needs to convert between C++ [] array data type to a nsVector object.
void* 
ConvertNSIPrincipalArrayToObject(JNIEnv *pJNIEnv, JSContext *pJSContext, void  **ppNSIPrincipalArrayIN, int numPrincipals, void *pNSISecurityContext)
{
    nsIPrincipal  **ppNSIPrincipalArray = NS_REINTERPRET_CAST(nsIPrincipal**, ppNSIPrincipalArrayIN);
    PRInt32        length = numPrincipals;
    nsresult       err    = NS_OK;
    nsCOMPtr<nsIJVMManager> managerService = do_GetService(kJVMManagerCID, &err);
    if (NS_FAILED(err)) return NULL;
    nsJVMManager* pJVMMgr = (nsJVMManager *)managerService.get();  
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
    }
    if (  (pNSPrincipalArray != NULL)
        &&(length != 0)
       )
    {
        return pNSPrincipalArray;
    }

    return NULL;
}
#endif //0


JS_STATIC_DLL_CALLBACK(JSPrincipals*)
get_JSPrincipals_from_java_caller_impl(JNIEnv *pJNIEnv, JSContext *pJSContext, void  **ppNSIPrincipalArrayIN, int numPrincipals, void *pNSISecurityContext)
{
    nsISupports* credentials = NS_REINTERPRET_CAST(nsISupports*, pNSISecurityContext);
    nsCOMPtr<nsISecurityContext> securityContext = do_QueryInterface(credentials);
    if (securityContext) {
        nsresult rv;
        nsCOMPtr<nsIScriptSecurityManager> ssm = do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
        if (NS_SUCCEEDED(rv)) {
            char codebase[512];
            rv = securityContext->GetOrigin(codebase, sizeof(codebase) - 1);
            if (NS_SUCCEEDED(rv)) {
                nsCOMPtr<nsIURI> codebaseURI;
                rv = NS_NewURI(getter_AddRefs(codebaseURI), codebase);
                if (NS_SUCCEEDED(rv)) {
                    nsCOMPtr<nsIPrincipal> principal;
                    rv = ssm->GetCodebasePrincipal(codebaseURI, getter_AddRefs(principal));
                    if (NS_SUCCEEDED(rv)) {
                        JSPrincipals* jsprincipals;
                        principal->GetJSPrincipals(&jsprincipals);
                        return jsprincipals;
                    }
                }
            }
        }
    } else {
        nsCOMPtr<nsIPrincipal> principal = do_QueryInterface(credentials);
        if (principal) {
            JSPrincipals* jsprincipals;
            principal->GetJSPrincipals(&jsprincipals);
            return jsprincipals;
        }
    }
    return NULL;
}

JS_STATIC_DLL_CALLBACK(jobject)
get_java_wrapper_impl(JNIEnv *pJNIEnv, jint a_jsobject)
{
    nsresult       err    = NS_OK;
    jobject  pJSObjectWrapper = NULL;
    nsCOMPtr<nsIJVMManager> managerService = do_GetService(kJVMManagerCID, &err);
    if (NS_FAILED(err)) return NULL;
    nsJVMManager* pJVMMgr = (nsJVMManager *)managerService.get();  
    if (pJVMMgr != NULL) {
      nsIJVMPlugin* pJVMPI = pJVMMgr->GetJVMPlugin();
      if (pJVMPI != NULL) {
         err = pJVMPI->GetJavaWrapper(pJNIEnv, a_jsobject, &pJSObjectWrapper);
      }
    }
    if ( err != NS_OK )
    {
       return NULL;
    }
    return pJSObjectWrapper;
}

JS_STATIC_DLL_CALLBACK(jint)
unwrap_java_wrapper_impl(JNIEnv *pJNIEnv, jobject java_wrapper)
{
    jint obj = 0;
    nsresult       err    = NS_OK;
    nsCOMPtr<nsIJVMManager> managerService = do_GetService(kJVMManagerCID, &err);
    if (NS_FAILED(err)) return 0;
    nsJVMManager* pJVMMgr = (nsJVMManager *)managerService.get();  
    if (pJVMMgr != NULL) {
      nsIJVMPlugin* pJVMPI = pJVMMgr->GetJVMPlugin();
      if (pJVMPI != NULL) {
         err = pJVMPI->UnwrapJavaWrapper(pJNIEnv, java_wrapper, &obj);
      }
    }
    if ( err != NS_OK )
    {
       return 0;
    }
    return obj;
}

JS_STATIC_DLL_CALLBACK(JSBool)
enter_js_from_java_impl(JNIEnv *jEnv, char **errp,
                        void **pNSIPrincipaArray, int numPrincipals, 
                        void *pNSISecurityContext,
                        void *java_applet_obj)
{
    JSContext *pJSCX    = map_jsj_thread_to_js_context_impl(nsnull,java_applet_obj,jEnv,errp);
    nsCOMPtr<nsIPrincipal> principal;

    nsISupports* credentials = NS_REINTERPRET_CAST(nsISupports*, pNSISecurityContext);
    nsCOMPtr<nsISecurityContext> javaSecurityContext = do_QueryInterface(credentials);
    if (javaSecurityContext) {
        if (pJSCX) {
            nsCOMPtr<nsIScriptContext> scriptContext = NS_REINTERPRET_CAST(nsIScriptContext*, JS_GetContextPrivate(pJSCX));
            if (scriptContext) {
                nsCOMPtr<nsIScriptGlobalObject> global;
                scriptContext->GetGlobalObject(getter_AddRefs(global));
                NS_ASSERTION(global, "script context has no global object");

                if (global) {
                    nsCOMPtr<nsIScriptObjectPrincipal> globalData = do_QueryInterface(global);
                    if (globalData) {
                        if (NS_FAILED(globalData->GetPrincipal(getter_AddRefs(principal))))
                            return NS_ERROR_FAILURE;
                    }
                }
            }
        }

        // What if !pJSCX? 

        nsCOMPtr<nsISecurityContext> jsSecurityContext = new nsCSecurityContext(principal);
        if (!jsSecurityContext)
            return PR_FALSE;

        // Check that the origin + certificate are the same.
        // If not, then return false.

        const int buflen = 512;
        char jsorigin[buflen];
        char jvorigin[buflen];
        *jsorigin = nsnull;
        *jvorigin = nsnull;
        
        jsSecurityContext->GetOrigin(jsorigin,buflen);
        javaSecurityContext->GetOrigin(jvorigin,buflen);

        if (nsCRT::strcasecmp(jsorigin,jvorigin)) {
            return PR_FALSE;
        }

#if 0
      // ISSUE: Needs security review. We don't compare certificates. 
      // because currently there is no basis for making a postive comparision.
      // If one or the other context is signed, the comparision will fail.

        char jscertid[buflen];
        char jvcertid[buflen];
        *jscertid = nsnull;
        *jvcertid = nsnull;

        jsSecurityContext->GetCertificateID(jscertid,buflen);
        javaSecurityContext->GetCertificateID(jvcertid,buflen);

        if (nsCRT::strcasecmp(jscertid,jvcertid)) {
            return PR_FALSE;
        }
#endif

    }

	return PR_TRUE;
}

JS_STATIC_DLL_CALLBACK(void)
exit_js_impl(JNIEnv *jEnv, JSContext *cx)
{
    //TODO:  
    //LM_UnlockJS();

    // Pop the security context stack
    JVMContext* context = GetJVMContext();
    JVMSecurityStack *pSecInfoBottom = context->securityStack;
    if (pSecInfoBottom != NULL)
    {
      if(pSecInfoBottom->next == pSecInfoBottom)
      {
        context->securityStack = NULL;
        pSecInfoBottom->next   = NULL;            
        pSecInfoBottom->prev   = NULL;            
        delete pSecInfoBottom;
      }
      else
      {
        JVMSecurityStack *top = pSecInfoBottom->prev;
        top->next        = NULL;            
        pSecInfoBottom->prev = top->prev;        
        top->prev->next  = pSecInfoBottom;
        top->prev        = NULL;
        delete top;
      }
    }
    // The main idea is to execute terminate function if have any;
    if (cx)
    {
        nsISupports* supports = NS_REINTERPRET_CAST(nsIScriptContext*, 
                                                    JS_GetContextPrivate(cx));
        nsCOMPtr<nsIScriptContext> scriptContext = do_QueryInterface(supports);

        if (scriptContext)
        {
            scriptContext->ScriptEvaluated(PR_TRUE);
        }
    }
    return;
}

JS_STATIC_DLL_CALLBACK(PRBool)
create_java_vm_impl(SystemJavaVM* *jvm, JNIEnv* *initialEnv, void* initargs)
{
    // const char* classpath = (const char*)initargs;
     nsresult rv;
     nsCOMPtr<nsIJVMManager> managerService = do_GetService(kJVMManagerCID, &rv);
     if (NS_FAILED(rv)) return PR_FALSE;
    *jvm = NS_REINTERPRET_CAST(SystemJavaVM*, managerService.get());  // unused in the browse
    *initialEnv = JVM_GetJNIEnv();
    return (*jvm != NULL && *initialEnv != NULL);
}

JS_STATIC_DLL_CALLBACK(PRBool)
destroy_java_vm_impl(SystemJavaVM* jvm, JNIEnv* initialEnv)
{
    JVM_ReleaseJNIEnv(initialEnv);
    // need to release jvm
    return PR_TRUE;
}

JS_STATIC_DLL_CALLBACK(JNIEnv*)
attach_current_thread_impl(SystemJavaVM* jvm)
{
    return JVM_GetJNIEnv();
}

JS_STATIC_DLL_CALLBACK(PRBool)
detach_current_thread_impl(SystemJavaVM* jvm, JNIEnv* env)
{
    JVM_ReleaseJNIEnv(env);
    return PR_TRUE;
}

JS_STATIC_DLL_CALLBACK(SystemJavaVM*)
get_java_vm_impl(JNIEnv* env)
{
    // only one SystemJavaVM for the whole browser, so it doesn't depend on env
    nsresult rv;
    nsCOMPtr<nsIJVMManager> managerService = do_GetService(kJVMManagerCID, &rv);
    if (NS_FAILED(rv)) return NULL;
    SystemJavaVM* jvm = NS_REINTERPRET_CAST(SystemJavaVM*, managerService.get());  
    return jvm;
}

JS_END_EXTERN_C

static JSJCallbacks jsj_callbacks = {
    map_jsj_thread_to_js_context_impl,
    map_js_context_to_jsj_thread_impl,
    map_java_object_to_js_object_impl,
    get_JSPrincipals_from_java_caller_impl,
    enter_js_from_java_impl,
    exit_js_impl,
    NULL,       // error_print
    get_java_wrapper_impl,
    unwrap_java_wrapper_impl,
    create_java_vm_impl,
    destroy_java_vm_impl,
    attach_current_thread_impl,
    detach_current_thread_impl,
    get_java_vm_impl
};

void
JVM_InitLCGlue(void)
{
    JSJ_Init(&jsj_callbacks);
}

////////////////////////////////////////////////////////////////////////////////

/*
TODO:Tom Pixley.
APIs required from Tom Pixley.
o LM_LockJS(errp);         Grab the mocha lock before doing any liveconect stuff. 
                           This is because layers above JS engine including liveconnect
                           DLL itself are not thread safe.
o LM_UnlockJS()
o LM_GetMochaEnabled()     Check to see if Mocha is enabled.
o LM_GetCrippledContext(). Get to a pre-created crippled context. All spontaneous
                           Java calls map into one crippled context.
o ET_InitMoja(0) != LM_MOJA_OK: This tells if moja initialization went ok.
o LM_GetJSPrincipalsFromJavaCaller : Wrap a nsIPrincipal array object to get back a JSPrincipals data struct.
o LM_CanAccessTargetStr    This code is used to figure out if access is allowed. It is used during security
                           stack walking. The tricky thing is that we need to set the start frame into
                           TLS before calling this code.
                           Look into nsCSecurityContext::Implies
*/

