/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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
		context->jsj_env = NULL;
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

    // FIXME:  how do we ever break the association between the jsj_env and the
    // JVMContext? This needs to be figured out. Otherwise, we'll end up with the
    // same dangling JSContext problem we are trying to weed out.

	JVMContext* context = GetJVMContext();
	JSJavaThreadState* jsj_env = context->jsj_env;
	if (jsj_env != NULL)
		return jsj_env;

	JSJavaVM* js_jvm = NULL;
	nsresult rv;
	nsCOMPtr<nsIJVMManager> managerService = do_GetService(kJVMManagerCID, &rv);
	if (NS_FAILED(rv)) return NULL;
	nsJVMManager* pJVMMgr = (nsJVMManager*) managerService.get();  
	if (pJVMMgr != NULL) {
		js_jvm = pJVMMgr->GetJSJavaVM();
		if (js_jvm == NULL) {
			*errp = strdup("Failed to attach to a Java VM.");
			return NULL;
		}
	}

	jsj_env = JSJ_AttachCurrentThreadToJava(js_jvm, NULL, NULL);
	context->jsj_env = jsj_env;

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

JS_STATIC_DLL_CALLBACK(JSPrincipals*)
get_JSPrincipals_from_java_caller_impl(JNIEnv *pJNIEnv, JSContext *pJSContext, void  **ppNSIPrincipalArrayIN, int numPrincipals, void *pNSISecurityContext)
{
    nsresult rv;
    nsCOMPtr<nsIScriptSecurityManager> secMan = 
        do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID, &rv);
    if (NS_FAILED(rv))
        return NULL;

    nsCOMPtr<nsIPrincipal> principal;
    rv = secMan->GetPrincipalFromContext(pJSContext,
                                         getter_AddRefs(principal));
    if (NS_FAILED(rv))
        return NULL;

    JSPrincipals* jsprincipals = NULL;
    principal->GetJSPrincipals(pJSContext, &jsprincipals);
    return jsprincipals;
}

JS_STATIC_DLL_CALLBACK(jobject)
get_java_wrapper_impl(JNIEnv *pJNIEnv, lcjsobject a_jsobject)
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

JS_STATIC_DLL_CALLBACK(lcjsobject)
unwrap_java_wrapper_impl(JNIEnv *pJNIEnv, jobject java_wrapper)
{
    lcjsobject obj = 0;
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
	return PR_TRUE;
}

JS_STATIC_DLL_CALLBACK(void)
exit_js_impl(JNIEnv *jEnv, JSContext *cx)
{
    // The main idea is to execute terminate function if have any;
    if (cx)
    {
        nsIScriptContext *scriptContext = GetScriptContextFromJSContext(cx);

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

