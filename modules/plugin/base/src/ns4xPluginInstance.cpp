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

#include "ns4xPluginInstance.h"
#include "nsIPluginStreamListener.h"

#include "prlog.h"
#include "prmem.h"
#include "nscore.h"

#ifdef XP_UNIX
#include "gtkxtbin.h"
#include "gdksuperwin.h"
#include "gtkmozbox.h"
#endif

class ns4xPluginStreamListener : public nsIPluginStreamListener {

public:

    NS_DECL_ISUPPORTS

    ///////////////////////////////////////////////////////////////////////////
    // from nsIPluginStreamListener:

    NS_IMETHOD
    OnStartBinding(nsIPluginStreamInfo* pluginInfo);

    NS_IMETHOD
    OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, 
                    PRUint32 length);

    NS_IMETHOD
    OnFileAvailable( nsIPluginStreamInfo* pluginInfo, const char* fileName);

    NS_IMETHOD
    OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status);

	   NS_IMETHOD
    GetStreamType(nsPluginStreamType *result);

    ///////////////////////////////////////////////////////////////////////////
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


///////////////////////////////////////////////////////////////////////////////
// ns4xPluginStreamListener Methods
///////////////////////////////////////////////////////////////////////////////

static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);

ns4xPluginStreamListener::ns4xPluginStreamListener(nsIPluginInstance* inst, 
                                                   void* notifyData)
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
ns4xPluginStreamListener::OnStartBinding(nsIPluginStreamInfo* pluginInfo)
{

	NPP npp;
	const NPPluginFuncs *callbacks;
	PRBool seekable;
	nsMIMEType contentType;
	PRUint16 streamType = NP_NORMAL;
	NPError error;

    mNPStream.ndata        = (void*) this;
    pluginInfo->GetURL(&mNPStream.url);
	mNPStream.notifyData   = mNotifyData;
 
	pluginInfo->GetLength((PRUint32*)&(mNPStream.end));
    pluginInfo->GetLastModified((PRUint32*)&(mNPStream.lastmodified));
	pluginInfo->IsSeekable(&seekable);
	pluginInfo->GetContentType(&contentType);

    mInst->GetCallbacks(&callbacks);
    mInst->GetNPP(&npp);

#if !TARGET_CARBON
// pinkerton
// relies on routine descriptors, not present in carbon. We need to fix this.
    error = CallNPP_NewStreamProc(callbacks->newstream,
                                          npp,
                                          (char *)contentType,
                                          &mNPStream,
                                          seekable,
                                          &streamType);
	if(error != NPERR_NO_ERROR)
		return NS_ERROR_FAILURE;
#endif
	// translate the old 4x style stream type to the new one
	switch(streamType)
	{
		case NP_NORMAL : mStreamType = nsPluginStreamType_Normal; break;

		case NP_ASFILEONLY : mStreamType = nsPluginStreamType_AsFileOnly; 
          break;

		case NP_ASFILE : mStreamType = nsPluginStreamType_AsFile; break;

		case NP_SEEK : mStreamType = nsPluginStreamType_Seek; break;

		default: return NS_ERROR_FAILURE;
	}

	
    return NS_OK;
}

NS_IMETHODIMP
ns4xPluginStreamListener::OnDataAvailable(nsIPluginStreamInfo* pluginInfo,
										  nsIInputStream* input,
                                          PRUint32 length)
{
	const NPPluginFuncs *callbacks;
    NPP                 npp;
	PRUint32			numtowrite = 0;
	PRUint32			amountRead = 0;
	PRInt32				writeCount = 0;

    pluginInfo->GetURL(&mNPStream.url);
    pluginInfo->GetLastModified((PRUint32*)&(mNPStream.lastmodified));

    mInst->GetCallbacks(&callbacks);
    mInst->GetNPP(&npp);

    if (callbacks->write == NULL || length == 0)
        return NS_OK; // XXX ?

	// Get the data from the input stream
	char* buffer = (char*) PR_Malloc(length);
    if (buffer) 
        input->Read(buffer, length, &amountRead);

	// amountRead tells us how many bytes were put in the buffer
	// WriteReady returns to us how many bytes the plugin is 
	// ready to handle  - we have to keep calling WriteReady and
	// Write until the buffer is empty
    while (amountRead > 0)
    {
      if (callbacks->writeready != NULL)
        {
#if !TARGET_CARBON
          // pinkerton
          // relies on routine descriptors, not present in carbon. 
          // We need to fix this.
          numtowrite = CallNPP_WriteReadyProc(callbacks->writeready,
                                              npp,
                                              &mNPStream);
#endif
		    
		    if (numtowrite > amountRead)
			    numtowrite = amountRead;
		  }
      else 
        // if WriteReady is not supported by the plugin, 
        // just write the whole buffer
        numtowrite = length;

      // if WriteReady returned 0, the plugin is not ready to handle 
      // the data, so just skip the Write until WriteReady returns a >0 value
      if(numtowrite > 0)
        {
#if !TARGET_CARBON
          // pinkerton
          // relies on routine descriptors, not present in carbon. 
          // We need to fix this.
          writeCount = CallNPP_WriteProc(callbacks->write,
                                         npp,
                                         &mNPStream, 
                                         mPosition,
                                         numtowrite,
                                         (void *)buffer);
          if(writeCount < 0)
            return NS_ERROR_FAILURE;
#endif
          amountRead -= numtowrite;
          mPosition += numtowrite;
        }
    }
    
    return NS_OK;
}

NS_IMETHODIMP
ns4xPluginStreamListener::OnFileAvailable(nsIPluginStreamInfo* pluginInfo, 
                                          const char* fileName)
{
    const NPPluginFuncs *callbacks;
    NPP                 npp;

    pluginInfo->GetURL(&mNPStream.url);

    mInst->GetCallbacks(&callbacks);
    mInst->GetNPP(&npp);

    if (callbacks->asfile == NULL)
        return NS_OK;

#if !TARGET_CARBON
    // pinkerton
    // relies on routine descriptors, not present in carbon. 
    // We need to fix this.
    CallNPP_StreamAsFileProc(callbacks->asfile,
                             npp,
                             &mNPStream,
                             fileName);
#endif
 
    return NS_OK;
}

NS_IMETHODIMP
ns4xPluginStreamListener::OnStopBinding(nsIPluginStreamInfo* pluginInfo, 
                                        nsresult status)
{
    const NPPluginFuncs *callbacks;
    NPP                 npp;
	NPError error;

    pluginInfo->GetURL(&mNPStream.url);
    pluginInfo->GetLastModified((PRUint32*)&(mNPStream.lastmodified));

    mInst->GetCallbacks(&callbacks);
    mInst->GetNPP(&npp);

    if (callbacks->destroystream != NULL)
    {
      // XXX need to convert status to NPReason
#if !TARGET_CARBON
      // pinkerton
      // relies on routine descriptors, not present in carbon. 
      // We need to fix this.
      error = CallNPP_DestroyStreamProc(callbacks->destroystream,
                                        npp,
                                        &mNPStream,
                                        NPRES_DONE);
      if(error != NPERR_NO_ERROR)
        return NS_ERROR_FAILURE;
#endif
    }

	// check to see if we have a call back
    if (callbacks->urlnotify != NULL && mNotifyData != nsnull)
    {
#if !TARGET_CARBON
      // pinkerton
      // relies on routine descriptors, not present in carbon. 
      // We need to fix this.
      CallNPP_URLNotifyProc(callbacks->urlnotify,
                            npp,
                            mNPStream.url,
                            nsPluginReason_Done,
                            mNotifyData);
#endif
    }
    


    return NS_OK;
}

NS_IMETHODIMP
ns4xPluginStreamListener::GetStreamType(nsPluginStreamType *result)
{
  *result = mStreamType;
  return NS_OK;
}

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
    mStarted = PR_FALSE;
}


ns4xPluginInstance :: ~ns4xPluginInstance(void)
{
    NS_RELEASE(fPeer);
}


////////////////////////////////////////////////////////////////////////

NS_IMPL_ADDREF(ns4xPluginInstance);
NS_IMPL_RELEASE(ns4xPluginInstance);

static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID); 
static NS_DEFINE_IID(kIPluginTagInfoIID, NS_IPLUGINTAGINFO_IID); 
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

NS_IMETHODIMP ns4xPluginInstance :: QueryInterface(const nsIID& iid, 
                                                   void** instance)
{
  if (instance == NULL)
    return NS_ERROR_NULL_POINTER;
  
  if (iid.Equals(kIPluginInstanceIID))
    {
      *instance = (void *)(nsIPluginInstance *)this;
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

NS_IMETHODIMP ns4xPluginInstance::Initialize(nsIPluginInstancePeer* peer)
{
#ifdef XP_UNIX
  xtbin = nsnull;
#endif
  return InitializePlugin(peer);
}

NS_IMETHODIMP ns4xPluginInstance::GetPeer(nsIPluginInstancePeer* *resultingPeer)
{
  NS_ADDREF(fPeer);
  *resultingPeer = fPeer;
  
  return NS_OK;
}

NS_IMETHODIMP ns4xPluginInstance::Start(void)
{
#ifdef NS_DEBUG
  printf("Inside ns4xPluginInstance::Start(void)...\n");
#endif

  if(mStarted)
    return NS_OK;
  else
    return InitializePlugin(fPeer); 
}

NS_IMETHODIMP ns4xPluginInstance::Stop(void)
{
  NPError error;

  if (fCallbacks->destroy == NULL)
    return NS_ERROR_FAILURE; // XXX right error?

  NPSavedData *sdata;

#if !TARGET_CARBON
  // pinkerton
  // relies on routine descriptors, not present in carbon. 
  // We need to fix this.
  error = (nsresult)CallNPP_DestroyProc(fCallbacks->destroy,
                                        &fNPP, &sdata); // saved data
#endif

  mStarted = PR_FALSE;
  if(error != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}

nsresult ns4xPluginInstance::InitializePlugin(nsIPluginInstancePeer* peer)
{
  PRUint16 count = 0;
  const char* const* names = nsnull;
  const char* const* values = nsnull;
  nsresult rv;
  NPError error;
  nsIPluginTagInfo* taginfo;
  
  NS_ASSERTION(peer != NULL, "null peer");

  fPeer = peer;
  NS_ADDREF(fPeer);

  rv = fPeer->QueryInterface(kIPluginTagInfoIID, (void **)&taginfo);

  if (NS_OK == rv)
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

#if !TARGET_CARBON
  // pinkerton
  // relies on routine descriptors, not present in carbon. We need to fix this.
  error = CallNPP_NewProc(fCallbacks->newp,
                          (char *)mimetype,
                          &fNPP,
                          (PRUint16)mode,
                          count,
                          (char**)names,
                          (char**)values,
                          NULL); // saved data
#endif
  
  if(error != NPERR_NO_ERROR)
    rv = NS_ERROR_FAILURE;
  
  mStarted = PR_TRUE;
  
  return rv;
}

NS_IMETHODIMP ns4xPluginInstance::Destroy(void)
{
  // destruction is handled in the Stop call
  return NS_OK;
}

NS_IMETHODIMP ns4xPluginInstance::SetWindow(nsPluginWindow* window)
{
#ifdef XP_UNIX 
  GdkSuperWin               *superwin;
  NPSetWindowCallbackStruct *ws;
#endif

  // XXX 4.x plugins don't want a SetWindow(NULL).

#ifdef NS_DEBUG
  printf("Inside ns4xPluginInstance::SetWindow(%p)...\n", window);
#endif

  if (window == NULL || mStarted == PR_FALSE)
    return NS_OK;
  
  NPError error;
  
#ifdef XP_UNIX  
  // Allocate and fill out the ws_info data
  if (!window->ws_info) {
#ifdef NS_DEBUG
    printf("About to create new ws_info...\n");
#endif    

    // allocate a new NPSetWindowCallbackStruct structure at ws_info
    window->ws_info = (NPSetWindowCallbackStruct *)PR_MALLOC(sizeof(NPSetWindowCallbackStruct));
    
    if (!window->ws_info)
      return NS_ERROR_OUT_OF_MEMORY;

    superwin = (GdkSuperWin *)(window->window);
    ws = (NPSetWindowCallbackStruct *)window->ws_info;

    if (superwin->bin_window) {
#ifdef NS_DEBUG      
      printf("About to create new xtbin of %i X %i from %p...\n",
             window->width, window->height, superwin->bin_window);
#endif

      // Initialize GtkXtBin widget to encapsulate
      // the Xt toolkit within this Gtk Application
      xtbin = gtk_xtbin_new(superwin->bin_window, 0);
      gtk_widget_set_usize(xtbin, window->width, window->height);

      printf("About to show xtbin(%p)...\n", xtbin); fflush(NULL);
      gtk_widget_show(xtbin);
      printf("completed gtk_widget_show(%p)\n", xtbin); fflush(NULL);
    }

    // fill in window info structure 
    ws->type = 0; // OK, that was a guess!!
    ws->depth = gdk_visual_get_best_depth();
    ws->display = GTK_XTBIN(xtbin)->xtdisplay;
    ws->visual = GDK_VISUAL_XVISUAL(gdk_rgb_get_visual());
    ws->colormap = GDK_COLORMAP_XCOLORMAP(gdk_window_get_colormap(superwin->bin_window));

    XFlush(ws->display);
  } // !window->ws_info

  // And now point the NPWindow structures window 
  // to the actual X window
  window->window = (nsPluginPort *)GTK_XTBIN(xtbin)->xtwindow;
#endif // XP_UNIX

  if (fCallbacks->setwindow) {
    // XXX Turns out that NPPluginWindow and NPWindow are structurally
    // identical (on purpose!), so there's no need to make a copy.
      
#if !TARGET_CARBON
    // pinkerton
    // relies on routine descriptors, not present in carbon. We 
    // need to fix this.
#ifdef NS_DEBUG
    printf("About to call CallNPP_SetWindowProc()...\n");
    fflush(NULL);
#endif

    error = CallNPP_SetWindowProc(fCallbacks->setwindow,
                                  &fNPP,
                                  (NPWindow*) window);
#endif
      
    // XXX In the old code, we'd just ignore any errors coming
    // back from the plugin's SetWindow(). Is this the correct
    // behavior?!?
  }
  
#ifdef NS_DEBUG
  printf("Falling out of ns4xPluginInstance::SetWindow()...\n");
#endif
  return NS_OK;
}

/* NOTE: the caller must free the stream listener */

NS_IMETHODIMP ns4xPluginInstance::NewStream(nsIPluginStreamListener** listener)
{
  ns4xPluginStreamListener* stream = new ns4xPluginStreamListener(this, 
                                                                  nsnull);

	if(stream == nsnull)
		return NS_ERROR_OUT_OF_MEMORY;

	NS_ADDREF(stream);  // Stabilize
    
    nsresult res = stream->QueryInterface(kIPluginStreamListenerIID, (void**)listener);

    // Destabilize and avoid leaks. 
    // Avoid calling delete <interface pointer>
	NS_RELEASE(stream);

	return res;
}

nsresult ns4xPluginInstance::NewNotifyStream(nsIPluginStreamListener** listener, void* notifyData)
{
  *listener = new ns4xPluginStreamListener(this, notifyData);
  return NS_OK;
}

NS_IMETHODIMP ns4xPluginInstance::Print(nsPluginPrint* platformPrint)
{
  return NS_OK;
}

NS_IMETHODIMP ns4xPluginInstance::HandleEvent(nsPluginEvent* event, PRBool* handled)
{
  PRInt16 res = 0;
  
  if (fCallbacks->event)
    {
#if !TARGET_CARBON
      // pinkerton
      // relies on routine descriptors, not present in carbon. 
      // We need to fix this.
#ifdef XP_MAC
      res = CallNPP_HandleEventProc(fCallbacks->event,
                                    &fNPP,
                                    (void*) event->event);
#endif

#ifdef XP_WIN //~~~
      NPEvent npEvent;
      npEvent.event = event->event;
      npEvent.wParam = event->wParam;
      npEvent.lParam = event->lParam;
      res = CallNPP_HandleEventProc(fCallbacks->event,
                                    &fNPP,
                                    (void*)&npEvent);
#endif
      
#endif
      
      *handled = res;
    }

  return NS_OK;
}

NS_IMETHODIMP ns4xPluginInstance :: GetValue(nsPluginInstanceVariable variable,
                                             void *value)
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



