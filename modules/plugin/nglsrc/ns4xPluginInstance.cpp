/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "ns4xPluginInstance.h"

#ifdef NEW_PLUGIN_STREAM_API
#include "nsIPluginStreamListener.h"
#else
#include "ns4xPluginStream.h"
#endif

#include "prlog.h"

////////////////////////////////////////////////////////////////////////
//
////////////////////////////////////////////////////////////////////////

#ifdef NEW_PLUGIN_STREAM_API

class ns4xPluginStreamListener : public nsIPluginStreamListener {

public:

    NS_DECL_ISUPPORTS

    ////////////////////////////////////////////////////////////////////////////
    // from nsIPluginStreamListener:

    NS_IMETHOD
    OnStartBinding(const char* url, nsIPluginStreamInfo* pluginInfo);

    NS_IMETHOD
    OnDataAvailable(const char* url, nsIInputStream* input,
                    PRUint32 offset, PRUint32 length, nsIPluginStreamInfo* pluginInfo);

    NS_IMETHOD
    OnFileAvailable(const char* url, const char* fileName);

    NS_IMETHOD
    OnStopBinding(const char* url, nsresult status, nsIPluginStreamInfo* pluginInfo);

	   NS_IMETHOD
    GetStreamType(nsPluginStreamType *result);

	   NS_IMETHOD
	   OnNotify(const char* url, nsresult status);

    ////////////////////////////////////////////////////////////////////////////
    // ns4xPluginStreamListener specific methods:

    ns4xPluginStreamListener(nsIPluginInstance* inst, void* notifyData);
    virtual ~ns4xPluginStreamListener(void);

protected:

    void* mNotifyData;
	   ns4xPluginInstance* mInst;
	   NPStream mNPStream;
	   PRUint32 mPosition;
	   nsPluginStreamType mStreamType;
};

////////////////////////////////////////////////////////////////////////////////
// ns4xPluginStreamListener Methods
////////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);

ns4xPluginStreamListener::ns4xPluginStreamListener(nsIPluginInstance* inst, void* notifyData)
    : mNotifyData(notifyData)
{
    NS_INIT_REFCNT();
	mInst = (ns4xPluginInstance*) inst;
	mPosition = 0;

    // Initialize the 4.x interface structure
    memset(&mNPStream, 0, sizeof(mNPStream));

	NS_IF_ADDREF(mInst);
}

ns4xPluginStreamListener::~ns4xPluginStreamListener(void)
{
	NS_IF_RELEASE(mInst);
}

NS_IMPL_ISUPPORTS(ns4xPluginStreamListener, kIPluginStreamListenerIID);

NS_IMETHODIMP
ns4xPluginStreamListener::OnStartBinding(const char* url, nsIPluginStreamInfo* pluginInfo)
{
	NPP npp;
	const NPPluginFuncs *callbacks;
	PRBool seekable;
	nsMIMEType contentType;


    mNPStream.ndata        = (void*) this;
    mNPStream.url          = url;
	mNPStream.notifyData   = mNotifyData;
 
	pluginInfo->GetLength((PRUint32*)&(mNPStream.end));
    pluginInfo->GetLastModified((PRUint32*)&(mNPStream.lastmodified));
	pluginInfo->IsSeekable(&seekable);
	pluginInfo->GetContentType(&contentType);

    mInst->GetCallbacks(&callbacks);
    mInst->GetNPP(&npp);

    nsresult error
        = (nsresult)CallNPP_NewStreamProc(callbacks->newstream,
                                          npp,
                                          (char *)contentType,
                                          &mNPStream,
                                          seekable,
                                          (PRUint16*)&mStreamType);
    return NS_OK;
}

NS_IMETHODIMP
ns4xPluginStreamListener::OnDataAvailable(const char* url, nsIInputStream* input,
                                            PRUint32 offset, PRUint32 length, nsIPluginStreamInfo* pluginInfo)
{
	const NPPluginFuncs *callbacks;
    NPP                 npp;
	PRUint32			numtowrite = 0;
	PRUint32			amountRead = 0;
	PRInt32				writeCount = 0;

	mNPStream.url = url;
    pluginInfo->GetLastModified((PRUint32*)&(mNPStream.lastmodified));

    mInst->GetCallbacks(&callbacks);
    mInst->GetNPP(&npp);

    if (callbacks->write == NULL)
        return NS_OK; // XXX ?

	// Get the data from the input stream
	char* buffer = new char[length];
    if (buffer) 
        input->Read(buffer, offset, length, &amountRead);

	// amountRead tells us how many bytes were put in the buffer
	// WriteReady returns to us how many bytes the plugin is 
	// ready to handle  - we have to keep calling WriteReady and
	// Write until the buffer is empty
    while (amountRead > 0)
    {
      if (callbacks->writeready != NULL)
      {
        numtowrite = CallNPP_WriteReadyProc(callbacks->writeready,
                                            npp,
                                            &mNPStream);

        if (numtowrite > amountRead)
          numtowrite = amountRead;
      }
      else // if WriteReady is not supported by the plugin, just write the whole buffer
        numtowrite = length;


      writeCount = CallNPP_WriteProc(callbacks->write,
                                       npp,
                                       &mNPStream, 
                                       mPosition,
                                       numtowrite,
                                       (void *)buffer);
	  if(writeCount < 0)
		  return NS_ERROR_FAILURE;

      amountRead -= numtowrite;
	  mPosition += numtowrite;
    }

    return NS_OK;
}

NS_IMETHODIMP
ns4xPluginStreamListener::OnFileAvailable(const char* url, const char* fileName)
{
    const NPPluginFuncs *callbacks;
    NPP                 npp;

	mNPStream.url = url;

    mInst->GetCallbacks(&callbacks);
    mInst->GetNPP(&npp);

    if (callbacks->asfile == NULL)
        return NS_OK;

    CallNPP_StreamAsFileProc(callbacks->asfile,
                             npp,
                             &mNPStream,
                             fileName);

    return NS_OK;
}

NS_IMETHODIMP
ns4xPluginStreamListener::OnStopBinding(const char* url, nsresult status, nsIPluginStreamInfo* pluginInfo)
{
    const NPPluginFuncs *callbacks;
    NPP                 npp;

	mNPStream.url = url;
    pluginInfo->GetLastModified((PRUint32*)&(mNPStream.lastmodified));

    mInst->GetCallbacks(&callbacks);
    mInst->GetNPP(&npp);

    if (callbacks->destroystream != NULL)
    {
        CallNPP_DestroyStreamProc(callbacks->destroystream,
                                  npp,
                                  &mNPStream,
                                  nsPluginReason_Done);
    }

    return NS_OK;
}

NS_IMETHODIMP
ns4xPluginStreamListener::GetStreamType(nsPluginStreamType *result)
{
  *result = mStreamType;
  return NS_OK;
}

NS_IMETHODIMP
ns4xPluginStreamListener::OnNotify(const char* url, nsresult status)
{
	const NPPluginFuncs *callbacks;
    NPP                 npp;

	mNPStream.url = url;

    mInst->GetCallbacks(&callbacks);
    mInst->GetNPP(&npp);

	// check to see if we have a call back
    if (callbacks->urlnotify != NULL && mNotifyData != nsnull)
    {
        CallNPP_URLNotifyProc(callbacks->urlnotify,
                              npp,
                              url,
                              nsPluginReason_Done,
                              mNotifyData);
    }

	return NS_OK;
}

#endif

ns4xPluginInstance :: ns4xPluginInstance(NPPluginFuncs* callbacks)
    : fCallbacks(callbacks)
{
    NS_INIT_REFCNT();

    NS_ASSERTION(fCallbacks != NULL, "null callbacks");

    // Initialize the NPP structure.

    fNPP.pdata = NULL;
    fNPP.ndata = this;

    fPeer = nsnull;

    mWindowless = PR_FALSE;
    mTransparent = PR_FALSE;
}


ns4xPluginInstance :: ~ns4xPluginInstance(void)
{
    NS_RELEASE(fPeer);
}

////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(ns4xPluginInstance);
NS_IMPL_RELEASE(ns4xPluginInstance);

static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID); 
static NS_DEFINE_IID(kIEventHandlerIID, NS_IEVENTHANDLER_IID); 
static NS_DEFINE_IID(kIPluginTagInfoIID, NS_IPLUGINTAGINFO_IID); 
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMETHODIMP ns4xPluginInstance :: QueryInterface(const nsIID& iid, void** instance)
{
    if (instance == NULL)
        return NS_ERROR_NULL_POINTER;

    if (iid.Equals(kIPluginInstanceIID))
    {
        *instance = (void *)(nsIPluginInstance *)this;
        AddRef();
        return NS_OK;
    }

    if (iid.Equals(kIEventHandlerIID))
    {
        *instance = (void *)(nsIEventHandler *)this;
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

NS_IMETHODIMP ns4xPluginInstance :: Initialize(nsIPluginInstancePeer* peer)
{
    PRUint16 count = 0;
    const char* const* names = nsnull;
    const char* const* values = nsnull;

    NS_ASSERTION(peer != NULL, "null peer");

    fPeer = peer;

    NS_ADDREF(fPeer);

    nsresult error;
    nsIPluginTagInfo  *taginfo = nsnull;

    error = fPeer->QueryInterface(kIPluginTagInfoIID, (void **)&taginfo);

    if (NS_OK == error)
    {
      taginfo->GetAttributes(count, names, values);
      NS_IF_RELEASE(taginfo);
    }

    if (fCallbacks->newp == NULL)
        return NS_ERROR_FAILURE; // XXX right error?
      
    // XXX Note that the NPPluginType_* enums were crafted to be
    // backward compatible...

    nsPluginMode  mode;
    nsMIMEType    mimetype;

    fPeer->GetMode(&mode);
    fPeer->GetMIMEType(&mimetype);

    error = (nsresult)
        CallNPP_NewProc(fCallbacks->newp,
                        (char *)mimetype,
                        &fNPP,
                        (PRUint16)mode,
                        count,
                        (char**)names,
                        (char**)values,
                        NULL); // saved data

    return error;
}

NS_IMETHODIMP ns4xPluginInstance :: GetPeer(nsIPluginInstancePeer* *resultingPeer)
{
  NS_ADDREF(fPeer);
  *resultingPeer = fPeer;

  return NS_OK;
}

NS_IMETHODIMP ns4xPluginInstance::Start(void)
{
    // XXX At some point, we maybe should implement start and stop to
    // load/unload the 4.x plugin, just in case there are some plugins
    // that rely on that behavior...
printf("instance start called\n");
    return NS_OK;
}

NS_IMETHODIMP ns4xPluginInstance::Stop(void)
{
printf("instance stop called\n");
    return NS_OK;
}

NS_IMETHODIMP ns4xPluginInstance::Destroy(void)
{
    nsresult error;

printf("instance destroy called\n");
    if (fCallbacks->destroy == NULL)
        return NS_ERROR_FAILURE; // XXX right error?

    NPSavedData *sdata;

    error = (nsresult)CallNPP_DestroyProc(fCallbacks->destroy,
                                          &fNPP, &sdata); // saved data

    return error;
}

NS_IMETHODIMP ns4xPluginInstance::SetWindow(nsPluginWindow* window)
{
    // XXX 4.x plugins don't want a SetWindow(NULL).

    if (window == NULL)
        return NS_OK;

    nsresult error = NS_OK;

    if (fCallbacks->setwindow)
    {
        // XXX Turns out that NPPluginWindow and NPWindow are structurally
        // identical (on purpose!), so there's no need to make a copy.

        error = (nsresult) CallNPP_SetWindowProc(fCallbacks->setwindow,
                                                 &fNPP,
                                                 (NPWindow*) window);

        // XXX In the old code, we'd just ignore any errors coming
        // back from the plugin's SetWindow(). Is this the correct
        // behavior?!?

        if (error != NS_OK)
          printf("error in setwindow %d\n", error);
    }

    return error;
}

#ifdef NEW_PLUGIN_STREAM_API

/* NOTE: the caller must free the stream listener */

NS_IMETHODIMP ns4xPluginInstance::NewStream(nsIPluginStreamListener** listener)
{
	nsISupports* stream = nsnull;
	stream = (nsISupports *)(nsIPluginStreamListener *)new ns4xPluginStreamListener(this, nsnull);

	if(stream == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	nsresult res = stream->QueryInterface(kIPluginStreamListenerIID, (void**)listener);

	// If we didn't get the right interface, clean up  
	if (res != NS_OK)
		delete stream;  

	return res;
}

nsresult ns4xPluginInstance::NewNotifyStream(nsIPluginStreamListener** listener, void* notifyData)
{
	*listener = new ns4xPluginStreamListener(this, notifyData);
	return NS_OK;
}

#else
NS_IMETHODIMP ns4xPluginInstance::NewStream(nsIPluginStreamPeer* peer, nsIPluginStream* *result)
{
    (*result) = NULL;

    ns4xPluginStream* stream = new ns4xPluginStream(); 

    if (stream == NULL)
        return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(stream);

    nsresult error;

    // does it need the peer?

    if ((error = stream->Initialize(this, peer)) != NS_OK)
    {
        NS_RELEASE(stream);
        return error;
    }

    (*result) = stream;
    return NS_OK;
}
#endif

NS_IMETHODIMP ns4xPluginInstance::Print(nsPluginPrint* platformPrint)
{
printf("instance print called\n");
  return NS_OK;
}

NS_IMETHODIMP ns4xPluginInstance::HandleEvent(nsPluginEvent* event, PRBool* handled)
{
printf("instance handleevent called\n");
    *handled = PR_FALSE;

    return NS_OK;
}

#ifndef NEW_PLUGIN_STREAM_API
NS_IMETHODIMP ns4xPluginInstance::URLNotify(const char* url, const char* target,
                                           nsPluginReason reason, void* notifyData)
{
    if (fCallbacks->urlnotify != NULL)
    {
        CallNPP_URLNotifyProc(fCallbacks->urlnotify,
                              &fNPP,
                              url,
                              reason,
                              notifyData);
    }

    return NS_OK; //XXX this seems bad...
}
#endif

NS_IMETHODIMP ns4xPluginInstance :: GetValue(nsPluginInstanceVariable variable, void *value)
{
  nsresult  rv = NS_OK;

  switch (variable)
  {
    case nsPluginInstanceVariable_WindowlessBool:
      *(PRBool *)value = mWindowless;
      break;

    case nsPluginInstanceVariable_TransparentBool:
      *(PRBool *)value = mTransparent;
      break;

    default:
      rv = NS_ERROR_FAILURE;    //XXX this is bad
  }

  return rv;
}

nsresult ns4xPluginInstance::GetNPP(NPP* aNPP) 
{
	if(aNPP != nsnull)
		*aNPP = &fNPP;
	else
		return NS_ERROR_NULL_POINTER;

    return NS_OK;
}

nsresult ns4xPluginInstance::GetCallbacks(const NPPluginFuncs ** aCallbacks)
{
	if(aCallbacks != nsnull)
		*aCallbacks = fCallbacks;
	else
		return NS_ERROR_NULL_POINTER;

	return NS_OK;
}

nsresult ns4xPluginInstance :: SetWindowless(PRBool aWindowless)
{
  mWindowless = aWindowless;
  return NS_OK;
}

nsresult ns4xPluginInstance :: SetTransparent(PRBool aTransparent)
{
  mTransparent = aTransparent;
  return NS_OK;
}
