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

/*******************************************************************************
 * npsimple.cpp
 ******************************************************************************
 * Simple LiveConnect Sample Plugin
 * Copyright (c) 1996 Netscape Communications. All rights reserved.
 ******************************************************************************
 * OVERVIEW
 * --------
 * Section 1 - Includes
 * Section 2 - Instance Structs
 * Section 3 - API Plugin Implementations
 * Section 4 - Java Native Method Implementations
 * Section 5 - Utility Method Implementations
 *******************************************************************************/

/*******************************************************************************
 * SECTION 1 - Includes
 ******************************************************************************/
#include <stdio.h>
#include <string.h>
#include "nsplugin.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "simpleCID.h"

/*------------------------------------------------------------------------------
 * Windows Includes
 *----------------------------------------------------------------------------*/
#ifdef _WINDOWS /* Windows Includes */
#include <windows.h>
#endif /* _WINDOWS */
/*------------------------------------------------------------------------------
 * UNIX includes
 *----------------------------------------------------------------------------*/
#ifdef XP_UNIX
//#include <X11/Xlib.h>
//#include <X11/Intrinsic.h>
//#include <X11/StringDefs.h>
#endif /* XP_UNIX */


/*******************************************************************************
 * SECTION 2 - Instance Structs
 *******************************************************************************
 * Instance state information about the plugin.
 *
 * PLUGIN DEVELOPERS:
 *	Use this struct to hold per-instance information that you'll
 *	need in the various functions in this file.
 ******************************************************************************
 * First comes the PlatformInstance struct, which contains platform specific
 * information for each instance.
 *****************************************************************************/
 
/*------------------------------------------------------------------------------
 * Windows PlatformInstance
 *----------------------------------------------------------------------------*/

#ifdef XP_PC
typedef struct _PlatformInstance
{
    HWND		fhWnd;
    WNDPROC		fDefaultWindowProc;
} PlatformInstance;
#endif /* XP_PC */

/*------------------------------------------------------------------------------
 * UNIX PlatformInstance
 *----------------------------------------------------------------------------*/

#ifdef XP_UNIX
typedef struct _PlatformInstance
{
    Window 		window;
    Display *		display;
    uint32 		x, y;
    uint32 		width, height;
} PlatformInstance;
#endif /* XP_UNIX */

/*------------------------------------------------------------------------------
 * Macintosh PlatformInstance
 *----------------------------------------------------------------------------*/

#ifdef XP_MAC
typedef struct _PlatformInstance
{
    int			placeholder;
} PlatformInstance;
#endif /* macintosh */


// Define constants for easy use
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kIPluginIID, NS_IPLUGIN_IID);
static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID);
static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIServiceManagerIID, NS_ISERVICEMANAGER_IID);
static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);

static NS_DEFINE_CID(kPluginCID, NS_PLUGIN_CID);
static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_CID(kSimplePluginCID, NS_SIMPLEPLUGIN_CID);

#define PLUGIN_NAME             "Simple Sample Plug-in"
#define PLUGIN_DESCRIPTION      "Demonstrates a simple plug-in."
#define PLUGIN_MIME_DESCRIPTION "application/x-simple:smp:Simple Sample Plug-in"
#define PLUGIN_MIME_TYPE "application/x-simple"

static const char* g_desc = "Sample XPCOM Plugin";


////////////////////////////////////////////////////////////////////////////////
// Simple Plugin Classes
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SimplePlugin represents the class of all simple plugins. One 
// instance of this class is kept around for as long as there are
// plugin instances outstanding.

class SimplePlugin : public nsIPlugin {
public:
    
    ////////////////////////////////////////////////////////////////////////////
    // from nsIFactory:

    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              REFNSIID aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);
    
    ////////////////////////////////////////////////////////////////////////////
    // from nsIPlugin:

    // This call initializes the plugin and will be called before any new
    // instances are created. It is passed browserInterfaces on which QueryInterface
    // may be used to obtain an nsIPluginManager, and other interfaces.
    NS_IMETHOD
    Initialize(void);

    // (Corresponds to NPP_Shutdown.)
    // Called when the browser is done with the plugin factory, or when
    // the plugin is disabled by the user.
    NS_IMETHOD
    Shutdown(void);

    // (Corresponds to NPP_GetMIMEDescription.)
    NS_IMETHOD
    GetMIMEDescription(const char* *result);

    // (Corresponds to NPP_GetValue.)
    NS_IMETHOD
    GetValue(nsPluginVariable variable, void *value);

    NS_IMETHOD
    CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, 
                         const char* aPluginMIMEType,
                         void **aResult);

    // The old NPP_New call has been factored into two plugin instance methods:
    //
    // CreateInstance -- called once, after the plugin instance is created. This 
    // method is used to initialize the new plugin instance (although the actual
    // plugin instance object will be created by the plugin manager).
    //
    // nsIPluginInstance::Start -- called when the plugin instance is to be
    // started. This happens in two circumstances: (1) after the plugin instance
    // is first initialized, and (2) after a plugin instance is returned to
    // (e.g. by going back in the window history) after previously being stopped
    // by the Stop method. 

    // SimplePlugin specific methods:

    SimplePlugin();
    virtual ~SimplePlugin(void);

    NS_DECL_ISUPPORTS

    nsIPluginManager* GetPluginManager(void) { return mPluginManager; }

protected:
    nsIPluginManager* mPluginManager;
	nsIServiceManager* mServiceManager;

};

////////////////////////////////////////////////////////////////////////////////
// SimplePluginInstance represents an instance of the SimplePlugin class.

class SimplePluginInstance : public nsIPluginInstance {
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIEventHandler:

    // (Corresponds to NPP_HandleEvent.)
    // Note that for Unix and Mac the nsPluginEvent structure is different
    // from the old NPEvent structure -- it's no longer the native event
    // record, but is instead a struct. This was done for future extensibility,
    // and so that the Mac could receive the window argument too. For Windows
    // and OS2, it's always been a struct, so there's no change for them.
    NS_IMETHOD
    HandleEvent(nsPluginEvent* event, PRBool* handled);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginInstance:

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

    NS_IMETHOD
    NewStream(nsIPluginStreamListener** listener);

    // (Corresponds to NPP_Print.)
    NS_IMETHOD
    Print(nsPluginPrint* platformPrint);

    NS_IMETHOD
    GetValue(nsPluginInstanceVariable variable, void *value);

    ////////////////////////////////////////////////////////////////////////////
    // SimplePluginInstance specific methods:

    SimplePluginInstance(void);
    virtual ~SimplePluginInstance(void);

    NS_DECL_ISUPPORTS

    void            PlatformNew(void);
    nsresult        PlatformDestroy(void);
    nsresult    	PlatformSetWindow(nsPluginWindow* window);
    PRInt16         PlatformHandleEvent(nsPluginEvent* event);

    void SetMode(nsPluginMode mode) { fMode = mode; }

#ifdef XP_PC
    static LRESULT CALLBACK 
    PluginWindowProc( HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
#endif

protected:
    nsIPluginInstancePeer*      fPeer;
    nsPluginWindow*             fWindow;
    nsPluginMode                fMode;
    PlatformInstance            fPlatform;

};

////////////////////////////////////////////////////////////////////////////////
// SimplePluginStream represents the stream used by SimplePluginInstances
// to receive data from the browser. 

class SimplePluginStreamListener : public nsIPluginStreamListener {
public:

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginStreamListener:

    /**
     * Notify the observer that the URL has started to load.  This method is
     * called only once, at the beginning of a URL load.<BR><BR>
     *
     * @return The return value is currently ignored.  In the future it may be
     * used to cancel the URL load..
     */
    NS_IMETHOD
    OnStartBinding(nsIPluginStreamInfo* pluginInfo);

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
    OnDataAvailable(nsIPluginStreamInfo* pluginInfo, 
                                            nsIInputStream* input, 
                                            PRUint32 length);

    NS_IMETHOD
    OnFileAvailable(nsIPluginStreamInfo* pluginInfo, const char* fileName);

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
    OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status);

    NS_IMETHOD
    OnNotify(const char* url, nsresult status);

    NS_IMETHOD
    GetStreamType(nsPluginStreamType *result);

    ////////////////////////////////////////////////////////////////////////////
    // SimplePluginStreamListener specific methods:

    SimplePluginStreamListener(SimplePluginInstance* inst, const char* url);
    virtual ~SimplePluginStreamListener(void);

protected:
    const char*                 fMessageName;

};

/*******************************************************************************
 * SECTION 3 - API Plugin Implementations
 ******************************************************************************/

// This counter is used to keep track of the number of outstanding objects.
// It is used to determine whether the plugin's DLL can be unloaded.
static PRUint32 gPluginObjectCount = 0;

// This flag is used to keep track of whether the plugin's DLL is explicitly
// being retained by some client.
static PRBool gPluginLocked = PR_FALSE;

////////////////////////////////////////////////////////////////////////////////
// SimplePlugin Methods
////////////////////////////////////////////////////////////////////////////////

SimplePlugin::SimplePlugin()
{
    NS_INIT_REFCNT();

    if(nsComponentManager::CreateInstance(kCPluginManagerCID, 
                                          NULL, kIPluginManagerIID, (void**)&mPluginManager) != NS_OK)
        return;

    gPluginObjectCount++;
}

SimplePlugin::~SimplePlugin(void)
{
    if(mPluginManager)
        mPluginManager->Release();

    if(mServiceManager)
        mServiceManager->Release();

    gPluginObjectCount--;
}

// These macros produce simple version of QueryInterface and AddRef.
// See the nsISupports.h header file for details.

NS_METHOD
SimplePlugin::QueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (!aInstancePtr)
        return NS_ERROR_NULL_POINTER;

    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsISupports*,this);
    } else if (aIID.Equals(kIFactoryIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsISupports*,NS_STATIC_CAST(nsIFactory*,this));
    } else if (aIID.Equals(kIPluginIID)) {
        *aInstancePtr = NS_STATIC_CAST(nsISupports*,NS_STATIC_CAST(nsIPlugin*,this));
    } else {
        *aInstancePtr = nsnull;
        return NS_ERROR_NO_INTERFACE;
    }

    NS_ADDREF(NS_REINTERPRET_CAST(nsISupports*,*aInstancePtr));

    return NS_OK;
}
 
NS_IMPL_ADDREF(SimplePlugin);
NS_IMPL_RELEASE(SimplePlugin);

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NSGetFactory:
 * Provides global initialization for a plug-in, and returns an error value. 
 *
 * This function is called once when a plug-in is loaded, before the first instance
 * is created. You should allocate any memory or resources shared by all
 * instances of your plug-in at this time. After the last instance has been deleted,
 * NPP_Shutdown will be called, where you can release any memory or
 * resources allocated by NPP_Initialize. 
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

SimplePlugin* gPlugin = NULL;

extern "C" NS_EXPORT nsresult
NSGetFactory(nsISupports* serviceMgr,
             const nsCID &aCID,
             const char *aClassName,
             const char *aProgID,
             nsIFactory **aResult)
{
    if (!aResult)
        return NS_ERROR_NULL_POINTER; 

    *aResult = nsnull; 

    nsISupports *inst; 

    if (aCID.Equals(kSimplePluginCID)) {
        // Ok, we know this CID and here is the factory
        // that can manufacture the objects
        inst = new SimplePlugin(); 
    } else if (aCID.Equals(kPluginCID)) { 
        inst = new SimplePlugin(); 
    } else {
        return NS_ERROR_NO_INTERFACE; 
    } 

    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY; 

    nsresult rv = inst->QueryInterface(kIFactoryIID, 
                                       (void **) aResult); 

    if (NS_FAILED(rv)) { 
        delete inst; 
    } 

    return rv; 
}

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* serviceMgr)
{
    return gPluginObjectCount == 1 && !gPluginLocked;
}

extern "C" NS_EXPORT nsresult 
NSRegisterSelf(nsISupports *aServMgr, const char *path)
{
    nsresult rv = NS_OK;

    char buf[255];    // todo: use a const

    nsString2 progID(NS_INLINE_PLUGIN_PROGID_PREFIX);

    // We will use the service manager to obtain the component
    // manager, which will enable us to register a component
    // with a ProgID (text string) instead of just the CID.

    nsIServiceManager *sm;

    // We can get the IID of an interface with the static GetIID() method as
    // well.
    
    rv = aServMgr->QueryInterface(nsIServiceManager::GetIID(), (void **)&sm);

    if (NS_FAILED(rv))
        return rv;

    nsIComponentManager *cm;

    rv = sm->GetService(kComponentManagerCID, nsIComponentManager::GetIID(), (nsISupports **)&cm);

    if (NS_FAILED(rv)) {
        NS_RELEASE(sm);

        return rv;
    }

    // Note the text string, we can access the hello component with just this
    // string without knowing the CID

    progID += PLUGIN_MIME_TYPE;
    progID.ToCString(buf, 255);     // todo: need to use a const

    rv = cm->RegisterComponent(kSimplePluginCID, g_desc, buf,
		path, PR_TRUE, PR_TRUE);

    sm->ReleaseService(kComponentManagerCID, cm);

    NS_RELEASE(sm);

#ifdef NS_DEBUG
	printf("*** %s registered\n",g_desc);
#endif

	return rv;
}

extern "C" NS_EXPORT nsresult 
NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
	nsresult rv = NS_OK;

	nsIServiceManager *sm;

	rv = aServMgr->QueryInterface(nsIServiceManager::GetIID(), (void **)&sm);

	if (NS_FAILED(rv))
		return rv;

	nsIComponentManager *cm;

	rv = sm->GetService(kComponentManagerCID, nsIComponentManager::GetIID(), (nsISupports **)&cm);

	if (NS_FAILED(rv)) {
		NS_RELEASE(sm);

		return rv;
	}

	rv = cm->UnregisterComponent(kSimplePluginCID, path);

	sm->ReleaseService(kComponentManagerCID, cm);

	NS_RELEASE(sm);

	return rv;
}


NS_METHOD
SimplePlugin::CreateInstance(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
    SimplePluginInstance* inst = new SimplePluginInstance();
    if (inst == NULL) 
        return NS_ERROR_OUT_OF_MEMORY;
    inst->AddRef();
    *aResult = inst;
    return NS_OK;
}

NS_METHOD
SimplePlugin::LockFactory(PRBool aLock)
{
    gPluginLocked = aLock;
    return NS_OK;
}

NS_METHOD
SimplePlugin::Initialize()
{
#ifdef NS_DEBUG
    printf("SimplePlugin::Initialize\n");
#endif

    return NS_OK;
}

NS_METHOD
SimplePlugin::Shutdown(void)
{
#ifdef NS_DEBUG
    printf("SimplePlugin::Shutdown\n");
#endif

    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * GetMIMEDescription:
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

NS_METHOD
SimplePlugin::GetMIMEDescription(const char* *result)
{
#ifdef NS_DEBUG
    printf("SimplePlugin::GetMIMEDescription\n");
#endif

    *result = PLUGIN_MIME_DESCRIPTION;
    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NPP_GetValue:
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

NS_METHOD
SimplePlugin::GetValue(nsPluginVariable variable, void *value)
{
#ifdef NS_DEBUG
    printf("SimplePlugin::GetValue\n");
#endif

    nsresult err = NS_OK;
    if (variable == nsPluginVariable_NameString)
        *((char **)value) = PLUGIN_NAME;
    else if (variable == nsPluginVariable_DescriptionString)
        *((char **)value) = PLUGIN_DESCRIPTION;
    else
        err = NS_ERROR_FAILURE;

    return err;
}

NS_IMETHODIMP
SimplePlugin::CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, 
                                      const char* aPluginMIMEType,
                                      void **aResult)
{
#ifdef NS_DEBUG
    printf("SimplePlugin::CreatePluginInstance\n");
#endif

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// SimplePluginInstance Methods
////////////////////////////////////////////////////////////////////////////////

SimplePluginInstance::SimplePluginInstance(void)
    : fPeer(NULL), fWindow(NULL), fMode(nsPluginMode_Embedded)
{
    NS_INIT_REFCNT();
    gPluginObjectCount++;
}

SimplePluginInstance::~SimplePluginInstance(void)
{
    gPluginObjectCount--;
    PlatformDestroy(); // Perform platform specific cleanup
}

// These macros produce simple version of QueryInterface and AddRef.
// See the nsISupports.h header file for details.

NS_IMPL_QUERY_INTERFACE(SimplePluginInstance, kIPluginInstanceIID);
NS_IMPL_ADDREF(SimplePluginInstance);
NS_IMPL_RELEASE(SimplePluginInstance);

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NewInstance:
 * Creates a new instance of a plug-in and returns an error value. 
 * 
 * NewInstance creates a new instance of your plug-in with MIME type specified
 * by pluginType. The parameter mode is NP_EMBED if the instance was created
 * by an EMBED tag, or NP_FULL if the instance was created by a separate file.
 * You can allocate any instance-specific private data in instance->pdata at this
 * time. The NPP pointer is valid until the instance is destroyed. 
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

NS_METHOD
SimplePluginInstance::Initialize(nsIPluginInstancePeer* peer)
{
#ifdef NS_DEBUG
    printf("SimplePluginInstance::Initialize\n");
#endif
    
    NS_ASSERTION(peer != NULL, "null peer");

    fPeer = peer;
    nsIPluginTagInfo* taginfo;
    const char* const* names = nsnull;
    const char* const* values = nsnull;
    PRUint16 count = 0;
    nsresult result;

    peer->AddRef();
    result = peer->GetMode(&fMode);
    if (NS_FAILED(result)) return result;

   result = peer->QueryInterface(nsIPluginTagInfo::GetIID(), (void **)&taginfo);

    if (!NS_FAILED(result))
    {
        taginfo->GetAttributes(count, names, values);
        NS_IF_RELEASE(taginfo);
    }

#ifdef NS_DEBUG
    printf("Attribute count = %d\n", count);

    for (int i = 0; i < count; i++)
    {
        printf("plugin param=%s, value=%s\n", names[i], values[i]);
    }
#endif

    PlatformNew(); 	/* Call Platform-specific initializations */
    return NS_OK;
}

NS_METHOD
SimplePluginInstance::GetPeer(nsIPluginInstancePeer* *result)
{
#ifdef NS_DEBUG
    printf("SimplePluginInstance::GetPeer\n");
#endif

    fPeer->AddRef();
    *result = fPeer;
    return NS_OK;
}

NS_METHOD
SimplePluginInstance::Start(void)
{
#ifdef NS_DEBUG
    printf("SimplePluginInstance::Start\n");
#endif

    return NS_OK;
}

NS_METHOD
SimplePluginInstance::Stop(void)
{
#ifdef NS_DEBUG
    printf("SimplePluginInstance::Stop\n");
#endif

    return NS_OK;
}

NS_METHOD
SimplePluginInstance::Destroy(void)
{
#ifdef NS_DEBUG
    printf("SimplePluginInstance::Destroy\n");
#endif

    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NPP_SetWindow:
 * Sets the window in which a plug-in draws, and returns an error value. 
 * 
 * NPP_SetWindow informs the plug-in instance specified by instance of the
 * the window denoted by window in which the instance draws. This nsPluginWindow
 * pointer is valid for the life of the instance, or until NPP_SetWindow is called
 * again with a different value. Subsequent calls to NPP_SetWindow for a given
 * instance typically indicate that the window has been resized. If either window
 * or window->window are NULL, the plug-in must not perform any additional
 * graphics operations on the window and should free any resources associated
 * with the window. 
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

NS_METHOD
SimplePluginInstance::SetWindow(nsPluginWindow* window)
{
#ifdef NS_DEBUG
    printf("SimplePluginInstance::SetWindow\n");
#endif

    nsresult result;

    /*
     * PLUGIN DEVELOPERS:
     *	Before setting window to point to the
     *	new window, you may wish to compare the new window
     *	info to the previous window (if any) to note window
     *	size changes, etc.
     */
    result = PlatformSetWindow(window);
    fWindow = window;
    return result;
}

NS_METHOD
SimplePluginInstance::NewStream(nsIPluginStreamListener** listener)
{
#ifdef NS_DEBUG
    printf("SimplePluginInstance::NewStream\n");
#endif

    if(listener != NULL)
        *listener = new SimplePluginStreamListener(this, "http://warp");
    
    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NPP_Print:
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

NS_METHOD
SimplePluginInstance::Print(nsPluginPrint* printInfo)
{
#ifdef NS_DEBUG
    printf("SimplePluginInstance::Print\n");
#endif

    if (printInfo == NULL)
        return NS_ERROR_FAILURE;

    if (printInfo->mode == nsPluginMode_Full) {
        /*
         * PLUGIN DEVELOPERS:
         *	If your plugin would like to take over
         *	printing completely when it is in full-screen mode,
         *	set printInfo->pluginPrinted to TRUE and print your
         *	plugin as you see fit.  If your plugin wants Netscape
         *	to handle printing in this case, set
         *	printInfo->pluginPrinted to FALSE (the default) and
         *	do nothing.  If you do want to handle printing
         *	yourself, printOne is true if the print button
         *	(as opposed to the print menu) was clicked.
         *	On the Macintosh, platformPrint is a THPrint; on
         *	Windows, platformPrint is a structure
         *	(defined in npapi.h) containing the printer name, port,
         *	etc.
         */

        /* Do the default*/
        printInfo->print.fullPrint.pluginPrinted = PR_FALSE;
    }
    else {	/* If not fullscreen, we must be embedded */
        /*
         * PLUGIN DEVELOPERS:
         *	If your plugin is embedded, or is full-screen
         *	but you returned false in pluginPrinted above, NPP_Print
         *	will be called with mode == nsPluginMode_Embedded.  The nsPluginWindow
         *	in the printInfo gives the location and dimensions of
         *	the embedded plugin on the printed page.  On the
         *	Macintosh, platformPrint is the printer port; on
         *	Windows, platformPrint is the handle to the printing
         *	device context.
         */
    }
    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NPP_HandleEvent:
 * Mac-only, but stub must be present for Windows
 * Delivers a platform-specific event to the instance. 
 * 
 * On the Macintosh, event is a pointer to a standard Macintosh EventRecord.
 * All standard event types are passed to the instance as appropriate. In general,
 * return TRUE if you handle the event and FALSE if you ignore the event. 
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

NS_METHOD
SimplePluginInstance::HandleEvent(nsPluginEvent* event, PRBool* handled)
{
#ifdef NS_DEBUG
    printf("SimplePluginInstance::HandleEvent\n");
#endif

    *handled = (PRBool)PlatformHandleEvent(event);
    return NS_OK;
}

NS_METHOD
SimplePluginInstance::GetValue(nsPluginInstanceVariable variable, void *value)
{
#ifdef NS_DEBUG
    printf("SimplePluginInstance::GetValue\n");
#endif

    return NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
// SimplePluginStreamListener Methods
////////////////////////////////////////////////////////////////////////////////

SimplePluginStreamListener::SimplePluginStreamListener(SimplePluginInstance* inst,
                                                       const char* msgName)
    : fMessageName(msgName)
{
    gPluginObjectCount++;
    NS_INIT_REFCNT();
    char msg[256];
    sprintf(msg, "### Creating SimplePluginStreamListener for %s\n", fMessageName);
}

SimplePluginStreamListener::~SimplePluginStreamListener(void)
{
    gPluginObjectCount--;
    char msg[256];
    sprintf(msg, "### Destroying SimplePluginStreamListener for %s\n", fMessageName);
}

// This macro produces a simple version of QueryInterface, AddRef and Release.
// See the nsISupports.h header file for details.

NS_IMPL_ISUPPORTS(SimplePluginStreamListener, kIPluginStreamListenerIID);

NS_METHOD
SimplePluginStreamListener::OnStartBinding(nsIPluginStreamInfo* pluginInfo)
{
#ifdef NS_DEBUG
    printf("SimplePluginStreamListener::OnStartBinding\n");
#endif

    char msg[256];
    sprintf(msg, "### Opening plugin stream for %s\n", fMessageName);
    return NS_OK;
}

NS_METHOD
SimplePluginStreamListener::OnDataAvailable(nsIPluginStreamInfo* pluginInfo, 
                                            nsIInputStream* input, 
                                            PRUint32 length)
{
#ifdef NS_DEBUG
    printf("SimplePluginStreamListener::OnDataAvailable\n");
#endif

    char* buffer = new char[length];
    if (buffer) {
        PRUint32 amountRead = 0;
        nsresult rslt = input->Read(buffer, length, &amountRead);
        if (rslt == NS_OK) {
            char msg[256];
            sprintf(msg, "### Received %d bytes for %s\n", length, fMessageName);
        }
        delete buffer;
    }
    return NS_OK;
}

NS_METHOD
SimplePluginStreamListener::OnFileAvailable(nsIPluginStreamInfo* pluginInfo, 
                                            const char* fileName)
{
#ifdef NS_DEBUG
    printf("SimplePluginStreamListener::OnFileAvailable\n");
#endif

    char msg[256];
    sprintf(msg, "### File available for %s: %s\n", fMessageName, fileName);
    return NS_OK;
}

NS_METHOD
SimplePluginStreamListener::OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status )
{
#ifdef NS_DEBUG
    printf("SimplePluginStreamListener::OnStopBinding\n");
#endif

    char msg[256];
    sprintf(msg, "### Closing plugin stream for %s\n", fMessageName);
    return NS_OK;
}

NS_METHOD
SimplePluginStreamListener::OnNotify(const char* url, nsresult status)
{
#ifdef NS_DEBUG
    printf("SimplePluginStreamListener::OnNotify\n");
#endif

    return NS_OK;
}

NS_METHOD
SimplePluginStreamListener::GetStreamType(nsPluginStreamType *result)
{
#ifdef NS_DEBUG
    printf("SimplePluginStreamListener::GetStreamType\n");
#endif

    *result = nsPluginStreamType_Normal;
    return NS_OK;
}

/*******************************************************************************
 * SECTION 5 - Utility Method Implementations
 *******************************************************************************/

/*------------------------------------------------------------------------------
 * Platform-Specific Implemenations
 *------------------------------------------------------------------------------
 * UNIX Implementations
 *----------------------------------------------------------------------------*/

#ifdef XP_UNIX

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformNew
 *
 * Initialize any Platform-Specific instance data.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

void
SimplePluginInstance::PlatformNew(void)
{
    fPlatform.window = 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformDestroy
 *
 * Destroy any Platform-Specific instance data.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

nsresult
SimplePluginInstance::PlatformDestroy(void)
{
	return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformSetWindow
 *
 * Perform platform-specific window operations
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

nsresult
SimplePluginInstance::PlatformSetWindow(nsPluginWindow* window)
{
    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformHandleEvent
 *
 * Handle platform-specific events.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

int16
SimplePluginInstance::PlatformHandleEvent(nsPluginEvent* event)
{
    /* UNIX Plugins do not use HandleEvent */
    return 0;
}
#endif /* XP_UNIX */

/*------------------------------------------------------------------------------
 * Windows Implementations
 *----------------------------------------------------------------------------*/

#ifdef XP_PC
const char* gInstanceLookupString = "instance->pdata";

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformNew
 *
 * Initialize any Platform-Specific instance data.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

void
SimplePluginInstance::PlatformNew(void)
{
    fPlatform.fhWnd = NULL;
    fPlatform.fDefaultWindowProc = NULL;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformDestroy
 *
 * Destroy any Platform-Specific instance data.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

nsresult
SimplePluginInstance::PlatformDestroy(void)
{
    if( fWindow != NULL ) { /* If we have a window, clean
                                   * it up. */
        SetWindowLong( fPlatform.fhWnd, GWL_WNDPROC, (LONG)fPlatform.fDefaultWindowProc);
        fPlatform.fDefaultWindowProc = NULL;
        fPlatform.fhWnd = NULL;
    }

    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformSetWindow
 *
 * Perform platform-specific window operations
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

nsresult
SimplePluginInstance::PlatformSetWindow(nsPluginWindow* window)
{
    if( fWindow != NULL ) /* If we already have a window, clean
                           * it up before trying to subclass
                           * the new window. */
    {
        if( (window == NULL) || ( window->window == NULL ) ) {
            /* There is now no window to use. get rid of the old
             * one and exit. */
            SetWindowLong( fPlatform.fhWnd, GWL_WNDPROC, (LONG)fPlatform.fDefaultWindowProc);
            fPlatform.fDefaultWindowProc = NULL;
            fPlatform.fhWnd = NULL;
            return NS_OK;
        }

        else if ( fPlatform.fhWnd == (HWND) window->window ) {
            /* The new window is the same as the old one. Exit now. */
            return NS_OK;
        }
        else {
            /* Clean up the old window, so that we can subclass the new
             * one later. */
            SetWindowLong( fPlatform.fhWnd, GWL_WNDPROC, (LONG)fPlatform.fDefaultWindowProc);
            fPlatform.fDefaultWindowProc = NULL;
            fPlatform.fhWnd = NULL;
        }
    }
    else if( (window == NULL) || ( window->window == NULL ) ) {
        /* We can just get out of here if there is no current
         * window and there is no new window to use. */
        return NS_OK;
    }

    /* At this point, we will subclass
     * window->window so that we can begin drawing and
     * receiving window messages. */
    fPlatform.fDefaultWindowProc =
        (WNDPROC)SetWindowLong( (HWND)window->window,
                                GWL_WNDPROC, (LONG)SimplePluginInstance::PluginWindowProc);
    fPlatform.fhWnd = (HWND) window->window;
    SetProp(fPlatform.fhWnd, gInstanceLookupString, (HANDLE)this);

    InvalidateRect( fPlatform.fhWnd, NULL, TRUE );
    UpdateWindow( fPlatform.fhWnd );
    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformHandleEvent
 *
 * Handle platform-specific events.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

PRInt16
SimplePluginInstance::PlatformHandleEvent(nsPluginEvent* event)
{
    /* Windows Plugins use the Windows event call-back mechanism
       for events. (See PluginWindowProc) */
    return 0;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PluginWindowProc
 *
 * Handle the Windows window-event loop.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

LRESULT CALLBACK 
SimplePluginInstance::PluginWindowProc( HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam)
{
    SimplePluginInstance* inst = (SimplePluginInstance*) GetProp(hWnd, gInstanceLookupString);

    switch( Msg ) {
      case WM_PAINT: {
          PAINTSTRUCT paintStruct;
          HDC hdc;

          hdc = BeginPaint( hWnd, &paintStruct );
          TextOut(hdc, 0, 0, "Hello, World!", 15);

          EndPaint( hWnd, &paintStruct );
          break;
      }
      default: {
          inst->fPlatform.fDefaultWindowProc(hWnd, Msg, wParam, lParam);
      }
    }
    return 0;
}
#endif /* XP_PC */



/*------------------------------------------------------------------------------
 * Macintosh Implementations
 *----------------------------------------------------------------------------*/

#ifdef macintosh

PRBool	StartDraw(nsPluginWindow* window);
void 	EndDraw(nsPluginWindow* window);
void 	DoDraw(SimplePluginInstance* This);

CGrafPort 		gSavePort;
CGrafPtr		gOldPort;

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformNew
 *
 * Initialize any Platform-Specific instance data.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

void
SimplePluginInstance::PlatformNew(void)
{
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformDestroy
 *
 * Destroy any Platform-Specific instance data.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

nsresult
SimplePluginInstance::PlatformDestroy(void)
{
    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformSetWindow
 *
 * Perform platform-specific window operations
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

nsresult
SimplePluginInstance::PlatformSetWindow(nsPluginWindow* window)
{
    fWindow = window;
    if( StartDraw( window ) ) {
        DoDraw(This);
        EndDraw( window );
    }
    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * PlatformHandleEvent
 *
 * Handle platform-specific events.
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

int16
SimplePluginInstance::PlatformHandleEvent(nsPluginEvent* event)
{
    int16 eventHandled = FALSE;
	
    EventRecord* ev = (EventRecord*) event;
    if (This != NULL && event != NULL)
    {
        switch (ev->what)
        {
            /*
             * Draw ourselves on update events
             */
          case updateEvt:
            if( StartDraw( fWindow ) ) {
                DoDraw(This);
                EndDraw( fWindow );
            }
            eventHandled = true;
            break;
          default:
            break;
        }
    }
    return eventHandled;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * StartDraw
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

PRBool
SimplePluginInstance::StartDraw(nsPluginWindow* window)
{
    NP_Port* port;
    Rect clipRect;
    RGBColor  col;
	
    if (window == NULL)
        return FALSE;
    port = (NP_Port*) window->window;
    if (window->clipRect.left < window->clipRect.right)
    {
	/* Preserve the old port */
        GetPort((GrafPtr*)&gOldPort);
        SetPort((GrafPtr)port->port);
	/* Preserve the old drawing environment */
        gSavePort.portRect = port->port->portRect;
        gSavePort.txFont = port->port->txFont;
        gSavePort.txFace = port->port->txFace;
        gSavePort.txMode = port->port->txMode;
        gSavePort.rgbFgColor = port->port->rgbFgColor;
        gSavePort.rgbBkColor = port->port->rgbBkColor;
        GetClip(gSavePort.clipRgn);
	/* Setup our drawing environment */
        clipRect.top = window->clipRect.top + port->porty;
        clipRect.left = window->clipRect.left + port->portx;
        clipRect.bottom = window->clipRect.bottom + port->porty;
        clipRect.right = window->clipRect.right + port->portx;
        SetOrigin(port->portx,port->porty);
        ClipRect(&clipRect);
        clipRect.top = clipRect.left = 0;
        TextSize(12);
        TextFont(geneva);
        TextMode(srcCopy);
        col.red = col.green = col.blue = 0;
        RGBForeColor(&col);
        col.red = col.green = col.blue = 65000;
        RGBBackColor(&col);
        return TRUE;
    }
    else
        return FALSE;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * EndDraw
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

void
SimplePluginInstance::EndDraw(nsPluginWindow* window)
{
    CGrafPtr myPort;
    NP_Port* port = (NP_Port*) window->window;
    SetOrigin(gSavePort.portRect.left, gSavePort.portRect.top);
    SetClip(gSavePort.clipRgn);
    GetPort((GrafPtr*)&myPort);
    myPort->txFont = gSavePort.txFont;
    myPort->txFace = gSavePort.txFace;
    myPort->txMode = gSavePort.txMode;
    RGBForeColor(&gSavePort.rgbFgColor);
    RGBBackColor(&gSavePort.rgbBkColor);
    SetPort((GrafPtr)gOldPort);
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * DoDraw
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

void
SimplePluginInstance::DoDraw(void)
{
    Rect drawRect;
    drawRect.top = 0;
    drawRect.left = 0;
    drawRect.bottom = drawRect.top + fWindow->height;
    drawRect.right = drawRect.left + fWindow->width;
    EraseRect( &drawRect );
    MoveTo( 2, 12 );
    DrawString("\pHello, World!");
}

#endif /* macintosh */

/******************************************************************************/
