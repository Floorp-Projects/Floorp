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

#ifdef XP_UNIX
#define TRACE(foo) trace(foo)
#endif
#if defined(__cplusplus)
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
// SECTION 1 - Includes
////////////////////////////////////////////////////////////////////////////////
#ifdef XP_UNIX
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#else
#include <windows.h>
#endif

#include "nsIPlug.h"
#include "npapi.h"
#include "badapter.h"
#include "Debug.h"

////////////////////////////////////////////////////////////////////////////////
// SECTION 2 - Globar Variables
////////////////////////////////////////////////////////////////////////////////

//
// thePlugin and thePluginManager are used in the life of the plugin.
//
// These two will be created on NPP_Initialize and destroyed on NPP_Shutdown.
//
nsIPluginManager* thePluginManager = NULL;
nsIPlugin* thePlugin = NULL;

//
// nsISupports IDs
//
// Interface IDs for nsISupports
//
NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_DEFINE_IID(kPluginIID, NS_IPLUGIN_IID);
NS_DEFINE_IID(kPluginInstanceIID, NS_IPLUGININSTANCE_IID);
NS_DEFINE_IID(kPluginManagerIID, NS_IPLUGINMANAGER_IID);
NS_DEFINE_IID(kPluginTagInfoIID, NS_IPLUGINTAGINFO_IID);
NS_DEFINE_IID(kOutputStreamIID, NS_IOUTPUTSTREAM_IID);
NS_DEFINE_IID(kPluginInstancePeerIID, NS_IPLUGININSTANCEPEER_IID); 
NS_DEFINE_IID(kPluginStreamPeerIID, NS_IPLUGINSTREAMPEER_IID);
NS_DEFINE_IID(kSeekablePluginStreamPeerIID, NS_ISEEKABLEPLUGINSTREAMPEER_IID);


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
    fprintf(stderr, "MIME description\n");
    if (thePlugin == NULL) {
	freeFac = 1;
	NSGetFactory(kPluginIID, (nsIFactory** )(&thePlugin));
    }
    fprintf(stderr, "Allocated Plugin 0x%08x\n", thePlugin);
    char * ret =  (char *) thePlugin->GetMIMEDescription();
    fprintf(stderr, "Get response %s\n", ret);
    if (freeFac) {
	fprintf(stderr, "Freeing plugin...");
	thePlugin->Release();
	thePlugin = NULL;
    }
    fprintf(stderr, "Done\n");
    return ret;
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
    return (NPError)nsPluginError_NoError;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_GetValue:
//+++++++++++++++++++++++++++++++++++++++++++++++++
NPError
NPP_GetValue(NPP instance, NPPVariable variable, void *value) {
    int freeFac = 0;
    fprintf(stderr, "MIME description\n");
    if (thePlugin == NULL) {
	freeFac = 1;
	if (NSGetFactory(kPluginIID, (nsIFactory** )(&thePlugin)) != NS_OK)
	    return NPERR_GENERIC_ERROR;
    }
    fprintf(stderr, "Allocated Plugin 0x%08x\n", thePlugin);
    NPError ret = (NPError) thePlugin->GetValue((nsPluginVariable)variable, value);
    fprintf(stderr, "Get response %08x\n", ret);
    if (freeFac) {
	fprintf(stderr, "Freeing plugin...");
	thePlugin->Release();
	thePlugin = NULL;
    }
    fprintf(stderr, "Done\n");
    return ret;
}
#endif
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
    TRACE("NPP_Initialize\n");

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
    nsresult error = NS_OK;  
    // On UNIX the plugin might have been created when calling NPP_GetMIMEType.
    if (thePlugin == NULL)
	// create nsIPlugin factory
	error = (NPError)NSGetFactory(kPluginIID, (nsIFactory**) &thePlugin);
    if (error == NS_OK)
	thePlugin->Initialize(thePluginManager);

    return (NPError) error;	
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
  /*  if (thePluginManager == NULL) {
        // Create the plugin manager and plugin objects.
        NPError result = CPluginManager::Create();	
        if (result) return NULL;
	    assert( thePluginManager != NULL );
        thePluginManager->AddRef();
        NP_CreatePlugin(thePluginManager, (nsIPlugin** )(&thePlugin));
        assert( thePlugin != NULL );
    }
*/
//	return thePlugin->GetJavaClass();
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
	TRACE("NPP_Shutdown\n");

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
    TRACE("NPP_New\n");
    
    if (instance == NULL)
	return NPERR_INVALID_INSTANCE_ERROR;

    // Create a new plugin instance and start it.
    nsIPluginInstance* pluginInstance = NULL;
    thePlugin->CreateInstance(NULL, kPluginInstanceIID, (void**)&pluginInstance);
    if (pluginInstance == NULL) {
	return NPERR_OUT_OF_MEMORY_ERROR;
    } 
    
    // Create a new plugin instance peer,
    // XXX - Since np_instance is not implemented in the 4.0x browser, I
    // XXX - had to save the plugin parameter in the peer class.
    // XXX - Ask Warren about np_instance.
    CPluginInstancePeer* peer = 
	new CPluginInstancePeer(instance, (nsMIMEType)pluginType, 
				(nsPluginType)mode, (PRUint16)argc, 
				(const char** )argn, (const char** )argv);
    assert( peer != NULL );
    if (!peer) return NPERR_OUT_OF_MEMORY_ERROR;
    peer->AddRef();
    pluginInstance->Initialize(peer);
    pluginInstance->Start();
    // Set the user instance and store the peer in npp->pdata.
    instance->pdata = pluginInstance;
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
    TRACE("NPP_Destroy\n");
    
    if (instance == NULL)
	return NPERR_INVALID_INSTANCE_ERROR;
    
    nsIPluginInstance* pluginInstance = (nsIPluginInstance* )instance->pdata;
    pluginInstance->Stop();
    pluginInstance->Destroy();
    pluginInstance->Release();
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
    TRACE("NPP_SetWindow\n");
    
    if (instance == NULL)
	return NPERR_INVALID_INSTANCE_ERROR;

    nsIPluginInstance* pluginInstance = (nsIPluginInstance* )instance->pdata;
    
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
   
    TRACE("NPP_NewStream\n");

    if (instance == NULL)
	return NPERR_INVALID_INSTANCE_ERROR;
				
    // Create a new plugin stream peer and plugin stream.
    CPluginStreamPeer* speer = new CPluginStreamPeer((nsMIMEType)type, stream,
						     (PRBool)seekable, stype); 
    if (speer == NULL) return NPERR_OUT_OF_MEMORY_ERROR;
    speer->AddRef();
    nsIPluginStream* pluginStream = NULL; 
    nsIPluginInstance* pluginInstance = (nsIPluginInstance*) instance->pdata;
    NPError result = (NPError)pluginInstance->NewStream(speer, &pluginStream);
    speer->Release();
    
    if (pluginStream == NULL)
	return NPERR_OUT_OF_MEMORY_ERROR;
		
    stream->pdata = (void*) pluginStream;
    *stype = pluginStream->GetStreamType();
	
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
    TRACE("NPP_WriteReady\n");

    if (instance == NULL)
	return -1;

    nsIPluginStream* theStream = (nsIPluginStream*) stream->pdata;	
    if( theStream == 0 )
	return -1;
	
    return 8192;
}


//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_Write:
// Delivers data from a stream and returns the number of bytes written. 
//+++++++++++++++++++++++++++++++++++++++++++++++++
int32 
NPP_Write(NPP instance, NPStream *stream, int32 offset, int32 len, void *buffer)
{
    TRACE("NPP_Write\n");

    if (instance == NULL)
	return -1;
	
    nsIPluginStream* theStream = (nsIPluginStream*) stream->pdata;
    if( theStream == 0 )
	return -1;
		
    nsresult result;
    return theStream->Write((const char* )buffer, offset, len, &result);
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
    TRACE("NPP_DestroyStream\n");

    if (instance == NULL)
	return NPERR_INVALID_INSTANCE_ERROR;
		
    nsIPluginStream* theStream = (nsIPluginStream*) stream->pdata;
    if( theStream == 0 )
	return NPERR_GENERIC_ERROR;
	
    theStream->Release();
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
	TRACE("NPP_StreamAsFile\n");

	if (instance == NULL)
		return;
		
	nsIPluginStream* theStream = (nsIPluginStream*) stream->pdata;
	if( theStream == 0 )
		return;

	theStream->AsFile( fname );
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// NPP_Print:
//+++++++++++++++++++++++++++++++++++++++++++++++++
void 
NPP_Print(NPP instance, NPPrint* printInfo)
{
    TRACE("NPP_Print\n");

    if(printInfo == NULL)   // trap invalid parm
        return;

	if (instance != NULL)
	{
		nsIPluginInstance* pluginInstance = (nsIPluginInstance*) instance->pdata;
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
   	TRACE("NPP_URLNotify\n");

	if( instance != NULL )
	{
	    nsIPluginInstance* pluginInstance = (nsIPluginInstance*) instance->pdata;
		pluginInstance->URLNotify(url, NULL, (nsPluginReason)reason, notifyData);

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
    TRACE("NPP_HandleEvent\n");

	int16 eventHandled = FALSE;
	if (instance == NULL)
		return eventHandled;
		
	nsIPluginInstance* pluginInstance = (nsIPluginInstance*) instance->pdata;
	if( pluginInstance )
		eventHandled = (int16) pluginInstance->HandleEvent((nsPluginEvent*) event );
	
	return eventHandled;
}
#endif // ndef XP_UNIX 
//////////////////////////////////////////////////////////////////////////////

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
}

CPluginManager::~CPluginManager(void) 
{
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// MemAlloc:
//+++++++++++++++++++++++++++++++++++++++++++++++++
void* NP_LOADDS 
CPluginManager::MemAlloc(PRUint32 size)
{
    return NPN_MemAlloc(size);
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// MemFree:
//+++++++++++++++++++++++++++++++++++++++++++++++++
void NP_LOADDS 
CPluginManager::MemFree(void* ptr)
{
    assert( ptr != NULL );

    NPN_MemFree(ptr);
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// MemFlush:
//+++++++++++++++++++++++++++++++++++++++++++++++++
PRUint32 NP_LOADDS 
CPluginManager::MemFlush(PRUint32 size)
{
#ifdef XP_MAC
	return NPN_MemFlush(size);	
#else
	return 0;
#endif
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// ReloadPlugins:
//+++++++++++++++++++++++++++++++++++++++++++++++++
void NP_LOADDS 
CPluginManager::ReloadPlugins(PRBool reloadPages)
{
	NPN_ReloadPlugins(reloadPages);
}


// (Corresponds to NPN_GetURL and NPN_GetURLNotify.)
//   notifyData: When present, URLNotify is called passing the notifyData back
//          to the client. When NULL, this call behaves like NPN_GetURL.
// New arguments:
//   peer:  A plugin instance peer. The peer's window will be used to display
//          progress information. If NULL, the load happens in the background.
//   altHost: An IP-address string that will be used instead of the host
//          specified in the URL. This is used to prevent DNS-spoofing attacks.
//          Can be defaulted to NULL meaning use the host in the URL.
//   referrer: 
//   forceJSEnabled: Forces JavaScript to be enabled for 'javascript:' URLs,
//          even if the user currently has JavaScript disabled. 
nsPluginError NP_LOADDS 
CPluginManager::GetURL(nsISupports* peer, const char* url, const char* target, void* notifyData,
					   const char* altHost, const char* referrer, PRBool forceJSEnabled)
{
   // assert( npp != NULL );
   // assert( url != NULL );
 	assert( peer != NULL);

	CPluginInstancePeer* instancePeer = (CPluginInstancePeer*)(nsIPluginInstancePeer*) peer;
	NPP npp = instancePeer->GetNPPInstance();

    
    // Call the correct GetURL* function.
    // This is determinded by checking notifyData.
    if (notifyData == NULL) {
        return (nsPluginError)NPN_GetURL(npp, url, target);
    } else {
        return (nsPluginError)NPN_GetURLNotify(npp, url, target, notifyData);
    }
}

// (Corresponds to NPN_PostURL and NPN_PostURLNotify.)
//   notifyData: When present, URLNotify is called passing the notifyData back
//          to the client. When NULL, this call behaves like NPN_GetURL.
// New arguments:
//   peer:  A plugin instance peer. The peer's window will be used to display
//          progress information. If NULL, the load happens in the background.
//   altHost: An IP-address string that will be used instead of the host
//          specified in the URL. This is used to prevent DNS-spoofing attacks.
//          Can be defaulted to NULL meaning use the host in the URL.
//   referrer: 
//   forceJSEnabled: Forces JavaScript to be enabled for 'javascript:' URLs,
//          even if the user currently has JavaScript disabled. 
//   postHeaders: A string containing post headers.
//   postHeadersLength: The length of the post headers string.
nsPluginError NP_LOADDS 
CPluginManager::PostURL(nsISupports* peer, const char* url, const char* target, PRUint32 bufLen, 
						const char* buf, PRBool file, void* notifyData, const char* altHost, 
						const char* referrer, PRBool forceJSEnabled, PRUint32 postHeadersLength, 
						const char* postHeaders) 
{
	assert( peer != NULL);

	CPluginInstancePeer* instancePeer = (CPluginInstancePeer*)(nsIPluginInstancePeer*)peer;
	NPP npp = instancePeer->GetNPPInstance();

    // Call the correct PostURL* function.
    // This is determinded by checking notifyData.
    if (notifyData == NULL) {
        return (nsPluginError)NPN_PostURL(npp, url, target, bufLen, buf, file);
    } else {
        return (nsPluginError)NPN_PostURLNotify(npp, url, target, bufLen, buf, file, notifyData);
    }
}


//+++++++++++++++++++++++++++++++++++++++++++++++++
// UserAgent:
//+++++++++++++++++++++++++++++++++++++++++++++++++
const char* NP_LOADDS 
CPluginManager::UserAgent(void)
{
    return NPN_UserAgent(NULL);
}


//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetValue:
//+++++++++++++++++++++++++++++++++++++++++++++++++
nsPluginError NP_LOADDS 
CPluginManager::GetValue(nsPluginManagerVariable variable, void *value)
{
    // XXX - This may need to be stubbed out for
    // XXX - other platforms than Unix.
    // XXX - Change this to return NPPPlugin_Error;
#ifdef XP_UNIX
    return (nsPluginError)NPN_GetValue(NULL, (NPNVariable)variable, value);
#else
    // XXX - Need to check this on the new API.
    return nsPluginError_NoData;
#endif // XP_UNIX
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// SetValue:
//+++++++++++++++++++++++++++++++++++++++++++++++++
nsPluginError NP_LOADDS 
CPluginManager::SetValue(nsPluginManagerVariable variable, void *value) 
{
    // XXX - This should do something like this.
    // XXX - return (nsPluginError)npn_setvalue(npp, (NPPVariable)variable, value);
    // XXX - Is this XP in 4.0x?
    // XXX - Need to check this on the new API.
    return nsPluginError_NoData;    
}




//+++++++++++++++++++++++++++++++++++++++++++++++++
// nsISupports functions
//+++++++++++++++++++++++++++++++++++++++++++++++++
NS_IMPL_ADDREF(CPluginManager);
NS_IMPL_RELEASE(CPluginManager);
//NS_IMPL_QUERY_INTERFACE(CPluginManager, kPluginManagerIID);
nsresult CPluginManager::QueryInterface(const nsIID& iid, void** ptr) 
{                                                                        
  if (NULL == ptr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  
  if (iid.Equals(kPluginManagerIID)) {
      *ptr = (void*) this;                                        
      AddRef();                                                            
      return NS_OK;                                                        
  }                                                                      
  if (iid.Equals(kISupportsIID)) {                                      
      *ptr = (void*) ((nsISupports*)this);                        
      AddRef();                                                            
      return NS_OK;                                                        
  }                                                                      
  return NS_NOINTERFACE;                                                 
}


//////////////////////////////////////////////////////////////////////////////
//
// CPluginInstancePeer
//

CPluginInstancePeer::CPluginInstancePeer(NPP npp,
                                         nsMIMEType typeString, 
                                         nsPluginType type,
                                         PRUint16 attr_cnt, 
                                         const char** attr_list, 
                                         const char** val_list)
    : npp(npp), typeString(typeString), type(type), attribute_cnt(attr_cnt),
    attribute_list(NULL), values_list(NULL)
{
    // Set the reference count to 0.
    NS_INIT_REFCNT();

	attribute_list = (char**) NPN_MemAlloc(attr_cnt * sizeof(const char*));
	values_list = (char**) NPN_MemAlloc(attr_cnt * sizeof(const char*));

	for (int i=0; i < attribute_cnt; i++)   {
		attribute_list[i] = (char*) NPN_MemAlloc(sizeof(char) + strlen(attr_list[i] + 1));
		values_list[i] = (char*) NPN_MemAlloc(sizeof(char) + strlen(val_list[i] + 1));

		strcpy(attribute_list[i], attr_list[i]);
		strcpy(values_list[i], val_list[i]);
	}
}

CPluginInstancePeer::~CPluginInstancePeer(void) 
{
	for (int i=0; i < attribute_cnt; i++)   {
		NPN_MemFree(attribute_list[i]);
		NPN_MemFree(values_list[i]);
	}

	NPN_MemFree(attribute_list);
	NPN_MemFree(values_list);
}   


//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetMIMEType:
// Corresponds to NPP_New's MIMEType argument.
//+++++++++++++++++++++++++++++++++++++++++++++++++
nsMIMEType NP_LOADDS 
CPluginInstancePeer::GetMIMEType(void) 
{
    return typeString;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetMode:
// Corresponds to NPP_New's mode argument.
//+++++++++++++++++++++++++++++++++++++++++++++++++
nsPluginType NP_LOADDS 
CPluginInstancePeer::GetMode(void)
{
    return type;
}


// Get a ptr to the paired list of attribute names and values,
// returns the length of the array.
//
// Each name or value is a null-terminated string.
nsPluginError NP_LOADDS 
CPluginInstancePeer::GetAttributes(PRUint16& n, const char* const*& names, const char* const*& values)  
{
	n = attribute_cnt;
	names = attribute_list;
	values = values_list;

	return nsPluginError_NoError;
}


// Get the value for the named attribute.  Returns null
// if the attribute was not set.
const char* NP_LOADDS 
CPluginInstancePeer::GetAttribute(const char* name) 
{
	for (int i=0; i < attribute_cnt; i++)  {
#ifdef XP_UNIX
		if (strcasecmp(name, attribute_list[i]) == 0)
#else
		if (stricmp(name, attribute_list[i]) == 0)
#endif
			return values_list[i];
	}

	return NULL;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// Version:
//
// XXX - Where did this go in the new API?
//
//+++++++++++++++++++++++++++++++++++++++++++++++++
void NP_LOADDS 
CPluginInstancePeer::Version(int* plugin_major, int* plugin_minor,
                                   int* netscape_major, int* netscape_minor)
{
    NPN_Version(plugin_major, plugin_minor, netscape_major, netscape_minor);
}


//+++++++++++++++++++++++++++++++++++++++++++++++++
// NewStream:
//+++++++++++++++++++++++++++++++++++++++++++++++++
nsPluginError NP_LOADDS 
CPluginInstancePeer::NewStream(nsMIMEType type, const char* target, 
                                     nsIOutputStream* *result)
{
    assert( npp != NULL );
    
    // Create a new NPStream.
    NPStream* ptr = NULL;
    nsPluginError error = (nsPluginError)NPN_NewStream(npp, (NPMIMEType)type, target, &ptr);
    if (error) return error;
    
    // Create a new Plugin Manager Stream.
    // XXX - Do we have to Release() the manager stream before doing this?
    // XXX - See the BAM doc for more info.
    CPluginManagerStream* mstream = new CPluginManagerStream(npp, ptr);
    if (mstream == NULL) return nsPluginError_OutOfMemoryError;
    mstream->AddRef();
    *result = (nsIOutputStream* )mstream;

    return nsPluginError_NoError;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// ShowStatus:
//+++++++++++++++++++++++++++++++++++++++++++++++++
void NP_LOADDS 
CPluginInstancePeer::ShowStatus(const char* message)
{
    assert( message != NULL );

    NPN_Status(npp, message);
}


//+++++++++++++++++++++++++++++++++++++++++++++++++
// nsISupports functions
//+++++++++++++++++++++++++++++++++++++++++++++++++
NS_IMPL_ADDREF(CPluginInstancePeer);
NS_IMPL_RELEASE(CPluginInstancePeer);


nsresult CPluginInstancePeer::QueryInterface(const nsIID& iid, void** ptr) 
{                                                                        
  if (NULL == ptr) {                                            
    return NS_ERROR_NULL_POINTER;                                        
  }                                                                      
  
  if (iid.Equals(kPluginInstancePeerIID)) {
      *ptr = (void*) this;                                        
      AddRef();                                                            
      return NS_OK;                                                        
  }                                                                      
  if (iid.Equals(kPluginTagInfoIID) || iid.Equals(kISupportsIID)) {                                      
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
PRInt32 NP_LOADDS 
CPluginManagerStream::Write(const char* buffer, PRInt32 offset, PRInt32 len, nsresult* error)
{
    assert( npp != NULL );
    assert( pstream != NULL );

    return NPN_Write(npp, pstream, len, (void* )buffer);
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetURL:
//+++++++++++++++++++++++++++++++++++++++++++++++++
const char* NP_LOADDS 
CPluginManagerStream::GetURL(void)
{
    assert( pstream != NULL );

    return pstream->url;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetEnd:
//+++++++++++++++++++++++++++++++++++++++++++++++++
PRUint32 NP_LOADDS 
CPluginManagerStream::GetEnd(void)
{
    assert( pstream != NULL );

    return pstream->end;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetLastModified:
//+++++++++++++++++++++++++++++++++++++++++++++++++
PRUint32 NP_LOADDS 
CPluginManagerStream::GetLastModified(void)
{
    assert( pstream != NULL );

    return pstream->lastmodified;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetNotifyData:
//+++++++++++++++++++++++++++++++++++++++++++++++++
void* NP_LOADDS 
CPluginManagerStream::GetNotifyData(void)
{
    assert( pstream != NULL );

    return pstream->notifyData;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetNotifyData:
//+++++++++++++++++++++++++++++++++++++++++++++++++
nsresult NP_LOADDS 
CPluginManagerStream::Close(void)
{
    assert( pstream != NULL );

    return NS_OK;
}


//+++++++++++++++++++++++++++++++++++++++++++++++++
// nsISupports functions
//+++++++++++++++++++++++++++++++++++++++++++++++++
NS_IMPL_ADDREF(CPluginManagerStream);
//NS_IMPL_RELEASE(CPluginManagerStream);
nsrefcnt CPluginManagerStream::Release(void)     
{                  
    if (--mRefCnt == 0) {                                
        delete this;                                       
        return 0;                                          
    }                                                    
    return mRefCnt;                                      
}
NS_IMPL_QUERY_INTERFACE(CPluginManagerStream, kOutputStreamIID);

//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
//
// CPluginStreamPeer
//

CPluginStreamPeer::CPluginStreamPeer(nsMIMEType type, NPStream* npStream,
											   PRBool seekable, PRUint16* stype)
	: type(type), npStream(npStream), seekable(seekable),
	  stype(stype), reason(nsPluginReason_NoReason)
{
    // Set the reference count to 0.
    NS_INIT_REFCNT();
}

CPluginStreamPeer::~CPluginStreamPeer(void)
{
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetURL:
//+++++++++++++++++++++++++++++++++++++++++++++++++
const char* CPluginStreamPeer::GetURL(void)
{
    return npStream->url;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetEnd:
//+++++++++++++++++++++++++++++++++++++++++++++++++
PRUint32 CPluginStreamPeer::GetEnd(void)
{
    return npStream->end;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetLastModified:
//+++++++++++++++++++++++++++++++++++++++++++++++++
PRUint32 CPluginStreamPeer::GetLastModified(void)
{
    return npStream->lastmodified;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetNotifyData:
//+++++++++++++++++++++++++++++++++++++++++++++++++
void* CPluginStreamPeer::GetNotifyData(void)
{
    return npStream->notifyData;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetReason:
//+++++++++++++++++++++++++++++++++++++++++++++++++
nsPluginReason CPluginStreamPeer::GetReason(void)
{
    return reason;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// GetMIMEType:
//+++++++++++++++++++++++++++++++++++++++++++++++++
nsMIMEType NP_LOADDS 
CPluginStreamPeer::GetMIMEType(void)
{
    return type;
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// RequestRead:
//+++++++++++++++++++++++++++++++++++++++++++++++++
nsPluginError NP_LOADDS 
CPluginStreamPeer::RequestRead(nsByteRange* rangeList)
{
    return (nsPluginError)NPN_RequestRead(npStream, (NPByteRange* )rangeList);
}

//+++++++++++++++++++++++++++++++++++++++++++++++++
// nsISupports functions
//+++++++++++++++++++++++++++++++++++++++++++++++++
NS_IMPL_ADDREF(CPluginStreamPeer);
NS_IMPL_RELEASE(CPluginStreamPeer);
//NS_IMPL_QUERY_INTERFACE(CPluginStreamPeer, kSeekablePluginStreamPeerIID);
nsresult CPluginStreamPeer::QueryInterface(const nsIID& iid, void** ptr) 
{
    if (NULL == ptr) {
        return NS_ERROR_NULL_POINTER; 
    } 
    if (iid.Equals(kSeekablePluginStreamPeerIID))  {
        *ptr = (void*) ((nsISeekablePluginStreamPeer*)this); 
        AddRef(); 
        return NS_OK; 
	} else if (iid.Equals(kPluginStreamPeerIID) ||
			   iid.Equals(kISupportsIID)) {
        *ptr = (void*) ((nsIPluginStreamPeer*)this); 
        AddRef(); 
        return NS_OK; 
    } 
    return NS_NOINTERFACE; 
} 

//////////////////////////////////////////////////////////////////////////////

#if defined(__cplusplus)
} /* extern "C" */
#endif

