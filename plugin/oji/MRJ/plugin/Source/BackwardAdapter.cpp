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

////////////////////////////////////////////////////////////////////////////////
// Backward Adapter
// This acts as a adapter layer to allow 5.0 plugins work with the 4.0/3.0 
// browser.
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// SECTION 1 - Includes
////////////////////////////////////////////////////////////////////////////////

#include "npapi.h"
#include "nsIPluginManager2.h"
#include "nsIServiceManager.h"
#include "nsIAllocator.h"
#include "nsLiveConnect.h"
#include "nsIEventHandler.h"
#include "nsplugin.h"
#include "nsDebug.h"

#ifdef XP_MAC
#include "jGNE.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// SECTION 3 - Classes
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//
// CPluginManager
//
// This is the dummy plugin manager that interacts with the 5.0 plugin.
//

#pragma mark CPluginManager

class CPluginManager : public nsIPluginManager2, public nsIServiceManager, public nsIAllocator {
public:
	// Need an operator new for this.
	void* operator new(size_t size) { return ::NPN_MemAlloc(size); }
	void operator delete(void* ptr) { ::NPN_MemFree(ptr); }

    CPluginManager(void);
    virtual ~CPluginManager(void);

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginManager:

    /**
     * Returns the value of a variable associated with the plugin manager.
     *
     * (Corresponds to NPN_GetValue.)
     *
     * @param variable - the plugin manager variable to get
     * @param value - the address of where to store the resulting value
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetValue(nsPluginManagerVariable variable, void *value);

    /**
     * Causes the plugins directory to be searched again for new plugin 
     * libraries.
     *
     * (Corresponds to NPN_ReloadPlugins.)
     *
     * @param reloadPages - indicates whether currently visible pages should 
     * also be reloaded
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    ReloadPlugins(PRBool reloadPages);

    /**
     * Returns the user agent string for the browser. 
     *
     * (Corresponds to NPN_UserAgent.)
     *
     * @param resultingAgentString - the resulting user agent string
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    UserAgent(const char* *resultingAgentString);

    /**
     * Fetches a URL.
     *
     * (Corresponds to NPN_GetURL and NPN_GetURLNotify.)
     *
     * @param pluginInst - the plugin making the request. If NULL, the URL
     *   is fetched in the background.
     * @param url - the URL to fetch
     * @param target - the target window into which to load the URL
     * @param notifyData - when present, URLNotify is called passing the
     *   notifyData back to the client. When NULL, this call behaves like
     *   NPN_GetURL.
     * @param altHost - an IP-address string that will be used instead of the 
     *   host specified in the URL. This is used to prevent DNS-spoofing 
     *   attacks. Can be defaulted to NULL meaning use the host in the URL.
     * @param referrer - the referring URL (may be NULL)
     * @param forceJSEnabled - forces JavaScript to be enabled for 'javascript:'
     *   URLs, even if the user currently has JavaScript disabled (usually 
     *   specify PR_FALSE) 
     * @result - NS_OK if this operation was successful
     */

    NS_IMETHOD
    GetURL(nsISupports* pluginInst, 
           const char* url, 
           const char* target = NULL,
           nsIPluginStreamListener* streamListener = NULL,
           const char* altHost = NULL,
           const char* referrer = NULL,
           PRBool forceJSEnabled = PR_FALSE);

    /**
     * Posts to a URL with post data and/or post headers.
     *
     * (Corresponds to NPN_PostURL and NPN_PostURLNotify.)
     *
     * @param pluginInst - the plugin making the request. If NULL, the URL
     *   is fetched in the background.
     * @param url - the URL to fetch
     * @param target - the target window into which to load the URL
     * @param postDataLength - the length of postData (if non-NULL)
     * @param postData - the data to POST. NULL specifies that there is not post
     *   data
     * @param isFile - whether the postData specifies the name of a file to 
     *   post instead of data. The file will be deleted afterwards.
     * @param notifyData - when present, URLNotify is called passing the 
     *   notifyData back to the client. When NULL, this call behaves like 
     *   NPN_GetURL.
     * @param altHost - an IP-address string that will be used instead of the 
     *   host specified in the URL. This is used to prevent DNS-spoofing 
     *   attacks. Can be defaulted to NULL meaning use the host in the URL.
     * @param referrer - the referring URL (may be NULL)
     * @param forceJSEnabled - forces JavaScript to be enabled for 'javascript:'
     *   URLs, even if the user currently has JavaScript disabled (usually 
     *   specify PR_FALSE) 
     * @param postHeadersLength - the length of postHeaders (if non-NULL)
     * @param postHeaders - the headers to POST. NULL specifies that there 
     *   are no post headers
     * @result - NS_OK if this operation was successful
     */

    NS_IMETHOD
    PostURL(nsISupports* pluginInst,
            const char* url,
            PRUint32 postDataLen, 
            const char* postData,
            PRBool isFile = PR_FALSE,
            const char* target = NULL,
            nsIPluginStreamListener* streamListener = NULL,
            const char* altHost = NULL, 
            const char* referrer = NULL,
            PRBool forceJSEnabled = PR_FALSE,
            PRUint32 postHeadersLength = 0, 
            const char* postHeaders = NULL);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginManager2:

    /**
     * Puts up a wait cursor.
     *
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    BeginWaitCursor(void)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * Restores the previous (non-wait) cursor.
     *
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    EndWaitCursor(void)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * Returns true if a URL protocol (e.g. "http") is supported.
     *
     * @param protocol - the protocol name
     * @param result - true if the protocol is supported
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    SupportsURLProtocol(const char* protocol, PRBool *result)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * This method may be called by the plugin to indicate that an error 
     * has occurred, e.g. that the plugin has failed or is shutting down 
     * spontaneously. This allows the browser to clean up any plugin-specific 
     * state.
     *
     * @param plugin - the plugin whose status is changing
     * @param errorStatus - the the error status value
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    NotifyStatusChange(nsIPlugin* plugin, nsresult errorStatus)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}
    
    /**
     * Returns the proxy info for a given URL. The caller is required to
     * free the resulting memory with nsIMalloc::Free. The result will be in the
     * following format
     * 
     *   i)   "DIRECT"  -- no proxy
     *   ii)  "PROXY xxx.xxx.xxx.xxx"   -- use proxy
     *   iii) "SOCKS xxx.xxx.xxx.xxx"  -- use SOCKS
     *   iv)  Mixed. e.g. "PROXY 111.111.111.111;PROXY 112.112.112.112",
     *                    "PROXY 111.111.111.111;SOCKS 112.112.112.112"....
     *
     * Which proxy/SOCKS to use is determined by the plugin.
     */
    NS_IMETHOD
    FindProxyForURL(const char* url, char* *result)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

    ////////////////////////////////////////////////////////////////////////////
    // New top-level window handling calls for Mac:
    
    /**
     * Registers a top-level window with the browser. Events received by that
     * window will be dispatched to the event handler specified.
     * 
     * @param handler - the event handler for the window
     * @param window - the platform window reference
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    RegisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window);
    
    /**
     * Unregisters a top-level window with the browser. The handler and window pair
     * should be the same as that specified to RegisterWindow.
     *
     * @param handler - the event handler for the window
     * @param window - the platform window reference
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    UnregisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window);

	/**
     * Allocates a new menu ID (for the Mac).
     *
     * @param handler - the event handler for the window
     * @param isSubmenu - whether this is a sub-menu ID or not
     * @param result - the resulting menu ID
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
	AllocateMenuID(nsIEventHandler* handler, PRBool isSubmenu, PRInt16 *result)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

	/**
     * Deallocates a menu ID (for the Mac).
     *
     * @param handler - the event handler for the window
     * @param menuID - the menu ID
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
	DeallocateMenuID(nsIEventHandler* handler, PRInt16 menuID)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

	/**
	 * Indicates whether this event handler has allocated the given menu ID.
     *
     * @param handler - the event handler for the window
     * @param menuID - the menu ID
     * @param result - returns PR_TRUE if the menu ID is allocated
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    HasAllocatedMenuID(nsIEventHandler* handler, PRInt16 menuID, PRBool *result)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

    ////////////////////////////////////////////////////////////////////////////
    // from nsIServiceManager:

   /**
     * RegisterService may be called explicitly to register a service
     * with the service manager. If a service is not registered explicitly,
     * the component manager will be used to create an instance according
     * to the class ID specified.
     */
    NS_IMETHOD
    RegisterService(const nsCID& aClass, nsISupports* aService)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * Requests a service to be shut down, possibly unloading its DLL.
     *
     * @returns NS_OK - if shutdown was successful and service was unloaded,
     * @returns NS_ERROR_SERVICE_NOT_FOUND - if shutdown failed because
     *          the service was not currently loaded
     * @returns NS_ERROR_SERVICE_IN_USE - if shutdown failed because some
     *          user of the service wouldn't voluntarily release it by using
     *          a shutdown listener.
     */
    NS_IMETHOD
    UnregisterService(const nsCID& aClass)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

    NS_IMETHOD
    GetService(const nsCID& aClass, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = NULL);

    NS_IMETHOD
    ReleaseService(const nsCID& aClass, nsISupports* service,
                   nsIShutdownListener* shutdownListener = NULL);

    NS_IMETHOD
    GetService(const char* aProgID, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener = NULL)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

    NS_IMETHOD
    ReleaseService(const char* aProgID, nsISupports* service,
                   nsIShutdownListener* shutdownListener = NULL)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

    /**
     * Allocates a block of memory of a particular size. 
     *
     * @param size - the size of the block to allocate
     * @result the block of memory
     */
    NS_IMETHOD_(void*)
    Alloc(PRUint32 size);

    /**
     * Reallocates a block of memory to a new size.
     *
     * @param ptr - the block of memory to reallocate
     * @param size - the new size
     * @result the rellocated block of memory
     */
    NS_IMETHOD_(void*)
    Realloc(void* ptr, PRUint32 size);

    /**
     * Frees a block of memory. 
     *
     * @param ptr - the block of memory to free
     */
    NS_IMETHOD
    Free(void* ptr);

    /**
     * Attempts to shrink the heap.
     */
    NS_IMETHOD
    HeapMinimize(void);

private:
	nsILiveconnect* mLiveconnect;

	struct RegisteredWindow {
		RegisteredWindow* mNext;
		nsIEventHandler* mHandler;
		nsPluginPlatformWindowRef mWindow;
		
		RegisteredWindow(RegisteredWindow* next, nsIEventHandler* handler, nsPluginPlatformWindowRef window)
			: mNext(next), mHandler(handler), mWindow(window)
		{
			NS_ADDREF(mHandler);
		}
		
		~RegisteredWindow()
		{
			NS_RELEASE(mHandler);
		}
	};

	static RegisteredWindow* theRegisteredWindows;
	static RegisteredWindow** GetRegisteredWindow(nsPluginPlatformWindowRef window);

#ifdef XP_MAC
	Boolean mEventFilterInstalled;
	static Boolean EventFilter(EventRecord* event);
#endif
};

////////////////////////////////////////////////////////////////////////////////
//
// CPluginManagerStream
//
// This is the dummy plugin manager stream that interacts with the 5.0 plugin.
//

#pragma mark CPluginManagerStream

class CPluginManagerStream : public nsIOutputStream {
public:

    CPluginManagerStream(NPP npp, NPStream* pstr);
    virtual ~CPluginManagerStream(void);

    NS_DECL_ISUPPORTS

    //////////////////////////////////////////////////////////////////////////
    //
    // Taken from nsIStream
    //
    
    /** Write data into the stream.
     *  @param aBuf the buffer into which the data is read
     *  @param aCount the maximum number of bytes to read
     *  @param errorResult the error code if an error occurs
     *  @return number of bytes read or -1 if error
     */   
    NS_IMETHOD
    Write(const char* aBuf, PRUint32 aCount, PRUint32 *aWriteCount); 

    //////////////////////////////////////////////////////////////////////////
    //
    // Specific methods to nsIPluginManagerStream.
    //
    
    // Corresponds to NPStream's url field.
    NS_IMETHOD
    GetURL(const char*  *result);

    // Corresponds to NPStream's end field.
    NS_IMETHOD
    GetEnd(PRUint32 *result);

    // Corresponds to NPStream's lastmodfied field.
    NS_IMETHOD
    GetLastModified(PRUint32 *result);

    // Corresponds to NPStream's notifyData field.
    NS_IMETHOD
    GetNotifyData(void*  *result);

    // Corresponds to NPStream's url field.
    NS_IMETHOD Close(void);

protected:

    // npp
    // The plugin instance that the manager stream belongs to.
    NPP npp;

    // pstream
    // The stream the class is using.
    NPStream* pstream;

};

////////////////////////////////////////////////////////////////////////////////
//
// CPluginInstancePeer
//
// This is the dummy instance peer that interacts with the 5.0 plugin.
// In order to do LiveConnect, the class subclasses nsILiveConnectPluginInstancePeer.
//

#pragma mark CPluginInstancePeer

class CPluginInstancePeer : public nsIPluginInstancePeer, public nsIPluginTagInfo {
public:

    // XXX - I add parameters to the constructor because I wasn't sure if
    // XXX - the 4.0 browser had the npp_instance struct implemented.
    // XXX - If so, then I can access npp_instance through npp->ndata.
    CPluginInstancePeer(nsIPluginInstance* pluginInstance, NPP npp, nsMIMEType typeString, nsPluginMode type,
        PRUint16 attribute_cnt, const char** attribute_list, const char** values_list);

    virtual ~CPluginInstancePeer(void);

    NS_DECL_ISUPPORTS

    // (Corresponds to NPN_GetValue.)
    NS_IMETHOD
    GetValue(nsPluginInstancePeerVariable variable, void *value);

    // (Corresponds to NPN_SetValue.)
    NS_IMETHOD
    SetValue(nsPluginInstancePeerVariable variable, void *value);

    // Corresponds to NPP_New's MIMEType argument.
    NS_IMETHOD
    GetMIMEType(nsMIMEType *result);

    // Corresponds to NPP_New's mode argument.
    NS_IMETHOD
    GetMode(nsPluginMode *result);

    // Get a ptr to the paired list of attribute names and values,
    // returns the length of the array.
    //
    // Each name or value is a null-terminated string.
    NS_IMETHOD
    GetAttributes(PRUint16& n, const char* const*& names, const char* const*& values);

    // Get the value for the named attribute.  Returns null
    // if the attribute was not set.
    NS_IMETHOD
    GetAttribute(const char* name, const char* *result);

    // Corresponds to NPN_NewStream.
    NS_IMETHOD
    NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result);

    // Corresponds to NPN_ShowStatus.
    NS_IMETHOD
    ShowStatus(const char* message);

    NS_IMETHOD
    SetWindowSize(PRUint32 width, PRUint32 height);

	nsIPluginInstance* GetInstance(void) { return mInstance; }
	NPP GetNPPInstance(void) { return npp; }
	
	void SetWindow(NPWindow* window) { mWindow = window; }
	NPWindow* GetWindow() { return mWindow; }
	
protected:

    NPP npp;
    // XXX - The next five variables may need to be here since I
    // XXX - don't think np_instance is available in 4.0X.
    nsIPluginInstance* mInstance;
    NPWindow* mWindow;
    nsMIMEType typeString;
	nsPluginMode type;
	PRUint16 attribute_cnt;
	char** attribute_list;
	char** values_list;
};

#pragma mark CPluginStreamInfo

class CPluginStreamInfo : public nsIPluginStreamInfo {
public:

	CPluginStreamInfo(const char* URL, nsIPluginInputStream* inStr, nsMIMEType type, PRBool seekable)
		 : mURL(URL), mInputStream(inStr), mMimeType(type), mIsSeekable(seekable)
	{
		NS_INIT_REFCNT();
	}

	virtual ~CPluginStreamInfo() {}

    NS_DECL_ISUPPORTS
    
	NS_METHOD
	GetContentType(nsMIMEType* result)
	{
		*result = mMimeType;
		return NS_OK;
	}

	NS_METHOD
	IsSeekable(PRBool* result)
	{
		*result = mIsSeekable;
		return NS_OK;
	}

	NS_METHOD
	GetLength(PRUint32* result)
	{
		return mInputStream->GetLength(result);
	}

	NS_METHOD
	GetLastModified(PRUint32* result)
	{
		return mInputStream->GetLastModified(result);
	}

	NS_METHOD
	GetURL(const char** result)
	{
		*result = mURL;
		return NS_OK;
	}

	NS_METHOD
	RequestRead(nsByteRange* rangeList)
	{
		return mInputStream->RequestRead(rangeList);
	}

private:
	const char* mURL;
	nsIPluginInputStream* mInputStream;
	nsMIMEType mMimeType;
	PRBool mIsSeekable;
};

#pragma mark CPluginInputStream

class CPluginInputStream : public nsIPluginInputStream {
public:

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsIBaseStream:

    /** Close the stream. */
    NS_IMETHOD
    Close(void);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIInputStream:

    /** Return the number of bytes in the stream
     *  @param aLength out parameter to hold the length
     *         of the stream. if an error occurs, the length
     *         will be undefined
     *  @return error status
     */
    NS_IMETHOD
    GetLength(PRUint32 *aLength);

    /** Read data from the stream.
     *  @param aErrorCode the error code if an error occurs
     *  @param aBuf the buffer into which the data is read
     *  @param aCount the maximum number of bytes to read
     *  @param aReadCount out parameter to hold the number of
     *         bytes read, eof if 0. if an error occurs, the
     *         read count will be undefined
     *  @return error status
     */   
    NS_IMETHOD
    Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount); 

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginInputStream:

    // (Corresponds to NPStream's lastmodified field.)
    NS_IMETHOD
    GetLastModified(PRUint32 *result);

    NS_IMETHOD
    RequestRead(nsByteRange* rangeList);

    ////////////////////////////////////////////////////////////////////////////
    // CPluginInputStream specific methods:

    CPluginInputStream(nsIPluginStreamListener* listener);
    virtual ~CPluginInputStream(void);

    void SetStreamInfo(NPP npp, NPStream* stream) {
        mNPP = npp;
        mStream = stream;
    }

    nsIPluginStreamListener* GetListener(void) { return mListener; }
    nsPluginStreamType GetStreamType(void) { return mStreamType; }

    nsresult SetReadBuffer(PRUint32 len, const char* buffer) {
        // XXX this has to be way more sophisticated
        mBuffer = dup(len, buffer);
        mBufferLength = len;
        mAmountRead = 0;
        return NS_OK;
    }
    
    char* dup(PRUint32 len, const char* buffer) {
    	char* result = new char[len];
    	if (result != NULL) {
    		const char *src = buffer;
    		char *dest = result; 
    		while (len-- > 0)
    			*dest++ = *src++;
    	}
    	return result;
    }

	nsIPluginStreamInfo* CreatePluginStreamInfo(const char* url, nsMIMEType type, PRBool seekable) {
		if (mStreamInfo == NULL) {
			mStreamInfo = new CPluginStreamInfo(url, this, type, seekable);
			mStreamInfo->AddRef();
		}
		return mStreamInfo;
	}
	
	nsIPluginStreamInfo* GetPluginStreamInfo() {
		return mStreamInfo;
	}

protected:
    const char* mURL;
    nsIPluginStreamListener* mListener;
    nsPluginStreamType mStreamType;
    NPP mNPP;
    NPStream* mStream;
    char* mBuffer;
    PRUint32 mBufferLength;
    PRUint32 mAmountRead;
    nsIPluginStreamInfo* mStreamInfo;
};

//////////////////////////////////////////////////////////////////////////////

#ifdef XP_UNIX
#define TRACE(foo) trace(foo)
#endif

#ifdef XP_MAC
#undef assert
#define assert(cond)
#endif

//#if defined(__cplusplus)
//extern "C" {
//#endif

////////////////////////////////////////////////////////////////////////////////
// SECTION 1 - Includes
////////////////////////////////////////////////////////////////////////////////

#if defined(XP_UNIX) || defined(XP_MAC)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else
#include <windows.h>
#endif

////////////////////////////////////////////////////////////////////////////////
// SECTION 2 - Global Variables
////////////////////////////////////////////////////////////////////////////////
#pragma mark Globals

//
// thePlugin and thePluginManager are used in the life of the plugin.
//
// These two will be created on NPP_Initialize and destroyed on NPP_Shutdown.
//
nsIPluginManager* thePluginManager = NULL;
nsIPlugin* thePlugin = NULL;

//
// Interface IDs
//
#pragma mark IIDs

static NS_DEFINE_IID(kPluginCID, NS_PLUGIN_CID);
static NS_DEFINE_IID(kPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);

static NS_DEFINE_IID(kIPluginStreamInfoIID, NS_IPLUGINSTREAMINFO_IID);
static NS_DEFINE_IID(kIPluginInputStreamIID, NS_IPLUGININPUTSTREAM_IID);

// mapping from NPError to nsresult
nsresult fromNPError[] = {
    NS_OK,                          // NPERR_NO_ERROR,
    NS_ERROR_FAILURE,               // NPERR_GENERIC_ERROR,
    NS_ERROR_FAILURE,               // NPERR_INVALID_INSTANCE_ERROR,
    NS_ERROR_NOT_INITIALIZED,       // NPERR_INVALID_FUNCTABLE_ERROR,
    NS_ERROR_FACTORY_NOT_LOADED,    // NPERR_MODULE_LOAD_FAILED_ERROR,
    NS_ERROR_OUT_OF_MEMORY,         // NPERR_OUT_OF_MEMORY_ERROR,
    NS_NOINTERFACE,                 // NPERR_INVALID_PLUGIN_ERROR,
    NS_ERROR_ILLEGAL_VALUE,         // NPERR_INVALID_PLUGIN_DIR_ERROR,
    NS_NOINTERFACE,                 // NPERR_INCOMPATIBLE_VERSION_ERROR,
    NS_ERROR_ILLEGAL_VALUE,         // NPERR_INVALID_PARAM,
    NS_ERROR_ILLEGAL_VALUE,         // NPERR_INVALID_URL,
    NS_ERROR_ILLEGAL_VALUE,         // NPERR_FILE_NOT_FOUND,
    NS_ERROR_FAILURE,               // NPERR_NO_DATA,
    NS_ERROR_FAILURE                // NPERR_STREAM_NOT_SEEKABLE,
};

////////////////////////////////////////////////////////////////////////////////
// SECTION 4 - API Shim Plugin Implementations
// Glue code to the 5.0x Plugin.
//
// Most of the NPP_* functions that interact with the plug-in will need to get 
// the instance peer from npp->pdata so it can get the plugin instance from the
// peer. Once the plugin instance is available, the appropriate 5.0 plug-in
// function can be called:
//          
//  CPluginInstancePeer* peer = (CPluginInstancePeer* )instance->pdata;
//  nsIPluginInstance* inst = peer->GetUserInstance();
//  inst->NewPluginAPIFunction();
//
// Similar steps takes place with streams.  The stream peer is stored in NPStream's
// pdata.  Get the peer, get the stream, call the function.
//

////////////////////////////////////////////////////////////////////////////////
// UNIX-only API calls
////////////////////////////////////////////////////////////////////////////////

#ifdef XP_UNIX
char* NPP_GetMIMEDescription(void)
{
    int freeFac = 0;
    //fprintf(stderr, "MIME description\n");
    if (thePlugin == NULL) {
        freeFac = 1;
        NSGetFactory(thePluginManager, kPluginCID, NULL, NULL, (nsIFactory** )&thePlugin);
    }
    //fprintf(stderr, "Allocated Plugin 0x%08x\n", thePlugin);
    const char * ret;
    nsresult err = thePlugin->GetMIMEDescription(&ret);
    if (err) return NULL;
    //fprintf(stderr, "Get response %s\n", ret);
    if (freeFac) {
        //fprintf(stderr, "Freeing plugin...");
        thePlugin->Release();
        thePlugin = NULL;
    }
    //fprintf(stderr, "Done\n");
    return (char*)ret;
}


//------------------------------------------------------------------------------
// Cross-Platform Plug-in API Calls
//------------------------------------------------------------------------------

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_SetValue:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NPError
NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
    return NPERR_GENERIC_ERROR; // nothing to set
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_GetValue:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NPError
NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
    int freeFac = 0;
    //fprintf(stderr, "MIME description\n");
    if (thePlugin == NULL) {
        freeFac = 1;
        if (NSGetFactory(thePluginManager, kPluginCID, NULL, NULL, (nsIFactory** )&thePlugin) != NS_OK)
            return NPERR_GENERIC_ERROR;
    }
    //fprintf(stderr, "Allocated Plugin 0x%08x\n", thePlugin);
    nsresult err = thePlugin->GetValue((nsPluginVariable)variable, value);
    if (err) return NPERR_GENERIC_ERROR;
    //fprintf(stderr, "Get response %08x\n", ret);
    if (freeFac) {
        //fprintf(stderr, "Freeing plugin...");
        thePlugin->Release();
        thePlugin = NULL;
    }
    //fprintf(stderr, "Done\n");
    return NPERR_NO_ERROR;
}
#endif // XP_UNIX

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_Initialize:
// Provides global initialization for a plug-in, and returns an error value. 
//
// This function is called once when a plug-in is loaded, before the first instance
// is created. thePluginManager and thePlugin are both initialized.
//+++++++++++++++++++++++++++++++++++++++++++++++++

NPError
NPP_Initialize(void)
{
//    TRACE("NPP_Initialize\n");

    // Only call initialize the plugin if it hasn't been created.
    // This could happen if GetJavaClass() is called before
    // NPP Initialize.  
    if (thePluginManager == NULL) {
        // Create the plugin manager and plugin classes.
        thePluginManager = new CPluginManager();	
        if ( thePluginManager == NULL ) 
            return NPERR_OUT_OF_MEMORY_ERROR;  
        thePluginManager->AddRef();
    }
    NPError error = NPERR_INVALID_PLUGIN_ERROR;  
    // On UNIX the plugin might have been created when calling NPP_GetMIMEType.
    if (thePlugin == NULL) {
        // create nsIPlugin factory
        nsresult result = NSGetFactory(thePluginManager, kPluginCID, NULL, NULL, (nsIFactory**)&thePlugin);
        if (result == NS_OK && thePlugin->Initialize() == NS_OK)
        	error = NPERR_NO_ERROR;
      
	}
	
    return error;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_GetJavaClass:
// New in Netscape Navigator 3.0. 
// 
// NPP_GetJavaClass is called during initialization to ask your plugin
// what its associated Java class is. If you don't have one, just return
// NULL. Otherwise, use the javah-generated "use_" function to both
// initialize your class and return it. If you can't find your class, an
// error will be signalled by "use_" and will cause the Navigator to
// complain to the user.
//+++++++++++++++++++++++++++++++++++++++++++++++++

jref
NPP_GetJavaClass(void)
{
    // Only call initialize the plugin if it hasn't been `d.
#if 0
    if (thePluginManager == NULL) {
        // Create the plugin manager and plugin objects.
        NPError result = CPluginManager::Create();	
        if (result) return NULL;
        assert( thePluginManager != NULL );
        thePluginManager->AddRef();
        NP_CreatePlugin(thePluginManager, (nsIPlugin** )(&thePlugin));
        assert( thePlugin != NULL );
    }
    return thePlugin->GetJavaClass();
#endif
    return NULL;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_Shutdown:
// Provides global deinitialization for a plug-in. 
// 
// This function is called once after the last instance of your plug-in 
// is destroyed.  thePluginManager and thePlugin are delete at this time.
//+++++++++++++++++++++++++++++++++++++++++++++++++

void
NPP_Shutdown(void)
{
//    TRACE("NPP_Shutdown\n");

    if (thePlugin)
    {
        thePlugin->Shutdown();
        thePlugin->Release();
        thePlugin = NULL;
    }

    if (thePluginManager)  {
        thePluginManager->Release();
        thePluginManager = NULL;
    }
    
    return;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_New:
// Creates a new instance of a plug-in and returns an error value. 
// 
// A plugin instance peer and instance peer is created.  After
// a successful instansiation, the peer is stored in the plugin
// instance's pdata.
//+++++++++++++++++++++++++++++++++++++++++++++++++

NPError 
NPP_New(NPMIMEType pluginType,
	NPP instance,
	PRUint16 mode,
	int16 argc,
	char* argn[],
	char* argv[],
	NPSavedData* saved)
{
//    TRACE("NPP_New\n");
    
    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    // Create a new plugin instance and start it.
    nsIPluginInstance* pluginInstance = NULL;
    thePlugin->CreatePluginInstance(thePluginManager, nsIPluginInstance::GetIID(), pluginType, (void**)&pluginInstance);
    if (pluginInstance == NULL) {
        return NPERR_OUT_OF_MEMORY_ERROR;
    }
    
    // Create a new plugin instance peer,
    // XXX - Since np_instance is not implemented in the 4.0x browser, I
    // XXX - had to save the plugin parameter in the peer class.
    // XXX - Ask Warren about np_instance.
    CPluginInstancePeer* peer = new CPluginInstancePeer(pluginInstance, instance, (nsMIMEType)pluginType, 
						                                (nsPluginMode)mode, (PRUint16)argc, (const char** )argn, (const char** )argv);
    assert( peer != NULL );
    if (!peer) return NPERR_OUT_OF_MEMORY_ERROR;
    peer->AddRef();
    pluginInstance->Initialize(peer);
    pluginInstance->Start();
    // Set the user instance and store the peer in npp->pdata.
    instance->pdata = peer;
    peer->Release();

    return NPERR_NO_ERROR;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_Destroy:
// Deletes a specific instance of a plug-in and returns an error value. 
//
// The plugin instance peer and plugin instance are destroyed.
// The instance's pdata is set to NULL.
//+++++++++++++++++++++++++++++++++++++++++++++++++

NPError 
NPP_Destroy(NPP instance, NPSavedData** save)
{
//    TRACE("NPP_Destroy\n");
    
    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;
    
    CPluginInstancePeer* peer = (CPluginInstancePeer*) instance->pdata;
    nsIPluginInstance* pluginInstance = peer->GetInstance();
    pluginInstance->Stop();
    pluginInstance->Destroy();
    pluginInstance->Release();
	// peer->Release();
    instance->pdata = NULL;
    
    return NPERR_NO_ERROR;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_SetWindow:
// Sets the window in which a plug-in draws, and returns an error value. 
//+++++++++++++++++++++++++++++++++++++++++++++++++

NPError 
NPP_SetWindow(NPP instance, NPWindow* window)
{
//    TRACE("NPP_SetWindow\n");
    
    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    CPluginInstancePeer* peer = (CPluginInstancePeer*) instance->pdata;
    if ( peer == NULL)
        return NPERR_INVALID_PLUGIN_ERROR;

	// record the window in the peer, so we can deliver proper events.
	peer->SetWindow(window);

    nsIPluginInstance* pluginInstance = peer->GetInstance();
    if( pluginInstance == 0 )
        return NPERR_INVALID_PLUGIN_ERROR;

    return (NPError)pluginInstance->SetWindow((nsPluginWindow* ) window );
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_NewStream:
// Notifies an instance of a new data stream and returns an error value. 
//
// Create a stream peer and stream.  If succesful, save
// the stream peer in NPStream's pdata.
//+++++++++++++++++++++++++++++++++++++++++++++++++

NPError 
NPP_NewStream(NPP instance,
              NPMIMEType type,
              NPStream *stream, 
              NPBool seekable,
              PRUint16 *stype)
{
    // XXX - How do you set the fields of the peer stream and stream?
    // XXX - Looks like these field will have to be created since
    // XXX - We are not using np_stream.
   
//    TRACE("NPP_NewStream\n");

    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;
				
    CPluginInputStream* inStr = (CPluginInputStream*)stream->notifyData;
    if (inStr == NULL)
        return NPERR_GENERIC_ERROR;
    
    nsIPluginStreamInfo* info = inStr->CreatePluginStreamInfo(stream->url, type, seekable);
    nsresult err = inStr->GetListener()->OnStartBinding(info);
    if (err) return err;

    inStr->SetStreamInfo(instance, stream);
    stream->pdata = inStr;
    *stype = inStr->GetStreamType();

    return NPERR_NO_ERROR;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_WriteReady:
// Returns the maximum number of bytes that an instance is prepared to accept
// from the stream. 
//+++++++++++++++++++++++++++++++++++++++++++++++++

int32 
NPP_WriteReady(NPP instance, NPStream *stream)
{
//    TRACE("NPP_WriteReady\n");

    if (instance == NULL)
        return -1;

    CPluginInputStream* inStr = (CPluginInputStream*)stream->pdata;
    if (inStr == NULL)
        return -1;
    return NP_MAXREADY;
}


//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_Write:
// Delivers data from a stream and returns the number of bytes written. 
//+++++++++++++++++++++++++++++++++++++++++++++++++

int32 
NPP_Write(NPP instance, NPStream *stream, int32 offset, int32 len, void *buffer)
{
//    TRACE("NPP_Write\n");

    if (instance == NULL)
        return -1;
	
    CPluginInputStream* inStr = (CPluginInputStream*)stream->pdata;
    if (inStr == NULL)
        return -1;
    nsresult err = inStr->SetReadBuffer((PRUint32)len, (const char*)buffer);
    if (err != NS_OK) return -1;
    err = inStr->GetListener()->OnDataAvailable(inStr->GetPluginStreamInfo(), inStr, len);
    if (err != NS_OK) return -1;
    return len;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_DestroyStream:
// Indicates the closure and deletion of a stream, and returns an error value. 
//
// The stream peer and stream are destroyed.  NPStream's
// pdata is set to NULL.
//+++++++++++++++++++++++++++++++++++++++++++++++++

NPError 
NPP_DestroyStream(NPP instance, NPStream *stream, NPReason reason)
{
//    TRACE("NPP_DestroyStream\n");

    if (instance == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;
		
    CPluginInputStream* inStr = (CPluginInputStream*)stream->pdata;
    if (inStr == NULL)
        return NPERR_GENERIC_ERROR;
    inStr->GetListener()->OnStopBinding(inStr->GetPluginStreamInfo(), (nsPluginReason)reason);
    // inStr->Release();
    stream->pdata = NULL;
	
    return NPERR_NO_ERROR;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_StreamAsFile:
// Provides a local file name for the data from a stream. 
//+++++++++++++++++++++++++++++++++++++++++++++++++

void 
NPP_StreamAsFile(NPP instance, NPStream *stream, const char* fname)
{
//    TRACE("NPP_StreamAsFile\n");

    if (instance == NULL)
        return;
		
    CPluginInputStream* inStr = (CPluginInputStream*)stream->pdata;
    if (inStr == NULL)
        return;
    (void)inStr->GetListener()->OnFileAvailable(inStr->GetPluginStreamInfo(), fname);
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_Print:
//+++++++++++++++++++++++++++++++++++++++++++++++++

void 
NPP_Print(NPP instance, NPPrint* printInfo)
{
//    TRACE("NPP_Print\n");

    if(printInfo == NULL)   // trap invalid parm
        return;

    if (instance != NULL)
    {
        CPluginInstancePeer* peer = (CPluginInstancePeer*) instance->pdata;
        nsIPluginInstance* pluginInstance = peer->GetInstance();
        pluginInstance->Print((nsPluginPrint* ) printInfo );
    }
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_URLNotify:
// Notifies the instance of the completion of a URL request. 
//+++++++++++++++++++++++++++++++++++++++++++++++++

void
NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{
//    TRACE("NPP_URLNotify\n");

    if (instance != NULL) {
        CPluginInputStream* inStr = (CPluginInputStream*)notifyData;
        (void)inStr->GetListener()->OnStopBinding(inStr->GetPluginStreamInfo(), (nsPluginReason)reason);
        inStr->Release();
    }
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_HandleEvent:
// Mac-only, but stub must be present for Windows
// Delivers a platform-specific event to the instance. 
//+++++++++++++++++++++++++++++++++++++++++++++++++

#ifndef XP_UNIX
int16
NPP_HandleEvent(NPP instance, void* event)
{
//    TRACE("NPP_HandleEvent\n");
    int16 eventHandled = FALSE;
    if (instance == NULL)
        return eventHandled;
	
    NPEvent* npEvent = (NPEvent*) event;
    nsPluginEvent pluginEvent = {
#ifdef XP_MAC
        npEvent, NULL
#else
        npEvent->event, npEvent->wParam, npEvent->lParam
#endif
    };
	
    CPluginInstancePeer* peer = (CPluginInstancePeer*) instance->pdata;
    nsIPluginInstance* pluginInstance = peer->GetInstance();
    if (pluginInstance) {
        PRBool handled;
        nsresult err = pluginInstance->HandleEvent(&pluginEvent, &handled);
        if (err) return FALSE;
        eventHandled = (handled == PR_TRUE);
    }
	
    return eventHandled;
}
#endif // ndef XP_UNIX 

//////////////////////////////////////////////////////////////////////////////
// SECTION 5 - API Browser Implementations
//
// Glue code to the 4.0x Browser.
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
// CPluginManager
//

//******************************************************************************
//
// Once we moved to the new APIs, we need to implement fJVMMgr.
//
//******************************************************************************

CPluginManager::CPluginManager(void)
{
    // Set reference count to 0.
    NS_INIT_REFCNT();
    
    mLiveconnect = NULL;

#ifdef XP_MAC
    mEventFilterInstalled = false;
#endif
}

CPluginManager::~CPluginManager(void) 
{
	NS_IF_RELEASE(mLiveconnect);

#ifdef XP_MAC	
	if (mEventFilterInstalled)
		RemoveEventFilter();
#endif
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// ReloadPlugins:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginManager::ReloadPlugins(PRBool reloadPages)
{
    NPN_ReloadPlugins(reloadPages);
    return NS_OK;
}

NS_METHOD
CPluginManager::GetURL(nsISupports* pluginInst, 
                       const char* url, 
                       const char* target,
                       nsIPluginStreamListener* streamListener,
                       const char* altHost,
                       const char* referrer,
                       PRBool forceJSEnabled)
{
    if (altHost != NULL || referrer != NULL || forceJSEnabled != PR_FALSE) {
        return NPERR_INVALID_PARAM;
    }

    nsIPluginInstance* inst = NULL;
    nsresult rslt = pluginInst->QueryInterface(nsIPluginInstance::GetIID(), (void**)&inst);
    if (rslt != NS_OK) return rslt;
	CPluginInstancePeer* instancePeer = NULL;
    rslt = inst->GetPeer((nsIPluginInstancePeer**)&instancePeer);
    if (rslt != NS_OK) {
        inst->Release();
        return rslt;
    }
    NPP npp = instancePeer->GetNPPInstance();

    NPError err;
    if (streamListener) {
        CPluginInputStream* inStr = new CPluginInputStream(streamListener);
        if (inStr == NULL) {
            instancePeer->Release();
            inst->Release();
            return NS_ERROR_OUT_OF_MEMORY;
        }
        inStr->AddRef();
    
        err = NPN_GetURLNotify(npp, url, target, inStr);
    }
    else {
        err = NPN_GetURL(npp, url, target);
    }
    instancePeer->Release();
    inst->Release();
    return fromNPError[err];
}

NS_METHOD
CPluginManager::PostURL(nsISupports* pluginInst,
                        const char* url,
                        PRUint32 postDataLen, 
                        const char* postData,
                        PRBool isFile,
                        const char* target,
                        nsIPluginStreamListener* streamListener,
                        const char* altHost, 
                        const char* referrer,
                        PRBool forceJSEnabled,
                        PRUint32 postHeadersLength, 
                        const char* postHeaders)
{
    if (altHost != NULL || referrer != NULL || forceJSEnabled != PR_FALSE
        || postHeadersLength != 0 || postHeaders != NULL) {
        return NPERR_INVALID_PARAM;
    }

    nsIPluginInstance* inst = NULL;
    nsresult rslt = pluginInst->QueryInterface(nsIPluginInstance::GetIID(), (void**)&inst);
    if (rslt != NS_OK) return rslt;
	CPluginInstancePeer* instancePeer = NULL;
    rslt = inst->GetPeer((nsIPluginInstancePeer**)&instancePeer);
    if (rslt != NS_OK) {
        inst->Release();
        return rslt;
    }
    NPP npp = instancePeer->GetNPPInstance();

    NPError err;
    if (streamListener) {
        CPluginInputStream* inStr = new CPluginInputStream(streamListener);
        if (inStr == NULL) {
            instancePeer->Release();
            inst->Release();
            return NS_ERROR_OUT_OF_MEMORY;
        }
        inStr->AddRef();
    
        err = NPN_PostURLNotify(npp, url, target, postDataLen, postData, isFile, inStr);
    }
    else {
        err = NPN_PostURL(npp, url, target, postDataLen, postData, isFile);
    }
    instancePeer->Release();
    inst->Release();
    return fromNPError[err];
}

//////////////////////////////
// nsIPluginManager2 methods.



//+++++++++++++++++++++++++++++++++++++++++++++++++
// UserAgent:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginManager::UserAgent(const char* *result)
{
    *result = NPN_UserAgent(NULL);
    return NS_OK;
}


int varMap[] = {
    (int)NPNVxDisplay,                  // nsPluginManagerVariable_XDisplay = 1,
    (int)NPNVxtAppContext,              // nsPluginManagerVariable_XtAppContext,
    (int)NPNVnetscapeWindow,            // nsPluginManagerVariable_NetscapeWindow,
    (int)NPPVpluginWindowBool,          // nsPluginInstancePeerVariable_WindowBool,
    (int)NPPVpluginTransparentBool,     // nsPluginInstancePeerVariable_TransparentBool,
    (int)NPPVjavaClass,                 // nsPluginInstancePeerVariable_JavaClass,
    (int)NPPVpluginWindowSize,          // nsPluginInstancePeerVariable_WindowSize,
    (int)NPPVpluginTimerInterval,       // nsPluginInstancePeerVariable_TimerInterval
};

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetValue:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginManager::GetValue(nsPluginManagerVariable variable, void *value)
{
#ifdef XP_UNIX
    return fromNPError[NPN_GetValue(NULL, (NPNVariable)varMap[(int)variable], value)];
#else
    return fromNPError[NPERR_GENERIC_ERROR];
#endif // XP_UNIX
}

//////////////////////////////
// nsIPluginManager2 methods.
//////////////////////////////

CPluginManager::RegisteredWindow* CPluginManager::theRegisteredWindows = NULL;

CPluginManager::RegisteredWindow** CPluginManager::GetRegisteredWindow(nsPluginPlatformWindowRef window)
{
	RegisteredWindow** link = &theRegisteredWindows;
	RegisteredWindow* registeredWindow = *link;
	while (registeredWindow != NULL) {
		if (registeredWindow->mWindow == window)
			return link;
		link = &registeredWindow->mNext;
		registeredWindow = *link;
	}
	return NULL;
}

NS_METHOD
CPluginManager::RegisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
	theRegisteredWindows = new RegisteredWindow(theRegisteredWindows, handler, window);
	
#ifdef XP_MAC
	// use jGNE to obtain events for registered windows.
	if (!mEventFilterInstalled)
		::InstallEventFilter(&EventFilter);

	// plugin expect the window to be shown and selected at this point.
	::ShowWindow(window);
	::SelectWindow(window);
#endif

	return NS_OK;
}

NS_METHOD
CPluginManager::UnregisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
	RegisteredWindow** link = GetRegisteredWindow(window);
	if (link != NULL) {
		RegisteredWindow* registeredWindow = *link;
		*link = registeredWindow->mNext;
		delete registeredWindow;
	}

#ifdef XP_MAC
	::HideWindow(window);

	// if no windows registered, remove the filter.
	if (theRegisteredWindows == NULL) {
		::RemoveEventFilter();
		mEventFilterInstalled = false;
	}
#endif

	return NS_OK;
}

#ifdef XP_MAC

Boolean CPluginManager::EventFilter(EventRecord* event)
{
	Boolean filteredEvent = false;

    nsPluginEvent pluginEvent = { event, NULL };
	// see if this event is for one of our registered windows.
	switch (event->what) {
	case updateEvt:
		WindowRef window = WindowRef(event->message);
		RegisteredWindow** link = GetRegisteredWindow(window);
		if (link != NULL) {
			RegisteredWindow* registeredWindow = *link;
			nsIEventHandler* handler = registeredWindow->mHandler;
			pluginEvent.window = window;
			PRBool handled = PR_FALSE;
			GrafPtr port; GetPort(&port); SetPort(window); BeginUpdate(window);
				handler->HandleEvent(&pluginEvent, &handled);
			EndUpdate(window); SetPort(port);
			filteredEvent = true;
		}
		break;
	}
	
	return filteredEvent;
}

#endif /* XP_MAC */

//////////////////////////////
// nsIServiceManager methods.
//////////////////////////////

NS_METHOD
CPluginManager::GetService(const nsCID& aClass, const nsIID& aIID,
               nsISupports* *result,
               nsIShutdownListener* shutdownListener)
{
	// the only service we support currently is nsIAllocator.
	if (aClass.Equals(kPluginManagerCID) || aClass.Equals(kAllocatorCID)) {
		return QueryInterface(aIID, (void**) result);
	}
	if (aClass.Equals(nsILiveconnect::GetCID())) {
		if (mLiveconnect == NULL) {
			mLiveconnect = new nsLiveconnect;
			NS_IF_ADDREF(mLiveconnect);
		}
		return mLiveconnect->QueryInterface(aIID, result);
	}
	return NS_ERROR_SERVICE_NOT_FOUND;
}

NS_METHOD
CPluginManager::ReleaseService(const nsCID& aClass, nsISupports* service,
                   nsIShutdownListener* shutdownListener)
{
	NS_RELEASE(service);
	return NS_OK;
}

//////////////////////////////
// nsIAllocator methods.
//////////////////////////////

NS_METHOD_(void*)
CPluginManager::Alloc(PRUint32 size)
{
	return ::NPN_MemAlloc(size);
}

NS_METHOD_(void*)
CPluginManager::Realloc(void* ptr, PRUint32 size)
{
	if (ptr != NULL) {
		void* new_ptr = Alloc(size);
		if (new_ptr != NULL) {
			::memcpy(new_ptr, ptr, size);
			Free(ptr);
		}
		ptr = new_ptr;
	}
	return ptr;
}

NS_METHOD
CPluginManager::Free(void* ptr)
{
	if (ptr != NULL) {
		::NPN_MemFree(ptr);
		return NS_OK;
	}
	return NS_ERROR_NULL_POINTER;
}

NS_METHOD
CPluginManager::HeapMinimize()
{
#ifdef XP_MAC
	::NPN_MemFlush(1024);
#endif
	return NS_OK;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// nsISupports methods
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginManager::QueryInterface(const nsIID& iid, void** ptr) 
{                                                                        
    if (NULL == ptr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }
    
    if (iid.Equals(nsIPluginManager::GetIID()) || iid.Equals(nsIPluginManager2::GetIID())) {
        *ptr = (void*) ((nsIPluginManager2*)this);                        
        AddRef();                                                            
        return NS_OK;                                                        
    }                                                                      
    if (iid.Equals(nsIServiceManager::GetIID())) {                                                          
        *ptr = (void*) (nsIServiceManager*)this;                                        
        AddRef();                                                            
        return NS_OK;                                                        
    }
    if (iid.Equals(nsIAllocator::GetIID())) {                                                          
        *ptr = (void*) (nsIAllocator*)this;                                        
        AddRef();                                                            
        return NS_OK;                                                        
    }
    if (iid.Equals(nsISupports::GetIID())) {
        *ptr = (void*) this;                        
        AddRef();                                                            
        return NS_OK;                                                        
    }                                                                      
    return NS_NOINTERFACE;                                                 
}

NS_IMPL_ADDREF(CPluginManager);
NS_IMPL_RELEASE(CPluginManager);

//////////////////////////////////////////////////////////////////////////////
//
// CPluginInstancePeer
//

CPluginInstancePeer::CPluginInstancePeer(nsIPluginInstance* pluginInstance,
                                         NPP npp,
                                         nsMIMEType typeString, 
                                         nsPluginMode type,
                                         PRUint16 attr_cnt, 
                                         const char** attr_list,
                                         const char** val_list)
    :	mInstance(pluginInstance), mWindow(NULL),
		npp(npp), typeString(typeString), type(type), attribute_cnt(attr_cnt),
		attribute_list(NULL), values_list(NULL)
{
    // Set the reference count to 0.
    NS_INIT_REFCNT();
    
    NS_IF_ADDREF(mInstance);

    attribute_list = (char**) NPN_MemAlloc(attr_cnt * sizeof(const char*));
    values_list = (char**) NPN_MemAlloc(attr_cnt * sizeof(const char*));

    if (attribute_list != NULL && values_list != NULL) {
        for (int i = 0; i < attribute_cnt; i++)   {
            attribute_list[i] = (char*) NPN_MemAlloc(strlen(attr_list[i]) + 1);
            if (attribute_list[i] != NULL)
                strcpy(attribute_list[i], attr_list[i]);

            values_list[i] = (char*) NPN_MemAlloc(strlen(val_list[i]) + 1);
            if (values_list[i] != NULL)
                strcpy(values_list[i], val_list[i]);
        }
    }
}

CPluginInstancePeer::~CPluginInstancePeer(void) 
{
    if (attribute_list != NULL && values_list != NULL) {
        for (int i = 0; i < attribute_cnt; i++)   {
            NPN_MemFree(attribute_list[i]);
            NPN_MemFree(values_list[i]);
        }

        NPN_MemFree(attribute_list);
        NPN_MemFree(values_list);
    }
    
    NS_IF_RELEASE(mInstance);
}


//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetValue:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginInstancePeer::GetValue(nsPluginInstancePeerVariable variable, void *value)
{
#ifdef XP_UNIX
    return fromNPError[NPN_GetValue(NULL, (NPNVariable)varMap[(int)variable], value)];
#else
    return fromNPError[NPERR_GENERIC_ERROR];
#endif // XP_UNIX
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// SetValue:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginInstancePeer::SetValue(nsPluginInstancePeerVariable variable, void *value) 
{
#ifdef XP_UNIX
    return fromNPError[NPN_SetValue(NULL, (NPPVariable)varMap[(int)variable], value)];
#else
    return fromNPError[NPERR_GENERIC_ERROR];
#endif // XP_UNIX
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetMIMEType:
// Corresponds to NPP_New's MIMEType argument.
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginInstancePeer::GetMIMEType(nsMIMEType *result) 
{
    *result = typeString;
    return NS_OK;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetMode:
// Corresponds to NPP_New's mode argument.
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginInstancePeer::GetMode(nsPluginMode *result)
{
    *result = type;
    return NS_OK;
}


// Get a ptr to the paired list of attribute names and values,
// returns the length of the array.
//
// Each name or value is a null-terminated string.
NS_METHOD
CPluginInstancePeer::GetAttributes(PRUint16& n, const char* const*& names, const char* const*& values)  
{
    n = attribute_cnt;
    names = attribute_list;
    values = values_list;

    return NS_OK;
}

#if defined(XP_MAC)

inline unsigned char toupper(unsigned char c)
{
    return (c >= 'a' && c <= 'z') ? (c - ('a' - 'A')) : c;
}

static int strcasecmp(const char * str1, const char * str2)
{
#if __POWERPC__
	
    const	unsigned char * p1 = (unsigned char *) str1 - 1;
    const	unsigned char * p2 = (unsigned char *) str2 - 1;
    unsigned long		c1, c2;
		
    while (toupper(c1 = *++p1) == toupper(c2 = *++p2))
        if (!c1)
            return(0);

#else
	
    const	unsigned char * p1 = (unsigned char *) str1;
    const	unsigned char * p2 = (unsigned char *) str2;
    unsigned char		c1, c2;
	
    while (toupper(c1 = *p1++) == toupper(c2 = *p2++))
        if (!c1)
            return(0);

#endif
	
    return(toupper(c1) - toupper(c2));
}

#endif /* XP_MAC */

// Get the value for the named attribute.  Returns null
// if the attribute was not set.
NS_METHOD
CPluginInstancePeer::GetAttribute(const char* name, const char* *result) 
{
    for (int i=0; i < attribute_cnt; i++)  {
#if defined(XP_UNIX) || defined(XP_MAC)
        if (strcasecmp(name, attribute_list[i]) == 0)
#else
            if (stricmp(name, attribute_list[i]) == 0) 
#endif
            {
                *result = values_list[i];
                return NS_OK;
            }
    }

    return NS_ERROR_FAILURE;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NewStream:
//+++++++++++++++++++++++++++++++++++++++++++++++++
NS_METHOD
CPluginInstancePeer::NewStream(nsMIMEType type, const char* target, 
                               nsIOutputStream* *result)
{
    assert( npp != NULL );
    
    // Create a new NPStream.
    NPStream* ptr = NULL;
    NPError error = NPN_NewStream(npp, (NPMIMEType)type, target, &ptr);
    if (error) 
        return fromNPError[error];
    
    // Create a new Plugin Manager Stream.
    // XXX - Do we have to Release() the manager stream before doing this?
    // XXX - See the BAM doc for more info.
    CPluginManagerStream* mstream = new CPluginManagerStream(npp, ptr);
    if (mstream == NULL) 
        return NS_ERROR_OUT_OF_MEMORY;
    mstream->AddRef();
    *result = (nsIOutputStream* )mstream;

    return NS_OK;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// ShowStatus:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginInstancePeer::ShowStatus(const char* message)
{
    assert( message != NULL );

    NPN_Status(npp, message);
	return NS_OK;
}

NS_METHOD
CPluginInstancePeer::SetWindowSize(PRUint32 width, PRUint32 height)
{
    NPError err;
    NPSize size;
    size.width = width;
    size.height = height;
    err = NPN_SetValue(npp, NPPVpluginWindowSize, &size);
    return fromNPError[err];
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// nsISupports functions
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_IMPL_ADDREF(CPluginInstancePeer);
NS_IMPL_RELEASE(CPluginInstancePeer);

NS_METHOD
CPluginInstancePeer::QueryInterface(const nsIID& iid, void** ptr) 
{                                                                        
    if (NULL == ptr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }                                                                      
  
    if (iid.Equals(nsIPluginInstancePeer::GetIID())) {
        *ptr = (void*) this;                                        
        AddRef();                                                            
        return NS_OK;                                                        
    }                                                                      
    if (iid.Equals(nsIPluginTagInfo::GetIID()) || iid.Equals(nsISupports::GetIID())) {                                      
        *ptr = (void*) ((nsIPluginTagInfo*)this);                        
        AddRef();                                                            
        return NS_OK;                                                        
    }                                                                      
    return NS_NOINTERFACE;                                                 
}

//////////////////////////////////////////////////////////////////////////////
//
// CPluginManagerStream
//

CPluginManagerStream::CPluginManagerStream(NPP npp, NPStream* pstr)
    : npp(npp), pstream(pstr)
{
    // Set the reference count to 0.
    NS_INIT_REFCNT();
}

CPluginManagerStream::~CPluginManagerStream(void)
{
    //pstream = NULL;
    NPN_DestroyStream(npp, pstream, NPRES_DONE);
}


//+++++++++++++++++++++++++++++++++++++++++++++++++
// Write:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginManagerStream::Write(const char* buffer, PRUint32 len, PRUint32 *aWriteCount)
{
    assert( npp != NULL );
    assert( pstream != NULL );

    *aWriteCount = NPN_Write(npp, pstream, len, (void* )buffer);
    return *aWriteCount >= 0 ? NS_OK : NS_ERROR_FAILURE;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetURL:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginManagerStream::GetURL(const char* *result)
{
    assert( pstream != NULL );

    *result = pstream->url;
	return NS_OK;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetEnd:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginManagerStream::GetEnd(PRUint32 *result)
{
    assert( pstream != NULL );

    *result = pstream->end;
	return NS_OK;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetLastModified:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginManagerStream::GetLastModified(PRUint32 *result)
{
    assert( pstream != NULL );

    *result = pstream->lastmodified;
	return NS_OK;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetNotifyData:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginManagerStream::GetNotifyData(void* *result)
{
    assert( pstream != NULL );

    *result = pstream->notifyData;
	return NS_OK;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetNotifyData:
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_METHOD
CPluginManagerStream::Close(void)
{
    assert( pstream != NULL );

    return NS_OK;
}


//+++++++++++++++++++++++++++++++++++++++++++++++++
// nsISupports functions
//+++++++++++++++++++++++++++++++++++++++++++++++++

NS_IMPL_ADDREF(CPluginManagerStream);
NS_IMPL_RELEASE(CPluginManagerStream);

NS_IMPL_QUERY_INTERFACE(CPluginManagerStream, nsIOutputStream::GetIID());

//////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS(CPluginStreamInfo, kIPluginStreamInfoIID);

CPluginInputStream::CPluginInputStream(nsIPluginStreamListener* listener)
    : mListener(listener), mStreamType(nsPluginStreamType_Normal),
      mNPP(NULL), mStream(NULL),
      mBuffer(NULL), mBufferLength(0), mAmountRead(0),
      mStreamInfo(NULL)
{
    NS_INIT_REFCNT();

    if (mListener != NULL) {
        mListener->AddRef();
        mListener->GetStreamType(&mStreamType);
    }
}

CPluginInputStream::~CPluginInputStream(void)
{
	NS_IF_RELEASE(mListener);

    delete mBuffer;
    
    NS_IF_RELEASE(mStreamInfo);
}

NS_IMPL_ISUPPORTS(CPluginInputStream, kIPluginInputStreamIID);

NS_METHOD
CPluginInputStream::Close(void)
{
    if (mNPP == NULL || mStream == NULL)
        return NS_ERROR_FAILURE;
    NPError err = NPN_DestroyStream(mNPP, mStream, NPRES_USER_BREAK);
    return fromNPError[err];
}

NS_METHOD
CPluginInputStream::GetLength(PRUint32 *aLength)
{
    *aLength = mStream->end;
    return NS_OK;
}

NS_METHOD
CPluginInputStream::Read(char* aBuf, PRUint32 aCount, PRUint32 *aReadCount)
{
    PRUint32 cnt = PR_MIN(aCount, mBufferLength);
    memcpy(aBuf, mBuffer + mAmountRead, cnt);
    *aReadCount = cnt;
    mAmountRead += cnt;
    mBufferLength -= cnt;
    return NS_OK;
}

NS_METHOD
CPluginInputStream::GetLastModified(PRUint32 *result)
{
    *result = mStream->lastmodified;
    return NS_OK;
}

NS_METHOD
CPluginInputStream::RequestRead(nsByteRange* rangeList)
{
    NPError err = NPN_RequestRead(mStream, (NPByteRange*)rangeList);
    return fromNPError[err];
}


//////////////////////////////////////////////////////////////////////////////

//#if defined(__cplusplus)
//} /* extern "C" */
//#endif

