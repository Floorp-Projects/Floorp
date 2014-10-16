/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Attributes.h"

#include "nsIIncrementalDownload.h"
#include "nsIRequestObserver.h"
#include "nsIProgressEventSink.h"
#include "nsIChannelEventSink.h"
#include "nsIAsyncVerifyRedirectCallback.h"
#include "nsIInterfaceRequestor.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsIFile.h"
#include "nsITimer.h"
#include "nsNetUtil.h"
#include "nsAutoPtr.h"
#include "nsWeakReference.h"
#include "prio.h"
#include "prprf.h"
#include <algorithm>
#include "nsIContentPolicy.h"
#include "nsContentUtils.h"

// Default values used to initialize a nsIncrementalDownload object.
#define DEFAULT_CHUNK_SIZE (4096 * 16)  // bytes
#define DEFAULT_INTERVAL    60          // seconds

#define UPDATE_PROGRESS_INTERVAL PRTime(500 * PR_USEC_PER_MSEC) // 500ms

// Number of times to retry a failed byte-range request.
#define MAX_RETRY_COUNT 20

//-----------------------------------------------------------------------------

static nsresult
WriteToFile(nsIFile *lf, const char *data, uint32_t len, int32_t flags)
{
  PRFileDesc *fd;
  int32_t mode = 0600;
  nsresult rv;
#if defined(MOZ_WIDGET_GONK)
  // The sdcard on a B2G phone looks like:
  // d---rwx--- system   sdcard_rw          1970-01-01 01:00:00 sdcard
  // On the emulator, xpcshell fails when using 0600 mode to open the file,
  // and 0660 works.
  nsCOMPtr<nsIFile> parent;
  rv = lf->GetParent(getter_AddRefs(parent));
  if (NS_FAILED(rv)) {
    return rv;
  }
  uint32_t  parentPerm;
  rv = parent->GetPermissions(&parentPerm);
  if (NS_FAILED(rv)) {
    return rv;
  }
  if ((parentPerm & 0700) == 0) {
    // Parent directory has no owner-write, so try to use group permissions
    // instead of owner permissions.
    mode = 0660;
  }
#endif
  rv = lf->OpenNSPRFileDesc(flags, mode, &fd);
  if (NS_FAILED(rv))
    return rv;

  if (len)
    rv = PR_Write(fd, data, len) == int32_t(len) ? NS_OK : NS_ERROR_FAILURE;

  PR_Close(fd);
  return rv;
}

static nsresult
AppendToFile(nsIFile *lf, const char *data, uint32_t len)
{
  int32_t flags = PR_WRONLY | PR_CREATE_FILE | PR_APPEND;
  return WriteToFile(lf, data, len, flags);
}

// maxSize may be -1 if unknown
static void
MakeRangeSpec(const int64_t &size, const int64_t &maxSize, int32_t chunkSize,
              bool fetchRemaining, nsCString &rangeSpec)
{
  rangeSpec.AssignLiteral("bytes=");
  rangeSpec.AppendInt(int64_t(size));
  rangeSpec.Append('-');

  if (fetchRemaining)
    return;

  int64_t end = size + int64_t(chunkSize);
  if (maxSize != int64_t(-1) && end > maxSize)
    end = maxSize;
  end -= 1;

  rangeSpec.AppendInt(int64_t(end));
}

//-----------------------------------------------------------------------------

class nsIncrementalDownload MOZ_FINAL
  : public nsIIncrementalDownload
  , public nsIStreamListener
  , public nsIObserver
  , public nsIInterfaceRequestor
  , public nsIChannelEventSink
  , public nsSupportsWeakReference
  , public nsIAsyncVerifyRedirectCallback
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIREQUEST
  NS_DECL_NSIINCREMENTALDOWNLOAD
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIOBSERVER
  NS_DECL_NSIINTERFACEREQUESTOR
  NS_DECL_NSICHANNELEVENTSINK
  NS_DECL_NSIASYNCVERIFYREDIRECTCALLBACK

  nsIncrementalDownload();

private:
  ~nsIncrementalDownload() {}
  nsresult FlushChunk();
  void     UpdateProgress();
  nsresult CallOnStartRequest();
  void     CallOnStopRequest();
  nsresult StartTimer(int32_t interval);
  nsresult ProcessTimeout();
  nsresult ReadCurrentSize();
  nsresult ClearRequestHeader(nsIHttpChannel *channel);

  nsCOMPtr<nsIRequestObserver>             mObserver;
  nsCOMPtr<nsISupports>                    mObserverContext;
  nsCOMPtr<nsIProgressEventSink>           mProgressSink;
  nsCOMPtr<nsIURI>                         mURI;
  nsCOMPtr<nsIURI>                         mFinalURI;
  nsCOMPtr<nsIFile>                        mDest;
  nsCOMPtr<nsIChannel>                     mChannel;
  nsCOMPtr<nsITimer>                       mTimer;
  nsAutoArrayPtr<char>                     mChunk;
  int32_t                                  mChunkLen;
  int32_t                                  mChunkSize;
  int32_t                                  mInterval;
  int64_t                                  mTotalSize;
  int64_t                                  mCurrentSize;
  uint32_t                                 mLoadFlags;
  int32_t                                  mNonPartialCount;
  nsresult                                 mStatus;
  bool                                     mIsPending;
  bool                                     mDidOnStartRequest;
  PRTime                                   mLastProgressUpdate;
  nsCOMPtr<nsIAsyncVerifyRedirectCallback> mRedirectCallback;
  nsCOMPtr<nsIChannel>                     mNewRedirectChannel;
  nsCString                                mPartialValidator;
  bool                                     mCacheBust;
};

nsIncrementalDownload::nsIncrementalDownload()
  : mChunkLen(0)
  , mChunkSize(DEFAULT_CHUNK_SIZE)
  , mInterval(DEFAULT_INTERVAL)
  , mTotalSize(-1)
  , mCurrentSize(-1)
  , mLoadFlags(LOAD_NORMAL)
  , mNonPartialCount(0)
  , mStatus(NS_OK)
  , mIsPending(false)
  , mDidOnStartRequest(false)
  , mLastProgressUpdate(0)
  , mRedirectCallback(nullptr)
  , mNewRedirectChannel(nullptr)
  , mCacheBust(false)  
{
}

nsresult
nsIncrementalDownload::FlushChunk()
{
  NS_ASSERTION(mTotalSize != int64_t(-1), "total size should be known");

  if (mChunkLen == 0)
    return NS_OK;

  nsresult rv = AppendToFile(mDest, mChunk, mChunkLen);
  if (NS_FAILED(rv))
    return rv;

  mCurrentSize += int64_t(mChunkLen);
  mChunkLen = 0;

  return NS_OK;
}

void
nsIncrementalDownload::UpdateProgress()
{
  mLastProgressUpdate = PR_Now();

  if (mProgressSink)
    mProgressSink->OnProgress(this, mObserverContext,
                              uint64_t(int64_t(mCurrentSize) + mChunkLen),
                              uint64_t(int64_t(mTotalSize)));
}

nsresult
nsIncrementalDownload::CallOnStartRequest()
{
  if (!mObserver || mDidOnStartRequest)
    return NS_OK;

  mDidOnStartRequest = true;
  return mObserver->OnStartRequest(this, mObserverContext);
}

void
nsIncrementalDownload::CallOnStopRequest()
{
  if (!mObserver)
    return;

  // Ensure that OnStartRequest is always called once before OnStopRequest.
  nsresult rv = CallOnStartRequest();
  if (NS_SUCCEEDED(mStatus))
    mStatus = rv;

  mIsPending = false;

  mObserver->OnStopRequest(this, mObserverContext, mStatus);
  mObserver = nullptr;
  mObserverContext = nullptr;
}

nsresult
nsIncrementalDownload::StartTimer(int32_t interval)
{
  nsresult rv;
  mTimer = do_CreateInstance(NS_TIMER_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    return rv;

  return mTimer->Init(this, interval * 1000, nsITimer::TYPE_ONE_SHOT);
}

nsresult
nsIncrementalDownload::ProcessTimeout()
{
  NS_ASSERTION(!mChannel, "how can we have a channel?");

  // Handle existing error conditions
  if (NS_FAILED(mStatus)) {
    CallOnStopRequest();
    return NS_OK;
  }

  // Fetch next chunk
  
  nsCOMPtr<nsIChannel> channel;
  nsresult rv = NS_NewChannel(getter_AddRefs(channel),
                              mFinalURI,
                              nsContentUtils::GetSystemPrincipal(),
                              nsILoadInfo::SEC_NORMAL,
                              nsIContentPolicy::TYPE_OTHER,
                              nullptr,   // loadGroup
                              this,      // aCallbacks
                              mLoadFlags);

  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(channel, &rv);
  if (NS_FAILED(rv))
    return rv;

  NS_ASSERTION(mCurrentSize != int64_t(-1),
      "we should know the current file size by now");

  rv = ClearRequestHeader(http);
  if (NS_FAILED(rv))
    return rv;

  // Don't bother making a range request if we are just going to fetch the
  // entire document.
  if (mInterval || mCurrentSize != int64_t(0)) {
    nsAutoCString range;
    MakeRangeSpec(mCurrentSize, mTotalSize, mChunkSize, mInterval == 0, range);

    rv = http->SetRequestHeader(NS_LITERAL_CSTRING("Range"), range, false);
    if (NS_FAILED(rv))
      return rv;

    if (!mPartialValidator.IsEmpty())
      http->SetRequestHeader(NS_LITERAL_CSTRING("If-Range"),
                             mPartialValidator, false);

    if (mCacheBust) {
      http->SetRequestHeader(NS_LITERAL_CSTRING("Cache-Control"),
                             NS_LITERAL_CSTRING("no-cache"), false);
      http->SetRequestHeader(NS_LITERAL_CSTRING("Pragma"),
                             NS_LITERAL_CSTRING("no-cache"), false);
    }
  }

  rv = channel->AsyncOpen(this, nullptr);
  if (NS_FAILED(rv))
    return rv;

  // Wait to assign mChannel when we know we are going to succeed.  This is
  // important because we don't want to introduce a reference cycle between
  // mChannel and this until we know for a fact that AsyncOpen has succeeded,
  // thus ensuring that our stream listener methods will be invoked.
  mChannel = channel;
  return NS_OK;
}

// Reads the current file size and validates it.
nsresult
nsIncrementalDownload::ReadCurrentSize()
{
  int64_t size;
  nsresult rv = mDest->GetFileSize((int64_t *) &size);
  if (rv == NS_ERROR_FILE_NOT_FOUND ||
      rv == NS_ERROR_FILE_TARGET_DOES_NOT_EXIST) {
    mCurrentSize = 0;
    return NS_OK;
  }
  if (NS_FAILED(rv))
    return rv;

  mCurrentSize = size; 
  return NS_OK;
}

// nsISupports

NS_IMPL_ISUPPORTS(nsIncrementalDownload,
                  nsIIncrementalDownload,
                  nsIRequest,
                  nsIStreamListener,
                  nsIRequestObserver,
                  nsIObserver,
                  nsIInterfaceRequestor,
                  nsIChannelEventSink,
                  nsISupportsWeakReference,
                  nsIAsyncVerifyRedirectCallback)

// nsIRequest

NS_IMETHODIMP
nsIncrementalDownload::GetName(nsACString &name)
{
  NS_ENSURE_TRUE(mURI, NS_ERROR_NOT_INITIALIZED);

  return mURI->GetSpec(name);
}

NS_IMETHODIMP
nsIncrementalDownload::IsPending(bool *isPending)
{
  *isPending = mIsPending;
  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalDownload::GetStatus(nsresult *status)
{
  *status = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalDownload::Cancel(nsresult status)
{
  NS_ENSURE_ARG(NS_FAILED(status));

  // Ignore this cancelation if we're already canceled.
  if (NS_FAILED(mStatus))
    return NS_OK;

  mStatus = status;

  // Nothing more to do if callbacks aren't pending.
  if (!mIsPending)
    return NS_OK;

  if (mChannel) {
    mChannel->Cancel(mStatus);
    NS_ASSERTION(!mTimer, "what is this timer object doing here?");
  }
  else {
    // dispatch a timer callback event to drive invoking our listener's
    // OnStopRequest.
    if (mTimer)
      mTimer->Cancel();
    StartTimer(0);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalDownload::Suspend()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIncrementalDownload::Resume()
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIncrementalDownload::GetLoadFlags(nsLoadFlags *loadFlags)
{
  *loadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalDownload::SetLoadFlags(nsLoadFlags loadFlags)
{
  mLoadFlags = loadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalDownload::GetLoadGroup(nsILoadGroup **loadGroup)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsIncrementalDownload::SetLoadGroup(nsILoadGroup *loadGroup)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

// nsIIncrementalDownload

NS_IMETHODIMP
nsIncrementalDownload::Init(nsIURI *uri, nsIFile *dest,
                            int32_t chunkSize, int32_t interval)
{
  // Keep it simple: only allow initialization once
  NS_ENSURE_FALSE(mURI, NS_ERROR_ALREADY_INITIALIZED);

  mDest = do_QueryInterface(dest);
  NS_ENSURE_ARG(mDest);

  mURI = uri;
  mFinalURI = uri;

  if (chunkSize > 0)
    mChunkSize = chunkSize;
  if (interval >= 0)
    mInterval = interval;
  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalDownload::GetURI(nsIURI **result)
{
  NS_IF_ADDREF(*result = mURI);
  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalDownload::GetFinalURI(nsIURI **result)
{
  NS_IF_ADDREF(*result = mFinalURI);
  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalDownload::GetDestination(nsIFile **result)
{
  if (!mDest) {
    *result = nullptr;
    return NS_OK;
  }
  // Return a clone of mDest so that callers may modify the resulting nsIFile
  // without corrupting our internal object.  This also works around the fact
  // that some nsIFile impls may cache the result of stat'ing the filesystem.
  return mDest->Clone(result);
}

NS_IMETHODIMP
nsIncrementalDownload::GetTotalSize(int64_t *result)
{
  *result = mTotalSize;
  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalDownload::GetCurrentSize(int64_t *result)
{
  *result = mCurrentSize;
  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalDownload::Start(nsIRequestObserver *observer,
                             nsISupports *context)
{
  NS_ENSURE_ARG(observer);
  NS_ENSURE_FALSE(mIsPending, NS_ERROR_IN_PROGRESS);

  // Observe system shutdown so we can be sure to release any reference held
  // between ourselves and the timer.  We have the observer service hold a weak
  // reference to us, so that we don't have to worry about calling
  // RemoveObserver.  XXX(darin): The timer code should do this for us.
  nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
  if (obs)
    obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, true);

  nsresult rv = ReadCurrentSize();
  if (NS_FAILED(rv))
    return rv;

  rv = StartTimer(0);
  if (NS_FAILED(rv))
    return rv;

  mObserver = observer;
  mObserverContext = context;
  mProgressSink = do_QueryInterface(observer);  // ok if null

  mIsPending = true;
  return NS_OK;
}

// nsIRequestObserver

NS_IMETHODIMP
nsIncrementalDownload::OnStartRequest(nsIRequest *request,
                                      nsISupports *context)
{
  nsresult rv;

  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(request, &rv);
  if (NS_FAILED(rv))
    return rv;

  // Ensure that we are receiving a 206 response.
  uint32_t code;
  rv = http->GetResponseStatus(&code);
  if (NS_FAILED(rv))
    return rv;
  if (code != 206) {
    // We may already have the entire file downloaded, in which case
    // our request for a range beyond the end of the file would have
    // been met with an error response code.
    if (code == 416 && mTotalSize == int64_t(-1)) {
      mTotalSize = mCurrentSize;
      // Return an error code here to suppress OnDataAvailable.
      return NS_ERROR_DOWNLOAD_COMPLETE;
    }
    // The server may have decided to give us all of the data in one chunk.  If
    // we requested a partial range, then we don't want to download all of the
    // data at once.  So, we'll just try again, but if this keeps happening then
    // we'll eventually give up.
    if (code == 200) {
      if (mInterval) {
        mChannel = nullptr;
        if (++mNonPartialCount > MAX_RETRY_COUNT) {
          NS_WARNING("unable to fetch a byte range; giving up");
          return NS_ERROR_FAILURE;
        }
        // Increase delay with each failure.
        StartTimer(mInterval * mNonPartialCount);
        return NS_ERROR_DOWNLOAD_NOT_PARTIAL;
      }
      // Since we have been asked to download the rest of the file, we can deal
      // with a 200 response.  This may result in downloading the beginning of
      // the file again, but that can't really be helped.
    } else {
      NS_WARNING("server response was unexpected");
      return NS_ERROR_UNEXPECTED;
    }
  } else {
    // We got a partial response, so clear this counter in case the next chunk
    // results in a 200 response.
    mNonPartialCount = 0;

    // confirm that the content-range response header is consistent with
    // expectations on each 206. If it is not then drop this response and
    // retry with no-cache set.
    if (!mCacheBust) {
      nsAutoCString buf;
      int64_t startByte = 0;
      bool confirmedOK = false;

      rv = http->GetResponseHeader(NS_LITERAL_CSTRING("Content-Range"), buf);
      if (NS_FAILED(rv))
        return rv; // it isn't a useful 206 without a CONTENT-RANGE of some sort

      // Content-Range: bytes 0-299999/25604694
      int32_t p = buf.Find("bytes ");

      // first look for the starting point of the content-range
      // to make sure it is what we expect
      if (p != -1) {
        char *endptr = nullptr;
        const char *s = buf.get() + p + 6;
        while (*s && *s == ' ')
          s++;
        startByte = strtol(s, &endptr, 10);

        if (*s && endptr && (endptr != s) &&
            (mCurrentSize == startByte)) {

          // ok the starting point is confirmed. We still need to check the
          // total size of the range for consistency if this isn't
          // the first chunk
          if (mTotalSize == int64_t(-1)) {
            // first chunk
            confirmedOK = true;
          } else {
            int32_t slash = buf.FindChar('/');
            int64_t rangeSize = 0;
            if (slash != kNotFound &&
                (PR_sscanf(buf.get() + slash + 1, "%lld", (int64_t *) &rangeSize) == 1) &&
                rangeSize == mTotalSize) {
              confirmedOK = true;
            }
          }
        }
      }

      if (!confirmedOK) {
        NS_WARNING("unexpected content-range");
        mCacheBust = true;
        mChannel = nullptr;
        if (++mNonPartialCount > MAX_RETRY_COUNT) {
          NS_WARNING("unable to fetch a byte range; giving up");
          return NS_ERROR_FAILURE;
        }
        // Increase delay with each failure.
        StartTimer(mInterval * mNonPartialCount);
        return NS_ERROR_DOWNLOAD_NOT_PARTIAL;
      }
    }
  }

  // Do special processing after the first response.
  if (mTotalSize == int64_t(-1)) {
    // Update knowledge of mFinalURI
    rv = http->GetURI(getter_AddRefs(mFinalURI));
    if (NS_FAILED(rv))
      return rv;
    http->GetResponseHeader(NS_LITERAL_CSTRING("Etag"), mPartialValidator);
    if (StringBeginsWith(mPartialValidator, NS_LITERAL_CSTRING("W/")))
      mPartialValidator.Truncate(); // don't use weak validators
    if (mPartialValidator.IsEmpty())
      http->GetResponseHeader(NS_LITERAL_CSTRING("Last-Modified"), mPartialValidator);

    if (code == 206) {
      // OK, read the Content-Range header to determine the total size of this
      // download file.
      nsAutoCString buf;
      rv = http->GetResponseHeader(NS_LITERAL_CSTRING("Content-Range"), buf);
      if (NS_FAILED(rv))
        return rv;
      int32_t slash = buf.FindChar('/');
      if (slash == kNotFound) {
        NS_WARNING("server returned invalid Content-Range header!");
        return NS_ERROR_UNEXPECTED;
      }
      if (PR_sscanf(buf.get() + slash + 1, "%lld", (int64_t *) &mTotalSize) != 1)
        return NS_ERROR_UNEXPECTED;
    } else {
      rv = http->GetContentLength(&mTotalSize);
      if (NS_FAILED(rv))
        return rv;
      // We need to know the total size of the thing we're trying to download.
      if (mTotalSize == int64_t(-1)) {
        NS_WARNING("server returned no content-length header!");
        return NS_ERROR_UNEXPECTED;
      }
      // Need to truncate (or create, if it doesn't exist) the file since we
      // are downloading the whole thing.
      WriteToFile(mDest, nullptr, 0, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE);
      mCurrentSize = 0;
    }

    // Notify observer that we are starting...
    rv = CallOnStartRequest();
    if (NS_FAILED(rv))
      return rv;
  }

  // Adjust mChunkSize accordingly if mCurrentSize is close to mTotalSize.
  int64_t diff = mTotalSize - mCurrentSize;
  if (diff <= int64_t(0)) {
    NS_WARNING("about to set a bogus chunk size; giving up");
    return NS_ERROR_UNEXPECTED;
  }

  if (diff < int64_t(mChunkSize))
    mChunkSize = uint32_t(diff);

  mChunk = new char[mChunkSize];
  if (!mChunk)
    rv = NS_ERROR_OUT_OF_MEMORY;

  return rv;
}

NS_IMETHODIMP
nsIncrementalDownload::OnStopRequest(nsIRequest *request,
                                     nsISupports *context,
                                     nsresult status)
{
  // Not a real error; just a trick to kill off the channel without our
  // listener having to care.
  if (status == NS_ERROR_DOWNLOAD_NOT_PARTIAL)
    return NS_OK;

  // Not a real error; just a trick used to suppress OnDataAvailable calls.
  if (status == NS_ERROR_DOWNLOAD_COMPLETE)
    status = NS_OK;

  if (NS_SUCCEEDED(mStatus))
    mStatus = status;

  if (mChunk) {
    if (NS_SUCCEEDED(mStatus))
      mStatus = FlushChunk();

    mChunk = nullptr;  // deletes memory
    mChunkLen = 0;
    UpdateProgress();
  }

  mChannel = nullptr;

  // Notify listener if we hit an error or finished
  if (NS_FAILED(mStatus) || mCurrentSize == mTotalSize) {
    CallOnStopRequest();
    return NS_OK;
  }

  return StartTimer(mInterval);  // Do next chunk
}

// nsIStreamListener

NS_IMETHODIMP
nsIncrementalDownload::OnDataAvailable(nsIRequest *request,
                                       nsISupports *context,
                                       nsIInputStream *input,
                                       uint64_t offset,
                                       uint32_t count)
{
  while (count) {
    uint32_t space = mChunkSize - mChunkLen;
    uint32_t n, len = std::min(space, count);

    nsresult rv = input->Read(mChunk + mChunkLen, len, &n);
    if (NS_FAILED(rv))
      return rv;
    if (n != len)
      return NS_ERROR_UNEXPECTED;

    count -= n;
    mChunkLen += n;

    if (mChunkLen == mChunkSize) {
      rv = FlushChunk();
      if (NS_FAILED(rv))
        return rv;
    }
  }

  if (PR_Now() > mLastProgressUpdate + UPDATE_PROGRESS_INTERVAL)
    UpdateProgress();

  return NS_OK;
}

// nsIObserver

NS_IMETHODIMP
nsIncrementalDownload::Observe(nsISupports *subject, const char *topic,
                               const char16_t *data)
{
  if (strcmp(topic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    Cancel(NS_ERROR_ABORT);

    // Since the app is shutting down, we need to go ahead and notify our
    // observer here.  Otherwise, we would notify them after XPCOM has been
    // shutdown or not at all.
    CallOnStopRequest();
  }
  else if (strcmp(topic, NS_TIMER_CALLBACK_TOPIC) == 0) {
    mTimer = nullptr;
    nsresult rv = ProcessTimeout();
    if (NS_FAILED(rv))
      Cancel(rv);
  }
  return NS_OK;
}

// nsIInterfaceRequestor

NS_IMETHODIMP
nsIncrementalDownload::GetInterface(const nsIID &iid, void **result)
{
  if (iid.Equals(NS_GET_IID(nsIChannelEventSink))) {
    NS_ADDREF_THIS();
    *result = static_cast<nsIChannelEventSink *>(this);
    return NS_OK;
  }

  nsCOMPtr<nsIInterfaceRequestor> ir = do_QueryInterface(mObserver);
  if (ir)
    return ir->GetInterface(iid, result);

  return NS_ERROR_NO_INTERFACE;
}

nsresult 
nsIncrementalDownload::ClearRequestHeader(nsIHttpChannel *channel)
{
  NS_ENSURE_ARG(channel);
  
  // We don't support encodings -- they make the Content-Length not equal
  // to the actual size of the data. 
  return channel->SetRequestHeader(NS_LITERAL_CSTRING("Accept-Encoding"),
                                   NS_LITERAL_CSTRING(""), false);
}

// nsIChannelEventSink

NS_IMETHODIMP
nsIncrementalDownload::AsyncOnChannelRedirect(nsIChannel *oldChannel,
                                              nsIChannel *newChannel,
                                              uint32_t flags,
                                              nsIAsyncVerifyRedirectCallback *cb)
{
  // In response to a redirect, we need to propagate the Range header.  See bug
  // 311595.  Any failure code returned from this function aborts the redirect.
 
  nsCOMPtr<nsIHttpChannel> http = do_QueryInterface(oldChannel);
  NS_ENSURE_STATE(http);

  nsCOMPtr<nsIHttpChannel> newHttpChannel = do_QueryInterface(newChannel);
  NS_ENSURE_STATE(newHttpChannel);

  NS_NAMED_LITERAL_CSTRING(rangeHdr, "Range");

  nsresult rv = ClearRequestHeader(newHttpChannel);
  if (NS_FAILED(rv))
    return rv;

  // If we didn't have a Range header, then we must be doing a full download.
  nsAutoCString rangeVal;
  http->GetRequestHeader(rangeHdr, rangeVal);
  if (!rangeVal.IsEmpty()) {
    rv = newHttpChannel->SetRequestHeader(rangeHdr, rangeVal, false);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // A redirection changes the validator
  mPartialValidator.Truncate();

  if (mCacheBust) {
    newHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Cache-Control"),
                                     NS_LITERAL_CSTRING("no-cache"), false);
    newHttpChannel->SetRequestHeader(NS_LITERAL_CSTRING("Pragma"),
                                     NS_LITERAL_CSTRING("no-cache"), false);
  }

  // Prepare to receive callback
  mRedirectCallback = cb;
  mNewRedirectChannel = newChannel;

  // Give the observer a chance to see this redirect notification.
  nsCOMPtr<nsIChannelEventSink> sink = do_GetInterface(mObserver);
  if (sink) {
    rv = sink->AsyncOnChannelRedirect(oldChannel, newChannel, flags, this);
    if (NS_FAILED(rv)) {
        mRedirectCallback = nullptr;
        mNewRedirectChannel = nullptr;
    }
    return rv;
  }
  (void) OnRedirectVerifyCallback(NS_OK);
  return NS_OK;
}

NS_IMETHODIMP
nsIncrementalDownload::OnRedirectVerifyCallback(nsresult result)
{
  NS_ASSERTION(mRedirectCallback, "mRedirectCallback not set in callback");
  NS_ASSERTION(mNewRedirectChannel, "mNewRedirectChannel not set in callback");

  // Update mChannel, so we can Cancel the new channel.
  if (NS_SUCCEEDED(result))
    mChannel = mNewRedirectChannel;

  mRedirectCallback->OnRedirectVerifyCallback(result);
  mRedirectCallback = nullptr;
  mNewRedirectChannel = nullptr;
  return NS_OK;
}

extern nsresult
net_NewIncrementalDownload(nsISupports *outer, const nsIID &iid, void **result)
{
  if (outer)
    return NS_ERROR_NO_AGGREGATION;

  nsIncrementalDownload *d = new nsIncrementalDownload();
  if (!d)
    return NS_ERROR_OUT_OF_MEMORY;
  
  NS_ADDREF(d);
  nsresult rv = d->QueryInterface(iid, result);
  NS_RELEASE(d);
  return rv;
}
