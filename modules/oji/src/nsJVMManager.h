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
#include "nsVoidArray.h"
#include "nsILiveConnectManager.h"

class nsSymantecDebugManager;
class nsIWebBrowserChrome;


/*******************************************************************************
 * NsJVMManager is the interface to the JVM manager that the browser sees. All
 * files that want to include java services should include this header file.
 * nsIJVMManager is the more limited interface what the JVM plugin sees.
 ******************************************************************************/

struct nsJVMManager : public nsIJVMManager, public nsIThreadManager, public nsILiveConnectManager {
public:

    NS_DECL_AGGREGATED

    NS_DECL_NSIJVMMANAGER    
    
    /* from nsIThreadManager: */
    
	/**
	 * Returns a unique identifier for the "current" system thread.
	 */
	NS_IMETHOD
	GetCurrentThread(nsPluginThread* *threadID);

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
	 * Creates a new thread, calling the specified runnable's Run method (a la Java).
	 */
	NS_IMETHOD
	CreateThread(PRUint32* threadID, nsIRunnable* runnable);
	
	/**
	 * Posts an event to specified thread, calling the runnable from that thread.
	 * @param threadID thread to call runnable from
	 * @param runnable object to invoke from thread
	 * @param async if true, won't block current thread waiting for result
	 */
	NS_IMETHOD
	PostEvent(PRUint32 threadID, nsIRunnable* runnable, PRBool async);

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

    /**

     * @return conjure up THE nsIWebBrowserChrome instance from thin
     * air!

     */

    nsresult    GetChrome(nsIWebBrowserChrome **theChrome);
    const char* GetJavaErrorString(JRIEnv* env);

    nsIJVMPlugin*       fJVM;
    nsJVMStatus         fStatus;
    PRBool              fRegisteredJavaPrefChanged;
    nsISupports*        fDebugManager;
    JSJavaVM *          fJSJavaVM;  
    nsVoidArray*        fClassPathAdditions;
    char*               fClassPathAdditionsString;
    PRBool              fStartupMessagePosted;
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
