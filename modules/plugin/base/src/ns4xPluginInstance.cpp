/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Tim Copperfield <timecop@network.email.ne.jp>
 *   Roland Mainz <roland.mainz@informatik.med.uni-giessen.de>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prlog.h"
#include "prmem.h"
#include "nscore.h"


#include "ns4xPluginInstance.h"
#include "ns4xPluginStreamListener.h"
#include "nsPluginHostImpl.h"
#include "nsPluginSafety.h"
#include "nsIPref.h" // needed for NS_TRY_SAFE_CALL_*
#include "nsPluginLogging.h"

#if defined(MOZ_WIDGET_GTK)
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "gtkxtbin.h"
#elif defined(MOZ_WIDGET_XLIB)
#include "xlibxtbin.h"
#include "xlibrgb.h"
#endif


////////////////////////////////////////////////////////////////////////
// CID's && IID's
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID); // needed for NS_TRY_SAFE_CALL_*
static NS_DEFINE_IID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);
static NS_DEFINE_IID(kIPluginStreamListener2IID, NS_IPLUGINSTREAMLISTENER2_IID);
static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID); 
static NS_DEFINE_IID(kIPluginTagInfoIID, NS_IPLUGINTAGINFO_IID); 
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

///////////////////////////////////////////////////////////////////////////////
// ns4xPluginStreamListener Methods

NS_IMPL_ISUPPORTS2(ns4xPluginStreamListener, nsIPluginStreamListener, 
                                             nsIPluginStreamListener2);

///////////////////////////////////////////////////////////////////////////////

ns4xPluginStreamListener::ns4xPluginStreamListener(nsIPluginInstance* inst, 
                                                   void* notifyData)
    : mNotifyData(notifyData),
      mStreamStarted(PR_FALSE),
      mStreamCleanedUp(PR_FALSE),
      mStreamInfo(nsnull),
      mCallNotify(PR_FALSE)
{
  NS_INIT_REFCNT();
  mInst = (ns4xPluginInstance*) inst;
  mStreamBuffer=nsnull;
  mPosition = 0;
  mCurrentStreamOffset = -1;
  // Initialize the 4.x interface structure
  memset(&mNPStream, 0, sizeof(mNPStream));

  NS_IF_ADDREF(mInst);
}


///////////////////////////////////////////////////////////////////////////////
ns4xPluginStreamListener::~ns4xPluginStreamListener(void)
{
  // remove itself from the instance stream list
  ns4xPluginInstance *inst = mInst;
  if(inst) {
    nsInstanceStream * prev = nsnull;
    for(nsInstanceStream *is = inst->mStreams; is != nsnull; is = is->mNext) {
      if(is->mPluginStreamListener == this) {
        if(prev == nsnull)
          inst->mStreams = is->mNext;
        else
          prev->mNext = is->mNext;

        delete is;
        break;
      }
      prev = is;
    }
  }

  // For those cases when NewStream is never called, we still may need to fire a
  // notification callback. Return network error as fallback reason because for other
  // cases, notify should have already been called for other reasons elsewhere.
  CallURLNotify(NPRES_NETWORK_ERR);

  NS_IF_RELEASE(inst);
}

///////////////////////////////////////////////////////////////////////////////
nsresult ns4xPluginStreamListener::CleanUpStream(NPReason reason)
{
  nsresult rv = NS_ERROR_FAILURE;

  if(mStreamCleanedUp)
    return NS_OK;

  if(!mInst || !mInst->IsStarted())
    return rv;

  const NPPluginFuncs *callbacks = nsnull;
  mInst->GetCallbacks(&callbacks);
  if(!callbacks)
    return rv;

  NPP npp;
  mInst->GetNPP(&npp);

  if (mStreamStarted && callbacks->destroystream != NULL)
  {
    PRLibrary* lib = nsnull;
    lib = mInst->fLibrary;
    NPError error;
    NS_TRY_SAFE_CALL_RETURN(error, CallNPP_DestroyStreamProc(callbacks->destroystream,
                                                               npp,
                                                               &mNPStream,
                                                               reason), lib);

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP DestroyStream called: this=%p, npp=%p, reason=%d, return=%d, url=%s\n",
    this, npp, reason, error, mNPStream.url));

    if(error == NPERR_NO_ERROR)
      rv = NS_OK;
  }

  // lets get rid of the buffer if we made one globally
  if (mStreamBuffer)
  {
    PR_Free(mStreamBuffer);
    mStreamBuffer=nsnull;
  }

  mStreamCleanedUp = PR_TRUE;
  mStreamStarted   = PR_FALSE;

  // fire notification back to plugin, just like before
  CallURLNotify(reason);

  return rv;
}


///////////////////////////////////////////////////////////////////////////////
void ns4xPluginStreamListener::CallURLNotify(NPReason reason)
{
  if(!mCallNotify || !mInst || !mInst->IsStarted())
    return;

  mCallNotify = PR_FALSE; // only do this ONCE and prevent recursion

  const NPPluginFuncs *callbacks = nsnull;
  mInst->GetCallbacks(&callbacks);
  if(!callbacks)
    return;

  NPP npp;
  mInst->GetNPP(&npp);

  if (callbacks->urlnotify) {
    PRLibrary* lib = mInst->fLibrary;

    NS_TRY_SAFE_CALL_VOID(CallNPP_URLNotifyProc(callbacks->urlnotify,
                                                npp,
                                                mNPStream.url,
                                                reason,
                                                mNotifyData), lib);

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP URLNotify called: this=%p, npp=%p, notify=%p, reason=%d, url=%s\n",
    this, npp, mNotifyData, reason, mNPStream.url));
  }

  // Let's not leak this stream listener. Release the reference to the stream listener 
  // added for the notify callback in NewNotifyStream. 
  // Note: This may destroy us if we are not being destroyed already.
  NS_RELEASE_THIS();
}


///////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
ns4xPluginStreamListener::OnStartBinding(nsIPluginStreamInfo* pluginInfo)
{
  if(!mInst)
    return NS_ERROR_FAILURE;

  NPP npp;
  const NPPluginFuncs *callbacks = nsnull;

  mInst->GetCallbacks(&callbacks);
  mInst->GetNPP(&npp);

  if(!callbacks || !mInst->IsStarted())
    return NS_ERROR_FAILURE;

  PRBool seekable;
  nsMIMEType contentType;
  PRUint16 streamType = NP_NORMAL;
  NPError error;

  mNPStream.ndata = (void*) this;
  pluginInfo->GetURL(&mNPStream.url);
  mNPStream.notifyData = mNotifyData;

  pluginInfo->GetLength((PRUint32*)&(mNPStream.end));
  pluginInfo->GetLastModified((PRUint32*)&(mNPStream.lastmodified));
  pluginInfo->IsSeekable(&seekable);
  pluginInfo->GetContentType(&contentType);

  mStreamInfo = pluginInfo;

  // if we don't know the end of the stream, use 0 instead of -1. bug 59571
  if (mNPStream.end == -1)
    mNPStream.end = 0;

  NS_TRY_SAFE_CALL_RETURN(error, CallNPP_NewStreamProc(callbacks->newstream,
                                                       npp,
                                                       (char *)contentType,
                                                       &mNPStream,
                                                       seekable,
                                                       &streamType), mInst->fLibrary);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP NewStream called: this=%p, npp=%p, mime=%s, seek=%d, type=%d, return=%d, url=%s\n",
  this, npp, (char *)contentType, seekable, streamType, error, mNPStream.url));

  if(error != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;

  // translate the old 4x style stream type to the new one
  switch(streamType)
  {
    case NP_NORMAL:
      mStreamType = nsPluginStreamType_Normal; 
      break;
    case NP_ASFILEONLY:
      mStreamType = nsPluginStreamType_AsFileOnly; 
      break;
    case NP_ASFILE:
      mStreamType = nsPluginStreamType_AsFile; 
      break;
    case NP_SEEK:
      mStreamType = nsPluginStreamType_Seek; 
      break;
    default:
      return NS_ERROR_FAILURE;
  }

  // create buffer for stream here because we know the size
  mStreamBuffer = (char*) PR_Malloc((PRUint32)MAX_PLUGIN_NECKO_BUFFER);
  if (!mStreamBuffer)
  {
    NS_ASSERTION(PR_FALSE,"failed to create 4.x plugin stream buffer or size MAX_PLUGIN_NECKO_BUFFER");
    return NS_ERROR_OUT_OF_MEMORY;
  }

  mStreamStarted = PR_TRUE;
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
ns4xPluginStreamListener::OnDataAvailable(nsIPluginStreamInfo* pluginInfo,
                                          nsIInputStream* input,
                                          PRUint32 length)
{
  if (!mInst || !mInst->IsStarted())
    return NS_ERROR_FAILURE;

  const NPPluginFuncs *callbacks = nsnull;
  NPP npp;

  mInst->GetCallbacks(&callbacks);
  mInst->GetNPP(&npp);

  if(!callbacks)
    return NS_ERROR_FAILURE;

  PRInt32   numtowrite = 0;
  PRInt32   amountRead = 0;
  PRInt32   writeCount = 0;
  PRUint32  leftToRead = 0;         // just in case OnDataaAvail tries to overflow our buffer
  PRBool   createdHere = PR_FALSE;  // we did malloc in locally, so we must free locally

  pluginInfo->GetURL(&mNPStream.url);
  pluginInfo->GetLastModified((PRUint32*)&(mNPStream.lastmodified));

  if (callbacks->write == nsnull || length == 0)
    return NS_OK; // XXX ?

  if (nsnull == mStreamBuffer)
  {
    // create the buffer here because we failed in OnStartBinding
    // XXX why don't we always get an OnStartBinding? This will protect us.
    mStreamBuffer = (char*) PR_Malloc(length);
    if (!mStreamBuffer)
    return NS_ERROR_OUT_OF_MEMORY;
    createdHere = PR_TRUE;
  }

  if (length > MAX_PLUGIN_NECKO_BUFFER)  // what if Necko gives us a lot of data?
  {
    leftToRead = length - MAX_PLUGIN_NECKO_BUFFER; // break it up
    length     = MAX_PLUGIN_NECKO_BUFFER;
  }
  nsresult rv = input->Read(mStreamBuffer, length, (PRUint32*)&amountRead);
  if (NS_FAILED(rv))
    goto error;

  // amountRead tells us how many bytes were put in the buffer
  // WriteReady returns to us how many bytes the plugin is 
  // ready to handle  - we have to keep calling WriteReady and
  // Write until the buffer is empty
  while (amountRead > 0)
  {
    if (callbacks->writeready != NULL)
    {
      PRLibrary* lib = nsnull;
      lib = mInst->fLibrary;

      NS_TRY_SAFE_CALL_RETURN(numtowrite, CallNPP_WriteReadyProc(callbacks->writeready,
                                                                   npp,
                                                                   &mNPStream), lib);

      NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
      ("NPP WriteReady called: this=%p, npp=%p, return(towrite)=%d, url=%s\n",
      this, npp, numtowrite, mNPStream.url));

      // if WriteReady returned 0, the plugin is not ready to handle 
      // the data, return FAILURE for now
      if (numtowrite <= 0) {
        NS_ASSERTION(numtowrite,"WriteReady returned Zero");
        rv = NS_ERROR_FAILURE;
        goto error;
      }

      if (numtowrite > amountRead)
        numtowrite = amountRead;
    }
    else 
    {
      // if WriteReady is not supported by the plugin, 
      // just write the whole buffer
      numtowrite = length;
    }

    if (numtowrite > 0)
    {
      NS_TRY_SAFE_CALL_RETURN(writeCount, CallNPP_WriteProc(callbacks->write,
                                                            npp,
                                                            &mNPStream, 
                                                            mPosition,
                                                            numtowrite,
                                                            (void *)mStreamBuffer), mInst->fLibrary);

      NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
      ("NPP Write called: this=%p, npp=%p, pos=%d, len=%d, buf=%s, return(written)=%d,  url=%s\n",
      this, npp, mPosition, numtowrite, (char *)mStreamBuffer, writeCount, mNPStream.url));

      if (writeCount < 0) {
        rv = NS_ERROR_FAILURE;
        goto error;
      }

      amountRead -= writeCount;
      mPosition += writeCount;
      if (amountRead > 0)
        memmove(mStreamBuffer,mStreamBuffer+writeCount,amountRead); 
    }
  }

  rv = NS_OK;

error:
  if (PR_TRUE == createdHere) // cleanup buffer if we made it locally
  {
    PR_Free(mStreamBuffer);
    mStreamBuffer=nsnull;
  }

  if (leftToRead > 0)  // if we have more to read in this pass, do it recursively
  {
    OnDataAvailable(pluginInfo, input, leftToRead);
  }

  return rv;
}


///////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
ns4xPluginStreamListener::OnDataAvailable(nsIPluginStreamInfo* pluginInfo,
                                          nsIInputStream* input,
                                          PRUint32 sourceOffset, 
                                          PRUint32 length)
{
  if (!mInst || !mInst->IsStarted())
    return NS_ERROR_FAILURE;

  const NPPluginFuncs *callbacks = nsnull;
  NPP npp;

  mInst->GetCallbacks(&callbacks);
  mInst->GetNPP(&npp);

  if(!callbacks)
    return NS_ERROR_FAILURE;

  PRInt32   numtowrite = 0;
  PRInt32   amountRead = 0;
  PRInt32   writeCount = 0;
  PRUint32  leftToRead = 0;         // just in case OnDataaAvail tries to overflow our buffer
  PRBool   createdHere = PR_FALSE;  // we did malloc in locally, so we must free locally

  // if we are getting a range request, reset mPosition.  
  if (sourceOffset != mCurrentStreamOffset ||
      mCurrentStreamOffset == -1 ) 
  {
    mPosition = 0;
    mCurrentStreamOffset = sourceOffset;
  }

  pluginInfo->GetURL(&mNPStream.url);
  pluginInfo->GetLastModified((PRUint32*)&(mNPStream.lastmodified));

  if (callbacks->write == nsnull || length == 0)
    return NS_OK; // XXX ?

  if (nsnull == mStreamBuffer)
  {
    // create the buffer here because we failed in OnStartBinding
    // XXX why don't we always get an OnStartBinding? This will protect us.
    mStreamBuffer = (char*) PR_Malloc(length);
    if (!mStreamBuffer)
    return NS_ERROR_OUT_OF_MEMORY;
    createdHere = PR_TRUE;
  }

  if (length > MAX_PLUGIN_NECKO_BUFFER)  // what if Necko gives us a lot of data?
  {
    leftToRead = length - MAX_PLUGIN_NECKO_BUFFER; // break it up
    length     = MAX_PLUGIN_NECKO_BUFFER;
  }
  nsresult rv = input->Read(mStreamBuffer, length, (PRUint32*)&amountRead);
  if (NS_FAILED(rv))
    goto error;

  // amountRead tells us how many bytes were put in the buffer
  // WriteReady returns to us how many bytes the plugin is 
  // ready to handle  - we have to keep calling WriteReady and
  // Write until the buffer is empty
  while (amountRead > 0)
  {
    if (callbacks->writeready != NULL)
    {
      PRLibrary* lib = nsnull;
      lib = mInst->fLibrary;

      NS_TRY_SAFE_CALL_RETURN(numtowrite, CallNPP_WriteReadyProc(callbacks->writeready,
                                                                   npp,
                                                                   &mNPStream), lib);

      NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
      ("NPP WriteReady called: this=%p, npp=%p, return(towrite)=%d, url=%s\n",
      this, npp, numtowrite, mNPStream.url));

      // if WriteReady returned 0, the plugin is not ready to handle 
      // the data, return FAILURE for now
      if (numtowrite <= 0) {
        rv = NS_ERROR_FAILURE;
        goto error;
      }

      if (numtowrite > amountRead)
        numtowrite = amountRead;
    }
    else 
    {
      // if WriteReady is not supported by the plugin, 
      // just write the whole buffer
      numtowrite = length;
    }

    if (numtowrite > 0)
    {
#if 0 // useful for debugging problems with byte range buffers
      printf(">  %d - %d \n", sourceOffset + writeCount + mPosition, numtowrite);
      nsCString x; 
      x.Append("d:\\parts\\");
      x.AppendInt(sourceOffset);

      PRFileDesc* fd;
      fd = PR_Open(x, PR_CREATE_FILE |PR_SYNC| PR_APPEND | PR_RDWR, 777);
      PR_Write(fd, mStreamBuffer, numtowrite);
      PR_Close(fd);
#endif

      NS_TRY_SAFE_CALL_RETURN(writeCount, CallNPP_WriteProc(callbacks->write,
                                                            npp,
                                                            &mNPStream, 
                                                            sourceOffset + writeCount + mPosition,
                                                            numtowrite,
                                                            (void *)mStreamBuffer), mInst->fLibrary);

     NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
     ("NPP Write called: this=%p, npp=%p, pos=%d, len=%d, buf=%s, return(written)=%d, url=%s\n",
     this, npp, mPosition, numtowrite, (char *)mStreamBuffer, writeCount, mNPStream.url));

      if (writeCount < 0) {
        rv = NS_ERROR_FAILURE;
        goto error;
      }

      if (sourceOffset == 0)
          mPosition += numtowrite;

      amountRead -= numtowrite;
    }
  }

  rv = NS_OK;

error:
  if (PR_TRUE == createdHere) // cleanup buffer if we made it locally
  {
    PR_Free(mStreamBuffer);
    mStreamBuffer=nsnull;
  }

  if (leftToRead > 0)  // if we have more to read in this pass, do it recursively
  {
    OnDataAvailable(pluginInfo, input, sourceOffset, leftToRead);
  }

  return rv;
}


///////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
ns4xPluginStreamListener::OnFileAvailable(nsIPluginStreamInfo* pluginInfo, 
                                          const char* fileName)
{
  if(!mInst || !mInst->IsStarted())
    return NS_ERROR_FAILURE;

  NPP npp;
  const NPPluginFuncs *callbacks = nsnull;

  mInst->GetCallbacks(&callbacks);
  mInst->GetNPP(&npp);

  if(!callbacks)
    return NS_ERROR_FAILURE;

  pluginInfo->GetURL(&mNPStream.url);

  if (callbacks->asfile == NULL)
    return NS_OK;

  PRLibrary* lib = nsnull;
  lib = mInst->fLibrary;

  NS_TRY_SAFE_CALL_VOID(CallNPP_StreamAsFileProc(callbacks->asfile,
                                                   npp,
                                                   &mNPStream,
                                                   fileName), lib);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP StreamAsFile called: this=%p, npp=%p, url=%s, file=%s\n",
  this, npp, mNPStream.url, fileName));

  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
ns4xPluginStreamListener::OnStopBinding(nsIPluginStreamInfo* pluginInfo, 
                                        nsresult status)
{
  if(!mInst || !mInst->IsStarted())
    return NS_ERROR_FAILURE;

  if(pluginInfo) {
    pluginInfo->GetURL(&mNPStream.url);
    pluginInfo->GetLastModified((PRUint32*)&(mNPStream.lastmodified));
  }

  // check if the stream is of seekable type and later its destruction
  // see bug 91140    
  nsresult rv = NS_OK;
  if(mStreamType != nsPluginStreamType_Seek) {
    NPReason reason = NPRES_DONE;

    if (NS_FAILED(status))
      reason = NPRES_NETWORK_ERR;   // since the stream failed, we need to tell the plugin that

    rv = CleanUpStream(reason);
  }
  
  if(rv != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
ns4xPluginStreamListener::GetStreamType(nsPluginStreamType *result)
{
  *result = mStreamType;
  return NS_OK;
}


///////////////////////////////////////////////////////////////////////////////
nsInstanceStream::nsInstanceStream()
{
  mNext = nsnull;
  mPluginStreamListener = nsnull;
}


///////////////////////////////////////////////////////////////////////////////
nsInstanceStream::~nsInstanceStream()
{
}


///////////////////////////////////////////////////////////////////////////////

NS_IMPL_ISUPPORTS2(ns4xPluginInstance, nsIPluginInstance, nsIScriptablePlugin)

///////////////////////////////////////////////////////////////////////////////

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
  mStreams = nsnull;

  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("ns4xPluginInstance ctor: this=%p\n",this));
}


///////////////////////////////////////////////////////////////////////////////
ns4xPluginInstance :: ~ns4xPluginInstance(void)
{
  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("ns4xPluginInstance dtor: this=%p\n",this));

#if defined(MOZ_WIDGET_GTK)
  if (mXtBin)
    gtk_widget_destroy(mXtBin);
#elif defined(MOZ_WIDGET_XLIB)
  if (mXlibXtBin) {
    delete mXlibXtBin;
  }
#endif

  // clean the stream list if any
  for(nsInstanceStream *is = mStreams; is != nsnull;) {
    nsInstanceStream * next = is->mNext;
    delete is;
    is = next;
  }
}


///////////////////////////////////////////////////////////////////////////////
PRBool
ns4xPluginInstance :: IsStarted(void)
{
  return mStarted;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP ns4xPluginInstance::Initialize(nsIPluginInstancePeer* peer)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("ns4xPluginInstance::Initialize this=%p\n",this));

#ifdef MOZ_WIDGET_GTK
  mXtBin = nsnull;
#elif defined(MOZ_WIDGET_XLIB)
  mXlibXtBin = nsnull;
#endif
  return InitializePlugin(peer);
}

////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP ns4xPluginInstance::GetPeer(nsIPluginInstancePeer* *resultingPeer)
{
  *resultingPeer = mPeer;
  NS_IF_ADDREF(*resultingPeer);
  
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP ns4xPluginInstance::Start(void)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("ns4xPluginInstance::Start this=%p\n",this));

#ifdef MOZ_WIDGET_XLIB
  if (mXlibXtBin == nsnull)
    mXlibXtBin = new xtbin();

  if (mXlibXtBin == nsnull)
    return NS_ERROR_OUT_OF_MEMORY;

  if (!mXlibXtBin->xtbin_initialized())
    mXlibXtBin->xtbin_init();

#ifdef NS_DEBUG
  printf("Made new XtBin: %p, %d\n", mXlibXtBin, mXlibXtBin->xtbin_initialized());
#endif
#endif

  if(mStarted)
    return NS_OK;
  else
    return InitializePlugin(mPeer); 
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP ns4xPluginInstance::Stop(void)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("ns4xPluginInstance::Stop this=%p\n",this));

  NPError error;

#if defined(MOZ_WIDGET_GTK)
  if (mXtBin)
    gtk_widget_destroy(mXtBin);
#elif defined(MOZ_WIDGET_XLIB)
  if (mXlibXtBin)
    mXlibXtBin->xtbin_destroy();
#endif

  if(!mStarted)
    return NS_OK;

  if (fCallbacks->destroy == NULL)
    return NS_ERROR_FAILURE; // XXX right error?

  NPSavedData *sdata;

  // clean up open streams
  for(nsInstanceStream *is = mStreams; is != nsnull;) {
    ns4xPluginStreamListener * listener = is->mPluginStreamListener;

    nsInstanceStream *next = is->mNext;
    delete is;
    is = next;
    mStreams = is;

    // Clean up our stream after removing it from the list because 
    // it may be released and destroyed at this point.
    if(listener)
      listener->CleanUpStream(NPRES_USER_BREAK);
  }

  NS_TRY_SAFE_CALL_RETURN(error, CallNPP_DestroyProc(fCallbacks->destroy, &fNPP, &sdata), fLibrary);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP Destroy called: this=%p, npp=%p, return=%d\n", this, &fNPP, error));

  mStarted = PR_FALSE;

  if(error != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult ns4xPluginInstance::InitializePlugin(nsIPluginInstancePeer* peer)
{
  NS_ENSURE_ARG_POINTER(peer);
  mPeer = peer;
 
  nsCOMPtr<nsIPluginTagInfo2> taginfo = do_QueryInterface(mPeer);
  NS_ENSURE_TRUE(taginfo, NS_ERROR_NO_INTERFACE);
  
  PRUint16 count = 0;
  const char* const* names = nsnull;
  const char* const* values = nsnull;
  nsPluginTagType tagtype;
  nsresult rv = taginfo->GetTagType(&tagtype);
  if (NS_SUCCEEDED(rv)) {
    // Note: If we failed to get the tag type, we may be a full page plugin, so no arguments
    rv = taginfo->GetAttributes(count, names, values);
    NS_ENSURE_SUCCESS(rv, rv);
    
    // nsPluginTagType_Object or Applet may also have PARAM tags
    // Note: The arrays handed back by GetParameters() are
    // crafted specially to be directly behind the arrays from GetAtributes()
    // with a null entry as a seperator. This is for 4.x backwards compatibility!
    // see bug 111008 for details
    if (tagtype != nsPluginTagType_Embed) {
      PRUint16 pcount = 0;
      const char* const* pnames = nsnull;
      const char* const* pvalues = nsnull;    
      if (NS_SUCCEEDED(taginfo->GetParameters(pcount, pnames, pvalues))) {
        NS_ASSERTION(nsnull == values[count], "attribute/parameter array not setup correctly for 4.x plugins");
        if (pcount)
          count += ++pcount; //if it's all setup correctly, then all we need is to change the count (attrs + PARAM/blank + params)
      }
    }
  }

  NS_ENSURE_TRUE(fCallbacks->newp, NS_ERROR_FAILURE);
  
  // XXX Note that the NPPluginType_* enums were crafted to be
  // backward compatible...
  
  nsPluginMode  mode;
  nsMIMEType    mimetype;
  NPError       error;

  mPeer->GetMode(&mode);
  mPeer->GetMIMEType(&mimetype);

  NS_TRY_SAFE_CALL_RETURN(error, CallNPP_NewProc(fCallbacks->newp,
                                          (char *)mimetype,
                                          &fNPP,
                                          (PRUint16)mode,
                                          count,
                                          (char**)names,
                                          (char**)values,
                                          NULL), fLibrary);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP New called: this=%p, npp=%p, mime=%s, mode=%d, argc=%d, return=%d\n",
  this, &fNPP, mimetype, mode, count, error));

  if(error != NPERR_NO_ERROR)
    rv = NS_ERROR_FAILURE;
  
  mStarted = PR_TRUE;
  
  return rv;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP ns4xPluginInstance::Destroy(void)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("ns4xPluginInstance::Destroy this=%p\n",this));

  // destruction is handled in the Stop call
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP ns4xPluginInstance::SetWindow(nsPluginWindow* window)
{
#if defined(MOZ_WIDGET_GTK) || defined(MOZ_WIDGET_XLIB)
  NPSetWindowCallbackStruct *ws;
#endif

  // XXX 4.x plugins don't want a SetWindow(NULL).
  if (!window || !mStarted)
    return NS_OK;
  
  NPError error;
  
#ifdef MOZ_WIDGET_GTK
  // bug 108337, flash plugin on linux doesn't like window->width <= 0
  if ((PRInt32) window->width <= 0 || (PRInt32) window->height <= 0)
    return NS_OK;

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
    if (!win)
      return NS_ERROR_FAILURE;
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
        // Check to see if creating mXtBin failed for some reason.
        // if it did, we can't go any further.
        if (!mXtBin)
          return NS_ERROR_FAILURE;
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

  if (!mXtBin)
    return NS_ERROR_FAILURE;

  // And now point the NPWindow structures window 
  // to the actual X window
  window->window = (nsPluginPort *)GTK_XTBIN(mXtBin)->xtwindow;
  
  gtk_xtbin_resize(mXtBin, window->width, window->height);
  
#elif defined(MOZ_WIDGET_XLIB)


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

#if 1
     /* See comment above in GTK+ port ... */
     if (mXlibXtBin) {
       delete mXlibXtBin;
       mXlibXtBin = nsnull;
     }
#endif

      if (!mXlibXtBin) {
        mXlibXtBin = new xtbin();
        // Check to see if creating mXlibXtBin failed for some reason.
        // if it did, we can't go any further.
        if (!mXlibXtBin)
          return NS_ERROR_FAILURE;
      } 
      
    if (window->window) {
#ifdef NS_DEBUG
      printf("About to create new xtbin of %i X %i from %08x...\n",
             window->width, window->height, window->window);
#endif

      mXlibXtBin->xtbin_new((Window)window->window);
      mXlibXtBin->xtbin_resize(0, 0, window->width, window->height);
#ifdef NS_DEBUG
      printf("About to show xtbin(%p)...\n", mXlibXtBin); fflush(NULL);
#endif
      mXlibXtBin->xtbin_realize();
    }
    
    /* Set window attributes */
    XlibRgbHandle *xlibRgbHandle = xxlib_find_handle(XXLIBRGB_DEFAULT_HANDLE);
    Display *xdisplay = xxlib_rgb_get_display(xlibRgbHandle);

    /* Fill in window info structure */
    ws->type     = 0;
    ws->depth    = xxlib_rgb_get_depth(xlibRgbHandle);
    ws->display  = xdisplay;
    ws->visual   = xxlib_rgb_get_visual(xlibRgbHandle);
    ws->colormap = xxlib_rgb_get_cmap(xlibRgbHandle);
    XFlush(ws->display);
  } // !window->ws_info

  // And now point the NPWindow structures window 
  // to the actual X window
  window->window = (nsPluginPort *)mXlibXtBin->xtbin_xtwindow();
#endif // MOZ_WIDGET

  if (fCallbacks->setwindow) {
    // XXX Turns out that NPPluginWindow and NPWindow are structurally
    // identical (on purpose!), so there's no need to make a copy.

    PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("ns4xPluginInstance::SetWindow (about to call it) this=%p\n",this));

    NS_TRY_SAFE_CALL_RETURN(error, CallNPP_SetWindowProc(fCallbacks->setwindow,
                                  &fNPP,
                                  (NPWindow*) window), fLibrary);

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP SetWindow called: this=%p, [x=%d,y=%d,w=%d,h=%d], clip[t=%d,b=%d,l=%d,r=%d], return=%d\n",
    this, window->x, window->y, window->width, window->height,
    window->clipRect.top, window->clipRect.bottom, window->clipRect.left, window->clipRect.right, error));
      
    // XXX In the old code, we'd just ignore any errors coming
    // back from the plugin's SetWindow(). Is this the correct
    // behavior?!?
  }
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
/* NOTE: the caller must free the stream listener */
// Create a normal stream, one without a urlnotify callback
NS_IMETHODIMP ns4xPluginInstance::NewStream(nsIPluginStreamListener** listener)
{
  return NewNotifyStream(listener, nsnull, PR_FALSE);
}


////////////////////////////////////////////////////////////////////////
// Create a stream that will notify when complete
nsresult ns4xPluginInstance::NewNotifyStream(nsIPluginStreamListener** listener, 
                                             void* notifyData,
                                             PRBool aCallNotify)
{
  ns4xPluginStreamListener* stream = new ns4xPluginStreamListener(this, notifyData);
  NS_ENSURE_TRUE(stream, NS_ERROR_OUT_OF_MEMORY);

  // add it to the list
  nsInstanceStream * is = new nsInstanceStream();
  NS_ENSURE_TRUE(is, NS_ERROR_OUT_OF_MEMORY);

  is->mNext = mStreams;
  is->mPluginStreamListener = stream;
  mStreams = is;
  stream->SetCallNotify(aCallNotify);  // set flag in stream to call URLNotify

  NS_ADDREF(stream);  // Stabilize
    
  nsresult res = stream->QueryInterface(kIPluginStreamListenerIID, (void**)listener);

  // Destabilize and avoid leaks. Avoid calling delete <interface pointer>
  NS_RELEASE(stream);

  return res;
}

NS_IMETHODIMP ns4xPluginInstance::Print(nsPluginPrint* platformPrint)
{
  NS_ENSURE_TRUE(platformPrint, NS_ERROR_NULL_POINTER);

  NPPrint* thePrint = (NPPrint *)platformPrint;

  // to be compatible with the older SDK versions and to match what
  // 4.x and other browsers do, overwrite |window.type| field with one
  // more copy of |platformPrint|. See bug 113264
  if(fCallbacks) {
    PRUint16 sdkmajorversion = (fCallbacks->version & 0xff00)>>8;
    PRUint16 sdkminorversion = fCallbacks->version & 0x00ff;
    if((sdkmajorversion == 0) && (sdkminorversion < 11)) { 
      // Let's copy platformPrint bytes over to where it was supposed to be 
      // in older versions -- four bytes towards the beginning of the struct
      // but we should be careful about possible misalignments
      if(sizeof(NPWindowType) >= sizeof(void *)) {
        void* source = thePrint->print.embedPrint.platformPrint; 
        void** destination = (void **)&(thePrint->print.embedPrint.window.type); 
        *destination = source;
      } 
      else 
        NS_ASSERTION(PR_FALSE, "Incompatible OS for assignment");
    }
  }

  NS_TRY_SAFE_CALL_VOID(CallNPP_PrintProc(fCallbacks->print,
                                          &fNPP,
                                          thePrint), fLibrary);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP PrintProc called: this=%p, pDC=%p, [x=%d,y=%d,w=%d,h=%d], clip[t=%d,b=%d,l=%d,r=%d]\n",
  this,
  platformPrint->print.embedPrint.platformPrint,
  platformPrint->print.embedPrint.window.x,
  platformPrint->print.embedPrint.window.y,
  platformPrint->print.embedPrint.window.width,
  platformPrint->print.embedPrint.window.height,
  platformPrint->print.embedPrint.window.clipRect.top,
  platformPrint->print.embedPrint.window.clipRect.bottom,
  platformPrint->print.embedPrint.window.clipRect.left,
  platformPrint->print.embedPrint.window.clipRect.right));

  return NS_OK;
}

NS_IMETHODIMP ns4xPluginInstance::HandleEvent(nsPluginEvent* event, PRBool* handled)
{
  if(!mStarted)
    return NS_OK;

  if (event == nsnull)
    return NS_ERROR_FAILURE;

  PRInt16 result = 0;
  
  if (fCallbacks->event)
    {
#ifdef XP_MAC
      result = CallNPP_HandleEventProc(fCallbacks->event,
                                    &fNPP,
                                    (void*) event->event);
#endif

#if defined(XP_WIN) || defined(XP_OS2)
      NPEvent npEvent;
      npEvent.event = event->event;
      npEvent.wParam = event->wParam;
      npEvent.lParam = event->lParam;

      NS_TRY_SAFE_CALL_RETURN(result, CallNPP_HandleEventProc(fCallbacks->event,
                                    &fNPP,
                                    (void*)&npEvent), fLibrary);
#endif

      NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
      ("NPP HandleEvent called: this=%p, npp=%p, event=%d, return=%d\n", 
      this, &fNPP, event->event, result));

      *handled = result;
    }

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP ns4xPluginInstance :: GetValue(nsPluginInstanceVariable variable,
                                             void *value)
{
  if(!mStarted)
    return NS_OK;

  nsresult  res = NS_OK;

  switch (variable)
  {
    case nsPluginInstanceVariable_WindowlessBool:
      *(PRBool *)value = mWindowless;
      break;

    case nsPluginInstanceVariable_TransparentBool:
      *(PRBool *)value = mTransparent;
      break;

    default:
      if(fCallbacks->getvalue)
      {
        NS_TRY_SAFE_CALL_RETURN(res, 
                                CallNPP_GetValueProc(fCallbacks->getvalue, 
                                                     &fNPP, 
                                                     (NPPVariable)variable, 
                                                     value), 
                                fLibrary);
      }
  }

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP GetValue called: this=%p, npp=%p, var=%d, value=%d, return=%d\n", 
  this, &fNPP, variable, value, res));

  return res;
}


////////////////////////////////////////////////////////////////////////
nsresult ns4xPluginInstance::GetNPP(NPP* aNPP) 
{
  if(aNPP != nsnull)
    *aNPP = &fNPP;
  else
    return NS_ERROR_NULL_POINTER;

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult ns4xPluginInstance::GetCallbacks(const NPPluginFuncs ** aCallbacks)
{
  if(aCallbacks != nsnull)
    *aCallbacks = fCallbacks;
  else
    return NS_ERROR_NULL_POINTER;

  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult ns4xPluginInstance :: SetWindowless(PRBool aWindowless)
{
  mWindowless = aWindowless;
  return NS_OK;
}


////////////////////////////////////////////////////////////////////////
nsresult ns4xPluginInstance :: SetTransparent(PRBool aTransparent)
{
  mTransparent = aTransparent;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////
/* readonly attribute nsQIResult scriptablePeer; */
NS_IMETHODIMP ns4xPluginInstance :: GetScriptablePeer(void * *aScriptablePeer)
{
  if (!aScriptablePeer)
    return NS_ERROR_NULL_POINTER;

  *aScriptablePeer = nsnull;
  return GetValue(nsPluginInstanceVariable_ScriptableInstance, aScriptablePeer);
}


////////////////////////////////////////////////////////////////////////
/* readonly attribute nsIIDPtr scriptableInterface; */
NS_IMETHODIMP ns4xPluginInstance :: GetScriptableInterface(nsIID * *aScriptableInterface)
{
  if (!aScriptableInterface)
    return NS_ERROR_NULL_POINTER;

  *aScriptableInterface = nsnull;
  return GetValue(nsPluginInstanceVariable_ScriptableIID, (void*)aScriptableInterface);
}
