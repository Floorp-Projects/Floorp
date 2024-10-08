/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=4 sts=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "nsLoadGroup.h"

#include "nsArrayEnumerator.h"
#include "nsCOMArray.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "mozilla/Logging.h"
#include "nsString.h"
#include "nsTArray.h"
#include "mozilla/Telemetry.h"
#include "nsIHttpChannelInternal.h"
#include "nsITimedChannel.h"
#include "nsIInterfaceRequestor.h"
#include "nsIRequestObserver.h"
#include "CacheObserver.h"
#include "MainThreadUtils.h"
#include "RequestContextService.h"
#include "mozilla/glean/GleanMetrics.h"
#include "mozilla/StoragePrincipalHelper.h"
#include "mozilla/Unused.h"
#include "mozilla/net/NeckoCommon.h"
#include "mozilla/net/NeckoChild.h"
#include "mozilla/StaticPrefs_network.h"

namespace mozilla {
namespace net {

//
// Log module for nsILoadGroup logging...
//
// To enable logging (see prlog.h for full details):
//
//    set MOZ_LOG=LoadGroup:5
//    set MOZ_LOG_FILE=network.log
//
// This enables LogLevel::Debug level information and places all output in
// the file network.log.
//
static LazyLogModule gLoadGroupLog("LoadGroup");
#undef LOG
#define LOG(args) MOZ_LOG(gLoadGroupLog, mozilla::LogLevel::Debug, args)

////////////////////////////////////////////////////////////////////////////////

class RequestMapEntry : public PLDHashEntryHdr {
 public:
  explicit RequestMapEntry(nsIRequest* aRequest) : mKey(aRequest) {}

  nsCOMPtr<nsIRequest> mKey;
};

static bool RequestHashMatchEntry(const PLDHashEntryHdr* entry,
                                  const void* key) {
  const RequestMapEntry* e = static_cast<const RequestMapEntry*>(entry);
  const nsIRequest* request = static_cast<const nsIRequest*>(key);

  return e->mKey == request;
}

static void RequestHashClearEntry(PLDHashTable* table, PLDHashEntryHdr* entry) {
  RequestMapEntry* e = static_cast<RequestMapEntry*>(entry);

  // An entry is being cleared, let the entry do its own cleanup.
  e->~RequestMapEntry();
}

static void RequestHashInitEntry(PLDHashEntryHdr* entry, const void* key) {
  const nsIRequest* const_request = static_cast<const nsIRequest*>(key);
  nsIRequest* request = const_cast<nsIRequest*>(const_request);

  // Initialize the entry with placement new
  new (entry) RequestMapEntry(request);
}

static const PLDHashTableOps sRequestHashOps = {
    PLDHashTable::HashVoidPtrKeyStub, RequestHashMatchEntry,
    PLDHashTable::MoveEntryStub, RequestHashClearEntry, RequestHashInitEntry};

static void RescheduleRequest(nsIRequest* aRequest, int32_t delta) {
  nsCOMPtr<nsISupportsPriority> p = do_QueryInterface(aRequest);
  if (p) p->AdjustPriority(delta);
}

nsLoadGroup::nsLoadGroup()
    : mRequests(&sRequestHashOps, sizeof(RequestMapEntry)) {
  LOG(("LOADGROUP [%p]: Created.\n", this));
}

nsLoadGroup::~nsLoadGroup() {
  DebugOnly<nsresult> rv =
      CancelWithReason(NS_BINDING_ABORTED, "nsLoadGroup::~nsLoadGroup"_ns);
  NS_ASSERTION(NS_SUCCEEDED(rv), "Cancel failed");

  mDefaultLoadRequest = nullptr;

  if (mRequestContext && !mExternalRequestContext) {
    mRequestContextService->RemoveRequestContext(mRequestContext->GetID());
    if (IsNeckoChild() && gNeckoChild && gNeckoChild->CanSend()) {
      gNeckoChild->SendRemoveRequestContext(mRequestContext->GetID());
    }
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
    Unused << os->RemoveObserver(this, "last-pb-context-exited");
  }

  LOG(("LOADGROUP [%p]: Destroyed.\n", this));
}

////////////////////////////////////////////////////////////////////////////////
// nsISupports methods:

NS_IMPL_ISUPPORTS(nsLoadGroup, nsILoadGroup, nsILoadGroupChild, nsIRequest,
                  nsISupportsPriority, nsISupportsWeakReference, nsIObserver)

////////////////////////////////////////////////////////////////////////////////
// nsIRequest methods:

NS_IMETHODIMP
nsLoadGroup::GetName(nsACString& result) {
  // XXX is this the right "name" for a load group?

  if (!mDefaultLoadRequest) {
    result.Truncate();
    return NS_OK;
  }

  return mDefaultLoadRequest->GetName(result);
}

NS_IMETHODIMP
nsLoadGroup::IsPending(bool* aResult) {
  *aResult = mForegroundCount > 0;
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetStatus(nsresult* status) {
  if (NS_SUCCEEDED(mStatus) && mDefaultLoadRequest) {
    return mDefaultLoadRequest->GetStatus(status);
  }

  *status = mStatus;
  return NS_OK;
}

static bool AppendRequestsToArray(PLDHashTable* aTable,
                                  nsTArray<nsIRequest*>* aArray) {
  for (auto iter = aTable->Iter(); !iter.Done(); iter.Next()) {
    auto* e = static_cast<RequestMapEntry*>(iter.Get());
    nsIRequest* request = e->mKey;
    MOZ_DIAGNOSTIC_ASSERT(request, "Null key in mRequests PLDHashTable entry");

    // XXX(Bug 1631371) Check if this should use a fallible operation as it
    // pretended earlier.
    aArray->AppendElement(request);
    NS_ADDREF(request);
  }

  if (aArray->Length() != aTable->EntryCount()) {
    for (uint32_t i = 0, len = aArray->Length(); i < len; ++i) {
      NS_RELEASE((*aArray)[i]);
    }
    return false;
  }
  return true;
}

NS_IMETHODIMP nsLoadGroup::SetCanceledReason(const nsACString& aReason) {
  return SetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP nsLoadGroup::GetCanceledReason(nsACString& aReason) {
  return GetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP nsLoadGroup::CancelWithReason(nsresult aStatus,
                                            const nsACString& aReason) {
  return CancelWithReasonImpl(aStatus, aReason);
}

NS_IMETHODIMP
nsLoadGroup::Cancel(nsresult status) {
  MOZ_ASSERT(NS_IsMainThread());

  NS_ASSERTION(NS_FAILED(status), "shouldn't cancel with a success code");
  nsresult rv;
  uint32_t count = mRequests.EntryCount();

  AutoTArray<nsIRequest*, 8> requests;

  if (!AppendRequestsToArray(&mRequests, &requests)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  // set the load group status to our cancel status while we cancel
  // all our requests...once the cancel is done, we'll reset it...
  //
  mStatus = status;

  // Set the flag indicating that the loadgroup is being canceled...  This
  // prevents any new channels from being added during the operation.
  //
  mIsCanceling = true;

  nsresult firstError = NS_OK;
  while (count > 0) {
    nsCOMPtr<nsIRequest> request = requests.ElementAt(--count);

    NS_ASSERTION(request, "NULL request found in list.");

    if (!mRequests.Search(request)) {
      // |request| was removed already
      // We need to null out the entry in the request array so we don't try
      // to notify the observers for this request.
      nsCOMPtr<nsIRequest> request = dont_AddRef(requests.ElementAt(count));
      requests.ElementAt(count) = nullptr;

      continue;
    }

    if (MOZ_LOG_TEST(gLoadGroupLog, LogLevel::Debug)) {
      nsAutoCString nameStr;
      request->GetName(nameStr);
      LOG(("LOADGROUP [%p]: Canceling request %p %s.\n", this, request.get(),
           nameStr.get()));
    }

    // Cancel the request...
    rv = request->CancelWithReason(status, mCanceledReason);

    // Remember the first failure and return it...
    if (NS_FAILED(rv) && NS_SUCCEEDED(firstError)) firstError = rv;

    if (NS_FAILED(RemoveRequestFromHashtable(request, status))) {
      // It's possible that request->Cancel causes the request to be removed
      // from the loadgroup causing RemoveRequestFromHashtable to fail.
      // In that case we shouldn't call NotifyRemovalObservers or decrement
      // mForegroundCount since that has already happened.
      nsCOMPtr<nsIRequest> request = dont_AddRef(requests.ElementAt(count));
      requests.ElementAt(count) = nullptr;

      continue;
    }
  }

  for (count = requests.Length(); count > 0;) {
    nsCOMPtr<nsIRequest> request = dont_AddRef(requests.ElementAt(--count));
    (void)NotifyRemovalObservers(request, status);
  }

  if (mRequestContext) {
    Unused << mRequestContext->CancelTailPendingRequests(status);
  }

#if defined(DEBUG)
  NS_ASSERTION(mRequests.EntryCount() == 0, "Request list is not empty.");
  NS_ASSERTION(mForegroundCount == 0, "Foreground URLs are active.");
#endif

  mStatus = NS_OK;
  mIsCanceling = false;

  return firstError;
}

NS_IMETHODIMP
nsLoadGroup::Suspend() {
  nsresult rv, firstError;
  uint32_t count = mRequests.EntryCount();

  AutoTArray<nsIRequest*, 8> requests;

  if (!AppendRequestsToArray(&mRequests, &requests)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  firstError = NS_OK;
  //
  // Operate the elements from back to front so that if items get
  // get removed from the list it won't affect our iteration
  //
  while (count > 0) {
    nsCOMPtr<nsIRequest> request = dont_AddRef(requests.ElementAt(--count));

    NS_ASSERTION(request, "NULL request found in list.");
    if (!request) continue;

    if (MOZ_LOG_TEST(gLoadGroupLog, LogLevel::Debug)) {
      nsAutoCString nameStr;
      request->GetName(nameStr);
      LOG(("LOADGROUP [%p]: Suspending request %p %s.\n", this, request.get(),
           nameStr.get()));
    }

    // Suspend the request...
    rv = request->Suspend();

    // Remember the first failure and return it...
    if (NS_FAILED(rv) && NS_SUCCEEDED(firstError)) firstError = rv;
  }

  return firstError;
}

NS_IMETHODIMP
nsLoadGroup::Resume() {
  nsresult rv, firstError;
  uint32_t count = mRequests.EntryCount();

  AutoTArray<nsIRequest*, 8> requests;

  if (!AppendRequestsToArray(&mRequests, &requests)) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  firstError = NS_OK;
  //
  // Operate the elements from back to front so that if items get
  // get removed from the list it won't affect our iteration
  //
  while (count > 0) {
    nsCOMPtr<nsIRequest> request = dont_AddRef(requests.ElementAt(--count));

    NS_ASSERTION(request, "NULL request found in list.");
    if (!request) continue;

    if (MOZ_LOG_TEST(gLoadGroupLog, LogLevel::Debug)) {
      nsAutoCString nameStr;
      request->GetName(nameStr);
      LOG(("LOADGROUP [%p]: Resuming request %p %s.\n", this, request.get(),
           nameStr.get()));
    }

    // Resume the request...
    rv = request->Resume();

    // Remember the first failure and return it...
    if (NS_FAILED(rv) && NS_SUCCEEDED(firstError)) firstError = rv;
  }

  return firstError;
}

NS_IMETHODIMP
nsLoadGroup::GetLoadFlags(uint32_t* aLoadFlags) {
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetLoadFlags(uint32_t aLoadFlags) {
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return GetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
nsLoadGroup::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return SetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP
nsLoadGroup::GetLoadGroup(nsILoadGroup** loadGroup) {
  nsCOMPtr<nsILoadGroup> result = mLoadGroup;
  result.forget(loadGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetLoadGroup(nsILoadGroup* loadGroup) {
  mLoadGroup = loadGroup;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsILoadGroup methods:

NS_IMETHODIMP
nsLoadGroup::GetDefaultLoadRequest(nsIRequest** aRequest) {
  nsCOMPtr<nsIRequest> result = mDefaultLoadRequest;
  result.forget(aRequest);
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetDefaultLoadRequest(nsIRequest* aRequest) {
  LOG(("nsLoadGroup::SetDefaultLoadRequest this=%p default-request=%p", this,
       aRequest));

  mDefaultLoadRequest = aRequest;
  // Inherit the group load flags from the default load request
  if (mDefaultLoadRequest) {
    mDefaultLoadRequest->GetLoadFlags(&mLoadFlags);
    //
    // Mask off any bits that are not part of the nsIRequest flags.
    // in particular, nsIChannel::LOAD_DOCUMENT_URI...
    //
    mLoadFlags &= nsIRequest::LOAD_REQUESTMASK;

    nsCOMPtr<nsITimedChannel> timedChannel = do_QueryInterface(aRequest);
    mDefaultLoadIsTimed = timedChannel != nullptr;
    if (mDefaultLoadIsTimed) {
      timedChannel->GetChannelCreation(&mDefaultRequestCreationTime);
      timedChannel->SetTimingEnabled(true);
    }
  }
  // Else, do not change the group's load flags (see bug 95981)
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::AddRequest(nsIRequest* request, nsISupports* ctxt) {
  nsresult rv;

  if (MOZ_LOG_TEST(gLoadGroupLog, LogLevel::Debug)) {
    nsAutoCString nameStr;
    request->GetName(nameStr);
    LOG(("LOADGROUP [%p]: Adding request %p %s (count=%d).\n", this, request,
         nameStr.get(), mRequests.EntryCount()));
  }

  NS_ASSERTION(!mRequests.Search(request),
               "Entry added to loadgroup twice, don't do that");

  //
  // Do not add the channel, if the loadgroup is being canceled...
  //
  if (mIsCanceling) {
    LOG(
        ("LOADGROUP [%p]: AddChannel() ABORTED because LoadGroup is"
         " being canceled!!\n",
         this));

    return NS_BINDING_ABORTED;
  }

  nsLoadFlags flags;
  // if the request is the default load request or if the default load
  // request is null, then the load group should inherit its load flags from
  // the request, but also we need to enforce defaultLoadFlags.
  if (mDefaultLoadRequest == request || !mDefaultLoadRequest) {
    rv = MergeDefaultLoadFlags(request, flags);
  } else {
    rv = MergeLoadFlags(request, flags);
  }
  if (NS_FAILED(rv)) return rv;

  //
  // Add the request to the list of active requests...
  //

  auto* entry = static_cast<RequestMapEntry*>(mRequests.Add(request, fallible));
  if (!entry) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  if (mPriority != 0) RescheduleRequest(request, mPriority);

  nsCOMPtr<nsITimedChannel> timedChannel = do_QueryInterface(request);
  if (timedChannel) timedChannel->SetTimingEnabled(true);

  bool foreground = !(flags & nsIRequest::LOAD_BACKGROUND);
  if (foreground) {
    // Update the count of foreground URIs..
    mForegroundCount += 1;
  }

  if (foreground || mNotifyObserverAboutBackgroundRequests) {
    //
    // Fire the OnStartRequest notification out to the observer...
    //
    // If the notification fails then DO NOT add the request to
    // the load group.
    //
    nsCOMPtr<nsIRequestObserver> observer = do_QueryReferent(mObserver);
    if (observer) {
      LOG(
          ("LOADGROUP [%p]: Firing OnStartRequest for request %p."
           "(foreground count=%d).\n",
           this, request, mForegroundCount));

      rv = observer->OnStartRequest(request);
      if (NS_FAILED(rv)) {
        LOG(("LOADGROUP [%p]: OnStartRequest for request %p FAILED.\n", this,
             request));
        //
        // The URI load has been canceled by the observer.  Clean up
        // the damage...
        //

        mRequests.Remove(request);

        rv = NS_OK;

        if (foreground) {
          mForegroundCount -= 1;
        }
      }
    }

    // Ensure that we're part of our loadgroup while pending
    if (foreground && mForegroundCount == 1 && mLoadGroup) {
      mLoadGroup->AddRequest(this, nullptr);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsLoadGroup::RemoveRequest(nsIRequest* request, nsISupports* ctxt,
                           nsresult aStatus) {
  // Make sure we have a owning reference to the request we're about
  // to remove.
  nsCOMPtr<nsIRequest> kungFuDeathGrip(request);

  nsresult rv = RemoveRequestFromHashtable(request, aStatus);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NotifyRemovalObservers(request, aStatus);
}

nsresult nsLoadGroup::RemoveRequestFromHashtable(nsIRequest* request,
                                                 nsresult aStatus) {
  NS_ENSURE_ARG_POINTER(request);
  nsresult rv;

  if (MOZ_LOG_TEST(gLoadGroupLog, LogLevel::Debug)) {
    nsAutoCString nameStr;
    request->GetName(nameStr);
    LOG(("LOADGROUP [%p]: Removing request %p %s status %" PRIx32
         " (count=%d).\n",
         this, request, nameStr.get(), static_cast<uint32_t>(aStatus),
         mRequests.EntryCount() - 1));
  }

  //
  // Remove the request from the group.  If this fails, it means that
  // the request was *not* in the group so do not update the foreground
  // count or it will get messed up...
  //
  auto* entry = static_cast<RequestMapEntry*>(mRequests.Search(request));

  if (!entry) {
    LOG(("LOADGROUP [%p]: Unable to remove request %p. Not in group!\n", this,
         request));

    return NS_ERROR_FAILURE;
  }

  mRequests.RemoveEntry(entry);

  // Collect telemetry stats only when default request is a timed channel.
  // Don't include failed requests in the timing statistics.
  if (mDefaultLoadIsTimed && NS_SUCCEEDED(aStatus)) {
    nsCOMPtr<nsITimedChannel> timedChannel = do_QueryInterface(request);
    if (timedChannel) {
      // Figure out if this request was served from the cache
      ++mTimedRequests;
      TimeStamp timeStamp;
      rv = timedChannel->GetCacheReadStart(&timeStamp);
      if (NS_SUCCEEDED(rv) && !timeStamp.IsNull()) {
        ++mCachedRequests;
      }

      rv = timedChannel->GetAsyncOpen(&timeStamp);
      if (NS_SUCCEEDED(rv) && !timeStamp.IsNull()) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::HTTP_SUBITEM_OPEN_LATENCY_TIME,
            mDefaultRequestCreationTime, timeStamp);
      }

      rv = timedChannel->GetResponseStart(&timeStamp);
      if (NS_SUCCEEDED(rv) && !timeStamp.IsNull()) {
        Telemetry::AccumulateTimeDelta(
            Telemetry::HTTP_SUBITEM_FIRST_BYTE_LATENCY_TIME,
            mDefaultRequestCreationTime, timeStamp);
      }

      TelemetryReportChannel(timedChannel, false);
    }
  }

  if (mRequests.EntryCount() == 0) {
    TelemetryReport();
  }

  return NS_OK;
}

nsresult nsLoadGroup::NotifyRemovalObservers(nsIRequest* request,
                                             nsresult aStatus) {
  NS_ENSURE_ARG_POINTER(request);
  // Undo any group priority delta...
  if (mPriority != 0) RescheduleRequest(request, -mPriority);

  nsLoadFlags flags;
  nsresult rv = request->GetLoadFlags(&flags);
  if (NS_FAILED(rv)) return rv;

  bool foreground = !(flags & nsIRequest::LOAD_BACKGROUND);
  if (foreground) {
    NS_ASSERTION(mForegroundCount > 0, "ForegroundCount messed up");
    mForegroundCount -= 1;
  }

  if (foreground || mNotifyObserverAboutBackgroundRequests) {
    // Fire the OnStopRequest out to the observer...
    nsCOMPtr<nsIRequestObserver> observer = do_QueryReferent(mObserver);
    if (observer) {
      LOG(
          ("LOADGROUP [%p]: Firing OnStopRequest for request %p."
           "(foreground count=%d).\n",
           this, request, mForegroundCount));

      rv = observer->OnStopRequest(request, aStatus);

      if (NS_FAILED(rv)) {
        LOG(("LOADGROUP [%p]: OnStopRequest for request %p FAILED.\n", this,
             request));
      }
    }

    // If that was the last request -> remove ourselves from loadgroup
    if (foreground && mForegroundCount == 0 && mLoadGroup) {
      mLoadGroup->RemoveRequest(this, nullptr, aStatus);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsLoadGroup::GetRequests(nsISimpleEnumerator** aRequests) {
  nsCOMArray<nsIRequest> requests;
  requests.SetCapacity(mRequests.EntryCount());

  for (auto iter = mRequests.Iter(); !iter.Done(); iter.Next()) {
    auto* e = static_cast<RequestMapEntry*>(iter.Get());
    requests.AppendObject(e->mKey);
  }

  return NS_NewArrayEnumerator(aRequests, requests, NS_GET_IID(nsIRequest));
}

NS_IMETHODIMP
nsLoadGroup::SetGroupObserver(nsIRequestObserver* aObserver) {
  SetGroupObserver(aObserver, false);
  return NS_OK;
}

void nsLoadGroup::SetGroupObserver(nsIRequestObserver* aObserver,
                                   bool aIncludeBackgroundRequests) {
  mObserver = do_GetWeakReference(aObserver);
  mNotifyObserverAboutBackgroundRequests = aIncludeBackgroundRequests;
}

NS_IMETHODIMP
nsLoadGroup::GetGroupObserver(nsIRequestObserver** aResult) {
  nsCOMPtr<nsIRequestObserver> observer = do_QueryReferent(mObserver);
  observer.forget(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetActiveCount(uint32_t* aResult) {
  *aResult = mForegroundCount;
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetNotificationCallbacks(nsIInterfaceRequestor** aCallbacks) {
  NS_ENSURE_ARG_POINTER(aCallbacks);
  nsCOMPtr<nsIInterfaceRequestor> callbacks = mCallbacks;
  callbacks.forget(aCallbacks);
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetNotificationCallbacks(nsIInterfaceRequestor* aCallbacks) {
  mCallbacks = aCallbacks;
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetRequestContextID(uint64_t* aRCID) {
  if (!mRequestContext) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  *aRCID = mRequestContext->GetID();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsILoadGroupChild methods:

NS_IMETHODIMP
nsLoadGroup::GetParentLoadGroup(nsILoadGroup** aParentLoadGroup) {
  *aParentLoadGroup = nullptr;
  nsCOMPtr<nsILoadGroup> parent = do_QueryReferent(mParentLoadGroup);
  if (!parent) return NS_OK;
  parent.forget(aParentLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetParentLoadGroup(nsILoadGroup* aParentLoadGroup) {
  mParentLoadGroup = do_GetWeakReference(aParentLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetChildLoadGroup(nsILoadGroup** aChildLoadGroup) {
  *aChildLoadGroup = do_AddRef(this).take();
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetRootLoadGroup(nsILoadGroup** aRootLoadGroup) {
  // first recursively try the root load group of our parent
  nsCOMPtr<nsILoadGroupChild> ancestor = do_QueryReferent(mParentLoadGroup);
  if (ancestor) return ancestor->GetRootLoadGroup(aRootLoadGroup);

  // next recursively try the root load group of our own load grop
  ancestor = do_QueryInterface(mLoadGroup);
  if (ancestor) return ancestor->GetRootLoadGroup(aRootLoadGroup);

  // finally just return this
  *aRootLoadGroup = do_AddRef(this).take();
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// nsISupportsPriority methods:

NS_IMETHODIMP
nsLoadGroup::GetPriority(int32_t* aValue) {
  *aValue = mPriority;
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetPriority(int32_t aValue) {
  return AdjustPriority(aValue - mPriority);
}

NS_IMETHODIMP
nsLoadGroup::AdjustPriority(int32_t aDelta) {
  // Update the priority for each request that supports nsISupportsPriority
  if (aDelta != 0) {
    mPriority += aDelta;
    for (auto iter = mRequests.Iter(); !iter.Done(); iter.Next()) {
      auto* e = static_cast<RequestMapEntry*>(iter.Get());
      RescheduleRequest(e->mKey, aDelta);
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetDefaultLoadFlags(uint32_t* aFlags) {
  *aFlags = mDefaultLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::SetDefaultLoadFlags(uint32_t aFlags) {
  mDefaultLoadFlags = aFlags;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////

void nsLoadGroup::TelemetryReport() {
  nsresult defaultStatus = NS_ERROR_INVALID_ARG;
  // We should only report HTTP_PAGE_* telemetry if the defaultRequest was
  // actually successful.
  if (mDefaultLoadRequest) {
    mDefaultLoadRequest->GetStatus(&defaultStatus);
  }
  if (mDefaultLoadIsTimed && NS_SUCCEEDED(defaultStatus)) {
    Telemetry::Accumulate(Telemetry::HTTP_REQUEST_PER_PAGE, mTimedRequests);
    if (mTimedRequests) {
      Telemetry::Accumulate(Telemetry::HTTP_REQUEST_PER_PAGE_FROM_CACHE,
                            mCachedRequests * 100 / mTimedRequests);
    }

    nsCOMPtr<nsITimedChannel> timedChannel =
        do_QueryInterface(mDefaultLoadRequest);
    if (timedChannel) TelemetryReportChannel(timedChannel, true);
  }

  mTimedRequests = 0;
  mCachedRequests = 0;
  mDefaultLoadIsTimed = false;
}

void nsLoadGroup::TelemetryReportChannel(nsITimedChannel* aTimedChannel,
                                         bool aDefaultRequest) {
  nsresult rv;
  bool timingEnabled;
  rv = aTimedChannel->GetTimingEnabled(&timingEnabled);
  if (NS_FAILED(rv) || !timingEnabled) return;

  TimeStamp asyncOpen;
  rv = aTimedChannel->GetAsyncOpen(&asyncOpen);
  // We do not check !asyncOpen.IsNull() bellow, prevent ASSERTIONs this way
  if (NS_FAILED(rv) || asyncOpen.IsNull()) return;

  TimeStamp cacheReadStart;
  rv = aTimedChannel->GetCacheReadStart(&cacheReadStart);
  if (NS_FAILED(rv)) return;

  TimeStamp cacheReadEnd;
  rv = aTimedChannel->GetCacheReadEnd(&cacheReadEnd);
  if (NS_FAILED(rv)) return;

  TimeStamp domainLookupStart;
  rv = aTimedChannel->GetDomainLookupStart(&domainLookupStart);
  if (NS_FAILED(rv)) return;

  TimeStamp domainLookupEnd;
  rv = aTimedChannel->GetDomainLookupEnd(&domainLookupEnd);
  if (NS_FAILED(rv)) return;

  TimeStamp connectStart;
  rv = aTimedChannel->GetConnectStart(&connectStart);
  if (NS_FAILED(rv)) return;

  TimeStamp secureConnectionStart;
  rv = aTimedChannel->GetSecureConnectionStart(&secureConnectionStart);
  if (NS_FAILED(rv)) return;

  TimeStamp connectEnd;
  rv = aTimedChannel->GetConnectEnd(&connectEnd);
  if (NS_FAILED(rv)) return;

  TimeStamp requestStart;
  rv = aTimedChannel->GetRequestStart(&requestStart);
  if (NS_FAILED(rv)) return;

  TimeStamp responseStart;
  rv = aTimedChannel->GetResponseStart(&responseStart);
  if (NS_FAILED(rv)) return;

  TimeStamp responseEnd;
  rv = aTimedChannel->GetResponseEnd(&responseEnd);
  if (NS_FAILED(rv)) return;

  bool useHttp3 = false;
  bool supportHttp3 = false;
  nsCOMPtr<nsIHttpChannelInternal> httpChannel =
      do_QueryInterface(aTimedChannel);
  if (httpChannel) {
    uint32_t major;
    uint32_t minor;
    if (NS_SUCCEEDED(httpChannel->GetResponseVersion(&major, &minor))) {
      useHttp3 = major == 3;
      if (major == 2) {
        if (NS_FAILED(httpChannel->GetSupportsHTTP3(&supportHttp3))) {
          supportHttp3 = false;
        }
      }
    }
  }

#define HTTP_REQUEST_HISTOGRAMS(prefix)                                        \
  if (!domainLookupStart.IsNull()) {                                           \
    Telemetry::AccumulateTimeDelta(Telemetry::HTTP_##prefix##_DNS_ISSUE_TIME,  \
                                   asyncOpen, domainLookupStart);              \
  }                                                                            \
                                                                               \
  if (!domainLookupStart.IsNull() && !domainLookupEnd.IsNull()) {              \
    Telemetry::AccumulateTimeDelta(Telemetry::HTTP_##prefix##_DNS_LOOKUP_TIME, \
                                   domainLookupStart, domainLookupEnd);        \
  }                                                                            \
                                                                               \
  if (!secureConnectionStart.IsNull() && !connectEnd.IsNull()) {               \
    Telemetry::AccumulateTimeDelta(Telemetry::HTTP_##prefix##_TLS_HANDSHAKE,   \
                                   secureConnectionStart, connectEnd);         \
  }                                                                            \
                                                                               \
  if (!connectStart.IsNull() && !connectEnd.IsNull()) {                        \
    Telemetry::AccumulateTimeDelta(                                            \
        Telemetry::HTTP_##prefix##_TCP_CONNECTION_2, connectStart,             \
        connectEnd);                                                           \
  }                                                                            \
                                                                               \
  if (!requestStart.IsNull() && !responseEnd.IsNull()) {                       \
    Telemetry::AccumulateTimeDelta(                                            \
        Telemetry::HTTP_##prefix##_OPEN_TO_FIRST_SENT, asyncOpen,              \
        requestStart);                                                         \
                                                                               \
    Telemetry::AccumulateTimeDelta(                                            \
        Telemetry::HTTP_##prefix##_FIRST_SENT_TO_LAST_RECEIVED, requestStart,  \
        responseEnd);                                                          \
                                                                               \
    if (cacheReadStart.IsNull() && !responseStart.IsNull()) {                  \
      Telemetry::AccumulateTimeDelta(                                          \
          Telemetry::HTTP_##prefix##_OPEN_TO_FIRST_RECEIVED, asyncOpen,        \
          responseStart);                                                      \
    }                                                                          \
  }                                                                            \
                                                                               \
  if (!cacheReadStart.IsNull() && !cacheReadEnd.IsNull()) {                    \
    Telemetry::AccumulateTimeDelta(                                            \
        Telemetry::HTTP_##prefix##_OPEN_TO_FIRST_FROM_CACHE_V2, asyncOpen,     \
        cacheReadStart);                                                       \
                                                                               \
    Telemetry::AccumulateTimeDelta(                                            \
        Telemetry::HTTP_##prefix##_CACHE_READ_TIME_V2, cacheReadStart,         \
        cacheReadEnd);                                                         \
                                                                               \
    if (!requestStart.IsNull() && !responseEnd.IsNull()) {                     \
      Telemetry::AccumulateTimeDelta(Telemetry::HTTP_##prefix##_REVALIDATION,  \
                                     requestStart, responseEnd);               \
    }                                                                          \
  }                                                                            \
                                                                               \
  if (!cacheReadEnd.IsNull()) {                                                \
    Telemetry::AccumulateTimeDelta(                                            \
        Telemetry::HTTP_##prefix##_COMPLETE_LOAD_V2, asyncOpen, cacheReadEnd); \
    Telemetry::AccumulateTimeDelta(                                            \
        Telemetry::HTTP_##prefix##_COMPLETE_LOAD_CACHED_V2, asyncOpen,         \
        cacheReadEnd);                                                         \
  } else if (!responseEnd.IsNull()) {                                          \
    Telemetry::AccumulateTimeDelta(                                            \
        Telemetry::HTTP_##prefix##_COMPLETE_LOAD_V2, asyncOpen, responseEnd);  \
    Telemetry::AccumulateTimeDelta(                                            \
        Telemetry::HTTP_##prefix##_COMPLETE_LOAD_NET_V2, asyncOpen,            \
        responseEnd);                                                          \
  }

  // Glean instrumentation of metrics previously collected via Geckoview
  // Streaming.
  if (aDefaultRequest) {
    if (!cacheReadStart.IsNull() && !cacheReadEnd.IsNull()) {
      mozilla::glean::network::first_from_cache.AccumulateRawDuration(
          cacheReadStart - asyncOpen);
    }
    if (!connectEnd.IsNull()) {
      if (!connectStart.IsNull()) {
        mozilla::glean::network::tcp_connection.AccumulateRawDuration(
            connectEnd - connectStart);
      }
      if (!secureConnectionStart.IsNull()) {
        mozilla::glean::network::tls_handshake.AccumulateRawDuration(
            connectEnd - secureConnectionStart);
      }
    }
    if (!domainLookupStart.IsNull()) {
      mozilla::glean::network::dns_start.AccumulateRawDuration(
          domainLookupStart - asyncOpen);
      if (!domainLookupEnd.IsNull()) {
        mozilla::glean::network::dns_end.AccumulateRawDuration(
            domainLookupEnd - domainLookupStart);
      }
    }
  }

  if (aDefaultRequest) {
    HTTP_REQUEST_HISTOGRAMS(PAGE)
  } else {
    HTTP_REQUEST_HISTOGRAMS(SUB)
  }

  if ((useHttp3 || supportHttp3) && cacheReadStart.IsNull() &&
      cacheReadEnd.IsNull()) {
    nsCString key = (useHttp3) ? ((aDefaultRequest) ? "uses_http3_page"_ns
                                                    : "uses_http3_sub"_ns)
                               : ((aDefaultRequest) ? "supports_http3_page"_ns
                                                    : "supports_http3_sub"_ns);

    if (!secureConnectionStart.IsNull() && !connectEnd.IsNull()) {
      Telemetry::AccumulateTimeDelta(Telemetry::HTTP3_TLS_HANDSHAKE, key,
                                     secureConnectionStart, connectEnd);
    }

    if (supportHttp3 && !connectStart.IsNull() && !connectEnd.IsNull()) {
      Telemetry::AccumulateTimeDelta(Telemetry::SUP_HTTP3_TCP_CONNECTION, key,
                                     connectStart, connectEnd);
    }

    if (!requestStart.IsNull() && !responseEnd.IsNull()) {
      Telemetry::AccumulateTimeDelta(Telemetry::HTTP3_OPEN_TO_FIRST_SENT, key,
                                     asyncOpen, requestStart);

      Telemetry::AccumulateTimeDelta(
          Telemetry::HTTP3_FIRST_SENT_TO_LAST_RECEIVED, key, requestStart,
          responseEnd);

      if (!responseStart.IsNull()) {
        Telemetry::AccumulateTimeDelta(Telemetry::HTTP3_OPEN_TO_FIRST_RECEIVED,
                                       key, asyncOpen, responseStart);
      }

      if (!responseEnd.IsNull()) {
        Telemetry::AccumulateTimeDelta(Telemetry::HTTP3_COMPLETE_LOAD, key,
                                       asyncOpen, responseEnd);
      }
    }
  }

  bool hasHTTPSRR = false;
  if (httpChannel && NS_SUCCEEDED(httpChannel->GetHasHTTPSRR(&hasHTTPSRR)) &&
      cacheReadStart.IsNull() && cacheReadEnd.IsNull() &&
      !requestStart.IsNull()) {
    TimeDuration elapsed = requestStart - asyncOpen;
    if (hasHTTPSRR) {
      if (aDefaultRequest) {
        glean::networking::http_channel_page_open_to_first_sent_https_rr
            .AccumulateRawDuration(elapsed);
      } else {
        glean::networking::http_channel_sub_open_to_first_sent_https_rr
            .AccumulateRawDuration(elapsed);
      }
    } else {
      if (aDefaultRequest) {
        glean::networking::http_channel_page_open_to_first_sent
            .AccumulateRawDuration(elapsed);
      } else {
        glean::networking::http_channel_sub_open_to_first_sent
            .AccumulateRawDuration(elapsed);
      }
    }
  }

#undef HTTP_REQUEST_HISTOGRAMS
}

nsresult nsLoadGroup::MergeLoadFlags(nsIRequest* aRequest,
                                     nsLoadFlags& outFlags) {
  nsresult rv;
  nsLoadFlags flags, oldFlags;

  rv = aRequest->GetLoadFlags(&flags);
  if (NS_FAILED(rv)) {
    return rv;
  }

  oldFlags = flags;

  // Inherit some bits...
  flags |= mLoadFlags & kInheritedLoadFlags;

  // ... and force the default flags.
  flags |= mDefaultLoadFlags;

  if (flags != oldFlags) {
    rv = aRequest->SetLoadFlags(flags);
  }

  outFlags = flags;
  return rv;
}

nsresult nsLoadGroup::MergeDefaultLoadFlags(nsIRequest* aRequest,
                                            nsLoadFlags& outFlags) {
  nsresult rv;
  nsLoadFlags flags, oldFlags;

  rv = aRequest->GetLoadFlags(&flags);
  if (NS_FAILED(rv)) {
    return rv;
  }

  oldFlags = flags;
  // ... and force the default flags.
  flags |= mDefaultLoadFlags;

  if (flags != oldFlags) {
    rv = aRequest->SetLoadFlags(flags);
  }
  outFlags = flags;
  return rv;
}

nsresult nsLoadGroup::Init() {
  mRequestContextService = RequestContextService::GetOrCreate();
  if (mRequestContextService) {
    Unused << mRequestContextService->NewRequestContext(
        getter_AddRefs(mRequestContext));
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  NS_ENSURE_STATE(os);

  Unused << os->AddObserver(this, "last-pb-context-exited", true);

  return NS_OK;
}

nsresult nsLoadGroup::InitWithRequestContextId(
    const uint64_t& aRequestContextId) {
  mRequestContextService = RequestContextService::GetOrCreate();
  if (mRequestContextService) {
    Unused << mRequestContextService->GetRequestContext(
        aRequestContextId, getter_AddRefs(mRequestContext));
  }
  mExternalRequestContext = true;

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  NS_ENSURE_STATE(os);

  Unused << os->AddObserver(this, "last-pb-context-exited", true);

  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::Observe(nsISupports* aSubject, const char* aTopic,
                     const char16_t* aData) {
  MOZ_ASSERT(!strcmp(aTopic, "last-pb-context-exited"));

  OriginAttributes attrs;
  StoragePrincipalHelper::GetRegularPrincipalOriginAttributes(this, attrs);
  if (attrs.mPrivateBrowsingId == 0) {
    return NS_OK;
  }

  mBrowsingContextDiscarded = true;
  return NS_OK;
}

NS_IMETHODIMP
nsLoadGroup::GetIsBrowsingContextDiscarded(bool* aIsBrowsingContextDiscarded) {
  *aIsBrowsingContextDiscarded = mBrowsingContextDiscarded;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla

#undef LOG
