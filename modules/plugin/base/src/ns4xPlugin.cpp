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
#include "ns4xPluginStreamListener.h"
#include "nsIServiceManager.h"
#include "nsIMemory.h"
#include "nsIPluginStreamListener.h"
#include "nsPluginsDir.h"
#include "nsPluginSafety.h"
#include "nsIPref.h"
#include "nsPluginLogging.h"

#ifdef XP_MAC
#include <Resources.h>
#endif


////////////////////////////////////////////////////////////////////////
// CID's && IID's
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);
static NS_DEFINE_IID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);
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
// Globals
NPNetscapeFuncs ns4xPlugin::CALLBACKS;
static nsIServiceManager* gServiceMgr = nsnull;
static nsIMemory* gMalloc = nsnull;

////////////////////////////////////////////////////////////////////////
void
ns4xPlugin::CheckClassInitialized(void)
{
  static PRBool initialized = FALSE;

  if (initialized)
    return;

  // XXX It'd be nice to make this const and initialize it statically...
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

  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,("NPN callbacks initialized\n"));
}


////////////////////////////////////////////////////////////////////////
// nsISupports stuff
NS_IMPL_ISUPPORTS2(ns4xPlugin, nsIPlugin, nsIFactory);

ns4xPlugin::ns4xPlugin(NPPluginFuncs* callbacks, PRLibrary* aLibrary, NP_PLUGINSHUTDOWN aShutdown, nsIServiceManager* serviceMgr)
{
  NS_INIT_REFCNT();
  memset((void*) &fCallbacks, 0, sizeof(fCallbacks));
  gServiceMgr = serviceMgr;
  fLibrary = nsnull;

#if defined(XP_WIN)
  // On Windows (and Mac) we need to keep a direct reference to the fCallbacks and NOT
  // just copy the struct. See Bugzilla 85334

  NP_GETENTRYPOINTS pfnGetEntryPoints =
    (NP_GETENTRYPOINTS)PR_FindSymbol(aLibrary, "NP_GetEntryPoints");
  
  if (!pfnGetEntryPoints)
    return;

  fCallbacks.size = sizeof(fCallbacks);

  nsresult result = pfnGetEntryPoints(&fCallbacks);
  NS_ASSERTION( NS_OK == result,"Failed to get callbacks");

  NS_ASSERTION(HIBYTE(fCallbacks.version) >= NP_VERSION_MAJOR, "callback version is less than NP version");

  fShutdownEntry = (NP_PLUGINSHUTDOWN)PR_FindSymbol(aLibrary, "NP_Shutdown");
#elif defined(XP_MAC) && !TARGET_CARBON
  // get the main entry point
  NP_MAIN pfnMain = (NP_MAIN) PR_FindSymbol(aLibrary, "mainRD");

  if(pfnMain == NULL)
    return;

  // call into the entry point
  NPError error;
  NS_TRY_SAFE_CALL_RETURN(error, CallNPP_MainEntryProc(pfnMain, 
                                                       &(ns4xPlugin::CALLBACKS), 
                                                       &fCallbacks, 
                                                       &fShutdownEntry), aLibrary);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPP MainEntryProc called, return=%d\n",error));

  if(error != NPERR_NO_ERROR || ((fCallbacks.version >> 8) < NP_VERSION_MAJOR))
    return;

#else // for everyone else
  memcpy((void*) &fCallbacks, (void*) callbacks, sizeof(fCallbacks));
  fShutdownEntry = aShutdown;
#endif

  fLibrary = aLibrary;
}


////////////////////////////////////////////////////////////////////////
ns4xPlugin::~ns4xPlugin(void)
{
  //reset the callbacks list
  memset((void*) &fCallbacks, 0, sizeof(fCallbacks));
}


////////////////////////////////////////////////////////////////////////
void ns4xPlugin::ReleaseStatics()
{
  NS_IF_RELEASE(gMalloc);
}


#ifdef XP_MAC
////////////////////////////////////////////////////////////////////////
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

////////////////////////////////////////////////////////////////////////
void ns4xPlugin::SetPluginRefNum(short aRefNum)
{
  fPluginRefNum = aRefNum;
}
#endif


////////////////////////////////////////////////////////////////////////
// Static factory method.
//
///CreatePlugin()
//--------------
//Handles the initialization of old, 4x style plugins.  Creates the ns4xPlugin object.
//One ns4xPlugin object exists per Plugin (not instance).

nsresult
ns4xPlugin::CreatePlugin(nsIServiceManager* aServiceMgr,
                         const char* aFileName,
                         PRLibrary* aLibrary,
                         nsIPlugin** aResult)
{
  CheckClassInitialized();

  // set up the MemAllocator service now because it might be used by the plugin
  if (aServiceMgr != nsnull) {
    if (nsnull == gMalloc)
      aServiceMgr->GetService(kMemoryCID, kIMemoryIID, (nsISupports**)&gMalloc);
  }

#ifdef XP_UNIX

  ns4xPlugin *plptr;

  NPPluginFuncs callbacks;
  memset((void*) &callbacks, 0, sizeof(callbacks));
  callbacks.size = sizeof(callbacks);

  NP_PLUGINSHUTDOWN pfnShutdown = (NP_PLUGINSHUTDOWN)PR_FindSymbol(aLibrary, "NP_Shutdown");

  // create the new plugin handler
  *aResult = plptr = new ns4xPlugin(&callbacks, aLibrary, pfnShutdown, aServiceMgr);

  if (*aResult == NULL)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);

  // we must init here because the plugin may call NPN functions
  // when we call into the NP_Initialize entry point - NPN functions
  // require that mBrowserManager be set up
  plptr->Initialize();

  NP_PLUGINUNIXINIT pfnInitialize = (NP_PLUGINUNIXINIT)PR_FindSymbol(aLibrary, "NP_Initialize");

  if (pfnInitialize == NULL)
    return NS_ERROR_UNEXPECTED; // XXX Right error?

  if (pfnInitialize(&(ns4xPlugin::CALLBACKS),&callbacks) != NS_OK)
    return NS_ERROR_UNEXPECTED;

  // now copy function table back to ns4xPlugin instance
  memcpy((void*) &(plptr->fCallbacks), (void*)&callbacks, sizeof(callbacks));
#endif

#if defined(XP_PC) && !defined(XP_WIN)
  // XXX this probably should be factored out and
  //      just use trailing XP_WIN.
  NP_GETENTRYPOINTS pfnGetEntryPoints = (NP_GETENTRYPOINTS)PR_FindSymbol(aLibrary, "NP_GetEntryPoints");

  if (pfnGetEntryPoints == NULL)
    return NS_ERROR_FAILURE;

  NPPluginFuncs callbacks;
  memset((void*) &callbacks, 0, sizeof(callbacks));

  callbacks.size = sizeof(callbacks);

  if (pfnGetEntryPoints(&callbacks) != NS_OK)
    return NS_ERROR_FAILURE; // XXX

  if (HIBYTE(callbacks.version) < NP_VERSION_MAJOR)
    return NS_ERROR_FAILURE;

  NP_PLUGINSHUTDOWN pfnShutdown = (NP_PLUGINSHUTDOWN)PR_FindSymbol(aLibrary, "NP_Shutdown");

  // create the new plugin handler
  *aResult = new ns4xPlugin(&callbacks, aLibrary, pfnShutdown, aServiceMgr);
#elif defined(XP_WIN)
  // Note: on Windows, we must use the fCallback because plugins may change
  // the function table. The Shockwave installer makes changes in the table while running
  *aResult = new ns4xPlugin(nsnull, aLibrary, nsnull, aServiceMgr);
#endif
#ifdef XP_PC
  if (*aResult == NULL)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);

  // we must init here because the plugin may call NPN functions 
  // when we call into the NP_Initialize entry point - NPN functions
  // require that mBrowserManager be set up
  if (NS_FAILED((*aResult)->Initialize())) {
    NS_RELEASE(*aResult);
    return NS_ERROR_FAILURE;
  }

  // the NP_Initialize entry point was misnamed as NP_PluginInit,
  // early in plugin project development.  Its correct name is
  // documented now, and new developers expect it to work.  However,
  // I don't want to break the plugins already in the field, so
  // we'll accept either name

  NP_PLUGININIT pfnInitialize = (NP_PLUGININIT)PR_FindSymbol(aLibrary, "NP_Initialize");

  if (!pfnInitialize)
    pfnInitialize = (NP_PLUGININIT)PR_FindSymbol(aLibrary, "NP_PluginInit");

  if (pfnInitialize == NULL)
    return NS_ERROR_UNEXPECTED; // XXX Right error?

  if (pfnInitialize(&(ns4xPlugin::CALLBACKS)) != NS_OK)
    return NS_ERROR_UNEXPECTED;
#endif

#if defined(XP_MAC)
  nsPluginsDir pluginsDir(PLUGINS_DIR_LOCATION_MAC_SYSTEM_PLUGINS_FOLDER);

  short appRefNum = ::CurResFile();
  short pluginRefNum;
  Boolean found = false;

  if (pluginsDir.Valid()) { 
    for(nsDirectoryIterator iter(pluginsDir, PR_TRUE); iter.Exists(); iter++) {
      const nsFileSpec& file = iter;
      if (pluginsDir.IsPluginFile(file)) {
        FSSpec spec = file;
        if (!nsCRT::memcmp(spec.name + 1, aFileName, spec.name[0])) {
          nsPluginFile pluginFile(file);
          pluginRefNum = pluginFile.OpenPluginResource();
          if (pluginRefNum != -1)
            found = true;
        }
      }
    }
  }

  // if we didn't find it, try locally
  if (!found) {
    nsPluginsDir pluginsDir(PLUGINS_DIR_LOCATION_MOZ_LOCAL);
    if (!pluginsDir.Valid())
      return NS_ERROR_FAILURE;

    appRefNum = ::CurResFile();
    for(nsDirectoryIterator iter(pluginsDir, PR_TRUE); iter.Exists(); iter++) {
      const nsFileSpec& file = iter;
      if (pluginsDir.IsPluginFile(file)) {
        FSSpec spec = file;
        if (!nsCRT::memcmp(spec.name + 1, aFileName, spec.name[0])) {
          nsPluginFile pluginFile(file);
          pluginRefNum = pluginFile.OpenPluginResource();
          if (pluginRefNum != -1)
            found = true;
        }
      }
    }
  }

  if (!found)
    return NS_ERROR_FAILURE;

#if TARGET_CARBON
  // call into the entry point
  NP_MAIN pfnMain = (NP_MAIN) PR_FindSymbol(aLibrary, "main");

  if(pfnMain == NULL)
    return NS_ERROR_FAILURE;

  NPP_ShutdownUPP pfnShutdown;
  NPPluginFuncs callbacks;
  memset((void*) &callbacks, 0, sizeof(callbacks));
  callbacks.size = sizeof(callbacks);
  NPError error;

  NS_TRY_SAFE_CALL_RETURN(error, CallNPP_MainEntryProc(pfnMain, 
                                                       &(ns4xPlugin::CALLBACKS), 
                                                       &callbacks, 
                                                       &pfnShutdown), fLibrary);

  NPP_PLUGIN_LOG(PLUGIN_LOG_BASIC, ("NPP MainEntryProc called: return=%d\n",error));

  if(error != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;

  if ((callbacks.version >> 8) < NP_VERSION_MAJOR)
    return NS_ERROR_FAILURE;

  // create the new plugin handler
  ns4xPlugin* plugin = new ns4xPlugin(&callbacks, aLibrary, (NP_PLUGINSHUTDOWN)pfnShutdown, aServiceMgr);
#else // not carbon
  ns4xPlugin* plugin = new ns4xPlugin(nsnull, aLibrary, nsnull, aServiceMgr);
#endif
  if(plugin == NULL)
    return NS_ERROR_OUT_OF_MEMORY;

  ::UseResFile(appRefNum);
  *aResult = plugin;

  NS_ADDREF(*aResult);
  if (NS_FAILED((*aResult)->Initialize())) {
    NS_RELEASE(*aResult);
    return NS_ERROR_FAILURE;
  }

  plugin->SetPluginRefNum(pluginRefNum);
#endif  // XP_MAC

#ifdef XP_BEOS
  // I just copied UNIX version.
  // Makoto Hamanaka <VYA04230@nifty.com>

  ns4xPlugin *plptr;

  NPPluginFuncs callbacks;
  memset((void*) &callbacks, 0, sizeof(callbacks));
  callbacks.size = sizeof(callbacks);

  NP_PLUGINSHUTDOWN pfnShutdown = (NP_PLUGINSHUTDOWN)PR_FindSymbol(aLibrary, "NP_Shutdown");

  // create the new plugin handler
  *aResult = plptr = new ns4xPlugin(&callbacks, aLibrary, pfnShutdown, aServiceMgr);

  if (*aResult == NULL)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(*aResult);

  // we must init here because the plugin may call NPN functions
  // when we call into the NP_Initialize entry point - NPN functions
  // require that mBrowserManager be set up
  plptr->Initialize();

  NP_PLUGINUNIXINIT pfnInitialize = (NP_PLUGINUNIXINIT)PR_FindSymbol(aLibrary, "NP_Initialize");

  if (pfnInitialize == NULL)
    return NS_ERROR_FAILURE;

  if (pfnInitialize(&(ns4xPlugin::CALLBACKS),&callbacks) != NS_OK)
    return NS_ERROR_FAILURE;

  // now copy function table back to ns4xPlugin instance
  memcpy((void*) &(plptr->fCallbacks), (void*)&callbacks, sizeof(callbacks));
#endif

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
//CreateInstance()
//----------------
//Creates a ns4xPluginInstance object.

nsresult ns4xPlugin :: CreateInstance(nsISupports *aOuter,  
                                      const nsIID &aIID,  
                                      void **aResult)  
{
  if (aResult == NULL)
    return NS_ERROR_NULL_POINTER;

  *aResult = NULL;

  // XXX This is suspicuous!
  ns4xPluginInstance *inst = new ns4xPluginInstance(&fCallbacks, fLibrary);

  if (inst == NULL) 
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(inst);  // Stabilize

  nsresult res = inst->QueryInterface(aIID, aResult);

  NS_RELEASE(inst); // Destabilize and avoid leaks. Avoid calling delete <interface pointer>    

  return res;
}


////////////////////////////////////////////////////////////////////////
nsresult ns4xPlugin :: LockFactory(PRBool aLock)  
{  
  // Not implemented in simplest case.  
  return NS_OK;
}  


////////////////////////////////////////////////////////////////////////
NS_METHOD ns4xPlugin :: CreatePluginInstance(nsISupports *aOuter,
                                             REFNSIID    aIID, 
                                             const char  *aPluginMIMEType,
                                             void        **aResult)
{
  return CreateInstance(aOuter, aIID, aResult);
}


////////////////////////////////////////////////////////////////////////
nsresult
ns4xPlugin::Initialize(void)
{
  if (nsnull == fLibrary)
    return NS_ERROR_FAILURE;
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult
ns4xPlugin::Shutdown(void)
{
  NPP_PLUGIN_LOG(PLUGIN_LOG_BASIC, ("NPP Shutdown to be called: this=%p\n",this));

  if (nsnull != fShutdownEntry) {
#ifdef XP_MAC
    CallNPP_ShutdownProc(fShutdownEntry);
    ::CloseResFile(fPluginRefNum);
#else
    NS_TRY_SAFE_CALL_VOID(fShutdownEntry(), fLibrary);
#endif

    fShutdownEntry = nsnull;
  }

  PLUGIN_LOG(PLUGIN_LOG_NORMAL,("4xPlugin Shutdown done, this=%p",this));
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult
ns4xPlugin::GetMIMEDescription(const char* *resultingDesc)
{
  const char* (*npGetMIMEDescrpition)() = (const char* (*)()) PR_FindSymbol(fLibrary, "NP_GetMIMEDescription");

  *resultingDesc = npGetMIMEDescrpition ? npGetMIMEDescrpition() : "";

  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("ns4xPlugin::GetMIMEDescription called: this=%p, result=%s\n",this, *resultingDesc));

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult
ns4xPlugin::GetValue(nsPluginVariable variable, void *value)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("ns4xPlugin::GetValue called: this=%p, variable=%d\n",this,variable));

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
_geturl(NPP npp, const char* relativeURL, const char* target)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPN_GetURL: npp=%p, target=%s, url=%s\n", (void *)npp, target, relativeURL));

  if(!npp)
    return NPERR_INVALID_INSTANCE_ERROR;

  nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

  NS_ASSERTION(inst != NULL, "null instance");
  NS_ASSERTION(gServiceMgr != NULL, "null service manager");

  if (inst == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(gServiceMgr == nsnull)
    return NPERR_GENERIC_ERROR;

  nsIPluginStreamListener* listener = nsnull;
  if(target == nsnull)
    inst->NewStream(&listener);

  nsIPluginManager * pm;
  gServiceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&pm);

  if(pm->GetURL(inst, relativeURL, target, listener) != NS_OK)
    return NPERR_GENERIC_ERROR;

  NS_RELEASE(pm);

  return NPERR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
_geturlnotify(NPP npp, const char* relativeURL, const char* target, void* notifyData)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, 
    ("NPN_GetURLNotify: npp=%p, target=%s, notify=%p, url=%s\n", (void*)npp, target, notifyData, relativeURL));

  if(!npp)
    return NPERR_INVALID_INSTANCE_ERROR;

  nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

  NS_ASSERTION(inst != NULL, "null instance");
  NS_ASSERTION(gServiceMgr != NULL, "null service manager");

  if (inst == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(gServiceMgr == nsnull)
    return NPERR_GENERIC_ERROR;

  nsIPluginStreamListener* listener = nsnull;
  if(target == nsnull)
    ((ns4xPluginInstance*)inst)->NewNotifyStream(&listener, notifyData);

  nsIPluginManager * pm;
  gServiceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&pm);

  if(pm->GetURL(inst, relativeURL, target, listener) != NS_OK)
    return NPERR_GENERIC_ERROR;

  NS_RELEASE(pm);

  return NPERR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
_posturlnotify(NPP npp, const char *relativeURL, const char *target,
               uint32 len, const char *buf, NPBool file, void *notifyData)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPN_PostURLNotify: npp=%p, target=%s, len=%d, file=%d, notify=%p, url=%s, buf=%s\n",
  (void*)npp, target, len, file, notifyData, relativeURL, buf));

  if(!npp)
    return NPERR_INVALID_INSTANCE_ERROR;

  nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

  NS_ASSERTION(inst != NULL, "null instance");
  NS_ASSERTION(gServiceMgr != NULL, "null service manager");

  if (inst == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(gServiceMgr == nsnull)
    return NPERR_GENERIC_ERROR;

  nsIPluginStreamListener* listener = nsnull;
  if(target == nsnull)
    ((ns4xPluginInstance*)inst)->NewNotifyStream(&listener, notifyData);

  nsIPluginManager * pm;
  gServiceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&pm);

  if(pm->PostURL(inst, relativeURL, len, buf, file, target, listener) != NS_OK)
    return NPERR_GENERIC_ERROR;

  NS_RELEASE(pm);

  return NPERR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
_posturl(NPP npp, const char *relativeURL, const char *target,
         uint32 len, const char *buf, NPBool file)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPN_PostURL: npp=%p, target=%s, file=%d, len=%d, url=%s, buf=%s\n",
  (void*)npp, target, file, len, relativeURL, buf));

  if(!npp)
    return NPERR_INVALID_INSTANCE_ERROR;

  nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

  NS_ASSERTION(inst != NULL, "null instance");
  NS_ASSERTION(gServiceMgr != NULL, "null service manager");

  if (inst == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  if(gServiceMgr == nsnull)
    return NPERR_GENERIC_ERROR;

  nsIPluginStreamListener* listener = nsnull;
  if(target == nsnull)
    inst->NewStream(&listener);

  nsIPluginManager * pm;
  gServiceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&pm);

  if(pm->PostURL(inst, relativeURL, len, buf, file, target, listener) != NS_OK)
    return NPERR_GENERIC_ERROR;

  NS_RELEASE(pm);

  return NPERR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////
// A little helper class used to wrap up plugin manager streams (that is,
// streams from the plugin to the browser).

class ns4xStreamWrapper
{
protected:
  nsIOutputStream *fStream;
  NPStream        fNPStream;

public:
  ns4xStreamWrapper(nsIOutputStream* stream);
  ~ns4xStreamWrapper();

  void GetStream(nsIOutputStream* &result);
  NPStream* GetNPStream(void) { return &fNPStream; };
};

ns4xStreamWrapper::ns4xStreamWrapper(nsIOutputStream* stream)
  : fStream(stream)
{
  NS_ASSERTION(stream != NULL, "bad stream");

  fStream = stream;
  NS_ADDREF(fStream);

  memset(&fNPStream, 0, sizeof(fNPStream));
  fNPStream.ndata = (void*) this;
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
_newstream(NPP npp, NPMIMEType type, const char* window, NPStream* *result)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPN_NewStream: npp=%p, type=%s, window=%s\n", (void*)npp, (const char *)type, window));

  if(!npp)
    return NPERR_INVALID_INSTANCE_ERROR;

  nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

  NS_ASSERTION(inst != NULL, "null instance");

  if (inst == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  nsIOutputStream* stream;
  nsIPluginInstancePeer *peer;

  if (NS_OK == inst->GetPeer(&peer)) {
    if (peer->NewStream((const char*) type, window, &stream) != NS_OK) {
      NS_RELEASE(peer);
      return NPERR_GENERIC_ERROR;
    }

    ns4xStreamWrapper* wrapper = new ns4xStreamWrapper(stream);

    if (wrapper == NULL) {
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


////////////////////////////////////////////////////////////////////////
int32 NP_EXPORT
_write(NPP npp, NPStream *pstream, int32 len, void *buffer)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPN_Write: npp=%p, url=%s, len=%d, buffer=%s\n", (void*)npp, pstream->url, len, (char*)buffer));

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


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
_destroystream(NPP npp, NPStream *pstream, NPError reason)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPN_DestroyStream: npp=%p, url=%s, reason=%d\n", (void*)npp, pstream->url, (int)reason));

  if(!npp)
    return NPERR_INVALID_INSTANCE_ERROR;

  nsISupports* stream = (nsISupports*) pstream->ndata;
  nsIPluginStreamListener* listener;

  // DestroyStream can kill two kinds of streams: NPP derived and NPN derived.
  // check to see if they're trying to kill a NPP stream
  if(stream->QueryInterface(kIPluginStreamListenerIID, (void**)&listener) == NS_OK) {
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
  pstream->ndata = nsnull;
  return NPERR_NO_ERROR;
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
_status(NPP npp, const char *message)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_Status: npp=%p, message=%s\n", (void*)npp, message));

  if(!npp)
    return;

  nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;
  NS_ASSERTION(inst != NULL, "null instance");

  if (inst == NULL)
    return;

  nsIPluginInstancePeer *peer;

  if (NS_OK == inst->GetPeer(&peer)){
    peer->ShowStatus(message);
    NS_RELEASE(peer);
  }
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
_memfree (void *ptr)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY, ("NPN_MemFree: ptr=%p\n", ptr));

  if(ptr)
    gMalloc->Free(ptr);
}


////////////////////////////////////////////////////////////////////////
uint32 NP_EXPORT
_memflush(uint32 size)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY, ("NPN_MemFlush: size=%d\n", size));

  gMalloc->HeapMinimize(PR_TRUE);
  return 0;
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
_reloadplugins(NPBool reloadPages)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_ReloadPlugins: reloadPages=%d\n", reloadPages));
  NS_ASSERTION(gServiceMgr != NULL, "null service manager");

  if(gServiceMgr == nsnull)
    return;

  nsIPluginManager * pm;
  gServiceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&pm);

  pm->ReloadPlugins(reloadPages);

  NS_RELEASE(pm);
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
_invalidaterect(NPP npp, NPRect *invalidRect)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPN_InvalidateRect: npp=%p, top=%d, left=%d, bottom=%d, right=%d\n",
  (void *)npp, invalidRect->top, invalidRect->left, invalidRect->bottom, invalidRect->right));

  if(!npp)
    return;

  nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

  NS_ASSERTION(inst != NULL, "null instance");

  if (inst == NULL)
    return;

  nsIPluginInstancePeer *peer;
  nsIWindowlessPluginInstancePeer *wpeer;

  if (NS_OK == inst->GetPeer(&peer)) {
    if (NS_OK == peer->QueryInterface(kIWindowlessPluginInstancePeerIID, (void **)&wpeer)) {
      // XXX nsRect & NPRect are structurally equivalent
      wpeer->InvalidateRect((nsPluginRect *)invalidRect);
      NS_RELEASE(wpeer);
    }
    NS_RELEASE(peer);
  }
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
_invalidateregion(NPP npp, NPRegion invalidRegion)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPN_InvalidateRegion: npp=%p, region=%p\n", (void*)npp, (void*)invalidRegion));

  if(!npp)
    return;

  nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;
  NS_ASSERTION(inst != NULL, "null instance");

  if (inst == NULL)
    return;

  nsIPluginInstancePeer *peer;
  nsIWindowlessPluginInstancePeer *wpeer;

  if (NS_OK == inst->GetPeer(&peer)) {
    if (NS_OK == peer->QueryInterface(kIWindowlessPluginInstancePeerIID, (void **)&wpeer)) {
      // XXX nsRegion & NPRegion are typedef'd to the same thing
      wpeer->InvalidateRegion((nsPluginRegion)invalidRegion);
      NS_RELEASE(wpeer);
    }

    NS_RELEASE(peer);
  }
}


////////////////////////////////////////////////////////////////////////
void NP_EXPORT
_forceredraw(NPP npp)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_ForceDraw: npp=%p\n", (void*)npp));

  if(!npp)
    return;

  nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;
  NS_ASSERTION(inst != NULL, "null instance");

  if (inst == NULL)
    return;

  nsIPluginInstancePeer *peer;
  nsIWindowlessPluginInstancePeer *wpeer;

  if (NS_OK == inst->GetPeer(&peer)) {
    if (NS_OK == peer->QueryInterface(kIWindowlessPluginInstancePeerIID, (void **)&wpeer)) {
      wpeer->ForceRedraw();
      NS_RELEASE(wpeer);
    }
    NS_RELEASE(peer);
  }
}


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
_getvalue(NPP npp, NPNVariable variable, void *result)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_GetValue: npp=%p, var=%d\n", (void*)npp, (int)variable));

  nsresult res;

  if(!npp || !npp->ndata)
    return NPERR_INVALID_INSTANCE_ERROR;

  switch(variable) {
#ifdef XP_UNIX
  case NPNVxDisplay: 
    return NPERR_GENERIC_ERROR;

  case NPNVxtAppContext: 
    return NPERR_GENERIC_ERROR;
#endif

#ifdef XP_PC
  case NPNVnetscapeWindow: {
    ns4xPluginInstance *inst = (ns4xPluginInstance *) npp->ndata;
    nsIPluginInstancePeer *peer;

    if (NS_SUCCEEDED(inst->GetPeer(&peer)) && peer) {
      res = peer->GetValue(nsPluginInstancePeerVariable_NetscapeWindow, result);
      NS_RELEASE(peer);
      return res;
    }
    return NPERR_GENERIC_ERROR;
  }
#endif

  case NPNVjavascriptEnabledBool: {
    *(NPBool*)result = PR_TRUE; 

    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID, &res));
    if(NS_SUCCEEDED(res) && (nsnull != prefs)) {
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

  case NPNVserviceManager: {
    // GetGlobalServiceManager does not AddRef, so we do here.
    nsIServiceManager* serviceManager;
    res = nsServiceManager::GetGlobalServiceManager(&serviceManager);
    if (NS_SUCCEEDED(res)) {
      *(nsIServiceManager**)result = serviceManager;
      NS_ADDREF(serviceManager);
      return NPERR_NO_ERROR;
    } else
      return NPERR_GENERIC_ERROR;
  }

  default : return NPERR_GENERIC_ERROR;
  }

#if 0
  nsIPluginInstance *inst = (nsIPluginInstance *) npp->ndata;

  NS_ASSERTION(inst != NULL, "null instance");

  if (inst == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  nsIPluginInstancePeer *peer;

  if (NS_OK == inst->GetPeer(&peer)) {
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


////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
_setvalue(NPP npp, NPPVariable variable, void *result)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_SetValue: npp=%p, var=%d\n", (void*)npp, (int)variable));

  if(!npp)
    return NPERR_INVALID_INSTANCE_ERROR;

  ns4xPluginInstance *inst = (ns4xPluginInstance *) npp->ndata;
  
  NS_ASSERTION(inst != NULL, "null instance");

  if (inst == NULL)
    return NPERR_INVALID_INSTANCE_ERROR;

  switch (variable) {
    case NPPVpluginWindowBool: {
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
  
  if (NS_OK == inst->GetPeer(&peer)) {
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

////////////////////////////////////////////////////////////////////////
NPError NP_EXPORT
_requestread(NPStream *pstream, NPByteRange *rangeList)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_RequestRead: stream=%p\n", (void*)pstream));

#ifdef PLUGIN_LOGGING
  for(NPByteRange * range = rangeList; range != nsnull; range = range->next)
    PR_LOG(nsPluginLogging::gNPNLog,PLUGIN_LOG_NOISY, 
    ("%i-%i", range->offset, range->offset + range->length - 1));

  PR_LOG(nsPluginLogging::gNPNLog,PLUGIN_LOG_NOISY, ("\n\n"));
  PR_LogFlush();
#endif
  
  if(!pstream || !rangeList || !pstream->ndata)
    return NPERR_INVALID_PARAM;
  
  nsresult res = NS_OK;
  
  ns4xPluginStreamListener * streamlistener = (ns4xPluginStreamListener *)pstream->ndata;
  
  if(NS_FAILED(res))
    return NPERR_GENERIC_ERROR;
  
  nsPluginStreamType streamtype = nsPluginStreamType_Normal;
  
  streamlistener->GetStreamType(&streamtype);
  
  if(streamtype != nsPluginStreamType_Seek)
    return NPERR_STREAM_NOT_SEEKABLE;
  
  if(streamlistener->mStreamInfo)
    streamlistener->mStreamInfo->RequestRead((nsByteRange *)rangeList);
  
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
//
// On 68K Mac (XXX still supported?), we need to make sure that the
// pointers are in D0 for the following functions that return pointers.
//

#if defined(XP_MAC) && !defined(powerc)
#pragma pointers_in_D0
#endif

////////////////////////////////////////////////////////////////////////
JRIEnv* NP_EXPORT
_getJavaEnv(void)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_GetJavaEnv\n"));
  return NULL;
}

////////////////////////////////////////////////////////////////////////
const char * NP_EXPORT
_useragent(NPP npp)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_UserAgent: npp=%p\n", (void*)npp));

  NS_ASSERTION(gServiceMgr != NULL, "null service manager");
  if (gServiceMgr == NULL)
    return NULL;
  
  char *retstr;
  
  nsIPluginManager * pm;
  gServiceMgr->GetService(kPluginManagerCID, kIPluginManagerIID, (nsISupports**)&pm);
  
  pm->UserAgent((const char **)&retstr);
  
  NS_RELEASE(pm);
  
  return retstr;
}


////////////////////////////////////////////////////////////////////////
void * NP_EXPORT
_memalloc (uint32 size)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NOISY, ("NPN_MemAlloc: size=%d\n", size));
  return gMalloc->Alloc(size);
}


////////////////////////////////////////////////////////////////////////
java_lang_Class* NP_EXPORT
_getJavaClass(void* handle)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_GetJavaClass\n"));
  return NULL;
}


////////////////////////////////////////////////////////////////////////
jref NP_EXPORT
_getJavaPeer(NPP npp)
{
  NPN_PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("NPN_GetJavaPeer: npp=%p\n", (void*)npp));
  return NULL;
}
