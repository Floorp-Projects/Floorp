/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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


#include "ns4xPluginStream.h"
#include "prlog.h"             // for PR_ASSERT
#include "nsPluginSafety.h"

static NS_DEFINE_IID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);

////////////////////////////////////////////////////////////////////////


ns4xPluginStream::ns4xPluginStream(void)
    : fStreamType(nsPluginStreamType_Normal), fSeekable(PR_FALSE), fPosition(0)
{
    NS_INIT_REFCNT();

    fPeer = nsnull;
    fInstance = nsnull;

    // Initialize the 4.x interface structure
    memset(&fNPStream, 0, sizeof(fNPStream));
}


ns4xPluginStream::~ns4xPluginStream(void)
{
    const NPPluginFuncs *callbacks;
    NPP                 npp;
    nsPluginReason      reason;

    fInstance->GetCallbacks(&callbacks);
    fInstance->GetNPP(&npp);
    fPeer->GetReason(&reason);

    if (callbacks->destroystream != NULL)
    {
        PRLibrary* lib = nsnull;
        if(fInstance)
          lib = fInstance->fLibrary;

        NS_TRY_SAFE_CALL_VOID(CallNPP_DestroyStreamProc(callbacks->destroystream,
                                                        npp,
                                                        &fNPStream,
                                                        reason), lib);
    }

    NS_IF_RELEASE(fPeer);
    NS_IF_RELEASE(fInstance);
}


////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(ns4xPluginStream);
NS_IMPL_RELEASE(ns4xPluginStream);

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIPluginStreamIID, NS_IPLUGINSTREAM_IID);
static NS_DEFINE_IID(kISeekablePluginStreamPeerIID, NS_ISEEKABLEPLUGINSTREAMPEER_IID);

NS_IMETHODIMP ns4xPluginStream::QueryInterface(const nsIID& iid, void** instance)
{
    if (instance == NULL)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIPluginStreamIID))
    {
        *instance = (void *)(nsIPluginStream *)this;
        AddRef();
        return NS_OK;
    }
    else if (iid.Equals(kISupportsIID))
    {
        *instance = (void *)(nsISupports *)this;
        AddRef();
        return NS_OK;
    }

    return NS_NOINTERFACE;
}


////////////////////////////////////////////////////////////////////////


NS_IMETHODIMP ns4xPluginStream::Initialize(ns4xPluginInstance* instance,
                                          nsIPluginStreamPeer* peer)
{
    fInstance = instance;
    fPeer = peer; 

    NS_ASSERTION(fInstance != NULL, "null instance");

    NS_ADDREF(fInstance);

    NS_ASSERTION(fPeer != NULL, "null peer");

    NS_ADDREF(fPeer);

    const char *url;
    PRUint32  length, modified;

    fPeer->GetURL(&url);
    fPeer->GetEnd(&length);
    fPeer->GetLastModified(&modified);

    fNPStream.ndata        = (void*) fPeer;
    fNPStream.url          = url;
    fNPStream.end          = length;
    fNPStream.lastmodified = modified;

    // Are we seekable?

    nsISupports* seekablePeer;

    if (fPeer->QueryInterface(kISeekablePluginStreamPeerIID,
                              (void**) &seekablePeer) == NS_OK)
    {
        fSeekable = TRUE;
        NS_RELEASE(seekablePeer);
    }

    const NPPluginFuncs *callbacks;
    NPP                 npp;
    nsMIMEType          mimetype;

    fInstance->GetCallbacks(&callbacks);
    fInstance->GetNPP(&npp);
    fPeer->GetMIMEType(&mimetype);

    if (callbacks->newstream == NULL)
        return NS_ERROR_FAILURE;

    PRUint16 streamType = (PRUint16) fStreamType;

    PRLibrary* lib = nsnull;
    if(fInstance)
      lib = fInstance->fLibrary;

    NP_ERROR error = NS_OK;

    NS_TRY_SAFE_CALL_RETURN(error, CallNPP_NewStreamProc(callbacks->newstream,
                                                          npp,
                                                          (char *)mimetype,
                                                          &fNPStream,
                                                          fSeekable,
                                                          &streamType), lib);

    fStreamType = (nsPluginStreamType) streamType;

    return (nsresult)error;
}

NS_IMETHODIMP ns4xPluginStream::Write(const char* buffer, PRUint32 offset, PRUint32 len, PRUint32 *aWriteCount)
{
    const NPPluginFuncs *callbacks;
    NPP                 npp;
    PRUint32            remaining = len;

    fInstance->GetCallbacks(&callbacks);
    fInstance->GetNPP(&npp);

    if (callbacks->write == NULL)
        return NS_OK;

    while (remaining > 0)
    {
      PRUint32 numtowrite;

      if (callbacks->writeready != NULL)
      {
        PRLibrary* lib = nsnull;
        if(fInstance)
          lib = fInstance->fLibrary;

        NS_TRY_SAFE_CALL_RETURN(numtowrite, CallNPP_WriteReadyProc(callbacks->writeready,
                                                                    npp,
                                                                    &fNPStream), lib);

        if (numtowrite > remaining)
          numtowrite = remaining;
      }
      else
        numtowrite = (int32)len;

      PRLibrary* lib = nsnull;
      if(fInstance)
        lib = fInstance->fLibrary;

      NS_TRY_SAFE_CALL_RETURN(*aWriteCount, CallNPP_WriteProc(callbacks->write,
                                                               npp,
                                                               &fNPStream, 
                                                               fPosition,
                                                               numtowrite,
                                                               (void *)buffer), lib);

      remaining -= numtowrite;
    }

    fPosition += len;

    return NS_OK;   //XXX this seems bad...
}

NS_IMETHODIMP ns4xPluginStream::GetStreamType(nsPluginStreamType *result)
{
    *result = fStreamType;
    return NS_OK;
}

NS_IMETHODIMP ns4xPluginStream::AsFile(const char* filename)
{
    const NPPluginFuncs *callbacks;
    NPP                 npp;

    fInstance->GetCallbacks(&callbacks);
    fInstance->GetNPP(&npp);

    if (callbacks->asfile == NULL)
        return NS_OK;

    PRLibrary* lib = nsnull;
    if(fInstance)
      lib = fInstance->fLibrary;

    NS_TRY_SAFE_CALL_VOID(CallNPP_StreamAsFileProc(callbacks->asfile,
                                                   npp,
                                                   &fNPStream,
                                                   filename), lib);

    return NS_OK;
}

NS_IMETHODIMP ns4xPluginStream :: Close(void)
{
   return NS_OK;
}

////////////////////////////////////////////////////////////////////////

