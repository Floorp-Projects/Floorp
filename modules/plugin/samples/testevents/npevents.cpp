/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code was developed by ActiveState.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Neil Hodgson <nhodgson@bigpond.net.au>
 *   Mark Hammond <MarkH@ActiveState.com>
 */

// npevents.cxx
// Demonstration plugin for Mozilla that handles events, focus and keystrokes.
// See README.txt for more details.

#include "nsplugin.h"
#include "nsIServiceManager.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsString.h"
#include "nsIAllocator.h"

#include "nsIEventsPluginInstance.h"

#include <stdio.h> 
#include <string.h> 

#if defined (XP_WIN)

#include <windows.h>

#elif defined (XP_UNIX)

#include <gdk/gdkprivate.h> 
#include <gtk/gtk.h> 
#include <gdk/gdkkeysyms.h> 
#include <gtkmozbox.h> 

#endif 

#define EVENTSPLUGIN_DEBUG

/**
 * {CB2EF72A-856A-4818-8E72-3439395E335F}
 */
#define NS_EVENTSAMPLEPLUGIN_CID { 0xcb2ef72a, 0x856a, 0x4818, { 0x8e, 0x72, 0x34, 0x39, 0x39, 0x5e, 0x33, 0x5f } }

#if defined(XP_UNIX)
typedef struct _PlatformInstance {
	GtkWidget *moz_box;
	Display * display;
	uint32 x, y;
	uint32 width, height;
}
PlatformInstance;

typedef GtkWidget* WinID;

#endif // XP_UNIX

#if defined(XP_WIN)
typedef struct _PlatformInstance
{
	WNDPROC fOldChildWindowProc; // The original WNDPROC of the edit control.
	WNDPROC fParentWindowProc; // The Mozilla WNDPROC for main (parent) window.
} PlatformInstance;

// Unique string for associating a pointer with our WNDPROC
static const char* gInstanceLookupString = "instance->pdata";

typedef HWND WinID;

#endif // XP_WIN

static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_CID(kEventsPluginCID, NS_EVENTSAMPLEPLUGIN_CID);

const char *kPluginName = "Events Sample Plug-in";
const char *kPluginDescription = "Sample plugin that demonstrates events, focus and keystrokes.";
#define PLUGIN_MIME_TYPE "application/x-events-sample-plugin"

static const char* kMimeTypes[] = {
    PLUGIN_MIME_TYPE
};

static const char* kMimeDescriptions[] = {
    "Event Sample Plug-in"
};

static const char* kFileExtensions[] = {
    "smpev"
};

static const PRInt32 kNumMimeTypes = sizeof(kMimeTypes) / sizeof(*kMimeTypes);

////////////////////////////////////////////////////////////////////////////////
// EventsPluginInstance represents an instance of the EventsPlugin class.

class EventsPluginInstance :
	public nsIPluginInstance,
	public nsIEventsSampleInstance {
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
	NS_DECL_NSIEVENTSSAMPLEINSTANCE

	////////////////////////////////////////////////////////////////////////////
	// from nsIEventHandler:

	NS_IMETHOD HandleEvent(nsPluginEvent* event, PRBool* handled);

	////////////////////////////////////////////////////////////////////////////
	// from nsIPluginInstance:

	NS_IMETHOD Initialize(nsIPluginInstancePeer *peer);

	// Required backpointer to the peer.
	NS_IMETHOD GetPeer(nsIPluginInstancePeer **result);

	NS_IMETHOD Start(void);

	NS_IMETHOD Stop(void);

	NS_IMETHOD Destroy(void);

	NS_IMETHOD SetWindow(nsPluginWindow* window);

	NS_IMETHOD NewStream(nsIPluginStreamListener** listener);

	NS_IMETHOD Print(nsPluginPrint* platformPrint);

	NS_IMETHOD GetValue(nsPluginInstanceVariable variable, void *value);

	////////////////////////////////////////////////////////////////////////////
	// EventsPluginInstance specific methods:

	EventsPluginInstance();
	virtual ~EventsPluginInstance();

	void PlatformNew(void);
	nsresult PlatformDestroy(void);
	void PlatformResetWindow();
	PRInt16 PlatformHandleEvent(nsPluginEvent* event);
	void PlatformResizeWindow(nsPluginWindow* window);
	nsresult PlatformCreateWindow(nsPluginWindow* window);

	void SetMode(nsPluginMode mode) { fMode = mode; }

protected:
	////////////////////////////////////////////////////////////////////////////
	// Implementation methods
	nsresult DoSetWindow(nsPluginWindow* window);

	// Data
	nsCOMPtr<nsIPluginInstancePeer> fPeer;
	nsPluginWindow *fWindow; // no nsCOMPtr as not an interface!
	nsPluginMode fMode;
	PlatformInstance fPlatform;

	WinID wMain; // The window created by Mozilla for us.
	WinID wChild; // The window we create as a child of the nsWindow.

	////////////////////////////////////////////////////////////////////////////
	// Platform specific helpers
#ifdef XP_WIN
	static LRESULT CALLBACK WndProcChild(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam);
#endif
};

////////////////////////////////////////////////////////////////////////////////
// EventsPluginStream represents the stream used by EvMozs
// to receive data from the browser.

class EventsPluginStreamListener : public nsIPluginStreamListener {
public:

	NS_DECL_ISUPPORTS

	////////////////////////////////////////////////////////////////////////////
	// from nsIPluginStreamListener:

	// Notify the observer that the URL has started to load.  This method is
	// called only once, at the beginning of a URL load.
	NS_IMETHOD OnStartBinding(nsIPluginStreamInfo* pluginInfo);

	// Notify the client that data is available in the input stream.  This method is
	// called whenver data is written into the input stream by the networking library.
	NS_IMETHOD OnDataAvailable(nsIPluginStreamInfo* pluginInfo,
	                           nsIInputStream* input,
	                           PRUint32 length);

	NS_IMETHOD OnFileAvailable(nsIPluginStreamInfo* pluginInfo, const char* fileName);

	// Notify the observer that the URL has finished loading.  This method is
	// called once when the networking library has finished processing the
	// URL transaction initiatied via the nsINetService::Open(...) call.
	NS_IMETHOD OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status);

	NS_IMETHOD OnNotify(const char* url, nsresult status);

	NS_IMETHOD GetStreamType(nsPluginStreamType *result);

	////////////////////////////////////////////////////////////////////////////
	// EventsPluginStreamListener specific methods:

	EventsPluginStreamListener(EventsPluginInstance *inst_, const char* url);
	virtual ~EventsPluginStreamListener(void);

protected:
	const char* fMessageName;
	EventsPluginInstance *inst;
};

// The module loader information.
static nsModuleComponentInfo gComponentInfo[] = {
    { "Events Sample Plugin",
      NS_EVENTSAMPLEPLUGIN_CID,
      NS_INLINE_PLUGIN_CONTRACTID_PREFIX PLUGIN_MIME_TYPE,
      EventsPluginInstance::Create,
      EventsPluginInstance::RegisterSelf,
      EventsPluginInstance::UnregisterSelf },
};

NS_IMPL_NSGETMODULE(EventsPlugin, gComponentInfo);


////////////////////////////////////////////////////////////////////////////////
// EventsPluginInstance static Methods
////////////////////////////////////////////////////////////////////////////////

NS_METHOD
EventsPluginInstance::Create(nsISupports* aOuter, REFNSIID aIID, void** aResult)
{
    NS_PRECONDITION(aOuter == nsnull, "no aggregation");
    if (aOuter)
        return NS_ERROR_NO_AGGREGATION;

    EventsPluginInstance* plugin = new EventsPluginInstance();
    if (! plugin)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv;
    NS_ADDREF(plugin);
    rv = plugin->QueryInterface(aIID, aResult);
    NS_RELEASE(plugin);
    return rv;
}

NS_METHOD
EventsPluginInstance::RegisterSelf(nsIComponentManager* aCompMgr,
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
        rv = pm->RegisterPlugin(kEventsPluginCID,
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
EventsPluginInstance::UnregisterSelf(nsIComponentManager* aCompMgr,
                                     nsIFile* aPath,
                                     const char* aRegistryLocation,
                                     const nsModuleComponentInfo *info)
{
    nsresult rv;

    nsIPluginManager* pm;
    rv = nsServiceManager::GetService(kPluginManagerCID, NS_GET_IID(nsIPluginManager),
                                      NS_REINTERPRET_CAST(nsISupports**, &pm));
    if (NS_SUCCEEDED(rv)) {
        rv = pm->UnregisterPlugin(kEventsPluginCID);
        NS_RELEASE(pm);
    }

    return rv;
}

////////////////////////////////////////////////////////////////////////////////
// EventsPluginInstance Methods
////////////////////////////////////////////////////////////////////////////////

// These macros produce simple version of QueryInterface and AddRef.
// See the nsISupports.h header file for details.

NS_IMPL_ISUPPORTS2(EventsPluginInstance, nsIPluginInstance, nsIEventsSampleInstance)

EventsPluginInstance::EventsPluginInstance() :
		fPeer(NULL), fWindow(NULL), fMode(nsPluginMode_Embedded)
{
	NS_INIT_REFCNT();
	wChild = 0;
}

EventsPluginInstance::~EventsPluginInstance(void) {
}


NS_METHOD EventsPluginInstance::Initialize(nsIPluginInstancePeer *peer) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::Initialize\n");
#endif 

	NS_ASSERTION(peer != NULL, "null peer");

	fPeer = peer;
	nsCOMPtr<nsIPluginTagInfo> taginfo;
	const char* const* names = nsnull;
	const char* const* values = nsnull;
	PRUint16 count = 0;
	nsresult result;

	peer->AddRef();
	result = peer->GetMode(&fMode);
	if (NS_FAILED(result)) return result;

	taginfo = do_QueryInterface(peer, &result);
	if (!NS_FAILED(result)) {
		taginfo->GetAttributes(count, names, values);
	}

	PlatformNew(); 	/* Call Platform-specific initializations */
	return NS_OK;
}

NS_METHOD EventsPluginInstance::GetPeer(nsIPluginInstancePeer* *result) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::GetPeer\n");
#endif 

	*result = fPeer;
	NS_IF_ADDREF(*result);
	return NS_OK;
}

NS_METHOD EventsPluginInstance::Start(void) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::Start\n");
#endif 

	return NS_OK;
}

NS_METHOD EventsPluginInstance::Stop(void) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::Stop\n");
#endif 

	return NS_OK;
}

NS_METHOD EventsPluginInstance::Destroy(void) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::Destroy\n");
#endif 

	PlatformDestroy(); // Perform platform specific cleanup
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
NS_METHOD EventsPluginInstance::SetWindow(nsPluginWindow* window) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::SetWindow\n");
#endif 

	nsresult result;
	result = DoSetWindow(window);
	fWindow = window;
	return result;
}

nsresult EventsPluginInstance::DoSetWindow(nsPluginWindow* window) {
	/*
	 * PLUGIN DEVELOPERS:
	 *	Before setting window to point to the
	 *	new window, you may wish to compare the new window
	 *	info to the previous window (if any) to note window
	 *	size changes, etc.
	 */
	 nsresult result = NS_OK;
	if ( fWindow != NULL ) {
		// If we already have a window, clean it up 
		// before working with the new window
		if ( window && window->window && wMain == (WinID)window->window ) {
			/* The new window is the same as the old one. Exit now. */
			PlatformResizeWindow(window);
			return NS_OK;
		}
		// Otherwise, just reset the window ready for the new one.
		PlatformResetWindow();
	}
	else if ( (window == NULL) || ( window->window == NULL ) ) {
		/* We can just get out of here if there is no current
		 * window and there is no new window to use. */ 
		return NS_OK;
	}
	if (window && window->window) {
		// Remember our main parent window.
		wMain = (WinID)window->window;
		// And create the child window.
		result = PlatformCreateWindow(window);
	}
	return result;
}

NS_METHOD EventsPluginInstance::NewStream(nsIPluginStreamListener** listener) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::NewStream\n");
#endif 

	if (listener != NULL) {
		EventsPluginStreamListener* sl =
		    new EventsPluginStreamListener(this, "http://www.mozilla.org");
		if (!sl)
			return NS_ERROR_UNEXPECTED;
		sl->AddRef();
		*listener = sl;
	}

	return NS_OK;
}

NS_METHOD EventsPluginInstance::Print(nsPluginPrint* printInfo) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::Print\n");
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
	} else {	/* If not fullscreen, we must be embedded */
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

NS_METHOD EventsPluginInstance::HandleEvent(nsPluginEvent* event, PRBool* handled) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::HandleEvent\n");
#endif 

	*handled = (PRBool)PlatformHandleEvent(event);
	return NS_OK;
}

NS_METHOD EventsPluginInstance::GetValue(nsPluginInstanceVariable /*variable*/, void * /*value*/) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::GetValue\n");
#endif 

	return NS_ERROR_FAILURE;
}

////////////////////////////////////////////////////////////////////////////////
// EventsPluginStreamListener Methods
////////////////////////////////////////////////////////////////////////////////

EventsPluginStreamListener::EventsPluginStreamListener(EventsPluginInstance* inst_,
        const char* msgName)
		: fMessageName(msgName), inst(inst_) {
	NS_INIT_REFCNT();
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginStreamListener: EventsPluginStreamListener for %s\n", fMessageName);
#endif
}

EventsPluginStreamListener::~EventsPluginStreamListener(void) {
}

// This macro produces a simple version of QueryInterface, AddRef and Release.
// See the nsISupports.h header file for details.

NS_IMPL_ISUPPORTS1(EventsPluginStreamListener, nsIPluginStreamListener);

NS_METHOD EventsPluginStreamListener::OnStartBinding(nsIPluginStreamInfo * /*pluginInfo*/) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginStreamListener::OnStartBinding\n");
	printf("EventsPluginStreamListener: Opening plugin stream for %s\n", fMessageName);
#endif 
	return NS_OK;
}

NS_METHOD EventsPluginStreamListener::OnDataAvailable(
    nsIPluginStreamInfo * /*pluginInfo*/,
    nsIInputStream* input,
    PRUint32 length) {

#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginStreamListener::OnDataAvailable\n");
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

NS_METHOD EventsPluginStreamListener::OnFileAvailable(
    nsIPluginStreamInfo * /*pluginInfo*/,
    const char* fileName) {

#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginStreamListener::OnFileAvailable\n");
#endif 

	char msg[256];
	sprintf(msg, "### File available for %s: %s\n", fMessageName, fileName);
	return NS_OK;
}

NS_METHOD EventsPluginStreamListener::OnStopBinding(
    nsIPluginStreamInfo * /*pluginInfo*/,
    nsresult /*status*/) {

#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginStreamListener::OnStopBinding\n");
#endif 

	char msg[256];
	sprintf(msg, "### Closing plugin stream for %s\n", fMessageName);
	return NS_OK;
}

NS_METHOD EventsPluginStreamListener::OnNotify(const char * /*url*/, nsresult /*status*/) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginStreamListener::OnNotify\n");
#endif 

	return NS_OK;
}

NS_METHOD EventsPluginStreamListener::GetStreamType(nsPluginStreamType *result) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginStreamListener::GetStreamType\n");
#endif 

	*result = nsPluginStreamType_Normal;
	return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
// Platform-Specific Implemenations

// UNIX Implementations

#ifdef XP_UNIX

void EventsPluginInstance::PlatformNew(void) {
	fPlatform.moz_box = 0;
}

nsresult EventsPluginInstance::PlatformDestroy(void) {
	// the mozbox will be destroyed by the native destruction of the
	// widget's parent.
	return NS_OK;
}

void EventsPluginInstance::PlatformResetWindow() {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::PlatformResetWindow\n");
#endif
	fPlatform.moz_box = 0;
}

nsresult EventsPluginInstance::PlatformCreateWindow(nsPluginWindow* window) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::PlatformCreateWindow %lx\n", (long)window);
#endif 

	Window x_window = (Window)window->window;
	GdkWindow *gdk_window = (GdkWindow *)gdk_xid_table_lookup(x_window);
	if (!gdk_window) {
		fprintf(stderr, "NO WINDOW!!!\n");
		return NS_ERROR_FAILURE;
	}
	fPlatform.moz_box = gtk_mozbox_new(gdk_window);

	wChild = gtk_entry_new();
	gtk_container_add(GTK_CONTAINER(fPlatform.moz_box), wChild);
	gtk_widget_show_all(fPlatform.moz_box);
	return NS_OK;
}

void EventsPluginInstance::PlatformResizeWindow(nsPluginWindow* window) {
	NS_PRECONDITION(wChild, "Have no wChild!");
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::PlatformResizeWindow to size (%d,%d)\n", window->width, window->height);
#endif
	// Mozilla has already sized the mozbox - we just need to handle the child.
	gtk_widget_set_usize(wChild, window->width, window->height);
}

int16 EventsPluginInstance::PlatformHandleEvent(nsPluginEvent * /*event*/) {
	/* UNIX Plugins do not use HandleEvent */
	return 0;
}

/* attribute string text; */
NS_IMETHODIMP EventsPluginInstance::GetVal(char * *aText) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::GetVal\n");
#endif 
	char *text = gtk_entry_get_text(GTK_ENTRY(wChild));
	*aText = reinterpret_cast<char*>(nsAllocator::Clone(text, strlen(text) + 1));
	return (*aText) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP EventsPluginInstance::SetVal(const char * aText) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::SetVal\n");
#endif 
	gtk_entry_set_text(GTK_ENTRY(wChild), aText);
	return NS_OK;
}
#endif     /* XP_UNIX */

// Windows Implementations

#if defined(XP_WIN)

void EventsPluginInstance::PlatformNew(void) {
	// Nothing to do!
}

nsresult EventsPluginInstance::PlatformDestroy(void) {
	wChild = 0;
	return NS_OK;
}

nsresult EventsPluginInstance::PlatformCreateWindow(nsPluginWindow* window) {
	// Remember parent wndproc.
	fPlatform.fParentWindowProc = (WNDPROC)::GetWindowLong(wMain, GWL_WNDPROC);
	NS_ABORT_IF_FALSE(fPlatform.fParentWindowProc!=NULL, "Couldn't get the parent WNDPROC");

	// Create the child window that fills our nsWindow 
	RECT rc;
	::GetWindowRect(wMain, &rc);

	wChild = ::CreateWindow("Edit", // class - standard Windows edit control.
							"", // title
							WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL, // style
							0, 0, rc.right-rc.left, rc.bottom-rc.top,
							wMain, // parent
							(HMENU)1111, // window ID
							0, // instance
							NULL); //creation data.
	NS_ABORT_IF_FALSE(wChild != NULL, "Failed to create the child window!");
	if (!wChild)
		return NS_ERROR_FAILURE;
	// Stash away our "this" pointer so our WNDPROC can talk to us.
	::SetProp(wChild, gInstanceLookupString, (HANDLE)this);
	fPlatform.fOldChildWindowProc =
		(WNDPROC)::SetWindowLong( wChild,
								GWL_WNDPROC, 
								(LONG)EventsPluginInstance::WndProcChild);
	return NS_OK;
}

int16 EventsPluginInstance::PlatformHandleEvent(nsPluginEvent * /*event*/) {
	return NS_OK;
}

void EventsPluginInstance::PlatformResetWindow() {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::PlatformResetWindow\n");
#endif
	fPlatform.fParentWindowProc = NULL;
	::SetWindowLong(wChild, GWL_WNDPROC, (LONG)fPlatform.fOldChildWindowProc);
	fPlatform.fOldChildWindowProc = NULL;
	wChild = NULL;
	wMain = NULL;
}

void EventsPluginInstance::PlatformResizeWindow(nsPluginWindow* window) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::PlatformResizeWindow with new size (%d,%d)\n", window->width, window->height);
#endif
	RECT rc;
	NS_PRECONDITION(wMain != nsnull, "Must have a valid wMain to resize");
	::GetClientRect(wMain, &rc);
	::SetWindowPos(wChild, 0, rc.left, rc.top,
	               rc.right - rc.left, rc.bottom - rc.top, SWP_NOZORDER | SWP_NOACTIVATE);
}

/* attribute string text; */
NS_IMETHODIMP EventsPluginInstance::GetVal(char * *aText) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::GetVal\n");
#endif 
	static char *empty = "";
	char *value = empty;
	char buffer[256];
	if (wChild) {
		GetWindowText(wChild, buffer, sizeof(buffer)/sizeof(buffer[0]));
		value = buffer;
	}
	*aText = reinterpret_cast<char*>(nsAllocator::Clone(value, strlen(value) + 1));
	return (*aText) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP EventsPluginInstance::SetVal(const char * aText) {
#ifdef EVENTSPLUGIN_DEBUG
	printf("EventsPluginInstance::SetVal\n");
#endif 
	NS_ABORT_IF_FALSE(wChild != 0, "Don't have a window!");
	SetWindowText(wChild, aText);
	return NS_OK;
}

// This is the WndProc for our child window (the edit control)
LRESULT CALLBACK EventsPluginInstance::WndProcChild(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam) {
	EventsPluginInstance* inst = (EventsPluginInstance*) GetProp(hWnd, gInstanceLookupString);
	NS_ABORT_IF_FALSE(inst, "Could not get the inst from the Window!!");
	switch (Msg) {
	// NOTE: We DONT pass on DBLCLK messages, as both Scintilla and
	// Mozilla have their own special logic, and they step on each other.
	// (causing our child to see a double-click as a triple-click)
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		case WM_KEYUP:
		case WM_SYSKEYUP:
		case WM_CHAR:
		case WM_SYSCHAR:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_MOUSEMOVE:
			// pretend the message happened in our parent.
			return ::CallWindowProc(inst->fPlatform.fParentWindowProc, (HWND)inst->wMain, Msg, wParam, lParam);
		default:
			// let our child's default handle it.
			return ::CallWindowProc(inst->fPlatform.fOldChildWindowProc, hWnd, Msg, wParam, lParam);
	}
	/* not reached */
	NS_ABORT_IF_FALSE(0, "not reached!");
}


#endif     /* XP_WIN */
