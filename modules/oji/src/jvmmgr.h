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

#ifndef jvmmgr_h___
#define jvmmgr_h___

#include "xp_core.h"    /* include first because of Bool problem */
#include "prtypes.h"
#include "nsCom.h"
#include "nsError.h"	/* for nsresult */
#include "jni.h"
#include "jsdbgapi.h"

struct nsJVMMgr;

typedef enum nsJVMStatus {
    nsJVMStatus_Enabled,  /* but not Running */
    nsJVMStatus_Disabled, /* explicitly disabled */
    nsJVMStatus_Running,  /* enabled and started */
    nsJVMStatus_Failed    /* enabled but failed to start */
} nsJVMStatus;

#ifdef __cplusplus

#include "nsjvm.h"
#include "nsAgg.h"
#include "jsjava.h"
#include "nsVector.h"
#include "nsIThreadManager.h"
#include "nsISecurityContext.h"

class nsIPluginTagInfo2;
class nsSymantecDebugManager;

struct ThreadLocalStorageAtIndex0 {
 PRUint32 refcount;
};
typedef struct ThreadLocalStorageAtIndex0 ThreadLocalStorageAtIndex0;
typedef struct ThreadLocalStorageAtIndex1 ThreadLocalStorageAtIndex1;
struct ThreadLocalStorageAtIndex1 {
  void        **pNSIPrincipaArray;
  int           numPrincipals;
  void         *pNSISecurityContext;
  JSStackFrame *pJavaToJSSFrame;
  ThreadLocalStorageAtIndex1 *next;
  ThreadLocalStorageAtIndex1 *prev;
};

/*******************************************************************************
 * JVMMgr is the interface to the JVM manager that the browser sees. All
 * files that want to include java services should include this header file.
 * nsIJVMManager is the more limited interface what the JVM plugin sees.
 ******************************************************************************/

struct nsJVMMgr : public nsIJVMManager, public nsIThreadManager {
public:

    NS_DECL_AGGREGATED
    
    /* from nsIJVMManager: */
    
    /**
     * Creates a proxy JNI for a given secure environment.
     */
	NS_IMETHOD
	CreateProxyJNI(nsISecureJNI2* inSecureEnv, JNIEnv** outProxyEnv);
    
    /* from nsIThreadManager: */
    
	/**
	 * Returns a unique identifier for the "current" system thread.
	 */
	NS_IMETHOD
	GetCurrentThread(PRUint32* threadID);

	/**
	 * Pauses the current thread for the specified number of milliseconds.
	 * If milli is zero, then merely yields the CPU if another thread of
	 * greater or equal priority.
	 */
	NS_IMETHOD
	Sleep(PRUint32 milli = 0);

	/**
	 * Creates a unique monitor for the specified address, and makes the
	 * current system thread the owner of the monitor.
	 */
	NS_IMETHOD
	EnterMonitor(void* address);
	
	/**
	 * Exits the monitor associated with the address.
	 */
	NS_IMETHOD
	ExitMonitor(void* address);
	
	/**
	 * Waits on the monitor associated with the address (must be entered already).
	 * If milli is 0, wait indefinitely.
	 */
	NS_IMETHOD
	Wait(void* address, PRUint32 milli = 0);

	/**
	 * Notifies a single thread waiting on the monitor associated with the address (must be entered already).
	 */
	NS_IMETHOD
	Notify(void* address);

	/**
	 * Notifies all threads waiting on the monitor associated with the address (must be entered already).
	 */
	NS_IMETHOD
	NotifyAll(void* address);
	
	/**
	 * Thread creation primitives.
	 */
	NS_IMETHOD
	CreateThread(PRUint32* threadID, nsIRunnable* runnable);

    /* JVMMgr specific methods: */

    /* ====> From here on are things only called by the browser, not the plugin... */

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    nsIJVMPlugin* GetJVMPlugin(void) { return fJVM; }

    /* Unlike the nsIJVMPlugin::StartupJVM, this version handles putting
     * up any error dialog: */
    nsJVMStatus StartupJVM(void);
    nsJVMStatus ShutdownJVM(PRBool fullShutdown = PR_FALSE);
    nsJVMStatus GetJVMStatus(void);
    void SetJVMEnabled(PRBool enabled);
    
    void        ReportJVMError(nsresult err);
    const char* GetJavaErrorString(JNIEnv* env);

    nsresult    AddToClassPath(const char* dirPath);
    PRBool      MaybeStartupLiveConnect(void);
    PRBool      MaybeShutdownLiveConnect(void);
    PRBool      IsLiveConnectEnabled(void);
    JSJavaVM*   GetJSJavaVM() { return fJSJavaVM; }


protected:    
    nsJVMMgr(nsISupports* outer);
    virtual ~nsJVMMgr(void);

    void        EnsurePrefCallbackRegistered(void);
    const char* GetJavaErrorString(JRIEnv* env);

    nsIJVMPlugin*       fJVM;
    nsJVMStatus         fStatus;
    PRBool              fRegisteredJavaPrefChanged;
    nsISupports*        fDebugManager;
    JSJavaVM *          fJSJavaVM;  
    nsVector*           fClassPathAdditions;
};

/*******************************************************************************
 * Symantec Debugger Stuff
 ******************************************************************************/

class nsSymantecDebugManager : public nsISymantecDebugManager {
public:

    NS_DECL_AGGREGATED

    NS_IMETHOD
    SetDebugAgentPassword(PRInt32 pwd);

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr,
           nsJVMMgr* jvmMgr);

protected:
    nsSymantecDebugManager(nsISupports* outer, nsJVMMgr* jvmMgr);
    virtual ~nsSymantecDebugManager(void);

    nsJVMMgr*             fJVMMgr;

};

/*******************************************************************************
 * nsJVMPluginTagInfo: The browser makes one of these when it sees an APPLET or
 * appropriate OBJECT tag.
 ******************************************************************************/

class nsJVMPluginTagInfo : public nsIJVMPluginTagInfo {
public:

    NS_DECL_AGGREGATED

    /* from nsIJVMPluginTagInfo: */

    /* ====> These are usually only called by the plugin, not the browser... */

    NS_IMETHOD
    GetCode(const char* *result);

    NS_IMETHOD
    GetCodeBase(const char* *result);

    NS_IMETHOD
    GetArchive(const char* *result);

    NS_IMETHOD
    GetName(const char* *result);

    NS_IMETHOD
    GetMayScript(PRBool *result);

    /* Methods specific to nsJVMPluginInstancePeer: */
    
    /* ====> From here on are things only called by the browser, not the plugin... */

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr,
           nsIPluginTagInfo2* info);

protected:

    nsJVMPluginTagInfo(nsISupports* outer, nsIPluginTagInfo2* info);
    virtual ~nsJVMPluginTagInfo(void);

    /* Instance Variables: */
    nsIPluginTagInfo2*  fPluginTagInfo;
    char*               fSimulatedCodebase;
    char*               fSimulatedCode;
};

#endif /* __cplusplus */

/*******************************************************************************
 * Convenience Routines
 ******************************************************************************/

PR_BEGIN_EXTERN_C

/* Returns the JVM manager. You must do a Release on the pointer returned when
   you're done with it. */
PR_EXTERN(struct nsJVMMgr*)
JVM_GetJVMMgr(void);

PR_EXTERN(nsJVMStatus)
JVM_GetJVMStatus(void);

PR_EXTERN(PRBool)
JVM_AddToClassPath(const char* dirPath);

PR_EXTERN(void)
JVM_ShowConsole(void);

PR_EXTERN(void)
JVM_HideConsole(void);

PR_EXTERN(PRBool)
JVM_IsConsoleVisible(void);

PR_EXTERN(void)
JVM_ToggleConsole(void);

PR_EXTERN(void)
JVM_PrintToConsole(const char* msg);

PR_EXTERN(void)
JVM_StartDebugger(void);

PR_EXTERN(JNIEnv*)
JVM_GetJNIEnv(void);

PR_IMPLEMENT(void)
JVM_ReleaseJNIEnv(JNIEnv *pJNIEnv);

PR_EXTERN(nsresult)
JVM_SpendTime(PRUint32 timeMillis);

PR_EXTERN(PRBool)
JVM_MaybeStartupLiveConnect(void);

PR_EXTERN(PRBool)
JVM_MaybeShutdownLiveConnect(void);

PR_EXTERN(PRBool)
JVM_IsLiveConnectEnabled(void);

PR_EXTERN(nsJVMStatus)
JVM_ShutdownJVM(void);

PR_EXTERN(PRBool)
JVM_NSISecurityContextImplies(JSStackFrame  *pCurrentFrame, const char* target, const char* action);

PR_EXTERN(JSPrincipals*)
JVM_GetJavaPrincipalsFromStack(JSStackFrame  *pCurrentFrame);

PR_EXTERN(JSStackFrame*)
JVM_GetStartJSFrameFromParallelStack();

PR_EXTERN(JSStackFrame*)
JVM_GetEndJSFrameFromParallelStack(JSStackFrame  *pCurrentFrame);

typedef struct nsISecurityContext nsISecurityContext;
PR_EXTERN(nsISecurityContext*) 
JVM_GetJSSecurityContext();

PR_END_EXTERN_C

#endif /* jvmmgr_h___ */
