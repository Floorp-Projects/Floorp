/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BackgroundFileSaver.h"

#include "ScopedNSSTypes.h"
#include "mozilla/ArrayAlgorithm.h"
#include "mozilla/Casting.h"
#include "mozilla/Logging.h"
#include "mozilla/Telemetry.h"
#include "nsCOMArray.h"
#include "nsComponentManagerUtils.h"
#include "nsDependentSubstring.h"
#include "nsIAsyncInputStream.h"
#include "nsIFile.h"
#include "nsIMutableArray.h"
#include "nsIPipe.h"
#include "nsNetUtil.h"
#include "nsThreadUtils.h"
#include "pk11pub.h"
#include "secoidt.h"

#ifdef XP_WIN
#  include <windows.h>
#  include <softpub.h>
#  include <wintrust.h>
#endif  // XP_WIN

namespace mozilla {
namespace net {

// MOZ_LOG=BackgroundFileSaver:5
static LazyLogModule prlog("BackgroundFileSaver");
#define LOG(args) MOZ_LOG(prlog, mozilla::LogLevel::Debug, args)
#define LOG_ENABLED() MOZ_LOG_TEST(prlog, mozilla::LogLevel::Debug)

////////////////////////////////////////////////////////////////////////////////
//// Globals

/**
 * Buffer size for writing to the output file or reading from the input file.
 */
#define BUFFERED_IO_SIZE (1024 * 32)

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
class NotifyTargetChangeRunnable final : public Runnable {
 public:
  NotifyTargetChangeRunnable(BackgroundFileSaver* aSaver, nsIFile* aTarget)
      : Runnable("net::NotifyTargetChangeRunnable"),
        mSaver(aSaver),
        mTarget(aTarget) {}

  NS_IMETHOD Run() override { return mSaver->NotifyTargetChange(mTarget); }

 private:
  RefPtr<BackgroundFileSaver> mSaver;
  nsCOMPtr<nsIFile> mTarget;
};

////////////////////////////////////////////////////////////////////////////////
//// BackgroundFileSaver

uint32_t BackgroundFileSaver::sThreadCount = 0;
uint32_t BackgroundFileSaver::sTelemetryMaxThreadCount = 0;

BackgroundFileSaver::BackgroundFileSaver()
    : mControlEventTarget(nullptr),
      mWorkerThread(nullptr),
      mPipeOutputStream(nullptr),
      mPipeInputStream(nullptr),
      mObserver(nullptr),
      mLock("BackgroundFileSaver.mLock"),
      mWorkerThreadAttentionRequested(false),
      mFinishRequested(false),
      mComplete(false),
      mStatus(NS_OK),
      mAppend(false),
      mInitialTarget(nullptr),
      mInitialTargetKeepPartial(false),
      mRenamedTarget(nullptr),
      mRenamedTargetKeepPartial(false),
      mAsyncCopyContext(nullptr),
      mSha256Enabled(false),
      mSignatureInfoEnabled(false),
      mActualTarget(nullptr),
      mActualTargetKeepPartial(false),
      mDigestContext(nullptr) {
  LOG(("Created BackgroundFileSaver [this = %p]", this));
}

BackgroundFileSaver::~BackgroundFileSaver() {
  LOG(("Destroying BackgroundFileSaver [this = %p]", this));
}

// Called on the control thread.
nsresult BackgroundFileSaver::Init() {
  MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

  nsresult rv;

  rv = NS_NewPipe2(getter_AddRefs(mPipeInputStream),
                   getter_AddRefs(mPipeOutputStream), true, true, 0,
                   HasInfiniteBuffer() ? UINT32_MAX : 0);
  NS_ENSURE_SUCCESS(rv, rv);

  mControlEventTarget = GetCurrentThreadEventTarget();
  NS_ENSURE_TRUE(mControlEventTarget, NS_ERROR_NOT_INITIALIZED);

  rv = NS_NewNamedThread("BgFileSaver", getter_AddRefs(mWorkerThread));
  NS_ENSURE_SUCCESS(rv, rv);

  sThreadCount++;
  if (sThreadCount > sTelemetryMaxThreadCount) {
    sTelemetryMaxThreadCount = sThreadCount;
  }

  return NS_OK;
}

// Called on the control thread.
NS_IMETHODIMP
BackgroundFileSaver::GetObserver(nsIBackgroundFileSaverObserver** aObserver) {
  NS_ENSURE_ARG_POINTER(aObserver);
  *aObserver = mObserver;
  NS_IF_ADDREF(*aObserver);
  return NS_OK;
}

// Called on the control thread.
NS_IMETHODIMP
BackgroundFileSaver::SetObserver(nsIBackgroundFileSaverObserver* aObserver) {
  mObserver = aObserver;
  return NS_OK;
}

// Called on the control thread.
NS_IMETHODIMP
BackgroundFileSaver::EnableAppend() {
  MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

  MutexAutoLock lock(mLock);
  mAppend = true;

  return NS_OK;
}

// Called on the control thread.
NS_IMETHODIMP
BackgroundFileSaver::SetTarget(nsIFile* aTarget, bool aKeepPartial) {
  NS_ENSURE_ARG(aTarget);
  {
    MutexAutoLock lock(mLock);
    if (!mInitialTarget) {
      aTarget->Clone(getter_AddRefs(mInitialTarget));
      mInitialTargetKeepPartial = aKeepPartial;
    } else {
      aTarget->Clone(getter_AddRefs(mRenamedTarget));
      mRenamedTargetKeepPartial = aKeepPartial;
    }
  }

  // After the worker thread wakes up because attention is requested, it will
  // rename or create the target file as requested, and start copying data.
  return GetWorkerThreadAttention(true);
}

// Called on the control thread.
NS_IMETHODIMP
BackgroundFileSaver::Finish(nsresult aStatus) {
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

NS_IMETHODIMP
BackgroundFileSaver::EnableSha256() {
  MOZ_ASSERT(NS_IsMainThread(),
             "Can't enable sha256 or initialize NSS off the main thread");
  // Ensure Personal Security Manager is initialized. This is required for
  // PK11_* operations to work.
  nsresult rv;
  nsCOMPtr<nsISupports> nssDummy = do_GetService("@mozilla.org/psm;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mSha256Enabled = true;
  return NS_OK;
}

NS_IMETHODIMP
BackgroundFileSaver::GetSha256Hash(nsACString& aHash) {
  MOZ_ASSERT(NS_IsMainThread(), "Can't inspect sha256 off the main thread");
  // We acquire a lock because mSha256 is written on the worker thread.
  MutexAutoLock lock(mLock);
  if (mSha256.IsEmpty()) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  aHash = mSha256;
  return NS_OK;
}

NS_IMETHODIMP
BackgroundFileSaver::EnableSignatureInfo() {
  MOZ_ASSERT(NS_IsMainThread(),
             "Can't enable signature extraction off the main thread");
  // Ensure Personal Security Manager is initialized.
  nsresult rv;
  nsCOMPtr<nsISupports> nssDummy = do_GetService("@mozilla.org/psm;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  mSignatureInfoEnabled = true;
  return NS_OK;
}

NS_IMETHODIMP
BackgroundFileSaver::GetSignatureInfo(
    nsTArray<nsTArray<nsTArray<uint8_t>>>& aSignatureInfo) {
  MOZ_ASSERT(NS_IsMainThread(), "Can't inspect signature off the main thread");
  // We acquire a lock because mSignatureInfo is written on the worker thread.
  MutexAutoLock lock(mLock);
  if (!mComplete || !mSignatureInfoEnabled) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  for (const auto& signatureChain : mSignatureInfo) {
    aSignatureInfo.AppendElement(TransformIntoNewArray(
        signatureChain, [](const auto& element) { return element.Clone(); }));
  }
  return NS_OK;
}

// Called on the control thread.
nsresult BackgroundFileSaver::GetWorkerThreadAttention(
    bool aShouldInterruptCopy) {
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
    rv = mWorkerThread->Dispatch(
        NewRunnableMethod("net::BackgroundFileSaver::ProcessAttention", this,
                          &BackgroundFileSaver::ProcessAttention),
        NS_DISPATCH_NORMAL);
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
void BackgroundFileSaver::AsyncCopyCallback(void* aClosure, nsresult aStatus) {
  // We called NS_ADDREF_THIS when NS_AsyncCopy started, to keep the object
  // alive even if other references disappeared.  At the end of this method,
  // we've finished using the object and can safely release our reference.
  RefPtr<BackgroundFileSaver> self =
      dont_AddRef((BackgroundFileSaver*)aClosure);
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
}

// Called on the worker thread.
nsresult BackgroundFileSaver::ProcessAttention() {
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
nsresult BackgroundFileSaver::ProcessStateChange() {
  nsresult rv;

  // We might have been notified because the operation is complete, verify.
  if (CheckCompletion()) {
    return NS_OK;
  }

  // Get a copy of the current shared state for the worker thread.
  nsCOMPtr<nsIFile> initialTarget;
  bool initialTargetKeepPartial;
  nsCOMPtr<nsIFile> renamedTarget;
  bool renamedTargetKeepPartial;
  bool sha256Enabled;
  bool append;
  {
    MutexAutoLock lock(mLock);

    initialTarget = mInitialTarget;
    initialTargetKeepPartial = mInitialTargetKeepPartial;
    renamedTarget = mRenamedTarget;
    renamedTargetKeepPartial = mRenamedTargetKeepPartial;
    sha256Enabled = mSha256Enabled;
    append = mAppend;

    // From now on, another attention event needs to be posted if state changes.
    mWorkerThreadAttentionRequested = false;
  }

  // The initial target can only be null if it has never been assigned.  In this
  // case, there is nothing to do since we never created any output file.
  if (!initialTarget) {
    return NS_OK;
  }

  // Determine if we are processing the attention request for the first time.
  bool isContinuation = !!mActualTarget;
  if (!isContinuation) {
    // Assign the target file for the first time.
    mActualTarget = initialTarget;
    mActualTargetKeepPartial = initialTargetKeepPartial;
  }

  // Verify whether we have actually been instructed to use a different file.
  // This may happen the first time this function is executed, if SetTarget was
  // called two times before the worker thread processed the attention request.
  bool equalToCurrent = false;
  if (renamedTarget) {
    rv = mActualTarget->Equals(renamedTarget, &equalToCurrent);
    NS_ENSURE_SUCCESS(rv, rv);
    if (!equalToCurrent) {
      // If we were asked to rename the file but the initial file did not exist,
      // we simply create the file in the renamed location.  We avoid this check
      // if we have already started writing the output file ourselves.
      bool exists = true;
      if (!isContinuation) {
        rv = mActualTarget->Exists(&exists);
        NS_ENSURE_SUCCESS(rv, rv);
      }
      if (exists) {
        // We are moving the previous target file to a different location.
        nsCOMPtr<nsIFile> renamedTargetParentDir;
        rv = renamedTarget->GetParent(getter_AddRefs(renamedTargetParentDir));
        NS_ENSURE_SUCCESS(rv, rv);

        nsAutoString renamedTargetName;
        rv = renamedTarget->GetLeafName(renamedTargetName);
        NS_ENSURE_SUCCESS(rv, rv);

        // We must delete any existing target file before moving the current
        // one.
        rv = renamedTarget->Exists(&exists);
        NS_ENSURE_SUCCESS(rv, rv);
        if (exists) {
          rv = renamedTarget->Remove(false);
          NS_ENSURE_SUCCESS(rv, rv);
        }

        // Move the file.  If this fails, we still reference the original file
        // in mActualTarget, so that it is deleted if requested.  If this
        // succeeds, the nsIFile instance referenced by mActualTarget mutates
        // and starts pointing to the new file, but we'll discard the reference.
        rv = mActualTarget->MoveTo(renamedTargetParentDir, renamedTargetName);
        NS_ENSURE_SUCCESS(rv, rv);
      }

      // We should not only update the mActualTarget with renameTarget when
      // they point to the different files.
      // In this way, if mActualTarget and renamedTarget point to the same file
      // with different addresses, "CheckCompletion()" will return false
      // forever.
    }

    // Update mActualTarget with renameTarget,
    // even if they point to the same file.
    mActualTarget = renamedTarget;
    mActualTargetKeepPartial = renamedTargetKeepPartial;
  }

  // Notify if the target file name actually changed.
  if (!equalToCurrent) {
    // We must clone the nsIFile instance because mActualTarget is not
    // immutable, it may change if the target is renamed later.
    nsCOMPtr<nsIFile> actualTargetToNotify;
    rv = mActualTarget->Clone(getter_AddRefs(actualTargetToNotify));
    NS_ENSURE_SUCCESS(rv, rv);

    RefPtr<NotifyTargetChangeRunnable> event =
        new NotifyTargetChangeRunnable(this, actualTargetToNotify);
    NS_ENSURE_TRUE(event, NS_ERROR_FAILURE);

    rv = mControlEventTarget->Dispatch(event, NS_DISPATCH_NORMAL);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  if (isContinuation) {
    // The pending rename operation might be the last task before finishing. We
    // may return here only if we have already created the target file.
    if (CheckCompletion()) {
      return NS_OK;
    }

    // Even if the operation did not complete, the pipe input stream may be
    // empty and may have been closed already.  We detect this case using the
    // Available property, because it never returns an error if there is more
    // data to be consumed.  If the pipe input stream is closed, we just exit
    // and wait for more calls like SetTarget or Finish to be invoked on the
    // control thread.  However, we still truncate the file or create the
    // initial digest context if we are expected to do that.
    uint64_t available;
    rv = mPipeInputStream->Available(&available);
    if (NS_FAILED(rv)) {
      return NS_OK;
    }
  }

  // Create the digest context if requested and NSS hasn't been shut down.
  if (sha256Enabled && !mDigestContext) {
    mDigestContext =
        UniquePK11Context(PK11_CreateDigestContext(SEC_OID_SHA256));
    NS_ENSURE_TRUE(mDigestContext, NS_ERROR_OUT_OF_MEMORY);
  }

  // When we are requested to append to an existing file, we should read the
  // existing data and ensure we include it as part of the final hash.
  if (mDigestContext && append && !isContinuation) {
    nsCOMPtr<nsIInputStream> inputStream;
    rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), mActualTarget,
                                    PR_RDONLY | nsIFile::OS_READAHEAD);
    if (rv != NS_ERROR_FILE_NOT_FOUND) {
      NS_ENSURE_SUCCESS(rv, rv);

      char buffer[BUFFERED_IO_SIZE];
      while (true) {
        uint32_t count;
        rv = inputStream->Read(buffer, BUFFERED_IO_SIZE, &count);
        NS_ENSURE_SUCCESS(rv, rv);

        if (count == 0) {
          // We reached the end of the file.
          break;
        }

        nsresult rv = MapSECStatus(
            PK11_DigestOp(mDigestContext.get(),
                          BitwiseCast<unsigned char*, char*>(buffer), count));
        NS_ENSURE_SUCCESS(rv, rv);
      }

      rv = inputStream->Close();
      NS_ENSURE_SUCCESS(rv, rv);
    }
  }

  // We will append to the initial target file only if it was requested by the
  // caller, but we'll always append on subsequent accesses to the target file.
  int32_t creationIoFlags;
  if (isContinuation) {
    creationIoFlags = PR_APPEND;
  } else {
    creationIoFlags = (append ? PR_APPEND : PR_TRUNCATE) | PR_CREATE_FILE;
  }

  // Create the target file, or append to it if we already started writing it.
  // The 0600 permissions are used while the file is being downloaded, and for
  // interrupted downloads. Those may be located in the system temporary
  // directory, as well as the target directory, and generally have a ".part"
  // extension. Those part files should never be group or world-writable even
  // if the umask allows it.
  nsCOMPtr<nsIOutputStream> outputStream;
  rv = NS_NewLocalFileOutputStream(getter_AddRefs(outputStream), mActualTarget,
                                   PR_WRONLY | creationIoFlags, 0600);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIOutputStream> bufferedStream;
  rv = NS_NewBufferedOutputStream(getter_AddRefs(bufferedStream),
                                  outputStream.forget(), BUFFERED_IO_SIZE);
  NS_ENSURE_SUCCESS(rv, rv);
  outputStream = bufferedStream;

  // Wrap the output stream so that it feeds the digest context if needed.
  if (mDigestContext) {
    // Constructing the DigestOutputStream cannot fail. Passing mDigestContext
    // to DigestOutputStream is safe, because BackgroundFileSaver always
    // outlives the outputStream. BackgroundFileSaver is reference-counted
    // before the call to AsyncCopy, and mDigestContext is never destroyed
    // before AsyncCopyCallback.
    outputStream = new DigestOutputStream(outputStream, mDigestContext.get());
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

  // If the operation succeeded, we must ensure that we keep this object alive
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
bool BackgroundFileSaver::CheckCompletion() {
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

      // We did not incur in an error, so we must determine if we can stop now.
      // If the Finish method has not been called, we can just continue now.
      if (!mFinishRequested) {
        return false;
      }

      // We can only stop when all the operations requested by the control
      // thread have been processed.  First, we check whether we have processed
      // the first SetTarget call, if any.  Then, we check whether we have
      // processed any rename requested by subsequent SetTarget calls.
      if ((mInitialTarget && !mActualTarget) ||
          (mRenamedTarget && mRenamedTarget != mActualTarget)) {
        return false;
      }

      // If we still have data to write to the output file, allow the copy
      // operation to resume.  The Available getter may return an error if one
      // of the pipe's streams has been already closed.
      uint64_t available;
      rv = mPipeInputStream->Available(&available);
      if (NS_SUCCEEDED(rv) && available != 0) {
        return false;
      }
    }

    mComplete = true;
  }

  // Ensure we notify completion now that the operation finished.
  // Do a best-effort attempt to remove the file if required.
  if (failed && mActualTarget && !mActualTargetKeepPartial) {
    (void)mActualTarget->Remove(false);
  }

  // Finish computing the hash
  if (!failed && mDigestContext) {
    Digest d;
    rv = d.End(SEC_OID_SHA256, mDigestContext);
    if (NS_SUCCEEDED(rv)) {
      MutexAutoLock lock(mLock);
      mSha256 = nsDependentCSubstring(
          BitwiseCast<char*, unsigned char*>(d.get().data), d.get().len);
    }
  }

  // Compute the signature of the binary. ExtractSignatureInfo doesn't do
  // anything on non-Windows platforms except return an empty nsIArray.
  if (!failed && mActualTarget) {
    nsString filePath;
    mActualTarget->GetTarget(filePath);
    nsresult rv = ExtractSignatureInfo(filePath);
    if (NS_FAILED(rv)) {
      LOG(("Unable to extract signature information [this = %p].", this));
    } else {
      LOG(("Signature extraction success! [this = %p]", this));
    }
  }

  // Post an event to notify that the operation completed.
  if (NS_FAILED(mControlEventTarget->Dispatch(
          NewRunnableMethod("BackgroundFileSaver::NotifySaveComplete", this,
                            &BackgroundFileSaver::NotifySaveComplete),
          NS_DISPATCH_NORMAL))) {
    NS_WARNING("Unable to post completion event to the control thread.");
  }

  return true;
}

// Called on the control thread.
nsresult BackgroundFileSaver::NotifyTargetChange(nsIFile* aTarget) {
  if (mObserver) {
    (void)mObserver->OnTargetChange(this, aTarget);
  }

  return NS_OK;
}

// Called on the control thread.
nsresult BackgroundFileSaver::NotifySaveComplete() {
  MOZ_ASSERT(NS_IsMainThread(), "This should be called on the main thread");

  nsresult status;
  {
    MutexAutoLock lock(mLock);
    status = mStatus;
  }

  if (mObserver) {
    (void)mObserver->OnSaveComplete(this, status);
    // If mObserver keeps alive an enclosure that captures `this`, we'll have a
    // cycle that won't be caught by the cycle-collector, so we need to break it
    // when we're done here (see bug 1444265).
    mObserver = nullptr;
  }

  // At this point, the worker thread will not process any more events, and we
  // can shut it down.  Shutting down a thread may re-enter the event loop on
  // this thread.  This is not a problem in this case, since this function is
  // called by a top-level event itself, and we have already invoked the
  // completion observer callback.  Re-entering the loop can only delay the
  // final release and destruction of this saver object, since we are keeping a
  // reference to it through the event object.
  mWorkerThread->Shutdown();

  sThreadCount--;

  // When there are no more active downloads, we consider the download session
  // finished. We record the maximum number of concurrent downloads reached
  // during the session in a telemetry histogram, and we reset the maximum
  // thread counter for the next download session
  if (sThreadCount == 0) {
    Telemetry::Accumulate(Telemetry::BACKGROUNDFILESAVER_THREAD_COUNT,
                          sTelemetryMaxThreadCount);
    sTelemetryMaxThreadCount = 0;
  }

  return NS_OK;
}

nsresult BackgroundFileSaver::ExtractSignatureInfo(const nsAString& filePath) {
  MOZ_ASSERT(!NS_IsMainThread(), "Cannot extract signature on main thread");
  {
    MutexAutoLock lock(mLock);
    if (!mSignatureInfoEnabled) {
      return NS_OK;
    }
  }
#ifdef XP_WIN
  // Setup the file to check.
  WINTRUST_FILE_INFO fileToCheck = {0};
  fileToCheck.cbStruct = sizeof(WINTRUST_FILE_INFO);
  fileToCheck.pcwszFilePath = filePath.Data();
  fileToCheck.hFile = nullptr;
  fileToCheck.pgKnownSubject = nullptr;

  // We want to check it is signed and trusted.
  WINTRUST_DATA trustData = {0};
  trustData.cbStruct = sizeof(trustData);
  trustData.pPolicyCallbackData = nullptr;
  trustData.pSIPClientData = nullptr;
  trustData.dwUIChoice = WTD_UI_NONE;
  trustData.fdwRevocationChecks = WTD_REVOKE_NONE;
  trustData.dwUnionChoice = WTD_CHOICE_FILE;
  trustData.dwStateAction = WTD_STATEACTION_VERIFY;
  trustData.hWVTStateData = nullptr;
  trustData.pwszURLReference = nullptr;
  // Disallow revocation checks over the network
  trustData.dwProvFlags = WTD_CACHE_ONLY_URL_RETRIEVAL;
  // no UI
  trustData.dwUIContext = 0;
  trustData.pFile = &fileToCheck;

  // The WINTRUST_ACTION_GENERIC_VERIFY_V2 policy verifies that the certificate
  // chains up to a trusted root CA and has appropriate permissions to sign
  // code.
  GUID policyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
  // Check if the file is signed by something that is trusted. If the file is
  // not signed, this is a no-op.
  LONG ret = WinVerifyTrust(nullptr, &policyGUID, &trustData);
  CRYPT_PROVIDER_DATA* cryptoProviderData = nullptr;
  // According to the Windows documentation, we should check against 0 instead
  // of ERROR_SUCCESS, which is an HRESULT.
  if (ret == 0) {
    cryptoProviderData = WTHelperProvDataFromStateData(trustData.hWVTStateData);
  }
  if (cryptoProviderData) {
    // Lock because signature information is read on the main thread.
    MutexAutoLock lock(mLock);
    LOG(("Downloaded trusted and signed file [this = %p].", this));
    // A binary may have multiple signers. Each signer may have multiple certs
    // in the chain.
    for (DWORD i = 0; i < cryptoProviderData->csSigners; ++i) {
      const CERT_CHAIN_CONTEXT* certChainContext =
          cryptoProviderData->pasSigners[i].pChainContext;
      if (!certChainContext) {
        break;
      }
      for (DWORD j = 0; j < certChainContext->cChain; ++j) {
        const CERT_SIMPLE_CHAIN* certSimpleChain =
            certChainContext->rgpChain[j];
        if (!certSimpleChain) {
          break;
        }

        nsTArray<nsTArray<uint8_t>> certList;
        bool extractionSuccess = true;
        for (DWORD k = 0; k < certSimpleChain->cElement; ++k) {
          CERT_CHAIN_ELEMENT* certChainElement = certSimpleChain->rgpElement[k];
          if (certChainElement->pCertContext->dwCertEncodingType !=
              X509_ASN_ENCODING) {
            continue;
          }
          nsTArray<uint8_t> cert;
          cert.AppendElements(certChainElement->pCertContext->pbCertEncoded,
                              certChainElement->pCertContext->cbCertEncoded);
          certList.AppendElement(std::move(cert));
        }
        if (extractionSuccess) {
          mSignatureInfo.AppendElement(std::move(certList));
        }
      }
    }
    // Free the provider data if cryptoProviderData is not null.
    trustData.dwStateAction = WTD_STATEACTION_CLOSE;
    WinVerifyTrust(nullptr, &policyGUID, &trustData);
  } else {
    LOG(("Downloaded unsigned or untrusted file [this = %p].", this));
  }
#endif
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
//// BackgroundFileSaverOutputStream

NS_IMPL_ISUPPORTS(BackgroundFileSaverOutputStream, nsIBackgroundFileSaver,
                  nsIOutputStream, nsIAsyncOutputStream,
                  nsIOutputStreamCallback)

BackgroundFileSaverOutputStream::BackgroundFileSaverOutputStream()
    : BackgroundFileSaver(), mAsyncWaitCallback(nullptr) {}

bool BackgroundFileSaverOutputStream::HasInfiniteBuffer() { return false; }

nsAsyncCopyProgressFun BackgroundFileSaverOutputStream::GetProgressCallback() {
  return nullptr;
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::Close() { return mPipeOutputStream->Close(); }

NS_IMETHODIMP
BackgroundFileSaverOutputStream::Flush() { return mPipeOutputStream->Flush(); }

NS_IMETHODIMP
BackgroundFileSaverOutputStream::Write(const char* aBuf, uint32_t aCount,
                                       uint32_t* _retval) {
  return mPipeOutputStream->Write(aBuf, aCount, _retval);
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::WriteFrom(nsIInputStream* aFromStream,
                                           uint32_t aCount, uint32_t* _retval) {
  return mPipeOutputStream->WriteFrom(aFromStream, aCount, _retval);
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::WriteSegments(nsReadSegmentFun aReader,
                                               void* aClosure, uint32_t aCount,
                                               uint32_t* _retval) {
  return mPipeOutputStream->WriteSegments(aReader, aClosure, aCount, _retval);
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::IsNonBlocking(bool* _retval) {
  return mPipeOutputStream->IsNonBlocking(_retval);
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::CloseWithStatus(nsresult reason) {
  return mPipeOutputStream->CloseWithStatus(reason);
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::AsyncWait(nsIOutputStreamCallback* aCallback,
                                           uint32_t aFlags,
                                           uint32_t aRequestedCount,
                                           nsIEventTarget* aEventTarget) {
  NS_ENSURE_STATE(!mAsyncWaitCallback);

  mAsyncWaitCallback = aCallback;

  return mPipeOutputStream->AsyncWait(this, aFlags, aRequestedCount,
                                      aEventTarget);
}

NS_IMETHODIMP
BackgroundFileSaverOutputStream::OnOutputStreamReady(
    nsIAsyncOutputStream* aStream) {
  NS_ENSURE_STATE(mAsyncWaitCallback);

  nsCOMPtr<nsIOutputStreamCallback> asyncWaitCallback = nullptr;
  asyncWaitCallback.swap(mAsyncWaitCallback);

  return asyncWaitCallback->OnOutputStreamReady(this);
}

////////////////////////////////////////////////////////////////////////////////
//// BackgroundFileSaverStreamListener

NS_IMPL_ISUPPORTS(BackgroundFileSaverStreamListener, nsIBackgroundFileSaver,
                  nsIRequestObserver, nsIStreamListener)

BackgroundFileSaverStreamListener::BackgroundFileSaverStreamListener()
    : BackgroundFileSaver(),
      mSuspensionLock("BackgroundFileSaverStreamListener.mSuspensionLock"),
      mReceivedTooMuchData(false),
      mRequest(nullptr),
      mRequestSuspended(false) {}

bool BackgroundFileSaverStreamListener::HasInfiniteBuffer() { return true; }

nsAsyncCopyProgressFun
BackgroundFileSaverStreamListener::GetProgressCallback() {
  return AsyncCopyProgressCallback;
}

NS_IMETHODIMP
BackgroundFileSaverStreamListener::OnStartRequest(nsIRequest* aRequest) {
  NS_ENSURE_ARG(aRequest);

  return NS_OK;
}

NS_IMETHODIMP
BackgroundFileSaverStreamListener::OnStopRequest(nsIRequest* aRequest,
                                                 nsresult aStatusCode) {
  // If an error occurred, cancel the operation immediately.  On success, wait
  // until the caller has determined whether the file should be renamed.
  if (NS_FAILED(aStatusCode)) {
    Finish(aStatusCode);
  }

  return NS_OK;
}

NS_IMETHODIMP
BackgroundFileSaverStreamListener::OnDataAvailable(nsIRequest* aRequest,
                                                   nsIInputStream* aInputStream,
                                                   uint64_t aOffset,
                                                   uint32_t aCount) {
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
void BackgroundFileSaverStreamListener::AsyncCopyProgressCallback(
    void* aClosure, uint32_t aCount) {
  BackgroundFileSaverStreamListener* self =
      (BackgroundFileSaverStreamListener*)aClosure;

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
      if (NS_FAILED(self->mControlEventTarget->Dispatch(
              NewRunnableMethod(
                  "BackgroundFileSaverStreamListener::NotifySuspendOrResume",
                  self,
                  &BackgroundFileSaverStreamListener::NotifySuspendOrResume),
              NS_DISPATCH_NORMAL))) {
        NS_WARNING("Unable to post resume event to the control thread.");
      }
    }
  }
}

// Called on the control thread.
nsresult BackgroundFileSaverStreamListener::NotifySuspendOrResume() {
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

////////////////////////////////////////////////////////////////////////////////
//// DigestOutputStream
NS_IMPL_ISUPPORTS(DigestOutputStream, nsIOutputStream)

DigestOutputStream::DigestOutputStream(nsIOutputStream* aStream,
                                       PK11Context* aContext)
    : mOutputStream(aStream), mDigestContext(aContext) {
  MOZ_ASSERT(mDigestContext, "Can't have null digest context");
  MOZ_ASSERT(mOutputStream, "Can't have null output stream");
}

NS_IMETHODIMP
DigestOutputStream::Close() { return mOutputStream->Close(); }

NS_IMETHODIMP
DigestOutputStream::Flush() { return mOutputStream->Flush(); }

NS_IMETHODIMP
DigestOutputStream::Write(const char* aBuf, uint32_t aCount, uint32_t* retval) {
  nsresult rv = MapSECStatus(PK11_DigestOp(
      mDigestContext, BitwiseCast<const unsigned char*, const char*>(aBuf),
      aCount));
  NS_ENSURE_SUCCESS(rv, rv);

  return mOutputStream->Write(aBuf, aCount, retval);
}

NS_IMETHODIMP
DigestOutputStream::WriteFrom(nsIInputStream* aFromStream, uint32_t aCount,
                              uint32_t* retval) {
  // Not supported. We could read the stream to a buf, call DigestOp on the
  // result, seek back and pass the stream on, but it's not worth it since our
  // application (NS_AsyncCopy) doesn't invoke this on the sink.
  MOZ_CRASH("DigestOutputStream::WriteFrom not implemented");
}

NS_IMETHODIMP
DigestOutputStream::WriteSegments(nsReadSegmentFun aReader, void* aClosure,
                                  uint32_t aCount, uint32_t* retval) {
  MOZ_CRASH("DigestOutputStream::WriteSegments not implemented");
}

NS_IMETHODIMP
DigestOutputStream::IsNonBlocking(bool* retval) {
  return mOutputStream->IsNonBlocking(retval);
}

#undef LOG_ENABLED

}  // namespace net
}  // namespace mozilla
