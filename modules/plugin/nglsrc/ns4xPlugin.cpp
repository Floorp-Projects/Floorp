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
#include "ns4xPluginInstance.h"
#include "nsIServiceManager.h"
#include "nsIAllocator.h"
#include "nsIPluginStreamListener.h"

////////////////////////////////////////////////////////////////////////

NPNetscapeFuncs ns4xPlugin::CALLBACKS;
nsIPluginManager *  ns4xPlugin::mPluginManager;
nsIAllocator *         ns4xPlugin::mMalloc;

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
// nsISupports stuff

NS_IMPL_ADDREF(ns4xPlugin);
NS_IMPL_RELEASE(ns4xPlugin);

//static NS_DEFINE_IID(kILiveConnectPluginIID, NS_ILIVECONNECTPLUGIN_IID); 
static NS_DEFINE_IID(kIPluginIID, NS_IPLUGIN_IID); 
static NS_DEFINE_IID(kIFactoryIID, NS_IFACTORY_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWindowlessPluginInstancePeerIID, NS_IWINDOWLESSPLUGININSTANCEPEER_IID);
static NS_DEFINE_IID(kPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_IID(kIPluginManagerIID, NS_IPLUGINMANAGER_IID); 
static NS_DEFINE_IID(kAllocatorCID, NS_ALLOCATOR_CID);
static NS_DEFINE_IID(kIAllocatorIID, NS_IALLOCATOR_IID);
static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);

////////////////////////////////////////////////////////////////////////

ns4xPlugin::ns4xPlugin(NPPluginFuncs* callbacks, NP_PLUGINSHUTDOWN aShutdown, nsIServiceManager* serviceMgr)
{
    NS_INIT_REFCNT();

    memcpy((void*) &fCallbacks, (void*) callbacks, sizeof(fCallbacks));
    fShutdownEntry = aShutdown;

	 // set up the connections to the plugin manager
	if (nsnull == mPluginManager)
		serviceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&mPluginManager);

	if (nsnull == mMalloc)
		serviceMgr->GetService(kAllocatorCID, kIAllocatorIID, (nsISupports**)&mMalloc);
}


ns4xPlugin::~ns4xPlugin(void)
{
}


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


////////////////////////////////////////////////////////////////////////
// Static factory method.
//

/* 
   CreatePlugin()
   --------------
   Handles the initialization of old, 4x style plugins.  Creates the ns4xPlugin object.
   One ns4xPlugin object exists per Plugin (not instance).
*/

nsresult
ns4xPlugin::CreatePlugin(PRLibrary *library,
                         nsIPlugin **result,
                         nsIServiceManager* serviceMgr)
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
    *result = new ns4xPlugin(&callbacks, pfnShutdown, serviceMgr);

    NS_ADDREF(*result);

    if (*result == NULL)
      return NS_ERROR_OUT_OF_MEMORY;
	
	// we must init here because the plugin may call NPN functions
	// when we call into the NP_Initialize entry point - NPN functions
	// require that mBrowserManager be set up
    (*result)->Initialize();

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

/*
   CreateInstance()
   ----------------
   Creates a ns4xPluginInstance object.
*/

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

NS_METHOD ns4xPlugin :: CreatePluginInstance(nsISupports *aOuter, REFNSIID aIID, 
                                  const char* aPluginMIMEType,
                                  void **aResult)
{
  return CreateInstance(aOuter, aIID, aResult);
}

nsresult
ns4xPlugin::Initialize(void)
{
	return NS_OK;
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

////////////////////////////////////////////////////////////////////////
//
// Static callbacks that get routed back through the new C++ API
//

NPError NP_EXPORT
ns4xPlugin::_geturl(NPP npp, const char* relativeURL, const char* target)
{
	if(!npp)
		return NPERR_INVALID_INSTANCE_ERROR;

    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");
    NS_ASSERTION(mPluginManager != NULL, "null manager");

    if (inst == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

	nsIPluginStreamListener* listener = nsnull;
	if(target == nsnull)
		inst->NewStream(&listener);

	if(mPluginManager->GetURL(inst, relativeURL, target, listener) != NS_OK)
		return NPERR_GENERIC_ERROR;

	return NPERR_NO_ERROR;
}

NPError NP_EXPORT
ns4xPlugin::_geturlnotify(NPP npp, const char* relativeURL, const char* target,
                          void* notifyData)
{
	if(!npp)
		return NPERR_INVALID_INSTANCE_ERROR;

    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");
    NS_ASSERTION(mPluginManager != NULL, "null manager");

    if (inst == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

	nsIPluginStreamListener* listener = nsnull;
	if(target == nsnull)
		((ns4xPluginInstance*)inst)->NewNotifyStream(&listener, notifyData);

	if(mPluginManager->GetURL(inst, relativeURL, target, listener) != NS_OK)
		return NPERR_GENERIC_ERROR;

	return NPERR_NO_ERROR;
}


NPError NP_EXPORT
ns4xPlugin::_posturlnotify(NPP npp, const char* relativeURL, const char *target,
                           uint32 len, const char *buf, NPBool file,
                           void* notifyData)
{
	if(!npp)
		return NPERR_INVALID_INSTANCE_ERROR;

    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");
    NS_ASSERTION(mPluginManager != NULL, "null manager");

    if (inst == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

	nsIPluginStreamListener* listener = nsnull;
	if(target == nsnull)
		((ns4xPluginInstance*)inst)->NewNotifyStream(&listener, notifyData);

	if(mPluginManager->PostURL(inst, relativeURL, len, buf, file, target, listener) != NS_OK)
		return NPERR_GENERIC_ERROR;

	return NPERR_NO_ERROR;
}


NPError NP_EXPORT
ns4xPlugin::_posturl(NPP npp, const char* relativeURL, const char *target, uint32 len,
                     const char *buf, NPBool file)
{
	if(!npp)
		return NPERR_INVALID_INSTANCE_ERROR;

    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");
    NS_ASSERTION(mPluginManager != NULL, "null manager");

    if (inst == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

	nsIPluginStreamListener* listener = nsnull;
	if(target == nsnull)
		inst->NewStream(&listener);

	if(mPluginManager->PostURL(inst, relativeURL, len, buf, file, target, listener) != NS_OK)
		return NPERR_GENERIC_ERROR;

	return NPERR_NO_ERROR;
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

    void GetStream(nsIOutputStream* &result);

    NPStream*
    GetNPStream(void) {
        return &fNPStream;
    };
};

ns4xStreamWrapper::ns4xStreamWrapper(nsIOutputStream* stream)
    : fStream(stream)
{
    NS_ASSERTION(stream != NULL, "bad stream");

    fStream = stream;
	NS_ADDREF(fStream);

    memset(&fNPStream, 0, sizeof(fNPStream));
    fNPStream.ndata    = (void*) this;
}

ns4xStreamWrapper::~ns4xStreamWrapper(void)
{
	fStream->Close();
    NS_IF_RELEASE(fStream);
}


void
ns4xStreamWrapper::GetStream(nsIOutputStream* &result)
{
	result = fStream;
    NS_IF_ADDREF(fStream);
}

////////////////////////////////////////////////////////////////////////


NPError NP_EXPORT
ns4xPlugin::_newstream(NPP npp, NPMIMEType type, const char* window, NPStream* *result)
{
	if(!npp)
		return NPERR_INVALID_INSTANCE_ERROR;

    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");

    if (inst == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    nsIOutputStream* stream;
    nsIPluginInstancePeer *peer;

    if (NS_OK == inst->GetPeer(&peer))
    {
      if (peer->NewStream((const char*) type, window, &stream) != NS_OK)
      {
        NS_RELEASE(peer);
        return NPERR_GENERIC_ERROR;
      }

      ns4xStreamWrapper* wrapper = new ns4xStreamWrapper(stream);

      if (wrapper == NULL)
      {
        NS_RELEASE(peer);
        NS_RELEASE(stream);
        return NPERR_OUT_OF_MEMORY_ERROR;
      }

      (*result) = wrapper->GetNPStream();

      NS_RELEASE(peer);

      return NPERR_NO_ERROR;
    }
    else
      return NPERR_GENERIC_ERROR;
}

int32 NP_EXPORT
ns4xPlugin::_write(NPP npp, NPStream *pstream, int32 len, void *buffer)
{
	// negative return indicates failure to the plugin
	if(!npp)
		return -1;

    ns4xStreamWrapper* wrapper = (ns4xStreamWrapper*) pstream->ndata;
    NS_ASSERTION(wrapper != NULL, "null stream");

    if (wrapper == NULL)
        return -1;

    nsIOutputStream* stream;
	wrapper->GetStream(stream);

    PRUint32 count = 0;
    nsresult rv = stream->Write((char *)buffer, len, &count);
    NS_RELEASE(stream);

	if(rv != NS_OK)
		return -1;

    return (int32)count;
}

NPError NP_EXPORT
ns4xPlugin::_destroystream(NPP npp, NPStream *pstream, NPError reason)
{
	if(!npp)
		return NPERR_INVALID_INSTANCE_ERROR;

	nsISupports* stream = (nsISupports*) pstream->ndata;
	nsIPluginStreamListener* listener;

	// DestroyStream can kill two kinds of streams: NPP derived and
	// NPN derived.
	// check to see if they're trying to kill a NPP stream
	if(stream->QueryInterface(kIPluginStreamListenerIID, (void**)&listener) == NS_OK)
	{
		// XXX we should try to kill this listener here somehow
		NS_RELEASE(listener);	
		return NPERR_NO_ERROR;
	}

    ns4xStreamWrapper* wrapper = (ns4xStreamWrapper*) pstream->ndata;

    NS_ASSERTION(wrapper != NULL, "null wrapper");

    if (wrapper == NULL)
        return NPERR_INVALID_PARAM;

    // This will release the wrapped nsIOutputStream.
    delete wrapper;

    return NPERR_NO_ERROR;
}

void NP_EXPORT
ns4xPlugin::_status(NPP npp, const char *message)
{
	if(!npp)
		return;

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
	if(ptr)
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
	if(!npp)
		return;

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
	if(!npp)
		return;

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
	if(!npp)
		return;

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

NPError NP_EXPORT
ns4xPlugin::_getvalue(NPP npp, NPNVariable variable, void *result)
{
	if(!npp)
		return NPERR_INVALID_INSTANCE_ERROR;

    nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");

    if (inst == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

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
      return NPERR_GENERIC_ERROR;
}

NPError NP_EXPORT
ns4xPlugin::_setvalue(NPP npp, NPPVariable variable, void *result)
{
	if(!npp)
		return NPERR_INVALID_INSTANCE_ERROR;

    ns4xPluginInstance *inst = (ns4xPluginInstance *) npp->ndata;

    NS_ASSERTION(inst != NULL, "null instance");

    if (inst == NULL)
        return NPERR_INVALID_INSTANCE_ERROR;

    switch (variable)
    {
      case NPPVpluginWindowBool:
        return inst->SetWindowless(*((NPBool *)result));

      case NPPVpluginTransparentBool:
        return inst->SetTransparent(*((NPBool *)result));

      default:
        return NPERR_NO_ERROR;
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

NPError NP_EXPORT
ns4xPlugin::_requestread(NPStream *pstream, NPByteRange *rangeList)
{
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

java_lang_Class* NP_EXPORT
ns4xPlugin::_getJavaClass(void* handle)
{
    // Is this just a generic call into the Java VM?
    return NULL;
}



jref NP_EXPORT
ns4xPlugin::_getJavaPeer(NPP npp)
{
    return NULL;
}


// eof
