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

#include "nsPluginInstancePeer.h"
#include "nsPluginTagInfo.h"
#include "nsPluginManagerStream.h"
#include "nsIPluginInstance.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kPluginInstancePeerCID, NS_PLUGININSTANCEPEER_CID);
static NS_DEFINE_IID(kIPluginInstancePeerIID, NS_IPLUGININSTANCEPEER_IID); 
static NS_DEFINE_IID(kILiveConnectPluginInstancePeerIID, NS_ILIVECONNECTPLUGININSTANCEPEER_IID);
static NS_DEFINE_IID(kIWindowlessPluginInstancePeerIID, NS_IWINDOWLESSPLUGININSTANCEPEER_IID);

////////////////////////////////////////////////////////////////////////////////
// Plugin Instance Peer Interface

nsPluginInstancePeer::nsPluginInstancePeer(NPP npp)
    : fNPP(npp), fPluginInst(NULL), fTagInfo(NULL)
{
//    NS_INIT_AGGREGATED(NULL);
    NS_INIT_REFCNT();
    fTagInfo = new nsPluginTagInfo(npp);
    fTagInfo->AddRef();
}

nsPluginInstancePeer::~nsPluginInstancePeer(void)
{
    if (fTagInfo != NULL) {
        fTagInfo->Release();
        fTagInfo = NULL;
    }
}

NS_IMPL_ADDREF(nsPluginInstancePeer);
NS_IMPL_RELEASE(nsPluginInstancePeer);

NS_METHOD
nsPluginInstancePeer::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
    if (NULL == aInstancePtr) {                                            
        return NS_ERROR_NULL_POINTER;                                        
    }                                                                      
    if (aIID.Equals(kIPluginInstancePeerIID) ||
        aIID.Equals(kPluginInstancePeerCID) ||
        aIID.Equals(kISupportsIID)) {
        // *aInstancePtr = (void*) (nsISupports*) (nsIPluginInstancePeer*)this; 
        *aInstancePtr = (nsIPluginInstancePeer*) this;
        AddRef();
        return NS_OK;
    }
    // beard:  check for interfaces that aren't on the left edge of the inheritance graph.
    // this is required so that the proper offsets are applied to this, and so the proper
    // vtable is used.
    if (aIID.Equals(kILiveConnectPluginInstancePeerIID)) {
        *aInstancePtr = (nsILiveConnectPluginInstancePeer*) this;
        AddRef();
        return NS_OK;
    }
    if (aIID.Equals(kIWindowlessPluginInstancePeerIID)) {
        *aInstancePtr = (nsIWindowlessPluginInstancePeer*) this;
        AddRef();
        return NS_OK;
    }
    return fTagInfo->QueryInterface(aIID, aInstancePtr);
}

NS_METHOD
nsPluginInstancePeer::GetValue(nsPluginInstancePeerVariable variable, void *value)
{
    NPError err = npn_getvalue(fNPP, (NPNVariable)variable, value);
    return fromNPError[err];
}

#if 0
NS_METHOD
nsPluginInstancePeer::SetValue(nsPluginInstancePeerVariable variable, void *value)
{
    NPError err = npn_setvalue(fNPP, (NPPVariable)variable, value);
    return fromNPError[err];
}
#endif

NS_METHOD
nsPluginInstancePeer::GetMIMEType(nsMIMEType *result)
{
    np_instance* instance = (np_instance*)fNPP->ndata;
    *result = instance->typeString;
    return NS_OK;
}

NS_METHOD
nsPluginInstancePeer::GetMode(nsPluginMode *result)
{
    np_instance* instance = (np_instance*)fNPP->ndata;
    *result = (nsPluginMode)instance->type;
    return NS_OK;
}

NS_METHOD
nsPluginInstancePeer::NewStream(nsMIMEType type, const char* target, nsIOutputStream* *result)
{
    NPStream* pstream;
    NPError err = npn_newstream(fNPP, (char*)type, (char*)target, &pstream);
    if (err != NPERR_NO_ERROR)
        return err;
    *result = new nsPluginManagerStream(fNPP, pstream);
    return NS_OK;
}

NS_METHOD
nsPluginInstancePeer::ShowStatus(const char* message)
{
    npn_status(fNPP, message);
    return NS_OK;
}

NS_METHOD
nsPluginInstancePeer::SetWindowSize(PRUint32 width, PRUint32 height)
{
    NPError err;
    NPSize size;
    size.width = width;
    size.height = height;
    err = npn_SetWindowSize((np_instance*)fNPP->ndata, &size);
    return fromNPError[err];
}

NS_METHOD
nsPluginInstancePeer::InvalidateRect(nsPluginRect *invalidRect)
{
    npn_invalidaterect(fNPP, (NPRect*)invalidRect);
    return NS_OK;
}

NS_METHOD
nsPluginInstancePeer::InvalidateRegion(nsPluginRegion invalidRegion)
{
    npn_invalidateregion(fNPP, invalidRegion);
    return NS_OK;
}

NS_METHOD
nsPluginInstancePeer::ForceRedraw(void)
{
    npn_forceredraw(fNPP);
    return NS_OK;
}

NS_METHOD
nsPluginInstancePeer::GetJavaPeer(jref *peer)
{
    *peer = npn_getJavaPeer(fNPP);
    return NS_OK;
}

void nsPluginInstancePeer::SetPluginInstance(nsIPluginInstance* inst)
{
    // We're now maintaining our reference to plugin instance in the
    // npp->ndata->sdata (saved data) field, and we access the peer 
    // from there. This method should be totally unnecessary.
#if 0
    if (fPluginInst != NULL) {
        fPluginInst->Release();
    }
#endif
    fPluginInst = inst;
#if 0
    if (fPluginInst != NULL) {
	    fPluginInst->AddRef();
    }
#endif
}

nsIPluginInstance* nsPluginInstancePeer::GetPluginInstance(void)
{
    if (fPluginInst != NULL) {
        fPluginInst->AddRef();
        return fPluginInst;
    }
    return NULL;
}

NPP
nsPluginInstancePeer::GetNPP(void)
{
    return fNPP;
}

JSContext *
nsPluginInstancePeer::GetJSContext(void)
{
    MWContext *pMWCX = GetMWContext();
    JSContext *pJSCX = NULL;
    if (!pMWCX || (pMWCX->type != MWContextBrowser && pMWCX->type != MWContextPane))
    {
       return 0;
    }
    if( pMWCX->mocha_context != NULL)
    {
      pJSCX = pMWCX->mocha_context;
    }
    else
    {
       pJSCX = LM_GetCrippledContext();
    }
    return pJSCX;
}

MWContext *
nsPluginInstancePeer::GetMWContext(void)
{
    np_instance* instance = (np_instance*) fNPP->ndata;
    MWContext *pMWCX = instance->cx;

    return pMWCX;
}

////////////////////////////////////////////////////////////////////////////////
