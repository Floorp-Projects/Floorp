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
#ifdef JAVA
#include "java.h"
#endif
#include "nppriv.h"
#include "shist.h"

#include "prefapi.h"
#include "proto.h"

#ifdef MOCHA
#include "libmocha.h"
#include "libevent.h"
#include "layout.h"         /* XXX From ../layout */
#endif

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
#endif /* OJI */

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
npn_registerwindow(NPP npp, void* window);

void NP_EXPORT
npn_unregisterwindow(NPP npp, void* window);

int16 NP_EXPORT
npn_allocateMenuID(NPP npp, XP_Bool isSubmenu);

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
npn_getJavaEnv(PRThread *pPRThread);
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


/* End of function prototypes */


/* this is a hack for now */
#define NP_MAXBUF (0xE000)

////////////////////////////////////////////////////////////////////////////////

class nsPluginManager : public nsIPluginManager2 {
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginManager:

    // (Corresponds to NPN_ReloadPlugins.)
    NS_IMETHOD_(void)
    ReloadPlugins(PRBool reloadPages);

    // (Corresponds to NPN_MemAlloc.)
    NS_IMETHOD_(void*)
    MemAlloc(PRUint32 size);

    // (Corresponds to NPN_MemFree.)
    NS_IMETHOD_(void)
    MemFree(void* ptr);

    // (Corresponds to NPN_MemFlush.)
    NS_IMETHOD_(PRUint32)
    MemFlush(PRUint32 size);

    // (Corresponds to NPN_UserAgent.)
    NS_IMETHOD_(const char*)
    UserAgent(void);

    // (Corresponds to NPN_GetURL and NPN_GetURLNotify.)
    NS_IMETHOD_(nsPluginError)
    GetURL(nsISupports* peer, const char* url, const char* target, void* notifyData, 
           const char* altHost, const char* referer, PRBool forceJSEnabled);

    // (Corresponds to NPN_PostURL and NPN_PostURLNotify.)
    NS_IMETHOD_(nsPluginError)
    PostURL(nsISupports* peer, const char* url, const char* target,
            PRUint32 len, const char* buf, PRBool file, void* notifyData, 
            const char* altHost, const char* referer, PRBool forceJSEnabled,
            PRUint32 postHeadersLength, const char* postHeaders);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginManager2:

    NS_IMETHOD_(void)
    BeginWaitCursor(void);

    NS_IMETHOD_(void)
    EndWaitCursor(void);

    NS_IMETHOD_(PRBool)
    SupportsURLProtocol(const char* protocol);

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

    nsISupports*        fJVMMgr;
    PRUint16            fWaiting;
    void*               fOldCursor;
};

extern nsPluginManager* thePluginManager;

////////////////////////////////////////////////////////////////////////////////

class nsFileUtilities : public nsIFileUtilities {
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIFileUtilities:
    
    NS_IMETHOD_(const char*)
    GetProgramPath(void);

    NS_IMETHOD_(const char*)
    GetTempDirPath(void);

    NS_IMETHOD_(nsresult)
    GetFileName(const char* fn, FileNameType type,
                char* resultBuf, PRUint32 bufLen);

    NS_IMETHOD_(nsresult)
    NewTempFileName(const char* prefix, char* resultBuf, PRUint32 bufLen);

    ////////////////////////////////////////////////////////////////////////////
    // nsFileUtilities specific methods:

    nsFileUtilities(nsISupports* outer);
    virtual ~nsFileUtilities(void);

    NS_DECL_AGGREGATED

    void SetProgramPath(const char* path) { fProgramPath = path; }

protected:
    const char*         fProgramPath;

};

////////////////////////////////////////////////////////////////////////////////
typedef struct JSContext JSContext;

class nsPluginTagInfo;

class nsPluginInstancePeer : public nsIPluginInstancePeer2,
                             public nsILiveConnectPluginInstancePeer,
                             public nsIWindowlessPluginInstancePeer
{
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginInstancePeer:

    // (Corresponds to NPP_New's MIMEType argument.)
    NS_IMETHOD_(nsMIMEType)
    GetMIMEType(void);

    // (Corresponds to NPP_New's mode argument.)
    NS_IMETHOD_(nsPluginType)
    GetMode(void);

    // (Corresponds to NPN_NewStream.)
    NS_IMETHOD_(nsPluginError)
    NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result);

    // (Corresponds to NPN_Status.)
    NS_IMETHOD_(void)
    ShowStatus(const char* message);

    // (Corresponds to NPN_GetValue.)
    NS_IMETHOD_(nsPluginError)
    GetValue(nsPluginManagerVariable variable, void *value);

    // (Corresponds to NPN_SetValue.)
    NS_IMETHOD_(nsPluginError)
    SetValue(nsPluginVariable variable, void *value);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginInstancePeer2:

    NS_IMETHOD_(void)
    RegisterWindow(void* window);
	
    NS_IMETHOD_(void)
    UnregisterWindow(void* window);	

    NS_IMETHOD_(PRInt16)
	AllocateMenuID(PRBool isSubmenu);

	// On the mac (and most likely win16), network activity can
    // only occur on the main thread. Therefore, we provide a hook
    // here for the case that the main thread needs to tickle itself.
    // In this case, we make sure that we give up the monitor so that
    // the tickle code can notify it without freezing.
    NS_IMETHOD_(PRBool)
    Tickle(void);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIJRILiveConnectPluginInstancePeer:

    // (Corresponds to NPN_GetJavaPeer.)
    NS_IMETHOD_(jobject)
    GetJavaPeer(void);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIWindowlessPluginInstancePeer:

    // (Corresponds to NPN_InvalidateRect.)
    NS_IMETHOD_(void)
    InvalidateRect(nsRect *invalidRect);

    // (Corresponds to NPN_InvalidateRegion.)
    NS_IMETHOD_(void)
    InvalidateRegion(nsRegion invalidRegion);

    // (Corresponds to NPN_ForceRedraw.)
    NS_IMETHOD_(void)
    ForceRedraw(void);

    ////////////////////////////////////////////////////////////////////////////
    // nsPluginInstancePeer specific methods:

    nsPluginInstancePeer(NPP npp);
    virtual ~nsPluginInstancePeer(void);

    NS_DECL_AGGREGATED

    nsIPluginInstance* GetUserInstance(void) {
        userInst->AddRef();
        return userInst;
    }

    void SetUserInstance(nsIPluginInstance* inst) {
        userInst = inst;
    }
    
    NPP GetNPP(void) { return npp; }
    JSContext *GetJSContext(void);
    MWContext *GetMWContext(void);
protected:

    // NPP is the old plugin structure. If we were implementing this
    // from scratch we wouldn't use it, but for now we're calling the old
    // npglue.c routines wherever possible.
    NPP npp;

    nsIPluginInstance*  userInst;
    nsPluginTagInfo*    tagInfo;

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
    NS_IMETHOD_(nsPluginError)
    GetAttributes(PRUint16& n, const char*const*& names, const char*const*& values);

    // Get the value for the named attribute.  Returns null
    // if the attribute was not set.
    NS_IMETHOD_(const char*)
    GetAttribute(const char* name);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginTagInfo2:

    // Get the type of the HTML tag that was used ot instantiate this
    // plugin.  Currently supported tags are EMBED, OBJECT and APPLET.
    NS_IMETHOD_(nsPluginTagType) 
    GetTagType(void);

    // Get the complete text of the HTML tag that was
    // used to instantiate this plugin
    NS_IMETHOD_(const char *)
    GetTagText(void);

    // Get a ptr to the paired list of parameter names and values,
    // returns the length of the array.
    //
    // Each name or value is a null-terminated string.
    NS_IMETHOD_(nsPluginError)
    GetParameters(PRUint16& n, const char*const*& names, const char*const*& values);

    // Get the value for the named parameter.  Returns null
    // if the parameter was not set.
    NS_IMETHOD_(const char*)
    GetParameter(const char* name);
    
    NS_IMETHOD_(const char*)
    GetDocumentBase(void);
    
    // Return an encoding whose name is specified in:
    // http://java.sun.com/products/jdk/1.1/docs/guide/intl/intl.doc.html#25303
    NS_IMETHOD_(const char*)
    GetDocumentEncoding(void);
    
    NS_IMETHOD_(const char*)
    GetAlignment(void);
    
    NS_IMETHOD_(PRUint32)
    GetWidth(void);
    
    NS_IMETHOD_(PRUint32)
    GetHeight(void);
    
    NS_IMETHOD_(PRUint32)
    GetBorderVertSpace(void);
    
    NS_IMETHOD_(PRUint32)
    GetBorderHorizSpace(void);

    // Returns a unique id for the current document on which the
    // plugin is displayed.
    NS_IMETHOD_(PRUint32)
    GetUniqueID(void);

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

    NS_IMETHOD_(PRInt32)
    Write(const char* aBuf, PRInt32 aOffset, PRInt32 aCount,
          nsresult *errorResult);

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

class nsPluginStreamPeer :
    virtual public nsIPluginStreamPeer2, 
    virtual public nsISeekablePluginStreamPeer
{
public:

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginStreamPeer:

    // (Corresponds to NPStream's url field.)
    NS_IMETHOD_(const char*)
    GetURL(void);

    // (Corresponds to NPStream's end field.)
    NS_IMETHOD_(PRUint32)
    GetEnd(void);

    // (Corresponds to NPStream's lastmodified field.)
    NS_IMETHOD_(PRUint32)
    GetLastModified(void);

    // (Corresponds to NPStream's notifyData field.)
    NS_IMETHOD_(void*)
    GetNotifyData(void);

    // (Corresponds to NPP_DestroyStream's reason argument.)
    NS_IMETHOD_(nsPluginReason)
    GetReason(void);

    // (Corresponds to NPP_NewStream's MIMEType argument.)
    NS_IMETHOD_(nsMIMEType)
    GetMIMEType(void);

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginStreamPeer2:

    NS_IMETHOD_(PRUint32)
    GetContentLength(void);

    NS_IMETHOD_(PRUint32)
    GetHeaderFieldCount(void);

    NS_IMETHOD_(const char*)
    GetHeaderFieldKey(PRUint32 index);

    NS_IMETHOD_(const char*)
    GetHeaderField(PRUint32 index);

    ////////////////////////////////////////////////////////////////////////////
    // from nsISeekablePluginStreamPeer:

    // (Corresponds to NPN_RequestRead.)
    NS_IMETHOD_(nsPluginError)
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

////////////////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
} /* extern "C" */
#endif

#endif /* npglue_h__ */
