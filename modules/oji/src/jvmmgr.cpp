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

#include "nsJVMManager.h"
#include "nsIServiceManager.h"
#include "nsIJVMPrefsWindow.h"
#include "ProxyJNI.h"
#include "lcglue.h"
#include "libmocha.h"
#include "nsCSecurityContext.h"
#include "nsISecurityContext.h"

static NS_DEFINE_CID(kJVMManagerCID, NS_JVMMANAGER_CID);
static NS_DEFINE_IID(kIJVMManagerIID, NS_IJVMMANAGER_IID);
static NS_DEFINE_IID(kIJVMConsoleIID, NS_IJVMCONSOLE_IID);
static NS_DEFINE_IID(kIJVMPrefsWindowIID, NS_IJVMPREFSWINDOW_IID);
static NS_DEFINE_IID(kISymantecDebuggerIID, NS_ISYMANTECDEBUGGER_IID);
static NS_DEFINE_IID(kISecurityContextIID, NS_ISECURITYCONTEXT_IID);
extern nsIServiceManager  *theServiceManager;

PR_BEGIN_EXTERN_C

#ifdef PRE_SERVICE_MANAGER
extern nsPluginManager* thePluginManager;
#endif

PR_IMPLEMENT(nsJVMManager*)
JVM_GetJVMMgr(void)
{
#ifdef PRE_SERVICE_MANAGER
	nsresult result = NS_OK;
    if (thePluginManager == NULL) {
        result = nsPluginManager::Create(NULL, kIPluginManagerIID, (void**)&thePluginManager);
		if (result != NS_OK)
			return NULL;
    }
    nsJVMManager* mgr = NULL;
    result = thePluginManager->QueryInterface(kIJVMManagerIID, (void**)&mgr);
    if (result != NS_OK)
        return NULL;
    return mgr;
#else
    nsJVMManager* mgr = NULL;
    nsresult err = theServiceManager->GetService(kJVMManagerCID, kIJVMManagerIID, 
                                                (nsISupports**)&mgr);
    if (err != NS_OK)
        return NULL;
    return mgr;
#endif
}

PR_IMPLEMENT(void)
JVM_ReleaseJVMMgr(nsJVMManager* mgr)
{
    nsresult err = nsServiceManager::ReleaseService(kJVMManagerCID, (nsISupports*)((nsIJVMManager*)mgr));
    PR_ASSERT(err == NS_OK);
}

static nsIJVMPlugin*
GetRunningJVM(void)
{
    nsIJVMPlugin* jvm = NULL;
    nsJVMManager* jvmMgr = JVM_GetJVMMgr();
    if (jvmMgr) {
        nsJVMStatus status = jvmMgr->GetJVMStatus();
        if (status == nsJVMStatus_Enabled)
            status = jvmMgr->StartupJVM();
        if (status == nsJVMStatus_Running) {
            jvm = jvmMgr->GetJVMPlugin();
        }
//        jvmMgr->Release();
    }
    return jvm;
}

PR_IMPLEMENT(nsJVMStatus)
JVM_StartupJVM(void)
{
    nsIJVMPlugin* jvm = GetRunningJVM();
    return JVM_GetJVMStatus();
}

PR_IMPLEMENT(nsJVMStatus)
JVM_ShutdownJVM(void)
{
    nsJVMStatus status = nsJVMStatus_Failed;
    nsJVMManager* mgr = JVM_GetJVMMgr();
    if (mgr) {
        status = mgr->ShutdownJVM();
//        mgr->Release();
    }
    return status;
}


PR_IMPLEMENT(nsJVMStatus)
JVM_GetJVMStatus(void)
{
    nsJVMStatus status = nsJVMStatus_Disabled;
    nsJVMManager* mgr = JVM_GetJVMMgr();
    if (mgr) {
        status = mgr->GetJVMStatus();
//        mgr->Release();
    }
    return status;
}

PR_IMPLEMENT(PRBool)
JVM_AddToClassPath(const char* dirPath)
{
    nsresult err = NS_ERROR_FAILURE;
    nsJVMManager* mgr = JVM_GetJVMMgr();
    if (mgr) {
        err = mgr->AddToClassPath(dirPath);
//        mgr->Release();
    }
    return err == NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

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
        console->Show();
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
            console->Hide();
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
            nsresult err = console->IsVisible(&result);
            PR_ASSERT(err != NS_OK ? result == PR_FALSE : PR_TRUE);
            console->Release();
        }
    }
    return result;
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

////////////////////////////////////////////////////////////////////////////////

// This will get the JVMPrefsWindow if one is available. You have to Release it 
// when you're done with it.
static nsIJVMPrefsWindow*
GetPrefsWindow(void)
{
    nsIJVMPrefsWindow* prefsWin = NULL;
    nsIJVMPlugin* jvm = GetRunningJVM();
    if (jvm) {
        jvm->QueryInterface(kIJVMPrefsWindowIID, (void**)&prefsWin);
        // jvm->Release(); // GetRunningJVM no longer calls AddRef
    }
    return prefsWin;
}

PR_IMPLEMENT(void)
JVM_ShowPrefsWindow(void)
{
    nsIJVMPrefsWindow* prefsWin = GetPrefsWindow();
    if (prefsWin) {
        prefsWin->Show();
        prefsWin->Release();
    }
}

PR_IMPLEMENT(void)
JVM_HidePrefsWindow(void)
{
    nsJVMStatus status = JVM_GetJVMStatus();
    if (status == nsJVMStatus_Running) {
        nsIJVMPrefsWindow* prefsWin = GetPrefsWindow();
        if (prefsWin) {
            prefsWin->Hide();
            prefsWin->Release();
        }
    }
}

PR_IMPLEMENT(PRBool)
JVM_IsPrefsWindowVisible(void)
{
    PRBool result = PR_FALSE;
    nsJVMStatus status = JVM_GetJVMStatus();
    if (status == nsJVMStatus_Running) {
        nsIJVMPrefsWindow* prefsWin = GetPrefsWindow();
        if (prefsWin) {
            nsresult err = prefsWin->IsVisible(&result);
            PR_ASSERT(err != NS_OK ? result == PR_FALSE : PR_TRUE);
            prefsWin->Release();
        }
    }
    return result;
}

////////////////////////////////////////////////////////////////////////////////

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


PR_IMPLEMENT(JNIEnv*)
JVM_GetJNIEnv(void)
{
	/* get proxy env for current thread. */
	JVMContext* context = GetJVMContext();
    JNIEnv* env = context->proxyEnv;
	if (env != NULL)
		return env;

	// Create a Proxy JNI to associate with this NSPR thread.
    nsIJVMPlugin* jvmPlugin = GetRunningJVM();
	if (jvmPlugin != NULL)
		env = CreateProxyJNI(jvmPlugin);

	/* Associate the JNIEnv with the current thread. */
	context->proxyEnv = env;

    return env;
}

PR_IMPLEMENT(void)
JVM_ReleaseJNIEnv(JNIEnv* env)
{
	/**
	 * this is now done when the NSPR thread is shutdown. JNIEnvs are always tied to the
	 * lifetime of threads.
	 */
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
    nsJVMManager* mgr = JVM_GetJVMMgr();
    if (mgr) {
        result = mgr->MaybeStartupLiveConnect();
//        mgr->Release();
    }
    return result;
}


PR_IMPLEMENT(PRBool)
JVM_MaybeShutdownLiveConnect(void)
{
    PRBool result = PR_FALSE;
    nsJVMManager* mgr = JVM_GetJVMMgr();
    if (mgr) {
        result = mgr->MaybeShutdownLiveConnect();
//        mgr->Release();
    }
    return result;
}

PR_IMPLEMENT(PRBool)
JVM_IsLiveConnectEnabled(void)
{
    PRBool result = PR_FALSE;
    nsJVMManager* mgr = JVM_GetJVMMgr();
    if (mgr) {
        result = mgr->IsLiveConnectEnabled();
//        mgr->Release();
    }
    return result;
}


static
JVMSecurityStack *
findPrevNode(JSStackFrame  *pCurrentFrame)
{
	  JVMContext* context = GetJVMContext();
   JVMSecurityStack *pSecInfoBottom = context->securityStack;
   if (pSecInfoBottom == NULL)
   {
      return NULL;
   }

   JVMSecurityStack *pSecInfoTop = pSecInfoBottom->prev;
   if (pCurrentFrame == NULL)
   {
      return pSecInfoTop;
   }
   if ( pSecInfoBottom->pJavaToJSFrame == pCurrentFrame )
   {
      return NULL;
   }
   JVMSecurityStack *pTempSecNode = pSecInfoTop;

   while( pTempSecNode->pJSToJavaFrame != pCurrentFrame )
   {
      pTempSecNode = pTempSecNode->prev;
      if ( pTempSecNode == pSecInfoTop )
      {
         break;
      }
   }
   if( pTempSecNode->pJSToJavaFrame == pCurrentFrame )
   {
     return pTempSecNode;
   }
   return NULL;
}

PR_IMPLEMENT(PRBool)
JVM_NSISecurityContextImplies(JSStackFrame  *pCurrentFrame, const char* target, const char* action)
{
    JVMSecurityStack *pSecInfo = findPrevNode(pCurrentFrame);

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

PR_IMPLEMENT(void *)
JVM_GetJavaPrincipalsFromStackAsNSVector(JSStackFrame  *pCurrentFrame)
{
    JVMSecurityStack *pSecInfo = findPrevNode(pCurrentFrame);

    if (pSecInfo == NULL)
    {
       return NULL;
    }

    JSPrincipals  *principals = NULL;
    JVMContext* context = GetJVMContext();
    JSContext *pJSCX = context->js_context;
    if (pJSCX == NULL)
    {
       //TODO: Get to the new context from DOM.
       //pJSCX = LM_GetCrippledContext();
    }
    /*
    ** TODO: Get raman's help here. I don't know how we are going to give back a nsPrincipals array.
    ** Tom's new code should now use a different signature and accept a nsIPrincipal vector object 
    ** instead in lm_taint.c and then call into caps. Caps needs to change to accommodate this.
    void *pNSPrincipalArray  = ConvertNSIPrincipalToNSPrincipalArray(NULL, pJSCX, pSecInfo->pNSIPrincipaArray, 
                                                                     pSecInfo->numPrincipals, pSecInfo->pNSISecurityContext);
    if (pNSPrincipalArray != NULL)
    {
       return pNSPrincipalArray;
    }
    */
    return NULL;
}


PR_IMPLEMENT(JSPrincipals*)
JVM_GetJavaPrincipalsFromStack(JSStackFrame  *pCurrentFrame)
{
    JVMSecurityStack *pSecInfo = findPrevNode(pCurrentFrame);

    if (pSecInfo == NULL)
    {
       return NULL;
    }

    JSPrincipals  *principals = NULL;
    JVMContext* context = GetJVMContext();
    JSContext *pJSCX = context->js_context;
    if (pJSCX == NULL)
    {
       //TODO: Get to the new context from DOM.
       //pJSCX = LM_GetCrippledContext();
    }
    /* TODO: 
    ** Get raman's help here. We should not need to convert nsIPrincipal to nsPrincipal anymore.
    ** But we should convert from nsIPrincipal array to a nsIPrincipal array object represented as
    ** nsVector. Use this vector to pass into Tom's code to get to the JSPrinciapals
    void *pNSPrincipalArray  = ConvertNSIPrincipalArrayToObject(NULL, pJSCX, pSecInfo->pNSIPrincipaArray, 
                                                                     pSecInfo->numPrincipals, pSecInfo->pNSISecurityContext);
    if (pNSPrincipalArray != NULL)
    {
        return LM_GetJSPrincipalsFromJavaCaller(pJSCX, pNSPrincipalArray, pSecInfo->pNSISecurityContext);
    }
    */
    return NULL;
}

PR_IMPLEMENT(JSStackFrame*)
JVM_GetEndJSFrameFromParallelStack(JSStackFrame  *pCurrentFrame)
{
    JVMSecurityStack *pSecInfo = findPrevNode(pCurrentFrame);

    if (pSecInfo == NULL)
    {
       return NULL;
    }
    return pSecInfo->pJavaToJSFrame;
}

PR_IMPLEMENT(JSStackFrame**)
JVM_GetStartJSFrameFromParallelStack()
{
	JVMContext* context = GetJVMContext();
	return &context->js_startframe;
}

PR_IMPLEMENT(nsISecurityContext*) 
JVM_GetJSSecurityContext()
{
    JVMContext* context = GetJVMContext();
    JVMSecurityStack  *securityStack = context->securityStack; 
    JVMSecurityStack  *securityStackTop = NULL;
    JSContext         *cx = context->js_context;

    if (cx == NULL) {
     //TODO: Get to the new context from DOM.
     //pJSCX = LM_GetCrippledContext();
    }

    if(securityStack != NULL) {
	securityStackTop = securityStack->prev; 
	JSStackFrame *fp = NULL;
	securityStackTop->pJSToJavaFrame = JS_FrameIterator(cx, &fp);
    }

    if (cx != NULL) {
	nsCSecurityContext *securityContext = new nsCSecurityContext(cx);
	securityContext->AddRef();
	return securityContext;
    }
    return NULL;
}

PR_END_EXTERN_C

////////////////////////////////////////////////////////////////////////////////
