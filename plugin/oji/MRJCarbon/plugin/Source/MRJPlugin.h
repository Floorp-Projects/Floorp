/* ----- BEGIN LICENSE BLOCK -----
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
 * The Original Code is the MRJ Carbon OJI Plugin.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):  Patrick C. Beard <beard@netscape.com>
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
 * ----- END LICENSE BLOCK ----- */

/*
	MRJPlugin.h
	
	MRJPlugin encapsulates the global state of the MRJ plugin as a single COM object.
	MRJPluginInstance represents an instance of the MRJ plugin.
	
	by Patrick C. Beard.
 */

#pragma once

#include "nsIPlugin.h"
#include "nsIJVMPlugin.h"
#include "nsIThreadManager.h"
#include "nsIPluginInstance.h"
#include "nsIJVMPluginInstance.h"
#include "nsIEventHandler.h"
#include "nsIPluginStreamListener.h"
#include "SupportsMixin.h"

class MRJPlugin;
class MRJPluginInstance;
class MRJSession;
class MRJContext;
class MRJConsole;

class nsIJVMManager;

class MRJPlugin :	public nsIPlugin, public nsIJVMPlugin,
					public nsIRunnable, public SupportsMixin {
public:
	MRJPlugin();
	virtual ~MRJPlugin();
	
	// Currently, this is a singleton, statically allocated object.
	void operator delete(void* ptr) {}

	// NS_DECL_ISUPPORTS
	NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
	NS_IMETHOD_(nsrefcnt) AddRef(void) { return addRef(); }
	NS_IMETHOD_(nsrefcnt) Release(void) { return release(); }
	
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
	LockFactory(PRBool aLock) { return NS_ERROR_NOT_IMPLEMENTED; }

	// nsIPlugin Methods.
	
	/**
     * Creates a new plugin instance, based on the MIME type. This
     * allows different impelementations to be created depending on
     * the specified MIME type.
     */
    NS_IMETHOD CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, 
                                    const char* aPluginMIMEType,
                                    void **aResult);

    /**
     * Initializes the plugin and will be called before any new instances are
     * created. This separates out the phase when a plugin is loaded just to
     * query for its mime type from the phase when a plugin is used for real.
     * The plugin should load up any resources at this point.
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Initialize(void);

    /**
     * Called when the browser is done with the plugin factory, or when
     * the plugin is disabled by the user.
     *
     * (Corresponds to NPP_Shutdown.)
     *
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Shutdown(void);

    /**
     * Returns the MIME description for the plugin. The MIME description 
     * is a colon-separated string containg the plugin MIME type, plugin
     * data file extension, and plugin name, e.g.:
     *
     * "application/x-simple-plugin:smp:Simple LiveConnect Sample Plug-in"
     *
     * (Corresponds to NPP_GetMIMEDescription.)
     *
     * @param resultingDesc - the resulting MIME description 
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetMIMEDescription(const char* *result);

    /**
     * Returns the value of a variable associated with the plugin.
     *
     * (Corresponds to NPP_GetValue.)
     *
     * @param variable - the plugin variable to get
     * @param value - the address of where to store the resulting value
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetValue(nsPluginVariable variable, void *value);

    // (Corresponds to NPP_SetValue.)
    NS_IMETHOD
    SetValue(nsPluginVariable variable, void *value);

	// JVM Plugin Methods.

    // This method us used to start the Java virtual machine.
    // It sets up any global state necessary to host Java programs.
    // Note that calling this method is distinctly separate from 
    // initializing the nsIJVMPlugin object (done by the Initialize
    // method).
    NS_IMETHOD
    StartupJVM(void);

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
	CreateSecureEnv(JNIEnv* proxyEnv, nsISecureEnv* *outSecureEnv);

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
	
	NS_IMETHOD
	UnwrapJavaWrapper(JNIEnv* jenv, jobject jobj, jint* obj);

private:
	nsIJVMManager* mManager;
	nsIThreadManager* mThreadManager;
	MRJSession* mSession;
    MRJConsole* mConsole;
    nsPluginThread *mPluginThreadID;
	Boolean mIsEnabled;
	
	// support for SupportsMixin.
	static const InterfaceInfo sInterfaces[];
	static const UInt32 kInterfaceCount;
};

class MRJPluginInstance :	public nsIPluginInstance, public nsIJVMPluginInstance,
							public nsIEventHandler, public nsIPluginStreamListener,
							private SupportsMixin {
public:
	MRJPluginInstance(MRJPlugin* plugin);
	virtual ~MRJPluginInstance();

	// NS_DECL_ISUPPORTS
	DECL_SUPPORTS_MIXIN

    // (Corresponds to NPP_HandleEvent.)
    NS_IMETHOD
    HandleEvent(nsPluginEvent* event, PRBool* handled);

    /**
     * Initializes a newly created plugin instance, passing to it the plugin
     * instance peer which it should use for all communication back to the browser.
     * 
     * @param peer - the corresponding plugin instance peer
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Initialize(nsIPluginInstancePeer* peer);

    /**
     * Returns a reference back to the plugin instance peer. This method is
     * used whenever the browser needs to obtain the peer back from a plugin
     * instance. The implementation of this method should be sure to increment
     * the reference count on the peer by calling AddRef.
     *
     * @param resultingPeer - the resulting plugin instance peer
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetPeer(nsIPluginInstancePeer* *result);

    /**
     * Called to instruct the plugin instance to start. This will be called after
     * the plugin is first created and initialized, and may be called after the
     * plugin is stopped (via the Stop method) if the plugin instance is returned
     * to in the browser window's history.
     *
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Start(void);

    /**
     * Called to instruct the plugin instance to stop, thereby suspending its state.
     * This method will be called whenever the browser window goes on to display
     * another page and the page containing the plugin goes into the window's history
     * list.
     *
     * @result - NS_OK if this operation was successful
     */
	NS_IMETHOD
	Stop(void);

    /**
     * Called to instruct the plugin instance to destroy itself. This is called when
     * it become no longer possible to return to the plugin instance, either because 
     * the browser window's history list of pages is being trimmed, or because the
     * window containing this page in the history is being closed.
     *
     * @result - NS_OK if this operation was successful
     */
	NS_IMETHOD
	Destroy(void);

    /**
     * Called when the window containing the plugin instance changes.
     *
     * (Corresponds to NPP_SetWindow.)
     *
     * @param window - the plugin window structure
     * @result - NS_OK if this operation was successful
     */
	NS_IMETHOD
	SetWindow(nsPluginWindow* window);

    /**
     * Called to tell the plugin that the initial src/data stream is
	 * ready.  Expects the plugin to return a nsIPluginStreamListener.
     *
     * (Corresponds to NPP_NewStream.)
     *
     * @param listener - listener the browser will use to give the plugin the data
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    NewStream(nsIPluginStreamListener** listener)
	{
		*listener = this;
		AddRef();
		return NS_OK;
	}

    /**
     * Called to instruct the plugin instance to print itself to a printer.
     *
     * (Corresponds to NPP_Print.)
     *
     * @param platformPrint - platform-specific printing information
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    Print(nsPluginPrint* platformPrint);

    /**
     * Returns the value of a variable associated with the plugin instance.
     *
     * @param variable - the plugin instance variable to get
     * @param value - the address of where to store the resulting value
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetValue(nsPluginInstanceVariable variable, void *value);

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

	// nsIPluginStreamListener implementation.
	
    /**
     * Notify the observer that the URL has started to load.  This method is
     * called only once, at the beginning of a URL load.<BR><BR>
     *
     * @return The return value is currently ignored.  In the future it may be
     * used to cancel the URL load..
     */
    NS_IMETHOD
    OnStartBinding(nsIPluginStreamInfo* pluginInfo)
    {
    	return NS_OK;
    }

    /**
     * Notify the client that data is available in the input stream.  This
     * method is called whenver data is written into the input stream by the
     * networking library...<BR><BR>
     * 
     * @param aIStream  The input stream containing the data.  This stream can
     * be either a blocking or non-blocking stream.
     * @param length    The amount of data that was just pushed into the stream.
     * @return The return value is currently ignored.
     */
    NS_IMETHOD
    OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, PRUint32 length);

    NS_IMETHOD
    OnFileAvailable(nsIPluginStreamInfo* pluginInfo, const char* fileName)
    {
		return NS_ERROR_NOT_IMPLEMENTED;
	}
	
    /**
     * Notify the observer that the URL has finished loading.  This method is 
     * called once when the networking library has finished processing the 
     * URL transaction initiatied via the nsINetService::Open(...) call.<BR><BR>
     * 
     * This method is called regardless of whether the URL loaded successfully.<BR><BR>
     * 
     * @param status    Status code for the URL load.
     * @param msg   A text string describing the error.
     * @return The return value is currently ignored.
     */
    NS_IMETHOD
    OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status)
    {
    	return NS_OK;
    }

	/**
	 * What is this method supposed to do?
	 */
    NS_IMETHOD
    GetStreamType(nsPluginStreamType *result)
    {
    	*result = nsPluginStreamType_Normal;
    	return NS_OK;
    }

    // Accessing the list of instances.
    static MRJPluginInstance* getInstances(void);
    MRJPluginInstance* getNextInstance(void);
    
    MRJContext* getContext(void);
    MRJSession* getSession(void);

private:
	void pushInstance(void);
	void popInstance(void);
	void inspectInstance(void);

private:
    nsIPluginInstancePeer* mPeer;
    nsIWindowlessPluginInstancePeer* mWindowlessPeer;
    MRJPlugin* mPlugin;
    MRJSession* mSession;
    MRJContext* mContext;
    jobject mApplet;
    nsPluginWindow* mPluginWindow;
    
    // maintain a list of instances.
    MRJPluginInstance* mNext;

	// support for SupportsMixin.
	static const InterfaceInfo sInterfaces[];
	static const UInt32 kInterfaceCount;
};
