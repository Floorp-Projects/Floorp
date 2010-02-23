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
#include "nsPluginHost.h"
#include "nsPluginSafety.h"
#include "nsPluginLogging.h"
#include "nsIPrivateBrowsingService.h"

#include "nsIDocument.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsDirectoryServiceDefs.h"

#include "nsJSNPRuntime.h"

using namespace mozilla::plugins::parent;
using mozilla::TimeStamp;

static NS_DEFINE_IID(kIPluginStreamListenerIID, NS_IPLUGINSTREAMLISTENER_IID);

// nsPluginStreamToFile
// --------------------
// Used to handle NPN_NewStream() - writes the stream as received by the plugin
// to a file and at completion (NPN_DestroyStream), tells the browser to load it into
// a plugin-specified target

static NS_DEFINE_IID(kIOutputStreamIID, NS_IOUTPUTSTREAM_IID);

class nsPluginStreamToFile : public nsIOutputStream
{
public:
  nsPluginStreamToFile(const char* target, nsIPluginInstanceOwner* owner);
  virtual ~nsPluginStreamToFile();

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTPUTSTREAM
protected:
  char* mTarget;
  nsCString mFileURL;
  nsCOMPtr<nsILocalFile> mTempFile;
  nsCOMPtr<nsIOutputStream> mOutputStream;
  nsIPluginInstanceOwner* mOwner;
};

NS_IMPL_ISUPPORTS1(nsPluginStreamToFile, nsIOutputStream)

nsPluginStreamToFile::nsPluginStreamToFile(const char* target,
                                           nsIPluginInstanceOwner* owner)
: mTarget(PL_strdup(target)),
mOwner(owner)
{
  nsresult rv;
  nsCOMPtr<nsIFile> pluginTmp;
  rv = NS_GetSpecialDirectory(NS_OS_TEMP_DIR, getter_AddRefs(pluginTmp));
  if (NS_FAILED(rv)) return;
  
  mTempFile = do_QueryInterface(pluginTmp, &rv);
  if (NS_FAILED(rv)) return;
  
  // need to create a file with a unique name - use target as the basis
  rv = mTempFile->AppendNative(nsDependentCString(target));
  if (NS_FAILED(rv)) return;
  
  // Yes, make it unique.
  rv = mTempFile->CreateUnique(nsIFile::NORMAL_FILE_TYPE, 0700); 
  if (NS_FAILED(rv)) return;
  
  // create the file
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(mOutputStream), mTempFile, -1, 00600);
  if (NS_FAILED(rv))
    return;
	
  // construct the URL we'll use later in calls to GetURL()
  NS_GetURLSpecFromFile(mTempFile, mFileURL);
  
#ifdef NS_DEBUG
  printf("File URL = %s\n", mFileURL.get());
#endif
}

nsPluginStreamToFile::~nsPluginStreamToFile()
{
  // should we be deleting mTempFile here?
  if (nsnull != mTarget)
    PL_strfree(mTarget);
}

NS_IMETHODIMP
nsPluginStreamToFile::Flush()
{
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamToFile::Write(const char* aBuf, PRUint32 aCount,
                            PRUint32 *aWriteCount)
{
  mOutputStream->Write(aBuf, aCount, aWriteCount);
  mOutputStream->Flush();
  mOwner->GetURL(mFileURL.get(), mTarget, nsnull, nsnull, 0);
  
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamToFile::WriteFrom(nsIInputStream *inStr, PRUint32 count,
                                PRUint32 *_retval)
{
  NS_NOTREACHED("WriteFrom");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginStreamToFile::WriteSegments(nsReadSegmentFun reader, void * closure,
                                    PRUint32 count, PRUint32 *_retval)
{
  NS_NOTREACHED("WriteSegments");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginStreamToFile::IsNonBlocking(PRBool *aNonBlocking)
{
  *aNonBlocking = PR_FALSE;
  return NS_OK;
}

NS_IMETHODIMP
nsPluginStreamToFile::Close(void)
{
  mOutputStream->Close();
  mOwner->GetURL(mFileURL.get(), mTarget, nsnull, nsnull, 0);
  return NS_OK;
}

// end of nsPluginStreamToFile

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
    mStreamType(NP_NORMAL),
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

  if (!mInst || !mInst->IsRunning())
    return rv;

  PluginDestructionGuard guard(mInst);

  const NPPluginFuncs *callbacks = nsnull;
  mInst->GetCallbacks(&callbacks);
  if (!callbacks)
    return rv;

  NPP npp;
  mInst->GetNPP(&npp);

  if (mStreamStarted && callbacks->destroystream) {
    NPPAutoPusher nppPusher(npp);

    PluginLibrary* lib = nsnull;
    lib = mInst->mLibrary;
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
  if (!mCallNotify || !mInst || !mInst->IsRunning())
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

    NS_TRY_SAFE_CALL_VOID((*callbacks->urlnotify)(npp, mNotifyURL, reason, mNotifyData), mInst->mLibrary, mInst);

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

  if (!callbacks || !mInst->IsRunning())
    return NS_ERROR_FAILURE;

  PRBool seekable;
  char* contentType;
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

  NPPAutoPusher nppPusher(npp);

  NS_TRY_SAFE_CALL_RETURN(error, (*callbacks->newstream)(npp, (char*)contentType, &mNPStream, seekable, &streamType), mInst->mLibrary, mInst);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP NewStream called: this=%p, npp=%p, mime=%s, seek=%d, type=%d, return=%d, url=%s\n",
  this, npp, (char *)contentType, seekable, streamType, error, mNPStream.url));

  if (error != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;

  switch(streamType)
  {
    case NP_NORMAL:
      mStreamType = NP_NORMAL; 
      break;
    case NP_ASFILEONLY:
      mStreamType = NP_ASFILEONLY; 
      break;
    case NP_ASFILE:
      mStreamType = NP_ASFILE; 
      break;
    case NP_SEEK:
      mStreamType = NP_SEEK; 
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
  if (!mInst || !mInst->IsRunning())
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

    mStreamBufferSize = NS_MAX(length, contentLength);

    // Limit the size of the initial buffer to MAX_PLUGIN_NECKO_BUFFER
    // (16k). This buffer will grow if needed, as in the case where
    // we're getting data faster than the plugin can process it.
    mStreamBufferSize = NS_MIN(mStreamBufferSize,
                               PRUint32(MAX_PLUGIN_NECKO_BUFFER));

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
        NS_MIN(length, mStreamBufferSize - mStreamBufferByteCount);

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
        NPPAutoPusher nppPusher(npp);

        NS_TRY_SAFE_CALL_RETURN(numtowrite, (*callbacks->writeready)(npp, &mNPStream), mInst->mLibrary, mInst);
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

        numtowrite = NS_MIN(numtowrite, mStreamBufferByteCount);
      } else {
        // if WriteReady is not supported by the plugin, just write
        // the whole buffer
        numtowrite = mStreamBufferByteCount;
      }

      NPPAutoPusher nppPusher(npp);

      PRInt32 writeCount = 0; // bytes consumed by plugin instance
      NS_TRY_SAFE_CALL_RETURN(writeCount, (*callbacks->write)(npp, &mNPStream, streamPosition, numtowrite, ptrStreamBuffer), mInst->mLibrary, mInst);

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

        writeCount = NS_MIN(writeCount, mStreamBufferByteCount);
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
  if (!mInst || !mInst->IsRunning())
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(mInst);

  const NPPluginFuncs *callbacks = nsnull;
  mInst->GetCallbacks(&callbacks);
  if (!callbacks || !callbacks->asfile)
    return NS_ERROR_FAILURE;
  
  NPP npp;
  mInst->GetNPP(&npp);

  PluginLibrary* lib = nsnull;
  lib = mInst->mLibrary;

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

  if (!mInst || !mInst->IsRunning())
    return NS_ERROR_FAILURE;

  // check if the stream is of seekable type and later its destruction
  // see bug 91140    
  nsresult rv = NS_OK;
  if (mStreamType != NP_SEEK) {
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
nsNPAPIPluginStreamListener::GetStreamType(PRInt32 *result)
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

NS_IMPL_ISUPPORTS1(nsNPAPIPluginInstance, nsIPluginInstance)

nsNPAPIPluginInstance::nsNPAPIPluginInstance(NPPluginFuncs* callbacks,
                                             PluginLibrary* aLibrary)
  : mCallbacks(callbacks),
#ifdef XP_MACOSX
#ifdef NP_NO_QUICKDRAW
    mDrawingModel(NPDrawingModelCoreGraphics),
#else
    mDrawingModel(NPDrawingModelQuickDraw),
#endif
#endif
    mWindowless(PR_FALSE),
    mWindowlessLocal(PR_FALSE),
    mTransparent(PR_FALSE),
    mRunning(PR_FALSE),
    mCached(PR_FALSE),
    mWantsAllNetworkStreams(PR_FALSE),
    mInPluginInitCall(PR_FALSE),
    mLibrary(aLibrary),
    mStreams(nsnull),
    mMIMEType(nsnull),
    mOwner(nsnull),
    mCurrentPluginEvent(nsnull)
{
  NS_ASSERTION(mCallbacks != NULL, "null callbacks");

  // Initialize the NPP structure.

  mNPP.pdata = NULL;
  mNPP.ndata = this;

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

  if (mMIMEType) {
    PR_Free((void *)mMIMEType);
    mMIMEType = nsnull;
  }
}

PRBool
nsNPAPIPluginInstance::IsRunning()
{
  return mRunning;
}

TimeStamp
nsNPAPIPluginInstance::LastStopTime()
{
  return mStopTime;
}

NS_IMETHODIMP nsNPAPIPluginInstance::Initialize(nsIPluginInstanceOwner* aOwner, const char* aMIMEType)
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Initialize this=%p\n",this));

  mOwner = aOwner;

  if (aMIMEType) {
    mMIMEType = (char*)PR_Malloc(PL_strlen(aMIMEType) + 1);

    if (mMIMEType)
      PL_strcpy(mMIMEType, aMIMEType);
  }

  return InitializePlugin();
}

NS_IMETHODIMP nsNPAPIPluginInstance::Start()
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Start this=%p\n",this));

  if (mRunning)
    return NS_OK;

  return InitializePlugin();
}

NS_IMETHODIMP nsNPAPIPluginInstance::Stop()
{
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::Stop this=%p\n",this));

  NPError error;

  // Make sure the plugin didn't leave popups enabled.
  if (mPopupStates.Length() > 0) {
    nsCOMPtr<nsPIDOMWindow> window = GetDOMWindow();

    if (window) {
      window->PopPopupControlState(openAbused);
    }
  }

  if (!mRunning) {
    return NS_OK;
  }

  // clean up all outstanding timers
  for (PRUint32 i = mTimers.Length(); i > 0; i--)
    UnscheduleTimer(mTimers[i - 1]->id);

  // If there's code from this plugin instance on the stack, delay the
  // destroy.
  if (PluginDestructionGuard::DelayDestroy(this)) {
    return NS_OK;
  }

  // Make sure we lock while we're writing to mRunning after we've
  // started as other threads might be checking that inside a lock.
  EnterAsyncPluginThreadCallLock();
  mRunning = PR_FALSE;
  mStopTime = TimeStamp::Now();
  ExitAsyncPluginThreadCallLock();

  OnPluginDestroy(&mNPP);

  if (mCallbacks->destroy == NULL) {
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

  NS_TRY_SAFE_CALL_RETURN(error, (*mCallbacks->destroy)(&mNPP, &sdata), mLibrary, this);

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP Destroy called: this=%p, npp=%p, return=%d\n", this, &mNPP, error));

  nsJSNPRuntime::OnPluginDestroy(&mNPP);

  if (error != NPERR_NO_ERROR)
    return NS_ERROR_FAILURE;
  else
    return NS_OK;
}

already_AddRefed<nsPIDOMWindow>
nsNPAPIPluginInstance::GetDOMWindow()
{
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
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

nsresult
nsNPAPIPluginInstance::GetTagType(nsPluginTagType *result)
{
  if (mOwner) {
    nsCOMPtr<nsIPluginTagInfo> tinfo(do_QueryInterface(mOwner));
    if (tinfo)
      return tinfo->GetTagType(result);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::GetAttributes(PRUint16& n, const char*const*& names,
                                     const char*const*& values)
{
  if (mOwner) {
    nsCOMPtr<nsIPluginTagInfo> tinfo(do_QueryInterface(mOwner));
    if (tinfo)
      return tinfo->GetAttributes(n, names, values);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::GetParameters(PRUint16& n, const char*const*& names,
                                     const char*const*& values)
{
  if (mOwner) {
    nsCOMPtr<nsIPluginTagInfo> tinfo(do_QueryInterface(mOwner));
    if (tinfo)
      return tinfo->GetParameters(n, names, values);
  }

  return NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::GetMode(PRInt32 *result)
{
  if (mOwner)
    return mOwner->GetMode(result);
  else
    return NS_ERROR_FAILURE;
}

nsresult
nsNPAPIPluginInstance::InitializePlugin()
{ 
  PluginDestructionGuard guard(this);

  PRUint16 count = 0;
  const char* const* names = nsnull;
  const char* const* values = nsnull;
  nsPluginTagType tagtype;
  nsresult rv = GetTagType(&tagtype);
  if (NS_SUCCEEDED(rv)) {
    // Note: If we failed to get the tag type, we may be a full page plugin, so no arguments
    rv = GetAttributes(count, names, values);
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
      if (NS_SUCCEEDED(GetParameters(pcount, pnames, pvalues))) {
        NS_ASSERTION(!values[count], "attribute/parameter array not setup correctly for NPAPI plugins");
        if (pcount)
          count += ++pcount; // if it's all setup correctly, then all we need is to
                             // change the count (attrs + PARAM/blank + params)
      }
    }
  }

  // XXX Note that the NPPluginType_* enums were crafted to be
  // backward compatible...
  
  PRInt32       mode;
  const char*   mimetype;
  NPError       error = NPERR_GENERIC_ERROR;

  GetMode(&mode);
  GetMIMEType(&mimetype);

  // Some older versions of Flash have a bug in them
  // that causes the stack to become currupt if we
  // pass swliveconnect=1 in the NPP_NewProc arrays.
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

  // Mark this instance as running before calling NPP_New because the plugin may
  // call other NPAPI functions, like NPN_GetURLNotify, that assume this is set
  // before returning. If the plugin returns failure, we'll clear it out below.
  mRunning = PR_TRUE;

  PRBool oldVal = mInPluginInitCall;
  mInPluginInitCall = PR_TRUE;

  // Need this on the stack before calling NPP_New otherwise some callbacks that
  // the plugin may make could fail (NPN_HasProperty, for example).
  NPPAutoPusher autopush(&mNPP);
  nsresult newResult = mLibrary->NPP_New((char*)mimetype, &mNPP, (PRUint16)mode, count, (char**)names, (char**)values, NULL, &error);
  if (NS_FAILED(newResult)) {
    mRunning = PR_FALSE;
    return newResult;
  }

  mInPluginInitCall = oldVal;

  NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
  ("NPP New called: this=%p, npp=%p, mime=%s, mode=%d, argc=%d, return=%d\n",
  this, &mNPP, mimetype, mode, count, error));

  if (error != NPERR_NO_ERROR) {
    mRunning = PR_FALSE;
    return NS_ERROR_FAILURE;
  }
  
  return NS_OK;
}

NS_IMETHODIMP nsNPAPIPluginInstance::SetWindow(NPWindow* window)
{
  // NPAPI plugins don't want a SetWindow(NULL).
  if (!window || !mRunning)
    return NS_OK;

#if defined(MOZ_WIDGET_GTK2)
  // bug 108347, flash plugin on linux doesn't like window->width <=
  // 0, but Java needs wants this call.
  if (!nsPluginHost::IsJavaMIMEType(mMIMEType) && window->type == NPWindowTypeWindow &&
      (window->width <= 0 || window->height <= 0)) {
    return NS_OK;
  }
#endif

  if (mCallbacks->setwindow) {
    PluginDestructionGuard guard(this);

    // XXX Turns out that NPPluginWindow and NPWindow are structurally
    // identical (on purpose!), so there's no need to make a copy.

    PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance::SetWindow (about to call it) this=%p\n",this));

    PRBool oldVal = mInPluginInitCall;
    mInPluginInitCall = PR_TRUE;

    NPPAutoPusher nppPusher(&mNPP);

    NPError error;
    NS_TRY_SAFE_CALL_RETURN(error, (*mCallbacks->setwindow)(&mNPP, (NPWindow*)window), mLibrary, this);

    mInPluginInitCall = oldVal;

    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP SetWindow called: this=%p, [x=%d,y=%d,w=%d,h=%d], clip[t=%d,b=%d,l=%d,r=%d], return=%d\n",
    this, window->x, window->y, window->width, window->height,
    window->clipRect.top, window->clipRect.bottom, window->clipRect.left, window->clipRect.right, error));
  }
  return NS_OK;
}

/* NOTE: the caller must free the stream listener */
// Create a normal stream, one without a urlnotify callback
NS_IMETHODIMP
nsNPAPIPluginInstance::NewStreamToPlugin(nsIPluginStreamListener** listener)
{
  return NewNotifyStream(listener, nsnull, PR_FALSE, nsnull);
}

NS_IMETHODIMP
nsNPAPIPluginInstance::NewStreamFromPlugin(const char* type, const char* target,
                                           nsIOutputStream* *result)
{
  nsPluginStreamToFile* stream = new nsPluginStreamToFile(target, mOwner);
  if (!stream)
    return NS_ERROR_OUT_OF_MEMORY;

  return stream->QueryInterface(kIOutputStreamIID, (void**)result);
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

NS_IMETHODIMP nsNPAPIPluginInstance::Print(NPPrint* platformPrint)
{
  NS_ENSURE_TRUE(platformPrint, NS_ERROR_NULL_POINTER);

  PluginDestructionGuard guard(this);

  NPPrint* thePrint = (NPPrint *)platformPrint;

  // to be compatible with the older SDK versions and to match what
  // NPAPI and other browsers do, overwrite |window.type| field with one
  // more copy of |platformPrint|. See bug 113264
  PRUint16 sdkmajorversion = (mCallbacks->version & 0xff00)>>8;
  PRUint16 sdkminorversion = mCallbacks->version & 0x00ff;
  if ((sdkmajorversion == 0) && (sdkminorversion < 11)) {
    // Let's copy platformPrint bytes over to where it was supposed to be
    // in older versions -- four bytes towards the beginning of the struct
    // but we should be careful about possible misalignments
    if (sizeof(NPWindowType) >= sizeof(void *)) {
      void* source = thePrint->print.embedPrint.platformPrint;
      void** destination = (void **)&(thePrint->print.embedPrint.window.type);
      *destination = source;
    } else {
      NS_ERROR("Incompatible OS for assignment");
    }
  }

  if (mCallbacks->print)
      NS_TRY_SAFE_CALL_VOID((*mCallbacks->print)(&mNPP, thePrint), mLibrary, this);

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

NS_IMETHODIMP nsNPAPIPluginInstance::HandleEvent(void* event, PRBool* handled)
{
  if (!mRunning)
    return NS_OK;

  if (!event)
    return NS_ERROR_FAILURE;

  PluginDestructionGuard guard(this);

  PRInt16 result = 0;
  
  if (mCallbacks->event) {
    mCurrentPluginEvent = event;
#if defined(XP_WIN) || defined(XP_OS2)
    NS_TRY_SAFE_CALL_RETURN(result, (*mCallbacks->event)(&mNPP, event), mLibrary, this);
#else
    result = (*mCallbacks->event)(&mNPP, event);
#endif
    NPP_PLUGIN_LOG(PLUGIN_LOG_NOISY,
      ("NPP HandleEvent called: this=%p, npp=%p, event=%p, return=%d\n", 
      this, &mNPP, event, result));

    *handled = result;
    mCurrentPluginEvent = nsnull;
  }

  return NS_OK;
}

NS_IMETHODIMP nsNPAPIPluginInstance::GetValueFromPlugin(NPPVariable variable, void* value)
{
#if (MOZ_PLATFORM_MAEMO == 5)
  // The maemo flash plugin does not remember this.  It sets the
  // value, but doesn't support the get value.
  if (variable == NPPVpluginWindowlessLocalBool) {
    *(NPBool*)value = mWindowlessLocal;
    return NS_OK;
  }
#endif
  nsresult  res = NS_ERROR_FAILURE;
  if (mCallbacks->getvalue && mRunning) {
    PluginDestructionGuard guard(this);

    NS_TRY_SAFE_CALL_RETURN(res, (*mCallbacks->getvalue)(&mNPP, variable, value), mLibrary, this);
    NPP_PLUGIN_LOG(PLUGIN_LOG_NORMAL,
    ("NPP GetValue called: this=%p, npp=%p, var=%d, value=%d, return=%d\n", 
    this, &mNPP, variable, value, res));
  }

  return res;
}

nsresult nsNPAPIPluginInstance::GetNPP(NPP* aNPP) 
{
  if (aNPP)
    *aNPP = &mNPP;
  else
    return NS_ERROR_NULL_POINTER;

  return NS_OK;
}

nsresult nsNPAPIPluginInstance::GetCallbacks(const NPPluginFuncs ** aCallbacks)
{
  if (aCallbacks)
    *aCallbacks = mCallbacks;
  else
    return NS_ERROR_NULL_POINTER;

  return NS_OK;
}

NPError nsNPAPIPluginInstance::SetWindowless(PRBool aWindowless)
{
  mWindowless = aWindowless;
  return NPERR_NO_ERROR;
}

NPError nsNPAPIPluginInstance::SetWindowlessLocal(PRBool aWindowlessLocal)
{
  mWindowlessLocal = aWindowlessLocal;
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

void nsNPAPIPluginInstance::SetEventModel(NPEventModel aModel)
{
  // the event model needs to be set for the object frame immediately
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner) {
    NS_WARNING("Trying to set event model without a plugin instance owner!");
    return;
  }

  owner->SetEventModel(aModel);
}

#endif

NS_IMETHODIMP nsNPAPIPluginInstance::GetDrawingModel(PRInt32* aModel)
{
#ifdef XP_MACOSX
  *aModel = (PRInt32)mDrawingModel;
  return NS_OK;
#else
  return NS_ERROR_FAILURE;
#endif
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetJSObject(JSContext *cx, JSObject** outObject)
{
  NPObject *npobj = nsnull;
  nsresult rv = GetValueFromPlugin(NPPVpluginScriptableNPObject, &npobj);
  if (NS_FAILED(rv) || !npobj)
    return NS_ERROR_FAILURE;

  *outObject = nsNPObjWrapper::GetNewOrUsed(&mNPP, cx, npobj);

  _releaseobject(npobj);

  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::DefineJavaProperties()
{
  NPObject *plugin_obj = nsnull;

  // The dummy Java plugin's scriptable object is what we want to
  // expose as window.Packages. And Window.Packages.java will be
  // exposed as window.java.

  // Get the scriptable plugin object.
  nsresult rv = GetValueFromPlugin(NPPVpluginScriptableNPObject, &plugin_obj);

  if (NS_FAILED(rv) || !plugin_obj) {
    return NS_ERROR_FAILURE;
  }

  // Get the NPObject wrapper for window.
  NPObject *window_obj = _getwindowobject(&mNPP);

  if (!window_obj) {
    _releaseobject(plugin_obj);

    return NS_ERROR_FAILURE;
  }

  NPIdentifier java_id = _getstringidentifier("java");
  NPIdentifier packages_id = _getstringidentifier("Packages");

  NPObject *java_obj = nsnull;
  NPVariant v;
  OBJECT_TO_NPVARIANT(plugin_obj, v);

  // Define the properties.

  bool ok = _setproperty(&mNPP, window_obj, packages_id, &v);
  if (ok) {
    ok = _getproperty(&mNPP, plugin_obj, java_id, &v);

    if (ok && NPVARIANT_IS_OBJECT(v)) {
      // Set java_obj so that we properly release it at the end of
      // this function.
      java_obj = NPVARIANT_TO_OBJECT(v);

      ok = _setproperty(&mNPP, window_obj, java_id, &v);
    }
  }

  _releaseobject(window_obj);
  _releaseobject(plugin_obj);
  _releaseobject(java_obj);

  if (!ok)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::SetCached(PRBool aCache)
{
  mCached = aCache;
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::ShouldCache(PRBool* shouldCache)
{
  *shouldCache = mCached;
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::IsWindowless(PRBool* isWindowless)
{
  *isWindowless = mWindowless;
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::IsTransparent(PRBool* isTransparent)
{
  *isTransparent = mTransparent;
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetFormValue(nsAString& aValue)
{
  aValue.Truncate();

  char *value = nsnull;
  nsresult rv = GetValueFromPlugin(NPPVformValue, &value);
  if (NS_FAILED(rv) || !value)
    return NS_ERROR_FAILURE;

  CopyUTF8toUTF16(value, aValue);

  // NPPVformValue allocates with NPN_MemAlloc(), which uses
  // nsMemory.
  nsMemory::Free(value);

  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::PushPopupsEnabledState(PRBool aEnabled)
{
  nsCOMPtr<nsPIDOMWindow> window = GetDOMWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  PopupControlState oldState =
    window->PushPopupControlState(aEnabled ? openAllowed : openAbused,
                                  PR_TRUE);

  if (!mPopupStates.AppendElement(oldState)) {
    // Appending to our state stack failed, pop what we just pushed.
    window->PopPopupControlState(oldState);
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::PopPopupsEnabledState()
{
  PRInt32 last = mPopupStates.Length() - 1;

  if (last < 0) {
    // Nothing to pop.
    return NS_OK;
  }

  nsCOMPtr<nsPIDOMWindow> window = GetDOMWindow();
  if (!window)
    return NS_ERROR_FAILURE;

  PopupControlState &oldState = mPopupStates[last];

  window->PopPopupControlState(oldState);

  mPopupStates.RemoveElementAt(last);
  
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetPluginAPIVersion(PRUint16* version)
{
  NS_ENSURE_ARG_POINTER(version);
  *version = mCallbacks->version;
  return NS_OK;
}

nsresult
nsNPAPIPluginInstance::PrivateModeStateChanged()
{
  if (!mRunning)
    return NS_OK;
  
  PLUGIN_LOG(PLUGIN_LOG_NORMAL, ("nsNPAPIPluginInstance informing plugin of private mode state change this=%p\n",this));
  
  if (mCallbacks->setvalue) {
    PluginDestructionGuard guard(this);
    
    nsCOMPtr<nsIPrivateBrowsingService> pbs = do_GetService(NS_PRIVATE_BROWSING_SERVICE_CONTRACTID);
    if (pbs) {
      PRBool pme = PR_FALSE;
      nsresult rv = pbs->GetPrivateBrowsingEnabled(&pme);
      if (NS_FAILED(rv))
        return rv;

      NPError error;
      NS_TRY_SAFE_CALL_RETURN(error, (*mCallbacks->setvalue)(&mNPP, NPNVprivateModeBool, &pme), mLibrary, this);
      return (error == NPERR_NO_ERROR) ? NS_OK : NS_ERROR_FAILURE;
    }
  }
  return NS_ERROR_FAILURE;
}

static void
PluginTimerCallback(nsITimer *aTimer, void *aClosure)
{
  nsNPAPITimer* t = (nsNPAPITimer*)aClosure;
  NPP npp = t->npp;
  uint32_t id = t->id;

  (*(t->callback))(npp, id);

  // Make sure we still have an instance and the timer is still alive
  // after the callback.
  nsNPAPIPluginInstance *inst = (nsNPAPIPluginInstance*)npp->ndata;
  if (!inst || !inst->TimerWithID(id, NULL))
    return;

  // use UnscheduleTimer to clean up if this is a one-shot timer
  PRUint32 timerType;
  t->timer->GetType(&timerType);
  if (timerType == nsITimer::TYPE_ONE_SHOT)
      inst->UnscheduleTimer(id);
}

nsNPAPITimer*
nsNPAPIPluginInstance::TimerWithID(uint32_t id, PRUint32* index)
{
  PRUint32 len = mTimers.Length();
  for (PRUint32 i = 0; i < len; i++) {
    if (mTimers[i]->id == id) {
      if (index)
        *index = i;
      return mTimers[i];
    }
  }
  return nsnull;
}

uint32_t
nsNPAPIPluginInstance::ScheduleTimer(uint32_t interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32_t timerID))
{
  nsNPAPITimer *newTimer = new nsNPAPITimer();

  newTimer->npp = &mNPP;

  // generate ID that is unique to this instance
  uint32_t uniqueID = mTimers.Length();
  while ((uniqueID == 0) || TimerWithID(uniqueID, NULL))
    uniqueID++;
  newTimer->id = uniqueID;

  // create new xpcom timer, scheduled correctly
  nsresult rv;
  nsCOMPtr<nsITimer> xpcomTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return 0;
  const short timerType = (repeat ? (short)nsITimer::TYPE_REPEATING_SLACK : (short)nsITimer::TYPE_ONE_SHOT);
  xpcomTimer->InitWithFuncCallback(PluginTimerCallback, newTimer, interval, timerType);
  newTimer->timer = xpcomTimer;

  // save callback function
  newTimer->callback = timerFunc;

  // add timer to timers array
  mTimers.AppendElement(newTimer);

  return newTimer->id;
}

void
nsNPAPIPluginInstance::UnscheduleTimer(uint32_t timerID)
{
  // find the timer struct by ID
  PRUint32 index;
  nsNPAPITimer* t = TimerWithID(timerID, &index);
  if (!t)
    return;

  // cancel the timer
  t->timer->Cancel();

  // remove timer struct from array
  mTimers.RemoveElementAt(index);

  // delete timer
  delete t;
}

// Show the context menu at the location for the current event.
// This can only be called from within an NPP_SendEvent call.
NPError
nsNPAPIPluginInstance::PopUpContextMenu(NPMenu* menu)
{
  if (mOwner && mCurrentPluginEvent)
    return mOwner->ShowNativeContextMenu(menu, mCurrentPluginEvent);

  return NPERR_GENERIC_ERROR;
}

NPBool
nsNPAPIPluginInstance::ConvertPoint(double sourceX, double sourceY, NPCoordinateSpace sourceSpace,
                                    double *destX, double *destY, NPCoordinateSpace destSpace)
{
  if (mOwner)
    return mOwner->ConvertPoint(sourceX, sourceY, sourceSpace, destX, destY, destSpace);

  return PR_FALSE;
}

nsresult
nsNPAPIPluginInstance::GetDOMElement(nsIDOMElement* *result)
{
  if (!mOwner) {
    *result = nsnull;
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsIPluginTagInfo> tinfo(do_QueryInterface(mOwner));
  if (tinfo)
    return tinfo->GetDOMElement(result);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::InvalidateRect(NPRect *invalidRect)
{
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  return owner->InvalidateRect(invalidRect);
}

NS_IMETHODIMP
nsNPAPIPluginInstance::InvalidateRegion(NPRegion invalidRegion)
{
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  return owner->InvalidateRegion(invalidRegion);
}

NS_IMETHODIMP
nsNPAPIPluginInstance::ForceRedraw()
{
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  return owner->ForceRedraw();
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetMIMEType(const char* *result)
{
  if (!mMIMEType)
    *result = "";
  else
    *result = mMIMEType;

  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetJSContext(JSContext* *outContext)
{
  nsCOMPtr<nsIPluginInstanceOwner> owner;
  GetOwner(getter_AddRefs(owner));
  if (!owner)
    return NS_ERROR_FAILURE;

  *outContext = NULL;
  nsCOMPtr<nsIDocument> document;

  nsresult rv = owner->GetDocument(getter_AddRefs(document));

  if (NS_SUCCEEDED(rv) && document) {
    nsIScriptGlobalObject *global = document->GetScriptGlobalObject();

    if (global) {
      nsIScriptContext *context = global->GetContext();

      if (context) {
        *outContext = (JSContext*) context->GetNativeContext();
      }
    }
  }

  return rv;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::GetOwner(nsIPluginInstanceOwner **aOwner)
{
  NS_ENSURE_ARG_POINTER(aOwner);
  *aOwner = mOwner;
  NS_IF_ADDREF(mOwner);
  return (mOwner ? NS_OK : NS_ERROR_FAILURE);
}

NS_IMETHODIMP
nsNPAPIPluginInstance::SetOwner(nsIPluginInstanceOwner *aOwner)
{
  mOwner = aOwner;
  return NS_OK;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::ShowStatus(const char* message)
{
  if (mOwner)
    return mOwner->ShowStatus(message);

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP
nsNPAPIPluginInstance::InvalidateOwner()
{
  mOwner = nsnull;

  return NS_OK;
}
