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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

/*******************************************************************************
 * npsimple.cpp
 ******************************************************************************
 * Simple Sample Plugin
 * Copyright (c) 1996 Netscape Communications. All rights reserved.
 ******************************************************************************
 * OVERVIEW
 * --------
 * Section 1 - Includes
 * Section 2 - Instance Structs
 * Section 3 - API Plugin Implementations
 * Section 4 - Utility Method Implementations
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
#include "nsIGenericFactory.h"
#include "nsMemory.h"
#include "nsString.h"
#include "simpleCID.h"

#include "nsISimplePluginInstance.h"
#include "nsIScriptablePlugin.h"

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
#include <gdk/gdk.h>
#include <gdk/gdkprivate.h>
#include <gtk/gtk.h>
#include <gdksuperwin.h>
#include <gtkmozbox.h>
#endif /* XP_UNIX */

#ifdef XP_UNIX

gboolean draw (GtkWidget *widget, GdkEventExpose *event, gpointer data);

#endif


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

#if defined(XP_PC) && !defined(XP_OS2)          // XXXX OS2TODO
typedef struct _PlatformInstance
{
    HWND		fhWnd;
    WNDPROC		fDefaultWindowProc;
} PlatformInstance;

/*------------------------------------------------------------------------------
 * UNIX PlatformInstance
 *----------------------------------------------------------------------------*/

#elif defined(XP_UNIX)
typedef struct _PlatformInstance
{
    Window 		       window;
    GtkWidget         *moz_box;
    GdkSuperWin       *superwin;
    GtkWidget         *label;
    Display *		   display;
    uint32 		       x, y;
    uint32 		       width, height;
} PlatformInstance;

/*------------------------------------------------------------------------------
 * Macintosh PlatformInstance
 *----------------------------------------------------------------------------*/

#elif defined(XP_MAC)
typedef struct _PlatformInstance
{
    int			placeholder;
} PlatformInstance;

/*------------------------------------------------------------------------------
 * Stub PlatformInstance
 *----------------------------------------------------------------------------*/

#else
typedef struct _PlatformInstance
{
    int			placeholder;
} PlatformInstance;
#endif /* end Section 2 */


// Define constants for easy use
static NS_DEFINE_CID(kSimplePluginCID, NS_SIMPLEPLUGIN_CID);
static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);

#define PLUGIN_MIME_TYPE "application/x-simple"

static const char kPluginName[] = "Simple Sample Plug-in";
static const char kPluginDescription[] = "Demonstrates a simple plug-in.";

static const char* kMimeTypes[] = {
    PLUGIN_MIME_TYPE
};

static const char* kMimeDescriptions[] = {
    "Simple Sample Plug-in"
};

static const char* kFileExtensions[] = {
    "smp"
};

static const PRInt32 kNumMimeTypes = sizeof(kMimeTypes) / sizeof(*kMimeTypes);



////////////////////////////////////////////////////////////////////////////////
// Simple Plugin Classes
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SimplePluginInstance represents an instance of the simple plugin class.

class SimplePluginInstance : 
    public nsIPluginInstance, 
    public nsIScriptablePlugin,
    public nsISimplePluginInstance {

public:
    ////////////////////////////////////////////////////////////////////////////
    // for implementing a generic module
    static NS_METHOD
    Create(nsISupports* aOuter, REFNSIID aIID, void** aResult);

    static NS_METHOD
    RegisterSelf(nsIComponentManager* aCompMgr,
                 nsIFile* aPath,
                 const char* aRegistryLocation,
                 const char* aComponentType,
                 const nsModuleComponentInfo *info);

    static NS_METHOD
    UnregisterSelf(nsIComponentManager* aCompMgr,
                   nsIFile* aPath,
                   const char* aRegistryLocation,
                   const nsModuleComponentInfo *info);


    NS_DECL_ISUPPORTS
    NS_DECL_NSISCRIPTABLEPLUGIN
    NS_DECL_NSISIMPLEPLUGININSTANCE

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

    void            PlatformNew(void);
    nsresult        PlatformDestroy(void);
    nsresult    	PlatformSetWindow(nsPluginWindow* window);
    PRInt16         PlatformHandleEvent(nsPluginEvent* event);

    void SetMode(nsPluginMode mode) { fMode = mode; }

#ifdef XP_UNIX
    NS_IMETHOD Repaint(void);
#endif

#if defined(XP_PC) && !defined(XP_OS2)
    static LRESULT CALLBACK 
    PluginWindowProc( HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
#endif

    char*                       fText;

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

// XXXwaterson document!
static nsModuleComponentInfo gComponentInfo[] = {
    { "Simple Plugin",
      NS_SIMPLEPLUGIN_CID,
      NS_INLINE_PLUGIN_CONTRACTID_PREFIX PLUGIN_MIME_TYPE,
      SimplePluginInstance::Create,
      SimplePluginInstance::RegisterSelf,
      SimplePluginInstance::UnregisterSelf },
};

NS_IMPL_NSGETMODULE(SimplePlugin, gComponentInfo);



////////////////////////////////////////////////////////////////////////////////
// nsIScriptablePlugin methods
////////////////////////////////////////////////////////////////////////////////
NS_METHOD
SimplePluginInstance::GetScriptablePeer(void **aScriptablePeer)
{
   // We implement the interface we want to be scriptable by
   // (nsISimplePluginInstance) so we just return this.

   // NOTE if this function returns something other than
   // nsIPluginInstance, then that object will also need to implement
   // nsISecurityCheckedComponent to be scriptable.  The security
   // system knows to special-case nsIPluginInstance when checking
   // security, but in general, XPCOM components must implement
   // nsISecurityCheckedComponent to be scriptable from content
   // javascript.

   *aScriptablePeer = NS_STATIC_CAST(nsISimplePluginInstance *, this);
   NS_ADDREF(NS_STATIC_CAST(nsISimplePluginInstance *, *aScriptablePeer));
   return NS_OK;
}

NS_METHOD
SimplePluginInstance::GetScriptableInterface(nsIID **aScriptableInterface)
{
  *aScriptableInterface = (nsIID *)nsMemory::Alloc(sizeof(nsIID));
  NS_ENSURE_TRUE(*aScriptableInterface, NS_ERROR_OUT_OF_MEMORY);

  **aScriptableInterface = NS_GET_IID(nsISimplePluginInstance);

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// SimplePluginInstance Methods
////////////////////////////////////////////////////////////////////////////////

NS_METHOD
SimplePluginInstance::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    SimplePluginInstance* plugin = new SimplePluginInstance();
    if (! plugin)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    NS_ADDREF(plugin);
    rv = plugin->QueryInterface(aIID, aResult);
    NS_RELEASE(plugin);
    return rv;
}

NS_METHOD
SimplePluginInstance::RegisterSelf(nsIComponentManager* aCompMgr,
                                   nsIFile* aPath,
                                   const char* aRegistryLocation,
                                   const char* aComponentType,
                                   const nsModuleComponentInfo *info)
{
    nsresult rv;

    nsIPluginManager* pm;
    rv = nsServiceManager::GetService(kPluginManagerCID, NS_GET_IID(nsIPluginManager),
                                      NS_REINTERPRET_CAST(nsISupports**, &pm));
    if (NS_SUCCEEDED(rv)) {
        rv = pm->RegisterPlugin(kSimplePluginCID,
                                kPluginName,
                                kPluginDescription,
                                kMimeTypes,
                                kMimeDescriptions,
                                kFileExtensions,
                                kNumMimeTypes);

        NS_RELEASE(pm);
    }

    return rv;
}


NS_METHOD
SimplePluginInstance::UnregisterSelf(nsIComponentManager* aCompMgr,
                                     nsIFile* aPath,
                                     const char* aRegistryLocation,
                                     const nsModuleComponentInfo *info)
{
    nsresult rv;

    nsIPluginManager* pm;
    rv = nsServiceManager::GetService(kPluginManagerCID, NS_GET_IID(nsIPluginManager),
                                      NS_REINTERPRET_CAST(nsISupports**, &pm));
    if (NS_SUCCEEDED(rv)) {
        rv = pm->UnregisterPlugin(kSimplePluginCID);
        NS_RELEASE(pm);
    }

    return rv;
}


SimplePluginInstance::SimplePluginInstance(void)
    : fText(NULL), fPeer(NULL), fWindow(NULL), fMode(nsPluginMode_Embedded)
{
    NS_INIT_REFCNT();

    static const char text[] = "Hello World!";
    fText = (char*) nsMemory::Clone(text, sizeof(text));

#ifdef XP_UNIX
    fPlatform.moz_box = nsnull;
    fPlatform.superwin = nsnull;
    fPlatform.label = nsnull;
#endif

}

SimplePluginInstance::~SimplePluginInstance(void)
{
    if(fText)
        nsMemory::Free(fText);
    PlatformDestroy(); // Perform platform specific cleanup
}

// These macros produce simple version of QueryInterface and AddRef.
// See the nsISupports.h header file for details.

NS_IMPL_ISUPPORTS3(SimplePluginInstance, nsIPluginInstance, nsISimplePluginInstance, nsIScriptablePlugin)

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

   result = peer->QueryInterface(NS_GET_IID(nsIPluginTagInfo), (void **)&taginfo);

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
    {
        SimplePluginStreamListener* sl = 
                new SimplePluginStreamListener(this, "http://www.mozilla.org");
        if(!sl)
            return NS_ERROR_UNEXPECTED;
        sl->AddRef();
        *listener = sl;
    }
    
    return NS_OK;
}


/* attribute string text; */
NS_IMETHODIMP SimplePluginInstance::GetText(char * *aText)
{
#ifdef NS_DEBUG
    printf("SimplePluginInstance::GetText\n");
#endif
    
    if(!fText)
    {
        *aText = NULL;
        return NS_OK;        
    }
    char* ptr = *aText = (char*) nsMemory::Clone(fText, strlen(fText)+1);
    return ptr ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}
NS_IMETHODIMP SimplePluginInstance::SetText(const char * aText)
{
#ifdef NS_DEBUG
    printf("SimplePluginInstance::SetText\n");
#endif

    if(fText)
    {
        nsMemory::Free(fText);
        fText = NULL;
    }

    if(aText)
    {
        fText = (char*) nsMemory::Clone(aText, strlen(aText)+1);
        if(!fText)
            return NS_ERROR_OUT_OF_MEMORY;

#if defined(XP_PC) && !defined(XP_OS2)
        if(fPlatform.fhWnd) {
            InvalidateRect( fPlatform.fhWnd, NULL, TRUE );
            UpdateWindow( fPlatform.fhWnd );
        }
#endif

#ifdef XP_UNIX
        // force a redraw
        Repaint();
#endif

    }

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
    NS_INIT_REFCNT();
    char msg[256];
    sprintf(msg, "### Creating SimplePluginStreamListener for %s\n", fMessageName);
}

SimplePluginStreamListener::~SimplePluginStreamListener(void)
{
    char msg[256];
    sprintf(msg, "### Destroying SimplePluginStreamListener for %s\n", fMessageName);
}

// This macro produces a simple version of QueryInterface, AddRef and Release.
// See the nsISupports.h header file for details.

NS_IMPL_ISUPPORTS1(SimplePluginStreamListener, nsIPluginStreamListener);

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
 * SECTION 4 - Utility Method Implementations
 *******************************************************************************/

/*------------------------------------------------------------------------------
 * Platform-Specific Implemenations
 *------------------------------------------------------------------------------
 * UNIX Implementations
 *----------------------------------------------------------------------------*/

#if defined(XP_UNIX)

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
    gtk_widget_destroy(fPlatform.moz_box);
    fPlatform.moz_box = 0;
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
#ifdef NS_DEBUG
    printf("SimplePluginInstance::PlatformSetWindow\n");
#endif

    if (window == NULL || window->window == NULL)
        return NS_ERROR_NULL_POINTER;

    GdkWindow *win = gdk_window_lookup((XID)window->window);

    if ( fPlatform.superwin && fPlatform.superwin->bin_window == win )
        return NS_OK;
    
    // should we destroy fPlatform.superwin ??
    
    fPlatform.superwin = gdk_superwin_new(win, 0, 0, window->width, window->height);

    // a little cleanup
    if (fPlatform.label)
        gtk_widget_destroy(fPlatform.label);
    if (fPlatform.moz_box)
        gtk_widget_destroy(fPlatform.moz_box);

    // create a containing mozbox and a label to put in it
    fPlatform.moz_box = gtk_mozbox_new(fPlatform.superwin->bin_window);
    fPlatform.label = gtk_label_new(fText);
    gtk_container_add(GTK_CONTAINER(fPlatform.moz_box), fPlatform.label);

    // grow the label to fit the entire mozbox
    gtk_widget_set_usize(fPlatform.label, window->width, window->height);

    // connect to expose events
    gtk_signal_connect (GTK_OBJECT(fPlatform.label), "expose_event",
                        GTK_SIGNAL_FUNC(draw), this);

    gtk_widget_show(fPlatform.label);
    gtk_widget_show(fPlatform.moz_box);

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

NS_IMETHODIMP
SimplePluginInstance::Repaint(void)
{
#ifdef DEBUG
    printf("SimplePluginInstance::Repaint()\n");
#endif
    
    if ( !fPlatform.moz_box || !fPlatform.label )
        return NS_ERROR_FAILURE;

    // Set the label text
    gtk_label_set_text(GTK_LABEL(fPlatform.label), fText);

    // show the new label
    gtk_widget_show(fPlatform.label);
    gtk_widget_show(fPlatform.moz_box);

    return NS_OK;
}

gboolean draw (GtkWidget *widget, GdkEventExpose *event, gpointer data)
{
    SimplePluginInstance * pthis = (SimplePluginInstance *)data;

    pthis->Repaint();
    return TRUE;
}

/*------------------------------------------------------------------------------
 * Windows Implementations
 *----------------------------------------------------------------------------*/

#elif defined(XP_PC) && !defined(XP_OS2)
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

          if(paintStruct.fErase)
            FillRect(hdc, &paintStruct.rcPaint, 
                     (HBRUSH) GetStockObject(WHITE_BRUSH)); 

          if(inst->fText) {
            RECT rcWnd;
            GetWindowRect(hWnd, &rcWnd);
            SetTextAlign(hdc, TA_CENTER);
            TextOut(hdc, (rcWnd.right-rcWnd.left)/2, (rcWnd.bottom-rcWnd.top)/2, inst->fText, strlen(inst->fText));
          }

          EndPaint( hWnd, &paintStruct );
          break;
      }
      default: {
          CallWindowProc(inst->fPlatform.fDefaultWindowProc, hWnd, Msg, wParam, lParam);
      }
    }
    return 0;
}



/*------------------------------------------------------------------------------
 * Macintosh Implementations
 *----------------------------------------------------------------------------*/

#elif defined(XP_MAC)

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



/*------------------------------------------------------------------------------
 * Stub Implementations
 *----------------------------------------------------------------------------*/

#else

void
SimplePluginInstance::PlatformNew(void)
{
}

nsresult
SimplePluginInstance::PlatformDestroy(void)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

nsresult
SimplePluginInstance::PlatformSetWindow(nsPluginWindow* window)
{
    return NS_ERROR_NOT_IMPLEMENTED;
}

int16
SimplePluginInstance::PlatformHandleEvent(nsPluginEvent* event)
{
    int16 eventHandled = FALSE;
    return eventHandled;
}

#endif /* end Section 5 */

/******************************************************************************/
