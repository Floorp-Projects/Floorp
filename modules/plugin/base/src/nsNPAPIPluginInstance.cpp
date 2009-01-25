/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "prlog.h"
#include "prmem.h"
#include "nscore.h"
#include "prenv.h"

#include "nsNPAPIPluginInstance.h"
#include "nsNPAPIPlugin.h"
#include "nsNPAPIPluginStreamListener.h"
#include "nsPluginHostImpl.h"
#include "nsPluginSafety.h"
#include "nsPluginLogging.h"

#include "nsPIPluginInstancePeer.h"
#include "nsPIDOMWindow.h"
#include "nsIDocument.h"

#include "nsJSNPRuntime.h"

#ifdef XP_OS2
#include "nsILegacyPluginWrapperOS2.h"
#endif

static NS_DEFINE_IID(kCPluginManagerCID, NS_PLUGINMANAGER_CID); // needed for NS_TRY_SAFE_CALL
static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);

// nsNPAPIPluginStreamListener Methods

NS_IMPL_ISUPPORTS3(nsNPAPIPluginStreamListener, nsIPluginStreamListener,
                   nsITimerCallback, nsIHTTPHeaderListener)

nsNPAPIPluginStreamListener::nsNPAPIPluginStreamListener(nsNPAPIPluginInstance* inst, 
                                                         void* notifyData,
                                                         const char* aURL)
  : mNotifyData(notifyData),
    mStreamBuffer(nsnull),
    mNotifyURL(aURL ? PL_strdup(aURL) : nsnull),
    mInst(inst),
    mStreamBufferSize(0),
    mStreamBufferByteCount(0),
    mStreamType(nsPluginStreamType_Normal),
    mStreamStarted(PR_FALSE),
    mStreamCleanedUp(PR_FALSE),
    mCallNotify(PR_FALSE),
    mIsSuspended(PR_FALSE),
    mIsPluginInitJSStream(mInst->mInPluginInitCall &&
                          aURL && strncmp(aURL, "javascript:",
                                          sizeof("javascript:") - 1) == 0),
    mResponseHeaderBuf(nsnull)
{
  memset(&mNPStream, 0, sizeof(mNPStream));

  NS_IF_ADDREF(mInst);
}

nsNPAPIPluginStreamListener::~nsNPAPIPluginStreamListener(void)
{
  // remove itself from the instance stream list
  nsNPAPIPluginInstance *inst = mInst;
  if (inst) {
    nsInstanceStream * prev = nsnull;
    for (nsInstanceStream *is = inst->mStreams; is != nsnull; is = is->mNext) {
      if (is->mPluginStreamListener == this) {
        if (!prev)
          inst->mStreams = is->mNext;
        else
          prev->mNext = is->mNext;

        delete is;
        break;
      }
      prev = is;
    }
  }

  // For those cases when NewStream is never called, we still may need
  // to fire a notification callback. Return network error as fallback
  // reason because for other cases, notify should have already been
  // called for other reasons elsewhere.
  CallURLNotify(NPRES_NETWORK_ERR);

  // lets get rid of the buffer
  if (mStreamBuffer) {
    PR_Free(mStreamBuffer);
    mStreamBuffer=nsnull;
  }

  NS_IF_RELEASE(inst);

  if (mNotifyURL)
    PL_strfree(mNotifyURL);

  if (mResponseHeaderBuf)
    PL_strfree(mResponseHeaderBuf);
}

nsresult nsNPAPIPluginStreamListener::CleanUpStream(NPReason reason)
{
  nsresult rv = NS_ERROR_FAILURE;

  if (mStreamCleanedUp)
    return NS_OK;

  if (!mInst || !mInst->IsStarted())
    return rv;

  PluginDestructionGuard guard(mInst);

  const NPPluginFuncs *callbacks = nsnull;
  mInst->GetCallbacks(&callbacks);
  if (!callbacks)
    return rv;

  NPP npp;
  mInst->GetNPP(&npp);

  if (mStreamStarted && callbacks->destroystream) {
    PRLibrary* lib = nsnull;
    lib = mInst->fLibrary;
    NPError error;
    NS_TRY_SAFE_CALL_RETURN(error, (*callbacks->destroystream)(npp, &mNPStream, reason), lib, mInst);

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP DestroyStream called: this=%p, npp=%p, reason=%d, return=%d, url=%s\n",
    this, npp, reason, error, mNPStream.url));

    if (error == NPERR_NO_ERROR)
      rv = NS_OK;
  }

  mStreamCleanedUp = PR_TRUE;
  mStreamStarted   = PR_FALSE;

  StopDataPump();

  // fire notification back to plugin, just like before
  CallURLNotify(reason);

  return rv;
}

void nsNPAPIPluginStreamListener::CallURLNotify(NPReason reason)
{
  if (!mCallNotify || !mInst || !mInst->IsStarted())
    return;

  PluginDestructionGuard guard(mInst);

  mCallNotify = PR_FALSE; // only do this ONCE and prevent recursion

  const NPPluginFuncs *callbacks = nsnull;
  mInst->GetCallbacks(&callbacks);
  if (!callbacks)
    return;
  
  if (callbacks->urlnotify) {

    NPP npp;
    mInst->GetNPP(&npp);

    NS_TRY_SAFE_CALL_VOID((*callbacks->urlnotify)(npp, mNotifyURL, reason, mNotifyData), mInst->fLibrary, mInst);

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP URLNotify called: this=%p, npp=%p, notify=%p, reason=%d, url=%s\n",
    this, npp, mNotifyData, reason, mNotifyURL));
  }
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::OnStartBinding(nsIPluginStreamInfo* pluginInfo)
{
  if (!mInst)
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(mInst);

  NPP npp;
  const NPPluginFuncs *callbacks = nsnull;

  mInst->GetCallbacks(&callbacks);
  mInst->GetNPP(&npp);

  if (!callbacks || !mInst->IsStarted())
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
  
  if (!mResponseHeaders.IsEmpty()) {
    mResponseHeaderBuf = PL_strdup(mResponseHeaders.get());
    mNPStream.headers = mResponseHeaderBuf;
  }

  mStreamInfo = pluginInfo;

  NS_TRY_SAFE_CALL_RETURN(error, (*callbacks->newstream)(npp, (char*)contentType, &mNPStream, seekable, &streamType), mInst->fLibrary, mInst);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP NewStream called: this=%p, npp=%p, mime=%s, seek=%d, type=%d, return=%d, url=%s\n",
  this, npp, (char *)contentType, seekable, streamType, error, mNPStream.url));

  if (error != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;

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

nsresult
nsNPAPIPluginStreamListener::SuspendRequest()
{
  NS_ASSERTION(!mIsSuspended,
               "Suspending a request that's already suspended!");

  nsCOMPtr<nsINPAPIPluginStreamInfo> pluginInfoNPAPI =
    do_QueryInterface(mStreamInfo);
  nsIRequest *request;

  if (!pluginInfoNPAPI || !(request = pluginInfoNPAPI->GetRequest())) {
    NS_ERROR("Trying to suspend a non-suspendable stream!");
    return NS_ERROR_FAILURE;
  }

  nsresult rv = StartDataPump();
  NS_ENSURE_SUCCESS(rv, rv);

  mIsSuspended = PR_TRUE;

  return request->Suspend();
}

void
nsNPAPIPluginStreamListener::ResumeRequest()
{
  nsCOMPtr<nsINPAPIPluginStreamInfo> pluginInfoNPAPI =
    do_QueryInterface(mStreamInfo);

  nsIRequest *request = pluginInfoNPAPI->GetRequest();

  // request can be null if the network stream is done.
  if (request)
    request->Resume();

  mIsSuspended = PR_FALSE;
}

nsresult
nsNPAPIPluginStreamListener::StartDataPump()
{
  nsresult rv;
  mDataPumpTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  // Start pumping data to the plugin every 100ms until it obeys and
  // eats the data.
  return mDataPumpTimer->InitWithCallback(this, 100,
                                          nsITimer::TYPE_REPEATING_SLACK);
}

void
nsNPAPIPluginStreamListener::StopDataPump()
{
  if (mDataPumpTimer) {
    mDataPumpTimer->Cancel();
    mDataPumpTimer = nsnull;
  }
}

// Return true if a javascript: load that was started while the plugin
// was being initialized is still in progress.
PRBool
nsNPAPIPluginStreamListener::PluginInitJSLoadInProgress()
{
  for (nsInstanceStream *is = mInst->mStreams; is; is = is->mNext) {
    if (is->mPluginStreamListener->mIsPluginInitJSStream) {
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

// This method is called when there's more data available off the
// network, but it's also called from our data pump when we're feeding
// the plugin data that we already got off the network, but the plugin
// was unable to consume it at the point it arrived. In the case when
// the plugin pump calls this method, the input argument will be null,
// and the length will be the number of bytes available in our
// internal buffer.
NS_IMETHODIMP
nsNPAPIPluginStreamListener::OnDataAvailable(nsIPluginStreamInfo* pluginInfo,
                                          nsIInputStream* input,
                                          PRUint32 length)
{
  if (!mInst || !mInst->IsStarted())
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(mInst);

  // Just in case the caller switches plugin info on us.
  mStreamInfo = pluginInfo;

  const NPPluginFuncs *callbacks = nsnull;
  mInst->GetCallbacks(&callbacks);
  // check out if plugin implements NPP_Write call
  if (!callbacks || !callbacks->write || !length)
    return NS_ERROR_FAILURE; // it'll cancel necko transaction 
  
  if (!mStreamBuffer) {
    // To optimize the mem usage & performance we have to allocate
    // mStreamBuffer here in first ODA when length of data available
    // in input stream is known.  mStreamBuffer will be freed in DTOR.
    // we also have to remember the size of that buff to make safe
    // consecutive Read() calls form input stream into our buff.

    PRUint32 contentLength;
    pluginInfo->GetLength(&contentLength);

    mStreamBufferSize = PR_MAX(length, contentLength);

    // Limit the size of the initial buffer to MAX_PLUGIN_NECKO_BUFFER
    // (16k). This buffer will grow if needed, as in the case where
    // we're getting data faster than the plugin can process it.
    mStreamBufferSize = PR_MIN(mStreamBufferSize, MAX_PLUGIN_NECKO_BUFFER);

    mStreamBuffer = (char*) PR_Malloc(mStreamBufferSize);
    if (!mStreamBuffer)
      return NS_ERROR_OUT_OF_MEMORY;
  }
  
  // prepare NPP_ calls params
  NPP npp;
  mInst->GetNPP(&npp);

  PRInt32 streamPosition;
  pluginInfo->GetStreamOffset(&streamPosition);
  PRInt32 streamOffset = streamPosition;

  if (input) {
    streamOffset += length;

    // Set new stream offset for the next ODA call regardless of how
    // following NPP_Write call will behave we pretend to consume all
    // data from the input stream.  It's possible that current steam
    // position will be overwritten from NPP_RangeRequest call made
    // from NPP_Write, so we cannot call SetStreamOffset after
    // NPP_Write.
    //
    // Note: there is a special case when data flow should be
    // temporarily stopped if NPP_WriteReady returns 0 (bug #89270)
    pluginInfo->SetStreamOffset(streamOffset);

    // set new end in case the content is compressed
    // initial end is less than end of decompressed stream
    // and some plugins (e.g. acrobat) can fail. 
    if ((PRInt32)mNPStream.end < streamOffset)
      mNPStream.end = streamOffset;
  }

  nsresult rv = NS_OK;
  while (NS_SUCCEEDED(rv) && length > 0) {
    if (input && length) {
      if (mStreamBufferSize < mStreamBufferByteCount + length && mIsSuspended) {
        // We're in the ::OnDataAvailable() call that we might get
        // after suspending a request, or we suspended the request
        // from within this ::OnDataAvailable() call while there's
        // still data in the input, and we don't have enough space to
        // store what we got off the network. Reallocate our internal
        // buffer.
        mStreamBufferSize = mStreamBufferByteCount + length;
        char *buf = (char*)PR_Realloc(mStreamBuffer, mStreamBufferSize);
        if (!buf)
          return NS_ERROR_OUT_OF_MEMORY;

        mStreamBuffer = buf;
      }

      PRUint32 bytesToRead =
        PR_MIN(length, mStreamBufferSize - mStreamBufferByteCount);

      PRUint32 amountRead = 0;
      rv = input->Read(mStreamBuffer + mStreamBufferByteCount, bytesToRead,
                       &amountRead);
      NS_ENSURE_SUCCESS(rv, rv);

      if (amountRead == 0) {
        NS_NOTREACHED("input->Read() returns no data, it's almost impossible "
                      "to get here");

        break;
      }

      mStreamBufferByteCount += amountRead;
      length -= amountRead;
    } else {
      // No input, nothing to read. Set length to 0 so that we don't
      // keep iterating through this outer loop any more.

      length = 0;
    }

    // Temporary pointer to the beginning of the data we're writing as
    // we loop and feed the plugin data.
    char *ptrStreamBuffer = mStreamBuffer;

    // it is possible plugin's NPP_Write() returns 0 byte consumed. We
    // use zeroBytesWriteCount to count situation like this and break
    // the loop
    PRInt32 zeroBytesWriteCount = 0;

    // mStreamBufferByteCount tells us how many bytes there are in the
    // buffer. WriteReady returns to us how many bytes the plugin is
    // ready to handle.
    while (mStreamBufferByteCount > 0) {
      PRInt32 numtowrite;
      if (callbacks->writeready) {
        NS_TRY_SAFE_CALL_RETURN(numtowrite, (*callbacks->writeready)(npp, &mNPStream), mInst->fLibrary, mInst);
        NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                       ("NPP WriteReady called: this=%p, npp=%p, "
                        "return(towrite)=%d, url=%s\n",
                        this, npp, numtowrite, mNPStream.url));

        if (!mStreamStarted) {
          // The plugin called NPN_DestroyStream() from within
          // NPP_WriteReady(), kill the stream.

          return NS_BINDING_ABORTED;
        }

        // if WriteReady returned 0, the plugin is not ready to handle
        // the data, suspend the stream (if it isn't already
        // suspended).
        //
        // Also suspend the stream if the stream we're loading is not
        // a javascript: URL load that was initiated during plugin
        // initialization and there currently is such a stream
        // loading. This is done to work around a Windows Media Player
        // plugin bug where it can't deal with being fed data for
        // other streams while it's waiting for data from the
        // javascript: URL loads it requests during
        // initialization. See bug 386493 for more details.

        if (numtowrite <= 0 ||
            (!mIsPluginInitJSStream && PluginInitJSLoadInProgress())) {
          if (!mIsSuspended) {
            rv = SuspendRequest();
          }

          // Break out of the inner loop, but keep going through the
          // outer loop in case there's more data to read from the
          // input stream.

          break;
        }

        numtowrite = PR_MIN(numtowrite, mStreamBufferByteCount);
      } else {
        // if WriteReady is not supported by the plugin, just write
        // the whole buffer
        numtowrite = mStreamBufferByteCount;
      }

      PRInt32 writeCount = 0; // bytes consumed by plugin instance
      NS_TRY_SAFE_CALL_RETURN(writeCount, (*callbacks->write)(npp, &mNPStream, streamPosition, numtowrite, ptrStreamBuffer), mInst->fLibrary, mInst);

      NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
                     ("NPP Write called: this=%p, npp=%p, pos=%d, len=%d, "
                      "buf=%s, return(written)=%d,  url=%s\n",
                      this, npp, streamPosition, numtowrite,
                      ptrStreamBuffer, writeCount, mNPStream.url));

      if (!mStreamStarted) {
        // The plugin called NPN_DestroyStream() from within
        // NPP_Write(), kill the stream.
        return NS_BINDING_ABORTED;
      }

      if (writeCount > 0) {
        NS_ASSERTION(writeCount <= mStreamBufferByteCount,
                     "Plugin read past the end of the available data!");

        writeCount = PR_MIN(writeCount, mStreamBufferByteCount);
        mStreamBufferByteCount -= writeCount;

        streamPosition += writeCount;

        zeroBytesWriteCount = 0;

        if (mStreamBufferByteCount > 0) {
          // This alignment code is most likely bogus, but we'll leave
          // it in for now in case it matters for some plugins on some
          // architectures. Who knows...
          if (writeCount % sizeof(PRWord)) {
            // memmove will take care  about alignment 
            memmove(mStreamBuffer, ptrStreamBuffer + writeCount,
                    mStreamBufferByteCount);
            ptrStreamBuffer = mStreamBuffer;
          } else {
            // if aligned we can use ptrStreamBuffer += to eliminate
            // memmove()
            ptrStreamBuffer += writeCount;
          }
        }
      } else if (writeCount == 0) {
        // if NPP_Write() returns writeCount == 0 lets say 3 times in
        // a row, suspend the request and continue feeding the plugin
        // the data we got so far. Once that data is consumed, we'll
        // resume the request.
        if (mIsSuspended || ++zeroBytesWriteCount == 3) {
          if (!mIsSuspended) {
            rv = SuspendRequest();
          }

          // Break out of the for loop, but keep going through the
          // while loop in case there's more data to read from the
          // input stream.

          break;
        }
      } else {
        // Something's really wrong, kill the stream.
        rv = NS_ERROR_FAILURE;

        break;
      }  
    } // end of inner while loop

    if (mStreamBufferByteCount && mStreamBuffer != ptrStreamBuffer) {
      memmove(mStreamBuffer, ptrStreamBuffer, mStreamBufferByteCount);
    }
  }

  if (streamPosition != streamOffset) {
    // The plugin didn't consume all available data, or consumed some
    // of our cached data while we're pumping cached data. Adjust the
    // plugin info's stream offset to match reality, except if the
    // plugin info's stream offset was set by a re-entering
    // NPN_RequestRead() call.

    PRInt32 postWriteStreamPosition;
    pluginInfo->GetStreamOffset(&postWriteStreamPosition);

    if (postWriteStreamPosition == streamOffset) {
      pluginInfo->SetStreamOffset(streamPosition);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::OnFileAvailable(nsIPluginStreamInfo* pluginInfo, 
                                             const char* fileName)
{
  if (!mInst || !mInst->IsStarted())
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(mInst);

  const NPPluginFuncs *callbacks = nsnull;
  mInst->GetCallbacks(&callbacks);
  if (!callbacks || !callbacks->asfile)
    return NS_ERROR_FAILURE;
  
  NPP npp;
  mInst->GetNPP(&npp);

  PRLibrary* lib = nsnull;
  lib = mInst->fLibrary;

  NS_TRY_SAFE_CALL_VOID((*callbacks->asfile)(npp, &mNPStream, fileName), lib, mInst);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP StreamAsFile called: this=%p, npp=%p, url=%s, file=%s\n",
  this, npp, mNPStream.url, fileName));

  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::OnStopBinding(nsIPluginStreamInfo* pluginInfo, 
                                        nsresult status)
{
  StopDataPump();

  if (NS_FAILED(status)) {
    // The stream was destroyed, or died for some reason. Make sure we
    // cancel the underlying request.
    nsCOMPtr<nsINPAPIPluginStreamInfo> pluginInfoNPAPI =
      do_QueryInterface(mStreamInfo);

    nsIRequest *request;
    if (pluginInfoNPAPI && (request = pluginInfoNPAPI->GetRequest())) {
      request->Cancel(status);
    }
  }

  if (!mInst || !mInst->IsStarted())
    return NS_ERROR_FAILURE;

  // check if the stream is of seekable type and later its destruction
  // see bug 91140    
  nsresult rv = NS_OK;
  if (mStreamType != nsPluginStreamType_Seek) {
    NPReason reason = NPRES_DONE;

    if (NS_FAILED(status))
      reason = NPRES_NETWORK_ERR;   // since the stream failed, we need to tell the plugin that

    rv = CleanUpStream(reason);
  }

  if (rv != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::GetStreamType(nsPluginStreamType *result)
{
  *result = mStreamType;
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::Notify(nsITimer *aTimer)
{
  NS_ASSERTION(aTimer == mDataPumpTimer, "Uh, wrong timer?");

  PRInt32 oldStreamBufferByteCount = mStreamBufferByteCount;

  nsresult rv = OnDataAvailable(mStreamInfo, nsnull, mStreamBufferByteCount);

  if (NS_FAILED(rv)) {
    // We ran into an error, no need to keep firing this timer then.
    aTimer->Cancel();
    return NS_OK;
  }

  if (mStreamBufferByteCount != oldStreamBufferByteCount &&
      ((mStreamStarted && mStreamBufferByteCount < 1024) ||
       mStreamBufferByteCount == 0)) {
    // The plugin read some data and we've got less than 1024 bytes in
    // our buffer (or its empty and the stream is already
    // done). Resume the request so that we get more data off the
    // network.
    ResumeRequest();
    // Necko will pump data now that we've resumed the request.
    StopDataPump();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::StatusLine(const char* line)
{
  mResponseHeaders.Append(line);
  mResponseHeaders.Append('\n');
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginStreamListener::NewResponseHeader(const char* headerName,
                                            const char* headerValue)
{
  mResponseHeaders.Append(headerName);
  mResponseHeaders.Append(": ");
  mResponseHeaders.Append(headerValue);
  mResponseHeaders.Append('\n');
  return NS_OK;
}

nsInstanceStream::nsInstanceStream()
{
  mNext = nsnull;
  mPluginStreamListener = nsnull;
}

nsInstanceStream::~nsInstanceStream()
{
}

NS_IMPL_ISUPPORTS3(nsNPAPIPluginInstance, nsIPluginInstance, nsIScriptablePlugin,
                   nsIPluginInstanceInternal)

nsNPAPIPluginInstance::nsNPAPIPluginInstance(NPPluginFuncs* callbacks,
                                       PRLibrary* aLibrary)
  : fCallbacks(callbacks),
#ifdef XP_MACOSX
#ifdef NP_NO_QUICKDRAW
    mDrawingModel(NPDrawingModelCoreGraphics),
#else
    mDrawingModel(NPDrawingModelQuickDraw),
#endif
#endif
    mWindowless(PR_FALSE),
    mTransparent(PR_FALSE),
    mStarted(PR_FALSE),
    mCached(PR_FALSE),
    mIsJavaPlugin(PR_FALSE),
    mWantsAllNetworkStreams(PR_FALSE),
    mInPluginInitCall(PR_FALSE),
    fLibrary(aLibrary),
    mStreams(nsnull)
{
  NS_ASSERTION(fCallbacks != NULL, "null callbacks");

  // Initialize the NPP structure.

  fNPP.pdata = NULL;
  fNPP.ndata = this;

  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("nsNPAPIPluginInstance ctor: this=%p\n",this));
}

nsNPAPIPluginInstance::~nsNPAPIPluginInstance(void)
{
  PLUGIN_LOG(PLUGIN_LOG_BASIC, ("nsNPAPIPluginInstance dtor: this=%p\n",this));

  // clean the stream list if any
  for (nsInstanceStream *is = mStreams; is != nsnull;) {
    nsInstanceStream * next = is->mNext;
    delete is;
    is = next;
  }
}

PRBool
nsNPAPIPluginInstance::IsStarted(void)
{
  return mStarted;
}

NS_IMETHODIMP nsNPAPIPluginInstance::Initialize(nsIPluginInstancePeer* peer)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Initialize this=%p\n",this));

  return InitializePlugin(peer);
}

NS_IMETHODIMP nsNPAPIPluginInstance::GetPeer(nsIPluginInstancePeer* *resultingPeer)
{
  *resultingPeer = mPeer;
  NS_IF_ADDREF(*resultingPeer);
  
  return NS_OK;
}

NS_IMETHODIMP nsNPAPIPluginInstance::Start(void)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Start this=%p\n",this));

  if (mStarted)
    return NS_OK;

  return InitializePlugin(mPeer); 
}

NS_IMETHODIMP nsNPAPIPluginInstance::Stop(void)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Stop this=%p\n",this));

  NPError error;

  // Make sure the plugin didn't leave popups enabled.
  if (mPopupStates.Count() > 0) {
    nsCOMPtr<nsPIDOMWindow> window = GetDOMWindow();

    if (window) {
      window->PopPopupControlState(openAbused);
    }
  }

  if (!mStarted) {
    // Break our cycle with the peer that owns us.
    mPeer = nsnull;
    return NS_OK;
  }

  // If there's code from this plugin instance on the stack, delay the
  // destroy.
  if (PluginDestructionGuard::DelayDestroy(this)) {
    return NS_OK;
  }

  // Make sure we lock while we're writing to mStarted after we've
  // started as other threads might be checking that inside a lock.
  EnterAsyncPluginThreadCallLock();
  mStarted = PR_FALSE;
  ExitAsyncPluginThreadCallLock();

  OnPluginDestroy(&fNPP);

  if (fCallbacks->destroy == NULL) {
    // Break our cycle with the peer that owns us.
    mPeer = nsnull;
    return NS_ERROR_FAILURE;
  }

  NPSavedData *sdata = 0;

  // clean up open streams
  for (nsInstanceStream *is = mStreams; is != nsnull;) {
    nsNPAPIPluginStreamListener * listener = is->mPluginStreamListener;

    nsInstanceStream *next = is->mNext;
    delete is;
    is = next;
    mStreams = is;

    // Clean up our stream after removing it from the list because 
    // it may be released and destroyed at this point.
    if (listener)
      listener->CleanUpStream(NPRES_USER_BREAK);
  }

  NS_TRY_SAFE_CALL_RETURN(error, (*fCallbacks->destroy)(&fNPP, &sdata), fLibrary, this);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP Destroy called: this=%p, npp=%p, return=%d\n", this, &fNPP, error));

  nsJSNPRuntime::OnPluginDestroy(&fNPP);

  // Break our cycle with the peer that owns us.
  mPeer = nsnull;

  if (error != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}

already_AddRefed<nsPIDOMWindow>
nsNPAPIPluginInstance::GetDOMWindow()
{
  nsCOMPtr<nsPIPluginInstancePeer> pp (do_QueryInterface(mPeer));
  if (!pp)
    return nsnull;

  nsCOMPtr<nsIPluginInstanceOwner> owner;
  pp->GetOwner(getter_AddRefs(owner));
  if (!owner)
    return nsnull;

  nsCOMPtr<nsIDocument> doc;
  owner->GetDocument(getter_AddRefs(doc));
  if (!doc)
    return nsnull;

  nsPIDOMWindow *window = doc->GetWindow();
  NS_IF_ADDREF(window);

  return window;
}

nsresult nsNPAPIPluginInstance::InitializePlugin(nsIPluginInstancePeer* peer)
{
  NS_ENSURE_ARG_POINTER(peer);
 
  nsCOMPtr<nsIPluginTagInfo2> taginfo = do_QueryInterface(peer);
  NS_ENSURE_TRUE(taginfo, NS_ERROR_NO_INTERFACE);
  
  PluginDestructionGuard guard(this);

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
    // crafted specially to be directly behind the arrays from GetAttributes()
    // with a null entry as a separator. This is for 4.x backwards compatibility!
    // see bug 111008 for details
    if (tagtype != nsPluginTagType_Embed) {
      PRUint16 pcount = 0;
      const char* const* pnames = nsnull;
      const char* const* pvalues = nsnull;    
      if (NS_SUCCEEDED(taginfo->GetParameters(pcount, pnames, pvalues))) {
        NS_ASSERTION(!values[count], "attribute/parameter array not setup correctly for NPAPI plugins");
        if (pcount)
          count += ++pcount; // if it's all setup correctly, then all we need is to
                             // change the count (attrs + PARAM/blank + params)
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
            // we cannot just *val=0, it won't be free properly in such case
            val[0] = '0';
            val[1] = 0;
          }
          break;
        }
      }
    }
  }

  mIsJavaPlugin = nsPluginHostImpl::IsJavaMIMEType(mimetype);

  // Assign mPeer now and mark this instance as started before calling NPP_New 
  // because the plugin may call other NPAPI functions, like NPN_GetURLNotify,
  // that assume these are set before returning. If the plugin returns failure,
  // we'll clear them out below.
  mPeer = peer;
  mStarted = PR_TRUE;

  PRBool oldVal = mInPluginInitCall;
  mInPluginInitCall = PR_TRUE;

  NS_TRY_SAFE_CALL_RETURN(error, (*fCallbacks->newp)((char*)mimetype, &fNPP, (PRUint16)mode, count, (char**)names, (char**)values, NULL), fLibrary,this);

  mInPluginInitCall = oldVal;

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP New called: this=%p, npp=%p, mime=%s, mode=%d, argc=%d, return=%d\n",
  this, &fNPP, mimetype, mode, count, error));

  if (error != NPERR_NO_ERROR) {
    // since the plugin returned failure, these should not be set
    mPeer = nsnull;
    mStarted = PR_FALSE;

    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsNPAPIPluginInstance::Destroy(void)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Destroy this=%p\n",this));

  // destruction is handled in the Stop call
  return NS_OK;
}

NS_IMETHODIMP nsNPAPIPluginInstance::SetWindow(nsPluginWindow* window)
{
  // XXX NPAPI plugins don't want a SetWindow(NULL).
  if (!window || !mStarted)
    return NS_OK;

  NPError error;

#if defined (MOZ_WIDGET_GTK2)
  // bug 108347, flash plugin on linux doesn't like window->width <=
  // 0, but Java needs wants this call.
  if (!mIsJavaPlugin && window->type == nsPluginWindowType_Window &&
      (window->width <= 0 || window->height <= 0)) {
    return NS_OK;
  }
#endif // MOZ_WIDGET

  if (fCallbacks->setwindow) {
    PluginDestructionGuard guard(this);

    // XXX Turns out that NPPluginWindow and NPWindow are structurally
    // identical (on purpose!), so there's no need to make a copy.

    PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::SetWindow (about to call it) this=%p\n",this));

    PRBool oldVal = mInPluginInitCall;
    mInPluginInitCall = PR_TRUE;

    NS_TRY_SAFE_CALL_RETURN(error, (*fCallbacks->setwindow)(&fNPP, (NPWindow*)window), fLibrary, this);

    mInPluginInitCall = oldVal;

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

/* NOTE: the caller must free the stream listener */
// Create a normal stream, one without a urlnotify callback
NS_IMETHODIMP nsNPAPIPluginInstance::NewStream(nsIPluginStreamListener** listener)
{
  return NewNotifyStream(listener, nsnull, PR_FALSE, nsnull);
}

// Create a stream that will notify when complete
nsresult nsNPAPIPluginInstance::NewNotifyStream(nsIPluginStreamListener** listener, 
                                                void* notifyData,
                                                PRBool aCallNotify,
                                                const char* aURL)
{
  nsNPAPIPluginStreamListener* stream = new nsNPAPIPluginStreamListener(this, notifyData, aURL);
  NS_ENSURE_TRUE(stream, NS_ERROR_OUT_OF_MEMORY);

  // add it to the list
  nsInstanceStream * is = new nsInstanceStream();
  NS_ENSURE_TRUE(is, NS_ERROR_OUT_OF_MEMORY);

  is->mNext = mStreams;
  is->mPluginStreamListener = stream;
  mStreams = is;
  stream->SetCallNotify(aCallNotify); // set flag in stream to call URLNotify

  NS_ADDREF(stream); // Stabilize
    
  nsresult res = stream->QueryInterface(kIPluginStreamListenerIID, (void**)listener);

  // Destabilize and avoid leaks. Avoid calling delete <interface pointer>
  NS_RELEASE(stream);

  return res;
}

NS_IMETHODIMP nsNPAPIPluginInstance::Print(nsPluginPrint* platformPrint)
{
  NS_ENSURE_TRUE(platformPrint, NS_ERROR_NULL_POINTER);

  PluginDestructionGuard guard(this);

  NPPrint* thePrint = (NPPrint *)platformPrint;

  // to be compatible with the older SDK versions and to match what
  // NPAPI and other browsers do, overwrite |window.type| field with one
  // more copy of |platformPrint|. See bug 113264
  if (fCallbacks) {
    PRUint16 sdkmajorversion = (fCallbacks->version & 0xff00)>>8;
    PRUint16 sdkminorversion = fCallbacks->version & 0x00ff;
    if ((sdkmajorversion == 0) && (sdkminorversion < 11)) { 
      // Let's copy platformPrint bytes over to where it was supposed to be 
      // in older versions -- four bytes towards the beginning of the struct
      // but we should be careful about possible misalignments
      if (sizeof(NPWindowType) >= sizeof(void *)) {
        void* source = thePrint->print.embedPrint.platformPrint; 
        void** destination = (void **)&(thePrint->print.embedPrint.window.type); 
        *destination = source;
      } 
      else 
        NS_ASSERTION(PR_FALSE, "Incompatible OS for assignment");
    }
  }

  if (fCallbacks->print)
      NS_TRY_SAFE_CALL_VOID((*fCallbacks->print)(&fNPP, thePrint), fLibrary, this);

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

NS_IMETHODIMP nsNPAPIPluginInstance::HandleEvent(nsPluginEvent* event, PRBool* handled)
{
  if (!mStarted)
    return NS_OK;

  if (!event)
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(this);

  PRInt16 result = 0;
  
  if (fCallbacks->event) {
#ifdef XP_MACOSX
    result = (*fCallbacks->event)(&fNPP, (void*)event->event);

#elif defined(XP_WIN) || defined(XP_OS2)
      NPEvent npEvent;
      npEvent.event = event->event;
      npEvent.wParam = event->wParam;
      npEvent.lParam = event->lParam;

      NS_TRY_SAFE_CALL_RETURN(result, (*fCallbacks->event)(&fNPP, (void*)&npEvent), fLibrary, this);

#else // MOZ_X11 or other
      result = (*fCallbacks->event)(&fNPP, (void*)&event->event);
#endif

      NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
      ("NPP HandleEvent called: this=%p, npp=%p, event=%d, return=%d\n", 
      this, &fNPP, event->event, result));

      *handled = result;
    }

  return NS_OK;
}

nsresult nsNPAPIPluginInstance::GetValueInternal(NPPVariable variable, void* value)
{
  nsresult  res = NS_OK;
  if (fCallbacks->getvalue && mStarted) {
    PluginDestructionGuard guard(this);

    NS_TRY_SAFE_CALL_RETURN(res, (*fCallbacks->getvalue)(&fNPP, variable, value), fLibrary, this);
    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP GetValue called: this=%p, npp=%p, var=%d, value=%d, return=%d\n", 
    this, &fNPP, variable, value, res));

#ifdef XP_OS2
    /* Query interface for legacy Flash plugin */
    if (res == NS_OK && variable == NPPVpluginScriptableInstance) {
      nsCOMPtr<nsILegacyPluginWrapperOS2> wrapper =
               do_GetService(NS_LEGACY_PLUGIN_WRAPPER_CONTRACTID, &res);
      if (res == NS_OK) {
        nsIID *iid = nsnull; 
        res = (*fCallbacks->getvalue)(&fNPP, NPPVpluginScriptableIID, (void *)&iid);
        if (res == NS_OK)
          res = wrapper->MaybeWrap(*iid, *(nsISupports**)value, (nsISupports**)value);
      }
    }
#endif
  }

  return res;
}

NS_IMETHODIMP nsNPAPIPluginInstance::GetValue(nsPluginInstanceVariable variable, void *value)
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

#ifdef XP_MACOSX
    case nsPluginInstanceVariable_DrawingModel:
      *(NPDrawingModel*)value = mDrawingModel;
      break;
#endif

    default:
      res = GetValueInternal((NPPVariable)variable, value);
  }

  return res;
}

nsresult nsNPAPIPluginInstance::GetNPP(NPP* aNPP) 
{
  if (aNPP)
    *aNPP = &fNPP;
  else
    return NS_ERROR_NULL_POINTER;

  return NS_OK;
}

nsresult nsNPAPIPluginInstance::GetCallbacks(const NPPluginFuncs ** aCallbacks)
{
  if (aCallbacks)
    *aCallbacks = fCallbacks;
  else
    return NS_ERROR_NULL_POINTER;

  return NS_OK;
}

NPError nsNPAPIPluginInstance::SetWindowless(PRBool aWindowless)
{
  mWindowless = aWindowless;
  return NPERR_NO_ERROR;
}

NPError nsNPAPIPluginInstance::SetTransparent(PRBool aTransparent)
{
  mTransparent = aTransparent;
  return NPERR_NO_ERROR;
}

NPError nsNPAPIPluginInstance::SetWantsAllNetworkStreams(PRBool aWantsAllNetworkStreams)
{
  mWantsAllNetworkStreams = aWantsAllNetworkStreams;
  return NPERR_NO_ERROR;
}

#ifdef XP_MACOSX
void nsNPAPIPluginInstance::SetDrawingModel(NPDrawingModel aModel)
{
  mDrawingModel = aModel;
}

NPDrawingModel nsNPAPIPluginInstance::GetDrawingModel()
{
  return mDrawingModel;
}
#endif

/* readonly attribute nsQIResult scriptablePeer; */
NS_IMETHODIMP nsNPAPIPluginInstance::GetScriptablePeer(void * *aScriptablePeer)
{
  if (!aScriptablePeer)
    return NS_ERROR_NULL_POINTER;

  *aScriptablePeer = nsnull;
  return GetValueInternal(NPPVpluginScriptableInstance, aScriptablePeer);
}

/* readonly attribute nsIIDPtr scriptableInterface; */
NS_IMETHODIMP nsNPAPIPluginInstance::GetScriptableInterface(nsIID * *aScriptableInterface)
{
  if (!aScriptableInterface)
    return NS_ERROR_NULL_POINTER;

  *aScriptableInterface = nsnull;
  return GetValueInternal(NPPVpluginScriptableIID, (void*)aScriptableInterface);
}

JSObject *
nsNPAPIPluginInstance::GetJSObject(JSContext *cx)
{
  JSObject *obj = nsnull;
  NPObject *npobj = nsnull;

  nsresult rv = GetValueInternal(NPPVpluginScriptableNPObject, &npobj);

  if (NS_SUCCEEDED(rv) && npobj) {
    obj = nsNPObjWrapper::GetNewOrUsed(&fNPP, cx, npobj);

    _releaseobject(npobj);
  }

  return obj;
}

void
nsNPAPIPluginInstance::DefineJavaProperties()
{
  NPObject *plugin_obj = nsnull;

  // The dummy Java plugin's scriptable object is what we want to
  // expose as window.Packages. And Window.Packages.java will be
  // exposed as window.java.

  // Get the scriptable plugin object.
  nsresult rv = GetValueInternal(NPPVpluginScriptableNPObject, &plugin_obj);

  if (NS_FAILED(rv) || !plugin_obj) {
    return;
  }

  // Get the NPObject wrapper for window.
  NPObject *window_obj = _getwindowobject(&fNPP);

  if (!window_obj) {
    _releaseobject(plugin_obj);

    return;
  }

  NPIdentifier java_id = _getstringidentifier("java");
  NPIdentifier packages_id = _getstringidentifier("Packages");

  NPObject *java_obj = nsnull;
  NPVariant v;
  OBJECT_TO_NPVARIANT(plugin_obj, v);

  // Define the properties.

  bool ok = _setproperty(&fNPP, window_obj, packages_id, &v);
  if (ok) {
    ok = _getproperty(&fNPP, plugin_obj, java_id, &v);

    if (ok && NPVARIANT_IS_OBJECT(v)) {
      // Set java_obj so that we properly release it at the end of
      // this function.
      java_obj = NPVARIANT_TO_OBJECT(v);

      ok = _setproperty(&fNPP, window_obj, java_id, &v);
    }
  }

  _releaseobject(window_obj);
  _releaseobject(plugin_obj);
  _releaseobject(java_obj);
}

nsresult
nsNPAPIPluginInstance::GetFormValue(nsAString& aValue)
{
  aValue.Truncate();

  char *value = nsnull;
  nsresult rv = GetValueInternal(NPPVformValue, &value);

  if (NS_SUCCEEDED(rv) && value) {
    CopyUTF8toUTF16(value, aValue);

    // NPPVformValue allocates with NPN_MemAlloc(), which uses
    // nsMemory.
    nsMemory::Free(value);
  }

  return NS_OK;
}

void
nsNPAPIPluginInstance::PushPopupsEnabledState(PRBool aEnabled)
{
  nsCOMPtr<nsPIDOMWindow> window = GetDOMWindow();
  if (!window)
    return;

  PopupControlState oldState =
    window->PushPopupControlState(aEnabled ? openAllowed : openAbused,
                                  PR_TRUE);

  if (!mPopupStates.AppendElement(NS_INT32_TO_PTR(oldState))) {
    // Appending to our state stack failed, push what we just popped.
    window->PopPopupControlState(oldState);
  }
}

void
nsNPAPIPluginInstance::PopPopupsEnabledState()
{
  PRInt32 last = mPopupStates.Count() - 1;

  if (last < 0) {
    // Nothing to pop.
    return;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetDOMWindow();
  if (!window)
    return;

  PopupControlState oldState =
    (PopupControlState)NS_PTR_TO_INT32(mPopupStates[last]);

  window->PopPopupControlState(oldState);

  mPopupStates.RemoveElementAt(last);
}

PRUint16
nsNPAPIPluginInstance::GetPluginAPIVersion()
{
  return fCallbacks->version;
}
