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

#include "nsPluginManager.h"
#include "nsPluginInstancePeer.h"
#include "nsPluginInputStream.h"

////////////////////////////////////////////////////////////////////////////////
// IMPLEMENTATION OF NETSCAPE COMMUNICATOR PLUGINS (NEW C++ API).
//
// This superscedes the old plugin API (npapi.h, npupp.h), and 
// eliminates the need for glue files: npunix.c, npwin.cpp and npmac.cpp. 
// Correspondences to the old API are shown throughout the file.
////////////////////////////////////////////////////////////////////////////////

#include "npglue.h" 
#include "np.h" 

#ifdef OJI
#include "nsplugin.h"
#include "jvmmgr.h" 
#ifdef PRE_SERVICE_MANAGER
#include "nsILiveconnect.h"
#endif // PRE_SERVICE_MANAGER
#endif

#ifdef XP_MAC
#include "MacMemAllocator.h"
#include "asyncCursors.h"
#include "LMenuSharing.h"
#include <TArray.h>
#endif

#include "prthread.h"
/* This is a private NSPR header - cls */
#ifdef OJI
#  ifdef XP_MAC
#    include "pprthred.h"
#  else
#    include "private/pprthred.h"
#  endif
#endif
#include "prtypes.h"
#include "nsHashtable.h"
#ifdef PRE_SERVICE_MANAGER
#include "nsMalloc.h"
#include "nsICapsManager.h"
#include "nsCCapsManager.h"
#endif // PRE_SERVICE_MANAGER

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID);
static NS_DEFINE_IID(kIPluginManager2IID, NS_IPLUGINMANAGER2_IID);
static NS_DEFINE_IID(kIJNIEnvIID, NS_IJNIENV_IID); 
static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID);

#include "prerror.h"

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
// THINGS IMPLEMENTED BY THE BROWSER...
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
// Plugin Manager Interface
// This interface defines the minimum set of functionality that a plugin
// manager will support if it implements plugins. Plugin implementations can
// QueryInterface to determine if a plugin manager implements more specific 
// APIs for the plugin to use.

#ifdef PRE_SERVICE_MANAGER
nsPluginManager* thePluginManager = NULL;
#endif // PRE_SERVICE_MANAGER

nsPluginManager::nsPluginManager(nsISupports* outer)
    : fAllocatedMenuIDs(NULL), fWaiting(0), fOldCursor(NULL)
#ifdef PRE_SERVICE_MANAGER
    , fJVMMgr(NULL), fMalloc(NULL), fFileUtils(NULL), fCapsManager(NULL), fLiveconnect(NULL)
#endif
{
    NS_INIT_AGGREGATED(outer);
}

nsPluginManager::~nsPluginManager(void)
{
#ifdef PRE_SERVICE_MANAGER
    if (fJVMMgr) {
        fJVMMgr->Release();
        fJVMMgr = NULL;
    }
    if (fMalloc) {
        fMalloc->Release();
        fMalloc = NULL;
    }
    if (fFileUtils) {
        fFileUtils->Release();
        fFileUtils = NULL;
    }

    if (fCapsManager) {
        fCapsManager->Release();
        fCapsManager = NULL;
    }

    if (fLiveconnect) {
        fCapsManager->Release();
        fCapsManager = NULL;
    }
#endif // PRE_SERVICE_MANAGER

#ifdef XP_MAC
    if (fAllocatedMenuIDs != NULL) {
        // Fix me, delete all the elements before deleting the table.
        delete fAllocatedMenuIDs;
    }
#endif
}

NS_IMPL_AGGREGATED(nsPluginManager);

NS_METHOD
nsPluginManager::Create(nsISupports* outer, const nsIID& aIID, void* *aInstancePtr)
{
    if (outer && !aIID.Equals(kISupportsIID))
        return NS_NOINTERFACE;   // XXX right error?
    nsPluginManager* mgr = new nsPluginManager(outer);
    if (mgr == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    mgr->AddRef();
    *aInstancePtr = outer ? (void *)mgr->GetInner() : (void *)mgr;
    return NS_OK;
}

NS_METHOD
nsPluginManager::GetValue(nsPluginManagerVariable variable, void *value)
{
    NPError err = npn_getvalue(NULL, (NPNVariable)variable, value);
    return fromNPError[err];
}

#if 0
NS_METHOD
nsPluginManager::SetValue(nsPluginManagerVariable variable, void *value)
{
    NPError err = npn_setvalue(NULL, (NPPVariable)variable, value);
    return fromNPError[err];
}
#endif

NS_METHOD
nsPluginManager::ReloadPlugins(PRBool reloadPages)
{
    npn_reloadplugins(reloadPages);
    return NS_OK;
}

NS_METHOD
nsPluginManager::UserAgent(const char* *resultingAgentString)
{
    *resultingAgentString = npn_useragent(NULL); // we don't really need an npp here
    return NS_OK;
}

NS_METHOD
nsPluginManager::AggregatedQueryInterface(const nsIID& aIID, void** aInstancePtr) 
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }                                                                      
    if (aIID.Equals(kIPluginManagerIID) || 
        aIID.Equals(kIPluginManager2IID) ||
        aIID.Equals(kISupportsIID)) {
        *aInstancePtr = (nsIPluginManager*) this; 
        AddRef(); 
        return NS_OK; 
    }
    return NS_NOINTERFACE;
}

#ifdef PRE_SERVICE_MANAGER
nsICapsManager*
nsPluginManager::GetCapsManager(const nsIID& aIID)
{
    nsICapsManager* result = NULL;
    nsresult        err    = NS_OK;
    PRThread       *threadAttached = NULL;
#ifdef OJI
    if (fCapsManager == NULL) {
        if ( PR_GetCurrentThread() == NULL )
        {
            threadAttached = PR_AttachThread(PR_USER_THREAD, PR_PRIORITY_NORMAL, NULL);
        }

        NS_DEFINE_CID(kCCapsManagerCID, NS_CCAPSMANAGER_CID);
        nsresult err    = NS_OK;
        err = nsRepository::CreateInstance(kCCapsManagerCID, 
                                           (nsIPluginManager*)this,    /* outer */
                                           kISupportsIID,
                                           (void **)&fCapsManager);
        NS_DEFINE_IID(kICapsManagerIID, NS_ICAPSMANAGER_IID);
        if (   (err == NS_OK) 
               && (fCapsManager != NULL) 
               && (err = (fCapsManager->QueryInterface(kICapsManagerIID, (void**)&result)) == NS_OK)
            )
        {
            ((nsCCapsManager*)result)->SetSystemPrivilegeManager();
            result->Release();
        }
    }
    if (  (err == NS_OK) 
          &&(fCapsManager->QueryInterface(aIID, (void**)&result) != NS_OK)
        )
    {
        result = NULL;
    }
    if (threadAttached != NULL )
    {
        PR_DetachThread();
    }
#endif
    return result;
}

#ifdef OJI
nsILiveconnect*
nsPluginManager::GetLiveconnect(const nsIID& aIID)
{
    nsILiveconnect* result = NULL;
    PRThread       *threadAttached = NULL;
    nsresult        err    = NS_OK;
    if (fLiveconnect == NULL) {
	     if ( PR_GetCurrentThread() == NULL )
	     {
 		      threadAttached = PR_AttachThread(PR_USER_THREAD, PR_PRIORITY_NORMAL, NULL);
	     }
      NS_DEFINE_CID(kCLiveconnectCID, NS_CLIVECONNECT_CID);
      err = nsRepository::CreateInstance(kCLiveconnectCID, 
                                         (nsIPluginManager*)this,    /* outer */
                                         kISupportsIID,
                                         (void **)&fLiveconnect);
     }
     if (  (err == NS_OK) 
         &&(fLiveconnect->QueryInterface(aIID, (void**)&result) != NS_OK)
        )
     {
       result = NULL;
     }
	    if (threadAttached != NULL )
     {
       PR_DetachThread();
     }
    return result;
}
#endif

#endif // PRE_SERVICE_MANAGER

NS_METHOD
nsPluginManager::BeginWaitCursor(void)
{
    if (fWaiting == 0) {
#ifdef XP_PC
        fOldCursor = (void*)GetCursor();
        HCURSOR newCursor = LoadCursor(NULL, IDC_WAIT);
        if (newCursor)
            SetCursor(newCursor);
#endif
#ifdef XP_MAC
        startAsyncCursors();
#endif
    }
    fWaiting++;
    return NS_OK;
}

NS_METHOD
nsPluginManager::EndWaitCursor(void)
{
    fWaiting--;
    if (fWaiting == 0) {
#ifdef XP_PC
        if (fOldCursor)
            SetCursor((HCURSOR)fOldCursor);
#endif
#ifdef XP_MAC
        stopAsyncCursors();
#endif
        fOldCursor = NULL;
    }
    return NS_OK;
}

NS_METHOD
nsPluginManager::SupportsURLProtocol(const char* protocol, PRBool *result)
{
    int type = NET_URL_Type(protocol);
    *result = (PRBool)(type != 0);
    return NS_OK;
}

NS_METHOD
nsPluginManager::NotifyStatusChange(nsIPlugin* plugin, nsresult errorStatus)
{
    // XXX need to shut down all instances of this plugin
    return NS_OK;
}

#if 0
static NPP getNPPFromHandler(nsIEventHandler* handler)
{
    NPP npp = NULL;
    nsIPluginInstance* pluginInst = NULL;
    if (handler->QueryInterface(kIPluginInstanceIID, (void**)&pluginInst) == NS_OK) {
        nsPluginInstancePeer* myPeer;
        nsresult err = pluginInst->GetPeer((nsIPluginInstancePeer**)&myPeer);
        PR_ASSERT(err == NS_OK);
        npp = myPeer->GetNPP();
        myPeer->Release();
        pluginInst->Release();
    }
    return npp;
}
#endif

NS_METHOD
nsPluginManager::RegisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
#if 1
    npn_registerwindow(handler, window);
    return NS_OK;
#else
    NPP npp = getNPPFromHandler(handler);
    if (npp != NULL) {
        npn_registerwindow(npp, window);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
#endif
}

NS_METHOD
nsPluginManager::UnregisterWindow(nsIEventHandler* handler, nsPluginPlatformWindowRef window)
{
#if 1
    npn_unregisterwindow(handler, window);
    return NS_OK;
#else
    NPP npp = getNPPFromHandler(handler);
    if (npp != NULL) {
        npn_unregisterwindow(npp, window);
        return NS_OK;
    }
    return NS_ERROR_FAILURE;
#endif
}

class nsEventHandlerKey : public nsHashKey {
public:
    nsEventHandlerKey(nsIEventHandler* handler) : mHandler(handler) {}

    virtual PRUint32 HashValue(void) const
    {
        return PRUint32(mHandler);
    }
	
    virtual PRBool Equals(const nsHashKey *aKey) const
    {
        return ((nsEventHandlerKey*)aKey)->mHandler == mHandler;
    }
	
    virtual nsHashKey *Clone(void) const
    {
        return new nsEventHandlerKey(mHandler);
    }

private:
    nsIEventHandler* mHandler;
};

NS_METHOD
nsPluginManager::AllocateMenuID(nsIEventHandler* handler, PRBool isSubmenu,
                                PRInt16 *result)
{
#ifdef XP_MAC
    PRInt16 menuID = LMenuSharingAttachment::AllocatePluginMenuID(isSubmenu);
    if (fAllocatedMenuIDs == NULL)
        fAllocatedMenuIDs  = new nsHashtable(16);
    if (fAllocatedMenuIDs != NULL) {
        nsEventHandlerKey key(handler);
        TArray<PRInt16>* menuIDs = (TArray<PRInt16>*) fAllocatedMenuIDs->Get(&key);
        if (menuIDs == NULL) {
            menuIDs = new TArray<PRInt16>;
            fAllocatedMenuIDs->Put(&key, menuIDs);
        }
        if (menuIDs != NULL)
            menuIDs->AddItem(menuID);
    }
    *result = menuID;
    return NS_OK;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_METHOD
nsPluginManager::DeallocateMenuID(nsIEventHandler* handler, PRInt16 menuID)
{
#ifdef XP_MAC
    if (fAllocatedMenuIDs != NULL) {
        nsEventHandlerKey key(handler);
        TArray<PRInt16>* menuIDs = (TArray<PRInt16>*) fAllocatedMenuIDs->Get(&key);
        if (menuIDs != NULL) {
            menuIDs->Remove(menuID);
            if (menuIDs->GetCount() == 0) {
                // let go of the vector and the hash table entry.
                fAllocatedMenuIDs->Remove(&key);
                delete menuIDs;
            }
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NS_METHOD
nsPluginManager::HasAllocatedMenuID(nsIEventHandler* handler, PRInt16 menuID, PRBool *result)
{
#ifdef XP_MAC
    if (fAllocatedMenuIDs != NULL) {
        nsEventHandlerKey key(handler);
        TArray<PRInt16>* menuIDs = (TArray<PRInt16>*) fAllocatedMenuIDs->Get(&key);
        if (menuIDs != NULL) {
            TArray<PRInt16>& menus = *menuIDs;
            UInt32 count = menus.GetCount();
            for (UInt32 i = count; i > 0; --i) {
                if (menus[i] == menuID) {
                    *result = PR_TRUE;
                    return NS_OK;
                }
            }
        }
    }
    return PR_FALSE;
#else
    return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

#if 0 // problematic
static NPL_ProcessNextEventProc npl_ProcessNextEventProc = NULL;
static void* npl_ProcessNextEventData = NULL;

PR_IMPLEMENT(void) 
NPL_InstallProcessNextEventProc(NPL_ProcessNextEventProc proc, void* data)
{
    npl_ProcessNextEventProc = proc;
    npl_ProcessNextEventData = data;
}

NS_METHOD
nsPluginManager::ProcessNextEvent(PRBool *bEventHandled)
{
#ifdef XP_MAC
    if (npl_ProcessNextEventProc != NULL)
        *bEventHandled = npl_ProcessNextEventProc(npl_ProcessNextEventData);
    return NS_OK;
#else 
    *bEventHandled = PR_FALSE;
    return NS_OK;
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////

#include "plevent.h"

extern "C" {
extern PREventQueue* mozilla_event_queue;
extern PRThread* mozilla_thread;
};

#ifdef NEW_PLUGIN_STREAM_API

struct GetURLEvent {
    PLEvent event;
    nsPluginInstancePeer* peer;
    const char* url;
    const char* target;
    void* notifyData;
    const char* altHost;
    const char* referrer;
    PRBool forceJSEnabled;
};

static void*
HandleGetURLEvent(PLEvent* event)
{
    PR_ASSERT(PR_CurrentThread() == mozilla_thread);
    GetURLEvent* e = (GetURLEvent*)event;
    NPP npp = e->peer->GetNPP();
    NPError rslt = np_geturlinternal(npp,
                                     e->url,
                                     e->target,
                                     e->altHost,
                                     e->referrer,
                                     e->forceJSEnabled,
                                     e->notifyData != NULL,
                                     e->notifyData);
    return (void*)rslt;
}

static void
DestroyGetURLEvent(PLEvent* event)
{
    GetURLEvent* e = (GetURLEvent*)event;
    PR_Free(event);
}

NS_METHOD
nsPluginManager::GetURL(nsISupports* pluginInst, 
                        const char* url, 
                        const char* target,
                        nsIPluginStreamListener* listener,
                        nsPluginStreamType streamType,
                        const char* altHost,
                        const char* referrer,
                        PRBool forceJSEnabled)
{
    void* notifyData = NULL;
    if (listener) {
        nsPluginInputStream* inStr = new nsPluginInputStream(listener, streamType);
        if (inStr == NULL) 
            return NS_ERROR_OUT_OF_MEMORY;
        inStr->AddRef();
        notifyData = inStr;
    }

    NPError rslt = NPERR_INVALID_PARAM;
    nsIPluginInstance* inst = NULL;
    if (pluginInst->QueryInterface(kIPluginInstanceIID, (void**)&inst) == NS_OK) {
        // Warning: Casting to our implementation type of plugin instance peer here:
        nsPluginInstancePeer* peer;
        nsresult err = inst->GetPeer((nsIPluginInstancePeer**)&peer);
        if (err == NS_OK) {
            if (PR_CurrentThread() == mozilla_thread) {
                NPP npp = peer->GetNPP();
                rslt = np_geturlinternal(npp,
                                         url,
                                         target,
                                         altHost,
                                         referrer,
                                         forceJSEnabled,
                                         notifyData != NULL,
                                         notifyData);
            }
            else {
                GetURLEvent* e = PR_NEW(GetURLEvent);
                if (e == NULL) {
                    rslt = NPERR_OUT_OF_MEMORY_ERROR;
                }
                else {
                    PL_InitEvent(&e->event, NULL, HandleGetURLEvent, DestroyGetURLEvent);
                    e->peer = peer;
                    e->url = url;
                    e->target = target;
                    e->notifyData = notifyData;
                    e->altHost = altHost;
                    e->referrer = referrer;
                    e->forceJSEnabled = forceJSEnabled;
                    /*rslt = (NPError)*/PL_PostSynchronousEvent(mozilla_event_queue, &e->event);
                    rslt = NPERR_NO_ERROR;  /* XXX irix c++ compiler doesn't like the above cast */
                }
            }
            peer->Release();
        }
        inst->Release();
    }
    return fromNPError[rslt];
}

struct PostURLEvent {
    PLEvent event;
    nsPluginInstancePeer* peer;
    const char* url;
    const char* target;
    PRUint32 postDataLen;
    const char* postData;
    PRBool isFile;
    void* notifyData;
    const char* altHost;
    const char* referrer;
    PRBool forceJSEnabled;
    PRUint32 postHeadersLen;
    const char* postHeaders;
};

static void*
HandlePostURLEvent(PLEvent* event)
{
    PR_ASSERT(PR_CurrentThread() == mozilla_thread);
    PostURLEvent* e = (PostURLEvent*)event;
    NPP npp = e->peer->GetNPP();
    NPError rslt = np_posturlinternal(npp,
                                      e->url, 
                                      e->target,
                                      e->altHost,
                                      e->referrer,
                                      e->forceJSEnabled,
                                      e->postDataLen,
                                      e->postData,
                                      e->isFile,
                                      e->notifyData != NULL,
                                      e->notifyData);
    return (void*)rslt;
}

static void
DestroyPostURLEvent(PLEvent* event)
{
    PostURLEvent* e = (PostURLEvent*)event;
    PR_Free(event);
}

NS_METHOD
nsPluginManager::PostURL(nsISupports* pluginInst,
                         const char* url,
                         PRUint32 postDataLen, 
                         const char* postData,
                         PRBool isFile,
                         const char* target,
                         nsIPluginStreamListener* listener,
                         nsPluginStreamType streamType,
                         const char* altHost, 
                         const char* referrer,
                         PRBool forceJSEnabled,
                         PRUint32 postHeadersLen, 
                         const char* postHeaders)
{
    void* notifyData = NULL;
    if (listener) {
        nsPluginInputStream* inStr = new nsPluginInputStream(listener, streamType);
        if (inStr == NULL) 
            return NS_ERROR_OUT_OF_MEMORY;
        inStr->AddRef();
        notifyData = inStr;
    }

    NPError rslt = NPERR_INVALID_PARAM;
    nsIPluginInstance* inst = NULL;
    if (pluginInst->QueryInterface(kIPluginInstanceIID, (void**)&inst) == NS_OK) {
        // Warning: Casting to our implementation type of plugin instance peer here:
        nsPluginInstancePeer* peer;
        nsresult err = inst->GetPeer((nsIPluginInstancePeer**)&peer);
        if (err == NS_OK) {
            if (PR_CurrentThread() == mozilla_thread) {
                NPP npp = peer->GetNPP();
                PR_ASSERT(postHeaders == NULL); // XXX need to deal with postHeaders
                rslt = np_posturlinternal(npp,
                                          url, 
                                          target,
                                          altHost,
                                          referrer,
                                          forceJSEnabled,
                                          postDataLen,
                                          postData,
                                          isFile,
                                          notifyData != NULL,
                                          notifyData);
            }
            else {
                PostURLEvent* e = PR_NEW(PostURLEvent);
                if (e == NULL) {
                    rslt = NPERR_OUT_OF_MEMORY_ERROR;
                }
                else {
                    PL_InitEvent(&e->event, NULL, HandlePostURLEvent, DestroyPostURLEvent);
                    e->peer = peer;
                    e->url = url;
                    e->target = target;
                    e->notifyData = notifyData;
                    e->altHost = altHost;
                    e->referrer = referrer;
                    e->forceJSEnabled = forceJSEnabled;
                    e->postDataLen = postDataLen;
                    e->postData = postData;
                    e->isFile = isFile;
                    e->postHeadersLen = postHeadersLen;
                    e->postHeaders = postHeaders;
                    /*rslt = (NPError)*/PL_PostSynchronousEvent(mozilla_event_queue, &e->event);
                    rslt = NPERR_NO_ERROR;  /* XXX irix c++ compiler doesn't like the above cast */
                }
            }
            peer->Release();
        }
        inst->Release();
    }
    return fromNPError[rslt];
}

#else // !NEW_PLUGIN_STREAM_API

struct GetURLEvent {
    PLEvent event;
    nsPluginInstancePeer* peer;
    const char* url;
    const char* target;
    void* notifyData;
    const char* altHost;
    const char* referrer;
    PRBool forceJSEnabled;
};

static void*
HandleGetURLEvent(PLEvent* event)
{
    PR_ASSERT(PR_CurrentThread() == mozilla_thread);
    GetURLEvent* e = (GetURLEvent*)event;
    NPP npp = e->peer->GetNPP();
    NPError rslt = np_geturlinternal(npp,
                                     e->url,
                                     e->target,
                                     e->altHost,
                                     e->referrer,
                                     e->forceJSEnabled,
                                     e->notifyData != NULL,
                                     e->notifyData);
    return (void*)rslt;
}

static void
DestroyGetURLEvent(PLEvent* event)
{
    GetURLEvent* e = (GetURLEvent*)event;
    PR_Free(event);
}

NS_METHOD
nsPluginManager::GetURL(nsISupports* pinst, const char* url, const char* target,
                        void* notifyData, const char* altHost,
                        const char* referrer, PRBool forceJSEnabled)
{
    NPError rslt = NPERR_INVALID_PARAM;
    nsIPluginInstance* inst = NULL;
    if (pinst->QueryInterface(kIPluginInstanceIID, (void**)&inst) == NS_OK) {
        // Warning: Casting to our implementation type of plugin instance peer here:
        nsPluginInstancePeer* peer;
        nsresult err = inst->GetPeer((nsIPluginInstancePeer**)&peer);
        if (err == NS_OK) {
            if (PR_CurrentThread() == mozilla_thread) {
                NPP npp = peer->GetNPP();
                rslt = np_geturlinternal(npp,
                                         url,
                                         target,
                                         altHost,
                                         referrer,
                                         forceJSEnabled,
                                         notifyData != NULL,
                                         notifyData);
            }
            else {
                GetURLEvent* e = PR_NEW(GetURLEvent);
                if (e == NULL) {
                    rslt = NPERR_OUT_OF_MEMORY_ERROR;
                }
                else {
                    PL_InitEvent(&e->event, NULL, HandleGetURLEvent, DestroyGetURLEvent);
                    e->peer = peer;
                    e->url = url;
                    e->target = target;
                    e->notifyData = notifyData;
                    e->altHost = altHost;
                    e->referrer = referrer;
                    e->forceJSEnabled = forceJSEnabled;
                    /*rslt = (NPError)*/PL_PostSynchronousEvent(mozilla_event_queue, &e->event);
                    rslt = NPERR_NO_ERROR;  /* XXX irix c++ compiler doesn't like the above cast */
                }
            }
            peer->Release();
        }
        inst->Release();
    }
    return fromNPError[rslt];
}

struct PostURLEvent {
    PLEvent event;
    nsPluginInstancePeer* peer;
    const char* url;
    const char* target;
    PRUint32 postDataLen;
    const char* postData;
    PRBool isFile;
    void* notifyData;
    const char* altHost;
    const char* referrer;
    PRBool forceJSEnabled;
    PRUint32 postHeadersLen;
    const char* postHeaders;
};

static void*
HandlePostURLEvent(PLEvent* event)
{
    PR_ASSERT(PR_CurrentThread() == mozilla_thread);
    PostURLEvent* e = (PostURLEvent*)event;
    NPP npp = e->peer->GetNPP();
    NPError rslt = np_posturlinternal(npp,
                                      e->url, 
                                      e->target,
                                      e->altHost,
                                      e->referrer,
                                      e->forceJSEnabled,
                                      e->postDataLen,
                                      e->postData,
                                      e->isFile,
                                      e->notifyData != NULL,
                                      e->notifyData);
    return (void*)rslt;
}

static void
DestroyPostURLEvent(PLEvent* event)
{
    PostURLEvent* e = (PostURLEvent*)event;
    PR_Free(event);
}

NS_METHOD
nsPluginManager::PostURL(nsISupports* pinst, const char* url, const char* target,
                         PRUint32 postDataLen, const char* postData,
                         PRBool isFile, void* notifyData,
                         const char* altHost, const char* referrer,
                         PRBool forceJSEnabled,
                         PRUint32 postHeadersLen, const char* postHeaders)
{
    NPError rslt = NPERR_INVALID_PARAM;
    nsIPluginInstance* inst = NULL;
    if (pinst->QueryInterface(kIPluginInstanceIID, (void**)&inst) == NS_OK) {
        // Warning: Casting to our implementation type of plugin instance peer here:
        nsPluginInstancePeer* peer;
        nsresult err = inst->GetPeer((nsIPluginInstancePeer**)&peer);
        if (err == NS_OK) {
            if (PR_CurrentThread() == mozilla_thread) {
                NPP npp = peer->GetNPP();
                PR_ASSERT(postHeaders == NULL); // XXX need to deal with postHeaders
                rslt = np_posturlinternal(npp,
                                          url, 
                                          target,
                                          altHost,
                                          referrer,
                                          forceJSEnabled,
                                          postDataLen,
                                          postData,
                                          isFile,
                                          notifyData != NULL,
                                          notifyData);
            }
            else {
                PostURLEvent* e = PR_NEW(PostURLEvent);
                if (e == NULL) {
                    rslt = NPERR_OUT_OF_MEMORY_ERROR;
                }
                else {
                    PL_InitEvent(&e->event, NULL, HandlePostURLEvent, DestroyPostURLEvent);
                    e->peer = peer;
                    e->url = url;
                    e->target = target;
                    e->notifyData = notifyData;
                    e->altHost = altHost;
                    e->referrer = referrer;
                    e->forceJSEnabled = forceJSEnabled;
                    e->postDataLen = postDataLen;
                    e->postData = postData;
                    e->isFile = isFile;
                    e->postHeadersLen = postHeadersLen;
                    e->postHeaders = postHeaders;
                    /*rslt = (NPError)*/PL_PostSynchronousEvent(mozilla_event_queue, &e->event);
                    rslt = NPERR_NO_ERROR;  /* XXX irix c++ compiler doesn't like the above cast */
                }
            }
            peer->Release();
        }
        inst->Release();
    }
    return fromNPError[rslt];
}

#endif // !NEW_PLUGIN_STREAM_API

extern "C" char *pacf_find_proxies_for_url(MWContext *context, 
                                           URL_Struct *URL_s);

NS_METHOD
nsPluginManager::FindProxyForURL(const char* url, char* *result)
{
    // Warning: Looking at the code in mkautocf.c, the context can
    // (fortunately) be NULL.
    URL_Struct* urls = NET_CreateURLStruct(url, NET_DONT_RELOAD);
    if (urls == NULL)
        return NS_ERROR_OUT_OF_MEMORY;
    *result = pacf_find_proxies_for_url(NULL, urls);
    NET_FreeURLStruct(urls);
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

#include "nsMalloc.h"
#include "nsFileUtilities.h"

class nsPluginFactory : public nsIFactory {
public:
    NS_DECL_ISUPPORTS

    NS_IMETHOD CreateInstance(nsISupports *aOuter,
                              REFNSIID aIID,
                              void **aResult);

    NS_IMETHOD LockFactory(PRBool aLock);

    nsPluginFactory(void);
    virtual ~nsPluginFactory(void);
};

static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
NS_IMPL_ISUPPORTS(nsPluginFactory, kIFactoryIID);

nsPluginFactory::nsPluginFactory(void)
{
}

nsPluginFactory::~nsPluginFactory(void)
{
}

NS_METHOD
nsPluginFactory::CreateInstance(nsISupports *aOuter,
                                REFNSIID aIID,
                                void **aResult)
{
    nsresult err;
    err = nsPluginManager::Create(aOuter, aIID, aResult);
    if (err == NS_OK) return err;

#if 0   // Browser interface should be JNI from now on. Hence all new code
        // should be written using JNI. sudu.
    if (aIID.Equals(kIJRIEnvIID)) {
        // XXX Need to implement ISupports for JRIEnv
        *aResult = (void*) ((nsISupports*)npn_getJavaEnv()); 
//        AddRef();     // XXX should the plugin instance peer and the env be linked?
        return NS_OK; 
    } 
#endif

#ifdef OJI

    if (aIID.Equals(kIJNIEnvIID)) {
        // XXX Need to implement ISupports for JNIEnv
        *aResult = (void*) ((nsISupports*)npn_getJavaEnv());       //=-= Fix this to return a Interface XXX need JNI version
//        AddRef();     // XXX should the plugin instance peer and the env be linked?
        return NS_OK; 
    }

#endif

    err = nsMalloc::Create(aOuter, aIID, aResult);
    if (err == NS_OK) return err;

    err = nsFileUtilities::Create(aOuter, aIID, aResult);
    if (err == NS_OK) return err;

    return NS_NOINTERFACE;
}

NS_METHOD
nsPluginFactory::LockFactory(PRBool aLock)
{
    // nothing to do here since we're not a DLL (yet)
    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// Bogus stuff that will go away when
// a) we have a persistent registry for CLSID -> Factory mappings, and
// b) when we ship a browser where all this Plugin management code is in a DLL.

static NS_DEFINE_CID(kPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_CID(kJNIEnvCID, NS_JNIENV_CID); 
#if 0
static NS_DEFINE_CID(kJRIEnvCID, NS_JRIENV_CID); 
#endif
static NS_DEFINE_CID(kMallocCID, NS_MALLOC_CID);
static NS_DEFINE_CID(kFileUtilitiesCID, NS_FILEUTILITIES_CID);

extern "C" nsresult
np_RegisterPluginMgr(void)
{
    nsPluginFactory* pluginFact = new nsPluginFactory();
    if (pluginFact == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    pluginFact->AddRef();
    nsRepository::RegisterFactory(kPluginManagerCID,    NULL, NULL, pluginFact, PR_TRUE);

    pluginFact->AddRef();
    nsRepository::RegisterFactory(kJNIEnvCID,           NULL, NULL, pluginFact, PR_TRUE);

#if 0
    pluginFact->AddRef();
    nsRepository::RegisterFactory(kJRIEnvCID,           NULL, NULL, pluginFact, PR_TRUE);
#endif

    pluginFact->AddRef();
    nsRepository::RegisterFactory(kMallocCID,           NULL, NULL, pluginFact, PR_TRUE);

    pluginFact->AddRef();
    nsRepository::RegisterFactory(kFileUtilitiesCID,    NULL, NULL, pluginFact, PR_TRUE);

    return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
