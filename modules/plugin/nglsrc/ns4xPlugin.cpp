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

// TODO: Implement Java callbacks

#include "xp_core.h"
#include "nsplugin.h"
#include "ns4xPlugin.h"
//#include "ns4xPluginInstance.h"

////////////////////////////////////////////////////////////////////////

// XXX These are defined in platform specific FE directories right now :-/
typedef NPError (*NP_GETENTRYPOINTS)(NPPluginFuncs* pCallbacks);
typedef NPError (*NP_PLUGININIT)(const NPNetscapeFuncs* pCallbacks);
typedef NPError (*NP_PLUGINSHUTDOWN)();


////////////////////////////////////////////////////////////////////////

NPNetscapeFuncs ns4xPlugin::CALLBACKS;
nsIPluginManager * ns4xPlugin::mManager;

void
ns4xPlugin::CheckClassInitialized(void)
{
    static PRBool initialized = FALSE;

    if (initialized)
        return;

    mManager = nsnull;

    // XXX It'd be nice to make this const and initialize it
    // statically...
    CALLBACKS.size = sizeof(CALLBACKS);
    CALLBACKS.version = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;

    CALLBACKS.geturl           = NewNPN_GetURLProc(_geturl);
    CALLBACKS.posturl          = NewNPN_PostURLProc(_posturl);
//    CALLBACKS.requestread      = NewNPN_RequestReadProc(_requestread);
//    CALLBACKS.newstream        = NewNPN_NewStreamProc(_newstream);
//    CALLBACKS.write            = NewNPN_WriteProc(_write);
//    CALLBACKS.destroystream    = NewNPN_DestroyStreamProc(_destroystream);
//    CALLBACKS.status           = NewNPN_StatusProc(_status);
//    CALLBACKS.uagent           = NewNPN_UserAgentProc(_useragent);
//    CALLBACKS.memalloc         = NewNPN_MemAllocProc(_memalloc);
//    CALLBACKS.memfree          = NewNPN_MemFreeProc(_memfree);
//    CALLBACKS.memflush         = NewNPN_MemFlushProc(_memflush);
//    CALLBACKS.reloadplugins    = NewNPN_ReloadPluginsProc(_reloadplugins);
//    CALLBACKS.getJavaEnv       = NewNPN_GetJavaEnvProc(_getJavaEnv);
//    CALLBACKS.getJavaPeer      = NewNPN_GetJavaPeerProc(_getJavaPeer);
    CALLBACKS.geturlnotify     = NewNPN_GetURLNotifyProc(_geturlnotify);
    CALLBACKS.posturlnotify    = NewNPN_PostURLNotifyProc(_posturlnotify);
//    CALLBACKS.getvalue         = NewNPN_GetValueProc(_getvalue);
//    CALLBACKS.setvalue         = NewNPN_SetValueProc(_setvalue);
//    CALLBACKS.invalidaterect   = NewNPN_InvalidateRectProc(_invalidaterect);
//    CALLBACKS.invalidateregion = NewNPN_InvalidateRegionProc(_invalidateregion);
//    CALLBACKS.forceredraw      = NewNPN_ForceRedrawProc(_forceredraw);

    initialized = TRUE;
};

////////////////////////////////////////////////////////////////////////


ns4xPlugin::ns4xPlugin(NPPluginFuncs* callbacks)
{
    NS_INIT_REFCNT();
    memcpy((void*) &fCallbacks, (void*) callbacks, sizeof(fCallbacks));
    mManager = nsnull;
}


ns4xPlugin::~ns4xPlugin(void)
{
  NS_IF_RELEASE(mManager);
}


////////////////////////////////////////////////////////////////////////
// nsISupports stuff

NS_IMPL_ADDREF(ns4xPlugin);
NS_IMPL_RELEASE(ns4xPlugin);

static NS_DEFINE_IID(kILiveConnectPluginIID, NS_ILIVECONNECTPLUGIN_IID); 
static NS_DEFINE_IID(kIPluginIID, NS_IPLUGIN_IID); 
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

nsresult
ns4xPlugin::QueryInterface(const nsIID& iid, void** instance)
{
    if (instance == NULL)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIPluginIID) ||
        iid.Equals(kISupportsIID)) {
        *instance = (void*) this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kILiveConnectPluginIID)) {
        // Check the 4.x plugin callbacks to see if it supports
        // LiveConnect...
        if (fCallbacks.javaClass == NULL)
            return NS_NOINTERFACE;

        *instance = (void*) this;
        AddRef();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}


////////////////////////////////////////////////////////////////////////
// Static factory method.
//

nsresult
ns4xPlugin::CreatePlugin(PRLibrary *library,
                         nsIPlugin **result)
{
    CheckClassInitialized();

    NP_GETENTRYPOINTS pfnGetEntryPoints =
        (NP_GETENTRYPOINTS)PR_FindSymbol(library, "NP_GetEntryPoints");

    if (pfnGetEntryPoints == NULL)
        return NS_ERROR_FAILURE;

    NPPluginFuncs callbacks;
    memset((void*) &callbacks, 0, sizeof(callbacks));

    callbacks.size = sizeof(callbacks);

    if (pfnGetEntryPoints(&callbacks) != NS_OK)
        return NS_ERROR_FAILURE; // XXX

#ifdef XP_WIN // XXX This is really XP, but we need to figure out how to do HIBYTE()
    if (HIBYTE(callbacks.version) < NP_VERSION_MAJOR)
        return NS_ERROR_FAILURE;
#endif    

    // the NP_Initialize entry point was misnamed as NP_PluginInit,
    // early in plugin project development.  Its correct name is
    // documented now, and new developers expect it to work.  However,
    // I don't want to break the plugins already in the field, so
    // we'll accept either name
    NP_PLUGININIT pfnInitialize =
        (NP_PLUGININIT)PR_FindSymbol(library, "NP_Initialize");

    if (!pfnInitialize) {
        pfnInitialize =
            (NP_PLUGININIT)PR_FindSymbol(library, "NP_PluginInit");
    }

    if (pfnInitialize == NULL)
        return NS_ERROR_UNEXPECTED; // XXX Right error?

    if (pfnInitialize(&ns4xPlugin::CALLBACKS) != NS_OK)
        return NS_ERROR_UNEXPECTED; // XXX shoudl convert the 4.x error...

    (*result) = new ns4xPlugin(&callbacks);

    if ((*result) == NULL)
      return NS_ERROR_OUT_OF_MEMORY;

    return NS_OK;
}

nsresult ns4xPlugin :: CreateInstance(nsISupports *aOuter,  
                                      const nsIID &aIID,  
                                      void **aResult)  
{  
  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  
  
  nsISupports *inst;

  inst = nsnull;
//  inst = (nsISupports *)(nsIPluginInstance *)new ns4xPluginInstance();

  if (inst == NULL) {  
    return NS_ERROR_OUT_OF_MEMORY;  
  }  

  nsresult res = inst->QueryInterface(aIID, aResult);

  if (res != NS_OK) {  
    // We didn't get the right interface, so clean up  
    delete inst;  
  }  

  return res;  
}  

nsresult ns4xPlugin :: LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  

static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID); 

nsresult
ns4xPlugin::Initialize(nsISupports* browserInterfaces)
{
  if (nsnull == mManager)
    return browserInterfaces->QueryInterface(kIPluginManagerIID, (void **)&mManager);
  else
    return NS_OK;
}

nsresult
ns4xPlugin::Shutdown(void)
{
  NS_IF_RELEASE(mManager);
  return NS_OK;
}

nsresult
ns4xPlugin::GetMIMEDescription(const char* *resultingDesc)
{
  *resultingDesc = "";
  return NS_OK; // XXX make a callback, etc.
}

nsresult
ns4xPlugin::GetValue(nsPluginVariable variable, void *value)
{
  return NS_OK;
}

nsresult
ns4xPlugin::SetValue(nsPluginVariable variable, void *value)
{
  return NS_OK;
}

nsresult
ns4xPlugin::GetJavaClass(jclass *resultingClass)
{
  *resultingClass = fCallbacks.javaClass;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//
// Static callbacks that get routed back through the new C++ API
//

nsresult NP_EXPORT
ns4xPlugin::_geturl(NPP npp, const char* relativeURL, const char* target)
{
    nsIPluginInstancePeer* peer = (nsIPluginInstancePeer*) npp->ndata;

    NS_ASSERTION(peer != NULL, "null peer");
    NS_ASSERTION(mManager != NULL, "null manager");

    if (peer == NULL)
        return NS_ERROR_UNEXPECTED; // XXX
#if 0
    nsURLInfo urlinfo;

    urlinfo.version = nsURLInfo_Version;
    urlinfo.url = relativeURL;
    urlinfo.target = target;
    urlinfo.notifyData = nsnull;
    urlinfo.altHost = nsnull;
    urlinfo.referrer = nsnull;
    urlinfo.forceJSEnabled = PR_FALSE;
    urlinfo.postData = nsnull;
    urlinfo.postDataLength = 0;
    urlinfo.postHeaders = nsnull;
    urlinfo.postHeadersLength = 0;
    urlinfo.postFile = PR_FALSE;

    return mManager->FetchURL(peer, &urlinfo);
#endif
    return NS_OK;
}

nsresult NP_EXPORT
ns4xPlugin::_geturlnotify(NPP npp, const char* relativeURL, const char* target,
                          void* notifyData)
{
    nsIPluginInstancePeer* peer = (nsIPluginInstancePeer*) npp->ndata;

    NS_ASSERTION(peer != NULL, "null peer");
    NS_ASSERTION(mManager != NULL, "null manager");

    if (peer == NULL)
        return NS_ERROR_UNEXPECTED; // XXX
#if 0
    nsURLInfo urlinfo;

    urlinfo.version = nsURLInfo_Version;
    urlinfo.url = relativeURL;
    urlinfo.target = target;
    urlinfo.notifyData = notifyData;
    urlinfo.altHost = nsnull;
    urlinfo.referrer = nsnull;
    urlinfo.forceJSEnabled = PR_FALSE;
    urlinfo.postData = nsnull;
    urlinfo.postDataLength = 0;
    urlinfo.postHeaders = nsnull;
    urlinfo.postHeadersLength = 0;
    urlinfo.postFile = PR_FALSE;

    return mManager->FetchURL(peer, &urlinfo);
#endif
    return NS_OK;
}


nsresult NP_EXPORT
ns4xPlugin::_posturlnotify(NPP npp, const char* relativeURL, const char *target,
                           uint32 len, const char *buf, NPBool file,
                           void* notifyData)
{
    nsIPluginInstancePeer* peer = (nsIPluginInstancePeer*) npp->ndata;

    NS_ASSERTION(peer != NULL, "null peer");
    NS_ASSERTION(mManager != NULL, "null manager");

    if (peer == NULL)
        return NS_ERROR_UNEXPECTED; // XXX
#if 0
    nsURLInfo urlinfo;

    urlinfo.version = nsURLInfo_Version;
    urlinfo.url = relativeURL;
    urlinfo.target = target;
    urlinfo.notifyData = notifyData;
    urlinfo.altHost = nsnull;
    urlinfo.referrer = nsnull;
    urlinfo.forceJSEnabled = PR_FALSE;
    urlinfo.postData = buf;
    urlinfo.postDataLength = len;
    urlinfo.postHeaders = nsnull;
    urlinfo.postHeadersLength = 0;
    urlinfo.postFile = file;

    return mManager->FetchURL(peer, &urlinfo);
#endif
    return NS_OK;
}


nsresult NP_EXPORT
ns4xPlugin::_posturl(NPP npp, const char* relativeURL, const char *target, uint32 len,
                     const char *buf, NPBool file)
{
    nsIPluginInstancePeer* peer = (nsIPluginInstancePeer*) npp->ndata;

    NS_ASSERTION(peer != NULL, "null peer");
    NS_ASSERTION(mManager != NULL, "null manager");

    if (peer == NULL)
        return NS_ERROR_UNEXPECTED; // XXX
#if 0
    nsURLInfo urlinfo;

    urlinfo.version = nsURLInfo_Version;
    urlinfo.url = relativeURL;
    urlinfo.target = target;
    urlinfo.notifyData = nsnull;
    urlinfo.altHost = nsnull;
    urlinfo.referrer = nsnull;
    urlinfo.forceJSEnabled = PR_FALSE;
    urlinfo.postData = buf;
    urlinfo.postDataLength = len;
    urlinfo.postHeaders = nsnull;
    urlinfo.postHeadersLength = 0;
    urlinfo.postFile = file;

    return mManager->FetchURL(peer, &urlinfo);
#endif
    return NS_OK;
}

#if 0

////////////////////////////////////////////////////////////////////////

/**
 * A little helper class used to wrap up plugin manager streams (that is,
 * streams from the plugin to the browser).
 */
class ns4xStreamWrapper {
protected:
    NPIPluginManagerStream* fStream;
    NPStream                fNPStream;

public:
    ns4xStreamWrapper(NPIPluginManagerStream* stream);
    ~ns4xStreamWrapper();

    NPIPluginManagerStream*
    GetStream(void);

    NPStream*
    GetNPStream(void) {
        return &fNPStream;
    };
};

ns4xStreamWrapper::ns4xStreamWrapper(NPIPluginManagerStream* stream)
    : fStream(stream)
{
    PR_ASSERT(stream != NULL);
    fStream->AddRef();

    memset(&fNPStream, 0, sizeof(fNPStream));
    fNPStream.ndata    = (void*) this;
}

ns4xStreamWrapper::~ns4xStreamWrapper(void)
{
    if (fStream != NULL)
        fStream->Release();
}

NPIPluginManagerStream*
ns4xStreamWrapper::GetStream(void)
{
    if (fStream != NULL)
        fStream->AddRef();

    return fStream;
}

////////////////////////////////////////////////////////////////////////


nsresult NP_EXPORT
ns4xPlugin::_newstream(NPP npp, NPMIMEType type, const char* window, NPStream* *result)
{
    nsIPluginInstancePeer* peer = (nsIPluginInstancePeer*) npp->ndata;

    NS_ASSERTION(peer != NULL, "null peer");

    if (peer == NULL)
        return NS_ERROR_UNEXPECTED; // XXX

    nsresult error;
    nsIPluginStream* stream;
    if ((error = peer->NewStream((const char*) type, window, &stream))
        != NPPluginError_NoError)
        return (NPError) error;

    ns4xStreamWrapper* wrapper = new ns4xStreamWrapper(stream);

    if (wrapper == NULL) {
        stream->Release();
        return NPERR_OUT_OF_MEMORY_ERROR;
    }

    (*result) = wrapper->GetNPStream();
    return NPERR_NO_ERROR;
}



int32 NP_EXPORT
ns4xPlugin::_write(NPP npp, NPStream *pstream, int32 len, void *buffer)
{
    ns4xStreamWrapper* wrapper = (ns4xStreamWrapper*) npp->ndata;
    PR_ASSERT(wrapper != NULL);
    if (wrapper == NULL)
        return 0;

    NPIPluginManagerStream* stream = wrapper->GetStream();
    PRUint32 count = 0;
    while (count < ((PRUint32) len)) {
        PRUint32 ready = stream->WriteReady();
        ready = (ready > ((PRUint32) len)) ? ((PRUint32) len) : ready;

        PRUint32 written = stream->Write(ready, ((const char*) buffer) + count);
        count += written;
    }

    stream->Release();

    return count;
}



NPError NP_EXPORT
ns4xPlugin::_destroystream(NPP npp, NPStream *pstream, NPError reason)
{
    ns4xStreamWrapper* wrapper = (ns4xStreamWrapper*) npp->ndata;
    PR_ASSERT(wrapper != NULL);
    if (wrapper == NULL)
        return 0;

    // This will release the wrapped NPIPluginManagerStream.
    delete wrapper;

    return NPERR_NO_ERROR;
}



void NP_EXPORT
ns4xPlugin::_status(NPP npp, const char *message)
{
    NPIPluginInstancePeer* peer = (NPIPluginInstancePeer*) npp->ndata;
    PR_ASSERT(peer != NULL);
    if (peer == NULL)
        return;

    peer->ShowStatus(message);
}



void NP_EXPORT
ns4xPlugin::_memfree (void *ptr)
{
    thePluginManager->MemFree(ptr);
}



uint32 NP_EXPORT
ns4xPlugin::_memflush(uint32 size)
{
    return thePluginManager->MemFlush(size);
}



void NP_EXPORT
ns4xPlugin::_reloadplugins(NPBool reloadPages)
{
    thePluginManager->ReloadPlugins(reloadPages);
}



void NP_EXPORT
ns4xPlugin::_invalidaterect(NPP npp, NPRect *invalidRect)
{
    NPIPluginInstancePeer* peer = (NPIPluginInstancePeer*) npp->ndata;
    PR_ASSERT(peer != NULL);
    if (peer == NULL)
        return;

    // XXX nsRect & NPRect are structurally equivalent
    peer->InvalidateRect((nsRect*) invalidRect);
}



void NP_EXPORT
ns4xPlugin::_invalidateregion(NPP npp, NPRegion invalidRegion)
{
    NPIPluginInstancePeer* peer = (NPIPluginInstancePeer*) npp->ndata;
    PR_ASSERT(peer != NULL);
    if (peer == NULL)
        return;

    // XXX nsRegion & NPRegion are typedef'd to the same thing
    peer->InvalidateRegion((nsRegion*) invalidRegion);
}



void NP_EXPORT
ns4xPlugin::_forceredraw(NPP npp)
{
    NPIPluginInstancePeer* peer = (NPIPluginInstancePeer*) npp->ndata;
    PR_ASSERT(peer != NULL);
    if (peer == NULL)
        return;

    peer->ForceRedraw();
}



NPError NP_EXPORT
ns4xPlugin::_getvalue(NPP npp, NPNVariable variable, void *result)
{
    NPIPluginInstancePeer* peer = (NPIPluginInstancePeer*) npp->ndata;
    PR_ASSERT(peer != NULL);
    if (peer == NULL)
        return NPERR_INVALID_PLUGIN_ERROR; // XXX

    // XXX Note that for backwards compatibility, the old NPNVariables
    // map correctly to NPPluginManagerVariables.
    return (NPError) peer->GetValue((NPPluginManagerVariable) variable, result);
}



NPError NP_EXPORT
ns4xPlugin::_setvalue(NPP npp, NPPVariable variable, void *result)
{
    NPIPluginInstancePeer* peer = (NPIPluginInstancePeer*) npp->ndata;
    PR_ASSERT(peer != NULL);
    if (peer == NULL)
        return NPERR_INVALID_PLUGIN_ERROR; // XXX

    // XXX Note that for backwards compatibility, the old NPPVariables
    // map correctly to NPPluginVariables.
    return (NPError) peer->SetValue((NPPluginVariable) variable, result);
}



NPError NP_EXPORT
ns4xPlugin::_requestread(NPStream *pstream, NPByteRange *rangeList)
{
    NPIPluginStreamPeer* streamPeer = (NPIPluginStreamPeer*) pstream->ndata;
    PR_ASSERT(streamPeer != NULL);
    if (streamPeer == NULL)
        return NPERR_INVALID_PLUGIN_ERROR; // XXX

    NPISeekablePluginStreamPeer* seekablePeer = NULL;
    static NS_DEFINE_IID(kISeekablePluginStreamPeerIID, NP_ISEEKABLEPLUGINSTREAMPEER_IID);
    if (streamPeer->QueryInterface(kISeekablePluginStreamPeerIID,
                                   (void**) seekablePeer) == NS_OK) {
        NPError error;

        // XXX nsByteRange & NPByteRange are structurally equivalent.
        error = (NPError) seekablePeer->RequestRead((nsByteRange*) rangeList);
        seekablePeer->Release();
        return error;
    }

    return NPERR_STREAM_NOT_SEEKABLE;
}


////////////////////////////////////////////////////////////////////////
//
// On 68K Mac (XXX still supported?), we need to make sure that the
// pointers are in D0 for the following functions that return pointers.
//

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif


JRIEnv* NP_EXPORT
ns4xPlugin::_getJavaEnv(void)
{
    return NULL;
}


const char * NP_EXPORT
ns4xPlugin::_useragent(NPP npp)
{
    NPIPluginInstancePeer* peer = (NPIPluginInstancePeer*) npp->ndata;
    PR_ASSERT(peer != NULL);
    if (peer == NULL)
        return NULL;

    return peer->UserAgent();
}


void * NP_EXPORT
ns4xPlugin::_memalloc (uint32 size)
{
    return thePluginManager->MemAlloc(size);
}


#ifdef JAVA
java_lang_Class* NP_EXPORT
ns4xPlugin::_getJavaClass(void* handle)
{
    // Is this just a generic call into the Java VM?
    return NULL;
}
#endif



jref NP_EXPORT
ns4xPlugin::_getJavaPeer(NPP npp)
{
    NPIPluginInstancePeer* peer = (NPIPluginInstancePeer*) npp->ndata;
    PR_ASSERT(peer != NULL);
    if (peer == NULL)
        return NULL;

    static NS_DEFINE_IID(kILiveConnectPluginInstancePeerIID,
                         NP_ILIVECONNECTPLUGININSTANCEPEER_IID);

    NPILiveConnectPluginInstancePeer* lcPeer = NULL;
    if (peer->QueryInterface(kILiveConnectPluginInstancePeerIID,
                             (void**) &lcPeer) == NS_OK) {
        jobject result = lcPeer->GetJavaPeer();
        lcPeer->Release();
        return result;
    }

    return NULL;
}

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif

//
////////////////////////////////////////////////////////////////////////

#endif
