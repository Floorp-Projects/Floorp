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
#include "prenv.h"

#include "ns4xPluginInstance.h"
#include "ns4xPluginStreamListener.h"
#include "nsPluginHostImpl.h"
#include "nsPluginSafety.h"
#include "nsIPref.h" // needed for NS_TRY_SAFE_CALL_*
#include "nsPluginLogging.h"

#ifdef MOZ_WIDGET_GTK
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "gtkxtbin.h"
#endif

#ifdef MOZ_WIDGET_GTK2
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include "gtk2xtbin.h"
#endif

#ifdef MOZ_WIDGET_XLIB
#include "xlibxtbin.h"
#include "xlibrgb.h"
#endif


////////////////////////////////////////////////////////////////////////
// CID's && IID's
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID); // needed for NS_TRY_SAFE_CALL_*
static NS_DEFINE_IID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);
static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);
static NS_DEFINE_IID(kIPluginInstanceIID, NS_IPLUGININSTANCE_IID); 
static NS_DEFINE_IID(kIPluginTagInfoIID, NS_IPLUGINTAGINFO_IID); 
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);

///////////////////////////////////////////////////////////////////////////////
// ns4xPluginStreamListener Methods

NS_IMPL_ISUPPORTS1(ns4xPluginStreamListener, nsIPluginStreamListener);

///////////////////////////////////////////////////////////////////////////////

ns4xPluginStreamListener::ns4xPluginStreamListener(nsIPluginInstance* inst, 
                                                   void* notifyData,
                                                   const char* aURL)
    : mNotifyData(notifyData),
      mStreamBuffer(nsnull),
      mNotifyURL(nsnull),  
      mStreamStarted(PR_FALSE),
      mStreamCleanedUp(PR_FALSE),
      mCallNotify(PR_FALSE),      
      mStreamInfo(nsnull)
{
  mInst = (ns4xPluginInstance*) inst;
  mPosition = 0;
  mStreamBufferSize = 0;
  // Initialize the 4.x interface structure
  memset(&mNPStream, 0, sizeof(mNPStream));

  NS_IF_ADDREF(mInst);

  if (aURL)
    mNotifyURL = PL_strdup(aURL);
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

  // lets get rid of the buffer
  if (mStreamBuffer)
  {
    PR_Free(mStreamBuffer);
    mStreamBuffer=nsnull;
  }


  NS_IF_RELEASE(inst);

  if (mNotifyURL)
    PL_strfree(mNotifyURL);
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
                                                               reason), lib, mInst);

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP DestroyStream called: this=%p, npp=%p, reason=%d, return=%d, url=%s\n",
    this, npp, reason, error, mNPStream.url));

    if(error == NPERR_NO_ERROR)
      rv = NS_OK;
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
  
  if (callbacks->urlnotify) {

    NPP npp;
    mInst->GetNPP(&npp);

    NS_TRY_SAFE_CALL_VOID(CallNPP_URLNotifyProc(callbacks->urlnotify,
                                                npp,
                                                mNotifyURL,
                                                reason,
                                                mNotifyData), mInst->fLibrary, mInst);

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP URLNotify called: this=%p, npp=%p, notify=%p, reason=%d, url=%s\n",
    this, npp, mNotifyData, reason, mNotifyURL));
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


  NS_TRY_SAFE_CALL_RETURN(error, CallNPP_NewStreamProc(callbacks->newstream,
                                                       npp,
                                                       (char *)contentType,
                                                       &mNPStream,
                                                       seekable,
                                                       &streamType), mInst->fLibrary, mInst);

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

  mStreamStarted = PR_TRUE;
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
ns4xPluginStreamListener::OnDataAvailable(nsIPluginStreamInfo* pluginInfo,
                                          nsIInputStream* input,
                                          PRUint32 length)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (!mInst || !mInst->IsStarted())
    return rv;

  const NPPluginFuncs *callbacks = nsnull;
  mInst->GetCallbacks(&callbacks);
  // check out if plugin implements NPP_Write call
  if(!callbacks || !callbacks->write || !length)
    return rv; // it'll cancel necko transaction 
  
  if (!mStreamBuffer)
  {
    // to optimize the mem usage & performance we have to allocate mStreamBuffer here
    // in first ODA when length of data available in input stream is known.
    // mStreamBuffer will be freed in DTOR.
    // we also have to remember the size of that buff
    // to make safe consecutive Read() calls form input stream into our buff.
    if (length >= MAX_PLUGIN_NECKO_BUFFER) {
        // ">" is rare case for decoded stream, but lets eat it all
        mStreamBufferSize = length;    
    } else {
      PRUint32 contentLength;
      pluginInfo->GetLength(&contentLength);
      if (contentLength < MAX_PLUGIN_NECKO_BUFFER) {
        // this is most common case for contentLength < 16k
        mStreamBufferSize = length < contentLength ? contentLength:length; 
      } else {
        mStreamBufferSize = MAX_PLUGIN_NECKO_BUFFER;
      }
    }
    mStreamBuffer = (char*) PR_Malloc(mStreamBufferSize);
    if (!mStreamBuffer)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  
  // prepare NPP_ calls params
  NPP npp;    
  mInst->GetNPP(&npp);
  PRInt32 streamOffset;
  pluginInfo->GetStreamOffset(&streamOffset);
  mPosition = streamOffset;
  streamOffset += length;

  // Set new stream offset for the next ODA call
  // regardless of how following NPP_Write call will behave
  // we pretend to consume all data from the input stream.
  // It's possible that current steam position will be overwritten
  // from NPP_RangeRequest call made from NPP_Write, so
  // we cannot call SetStreamOffset after NPP_Write.
  // Note: there is a special case when data flow
  // should be temporarily stopped if NPP_WriteReady returns 0 (bug #89270)
  pluginInfo->SetStreamOffset(streamOffset);

  // set new end in case the content is compressed
  // initial end is less than end of decompressed stream
  // and some plugins (e.g. acrobat) can fail. 
  if ((PRInt32)mNPStream.end < streamOffset)
    mNPStream.end = streamOffset;

  PRUint32 bytesToRead = mStreamBufferSize;
  if (length < mStreamBufferSize) {
    // do not read more that supplier wants us to read
    bytesToRead = length;
  }

  do 
  {
    PRInt32 amountRead = 0;
    rv = input->Read(mStreamBuffer, bytesToRead, (PRUint32*)&amountRead);
    if (amountRead == 0 || NS_FAILED(rv)) {
      NS_WARNING("input->Read() returns no data, it's almost impossible to get here");
      break;
    }

    // this loop in general case will end on length <= 0, without extra input->Read() call
    length -= amountRead;
    
    char *ptrStreamBuffer = mStreamBuffer; // tmp ptr

    // it is possible plugin's NPP_Write() returns 0 byte consumed
    // we use zeroBytesWriteCount to count situation like this
    // and break the loop
    PRInt32 zeroBytesWriteCount = 0;
    
    // amountRead tells us how many bytes were put in the buffer
    // WriteReady returns to us how many bytes the plugin is 
    // ready to handle  - we have to keep calling WriteReady and
    // Write until amountRead > 0
    for(;;) 
    { // it breaks on (amountRead <= 0) or on error
      PRInt32   numtowrite;
      if (callbacks->writeready)
      {
        NS_TRY_SAFE_CALL_RETURN(numtowrite, 
          CallNPP_WriteReadyProc(callbacks->writeready, npp, &mNPStream),
          mInst->fLibrary, mInst);
       
        NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
          ("NPP WriteReady called: this=%p, npp=%p, return(towrite)=%d, url=%s\n",
          this, npp, numtowrite, mNPStream.url));
        
        // if WriteReady returned 0, the plugin is not ready to handle 
        // the data, return FAILURE for now
        if (numtowrite <= 0) {
          NS_ASSERTION(numtowrite,"WriteReady returned Zero");
          rv = NS_ERROR_FAILURE;
          break;
        }
        if (numtowrite > amountRead)
          numtowrite = amountRead;
      }
      else
      {
        // if WriteReady is not supported by the plugin, 
        // just write the whole buffer
        numtowrite = amountRead;
      }
      
      PRInt32   writeCount = 0; // bytes consumed by plugin instance
      
      NS_TRY_SAFE_CALL_RETURN(writeCount, 
        CallNPP_WriteProc(callbacks->write, npp, &mNPStream, mPosition, numtowrite, (void *)ptrStreamBuffer),
        mInst->fLibrary, mInst);
      
      NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
        ("NPP Write called: this=%p, npp=%p, pos=%d, len=%d, buf=%s, return(written)=%d,  url=%s\n",
        this, npp, mPosition, numtowrite, (char *)ptrStreamBuffer, writeCount, mNPStream.url));
      
      if (writeCount > 0)
      {
        mPosition += writeCount;
        amountRead -= writeCount;
        if (amountRead <= 0)
          break; // in common case we'll break for(;;) loop here 
        
        zeroBytesWriteCount = 0;
        if (writeCount % sizeof(long)) {
          // memmove will take care  about alignment 
          memmove(ptrStreamBuffer,ptrStreamBuffer+writeCount,amountRead);
        } else {
          // if aligned we can use ptrStreamBuffer += to eliminate memmove()
          ptrStreamBuffer += writeCount;
        }
      } 
      else if (writeCount == 0)
      {
        // if NPP_Write() returns writeCount == 0 lets say 3 times in a raw
        // lets consider this as end of ODA call (plugin isn't hungry, or broken) without an error.
        if (++zeroBytesWriteCount == 3)
        {
          length = 0; // break do{}while
          rv = NS_OK;
          break;
        }
      } 
      else
      {
        length = 0; // break do{}while
        rv = NS_ERROR_FAILURE;
        break;
      }  
    } // end of for(;;)
  } while ((PRInt32)(length) > 0);

  return rv == NS_BASE_STREAM_WOULD_BLOCK ? NS_OK:rv;
}

///////////////////////////////////////////////////////////////////////////////
NS_IMETHODIMP
ns4xPluginStreamListener::OnFileAvailable(nsIPluginStreamInfo* pluginInfo, 
                                          const char* fileName)
{
  if(!mInst || !mInst->IsStarted())
    return NS_ERROR_FAILURE;

  const NPPluginFuncs *callbacks = nsnull;
  mInst->GetCallbacks(&callbacks);
  if(!callbacks && !callbacks->asfile)
    return NS_ERROR_FAILURE;
  
  NPP npp;
  mInst->GetNPP(&npp);

  PRLibrary* lib = nsnull;
  lib = mInst->fLibrary;

  NS_TRY_SAFE_CALL_VOID(CallNPP_StreamAsFileProc(callbacks->asfile,
                                                   npp,
                                                   &mNPStream,
                                                   fileName), lib, mInst);

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
  NS_ASSERTION(fCallbacks != NULL, "null callbacks");

  // Initialize the NPP structure.

  fNPP.pdata = NULL;
  fNPP.ndata = this;

  fLibrary = aLibrary;
  mWindowless = PR_FALSE;
  mTransparent = PR_FALSE;
  mStarted = PR_FALSE;
  mStreams = nsnull;
  mCached = PR_FALSE;

  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("ns4xPluginInstance ctor: this=%p\n",this));
}


///////////////////////////////////////////////////////////////////////////////
ns4xPluginInstance :: ~ns4xPluginInstance(void)
{
  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("ns4xPluginInstance dtor: this=%p\n",this));

#if defined(MOZ_WIDGET_GTK) || defined (MOZ_WIDGET_GTK2)
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

#if defined (MOZ_WIDGET_GTK) || defined (MOZ_WIDGET_GTK2)
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

#if defined(MOZ_WIDGET_GTK) || defined (MOZ_WIDGET_GTK2)
  if (mXtBin) {
    gtk_widget_destroy(mXtBin);
    mXtBin = 0;
  }
#elif defined(MOZ_WIDGET_XLIB)
  if (mXlibXtBin) {
    mXlibXtBin->xtbin_destroy();
    mXlibXtBin = 0;
  }
#endif

  if(!mStarted)
    return NS_OK;

  if (fCallbacks->destroy == NULL)
    return NS_ERROR_FAILURE; // XXX right error?

  NPSavedData *sdata = 0;

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

  NS_TRY_SAFE_CALL_RETURN(error, CallNPP_DestroyProc(fCallbacks->destroy, &fNPP, &sdata), fLibrary, this);

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
 
  nsCOMPtr<nsIPluginTagInfo2> taginfo = do_QueryInterface(peer);
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

  peer->GetMode(&mode);
  peer->GetMIMEType(&mimetype);

  // Some older versions of Flash have a bug in them
  // that causes the stack to become currupt if we
  // pass swliveconect=1 in the NPP_NewProc arrays.
  // See bug 149336 (UNIX), bug 186287 (Mac)
  //
  // The code below disables the attribute unless
  // the environment variable:
  // MOZILLA_PLUGIN_DISABLE_FLASH_SWLIVECONNECT_HACK
  // is set.
  //
  // It is okay to disable this attribute because
  // back in 4.x, scripting required liveconnect to
  // start Java which was slow. Scripting no longer
  // requires starting Java and is quick plus controled
  // from the browser, so Flash now ignores this attribute.
  //
  // This code can not be put at the time of creating
  // the array because we may need to examine the
  // stream header to determine we want Flash.

  static const char flashMimeType[] = "application/x-shockwave-flash";
  static const char blockedParam[] = "swliveconnect";
  if (count && !PL_strcasecmp(mimetype, flashMimeType)) {
    static int cachedDisableHack = 0;
    if (!cachedDisableHack) {
       if (PR_GetEnv("MOZILLA_PLUGIN_DISABLE_FLASH_SWLIVECONNECT_HACK"))
         cachedDisableHack = -1;
       else
         cachedDisableHack = 1;
    }
    if (cachedDisableHack > 0) {
      for (PRUint16 i=0; i<count; i++) {
        if (!PL_strcasecmp(names[i], blockedParam)) {
          // BIG FAT WARNIG:
          // I'm ugly casting |const char*| to |char*| and altering it
          // because I know we do malloc it values in
          // http://bonsai.mozilla.org/cvsblame.cgi?file=mozilla/layout/html/base/src/nsObjectFrame.cpp&rev=1.349&root=/cvsroot#3020
          // and free it at line #2096, so it couldn't be a const ptr to string literal
          char *val = (char*) values[i];
          if (val && *val) {
            // we cannot just *val=0, it wont be free properly in such case
            val[0] = '0';
            val[1] = 0;
          }
          break;
        }
      }
    }
  }

  // Assign mPeer now and mark this instance as started before calling NPP_New 
  // because the plugin may call other NPAPI functions, like NPN_GetURLNotify,
  // that assume these are set before returning. If the plugin returns failure,
  // we'll clear them out below.
  mPeer = peer;
  mStarted = PR_TRUE;

  NS_TRY_SAFE_CALL_RETURN(error, CallNPP_NewProc(fCallbacks->newp,
                                          (char *)mimetype,
                                          &fNPP,
                                          (PRUint16)mode,
                                          count,
                                          (char**)names,
                                          (char**)values,
                                          NULL), fLibrary,this);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP New called: this=%p, npp=%p, mime=%s, mode=%d, argc=%d, return=%d\n",
  this, &fNPP, mimetype, mode, count, error));

  if(error != NPERR_NO_ERROR) {
    // since the plugin returned failure, these should not be set
    mPeer = nsnull;
    mStarted = PR_FALSE;

    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
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
#if defined(MOZ_WIDGET_GTK) || defined (MOZ_WIDGET_GTK2) || defined(MOZ_WIDGET_XLIB)
  NPSetWindowCallbackStruct *ws;
#endif

  // XXX 4.x plugins don't want a SetWindow(NULL).
  if (!window || !mStarted)
    return NS_OK;
  
  NPError error;
  
#if defined (MOZ_WIDGET_GTK) || defined (MOZ_WIDGET_GTK2)
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

#if 0
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
    ws->depth = gdk_window_get_visual(win)->depth;
    ws->display = GTK_XTBIN(mXtBin)->xtdisplay;
    ws->visual = GDK_VISUAL_XVISUAL(gdk_window_get_visual(win));
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
                                  (NPWindow*) window), fLibrary, this);


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
  return NewNotifyStream(listener, nsnull, PR_FALSE, nsnull);
}


////////////////////////////////////////////////////////////////////////
// Create a stream that will notify when complete
nsresult ns4xPluginInstance::NewNotifyStream(nsIPluginStreamListener** listener, 
                                             void* notifyData,
                                             PRBool aCallNotify,
                                             const char* aURL)
{
  ns4xPluginStreamListener* stream = new ns4xPluginStreamListener(this, notifyData, aURL);
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
                                          thePrint), fLibrary, this);

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
  
  if (fCallbacks->event) {
#if defined(XP_MAC) || defined(XP_MACOSX)
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
                                    (void*)&npEvent), fLibrary, this);
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
  nsresult  res = NS_OK;

  switch (variable) {
    case nsPluginInstanceVariable_WindowlessBool:
      *(PRBool *)value = mWindowless;
      break;

    case nsPluginInstanceVariable_TransparentBool:
      *(PRBool *)value = mTransparent;
      break;

    case nsPluginInstanceVariable_DoCacheBool:
      *(PRBool *)value = mCached;
      break;

    case nsPluginInstanceVariable_CallSetWindowAfterDestroyBool:
      *(PRBool *)value = 0;  // not supported for 4.x plugins
      break;

    default:
      if(fCallbacks->getvalue && mStarted) {
        NS_TRY_SAFE_CALL_RETURN(res, 
                                CallNPP_GetValueProc(fCallbacks->getvalue, 
                                                     &fNPP, 
                                                     (NPPVariable)variable, 
                                                     value), 
                                                     fLibrary, this);
        NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
        ("NPP GetValue called: this=%p, npp=%p, var=%d, value=%d, return=%d\n", 
        this, &fNPP, variable, value, res));
      }
  }

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
#if !defined(XP_MACOSX) // Workaround suspected bug
  if (!aScriptableInterface)
    return NS_ERROR_NULL_POINTER;

  *aScriptableInterface = nsnull;
  return GetValue(nsPluginInstanceVariable_ScriptableIID, (void*)aScriptableInterface);
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}
