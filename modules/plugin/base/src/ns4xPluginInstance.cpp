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
#include "nsPluginHostImpl.h"

#include "prlog.h"
#include "prmem.h"
#include "nscore.h"

#if defined(MOZ_WIDGET_GTK)
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "gtkxtbin.h"
#endif

#include "nsPluginSafety.h"
#include "nsIPref.h" // needed for NS_TRY_SAFE_CALL_*

static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID); // needed for NS_TRY_SAFE_CALL_*
static NS_DEFINE_IID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);

class ns4xPluginStreamListener : public nsIPluginStreamListener {

public:

  NS_DECL_ISUPPORTS

  ///////////////////////////////////////////////////////////////////////////
  // from nsIPluginStreamListener:

  NS_IMETHOD OnStartBinding(nsIPluginStreamInfo* pluginInfo);

  NS_IMETHOD OnDataAvailable(nsIPluginStreamInfo* pluginInfo, nsIInputStream* input, 
                             PRUint32 length);

  NS_IMETHOD OnFileAvailable( nsIPluginStreamInfo* pluginInfo, const char* fileName);

  NS_IMETHOD OnStopBinding(nsIPluginStreamInfo* pluginInfo, nsresult status);

  NS_IMETHOD GetStreamType(nsPluginStreamType *result);

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

  PRLibrary* lib = nsnull;
  if(mInst)
    lib = mInst->fLibrary;

  NS_TRY_SAFE_CALL_RETURN(error, CallNPP_NewStreamProc(callbacks->newstream,
                                          npp,
                                          (char *)contentType,
                                          &mNPStream,
                                          seekable,
                                          &streamType), lib);
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
  NPP       npp;
  PRUint32  numtowrite = 0;
  PRUint32  amountRead = 0;
  PRInt32   writeCount = 0;

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

      PRLibrary* lib = nsnull;
      if(mInst)
        lib = mInst->fLibrary;

      NS_TRY_SAFE_CALL_RETURN(numtowrite, CallNPP_WriteReadyProc(callbacks->writeready,
                                              npp,
                                              &mNPStream), lib);
#endif
		    
      // if WriteReady returned 0, the plugin is not ready to handle 
      // the data, return FAILURE for now
      if(numtowrite <= 0)
        return NS_ERROR_FAILURE;

      if (numtowrite > amountRead)
        numtowrite = amountRead;
    }
    else 
      // if WriteReady is not supported by the plugin, 
      // just write the whole buffer
      numtowrite = length;

    if(numtowrite > 0)
    {
#if !TARGET_CARBON
      // pinkerton
      // relies on routine descriptors, not present in carbon. 
      // We need to fix this.

      PRLibrary* lib = nsnull;
      if(mInst)
        lib = mInst->fLibrary;

      NS_TRY_SAFE_CALL_RETURN(writeCount, CallNPP_WriteProc(callbacks->write,
                              npp,
                              &mNPStream, 
                              mPosition,
                              numtowrite,
                              (void *)buffer), lib);
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

    PRLibrary* lib = nsnull;
    if(mInst)
      lib = mInst->fLibrary;

    NS_TRY_SAFE_CALL_VOID(CallNPP_StreamAsFileProc(callbacks->asfile,
                             npp,
                             &mNPStream,
                             fileName), lib);
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
      PRLibrary* lib = nsnull;
      if(mInst)
        lib = mInst->fLibrary;

      NS_TRY_SAFE_CALL_RETURN(error, CallNPP_DestroyStreamProc(callbacks->destroystream,
                                        npp,
                                        &mNPStream,
                                        NPRES_DONE), lib);
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

      PRLibrary* lib = nsnull;
      if(mInst)
        lib = mInst->fLibrary;

      NS_TRY_SAFE_CALL_VOID(CallNPP_URLNotifyProc(callbacks->urlnotify,
                            npp,
                            mNPStream.url,
                            nsPluginReason_Done,
                            mNotifyData), lib);
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

ns4xPluginInstance :: ns4xPluginInstance(NPPluginFuncs* callbacks, PRLibrary* aLibrary)
    : fCallbacks(callbacks)
{
    NS_INIT_REFCNT();

    NS_ASSERTION(fCallbacks != NULL, "null callbacks");

    // Initialize the NPP structure.

    fNPP.pdata = NULL;
    fNPP.ndata = this;

    fLibrary = aLibrary;
    mWindowless = PR_FALSE;
    mTransparent = PR_FALSE;
    mStarted = PR_FALSE;
}


ns4xPluginInstance :: ~ns4xPluginInstance(void)
{
#ifdef NS_DEBUG
  printf("ns4xPluginInstance::~ns4xPluginInstance()\n");
#endif
#if defined(MOZ_WIDGET_GTK)
  if (mXtBin)
    gtk_widget_destroy(mXtBin);
#endif
}


////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS1(ns4xPluginInstance, nsIPluginInstance)

static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID); 
static NS_DEFINE_IID(kIPluginTagInfoIID, NS_IPLUGINTAGINFO_IID); 
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP ns4xPluginInstance::Initialize(nsIPluginInstancePeer* peer)
{
#ifdef MOZ_WIDGET_GTK
  mXtBin = nsnull;
#endif
  return InitializePlugin(peer);
}

NS_IMETHODIMP ns4xPluginInstance::GetPeer(nsIPluginInstancePeer* *resultingPeer)
{
  *resultingPeer = mPeer;
  NS_IF_ADDREF(*resultingPeer);
  
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
    return InitializePlugin(mPeer); 
}

NS_IMETHODIMP ns4xPluginInstance::Stop(void)
{
  NPError error;

#ifdef NS_DEBUG
  printf("ns4xPluginInstance::Stop()\n");
#endif

#ifdef MOZ_WIDGET_GTK
  if (mXtBin)
    gtk_widget_destroy(mXtBin);
#endif



  if (fCallbacks->destroy == NULL)
    return NS_ERROR_FAILURE; // XXX right error?

  NPSavedData *sdata;

#if !TARGET_CARBON
  // pinkerton
  // relies on routine descriptors, not present in carbon. 
  // We need to fix this.
  NS_TRY_SAFE_CALL_RETURN(error, CallNPP_DestroyProc(fCallbacks->destroy, &fNPP, &sdata), fLibrary);

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

  
  NS_ASSERTION(peer != NULL, "null peer");

  mPeer = peer;

  nsCOMPtr<nsIPluginTagInfo> taginfo = do_QueryInterface(mPeer, &rv);

  if (NS_SUCCEEDED(rv))
  {
    taginfo->GetAttributes(count, names, values);
  }

  if (fCallbacks->newp == NULL)
    return NS_ERROR_FAILURE; // XXX right error?
  
  // XXX Note that the NPPluginType_* enums were crafted to be
  // backward compatible...
  
  nsPluginMode  mode;
  nsMIMEType    mimetype;

  mPeer->GetMode(&mode);
  mPeer->GetMIMEType(&mimetype);

#if !TARGET_CARBON
  // pinkerton
  // relies on routine descriptors, not present in carbon. We need to fix this.

  NS_TRY_SAFE_CALL_RETURN(error, CallNPP_NewProc(fCallbacks->newp,
                                          (char *)mimetype,
                                          &fNPP,
                                          (PRUint16)mode,
                                          count,
                                          (char**)names,
                                          (char**)values,
                                          NULL), fLibrary);
#endif //!TARGET_CARBON
  
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
#ifdef MOZ_WIDGET_GTK
  NPSetWindowCallbackStruct *ws;
#endif

  // XXX 4.x plugins don't want a SetWindow(NULL).

#ifdef NS_DEBUG
  printf("Inside ns4xPluginInstance::SetWindow(%p)...\n", window);
#endif

  if (!window || !mStarted)
    return NS_OK;
  
  NPError error;
  
#ifdef MOZ_WIDGET_GTK
  // Allocate and fill out the ws_info data
  if (!window->ws_info) {
#ifdef NS_DEBUG
    printf("About to create new ws_info...\n");
#endif    

    // allocate a new NPSetWindowCallbackStruct structure at ws_info
    window->ws_info = (NPSetWindowCallbackStruct *)PR_MALLOC(sizeof(NPSetWindowCallbackStruct));

    if (!window->ws_info)
      return NS_ERROR_OUT_OF_MEMORY;

    ws = (NPSetWindowCallbackStruct *)window->ws_info;

    GdkWindow *win = gdk_window_lookup((XID)window->window);
    if (win)
    {
#ifdef NS_DEBUG      
      printf("About to create new xtbin of %i X %i from %p...\n",
             window->width, window->height, win);
#endif

#if 1
      // if we destroyed the plugin when we left the page, we could remove this
      // code (i believe) the problem here is that the window gets destroyed when
      // its parent, etc does by changing a page the plugin instance is being
      // held on to, so when we return to the page, we have a mXtBin, but it is
      // in a not-so-good state.
      // --
      // this is lame.  we shouldn't be destroying this everytime, but I can't find
      // a good way to tell if we need to destroy/recreate the xtbin or not
      // what if the plugin wants to change the window and not just resize it??
      // (pav)

      if (mXtBin) {
        gtk_widget_destroy(mXtBin);
        mXtBin = NULL;
      }
#endif


      if (!mXtBin) {
        mXtBin = gtk_xtbin_new(win, 0);
      } 

      gtk_widget_set_usize(mXtBin, window->width, window->height);

#ifdef NS_DEBUG
      printf("About to show xtbin(%p)...\n", mXtBin); fflush(NULL);
#endif
      gtk_widget_show(mXtBin);
#ifdef NS_DEBUG
      printf("completed gtk_widget_show(%p)\n", mXtBin); fflush(NULL);
#endif
    }

    // fill in window info structure 
    ws->type = 0; // OK, that was a guess!!
    ws->depth = gdk_rgb_get_visual()->depth;
    ws->display = GTK_XTBIN(mXtBin)->xtdisplay;
    ws->visual = GDK_VISUAL_XVISUAL(gdk_rgb_get_visual());
    ws->colormap = GDK_COLORMAP_XCOLORMAP(gdk_window_get_colormap(win));

    XFlush(ws->display);
  } // !window->ws_info

  // And now point the NPWindow structures window 
  // to the actual X window
  window->window = (nsPluginPort *)GTK_XTBIN(mXtBin)->xtwindow;
#endif // MOZ_WIDGET_GTK

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

    NS_TRY_SAFE_CALL_RETURN(error, CallNPP_SetWindowProc(fCallbacks->setwindow,
                                  &fNPP,
                                  (NPWindow*) window), fLibrary);

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

#ifdef XP_WIN
      NPEvent npEvent;
      npEvent.event = event->event;
      npEvent.wParam = event->wParam;
      npEvent.lParam = event->lParam;

      NS_TRY_SAFE_CALL_RETURN(res, CallNPP_HandleEventProc(fCallbacks->event,
                                    &fNPP,
                                    (void*)&npEvent), fLibrary);
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



