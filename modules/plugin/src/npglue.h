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

#ifndef npglue_h__
#define npglue_h__

#if defined(__cplusplus)
extern "C" {
#endif

#include "net.h"
#include "np.h"
#include "nppg.h"
#include "client.h"
#include "xpassert.h" 
#include "ntypes.h"
#include "fe_proto.h"
#include "cvactive.h"
#include "gui.h"			/* For XP_AppCodeName */
#include "merrors.h"
#include "xpgetstr.h"
#include "nsICapsManager.h"
#ifdef JAVA
#include "java.h"
#endif
#include "nppriv.h"
#include "shist.h"

#include "prefapi.h"
#include "proto.h"

#include "libmocha.h"
#include "libevent.h"
#include "layout.h"         /* XXX From ../layout */

#ifdef LAYERS
#include "layers.h"
#endif /* LAYERS */

#include "nsplugin.h"
#include "nsAgg.h"      /* nsPluginManager aggregates nsJVMManager */
#ifdef OJI
#include "nsjvm.h"
#else  /* OJI */
/* Just define a dummy struct for nsIJVMManager. In the
   plugin code, it is never actually dereferenced outside of an
   `#ifdef OJI'. */
struct nsIJVMManager;
class nsHashtable;
#endif /* OJI */

#include "plstr.h"

extern int XP_PLUGIN_LOADING_PLUGIN;
extern int MK_BAD_CONNECT;
extern int XP_PLUGIN_NOT_FOUND;
extern int XP_PLUGIN_CANT_LOAD_PLUGIN;
extern int XP_PROGRESS_STARTING_JAVA;

#define NP_LOCK    1
#define NP_UNLOCK  0

#define NPTRACE(n, msg)	TRACEMSG(msg)

#define RANGE_EQUALS  "bytes="

/* @@@@ steal the private call from netlib */
extern void NET_SetCallNetlibAllTheTime(MWContext *context, char *caller);
extern void NET_ClearCallNetlibAllTheTime(MWContext *context, char *caller);

#if defined(XP_WIN) || defined(XP_OS2)
/* Can't include FEEMBED.H because it's full of C++ */
extern NET_StreamClass *EmbedStream(int iFormatOut, void *pDataObj, URL_Struct *pUrlData, MWContext *pContext);
extern void EmbedUrlExit(URL_Struct *pUrl, int iStatus, MWContext *pContext);
#endif

extern void NET_RegisterAllEncodingConverters(char* format_in, FO_Present_Types format_out);


/* Internal prototypes */

void
NPL_EmbedURLExit(URL_Struct *urls, int status, MWContext *cx);

void
NPL_URLExit(URL_Struct *urls, int status, MWContext *cx);

void
np_streamAsFile(np_stream* stream);

NPError
np_switchHandlers(np_instance* instance,
				  np_handle* newHandle,
				  np_mimetype* newMimeType,
				  char* requestedType);

NET_StreamClass*
np_newstream(URL_Struct *urls, np_handle *handle, np_instance *instance);

void
np_findPluginType(NPMIMEType type, void* pdesc, np_handle** outHandle, np_mimetype** outMimetype);

void 
np_enablePluginType(np_handle* handle, np_mimetype* mimetype, XP_Bool enabled);

void
np_bindContext(NPEmbeddedApp* app, MWContext* cx);

void
np_unbindContext(NPEmbeddedApp* app, MWContext* cx);

void
np_deleteapp(MWContext* cx, NPEmbeddedApp* app);

np_instance*
np_newinstance(np_handle *handle, MWContext *cx, NPEmbeddedApp *app,
			   np_mimetype *mimetype, char *requestedType);
			   				
void
np_delete_instance(np_instance *instance);

void
np_recover_mochaWindow(JRIEnv * env, np_instance * instance);

XP_Bool
np_FakeHTMLStream(URL_Struct* urls, MWContext* cx, char * fakehtml);

/* Navigator plug-in API function prototypes */

/*
 * Use this macro before each exported function
 * (between the return address and the function
 * itself), to ensure that the function has the
 * right calling conventions on Win16.
 */
#ifdef XP_WIN16
#define NP_EXPORT __export
#elif defined(XP_OS2)
#define NP_EXPORT _System
#else
#define NP_EXPORT
#endif

NPError NP_EXPORT
npn_requestread(NPStream *pstream, NPByteRange *rangeList);

NPError NP_EXPORT
npn_geturlnotify(NPP npp, const char* relativeURL, const char* target, void* notifyData);

NPError NP_EXPORT
npn_getvalue(NPP npp, NPNVariable variable, void *r_value);

NPError NP_EXPORT
npn_setvalue(NPP npp, NPPVariable variable, void *r_value);

NPError NP_EXPORT
npn_geturl(NPP npp, const char* relativeURL, const char* target);

NPError NP_EXPORT
npn_posturlnotify(NPP npp, const char* relativeURL, const char *target,
                  uint32 len, const char *buf, NPBool file, void* notifyData);

NPError NP_EXPORT
npn_posturl(NPP npp, const char* relativeURL, const char *target, uint32 len,
            const char *buf, NPBool file);

NPError
np_geturlinternal(NPP npp, const char* relativeURL, const char* target, 
                  const char* altHost, const char* referer, PRBool forceJSEnabled,
                  NPBool notify, void* notifyData);

NPError
np_posturlinternal(NPP npp, const char* relativeURL, const char *target, 
                   const char* altHost, const char* referer, PRBool forceJSEnabled,
                   uint32 len, const char *buf, NPBool file, NPBool notify, void* notifyData);

NPError NP_EXPORT
npn_newstream(NPP npp, NPMIMEType type, const char* window, NPStream** pstream);

int32 NP_EXPORT
npn_write(NPP npp, NPStream *pstream, int32 len, void *buffer);

NPError NP_EXPORT
npn_destroystream(NPP npp, NPStream *pstream, NPError reason);

void NP_EXPORT
npn_status(NPP npp, const char *message);

void NP_EXPORT
npn_registerwindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window);

void NP_EXPORT
npn_unregisterwindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window);

#if 0
int16 NP_EXPORT
npn_allocateMenuID(NPP npp, XP_Bool isSubmenu);
#endif

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif
const char* NP_EXPORT
npn_useragent(NPP npp);
#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif
void* NP_EXPORT
npn_memalloc (uint32 size);
#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif


void NP_EXPORT
npn_memfree (void *ptr);

uint32 NP_EXPORT
npn_memflush(uint32 size);

void NP_EXPORT
npn_reloadplugins(NPBool reloadPages);

void NP_EXPORT
npn_invalidaterect(NPP npp, NPRect *invalidRect);

void NP_EXPORT
npn_invalidateregion(NPP npp, NPRegion invalidRegion);

void NP_EXPORT
npn_forceredraw(NPP npp);

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif
#if defined(OJI)
JNIEnv* NP_EXPORT
npn_getJavaEnv(void);
#else
JRIEnv* NP_EXPORT
npn_getJavaEnv(void);
#endif
#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif


#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif
jref NP_EXPORT
npn_getJavaPeer(NPP npp);
#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif

extern NPError
npn_SetWindowSize(np_instance* instance, NPSize* pnpsz);

/* End of function prototypes */

/* this is a hack for now */
#define NP_MAXBUF (0xE000)

////////////////////////////////////////////////////////////////////////////////

class nsPluginManager : public nsIPluginManager2
{
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginManager:

    NS_IMETHOD
    GetValue(nsPluginManagerVariable variable, void *value);

    // (Corresponds to NPN_ReloadPlugins.)
    NS_IMETHOD
    ReloadPlugins(PRBool reloadPages);

    // (Corresponds to NPN_UserAgent.)
    NS_IMETHOD
    UserAgent(const char* *result);

#ifdef NEW_PLUGIN_STREAM_API

    NS_IMETHOD
    GetURL(nsISupports* pluginInst, 
           const char* url, 
           const char* target = NULL,
           nsIPluginStreamListener* streamListener = NULL,
           nsPluginStreamType streamType = nsPluginStreamType_Normal,
           const char* altHost = NULL,
           const char* referrer = NULL,
           PRBool forceJSEnabled = PR_FALSE);

    NS_IMETHOD
    PostURL(nsISupports* pluginInst,
            const char* url,
            PRUint32 postDataLen, 
            const char* postData,
            PRBool isFile = PR_FALSE,
            const char* target = NULL,
            nsIPluginStreamListener* streamListener = NULL,
            nsPluginStreamType streamType = nsPluginStreamType_Normal,
            const char* altHost = NULL, 
            const char* referrer = NULL,
            PRBool forceJSEnabled = PR_FALSE,
            PRUint32 postHeadersLength = 0, 
            const char* postHeaders = NULL);

#else // !NEW_PLUGIN_STREAM_API
    NS_IMETHOD
    GetURL(nsISupports* peer, const char* url, const char* target,
           void* notifyData = NULL, const char* altHost = NULL,
           const char* referrer = NULL, PRBool forceJSEnabled = PR_FALSE);

    NS_IMETHOD
    PostURL(nsISupports* peer, const char* url, const char* target,
            PRUint32 postDataLen, const char* postData,
            PRBool isFile = PR_FALSE, void* notifyData = NULL,
            const char* altHost = NULL, const char* referrer = NULL,
            PRBool forceJSEnabled = PR_FALSE,
            PRUint32 postHeadersLength = 0, const char* postHeaders = NULL);
#endif // !NEW_PLUGIN_STREAM_API

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginManager2:

    NS_IMETHOD
    BeginWaitCursor(void);

    NS_IMETHOD
    EndWaitCursor(void);

    NS_IMETHOD
    SupportsURLProtocol(const char* protocol, PRBool *result);

    // This method may be called by the plugin to indicate that an error has
    // occurred, e.g. that the plugin has failed or is shutting down spontaneously.
    // This allows the browser to clean up any plugin-specific state.
    NS_IMETHOD
    NotifyStatusChange(nsIPlugin* plugin, nsresult errorStatus);
    
    NS_IMETHOD
    FindProxyForURL(const char* url, char* *result);

    ////////////////////////////////////////////////////////////////////////////
    // New top-level window handling calls for Mac:
    
    NS_IMETHOD
    RegisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window);
    
    NS_IMETHOD
    UnregisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window);

	// Menu ID allocation calls for Mac:
    NS_IMETHOD
	AllocateMenuID(nsIEventHandler* handler, PRBool isSubmenu, PRInt16 *result);

    NS_IMETHOD
	DeallocateMenuID(nsIEventHandler* handler, PRInt16 menuID);

	/**
	 * Indicates whether this event handler has allocated the given menu ID.
	 */
    NS_IMETHOD
    HasAllocatedMenuID(nsIEventHandler* handler, PRInt16 menuID, PRBool *result);

#if 0
	// On the mac (and most likely win16), network activity can
    // only occur on the main thread. Therefore, we provide a hook
    // here for the case that the main thread needs to tickle itself.
    // In this case, we make sure that we give up the monitor so that
    // the tickle code can notify it without freezing.
    NS_IMETHOD
    ProcessNextEvent(PRBool *bEventHandled);
#endif

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginManager specific methods:

    NS_DECL_AGGREGATED

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);
    
protected:
    nsPluginManager(nsISupports* outer);
    virtual ~nsPluginManager(void);

    // aggregated interfaces:
    nsIJVMManager* GetJVMMgr(const nsIID& aIID);
    nsICapsManager* GetCapsManager(const nsIID& aIID);
#ifdef OJI
    nsILiveconnect* GetLiveconnect(const nsIID& aIID);
#endif /* OJI */

    nsISupports*        fJVMMgr;
    nsISupports*        fMalloc;
    nsISupports*        fFileUtils;

    PRUint16            fWaiting;
    void*               fOldCursor;
    
    nsHashtable*		fAllocatedMenuIDs;
    nsISupports*        fCapsManager;
    nsISupports*        fLiveconnect;
};

extern nsPluginManager* thePluginManager;

////////////////////////////////////////////////////////////////////////////////

class nsFileUtilities : public nsIFileUtilities {
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIFileUtilities:
    
    NS_IMETHOD
    GetProgramPath(const char* *result);

    NS_IMETHOD
    GetTempDirPath(const char* *result);

    NS_IMETHOD
    NewTempFileName(const char* prefix, PRUint32 bufLen, char* resultBuf);

    ////////////////////////////////////////////////////////////////////////////
    // nsFileUtilities specific methods:

    nsFileUtilities(nsISupports* outer);
    virtual ~nsFileUtilities(void);

    NS_DECL_AGGREGATED

    static NS_METHOD
    Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr);
    
    void SetProgramPath(const char* path) { fProgramPath = path; }

protected:
    const char*         fProgramPath;

};

////////////////////////////////////////////////////////////////////////////////
typedef struct JSContext JSContext;

class nsPluginTagInfo;

class nsPluginInstancePeer : public nsIPluginInstancePeer,
                             public nsILiveConnectPluginInstancePeer,
                             public nsIWindowlessPluginInstancePeer
{
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginInstancePeer:

    NS_IMETHOD
    GetValue(nsPluginInstancePeerVariable variable, void *value);

    // (Corresponds to NPP_New's MIMEType argument.)
    NS_IMETHOD
    GetMIMEType(nsMIMEType *result);

    // (Corresponds to NPP_New's mode argument.)
    NS_IMETHOD
    GetMode(nsPluginMode *result);

    // (Corresponds to NPN_NewStream.)
    NS_IMETHOD
    NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result);

    // (Corresponds to NPN_Status.)
    NS_IMETHOD
    ShowStatus(const char* message);

    NS_IMETHOD
    SetWindowSize(PRUint32 width, PRUint32 height);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIJRILiveConnectPluginInstancePeer:

    // (Corresponds to NPN_GetJavaPeer.)
    NS_IMETHOD
    GetJavaPeer(jobject *result);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIWindowlessPluginInstancePeer:

    // (Corresponds to NPN_InvalidateRect.)
    NS_IMETHOD
    InvalidateRect(nsPluginRect *invalidRect);

    // (Corresponds to NPN_InvalidateRegion.)
    NS_IMETHOD
    InvalidateRegion(nsPluginRegion invalidRegion);

    // (Corresponds to NPN_ForceRedraw.)
    NS_IMETHOD
    ForceRedraw(void);

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginInstancePeer specific methods:

    nsPluginInstancePeer(NPP npp);
    virtual ~nsPluginInstancePeer(void);

    NS_DECL_ISUPPORTS

    void SetPluginInstance(nsIPluginInstance* inst);
    nsIPluginInstance* GetPluginInstance(void);
    
    NPP GetNPP(void);
    JSContext *GetJSContext(void);
    MWContext *GetMWContext(void);
protected:

    // NPP is the old plugin structure. If we were implementing this
    // from scratch we wouldn't use it, but for now we're calling the old
    // npglue.c routines wherever possible.
    NPP fNPP;

    nsIPluginInstance*  fPluginInst;
    nsPluginTagInfo*    fTagInfo;
};

#define NS_PLUGININSTANCEPEER_CID                    \
{ /* 766432d0-01ba-11d2-815b-006008119d7a */         \
    0x766432d0,                                      \
    0x01ba,                                          \
    0x11d2,                                          \
    {0x81, 0x5b, 0x00, 0x60, 0x08, 0x11, 0x9d, 0x7a} \
}

////////////////////////////////////////////////////////////////////////////////

class nsPluginTagInfo : public nsIPluginTagInfo2 {
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginTagInfo:

    // Get a ptr to the paired list of attribute names and values,
    // returns the length of the array.
    //
    // Each name or value is a null-terminated string.
    //
    NS_IMETHOD
    GetAttributes(PRUint16& n, const char*const*& names, const char*const*& values);

    // Get the value for the named attribute.  Returns null
    // if the attribute was not set.
    NS_IMETHOD
    GetAttribute(const char* name, const char* *result);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginTagInfo2:

    // Get the type of the HTML tag that was used ot instantiate this
    // plugin.  Currently supported tags are EMBED, OBJECT and APPLET.
    NS_IMETHOD
    GetTagType(nsPluginTagType *result);

    // Get the complete text of the HTML tag that was
    // used to instantiate this plugin
    NS_IMETHOD
    GetTagText(const char * *result);

    // Get a ptr to the paired list of parameter names and values,
    // returns the length of the array.
    //
    // Each name or value is a null-terminated string.
    NS_IMETHOD
    GetParameters(PRUint16& n, const char*const*& names, const char*const*& values);

    // Get the value for the named parameter.  Returns null
    // if the parameter was not set.
    NS_IMETHOD
    GetParameter(const char* name, const char* *result);
    
    NS_IMETHOD
    GetDocumentBase(const char* *result);
    
    // Return an encoding whose name is specified in:
    // http://java.sun.com/products/jdk/1.1/docs/guide/intl/intl.doc.html#25303
    NS_IMETHOD
    GetDocumentEncoding(const char* *result);
    
    NS_IMETHOD
    GetAlignment(const char* *result);
    
    NS_IMETHOD
    GetWidth(PRUint32 *result);
    
    NS_IMETHOD
    GetHeight(PRUint32 *result);
    
    NS_IMETHOD
    GetBorderVertSpace(PRUint32 *result);
    
    NS_IMETHOD
    GetBorderHorizSpace(PRUint32 *result);

    // Returns a unique id for the current document on which the
    // plugin is displayed.
    NS_IMETHOD
    GetUniqueID(PRUint32 *result);

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginTagInfo specific methods:
    
    nsPluginTagInfo(NPP npp);
    virtual ~nsPluginTagInfo(void);

    NS_DECL_AGGREGATED

protected:
    LO_CommonPluginStruct* GetLayoutElement(void)
    {
        np_instance* instance = (np_instance*) npp->ndata;
        NPEmbeddedApp* app = instance->app;
        np_data* ndata = (np_data*) app->np_data;
        return (LO_CommonPluginStruct*)ndata->lo_struct;
    }

    // aggregated interfaces:
    nsISupports*        fJVMPluginTagInfo;

    NPP                 npp;
    PRUint32            fUniqueID;
};

////////////////////////////////////////////////////////////////////////////////

class nsPluginManagerStream : public nsIOutputStream {
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIBaseStream:

    NS_IMETHOD
    Close(void);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIOutputStream:

    /** Write data into the stream.
     *  @param aBuf the buffer into which the data is read
     *  @param aOffset the start offset of the data
     *  @param aCount the maximum number of bytes to read
     *  @return number of bytes read or an error if < 0
     */   
    NS_IMETHOD
    Write(const char* aBuf, PRInt32 aCount, PRInt32 *resultingCount); 

    NS_IMETHOD Write(nsIInputStream* fromStream, PRUint32 *aWriteCount) {
        return NS_ERROR_NOT_IMPLEMENTED;
    }

    NS_IMETHOD Flush() {
        return NS_OK;
    }

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginManagerStream specific methods:

    nsPluginManagerStream(NPP npp, NPStream* pstr);
    virtual ~nsPluginManagerStream(void);

    NS_DECL_ISUPPORTS

protected:
    NPP npp;
    NPStream* pstream;

};

////////////////////////////////////////////////////////////////////////////////

#ifdef NEW_PLUGIN_STREAM_API

#include "nsIPluginInputStream2.h"

class nsPluginInputStream;

// stored in the fe_data of the URL_Struct:
struct nsPluginURLData {
    NPEmbeddedApp* app;
    nsIPluginStreamListener* listener;
    nsPluginInputStream* inStr;
};

class nsPluginInputStream : public nsIPluginInputStream2 {
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
    GetLength(PRInt32 *aLength);

    /** Read data from the stream.
     *  @param aErrorCode the error code if an error occurs
     *  @param aBuf the buffer into which the data is read
     *  @param aOffset the start offset of the data
     *  @param aCount the maximum number of bytes to read
     *  @param aReadCount out parameter to hold the number of
     *         bytes read, eof if 0. if an error occurs, the
     *         read count will be undefined
     *  @return error status
     */   
    NS_IMETHOD
    Read(char* aBuf, PRInt32 aOffset, PRInt32 aCount, PRInt32 *aReadCount); 

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginInputStream:

    // (Corresponds to NPStream's lastmodified field.)
    NS_IMETHOD
    GetLastModified(PRUint32 *result);

    NS_IMETHOD
    RequestRead(nsByteRange* rangeList);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginInputStream2:

    NS_IMETHOD
    GetContentLength(PRUint32 *result);

    NS_IMETHOD
    GetHeaderFields(PRUint16& n, const char*const*& names, const char*const*& values);

    NS_IMETHOD
    GetHeaderField(const char* name, const char* *result);

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginInputStream specific methods:

    nsPluginInputStream(nsIPluginStreamListener* listener,
                        nsPluginStreamType streamType);
    virtual ~nsPluginInputStream(void);

    nsIPluginStreamListener* GetListener(void) { return mListener; }
    nsPluginStreamType GetStreamType(void) { return mStreamType; }
    PRBool IsClosed(void) { return mClosed; }

    void SetStreamInfo(URL_Struct* urls, np_stream* stream) {
        mUrls = urls;
        mStream = stream;
    }

    nsresult ReceiveData(const char* buffer, PRUint32 offset, PRUint32 len);
    void Cleanup(void);

protected:
    nsIPluginStreamListener* mListener;
    nsPluginStreamType mStreamType;
    URL_Struct* mUrls;
    np_stream* mStream;

    struct BufferElement {
        BufferElement* next;
        char* segment;
        PRUint32 offset;
        PRUint32 length;
    };

    BufferElement* mBuffer;
    PRBool mClosed;
//    PRUint32 mReadCursor;

//    PRUint32 mBufferLength;
//    PRUint32 mAmountRead;

};

#else // !NEW_PLUGIN_STREAM_API

class nsPluginStreamPeer : public nsIPluginStreamPeer2, 
                           public nsISeekablePluginStreamPeer
{
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginStreamPeer:

    // (Corresponds to NPStream's url field.)
    NS_IMETHOD
    GetURL(const char* *result);

    // (Corresponds to NPStream's end field.)
    NS_IMETHOD
    GetEnd(PRUint32 *result);

    // (Corresponds to NPStream's lastmodified field.)
    NS_IMETHOD
    GetLastModified(PRUint32 *result);

    // (Corresponds to NPStream's notifyData field.)
    NS_IMETHOD
    GetNotifyData(void* *result);

    // (Corresponds to NPP_DestroyStream's reason argument.)
    NS_IMETHOD
    GetReason(nsPluginReason *result);

    // (Corresponds to NPP_NewStream's MIMEType argument.)
    NS_IMETHOD
    GetMIMEType(nsMIMEType *result);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginStreamPeer2:

    NS_IMETHOD
    GetContentLength(PRUint32 *result);

    NS_IMETHOD
    GetHeaderFieldCount(PRUint32 *result);

    NS_IMETHOD
    GetHeaderFieldKey(PRUint32 index, const char* *result);

    NS_IMETHOD
    GetHeaderField(PRUint32 index, const char* *result);

    ////////////////////////////////////////////////////////////////////////////
    // from nsISeekablePluginStreamPeer:

    // (Corresponds to NPN_RequestRead.)
    NS_IMETHOD
    RequestRead(nsByteRange* rangeList);

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginStreamPeer specific methods:
    
    nsPluginStreamPeer(URL_Struct *urls, np_stream *stream);
    virtual ~nsPluginStreamPeer(void);

    NS_DECL_ISUPPORTS
    
    nsIPluginStream* GetUserStream(void) {
        return userStream;
    }

    void SetUserStream(nsIPluginStream* str) {
        userStream = str;
    }

    void SetReason(nsPluginReason r) {
        reason = r;
    }

protected:
    nsIPluginStream* userStream;
    URL_Struct *urls;
    np_stream *stream;
    nsPluginReason reason;

};

#endif // !NEW_PLUGIN_STREAM_API

////////////////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* npglue_h__ */
