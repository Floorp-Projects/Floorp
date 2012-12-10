/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundFileSaver.h"
#include "nsIFile.h"
#include "nsIPipe.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"

namespace mozilla {
namespace net {

////////////////////////////////////////////////////////////////////////////////
//// Globals

/**
 * Buffer size for writing to the output file.
 */
#define BUFFERED_OUTPUT_SIZE (1024 * 32)

/**
 * When this upper limit is reached, the original request is suspended.
 */
#define REQUEST_SUSPEND_AT (1024 * 1024 * 4)

/**
 * When this lower limit is reached, the original request is resumed.
 */
#define REQUEST_RESUME_AT (1024 * 1024 * 2)

////////////////////////////////////////////////////////////////////////////////
//// NotifyTargetChangeRunnable

/**
 * Runnable object used to notify the control thread that file contents will now
 * be saved to the specified file.
 */
class NotifyTargetChangeRunnable MOZ_FINAL : public nsRunnable
{
public:
  NotifyTargetChangeRunnable(BackgroundFileSaver *aSaver, nsIFile *aTarget)
  : mSaver(aSaver)
  , mTarget(aTarget)
  {
  }

  NS_IMETHODIMP Run()
  {
    return mSaver->NotifyTargetChange(mTarget);
  }

private:
  nsCOMPtr<BackgroundFileSaver> mSaver;
  nsCOMPtr<nsIFile> mTarget;
};

////////////////////////////////////////////////////////////////////////////////
//// BackgroundFileSaver

BackgroundFileSaver::BackgroundFileSaver()
: mControlThread(nullptr)
, mWorkerThread(nullptr)
, mPipeOutputStream(nullptr)
, mPipeInputStream(nullptr)
, mObserver(nullptr)
, mLock("BackgroundFileSaver.mLock")
, mWorkerThreadAttentionRequested(false)
, mFinishRequested(false)
, mComplete(false)
, mStatus(NS_OK)
, mAssignedTarget(nullptr)
, mAssignedTargetKeepPartial(false)
, mAsyncCopyContext(nullptr)
, mActualTarget(nullptr)
, mActualTargetKeepPartial(false)
{
}

BackgroundFileSaver::~BackgroundFileSaver()
{
}

// Called on the control thread.
nsresult
BackgroundFileSaver::Init()
{
  nsresult rv;

  rv = NS_NewPipe2(getter_AddRefs(mPipeInputStream),
                   getter_AddRefs(mPipeOutputStream), true, true, 0,
                   HasInfiniteBuffer() ? UINT32_MAX : 0, nullptr);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_GetCurrentThread(getter_AddRefs(mControlThread));
  NS_ENSURE_SUCCESS(rv, rv);

  rv = NS_NewThread(getter_AddRefs(mWorkerThread));
  NS_ENSURE_SUCCESS(rv, rv);

  return NS_OK;
}

// Called on the control thread.
NS_IMETHODIMP
BackgroundFileSaver::GetObserver(nsIBackgroundFileSaverObserver **aObserver)
{
  NS_ENSURE_ARG_POINTER(aObserver);
  *aObserver = mObserver;
  NS_IF_ADDREF(*aObserver);
  return NS_OK;
}

// Called on the control thread.
NS_IMETHODIMP
BackgroundFileSaver::SetObserver(nsIBackgroundFileSaverObserver *aObserver)
{
  mObserver = aObserver;
  return NS_OK;
}

// Called on the control thread.
NS_IMETHODIMP
BackgroundFileSaver::SetTarget(nsIFile *aTarget, bool aKeepPartial)
{
  NS_ENSURE_ARG(aTarget);

  {
    MutexAutoLock lock(mLock);
    aTarget->Clone(getter_AddRefs(mAssignedTarget));
    mAssignedTargetKeepPartial = aKeepPartial;
  }

  // After the worker thread wakes up because attention is requested, it will
  // rename or create the target file as requested, and start copying data.
  return GetWorkerThreadAttention(true);
}

// Called on the control thread.
NS_IMETHODIMP
BackgroundFileSaver::Finish(nsresult aStatus)
{
  nsresult rv;

  // This will cause the NS_AsyncCopy operation, if it's in progress, to consume
  // all the data that is still in the pipe, and then finish.
  rv = mPipeOutputStream->Close();
  NS_ENSURE_SUCCESS(rv, rv);

  // Ensure that, when we get attention from the worker thread, if no pending
  // rename operation is waiting, the operation will complete.
  {
    MutexAutoLock lock(mLock);
    mFinishRequested = true;
    if (NS_SUCCEEDED(mStatus)) {
      mStatus = aStatus;
    }
  }

  // After the worker thread wakes up because attention is requested, it will
  // process the completion conditions, detect that completion is requested, and
  // notify the main thread of the completion.  If this function was called with
  // a success code, we wait for the copy to finish before processing the
  // completion conditions, otherwise we interrupt the copy immediately.
  return GetWorkerThreadAttention(NS_FAILED(aStatus));
}

// Called on the control thread.
nsresult
BackgroundFileSaver::GetWorkerThreadAttention(bool aShouldInterruptCopy)
{
  nsresult rv;

  MutexAutoLock lock(mLock);

  // We only require attention one time.  If this function is called two times
  // before the worker thread wakes up, and the first has aShouldInterruptCopy
  // false and the second true, we won't forcibly interrupt the copy from the
  // control thread.  However, that never happens, because calling Finish with a
  // success code is the only case that may result in aShouldInterruptCopy being
  // false.  In that case, we won't call this function again, because consumers
  // should not invoke other methods on the control thread after calling Finish.
  // And in any case, Finish already closes one end of the pipe, causing the
  // copy to finish properly on its own.
  if (mWorkerThreadAttentionRequested) {
    return NS_OK;
  }

  if (!mAsyncCopyContext) {
    // Copy is not in progress, post an event to handle the change manually.
    nsCOMPtr<nsIRunnable> event =
      NS_NewRunnableMethod(this, &BackgroundFileSaver::ProcessAttention);
    NS_ENSURE_TRUE(event, NS_ERROR_FAILURE);

    rv = mWorkerThread->Dispatch(event, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  } else if (aShouldInterruptCopy) {
    // Interrupt the copy.  The copy will be resumed, if needed, by the
    // ProcessAttention function, invoked by the AsyncCopyCallback function.
    NS_CancelAsyncCopy(mAsyncCopyContext, NS_ERROR_ABORT);
  }

  // Indicate that attention has been requested successfully, there is no need
  // to post another event until the worker thread processes the current one.
  mWorkerThreadAttentionRequested = true;

  return NS_OK;
}

// Called on the worker thread.
// static
void
BackgroundFileSaver::AsyncCopyCallback(void *aClosure, nsresult aStatus)
{
  BackgroundFileSaver *self = (BackgroundFileSaver *)aClosure;

  {
    MutexAutoLock lock(self->mLock);

    // Now that the copy was interrupted or terminated, any notification from
    // the control thread requires an event to be posted to the worker thread.
    self->mAsyncCopyContext = nullptr;

    // When detecting failures, ignore the status code we use to interrupt.
    if (NS_FAILED(aStatus) && aStatus != NS_ERROR_ABORT &&
        NS_SUCCEEDED(self->mStatus)) {
      self->mStatus = aStatus;
    }
  }

  (void)self->ProcessAttention();

  // We called NS_ADDREF_THIS when NS_AsyncCopy started, to keep the object
  // alive even if other references disappeared.  At this point, we've finished
  // using the object and can safely release our reference.
  NS_RELEASE(self);
}

// Called on the worker thread.
nsresult
BackgroundFileSaver::ProcessAttention()
{
  nsresult rv;

  // This function is called whenever the attention of the worker thread has
  // been requested.  This may happen in these cases:
  // * We are about to start the copy for the first time.  In this case, we are
  //   called from an event posted on the worker thread from the control thread
  //   by GetWorkerThreadAttention, and mAsyncCopyContext is null.
  // * We have interrupted the copy for some reason.  In this case, we are
  //   called by AsyncCopyCallback, and mAsyncCopyContext is null.
  // * We are currently executing ProcessStateChange, and attention is requested
  //   by the control thread, for example because SetTarget or Finish have been
  //   called.  In this case, we are called from from an event posted through
  //   GetWorkerThreadAttention.  While mAsyncCopyContext was always null when
  //   the event was posted, at this point mAsyncCopyContext may not be null
  //   anymore, because ProcessStateChange may have started the copy before the
  //   event that called this function was processed on the worker thread.
  // If mAsyncCopyContext is not null, we interrupt the copy and re-enter
  // through AsyncCopyCallback.  This allows us to check if, for instance, we
  // should rename the target file.  We will then restart the copy if needed.
  if (mAsyncCopyContext) {
    NS_CancelAsyncCopy(mAsyncCopyContext, NS_ERROR_ABORT);
    return NS_OK;
  }

  // Use the current shared state to determine the next operation to execute.
  rv = ProcessStateChange();
  if (NS_FAILED(rv)) {
    // If something failed while processing, terminate the operation now.
    {
      MutexAutoLock lock(mLock);

      if (NS_SUCCEEDED(mStatus)) {
        mStatus = rv;
      }
    }

    // Ensure we notify completion now that the operation failed.
    CheckCompletion();
  }

  return NS_OK;
}

// Called on the worker thread.
nsresult
BackgroundFileSaver::ProcessStateChange()
{
  nsresult rv;

  // We might have been notified because the operation is complete, verify.
  if (CheckCompletion()) {
    return NS_OK;
  }

  // Get a copy of the current shared state for the worker thread.
  nsCOMPtr<nsIFile> target;
  bool targetKeepPartial;
  {
    MutexAutoLock lock(mLock);

    target = mAssignedTarget;
    targetKeepPartial = mAssignedTargetKeepPartial;

    // From now on, another attention event needs to be posted if state changes.
    mWorkerThreadAttentionRequested = false;
  }

  // The target can only be null if it has never been assigned.  In this case,
  // there is nothing to do since we never created any output file.
  if (!target) {
    return NS_OK;
  }

  // We will append to the target file only if we already started writing it.
  bool equalToCurrent = false;
  int32_t creationIoFlags = PR_CREATE_FILE | PR_TRUNCATE;
  if (mActualTarget) {
    creationIoFlags = PR_APPEND;

    // Verify whether we have actually been instructed to use a different file.
    rv = mActualTarget->Equals(target, &equalToCurrent);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!equalToCurrent)
    {
      // We are moving the previous target file to a different location.
      nsCOMPtr<nsIFile> targetParentDir;
      rv = target->GetParent(getter_AddRefs(targetParentDir));
      NS_ENSURE_SUCCESS(rv, rv);

      nsAutoString targetName;
      rv = target->GetLeafName(targetName);
      NS_ENSURE_SUCCESS(rv, rv);

      // We must delete any existing target file before moving the current one.
      bool exists = false;
      rv = target->Exists(&exists);
      NS_ENSURE_SUCCESS(rv, rv);
      if (exists) {
        rv = target->Remove(false);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // Move the file.  If this fails, we still reference the original file
      // in mActualTarget, so that it is deleted if requested.  If this
      // succeeds, the nsIFile instance referenced by mActualTarget mutates and
      // starts pointing to the new file, but we'll discard the reference.
      rv = mActualTarget->MoveTo(targetParentDir, targetName);
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // Now we can update the actual target file name.
  mActualTarget = target;
  mActualTargetKeepPartial = targetKeepPartial;

  // Notify if the target file name actually changed.
  if (!equalToCurrent) {
    // We must clone the nsIFile instance because mActualTarget is not
    // immutable, it may change if the target is renamed later.
    nsCOMPtr<nsIFile> actualTargetToNotify;
    rv = mActualTarget->Clone(getter_AddRefs(actualTargetToNotify));
    NS_ENSURE_SUCCESS(rv, rv);

    nsRefPtr<NotifyTargetChangeRunnable> event =
      new NotifyTargetChangeRunnable(this, actualTargetToNotify);
    NS_ENSURE_TRUE(event, NS_ERROR_FAILURE);

    rv = mControlThread->Dispatch(event, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  // The pending rename operation might be the last task before finishing.
  if (CheckCompletion()) {
    return NS_OK;
  }

  // Even if the operation did not complete, the pipe input stream may be empty
  // and may have been closed already.  We detect this case using the Available
  // property, because it never returns an error if there is more data to be
  // consumed.  If the pipe input stream is closed, we just exit and wait for
  // more calls like SetTarget or Finish to be invoked on the control thread.
  // However, we still truncate the file if we are expected to do that.
  if (creationIoFlags == PR_APPEND) {
    uint64_t available;
    rv = mPipeInputStream->Available(&available);
    if (NS_FAILED(rv)) {
      return NS_OK;
    }
  }

  // Create the target file, or append to it if we already started writing it.
  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream),
                                   mActualTarget,
                                   PR_WRONLY | creationIoFlags, 0600);
  NS_ENSURE_SUCCESS(rv, rv);

  outputStream = NS_BufferOutputStream(outputStream, BUFFERED_OUTPUT_SIZE);
  if (!outputStream) {
    return NS_ERROR_FAILURE;
  }

  // Start copying our input to the target file.  No errors can be raised past
  // this point if the copy starts, since they should be handled by the thread.
  {
    MutexAutoLock lock(mLock);

    rv = NS_AsyncCopy(mPipeInputStream, outputStream, mWorkerThread,
                      NS_ASYNCCOPY_VIA_READSEGMENTS, 4096, AsyncCopyCallback,
                      this, false, true, getter_AddRefs(mAsyncCopyContext),
                      GetProgressCallback());
    if (NS_FAILED(rv)) {
      NS_WARNING("NS_AsyncCopy failed.");
      mAsyncCopyContext = nullptr;
      return rv;
    }
  }

  // If the operation succeded, we must ensure that we keep this object alive
  // for the entire duration of the copy, since only the raw pointer will be
  // provided as the argument of the AsyncCopyCallback function.  We can add the
  // reference now, after NS_AsyncCopy returned, because it always starts
  // processing asynchronously, and there is no risk that the callback is
  // invoked before we reach this point.  If the operation failed instead, then
  // AsyncCopyCallback will never be called.
  NS_ADDREF_THIS();

  return NS_OK;
}

// Called on the worker thread.
bool
BackgroundFileSaver::CheckCompletion()
{
  nsresult rv;

  MOZ_ASSERT(!mAsyncCopyContext,
             "Should not be copying when checking completion conditions.");

  bool failed = true;
  {
    MutexAutoLock lock(mLock);

    if (mComplete) {
      return true;
    }

    // If an error occurred, we don't need to do the checks in this code block,
    // and the operation can be completed immediately with a failure code.
    if (NS_SUCCEEDED(mStatus)) {
      failed = false;

      // On success, if there is a pending rename operation, we must process it
      // before finishing.  Otherwise, we can finish now if requested.
      if ((mAssignedTarget && mAssignedTarget != mActualTarget) ||
          !mFinishRequested) {
        return false;
      }

      // If completion was requested, but we still have data to write to the
      // output file, allow the copy operation to resume.  The Available getter
      // may return an error if one of the pipe's streams has been already closed.
      uint64_t available;
      rv = mPipeInputStream->Available(&available);
      if (NS_SUCCEEDED(rv) && available != 0) {
        return false;
      }
    }

    mComplete = true;
  }

  // Do a best-effort attempt to remove the file if required.
  if (failed && mActualTarget && !mActualTargetKeepPartial) {
    (void)mActualTarget->Remove(false);
  }

  // Post an event to notify that the operation completed.
  nsCOMPtr<nsIRunnable> event =
    NS_NewRunnableMethod(this, &BackgroundFileSaver::NotifySaveComplete);
  if (!event ||
      NS_FAILED(mControlThread->Dispatch(event, NS_DISPATCH_NORMAL))) {
    NS_WARNING("Unable to post completion event to the control thread.");
  }

  return true;
}

// Called on the control thread.
nsresult
BackgroundFileSaver::NotifyTargetChange(nsIFile *aTarget)
{
  if (mObserver) {
    (void)mObserver->OnTargetChange(this, aTarget);
  }

  return NS_OK;
}

// Called on the control thread.
nsresult
BackgroundFileSaver::NotifySaveComplete()
{
  nsresult status;
  {
    MutexAutoLock lock(mLock);
    status = mStatus;
  }

  if (mObserver) {
    (void)mObserver->OnSaveComplete(this, status);
  }

  // At this point, the worker thread will not process any more events, and we
  // can shut it down.  Shutting down a thread may re-enter the event loop on
  // this thread.  This is not a problem in this case, since this function is
  // called by a top-level event itself, and we have already invoked the
  // completion observer callback.  Re-entering the loop can only delay the
  // final release and destruction of this saver object, since we are keeping a
  // reference to it through the event object.
  mWorkerThread->Shutdown();

  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// BackgroundFileSaverOutputStream

NS_IMPL_THREADSAFE_ISUPPORTS4(BackgroundFileSaverOutputStream,
                              nsIBackgroundFileSaver,
                              nsIOutputStream,
                              nsIAsyncOutputStream,
                              nsIOutputStreamCallback)

BackgroundFileSaverOutputStream::BackgroundFileSaverOutputStream()
: BackgroundFileSaver()
, mAsyncWaitCallback(nullptr)
{
}

BackgroundFileSaverOutputStream::~BackgroundFileSaverOutputStream()
{
}

bool
BackgroundFileSaverOutputStream::HasInfiniteBuffer()
{
  return false;
}

nsAsyncCopyProgressFun
BackgroundFileSaverOutputStream::GetProgressCallback()
{
  return nullptr;
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::Close()
{
  return mPipeOutputStream->Close();
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::Flush()
{
  return mPipeOutputStream->Flush();
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::Write(const char *aBuf, uint32_t aCount,
                                       uint32_t *_retval)
{
  return mPipeOutputStream->Write(aBuf, aCount, _retval);
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::WriteFrom(nsIInputStream *aFromStream,
                                           uint32_t aCount, uint32_t *_retval)
{
  return mPipeOutputStream->WriteFrom(aFromStream, aCount, _retval);
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::WriteSegments(nsReadSegmentFun aReader,
                                               void *aClosure, uint32_t aCount,
                                               uint32_t *_retval)
{
  return mPipeOutputStream->WriteSegments(aReader, aClosure, aCount, _retval);
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::IsNonBlocking(bool *_retval)
{
  return mPipeOutputStream->IsNonBlocking(_retval);
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::CloseWithStatus(nsresult reason)
{
  return mPipeOutputStream->CloseWithStatus(reason);
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::AsyncWait(nsIOutputStreamCallback *aCallback,
                                           uint32_t aFlags,
                                           uint32_t aRequestedCount,
                                           nsIEventTarget *aEventTarget)
{
  NS_ENSURE_STATE(!mAsyncWaitCallback);

  mAsyncWaitCallback = aCallback;

  return mPipeOutputStream->AsyncWait(this, aFlags, aRequestedCount,
                                      aEventTarget);
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::OnOutputStreamReady(
                                 nsIAsyncOutputStream *aStream)
{
  NS_ENSURE_STATE(mAsyncWaitCallback);

  nsCOMPtr<nsIOutputStreamCallback> asyncWaitCallback = nullptr;
  asyncWaitCallback.swap(mAsyncWaitCallback);

  return asyncWaitCallback->OnOutputStreamReady(this);
}

////////////////////////////////////////////////////////////////////////////////
//// BackgroundFileSaverStreamListener

NS_IMPL_THREADSAFE_ISUPPORTS3(BackgroundFileSaverStreamListener,
                              nsIBackgroundFileSaver,
                              nsIRequestObserver,
                              nsIStreamListener)

BackgroundFileSaverStreamListener::BackgroundFileSaverStreamListener()
: BackgroundFileSaver()
, mSuspensionLock("BackgroundFileSaverStreamListener.mSuspensionLock")
, mReceivedTooMuchData(false)
, mRequest(nullptr)
, mRequestSuspended(false)
{
}

BackgroundFileSaverStreamListener::~BackgroundFileSaverStreamListener()
{
}

bool
BackgroundFileSaverStreamListener::HasInfiniteBuffer()
{
  return true;
}

nsAsyncCopyProgressFun
BackgroundFileSaverStreamListener::GetProgressCallback()
{
  return AsyncCopyProgressCallback;
}

NS_IMETHODIMP
BackgroundFileSaverStreamListener::OnStartRequest(nsIRequest *aRequest,
                                                  nsISupports *aContext)
{
  NS_ENSURE_ARG(aRequest);

  return NS_OK;
}

NS_IMETHODIMP
BackgroundFileSaverStreamListener::OnStopRequest(nsIRequest *aRequest,
                                                 nsISupports *aContext,
                                                 nsresult aStatusCode)
{
  // If an error occurred, cancel the operation immediately.  On success, wait
  // until the caller has determined whether the file should be renamed.
  if (NS_FAILED(aStatusCode)) {
    Finish(aStatusCode);
  }

  return NS_OK;
}

NS_IMETHODIMP
BackgroundFileSaverStreamListener::OnDataAvailable(nsIRequest *aRequest,
                                                   nsISupports *aContext,
                                                   nsIInputStream *aInputStream,
                                                   uint64_t aOffset,
                                                   uint32_t aCount)
{
  nsresult rv;

  NS_ENSURE_ARG(aRequest);

  // Read the requested data.  Since the pipe has an infinite buffer, we don't
  // expect any write error to occur here.
  uint32_t writeCount;
  rv = mPipeOutputStream->WriteFrom(aInputStream, aCount, &writeCount);
  NS_ENSURE_SUCCESS(rv, rv);

  // If reading from the input stream fails for any reason, the pipe will return
  // a success code, but without reading all the data.  Since we should be able
  // to read the requested data when OnDataAvailable is called, raise an error.
  if (writeCount < aCount) {
    NS_WARNING("Reading from the input stream should not have failed.");
    return NS_ERROR_UNEXPECTED;
  }

  bool stateChanged = false;
  {
    MutexAutoLock lock(mSuspensionLock);

    if (!mReceivedTooMuchData) {
      uint64_t available;
      nsresult rv = mPipeInputStream->Available(&available);
      if (NS_SUCCEEDED(rv) && available > REQUEST_SUSPEND_AT) {
        mReceivedTooMuchData = true;
        mRequest = aRequest;
        stateChanged = true;
      }
    }
  }

  if (stateChanged) {
    NotifySuspendOrResume();
  }

  return NS_OK;
}

// Called on the worker thread.
// static
void
BackgroundFileSaverStreamListener::AsyncCopyProgressCallback(void *aClosure,
                                                             uint32_t aCount)
{
  BackgroundFileSaverStreamListener *self =
    (BackgroundFileSaverStreamListener *)aClosure;

  // Wait if the control thread is in the process of suspending or resuming.
  MutexAutoLock lock(self->mSuspensionLock);

  // This function is called when some bytes are consumed by NS_AsyncCopy.  Each
  // time this happens, verify if a suspended request should be resumed, because
  // we have now consumed enough data.
  if (self->mReceivedTooMuchData) {
    uint64_t available;
    nsresult rv = self->mPipeInputStream->Available(&available);
    if (NS_FAILED(rv) || available < REQUEST_RESUME_AT) {
      self->mReceivedTooMuchData = false;

      // Post an event to verify if the request should be resumed.
      nsCOMPtr<nsIRunnable> event = NS_NewRunnableMethod(self,
        &BackgroundFileSaverStreamListener::NotifySuspendOrResume);
      if (!event || NS_FAILED(self->mControlThread->Dispatch(event,
                                                    NS_DISPATCH_NORMAL))) {
        NS_WARNING("Unable to post resume event to the control thread.");
      }
    }
  }
}

// Called on the control thread.
nsresult
BackgroundFileSaverStreamListener::NotifySuspendOrResume()
{
  // Prevent the worker thread from changing state while processing.
  MutexAutoLock lock(mSuspensionLock);

  if (mReceivedTooMuchData) {
    if (!mRequestSuspended) {
      // Try to suspend the request.  If this fails, don't try to resume later.
      if (NS_SUCCEEDED(mRequest->Suspend())) {
        mRequestSuspended = true;
      } else {
        NS_WARNING("Unable to suspend the request.");
      }
    }
  } else {
    if (mRequestSuspended) {
      // Resume the request only if we succeeded in suspending it.
      if (NS_SUCCEEDED(mRequest->Resume())) {
        mRequestSuspended = false;
      } else {
        NS_WARNING("Unable to resume the request.");
      }
    }
  }

  return NS_OK;
}

} // namespace net
} // namespace mozilla
