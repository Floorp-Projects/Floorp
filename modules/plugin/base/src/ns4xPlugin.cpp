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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// TODO: Implement Java callbacks

#include "xp_core.h"
#include "nsplugin.h"
#include "ns4xPlugin.h"
#include "ns4xPluginInstance.h"
#include "nsIServiceManager.h"
#include "nsIMemory.h"
#include "nsIPluginStreamListener.h"
#include "nsPluginsDir.h"
#include "nsPluginSafety.h"

#ifdef XP_MAC
#include <Resources.h>
#endif

#include "nsIPref.h"

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_IID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);

////////////////////////////////////////////////////////////////////////

NPNetscapeFuncs ns4xPlugin::CALLBACKS;
nsIServiceManager* ns4xPlugin::mServiceMgr;
nsIMemory *         ns4xPlugin::mMalloc;

void
ns4xPlugin::CheckClassInitialized(void)
{
    static PRBool initialized = FALSE;

    if (initialized)
        return;

    mServiceMgr = nsnull;
    mMalloc = nsnull;

    // XXX It'd be nice to make this const and initialize it
    // statically...
    CALLBACKS.size = sizeof(CALLBACKS);
    CALLBACKS.version = (NP_VERSION_MAJOR << 8) + NP_VERSION_MINOR;

#if !TARGET_CARBON
// pinkerton - these macros rely on BuildRoutineDescriptor(), which is no longer in
// Carbon. Our easy solution to this is to drop support for 68K plugins. Now we just
// need to do the work...
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
#endif

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
static NS_DEFINE_IID(kMemoryCID, NS_MEMORY_CID);
static NS_DEFINE_IID(kIMemoryIID, NS_IMEMORY_IID);
static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);

////////////////////////////////////////////////////////////////////////

ns4xPlugin::ns4xPlugin(NPPluginFuncs* callbacks, PRLibrary* aLibrary, NP_PLUGINSHUTDOWN aShutdown, nsIServiceManager* serviceMgr)
{
	NS_INIT_REFCNT();

	memcpy((void*) &fCallbacks, (void*) callbacks, sizeof(fCallbacks));
	fShutdownEntry = aShutdown;

    fLibrary = aLibrary;
	mServiceMgr = serviceMgr;
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

#ifdef XP_MAC
static char* p2cstrdup(StringPtr pstr)
{
	int len = pstr[0];
	char* cstr = new char[len + 1];
	if (cstr != NULL) {
		::BlockMoveData(pstr + 1, cstr, len);
		cstr[len] = '\0';
	}
	return cstr;
}

void
ns4xPlugin::SetPluginRefNum(short aRefNum)
{
	fPluginRefNum = aRefNum;
}
#endif

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
ns4xPlugin::CreatePlugin(nsIServiceManager* aServiceMgr,
                         const char* aFileName,
                         PRLibrary* aLibrary,
                         nsIPlugin** aResult)
{
    CheckClassInitialized();

	// set up the MemAllocator service now because it might be used by the plugin
	if (aServiceMgr != nsnull) {
		if (nsnull == mMalloc)
			aServiceMgr->GetService(kMemoryCID, kIMemoryIID, (nsISupports**)&mMalloc);
	}

#ifdef NS_DEBUG
    printf("debug: edburns ns4xPlugin::CreatePlugin\n");
#endif

#ifdef XP_UNIX

    ns4xPlugin *plptr;

    NPPluginFuncs callbacks;
    memset((void*) &callbacks, 0, sizeof(callbacks));
    callbacks.size = sizeof(callbacks);

#ifdef NS_DEBUG
    printf("debug: edburns ns4xPlugin::CreatePlugin: cleared callbacks\n");
#endif

    NP_PLUGINSHUTDOWN pfnShutdown =
        (NP_PLUGINSHUTDOWN)PR_FindSymbol(aLibrary, "NP_Shutdown");

	// create the new plugin handler
    *aResult = plptr = new ns4xPlugin(&callbacks, aLibrary, pfnShutdown, aServiceMgr);

    if (*aResult == NULL)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult);

	// we must init here because the plugin may call NPN functions
	// when we call into the NP_Initialize entry point - NPN functions
	// require that mBrowserManager be set up
    plptr->Initialize();

    NP_PLUGINUNIXINIT pfnInitialize =
        (NP_PLUGINUNIXINIT)PR_FindSymbol(aLibrary, "NP_Initialize");

    if (pfnInitialize == NULL)
        return NS_ERROR_UNEXPECTED; // XXX Right error?

	if (pfnInitialize(&(ns4xPlugin::CALLBACKS),&callbacks) != NS_OK)
		return NS_ERROR_UNEXPECTED;

#ifdef NS_DEBUG
	printf("debug: edburns: ns4xPlugin::CreatePlugin: callbacks->newstream: %p\n",
	       callbacks.newstream);
#endif

    // now copy function table back to ns4xPlugin instance
    memcpy((void*) &(plptr->fCallbacks), (void*)&callbacks, sizeof(callbacks));
#endif

#ifdef XP_PC
	// XXX this only applies on Windows
    NP_GETENTRYPOINTS pfnGetEntryPoints =
        (NP_GETENTRYPOINTS)PR_FindSymbol(aLibrary, "NP_GetEntryPoints");

    if (pfnGetEntryPoints == NULL)
        return NS_ERROR_FAILURE;

    NPPluginFuncs callbacks;
    memset((void*) &callbacks, 0, sizeof(callbacks));

    callbacks.size = sizeof(callbacks);

    if (pfnGetEntryPoints(&callbacks) != NS_OK)
        return NS_ERROR_FAILURE; // XXX

#ifdef XP_PC // XXX This is really XP, but we need to figure out how to do HIBYTE()
    if (HIBYTE(callbacks.version) < NP_VERSION_MAJOR)
        return NS_ERROR_FAILURE;
#endif

    NP_PLUGINSHUTDOWN pfnShutdown =
        (NP_PLUGINSHUTDOWN)PR_FindSymbol(aLibrary, "NP_Shutdown");

	// create the new plugin handler
    *aResult = new ns4xPlugin(&callbacks, aLibrary, pfnShutdown, aServiceMgr);

    if (*aResult == NULL)
      return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(*aResult);
	
	// we must init here because the plugin may call NPN functions
	// when we call into the NP_Initialize entry point - NPN functions
	// require that mBrowserManager be set up
    (*aResult)->Initialize();

    // the NP_Initialize entry point was misnamed as NP_PluginInit,
    // early in plugin project development.  Its correct name is
    // documented now, and new developers expect it to work.  However,
    // I don't want to break the plugins already in the field, so
    // we'll accept either name

    NP_PLUGININIT pfnInitialize =
        (NP_PLUGININIT)PR_FindSymbol(aLibrary, "NP_Initialize");

    if (!pfnInitialize) {
        pfnInitialize =
            (NP_PLUGININIT)PR_FindSymbol(aLibrary, "NP_PluginInit");
    }

    if (pfnInitialize == NULL)
        return NS_ERROR_UNEXPECTED; // XXX Right error?

	if (pfnInitialize(&(ns4xPlugin::CALLBACKS)) != NS_OK)
		return NS_ERROR_UNEXPECTED;
#endif

#if defined(XP_MAC) && !TARGET_CARBON
	// get the mainRD entry point
	NP_MAIN pfnMain = (NP_MAIN) PR_FindSymbol(aLibrary, "mainRD");
	if(pfnMain == NULL)
		return NS_ERROR_FAILURE;
		
	NPP_ShutdownUPP pfnShutdown;
	NPPluginFuncs callbacks;
    memset((void*) &callbacks, 0, sizeof(callbacks));
    callbacks.size = sizeof(callbacks);	

	nsPluginsDir pluginsDir;
	if(!pluginsDir.Valid())
		return NS_ERROR_FAILURE;
		
	short appRefNum = ::CurResFile();
	short pluginRefNum;
	for(nsDirectoryIterator iter(pluginsDir, PR_TRUE); iter.Exists(); iter++)
	{
		const nsFileSpec& file = iter;
		if (pluginsDir.IsPluginFile(file)) 
		{
				FSSpec spec = file;
				char* fileName = p2cstrdup(spec.name);
				if(!PL_strcmp(fileName, aFileName))
					{
					Boolean targetIsFolder, wasAliased;
					OSErr err = ::ResolveAliasFile(&spec, true, &targetIsFolder, &wasAliased);
					pluginRefNum = ::FSpOpenResFile(&spec, fsRdPerm);
					}
		}
	}

	// call into the entry point
	NPError error;
  NS_TRY_SAFE_CALL_RETURN(error, CallNPP_MainEntryProc(pfnMain, 
                                                       &(ns4xPlugin::CALLBACKS), 
                                                       &callbacks, 
                                                       &pfnShutdown), fLibrary);
  if(error != NPERR_NO_ERROR)
		return NS_ERROR_FAILURE;

	::UseResFile(appRefNum);

	if ((callbacks.version >> 8) < NP_VERSION_MAJOR)
		return NS_ERROR_FAILURE;
	
	// create the new plugin handler
	ns4xPlugin* plugin = new ns4xPlugin(&callbacks, aLibrary, (NP_PLUGINSHUTDOWN)pfnShutdown, aServiceMgr);

	if(plugin == NULL)
    	return NS_ERROR_OUT_OF_MEMORY;

	plugin->SetPluginRefNum(pluginRefNum);
	
    *aResult = plugin;
      
    NS_ADDREF(*aResult);
#endif

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
  
  // XXX This is suspicuous!
  ns4xPluginInstance *inst = new ns4xPluginInstance(&fCallbacks, fLibrary);

  if (inst == NULL) {  
    return NS_ERROR_OUT_OF_MEMORY;  
  }  

  NS_ADDREF(inst);  // Stabilize

  nsresult res = inst->QueryInterface(aIID, aResult);

  NS_RELEASE(inst); // Destabilize and avoid leaks. Avoid calling delete <interface pointer>    

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
	printf("shutting down plugin %08x\n",(int)this);
#endif
#ifdef XP_MAC
#if !TARGET_CARBON
	CallNPP_ShutdownProc(fShutdownEntry);
	::CloseResFile(fPluginRefNum);
#endif
#else
  NS_TRY_SAFE_CALL_VOID(fShutdownEntry(), fLibrary);
#endif

    fShutdownEntry = nsnull;
  }
  NS_IF_RELEASE(mMalloc);

  return NS_OK;
}

nsresult
ns4xPlugin::GetMIMEDescription(const char* *resultingDesc)
{
#ifdef NS_DEBUG
  printf("plugin getmimedescription called\n");
#endif
  const char* (*npGetMIMEDescrpition)() =
    (const char* (*)()) PR_FindSymbol(fLibrary, "NP_GetMIMEDescription");

  *resultingDesc = npGetMIMEDescrpition ? npGetMIMEDescrpition() : "";
  return NS_OK;
}

nsresult
ns4xPlugin::GetValue(nsPluginVariable variable, void *value)
{
#ifdef NS_DEBUG
  printf("plugin getvalue %d called\n", variable);
#endif

  NPError (*npGetValue)(void*, nsPluginVariable, void*) =
    (NPError (*)(void*, nsPluginVariable, void*)) PR_FindSymbol(fLibrary, "NP_GetValue");

  if (npGetValue && NPERR_NO_ERROR == npGetValue(nsnull, variable, value)) {
    return NS_OK;
  }
  else {
    return NS_ERROR_FAILURE;
  }
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
  NS_ASSERTION(mServiceMgr != NULL, "null service manager");

  if (inst == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(mServiceMgr == nsnull)
		return NPERR_GENERIC_ERROR;

	nsIPluginStreamListener* listener = nsnull;
	if(target == nsnull)
		inst->NewStream(&listener);

  nsIPluginManager * pm;
  mServiceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&pm);

	if(pm->GetURL(inst, relativeURL, target, listener) != NS_OK)
		return NPERR_GENERIC_ERROR;

  NS_RELEASE(pm);

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
  NS_ASSERTION(mServiceMgr != NULL, "null service manager");

  if (inst == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(mServiceMgr == nsnull)
		return NPERR_GENERIC_ERROR;

	nsIPluginStreamListener* listener = nsnull;
	if(target == nsnull)
		((ns4xPluginInstance*)inst)->NewNotifyStream(&listener, notifyData);

  nsIPluginManager * pm;
  mServiceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&pm);

	if(pm->GetURL(inst, relativeURL, target, listener) != NS_OK)
		return NPERR_GENERIC_ERROR;

  NS_RELEASE(pm);

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
  NS_ASSERTION(mServiceMgr != NULL, "null service manager");

  if (inst == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(mServiceMgr == nsnull)
		return NPERR_GENERIC_ERROR;

	nsIPluginStreamListener* listener = nsnull;
	if(target == nsnull)
		((ns4xPluginInstance*)inst)->NewNotifyStream(&listener, notifyData);

  nsIPluginManager * pm;
  mServiceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&pm);

	if(pm->PostURL(inst, relativeURL, len, buf, file, target, listener) != NS_OK)
		return NPERR_GENERIC_ERROR;

  NS_RELEASE(pm);

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
  NS_ASSERTION(mServiceMgr != NULL, "null service manager");

  if (inst == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(mServiceMgr == nsnull)
		return NPERR_GENERIC_ERROR;

	nsIPluginStreamListener* listener = nsnull;
	if(target == nsnull)
		inst->NewStream(&listener);

  nsIPluginManager * pm;
  mServiceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&pm);

	if(pm->PostURL(inst, relativeURL, len, buf, file, target, listener) != NS_OK)
		return NPERR_GENERIC_ERROR;

  NS_RELEASE(pm);

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
  NS_ASSERTION(mServiceMgr != NULL, "null service manager");

  if(mServiceMgr == nsnull)
		return;

  nsIPluginManager * pm;
  mServiceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&pm);

  pm->ReloadPlugins(reloadPages);

  NS_RELEASE(pm);
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
  nsresult res;

  if(!npp)
		return NPERR_INVALID_INSTANCE_ERROR;

  switch(variable)
  {
#ifdef XP_UNIX
  case NPNVxDisplay : return NPERR_GENERIC_ERROR;

  case NPNVxtAppContext : return NPERR_GENERIC_ERROR;
#endif

#ifdef XP_PC
  case NPNVnetscapeWindow : return NPERR_GENERIC_ERROR;
#endif

  case NPNVjavascriptEnabledBool: 
  {
    *(NPBool*)result = PR_TRUE; 

    NS_WITH_SERVICE(nsIPref, prefs, kPrefServiceCID, &res);
    if(NS_SUCCEEDED(res) && (nsnull != prefs))
    {
      PRBool js;
      res = prefs->GetBoolPref("javascript.enabled", &js);
      if(NS_SUCCEEDED(res))
        *(NPBool*)result = js; 
    }

    return NPERR_NO_ERROR;
  }

  case NPNVasdEnabledBool: 
    *(NPBool*)result = FALSE; 
    return NPERR_NO_ERROR;

  case NPNVisOfflineBool: 
    *(NPBool*)result = FALSE; 
    return NPERR_NO_ERROR;

  default : return NPERR_GENERIC_ERROR;

  }

#if 0
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
#endif
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
      {
        NPBool bWindowless = !(*((NPBool *)result));
        return inst->SetWindowless(bWindowless);
      }

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
  NS_ASSERTION(mServiceMgr != NULL, "null service manager");
  if (mServiceMgr == NULL)
    return NULL;

  char *retstr;

  nsIPluginManager * pm;
  mServiceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&pm);

  pm->UserAgent((const char **)&retstr);

  NS_RELEASE(pm);

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
    return NULL;
}



jref NP_EXPORT
ns4xPlugin::_getJavaPeer(NPP npp)
{
    return NULL;
}


// eof
