/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
#include "nsMemory.h"
#include "nsLiveConnect.h"
#include "nsIEventHandler.h"
#include "nsplugin.h"
#include "nsDebug.h"
#include "nsIJRILiveConnectPlugin.h"
#include "nsIJRILiveConnectPIPeer.h"
#include "nsIPluginInputStream.h"
#include "nsObsoleteModuleLoading.h"

#ifdef XP_MAC
#include "EventFilter.h"
#include <MacWindows.h>
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

class CPluginManager : public nsIPluginManager2, public nsIServiceManager, public nsIMemory {
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
            
    NS_IMETHOD
    RegisterPlugin(REFNSIID aCID,
                   const char* aPluginName,
                   const char* aDescription,
                   const char** aMimeTypes,
                   const char** aMimeDescriptions,
                   const char** aFileExtensions,
                   PRInt32 aCount);

    NS_IMETHOD
    UnregisterPlugin(REFNSIID aCID);

    NS_IMETHOD
    GetURLWithHeaders(nsISupports* pluginInst, 
                      const char* url, 
                      const char* target = NULL,
                      nsIPluginStreamListener* streamListener = NULL,
                      const char* altHost = NULL,
                      const char* referrer = NULL,
                      PRBool forceJSEnabled = PR_FALSE,
                      PRUint32 getHeadersLength = 0, 
                      const char* getHeaders = NULL);
    

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
	AllocateMenuID(nsIEventHandler* handler, PRBool isSubmenu, PRInt16 *result);

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

    NS_IMETHOD
    GetService(const nsCID& aClass, const nsIID& aIID, void* *result);

    NS_IMETHOD
    GetServiceByContractID(const char *aContractID, const nsIID & aIID, void * *result)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

    NS_IMETHOD
    IsServiceInstantiated(const nsCID & aClass, const nsIID & aIID, PRBool *_retval)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
	}

    NS_IMETHOD
    IsServiceInstantiatedByContractID(const char *aContractID, const nsIID & aIID, PRBool *_retval)
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
    Alloc(size_t size);

    /**
     * Reallocates a block of memory to a new size.
     *
     * @param ptr - the block of memory to reallocate
     * @param size - the new size
     * @result the rellocated block of memory
     */
    NS_IMETHOD_(void*)
    Realloc(void* ptr, size_t size);

    /**
     * Frees a block of memory. 
     *
     * @param ptr - the block of memory to free
     */
    NS_IMETHOD_(void)
    Free(void* ptr);

    NS_IMETHOD
    IsLowMemory(PRBool *_retval)
    {
    	return NS_ERROR_NOT_IMPLEMENTED;
    }

    /**
     * Attempts to shrink the heap.
     */
    NS_IMETHOD
    HeapMinimize(PRBool aImmediate);

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
	static RegisteredWindow* theActiveWindow;
	
	static RegisteredWindow** GetRegisteredWindow(nsPluginPlatformWindowRef window);
	static RegisteredWindow* FindRegisteredWindow(nsPluginPlatformWindowRef window);

#ifdef XP_MAC
	Boolean mEventFiltersInstalled;

	static Boolean EventFilter(EventRecord* event);
	static Boolean MenuFilter(long menuSelection);
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

    /**
     * Writes data into the stream from an input stream.
     * Implementer's note: This method is defined by this interface in order
     * to allow the output stream to efficiently copy the data from the input
     * stream into its internal buffer (if any). If this method was provide
     * as an external facility, a separate char* buffer would need to be used
     * in order to call the output stream's other Write method.
     * @param fromStream the stream from which the data is read
     * @param aWriteCount out parameter to hold the number of
     *         bytes written. if an error occurs, the writecount
     *         is undefined
     *  @return error status
     */
    NS_IMETHOD
    Write(nsIInputStream* fromStream, PRUint32 *aWriteCount);

    /**
     * Flushes the stream.
     */
    NS_IMETHOD
    Flush(void);

    NS_IMETHOD
    WriteFrom(nsIInputStream *inStr, PRUint32 count, PRUint32 *_retval) {
        NS_NOTREACHED("WriteFrom");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    WriteSegments(nsReadSegmentFun reader, void * closure, PRUint32 count, PRUint32 *_retval) {
        NS_NOTREACHED("WriteSegments");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD
    IsNonBlocking(PRBool *aNonBlocking) {
        NS_NOTREACHED("IsNonBlocking");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

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

class CPluginInstancePeer : public nsIPluginInstancePeer, public nsIPluginTagInfo, public nsIJRILiveConnectPluginInstancePeer {
public:

    // XXX - I add parameters to the constructor because I wasn't sure if
    // XXX - the 4.0 browser had the npp_instance struct implemented.
    // XXX - If so, then I can access npp_instance through npp->ndata.
    CPluginInstancePeer(nsIPluginInstance* pluginInstance, NPP npp, nsMIMEType typeString, nsPluginMode type,
        PRUint16 attribute_cnt, const char** attribute_list, const char** values_list);

    virtual ~CPluginInstancePeer(void);

    NS_DECL_ISUPPORTS

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
    GetValue(nsPluginInstancePeerVariable variable, void *value);

    // (Corresponds to NPN_SetValue.)
    NS_IMETHOD
    SetValue(nsPluginInstancePeerVariable variable, void *value);

    /**
     * Returns the MIME type of the plugin instance. 
     *
     * (Corresponds to NPP_New's MIMEType argument.)
     *
     * @param result - resulting MIME type
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetMIMEType(nsMIMEType *result);

    /**
     * Returns the mode of the plugin instance, i.e. whether the plugin is
     * embedded in the html, or full page. 
     *
     * (Corresponds to NPP_New's mode argument.)
     *
     * @param result - the resulting mode
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetMode(nsPluginMode *result);

    /**
     * This operation is called by the plugin instance when it wishes to send
     * a stream of data to the browser. It constructs a new output stream to which
     * the plugin may send the data. When complete, the Close and Release methods
     * should be called on the output stream.
     *
     * (Corresponds to NPN_NewStream.)
     *
     * @param type - type MIME type of the stream to create
     * @param target - the target window name to receive the data
     * @param result - the resulting output stream
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result);

    /**
     * This operation causes status information to be displayed on the window
     * associated with the plugin instance. 
     *
     * (Corresponds to NPN_Status.)
     *
     * @param message - the status message to display
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    ShowStatus(const char* message);

    /**
     * Set the desired size of the window in which the plugin instance lives.
     *
     * @param width - new window width
     * @param height - new window height
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    SetWindowSize(PRUint32 width, PRUint32 height);

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

    /**
     * Returns the DOM element corresponding to the tag which references
     * this plugin in the document.
     *
     * REMIND: do we need to expose as an nsISupports * to avoid
     * introducing runtime dependencies on XPCOM?
     *
     * @param result - resulting DOM element
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetDOMElement(nsIDOMElement* *result);

	/**
	 * Returns a JRI env corresponding to the current Java thread of the
	 * browser.
     *
     * (Corresponds to NPN_GetJavaEnv.)
     *
     * @result - NS_OK if this operation was successful
	 */
	NS_IMETHOD
	GetJavaEnv(JRIEnv* *resultingEnv);

    /**
     * Returns a JRI reference to the Java peer object associated with the
     * plugin instance. This object is an instance of the class specified
     * by nsIJRILiveConnectPlugin::GetJavaClass.
     *
     * (Corresponds to NPN_GetJavaPeer.)
     *
     * @param resultingJavaPeer - a resulting reference to the Java instance
     * @result - NS_OK if this operation was successful
     */
    NS_IMETHOD
    GetJavaPeer(jref *resultingJavaPeer);

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
		return mInputStream->Available(result);
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

	NS_METHOD
	GetStreamOffset(PRInt32 *result)
	{
		*result = mStreamOffset;
		return NS_OK;
	}


	NS_METHOD
	SetStreamOffset(PRInt32 offset)
	{
		mStreamOffset = offset;
		return NS_OK;
	}

private:
	const char* mURL;
	nsIPluginInputStream* mInputStream;
	nsMIMEType mMimeType;
	PRBool mIsSeekable;
	PRInt32 mStreamOffset;
};

#pragma mark CPluginInputStream

class CPluginInputStream : public nsIPluginInputStream {
public:

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsIInputStream:

    /** Close the stream. */
    NS_IMETHOD
    Close(void);

    /** Return the number of bytes in the stream
     *  @param aLength out parameter to hold the length
     *         of the stream. if an error occurs, the length
     *         will be undefined
     *  @return error status
     */
    NS_IMETHOD
    Available(PRUint32 *aLength);

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

    NS_IMETHOD ReadSegments(nsWriteSegmentFun writer, void * closure, PRUint32 count, PRUint32 *_retval) {
        NS_NOTREACHED("ReadSegments");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD IsNonBlocking(PRBool *aNonBlocking) {
        NS_NOTREACHED("IsNonBlocking");
        return NS_ERROR_NOT_IMPLEMENTED;
    }

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
        if (mBuffer != NULL) delete[] mBuffer;
        mBuffer = dup(len, buffer);
        mBufferLength = len;
        mAmountRead = 0;
        return NS_OK;
    }
    
    static char* dup(PRUint32 len, const char* buffer) {
    	char* result = new char[len];
    	if (result != NULL)
    	    memcpy(result, buffer, len);
    	return result;
    }

	nsIPluginStreamInfo* CreatePluginStreamInfo(const char* url, nsMIMEType type, PRBool seekable) {
		if (mStreamInfo == NULL) {
			mStreamInfo = new CPluginStreamInfo(url, this, type, seekable);
			NS_IF_ADDREF(mStreamInfo);
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
#pragma export on
nsIPluginManager* thePluginManager = NULL;
nsIPlugin* thePlugin = NULL;
#pragma export off

//
// Interface IDs
//
#pragma mark IIDs

static NS_DEFINE_CID(kPluginCID, NS_PLUGIN_CID);
static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_CID(kMemoryCID, NS_MEMORY_CID);

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
	jref pluginClass = NULL;
	if (thePlugin != NULL) {
		nsIJRILiveConnectPlugin* jriPlugin = NULL;
		if (thePlugin->QueryInterface(NS_GET_IID(nsIJRILiveConnectPlugin), (void**)&jriPlugin) == NS_OK) {
			jriPlugin->GetJavaClass(NPN_GetJavaEnv(), &pluginClass);
			NS_RELEASE(jriPlugin);
		}
	}
	return pluginClass;
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
    thePlugin->CreatePluginInstance(thePluginManager, NS_GET_IID(nsIPluginInstance), pluginType, (void**)&pluginInstance);
    if (pluginInstance == NULL) {
        return NPERR_OUT_OF_MEMORY_ERROR;
    }
    
    // Create a new plugin instance peer,
    // XXX - Since np_instance is not implemented in the 4.0x browser, I
    // XXX - had to save the plugin parameter in the peer class.
    // XXX - Ask Warren about np_instance.
    CPluginInstancePeer* peer = new CPluginInstancePeer(pluginInstance, instance,
    													(nsMIMEType)pluginType, 
						                                (nsPluginMode)mode, (PRUint16)argc,
						                                (const char** )argn, (const char** )argv);
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
    mLiveconnect = NULL;

#ifdef XP_MAC
    mEventFiltersInstalled = false;
#endif
}

CPluginManager::~CPluginManager(void) 
{
	NS_IF_RELEASE(mLiveconnect);

#ifdef XP_MAC	
	if (mEventFiltersInstalled)
		RemoveEventFilters();
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
    nsresult rslt = pluginInst->QueryInterface(NS_GET_IID(nsIPluginInstance), (void**)&inst);
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
    nsresult rslt = pluginInst->QueryInterface(NS_GET_IID(nsIPluginInstance), (void**)&inst);
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

NS_IMETHODIMP
CPluginManager::RegisterPlugin(REFNSIID aCID,
                               const char* aPluginName,
                               const char* aDescription,
                               const char** aMimeTypes,
                               const char** aMimeDescriptions,
                               const char** aFileExtensions,
                               PRInt32 aCount)
{
    // XXXwaterson I don't think we need to do anything here.
    return NS_OK;
}

NS_IMETHODIMP
CPluginManager::UnregisterPlugin(REFNSIID aCID)
{
    // XXXwaterson I don't think we need to do anything here.
    return NS_OK;
}

NS_IMETHODIMP
CPluginManager::GetURLWithHeaders(nsISupports* pluginInst, 
                                  const char* url, 
                                  const char* target,
                                  nsIPluginStreamListener* streamListener,
                                  const char* altHost,
                                  const char* referrer,
                                  PRBool forceJSEnabled,
                                  PRUint32 getHeadersLength, 
                                  const char* getHeaders)
{
    return NS_ERROR_NOT_IMPLEMENTED;
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
CPluginManager::RegisteredWindow* CPluginManager::theActiveWindow = NULL;

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

CPluginManager::RegisteredWindow* CPluginManager::FindRegisteredWindow(nsPluginPlatformWindowRef window)
{
	RegisteredWindow** link = GetRegisteredWindow(window);
	return (link != NULL ? *link : NULL);
}


NS_METHOD
CPluginManager::RegisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
	theRegisteredWindows = new RegisteredWindow(theRegisteredWindows, handler, window);
	
#ifdef XP_MAC
	// use jGNE to obtain events for registered windows.
	if (!mEventFiltersInstalled) {
		::InstallEventFilters(&EventFilter, &MenuFilter);
		mEventFiltersInstalled = true;
	}

	// plugin expects the window to be shown and selected at this point.
	
	SInt16 variant = ::GetWVariant(window);
	if (variant == plainDBox) {
		::ShowHide(window, true);
		::BringToFront(window);
	} else {
		::ShowWindow(window);
		::SelectWindow(window);
	}
#endif

	return NS_OK;
}

NS_METHOD
CPluginManager::UnregisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
	RegisteredWindow** link = GetRegisteredWindow(window);
	if (link != NULL) {
		RegisteredWindow* registeredWindow = *link;
		if (registeredWindow == theActiveWindow)
			theActiveWindow = NULL;
		*link = registeredWindow->mNext;
		delete registeredWindow;
	}

#ifdef XP_MAC
	::HideWindow(window);

	// if no windows registered, remove the filter.
	if (theRegisteredWindows == NULL) {
		::RemoveEventFilters();
		mEventFiltersInstalled = false;
	}
#endif

	return NS_OK;
}

#ifdef XP_MAC

static void sendActivateEvent(nsIEventHandler* handler, WindowRef window, Boolean active)
{
	EventRecord event;
	::OSEventAvail(0, &event);
	event.what = activateEvt;
	event.message = UInt32(window);
	if (active)
		event.modifiers |= activeFlag;
	else
		event.modifiers &= ~activeFlag;

	nsPluginEvent pluginEvent = { &event, window };
	PRBool handled = PR_FALSE;

	handler->HandleEvent(&pluginEvent, &handled);
}

/**
 * This method filters events using a very low-level mechanism known as a jGNE filter.
 * This filter gets first crack at all events before they are returned by WaitNextEvent
 * or EventAvail. One trickiness is that the filter runs in all processes, so care
 * must be taken not to act on events if the browser's process isn't current.
 * So far, with activates, updates, and mouse clicks, it works quite well.
 */
Boolean CPluginManager::EventFilter(EventRecord* event)
{
	Boolean filteredEvent = false;

	WindowRef window = WindowRef(event->message);
    nsPluginEvent pluginEvent = { event, window };
    EventRecord simulatedEvent;

    RegisteredWindow* registeredWindow;
	PRBool handled = PR_FALSE;
    
	// see if this event is for one of our registered windows.
	switch (event->what) {
	case nullEvent:
		// See if the frontmost window is one of our registered windows.
		// we want to somehow deliver mouse enter/leave events.
		window = ::FrontWindow();
		registeredWindow = FindRegisteredWindow(window);
		if (registeredWindow != NULL) {
			simulatedEvent = *event;
			simulatedEvent.what = nsPluginEventType_AdjustCursorEvent;
			pluginEvent.event = &simulatedEvent;
			pluginEvent.window = registeredWindow->mWindow;
			registeredWindow->mHandler->HandleEvent(&pluginEvent, &handled);
		}
		break;
	case keyDown:
	case keyUp:
	case autoKey:
		// See if the frontmost window is one of our registered windows.
		window = ::FrontWindow();
		registeredWindow = FindRegisteredWindow(window);
		if (registeredWindow != NULL) {
			pluginEvent.window = window;
			registeredWindow->mHandler->HandleEvent(&pluginEvent, &handled);
			filteredEvent = true;
		}
		break;
	case mouseDown:
		// use FindWindow to see if the click was in one our registered windows.
		short partCode = FindWindow(event->where, &window);
		switch (partCode) {
		case inContent:
		case inDrag:
		case inGrow:
		case inGoAway:
		case inZoomIn:
		case inZoomOut:
		case inCollapseBox:
		case inProxyIcon:
			registeredWindow = FindRegisteredWindow(window);
			if (registeredWindow != NULL) {
				// make sure this window has been activated before passing it the click.
				if (theActiveWindow == NULL) {
					sendActivateEvent(registeredWindow->mHandler, window, true);
					theActiveWindow = registeredWindow;
				}
				pluginEvent.window = window;
				registeredWindow->mHandler->HandleEvent(&pluginEvent, &handled);
				filteredEvent = true;
			} else if (theActiveWindow != NULL) {
				// a click is going into an unregistered window, if we are active,
				// the browser doesn't seem to be generating a deactivate event.
				// I think this is because PowerPlant is managing the windows, dang it.
				window = theActiveWindow->mWindow;
				sendActivateEvent(theActiveWindow->mHandler, window, false);
				::HiliteWindow(window, false);
				theActiveWindow = NULL;
			}
			break;
		}
		break;
	case activateEvt:
		registeredWindow = FindRegisteredWindow(window);
		if (registeredWindow != NULL) {
			registeredWindow->mHandler->HandleEvent(&pluginEvent, &handled);
			filteredEvent = true;
			theActiveWindow = registeredWindow;
		}
		break;
	case updateEvt:
		registeredWindow = FindRegisteredWindow(window);
		if (registeredWindow != NULL) {
			GrafPtr port; GetPort(&port); SetPort(window); BeginUpdate(window);
				registeredWindow->mHandler->HandleEvent(&pluginEvent, &handled);
			EndUpdate(window); SetPort(port);
			filteredEvent = true;
		}
		break;
	case osEvt:
		if ((event->message & osEvtMessageMask) == (suspendResumeMessage << 24)) {
			registeredWindow = theActiveWindow;
			if (registeredWindow != NULL) {
				window = registeredWindow->mWindow;
				Boolean active = (event->message & resumeFlag) != 0;
				sendActivateEvent(registeredWindow->mHandler, window, active);
				pluginEvent.window = window;
				registeredWindow->mHandler->HandleEvent(&pluginEvent, &handled);
				::HiliteWindow(window, active);
			}
		}
		break;
	}
	
	return filteredEvent;
}

// TODO:  find out what range of menus Communicator et. al. uses.
enum {
	kBaseMenuID = 20000,
	kBaseSubMenuID = 200
};

static PRInt16 nextMenuID = kBaseMenuID;
static PRInt16 nextSubMenuID = kBaseSubMenuID;

Boolean CPluginManager::MenuFilter(long menuSelection)
{
	if (theActiveWindow != NULL) {
		UInt16 menuID = (menuSelection >> 16);
		if ((menuID >= kBaseMenuID && menuID < nextMenuID) || (menuID >= kBaseSubMenuID && menuID < nextSubMenuID)) {
			EventRecord menuEvent;
			::OSEventAvail(0, &menuEvent);
			menuEvent.what = nsPluginEventType_MenuCommandEvent;
			menuEvent.message = menuSelection;

			WindowRef window = theActiveWindow->mWindow;
	    	nsPluginEvent pluginEvent = { &menuEvent, window };
			PRBool handled = PR_FALSE;
			theActiveWindow->mHandler->HandleEvent(&pluginEvent, &handled);
			
			return handled;
		}
	}
	return false;
}

NS_METHOD
CPluginManager::AllocateMenuID(nsIEventHandler* handler, PRBool isSubmenu, PRInt16 *result)
{
	*result = (isSubmenu ? nextSubMenuID++ : nextMenuID++);

	return NS_OK;
}

#else /* !XP_MAC */

NS_METHOD
CPluginManager::AllocateMenuID(nsIEventHandler* handler, PRBool isSubmenu, PRInt16 *result)
{
	return NS_ERROR_NOT_IMPLEMENTED;
}

#endif /* XP_MAC */

//////////////////////////////
// nsIServiceManager methods.
//////////////////////////////

NS_METHOD
CPluginManager::GetService(const nsCID& aClass, const nsIID& aIID, void* *result)
{
    // the only service we support currently is nsIMemory.
    if (aClass.Equals(kPluginManagerCID) || aClass.Equals(kMemoryCID)) {
        return QueryInterface(aIID, result);
    }
    if (!aClass.Equals(nsILiveconnect::GetCID())) {
        return NS_ERROR_SERVICE_NOT_FOUND;
    }
    if (mLiveconnect == NULL) {
        mLiveconnect = new nsLiveconnect;
        if (!mLiveconnect)
            return NS_ERROR_OUT_OF_MEMORY;
        NS_ADDREF(mLiveconnect);
    }
    return mLiveconnect->QueryInterface(aIID, result);
}

//////////////////////////////
// nsIMemory methods.
//////////////////////////////

NS_METHOD_(void*)
CPluginManager::Alloc(size_t size)
{
	return ::NPN_MemAlloc(size);
}

NS_METHOD_(void*)
CPluginManager::Realloc(void* ptr, size_t size)
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

NS_METHOD_(void)
CPluginManager::Free(void* ptr)
{
	if (ptr != NULL) {
		::NPN_MemFree(ptr);
	}
}

NS_METHOD
CPluginManager::HeapMinimize(PRBool aImmediate)
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
    
    if (iid.Equals(NS_GET_IID(nsIPluginManager)) || iid.Equals(NS_GET_IID(nsIPluginManager2))) {
        *ptr = (void*) ((nsIPluginManager2*)this);                        
        AddRef();                                                            
        return NS_OK;                                                        
    }                                                                      
    if (iid.Equals(NS_GET_IID(nsIServiceManager))) {                                                          
        *ptr = (void*) (nsIServiceManager*)this;                                        
        AddRef();                                                            
        return NS_OK;                                                        
    }
    if (iid.Equals(NS_GET_IID(nsIMemory))) {                                                          
        *ptr = (void*) (nsIMemory*)this;                                        
        AddRef();                                                            
        return NS_OK;                                                        
    }
    if (iid.Equals(NS_GET_IID(nsISupports))) {
        *ptr = (void*) this;                        
        AddRef();                                                            
        return NS_OK;                                                        
    }                                                                      
    return NS_NOINTERFACE;                                                 
}

NS_IMPL_ADDREF(CPluginManager)
NS_IMPL_RELEASE(CPluginManager)

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

/**
 * Returns a JRI env corresponding to the current Java thread of the
 * browser.
 *
 * (Corresponds to NPN_GetJavaEnv.)
 *
 * @result - NS_OK if this operation was successful
 */
NS_METHOD
CPluginInstancePeer::GetJavaEnv(JRIEnv* *resultingEnv)
{
	*resultingEnv = NPN_GetJavaEnv();
	return NS_OK;
}

/**
 * Returns a JRI reference to the Java peer object associated with the
 * plugin instance. This object is an instance of the class specified
 * by nsIJRILiveConnectPlugin::GetJavaClass.
 *
 * (Corresponds to NPN_GetJavaPeer.)
 *
 * @param resultingJavaPeer - a resulting reference to the Java instance
 * @result - NS_OK if this operation was successful
 */
NS_METHOD
CPluginInstancePeer::GetJavaPeer(jref *resultingJavaPeer)
{
	*resultingJavaPeer = NPN_GetJavaPeer(npp);
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

/**
 * Returns the DOM element corresponding to the tag which references
 * this plugin in the document.
 *
 * REMIND: do we need to expose as an nsISupports * to avoid
 * introducing runtime dependencies on XPCOM?
 *
 * @param result - resulting DOM element
 * @result - NS_OK if this operation was successful
 */
NS_METHOD
CPluginInstancePeer::GetDOMElement(nsIDOMElement* *result)
{
/** PENDING **/

	return NS_OK;
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
  if (error != NPERR_NO_ERROR)
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

NS_IMPL_ADDREF(CPluginInstancePeer)
NS_IMPL_RELEASE(CPluginInstancePeer)

NS_METHOD
CPluginInstancePeer::QueryInterface(const nsIID& iid, void** ptr) 
{                                                                        
    if (NULL == ptr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }                                                                      
  
    if (iid.Equals(NS_GET_IID(nsIPluginInstancePeer))) {
        *ptr = (void*) this;                                        
        AddRef();                                                            
        return NS_OK;                                                        
    }                                                                      
    if (iid.Equals(NS_GET_IID(nsIPluginTagInfo)) || iid.Equals(NS_GET_IID(nsISupports))) {                                      
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

NS_METHOD
CPluginManagerStream::Write(nsIInputStream* fromStream, PRUint32 *aWriteCount)
{
	nsresult rv = fromStream->Available(aWriteCount);
	if (rv == NS_OK) {
		char buffer[1024];
		PRUint32 len = *aWriteCount;
		while (len > 0) {		
			PRUint32 count = (len < sizeof(buffer) ? len : sizeof(buffer));
			rv = fromStream->Read(buffer, count, &count);
			if (rv == NS_OK)
				rv = Write(buffer, count, &count);
			if (rv != NS_OK) {
				*aWriteCount -= len;
				break;
			}
			len -= count;
		}
	}
	return rv;
}

NS_METHOD
CPluginManagerStream::Flush()
{
	return NS_OK;
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

NS_IMPL_ISUPPORTS1(CPluginManagerStream, nsIOutputStream)

//////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(CPluginStreamInfo, nsIPluginStreamInfo)

CPluginInputStream::CPluginInputStream(nsIPluginStreamListener* listener)
    : mListener(listener), mStreamType(nsPluginStreamType_Normal),
      mNPP(NULL), mStream(NULL),
      mBuffer(NULL), mBufferLength(0), mAmountRead(0),
      mStreamInfo(NULL)
{
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

NS_IMPL_ISUPPORTS1(CPluginInputStream, nsIPluginInputStream)

NS_METHOD
CPluginInputStream::Close(void)
{
    if (mNPP == NULL || mStream == NULL)
        return NS_ERROR_FAILURE;
    NPError err = NPN_DestroyStream(mNPP, mStream, NPRES_USER_BREAK);
    return fromNPError[err];
}

NS_METHOD
CPluginInputStream::Available(PRUint32 *aLength)
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

