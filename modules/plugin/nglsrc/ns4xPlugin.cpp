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
#include "nsIPluginStream.h"
#include "ns4xPluginInstance.h"

////////////////////////////////////////////////////////////////////////

NPNetscapeFuncs ns4xPlugin::CALLBACKS;
nsIPluginManager *  ns4xPlugin::mPluginManager;
nsIMalloc *         ns4xPlugin::mMalloc;

void
ns4xPlugin::CheckClassInitialized(void)
{
    static PRBool initialized = FALSE;

    if (initialized)
        return;

    mPluginManager = nsnull;
    mMalloc = nsnull;

    // XXX It'd be nice to make this const and initialize it
    // statically...
    CALLBACKS.size = sizeof(CALLBACKS);
    CALLBACKS.version = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;

    CALLBACKS.geturl           = NewNPN_GetURLProc(_geturl);
    CALLBACKS.posturl          = NewNPN_PostURLProc(_posturl);
    CALLBACKS.requestread      = NewNPN_RequestReadProc(_requestread);
    CALLBACKS.newstream        = NewNPN_NewStreamProc(_newstream);
    CALLBACKS.write            = NewNPN_WriteProc(_write);
    CALLBACKS.destroystream    = NewNPN_DestroyStreamProc(_destroystream);
    CALLBACKS.status           = NewNPN_StatusProc(_status);
    CALLBACKS.uagent           = NewNPN_UserAgentProc(_useragent);
    CALLBACKS.memalloc         = NewNPN_MemAllocProc(_memalloc);
    CALLBACKS.memfree          = NewNPN_MemFreeProc(_memfree);
    CALLBACKS.memflush         = NewNPN_MemFlushProc(_memflush);
    CALLBACKS.reloadplugins    = NewNPN_ReloadPluginsProc(_reloadplugins);
    CALLBACKS.getJavaEnv       = NewNPN_GetJavaEnvProc(_getJavaEnv);
    CALLBACKS.getJavaPeer      = NewNPN_GetJavaPeerProc(_getJavaPeer);
    CALLBACKS.geturlnotify     = NewNPN_GetURLNotifyProc(_geturlnotify);
    CALLBACKS.posturlnotify    = NewNPN_PostURLNotifyProc(_posturlnotify);
    CALLBACKS.getvalue         = NewNPN_GetValueProc(_getvalue);
    CALLBACKS.setvalue         = NewNPN_SetValueProc(_setvalue);
    CALLBACKS.invalidaterect   = NewNPN_InvalidateRectProc(_invalidaterect);
    CALLBACKS.invalidateregion = NewNPN_InvalidateRegionProc(_invalidateregion);
    CALLBACKS.forceredraw      = NewNPN_ForceRedrawProc(_forceredraw);

    initialized = TRUE;
};

////////////////////////////////////////////////////////////////////////


ns4xPlugin::ns4xPlugin(NPPluginFuncs* callbacks, NP_PLUGINSHUTDOWN aShutdown)
{
    NS_INIT_REFCNT();

    memcpy((void*) &fCallbacks, (void*) callbacks, sizeof(fCallbacks));
    fShutdownEntry = aShutdown;
}


ns4xPlugin::~ns4xPlugin(void)
{
}


////////////////////////////////////////////////////////////////////////
// nsISupports stuff

NS_IMPL_ADDREF(ns4xPlugin);
NS_IMPL_RELEASE(ns4xPlugin);

static NS_DEFINE_IID(kILiveConnectPluginIID, NS_ILIVECONNECTPLUGIN_IID); 
static NS_DEFINE_IID(kIPluginIID, NS_IPLUGIN_IID); 
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWindowlessPluginInstancePeerIID, NS_IWINDOWLESSPLUGININSTANCEPEER_IID);
static NS_DEFINE_IID(kISeekablePluginStreamPeerIID, NS_ISEEKABLEPLUGINSTREAMPEER_IID);

nsresult
ns4xPlugin::QueryInterface(const nsIID& iid, void** instance)
{
    if (instance == NULL)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIPluginIID))
    {
        *instance = (void *)(nsIPlugin *)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kILiveConnectPluginIID))
    {
        // Check the 4.x plugin callbacks to see if it supports
        // LiveConnect...
        if (fCallbacks.javaClass == NULL)
            return NS_NOINTERFACE;

        *instance = (void *)(nsILiveConnectPlugin *)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kIFactoryIID))
    {
        *instance = (void *)(nsIFactory *)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kISupportsIID))
    {
        *instance = (void *)(nsISupports *)this;
        AddRef();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}

static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID); 
static NS_DEFINE_IID(kIMallocIID, NS_IMALLOC_IID); 

////////////////////////////////////////////////////////////////////////
// Static factory method.
//

nsresult
ns4xPlugin::CreatePlugin(PRLibrary *library,
                         nsIPlugin **result, nsISupports* browserInterfaces)
{
    CheckClassInitialized();

	// XXX this only applies on Windows
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

    NP_PLUGINSHUTDOWN pfnShutdown =
        (NP_PLUGINSHUTDOWN)PR_FindSymbol(library, "NP_Shutdown");

	// create the new plugin handler
    *result = new ns4xPlugin(&callbacks, pfnShutdown);

    NS_ADDREF(*result);

    if (*result == NULL)
      return NS_ERROR_OUT_OF_MEMORY;
	
	// we must init here because the plugin may call NPN functions
	// when we call into the NP_Initialize entry point - NPN functions
	// require that mBrowserManager be set up
	(*result)->Initialize(browserInterfaces);

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

	if (pfnInitialize(&(ns4xPlugin::CALLBACKS)) != NS_OK)
		return NS_ERROR_UNEXPECTED;

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
  inst = (nsISupports *)(nsIPluginInstance *)new ns4xPluginInstance(&fCallbacks);

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

nsresult
ns4xPlugin::Initialize(nsISupports* browserInterfaces)
{
	nsresult rv = NS_OK;

	// set up the connections to the plugin manager
	if (nsnull == mPluginManager)
		if((rv = browserInterfaces->QueryInterface(kIPluginManagerIID, (void **)&mPluginManager)) != NS_OK)
			return rv;
	if (nsnull == mMalloc)
		if((rv = browserInterfaces->QueryInterface(kIMallocIID, (void **)&mMalloc)) != NS_OK)
			return rv;

	return rv;
}

nsresult
ns4xPlugin::Shutdown(void)
{
  if (nsnull != fShutdownEntry)
  {
#ifdef NS_DEBUG
printf("shutting down plugin %08x\n", this);
#endif
    fShutdownEntry();
    fShutdownEntry = nsnull;
  }

  NS_IF_RELEASE(mPluginManager);
  NS_IF_RELEASE(mMalloc);

  return NS_OK;
}

nsresult
ns4xPlugin::GetMIMEDescription(const char* *resultingDesc)
{
printf("plugin getmimedescription called\n");
  *resultingDesc = "";
  return NS_OK; // XXX make a callback, etc.
}

nsresult
ns4xPlugin::GetValue(nsPluginVariable variable, void *value)
{
printf("plugin getvalue %d called\n", variable);
  return NS_OK;
}

nsresult
ns4xPlugin::GetJavaClass(jclass *resultingClass)
{
  *resultingClass = (jclass)fCallbacks.javaClass;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//
// Static callbacks that get routed back through the new C++ API
//

nsresult NP_EXPORT
ns4xPlugin::_geturl(NPP npp, const char* relativeURL, const char* target)
{
    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");
    NS_ASSERTION(mPluginManager != NULL, "null manager");

    if (inst == NULL)
        return NS_ERROR_UNEXPECTED; // XXX

    return mPluginManager->GetURL(inst, relativeURL, target);
}

nsresult NP_EXPORT
ns4xPlugin::_geturlnotify(NPP npp, const char* relativeURL, const char* target,
                          void* notifyData)
{
    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");
    NS_ASSERTION(mPluginManager != NULL, "null manager");

    if (inst == NULL)
        return NS_ERROR_UNEXPECTED; // XXX

    return mPluginManager->GetURL(inst, relativeURL, target,
                                  notifyData);
}


nsresult NP_EXPORT
ns4xPlugin::_posturlnotify(NPP npp, const char* relativeURL, const char *target,
                           uint32 len, const char *buf, NPBool file,
                           void* notifyData)
{
    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");
    NS_ASSERTION(mPluginManager != NULL, "null manager");

    if (inst == NULL)
        return NS_ERROR_UNEXPECTED; // XXX

    return mPluginManager->PostURL(inst, relativeURL, target,
                                   len, buf, file, notifyData);
}


nsresult NP_EXPORT
ns4xPlugin::_posturl(NPP npp, const char* relativeURL, const char *target, uint32 len,
                     const char *buf, NPBool file)
{
    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");
    NS_ASSERTION(mPluginManager != NULL, "null manager");

    if (inst == NULL)
        return NS_ERROR_UNEXPECTED; // XXX

    return mPluginManager->PostURL(inst, relativeURL, target,
                                   len, buf, file);
}

////////////////////////////////////////////////////////////////////////

/**
 * A little helper class used to wrap up plugin manager streams (that is,
 * streams from the plugin to the browser).
 */
class ns4xStreamWrapper
{
protected:
    nsIOutputStream *fStream;
    NPStream        fNPStream;

public:
    ns4xStreamWrapper(nsIOutputStream* stream);
    ~ns4xStreamWrapper();

    nsIOutputStream*
    GetStream(void);

    NPStream*
    GetNPStream(void) {
        return &fNPStream;
    };
};

ns4xStreamWrapper::ns4xStreamWrapper(nsIOutputStream* stream)
    : fStream(stream)
{
    NS_ASSERTION(stream != NULL, "bad stream");

    NS_ADDREF(fStream);

    memset(&fNPStream, 0, sizeof(fNPStream));
    fNPStream.ndata    = (void*) this;
}

ns4xStreamWrapper::~ns4xStreamWrapper(void)
{
    NS_IF_RELEASE(fStream);
}

nsIOutputStream*
ns4xStreamWrapper::GetStream(void)
{
    NS_IF_ADDREF(fStream);

    return fStream;
}

////////////////////////////////////////////////////////////////////////


nsresult NP_EXPORT
ns4xPlugin::_newstream(NPP npp, NPMIMEType type, const char* window, NPStream* *result)
{
    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");

    if (inst == NULL)
        return NS_ERROR_UNEXPECTED; // XXX

    nsresult error;
    nsIOutputStream* stream;
    nsIPluginInstancePeer *peer;

    if (NS_OK == inst->GetPeer(&peer))
    {
      if ((error = peer->NewStream((const char*) type, window, &stream)) != NS_OK)
      {
        NS_RELEASE(peer);
        return error;
      }

      ns4xStreamWrapper* wrapper = new ns4xStreamWrapper(stream);

      if (wrapper == NULL)
      {
        NS_RELEASE(peer);
        NS_RELEASE(stream);
        return NS_ERROR_OUT_OF_MEMORY;
      }

      (*result) = wrapper->GetNPStream();

      NS_RELEASE(peer);

      return error;
    }
    else
      return NS_ERROR_UNEXPECTED;
}

int32 NP_EXPORT
ns4xPlugin::_write(NPP npp, NPStream *pstream, int32 len, void *buffer)
{
    ns4xStreamWrapper* wrapper = (ns4xStreamWrapper*) npp->ndata;

    NS_ASSERTION(wrapper != NULL, "null wrapper");

    if (wrapper == NULL)
        return 0;

    nsIOutputStream* stream = wrapper->GetStream();

    PRInt32 count = 0;
    nsresult rv = stream->Write((char *)buffer, 0, len, &count);

    NS_RELEASE(stream);

    return count;
}

nsresult NP_EXPORT
ns4xPlugin::_destroystream(NPP npp, NPStream *pstream, NPError reason)
{
    ns4xStreamWrapper* wrapper = (ns4xStreamWrapper*) npp->ndata;

    NS_ASSERTION(wrapper != NULL, "null wrapper");

    if (wrapper == NULL)
        return 0;

    // This will release the wrapped nsIOutputStream.
    delete wrapper;

    return NS_OK;
}

void NP_EXPORT
ns4xPlugin::_status(NPP npp, const char *message)
{
    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");

    if (inst == NULL)
        return;

    nsIPluginInstancePeer *peer;

    if (NS_OK == inst->GetPeer(&peer))
    {
      peer->ShowStatus(message);
      NS_RELEASE(peer);
    }
}

void NP_EXPORT
ns4xPlugin::_memfree (void *ptr)
{
    mMalloc->Free(ptr);
}

uint32 NP_EXPORT
ns4xPlugin::_memflush(uint32 size)
{
    mMalloc->HeapMinimize();

    return 0;
}

void NP_EXPORT
ns4xPlugin::_reloadplugins(NPBool reloadPages)
{
    mPluginManager->ReloadPlugins(reloadPages);
}

void NP_EXPORT
ns4xPlugin::_invalidaterect(NPP npp, NPRect *invalidRect)
{
    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");

    if (inst == NULL)
        return;

    nsIPluginInstancePeer *peer;
    nsIWindowlessPluginInstancePeer *wpeer;

    if (NS_OK == inst->GetPeer(&peer))
    {
      if (NS_OK == peer->QueryInterface(kIWindowlessPluginInstancePeerIID, (void **)&wpeer))
      {
        // XXX nsRect & NPRect are structurally equivalent
        wpeer->InvalidateRect((nsPluginRect *)invalidRect);
        NS_RELEASE(wpeer);
      }

      NS_RELEASE(peer);
    }
}

void NP_EXPORT
ns4xPlugin::_invalidateregion(NPP npp, NPRegion invalidRegion)
{
    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");

    if (inst == NULL)
        return;

    nsIPluginInstancePeer *peer;
    nsIWindowlessPluginInstancePeer *wpeer;

    if (NS_OK == inst->GetPeer(&peer))
    {
      if (NS_OK == peer->QueryInterface(kIWindowlessPluginInstancePeerIID, (void **)&wpeer))
      {
        // XXX nsRegion & NPRegion are typedef'd to the same thing
        wpeer->InvalidateRegion((nsPluginRegion)invalidRegion);
        NS_RELEASE(wpeer);
      }

      NS_RELEASE(peer);
    }
}

void NP_EXPORT
ns4xPlugin::_forceredraw(NPP npp)
{
    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");

    if (inst == NULL)
        return;

    nsIPluginInstancePeer *peer;
    nsIWindowlessPluginInstancePeer *wpeer;

    if (NS_OK == inst->GetPeer(&peer))
    {
      if (NS_OK == peer->QueryInterface(kIWindowlessPluginInstancePeerIID, (void **)&wpeer))
      {
        wpeer->ForceRedraw();
        NS_RELEASE(wpeer);
      }

      NS_RELEASE(peer);
    }
}

nsresult NP_EXPORT
ns4xPlugin::_getvalue(NPP npp, NPNVariable variable, void *result)
{
    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");

    if (inst == NULL)
        return NS_ERROR_FAILURE; // XXX

    nsIPluginInstancePeer *peer;

    if (NS_OK == inst->GetPeer(&peer))
    {
      nsresult rv;

      // XXX Note that for backwards compatibility, the old NPNVariables
      // map correctly to NPPluginManagerVariables.
      rv = peer->GetValue((nsPluginInstancePeerVariable)variable, result);
      NS_RELEASE(peer);
      return rv;
    }
    else
      return NS_ERROR_UNEXPECTED;
}

nsresult NP_EXPORT
ns4xPlugin::_setvalue(NPP npp, NPPVariable variable, void *result)
{
    ns4xPluginInstance *inst = (ns4xPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");

    if (inst == NULL)
        return NS_ERROR_FAILURE; // XXX

    switch (variable)
    {
      case NPPVpluginWindowBool:
        return inst->SetWindowless(*((NPBool *)result));

      case NPPVpluginTransparentBool:
        return inst->SetTransparent(*((NPBool *)result));

      default:
        return NS_OK;
    }

#if 0
    nsIPluginInstancePeer *peer;

    if (NS_OK == inst->GetPeer(&peer))
    {
      nsresult rv;

      // XXX Note that for backwards compatibility, the old NPPVariables
      // map correctly to NPPluginVariables.
      rv = peer->SetValue((nsPluginInstancePeerVariable)variable, result);
      NS_RELEASE(peer);
      return rv;
    }
    else
      return NS_ERROR_UNEXPECTED;
#endif
}

nsresult NP_EXPORT
ns4xPlugin::_requestread(NPStream *pstream, NPByteRange *rangeList)
{
    nsIPluginStreamPeer* streamPeer = (nsIPluginStreamPeer*) pstream->ndata;

    NS_ASSERTION(streamPeer != NULL, "null streampeer");

    if (streamPeer == NULL)
        return NS_ERROR_FAILURE; // XXX

    nsISeekablePluginStreamPeer* seekablePeer = NULL;

    if (streamPeer->QueryInterface(kISeekablePluginStreamPeerIID,
                                   (void**)seekablePeer) == NS_OK)
    {
        nsresult error;

        // XXX nsByteRange & NPByteRange are structurally equivalent.
        error = seekablePeer->RequestRead((nsByteRange *)rangeList);
        NS_RELEASE(seekablePeer);
        return error;
    }

    return NS_ERROR_UNEXPECTED;
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
    NS_ASSERTION(mPluginManager != NULL, "null pluginmanager");

    if (mPluginManager == NULL)
        return NULL;

    char *retstr;

    mPluginManager->UserAgent((const char **)&retstr);

    return retstr;
}

void * NP_EXPORT
ns4xPlugin::_memalloc (uint32 size)
{
    return mMalloc->Alloc(size);
}

#if 1

java_lang_Class* NP_EXPORT
ns4xPlugin::_getJavaClass(void* handle)
{
    // Is this just a generic call into the Java VM?
    return NULL;
}



jref NP_EXPORT
ns4xPlugin::_getJavaPeer(NPP npp)
{
#if 0
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
#endif
    return NULL;
}

#endif

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_A0
#endif

//
////////////////////////////////////////////////////////////////////////
