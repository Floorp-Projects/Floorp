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
#include <X11/Xlib.h>
#include <X11/Intrinsic.h>
#include <X11/StringDefs.h>
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

    SimplePlugin(nsISupports* aManager);
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

#ifdef NEW_PLUGIN_STREAM_API

    NS_IMETHOD
    NewStream(nsIPluginStreamListener** listener);

#else

    // (Corresponds to NPP_NewStream.)
    NS_IMETHOD
    NewStream(nsIPluginStreamPeer* peer, nsIPluginStream* *result);

#endif // NEW_PLUGIN_STREAM_API

    // (Corresponds to NPP_Print.)
    NS_IMETHOD
    Print(nsPluginPrint* platformPrint);

#ifndef NEW_PLUGIN_STREAM_API

    // (Corresponds to NPP_URLNotify.)
    NS_IMETHOD
    URLNotify(const char* url, const char* target,
              nsPluginReason reason, void* notifyData);

#endif // NEW_PLUGIN_STREAM_API

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

#ifdef XP_UNIX
	static void Redraw(Widget w, XtPointer closure, XEvent *event);
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

#ifdef NEW_PLUGIN_STREAM_API

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
    OnStartBinding(const char* url, nsIPluginStreamInfo* pluginInfo);

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
    OnDataAvailable(const char* url, nsIInputStream* input,
                    PRUint32 offset, PRUint32 length, 
					nsIPluginStreamInfo* pluginInfo);

    NS_IMETHOD
    OnFileAvailable(const char* url, const char* fileName);

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
    OnStopBinding(const char* url, nsresult status, nsIPluginStreamInfo* pluginInfo);

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

#else // !NEW_PLUGIN_STREAM_API

class SimplePluginStream : public nsIPluginStream {
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIBaseStream:

    /** Close the stream. */
    NS_IMETHOD
    Close(void);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIOutputStream:

    /** Write data into the stream.
     *  @param aBuf the buffer into which the data is read
     *  @param aOffset the start offset of the data
     *  @param aCount the maximum number of bytes to read
     *  @param errorResult the error code if an error occurs
     *  @return number of bytes read or -1 if error
     */   
    NS_IMETHOD
    Write(const char* aBuf, PRUint32 aOffset, PRUint32 aCount, PRUint32 *aWriteCount); 

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginStream:

    // (Corresponds to NPP_NewStream's stype return parameter.)
    NS_IMETHOD
    GetStreamType(nsPluginStreamType *result);

    // (Corresponds to NPP_StreamAsFile.)
    NS_IMETHOD
    AsFile(const char* fname);

    ////////////////////////////////////////////////////////////////////////////
    // nsSimplePluginStream specific methods:

    SimplePluginStream(nsIPluginStreamPeer* peer);
    virtual ~SimplePluginStream(void);

    NS_DECL_ISUPPORTS

protected:
    nsIPluginStreamPeer*        fPeer;
};

#endif // !NEW_PLUGIN_STREAM_API

// Interface IDs we'll need:
static NS_DEFINE_IID(kIPluginIID, NS_IPLUGIN_IID);
static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID);
static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);
static NS_DEFINE_CID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_IID(kIServiceManagerIID, NS_ISERVICEMANAGER_IID);

#ifdef NEW_PLUGIN_STREAM_API
static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);
#else // !NEW_PLUGIN_STREAM_API
static NS_DEFINE_IID(kIPluginStreamIID, NS_IPLUGINSTREAM_IID);
#endif // !NEW_PLUGIN_STREAM_API

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

SimplePlugin::SimplePlugin(nsISupports* aService)
{
    NS_INIT_REFCNT();

	nsISupports* result;
	aService->QueryInterface(kIServiceManagerIID, (void**)&mServiceManager);

	if(mServiceManager->GetService(kCPluginManagerCID, kIPluginManagerIID, &result, NULL) != NS_OK)
		return;

	result->QueryInterface(kIPluginManagerIID, (void**) &mPluginManager);
 
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
    if (NULL == aInstancePtr) {
        return NS_ERROR_NULL_POINTER; 
    }  
    static NS_DEFINE_IID(kIPluginIID, NS_IPLUGIN_IID); 
    if (aIID.Equals(kIPluginIID)) {
        *aInstancePtr = (void*) this; 
        AddRef(); 
        return NS_OK; 
    } 
    static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID); 
    if (aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (void*) ((nsISupports*)this); 
        AddRef(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE; 
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
NSGetFactory(const nsCID &aClass, nsISupports* serviceMgr, nsIFactory **aFactory)
{
    if (aClass.Equals(kIPluginIID)) {
        if (gPlugin) {
            *aFactory = gPlugin;
            return NS_OK;
        }

        SimplePlugin* fact = new SimplePlugin(serviceMgr);
        if (fact == NULL) 
            return NS_ERROR_OUT_OF_MEMORY;
        fact->AddRef();
        gPlugin = fact;
        *aFactory = fact;
        return NS_OK;
    }
    return NS_ERROR_FAILURE;    // XXX right error?
}

extern "C" NS_EXPORT PRBool
NSCanUnload(void)
{
    return gPluginObjectCount == 1 && !gPluginLocked;
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
    return NS_OK;
}

NS_METHOD
SimplePlugin::Shutdown(void)
{
    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * GetMIMEDescription:
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

NS_METHOD
SimplePlugin::GetMIMEDescription(const char* *result)
{
    *result = "application/x-simple-plugin:smp:Simple LiveConnect Sample Plug-in";
    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NPP_GetValue:
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

#define PLUGIN_NAME             "Simple LiveConnect Sample Plug-in"
#define PLUGIN_DESCRIPTION      "Demonstrates a simple LiveConnected plug-in."

NS_METHOD
SimplePlugin::GetValue(nsPluginVariable variable, void *value)
{
    nsresult err = NS_OK;
    if (variable == nsPluginVariable_NameString)
        *((char **)value) = PLUGIN_NAME;
    else if (variable == nsPluginVariable_DescriptionString)
        *((char **)value) = PLUGIN_DESCRIPTION;
    else
        err = NS_ERROR_FAILURE;

    return err;
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
    fPeer = peer;
    peer->AddRef();
    nsresult err = peer->GetMode(&fMode);
    if (err) return err;
    PlatformNew(); 	/* Call Platform-specific initializations */
    return NS_OK;
}

NS_METHOD
SimplePluginInstance::GetPeer(nsIPluginInstancePeer* *result)
{
    fPeer->AddRef();
    *result = fPeer;
    return NS_OK;
}

NS_METHOD
SimplePluginInstance::Start(void)
{

#ifdef NEW_PLUGIN_STREAM_API
    // Try getting some streams:
    gPlugin->GetPluginManager()->GetURL(this, "http://warp", NULL,
                                        new SimplePluginStreamListener(this, "http://warp (Normal)"));

    gPlugin->GetPluginManager()->GetURL(this, "http://home.netscape.com", NULL,
                                        new SimplePluginStreamListener(this, "http://home.netscape.com (AsFile)"));

    gPlugin->GetPluginManager()->GetURL(this, "http://warp/java", NULL,
                                        new SimplePluginStreamListener(this, "http://warp/java (AsFileOnly)"));

    gPlugin->GetPluginManager()->GetURL(this, "http://warp/java/oji", NULL,
                                        new SimplePluginStreamListener(this, "http://warp/java/oji (Seek)"));
#endif

    return NS_OK;
}

NS_METHOD
SimplePluginInstance::Stop(void)
{
    return NS_OK;
}

NS_METHOD
SimplePluginInstance::Destroy(void)
{
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

#ifdef NEW_PLUGIN_STREAM_API

NS_METHOD
SimplePluginInstance::NewStream(nsIPluginStreamListener** listener)
{
	if(listener != NULL)
		*listener = new SimplePluginStreamListener(this, "http://warp");

	return NS_OK;
}

#else

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NewStream:
 * Notifies an instance of a new data stream and returns an error value. 
 * 
 * NewStream notifies the instance denoted by instance of the creation of
 * a new stream specifed by stream. The NPStream* pointer is valid until the
 * stream is destroyed. The MIME type of the stream is provided by the
 * parameter type. 
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

NS_METHOD
SimplePluginInstance::NewStream(nsIPluginStreamPeer* peer, nsIPluginStream* *result)
{ 
    SimplePluginStream* strm = new SimplePluginStream(peer);
    if (strm == NULL) 
        return NS_ERROR_OUT_OF_MEMORY;
    strm->AddRef();
    *result = strm;
    return NS_OK;
}

#endif // NEW_PLUGIN_STREAM_API

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NPP_Print:
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

NS_METHOD
SimplePluginInstance::Print(nsPluginPrint* printInfo)
{

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

        void* platformPrint =
            printInfo->print.fullPrint.platformPrint;
        PRBool printOne =
            printInfo->print.fullPrint.printOne;
			
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

        nsPluginWindow* printWindow =
            &(printInfo->print.embedPrint.window);
        void* platformPrint =
            printInfo->print.embedPrint.platformPrint;
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
    *handled = (PRBool)PlatformHandleEvent(event);
    return NS_OK;
}

#ifndef NEW_PLUGIN_STREAM_API

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * URLNotify:
 * Notifies the instance of the completion of a URL request. 
 * 
 * URLNotify is called when Netscape completes a GetURLNotify or
 * PostURLNotify request, to inform the plug-in that the request,
 * identified by url, has completed for the reason specified by reason. The most
 * common reason code is NPRES_DONE, indicating simply that the request
 * completed normally. Other possible reason codes are NPRES_USER_BREAK,
 * indicating that the request was halted due to a user action (for example,
 * clicking the "Stop" button), and NPRES_NETWORK_ERR, indicating that the
 * request could not be completed (for example, because the URL could not be
 * found). The complete list of reason codes is found in npapi.h. 
 * 
 * The parameter notifyData is the same plug-in-private value passed as an
 * argument to the corresponding GetURLNotify or PostURLNotify
 * call, and can be used by your plug-in to uniquely identify the request. 
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

NS_METHOD
SimplePluginInstance::URLNotify(const char* url, const char* target,
                                nsPluginReason reason, void* notifyData)
{
    // Not used in the Simple plugin
    return NS_OK;
}

#endif // NEW_PLUGIN_STREAM_API

NS_METHOD
SimplePluginInstance::GetValue(nsPluginInstanceVariable variable, void *value)
{
    return NS_ERROR_FAILURE;
}

#ifdef NEW_PLUGIN_STREAM_API

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
SimplePluginStreamListener::OnStartBinding(const char* url, 
										   nsIPluginStreamInfo* pluginInfo)
{
    char msg[256];
    sprintf(msg, "### Opening plugin stream for %s\n", fMessageName);
    return NS_OK;
}

NS_METHOD
SimplePluginStreamListener::OnDataAvailable(const char* url, nsIInputStream* input,
                                            PRUint32 offset, PRUint32 length,
											nsIPluginStreamInfo* pluginInfo)
{
    if (strcmp(url, "http://warp/java/oji/") == 0 && offset != 0) {
        // Try closing the stream prematurely
        input->Close();
        return NS_OK;
    }

    char* buffer = new char[length];
    if (buffer) {
        PRUint32 amountRead = 0;
        nsresult rslt = input->Read(buffer, offset, length, &amountRead);
        if (rslt == NS_OK) {
            char msg[256];
            sprintf(msg, "### Received %d bytes at %d for %s\n", length, offset, fMessageName);
        }
        delete buffer;
    }
    return NS_OK;
}

NS_METHOD
SimplePluginStreamListener::OnFileAvailable(const char* url, const char* fileName)
{
    char msg[256];
    sprintf(msg, "### File available for %s: %s\n", fMessageName, fileName);
    return NS_OK;
}

NS_METHOD
SimplePluginStreamListener::OnStopBinding(const char* url, nsresult status, 
										  nsIPluginStreamInfo* pluginInfo)
{
    char msg[256];
    sprintf(msg, "### Closing plugin stream for %s\n", fMessageName);
    return NS_OK;
}

NS_METHOD
SimplePluginStreamListener::OnNotify(const char* url, nsresult status)
{
	return NS_OK;
}

NS_METHOD
SimplePluginStreamListener::GetStreamType(nsPluginStreamType *result)
{
	*result = nsPluginStreamType_Normal;
	return NS_OK;
}

#else // !NEW_PLUGIN_STREAM_API

////////////////////////////////////////////////////////////////////////////////
// SimplePluginStream Methods
////////////////////////////////////////////////////////////////////////////////

SimplePluginStream::SimplePluginStream(nsIPluginStreamPeer* peer)
    : fPeer(peer)
{
    gPluginObjectCount++;
    NS_INIT_REFCNT();
}

SimplePluginStream::~SimplePluginStream(void)
{
    gPluginObjectCount--;
}

// These macros produce simple version of QueryInterface and AddRef.
// See the nsISupports.h header file for details.

NS_IMPL_QUERY_INTERFACE(SimplePluginStream, kIPluginStreamIID);
NS_IMPL_ADDREF(SimplePluginStream);
NS_IMPL_RELEASE(SimplePluginStream);

NS_METHOD
SimplePluginStream::Close(void)
{
    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * NPP_Write:
 * Delivers data from a stream and returns the number of bytes written. 
 * 
 * NPP_Write is called after a call to NPP_NewStream in which the plug-in
 * requested a normal-mode stream, in which the data in the stream is delivered
 * progressively over a series of calls to NPP_WriteReady and NPP_Write. The
 * function delivers a buffer buf of len bytes of data from the stream identified
 * by stream to the instance. The parameter offset is the logical position of
 * buf from the beginning of the data in the stream. 
 * 
 * The function returns the number of bytes written (consumed by the instance).
 * A negative return value causes an error on the stream, which will
 * subsequently be destroyed via a call to NPP_DestroyStream. 
 * 
 * Note that a plug-in must consume at least as many bytes as it indicated in the
 * preceeding NPP_WriteReady call. All data consumed must be either processed
 * immediately or copied to memory allocated by the plug-in: the buf parameter
 * is not persistent. 
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

NS_METHOD
SimplePluginStream::Write(const char* aBuf, PRUint32 aOffset, PRUint32 aCount, PRUint32 *aWriteCount)
{
    PR_ASSERT(aOffset == 0);    // XXX need to handle the non-sequential write case
    *aWriteCount = aCount;		/* The number of bytes accepted */
    return NS_OK;
}

/*******************************************************************************/

NS_METHOD
SimplePluginStream::GetStreamType(nsPluginStreamType *result)
{
    // XXX these should become subclasses
    *result = nsPluginStreamType_Normal;
    return NS_OK;
}

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * AsFile:
 * Provides a local file name for the data from a stream. 
 * 
 * AsFile provides the instance with a full path to a local file,
 * identified by fname, for the stream specified by stream. NPP_StreamAsFile is
 * called as a result of the plug-in requesting mode NP_ASFILEONLY or
 * NP_ASFILE in a previous call to NPP_NewStream. If an error occurs while
 * retrieving the data or writing the file, fname may be NULL. 
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

NS_METHOD
SimplePluginStream::AsFile(const char* fname)
{ 
    return NS_OK;
}

#endif // !NEW_PLUGIN_STREAM_API


/*******************************************************************************
 * SECTION 5 - Utility Method Implementations
 *******************************************************************************


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
    Widget netscape_widget;

    fPlatform.window = (Window) window->window;
    fPlatform.x = window->x;
    fPlatform.y = window->y;
    fPlatform.width = window->width;
    fPlatform.height = window->height;
    fPlatform.display = ((nsPluginSetWindowCallbackStruct *)window->ws_info)->display;
	
    netscape_widget = XtWindowToWidget(fPlatform.display, fPlatform.window);
    XtAddEventHandler(netscape_widget, ExposureMask, FALSE, (XtEventHandler)Redraw, this);
    Redraw(netscape_widget, (XtPointer)this, NULL);
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

/*+++++++++++++++++++++++++++++++++++++++++++++++++
 * Redraw
 +++++++++++++++++++++++++++++++++++++++++++++++++*/

void
SimplePluginInstance::Redraw(Widget w, XtPointer closure, XEvent *event)
{
    GC gc;
    XGCValues gcv;
    const char* text = "Hello World";
	SimplePluginInstance* inst = (SimplePluginInstance*)closure;

    XtVaGetValues(w, XtNbackground, &gcv.background,
                  XtNforeground, &gcv.foreground, 0);
    gc = XCreateGC(inst->fPlatform.display, inst->fPlatform.window, 
                   GCForeground|GCBackground, &gcv);
    XDrawRectangle(inst->fPlatform.display, inst->fPlatform.window, gc, 
                   0, 0, inst->fPlatform.width-1, inst->fPlatform.height-1);
    XDrawString(inst->fPlatform.display, inst->fPlatform.window, gc, 
                inst->fPlatform.width/2 - 100, inst->fPlatform.height/2,
                text, strlen(text));
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
