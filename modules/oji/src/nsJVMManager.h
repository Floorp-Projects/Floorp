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

#ifndef nsJVMManager_h___
#define nsJVMManager_h___

#include "jvmmgr.h"
#include "xp_core.h"    /* include first because of Bool problem */
#include "prtypes.h"
#include "nsCom.h"
#include "jni.h"
#include "jsdbgapi.h"
#include "nsError.h"


#include "nsjvm.h"
#include "nsAgg.h"
#include "jsjava.h"
#include "nsVector.h"
#include "nsILiveConnectManager.h"

class nsSymantecDebugManager;


/*******************************************************************************
 * NsJVMManager is the interface to the JVM manager that the browser sees. All
 * files that want to include java services should include this header file.
 * nsIJVMManager is the more limited interface what the JVM plugin sees.
 ******************************************************************************/

struct nsJVMManager : public nsIJVMManager, public nsIThreadManager, public nsILiveConnectManager {
public:

    NS_DECL_AGGREGATED
    
    /* from nsIJVMManager: */
    
    /**
     * Creates a proxy JNI with an optional secure environment (which can be NULL).
     * There is a one-to-one correspondence between proxy JNIs and threads, so
     * calling this method multiple times from the same thread will return
     * the same proxy JNI.
     */
	NS_IMETHOD
	CreateProxyJNI(nsISecureEnv* inSecureEnv, JNIEnv** outProxyEnv);
	
	/**
	 * Returns the proxy JNI associated with the current thread, or NULL if no
	 * such association exists.
	 */
	NS_IMETHOD
	GetProxyJNI(JNIEnv** outProxyEnv);
    
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

	/* from nsILiveConnectManager: */

	/**
	 * Attempts to start LiveConnect using the specified JSRuntime.
	 */
	NS_IMETHOD
    StartupLiveConnect(JSRuntime* runtime, PRBool& outStarted)
    {
    	outStarted = MaybeStartupLiveConnect();
    	return NS_OK;
    }
    
	/**
	 * Attempts to stop LiveConnect using the specified JSRuntime.
	 */
	NS_IMETHOD
    ShutdownLiveConnect(JSRuntime* runtime, PRBool& outShutdown)
    {
    	outShutdown = MaybeShutdownLiveConnect();
    	return NS_OK;
    }

	/**
	 * Indicates whether LiveConnect can be used.
	 */
	NS_IMETHOD
    IsLiveConnectEnabled(PRBool& outEnabled)
    {
    	outEnabled = IsLiveConnectEnabled();
    	return NS_OK;
    }

    /**
     * Initializes a JSContext with the proper LiveConnect support classes.
     */
	NS_IMETHOD
    InitLiveConnectClasses(JSContext* context, JSObject* globalObject);

    /**
     * Creates a JavaScript wrapper for a Java object.
     */
	NS_IMETHOD
	WrapJavaObject(JSContext* context, jobject javaObject, JSObject* *outJSObject);

    /* JVMMgr specific methods: */

    /* ====> From here on are things only called by the browser, not the plugin... */
    NS_IMETHOD
    GetClasspathAdditions(const char* *result);

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);

    nsIJVMPlugin* GetJVMPlugin(void) { return fJVM; }

    /* Unlike the nsIJVMPlugin::StartupJVM, this version handles putting
     * up any error dialog: */
    nsJVMStatus StartupJVM(void);
    nsJVMStatus ShutdownJVM(PRBool fullShutdown = PR_FALSE);
    nsJVMStatus GetJVMStatus(void);
    void SetJVMEnabled(PRBool enabled);

#if 0    
    void        ReportJVMError(nsresult err);
    const char* GetJavaErrorString(JNIEnv* env);
#endif

    nsresult    AddToClassPath(const char* dirPath);
    PRBool      MaybeStartupLiveConnect(void);
    PRBool      MaybeShutdownLiveConnect(void);
    PRBool      IsLiveConnectEnabled(void);
    JSJavaVM*   GetJSJavaVM(void) { return fJSJavaVM; }


    nsJVMManager(nsISupports* outer);
    virtual ~nsJVMManager(void);

protected:    

    void        EnsurePrefCallbackRegistered(void);
    const char* GetJavaErrorString(JRIEnv* env);

    nsIJVMPlugin*       fJVM;
    nsJVMStatus         fStatus;
    PRBool              fRegisteredJavaPrefChanged;
    nsISupports*        fDebugManager;
    JSJavaVM *          fJSJavaVM;  
    nsVector*           fClassPathAdditions;
    char*               fClassPathAdditionsString;
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
           nsJVMManager* nsJVMManager);

protected:
    nsSymantecDebugManager(nsISupports* outer, nsJVMManager* nsJVMManager);
    virtual ~nsSymantecDebugManager(void);

    nsJVMManager* fJVMMgr;

};

////////////////////////////////////////////////////////////////////////////////

#endif // nsJVMManager_h___
