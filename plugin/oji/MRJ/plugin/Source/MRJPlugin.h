/*
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

/*
	MRJPlugin.h
	
	MRJPlugin encapsulates the global state of the MRJ plugin as a single COM object.
	MRJPluginInstance represents an instance of the MRJ plugin.
	
	by Patrick C. Beard.
 */

#pragma once

#include "nsIJVMPlugin.h"
#include "nsIThreadManager.h"
#include "nsIJVMPluginInstance.h"
#include "SupportsMixin.h"

class MRJPlugin;
class MRJPluginInstance;
class MRJSession;
class MRJContext;
class MRJConsole;

class nsIJVMManager;

class MRJPlugin :	public nsIJVMPlugin, public nsIRunnable,
					public SupportsMixin {
public:
	MRJPlugin();
	virtual ~MRJPlugin();
	
	// Currently, this is a singleton, statically allocated object.
	void operator delete(void* ptr) {}

	// NS_DECL_ISUPPORTS
	NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
	NS_IMETHOD_(nsrefcnt) AddRef(void);
	NS_IMETHOD_(nsrefcnt) Release(void);
	
	// The Release method on NPIPlugin corresponds to NPP_Shutdown.

	// The old NPP_New call has been factored into two plugin instance methods:
	//
	// NewInstance -- called once, after the plugin instance is created. This 
	// method is used to initialize the new plugin instance (although the actual
	// plugin instance object will be created by the plugin manager).
	//
	// NPIPluginInstance::Start -- called when the plugin instance is to be
	// started. This happens in two circumstances: (1) after the plugin instance
	// is first initialized, and (2) after a plugin instance is returned to
	// (e.g. by going back in the window history) after previously being stopped
	// by the Stop method. 

	// nsIFactory Methods.
	
	NS_IMETHOD
	CreateInstance(nsISupports *aOuter, const nsIID& aIID, void **aResult);

	NS_IMETHOD
	LockFactory(PRBool aLock) { return NS_ERROR_FAILURE; }

	// nsIPlugin Methods.
	
    // This call initializes the plugin and will be called before any new
    // instances are created. It is passed browserInterfaces on which QueryInterface
    // may be used to obtain an nsIPluginManager, and other interfaces.
    NS_IMETHOD
    Initialize(nsISupports* browserInterfaces);

    // (Corresponds to NPP_Shutdown.)
    // Called when the browser is done with the plugin factory, or when
    // the plugin is disabled by the user.
    NS_IMETHOD
    Shutdown(void) { return NS_OK; }

    // (Corresponds to NPP_GetMIMEDescription.)
    NS_IMETHOD
    GetMIMEDescription(const char* *result);

    // (Corresponds to NPP_GetValue.)
    NS_IMETHOD
    GetValue(nsPluginVariable variable, void *value)
    {
    	return NS_ERROR_FAILURE;
    }

    // (Corresponds to NPP_SetValue.)
    NS_IMETHOD
    SetValue(nsPluginVariable variable, void *value)
    {
    	return NS_ERROR_FAILURE;
    }

	// JVM Plugin Methods.

    // This method us used to start the Java virtual machine.
    // It sets up any global state necessary to host Java programs.
    // Note that calling this method is distinctly separate from 
    // initializing the nsIJVMPlugin object (done by the Initialize
    // method).
    NS_IMETHOD
    StartupJVM(nsJVMInitArgs* initargs);

    // This method us used to stop the Java virtual machine.
    // It tears down any global state necessary to host Java programs.
    // The fullShutdown flag specifies whether the browser is quitting
    // (PR_TRUE) or simply whether the JVM is being shut down (PR_FALSE).
    NS_IMETHOD
    ShutdownJVM(PRBool fullShutdown);

    // Causes the JVM to append a new directory to its classpath.
    // If the JVM doesn't support this operation, an error is returned.
    NS_IMETHOD
    AddToClassPath(const char* dirPath);

    // Causes the JVM to remove a directory from its classpath.
    // If the JVM doesn't support this operation, an error is returned.
    NS_IMETHOD
    RemoveFromClassPath(const char* dirPath)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
    }

    // Returns the current classpath in use by the JVM.
    NS_IMETHOD
    GetClassPath(const char* *result);

    NS_IMETHOD
    GetJavaWrapper(JNIEnv* env, jint jsobj, jobject *jobj);

    NS_IMETHOD
    GetJavaVM(JavaVM* *result);

	// nsIJNIPlugin Methods.

    // Find or create a JNIEnv for the current thread.
    // Returns NULL if an error occurs.
    NS_IMETHOD_(nsrefcnt)
    GetJNIEnv(JNIEnv* *result);

    // This method must be called when the caller is done using the JNIEnv.
    // This decrements a refcount associated with it may free it.
    NS_IMETHOD_(nsrefcnt)
    ReleaseJNIEnv(JNIEnv* env);

	/**
	 * This creates a new secure communication channel with Java. The second parameter,
	 * nativeEnv, if non-NULL, will be the actual thread for Java communication.
	 * Otherwise, a new thread should be created.
	 * @param	proxyEnv		the env to be used by all clients on the browser side
	 * @return	outSecureEnv	the secure environment used by the proxyEnv
	 */
	NS_IMETHOD
	CreateSecureEnv(JNIEnv* proxyEnv, nsISecureJNI2* *outSecureEnv);

	/**
	 * Gives time to the JVM from the main event loop of the browser. This is
	 * necessary when there aren't any plugin instances around, but Java threads exist.
	 */
	NS_IMETHOD
	SpendTime(PRUint32 timeMillis);
	
	/**
	 * The Run method gives time to the JVM periodically. This makes SpendTIme() obsolete.
	 */
	NS_IMETHOD
	Run();
	
	// NON-INTERFACE methods, for internal use only.

	MRJSession* getSession();
	nsIJVMManager* getManager();
	nsIThreadManager* getThreadManager();

	MRJPluginInstance* getPluginInstance(jobject applet);
    MRJPluginInstance* getPluginInstance(JNIEnv* jenv);
    
    Boolean inPluginThread();
	
private:
	nsIJVMManager* mManager;
	nsIThreadManager* mThreadManager;
	MRJSession* mSession;
    MRJConsole* mConsole;
    PRUint32 mPluginThreadID;
	Boolean mIsEnabled;
	
	// support for nsISupports.
	static nsID sInterfaceIDs[];
};

class MRJPluginInstance :	public nsIJVMPluginInstance,
							private SupportsMixin {
public:
	MRJPluginInstance(MRJPlugin* plugin);
	virtual ~MRJPluginInstance();

	// NS_DECL_ISUPPORTS
	NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
	NS_IMETHOD_(nsrefcnt) AddRef(void);
	NS_IMETHOD_(nsrefcnt) Release(void);

    // (Corresponds to NPP_HandleEvent.)
    NS_IMETHOD
    HandleEvent(nsPluginEvent* event, PRBool* handled);

    // The Release method on NPIPluginInstance corresponds to NPP_Destroy.

    NS_IMETHOD
    Initialize(nsIPluginInstancePeer* peer);

    // Required backpointer to the peer.
    NS_IMETHOD
    GetPeer(nsIPluginInstancePeer* *result);

    // See comment for nsIPlugin::CreateInstance, above.
    NS_IMETHOD
    Start(void);

    // The old NPP_Destroy call has been factored into two plugin instance 
    // methods:
    //
    // Stop -- called when the plugin instance is to be stopped (e.g. by 
    // displaying another plugin manager window, causing the page containing 
    // the plugin to become removed from the display).
    //
    // Destroy -- called once, before the plugin instance peer is to be 
    // destroyed. This method is used to destroy the plugin instance.

	NS_IMETHOD
	Stop(void);

	NS_IMETHOD
	Destroy(void);

    // (Corresponds to NPP_SetWindow.)
	NS_IMETHOD
	SetWindow(nsPluginWindow* window);

    // (Corresponds to NPP_NewStream.)
	NS_IMETHOD
	NewStream(nsIPluginStreamPeer* peer, nsIPluginStream* *result)
	{
		*result = NULL;
		return NS_ERROR_NOT_IMPLEMENTED;
	}

    // (Corresponds to NPP_Print.)
    NS_IMETHOD
    Print(nsPluginPrint* platformPrint)
    {
		return NS_ERROR_NOT_IMPLEMENTED;
    }

    // (Corresponds to NPP_URLNotify.)
    NS_IMETHOD
    URLNotify(const char* url, const char* target,
    			nsPluginReason reason, void* notifyData)
    {
    	return NS_OK;
    }

    /**
     * Returns the value of a variable associated with the plugin instance.
     *
     * @param variable - the plugin instance variable to get
     * @param value - the address of where to store the resulting value
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetValue(nsPluginInstanceVariable variable, void *value)
    {
		return NS_ERROR_NOT_IMPLEMENTED;
	}

	// nsIJVMPluginInstance methods.

    // This method is called when LiveConnect wants to find the Java object
    // associated with this plugin instance, e.g. the Applet or JavaBean object.
    NS_IMETHOD
    GetJavaObject(jobject *result);

    NS_IMETHOD
    GetText(const char* *result)
    {
        *result = NULL;
    	return NS_OK;
    }

    // Accessing the list of instances.
    static MRJPluginInstance* getInstances(void);
    MRJPluginInstance* getNextInstance(void);
    
    MRJContext* getContext(void);
    MRJSession* getSession(void);

private:
	void pushInstance();
	void popInstance();

private:
    nsIPluginInstancePeer* mPeer;
    nsIWindowlessPluginInstancePeer* mWindowlessPeer;
    MRJPlugin* mPlugin;
    MRJSession* mSession;
    MRJContext* mContext;
    jobject mApplet;
    
    // maintain a list of instances.
    MRJPluginInstance* mNext;

    // support for nsISupports.
    static nsID sInterfaceIDs[];
};
