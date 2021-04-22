/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ArrayUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/AutoRestore.h"

#include "nsProtocolProxyService.h"
#include "nsProxyInfo.h"
#include "nsIClassInfoImpl.h"
#include "nsIIOService.h"
#include "nsIObserverService.h"
#include "nsIProtocolHandler.h"
#include "nsIProtocolProxyCallback.h"
#include "nsIChannel.h"
#include "nsICancelable.h"
#include "nsIDNSService.h"
#include "nsPIDNSService.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsContentUtils.h"
#include "nsThreadUtils.h"
#include "nsQueryObject.h"
#include "nsSOCKSIOLayer.h"
#include "nsString.h"
#include "nsNetUtil.h"
#include "nsNetCID.h"
#include "plstr.h"
#include "prnetdb.h"
#include "nsPACMan.h"
#include "nsProxyRelease.h"
#include "mozilla/Mutex.h"
#include "mozilla/CondVar.h"
#include "nsISystemProxySettings.h"
#include "nsINetworkLinkService.h"
#include "nsIHttpChannelInternal.h"
#include "mozilla/dom/nsMixedContentBlocker.h"
#include "mozilla/Logging.h"
#include "mozilla/StaticPrefs_network.h"
#include "mozilla/Tokenizer.h"
#include "mozilla/Unused.h"
#include "mozilla/StaticPrefs_network.h"

//----------------------------------------------------------------------------

namespace mozilla {
namespace net {

extern const char kProxyType_HTTP[];
extern const char kProxyType_HTTPS[];
extern const char kProxyType_SOCKS[];
extern const char kProxyType_SOCKS4[];
extern const char kProxyType_SOCKS5[];
extern const char kProxyType_DIRECT[];

#undef LOG
#define LOG(args) MOZ_LOG(gProxyLog, LogLevel::Debug, args)

//----------------------------------------------------------------------------

#define PROXY_PREF_BRANCH "network.proxy"
#define PROXY_PREF(x) PROXY_PREF_BRANCH "." x

//----------------------------------------------------------------------------

// This structure is intended to be allocated on the stack
struct nsProtocolInfo {
  nsAutoCString scheme;
  uint32_t flags;
  int32_t defaultPort;
};

//----------------------------------------------------------------------------

// Return the channel's proxy URI, or if it doesn't exist, the
// channel's main URI.
static nsresult GetProxyURI(nsIChannel* channel, nsIURI** aOut) {
  nsresult rv = NS_OK;
  nsCOMPtr<nsIURI> proxyURI;
  nsCOMPtr<nsIHttpChannelInternal> httpChannel(do_QueryInterface(channel));
  if (httpChannel) {
    rv = httpChannel->GetProxyURI(getter_AddRefs(proxyURI));
  }
  if (!proxyURI) {
    rv = channel->GetURI(getter_AddRefs(proxyURI));
  }
  if (NS_FAILED(rv)) {
    return rv;
  }
  proxyURI.forget(aOut);
  return NS_OK;
}

//-----------------------------------------------------------------------------

nsProtocolProxyService::FilterLink::FilterLink(uint32_t p,
                                               nsIProtocolProxyFilter* f)
    : position(p), filter(f), channelFilter(nullptr) {
  LOG(("nsProtocolProxyService::FilterLink::FilterLink %p, filter=%p", this,
       f));
}
nsProtocolProxyService::FilterLink::FilterLink(
    uint32_t p, nsIProtocolProxyChannelFilter* cf)
    : position(p), filter(nullptr), channelFilter(cf) {
  LOG(("nsProtocolProxyService::FilterLink::FilterLink %p, channel-filter=%p",
       this, cf));
}

nsProtocolProxyService::FilterLink::~FilterLink() {
  LOG(("nsProtocolProxyService::FilterLink::~FilterLink %p", this));
}

//-----------------------------------------------------------------------------

// The nsPACManCallback portion of this implementation should be run
// on the main thread - so call nsPACMan::AsyncGetProxyForURI() with
// a true mainThreadResponse parameter.
class nsAsyncResolveRequest final : public nsIRunnable,
                                    public nsPACManCallback,
                                    public nsICancelable {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  nsAsyncResolveRequest(nsProtocolProxyService* pps, nsIChannel* channel,
                        uint32_t aResolveFlags,
                        nsIProtocolProxyCallback* callback)
      : mStatus(NS_OK),
        mDispatched(false),
        mResolveFlags(aResolveFlags),
        mPPS(pps),
        mXPComPPS(pps),
        mChannel(channel),
        mCallback(callback) {
    NS_ASSERTION(mCallback, "null callback");
  }

 private:
  ~nsAsyncResolveRequest() {
    if (!NS_IsMainThread()) {
      // these xpcom pointers might need to be proxied back to the
      // main thread to delete safely, but if this request had its
      // callbacks called normally they will all be null and this is a nop

      if (mChannel) {
        NS_ReleaseOnMainThread("nsAsyncResolveRequest::mChannel",
                               mChannel.forget());
      }

      if (mCallback) {
        NS_ReleaseOnMainThread("nsAsyncResolveRequest::mCallback",
                               mCallback.forget());
      }

      if (mProxyInfo) {
        NS_ReleaseOnMainThread("nsAsyncResolveRequest::mProxyInfo",
                               mProxyInfo.forget());
      }

      if (mXPComPPS) {
        NS_ReleaseOnMainThread("nsAsyncResolveRequest::mXPComPPS",
                               mXPComPPS.forget());
      }
    }
  }

  // Helper class to loop over all registered asynchronous filters.
  // There is a cycle between nsAsyncResolveRequest and this class that
  // is broken after the last filter has called back on this object.
  class AsyncApplyFilters final : public nsIProxyProtocolFilterResult,
                                  public nsIRunnable,
                                  public nsICancelable {
    // The reference counter is thread-safe, but the processing logic is
    // considered single thread only.  We want the counter be thread safe,
    // since this class can be released on a background thread.
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIPROXYPROTOCOLFILTERRESULT
    NS_DECL_NSIRUNNABLE
    NS_DECL_NSICANCELABLE

    typedef std::function<nsresult(nsAsyncResolveRequest*, nsIProxyInfo*, bool)>
        Callback;

    explicit AsyncApplyFilters(nsProtocolInfo& aInfo,
                               Callback const& aCallback);
    // This method starts the processing or filters.  If all of them
    // answer synchronously (call back from within applyFilters) this method
    // will return immediately and the returning result will carry return
    // result of the callback given in constructor.
    // This method is looping the registered filters (that have been copied
    // locally) as long as an answer from a filter is obtained synchronously.
    // Note that filters are processed serially to let them build a list
    // of proxy info.
    nsresult AsyncProcess(nsAsyncResolveRequest* aRequest);

   private:
    typedef nsProtocolProxyService::FilterLink FilterLink;

    virtual ~AsyncApplyFilters();
    // Processes the next filter and loops until a filter is successfully
    // called on or it has called back to us.
    nsresult ProcessNextFilter();
    // Called after the last filter has been processed (=called back or failed
    // to be called on)
    nsresult Finish();

    nsProtocolInfo mInfo;
    // This is nullified before we call back on the request or when
    // Cancel() on this object has been called to break the cycle
    // and signal to stop.
    RefPtr<nsAsyncResolveRequest> mRequest;
    Callback mCallback;
    // A shallow snapshot of filters as they were registered at the moment
    // we started to process filters for the given resolve request.
    nsTArray<RefPtr<FilterLink>> mFiltersCopy;

    nsTArray<RefPtr<FilterLink>>::index_type mNextFilterIndex;
    // true when we are calling ProcessNextFilter() from inside AsyncProcess(),
    // false otherwise.
    bool mProcessingInLoop;
    // true after a filter called back to us with a result, dropped to false
    // just before we call a filter.
    bool mFilterCalledBack;

    // This keeps the initial value we pass to the first filter in line and also
    // collects the result from each filter call.
    nsCOMPtr<nsIProxyInfo> mProxyInfo;

    // The logic is written as non-thread safe, assert single-thread usage.
    nsCOMPtr<nsISerialEventTarget> mProcessingThread;
  };

  void EnsureResolveFlagsMatch() {
    nsCOMPtr<nsIProxyInfo> proxyInfo = mProxyInfo;
    while (proxyInfo) {
      proxyInfo->SetResolveFlags(mResolveFlags);
      proxyInfo->GetFailoverProxy(getter_AddRefs(proxyInfo));
    }
  }

 public:
  nsresult ProcessLocally(nsProtocolInfo& info, nsIProxyInfo* pi,
                          bool isSyncOK) {
    SetResult(NS_OK, pi);

    auto consumeFiltersResult = [isSyncOK](nsAsyncResolveRequest* ctx,
                                           nsIProxyInfo* pi,
                                           bool aCalledAsync) -> nsresult {
      ctx->SetResult(NS_OK, pi);
      if (isSyncOK || aCalledAsync) {
        ctx->Run();
        return NS_OK;
      }

      return ctx->DispatchCallback();
    };

    mAsyncFilterApplier = new AsyncApplyFilters(info, consumeFiltersResult);
    // may call consumeFiltersResult() directly
    return mAsyncFilterApplier->AsyncProcess(this);
  }

  void SetResult(nsresult status, nsIProxyInfo* pi) {
    mStatus = status;
    mProxyInfo = pi;
  }

  NS_IMETHOD Run() override {
    if (mCallback) DoCallback();
    return NS_OK;
  }

  NS_IMETHOD Cancel(nsresult reason) override {
    NS_ENSURE_ARG(NS_FAILED(reason));

    if (mAsyncFilterApplier) {
      mAsyncFilterApplier->Cancel(reason);
    }

    // If we've already called DoCallback then, nothing more to do.
    if (!mCallback) return NS_OK;

    SetResult(reason, nullptr);
    return DispatchCallback();
  }

  nsresult DispatchCallback() {
    if (mDispatched)  // Only need to dispatch once
      return NS_OK;

    nsresult rv = NS_DispatchToCurrentThread(this);
    if (NS_FAILED(rv))
      NS_WARNING("unable to dispatch callback event");
    else {
      mDispatched = true;
      return NS_OK;
    }

    mCallback = nullptr;  // break possible reference cycle
    return rv;
  }

 private:
  // Called asynchronously, so we do not need to post another PLEvent
  // before calling DoCallback.
  void OnQueryComplete(nsresult status, const nsACString& pacString,
                       const nsACString& newPACURL) override {
    // If we've already called DoCallback then, nothing more to do.
    if (!mCallback) return;

    // Provided we haven't been canceled...
    if (mStatus == NS_OK) {
      mStatus = status;
      mPACString = pacString;
      mPACURL = newPACURL;
    }

    // In the cancelation case, we may still have another PLEvent in
    // the queue that wants to call DoCallback.  No need to wait for
    // it, just run the callback now.
    DoCallback();
  }

  void DoCallback() {
    bool pacAvailable = true;
    if (mStatus == NS_ERROR_NOT_AVAILABLE && !mProxyInfo) {
      // If the PAC service is not avail (e.g. failed pac load
      // or shutdown) then we will be going direct. Make that
      // mapping now so that any filters are still applied.
      mPACString = "DIRECT;"_ns;
      mStatus = NS_OK;

      LOG(("pac not available, use DIRECT\n"));
      pacAvailable = false;
    }

    // Generate proxy info from the PAC string if appropriate
    if (NS_SUCCEEDED(mStatus) && !mProxyInfo && !mPACString.IsEmpty()) {
      mPPS->ProcessPACString(mPACString, mResolveFlags,
                             getter_AddRefs(mProxyInfo));
      nsCOMPtr<nsIURI> proxyURI;
      GetProxyURI(mChannel, getter_AddRefs(proxyURI));

      // Now apply proxy filters
      nsProtocolInfo info;
      mStatus = mPPS->GetProtocolInfo(proxyURI, &info);

      auto consumeFiltersResult = [pacAvailable](nsAsyncResolveRequest* self,
                                                 nsIProxyInfo* pi,
                                                 bool async) -> nsresult {
        LOG(("DoCallback::consumeFiltersResult this=%p, pi=%p, async=%d", self,
             pi, async));

        self->mProxyInfo = pi;

        if (pacAvailable) {
          // if !pacAvailable, it was already logged above
          LOG(("pac thread callback %s\n", self->mPACString.get()));
        }

        if (NS_SUCCEEDED(self->mStatus)) {
          self->mPPS->MaybeDisableDNSPrefetch(self->mProxyInfo);
        }

        self->EnsureResolveFlagsMatch();
        self->mCallback->OnProxyAvailable(self, self->mChannel,
                                          self->mProxyInfo, self->mStatus);

        return NS_OK;
      };

      if (NS_SUCCEEDED(mStatus)) {
        mAsyncFilterApplier = new AsyncApplyFilters(info, consumeFiltersResult);
        // This may call consumeFiltersResult() directly.
        mAsyncFilterApplier->AsyncProcess(this);
        return;
      }

      consumeFiltersResult(this, nullptr, false);
    } else if (NS_SUCCEEDED(mStatus) && !mPACURL.IsEmpty()) {
      LOG(("pac thread callback indicates new pac file load\n"));

      nsCOMPtr<nsIURI> proxyURI;
      GetProxyURI(mChannel, getter_AddRefs(proxyURI));

      // trigger load of new pac url
      nsresult rv = mPPS->ConfigureFromPAC(mPACURL, false);
      if (NS_SUCCEEDED(rv)) {
        // now that the load is triggered, we can resubmit the query
        RefPtr<nsAsyncResolveRequest> newRequest =
            new nsAsyncResolveRequest(mPPS, mChannel, mResolveFlags, mCallback);
        rv = mPPS->mPACMan->AsyncGetProxyForURI(proxyURI, newRequest,
                                                mResolveFlags, true);
      }

      if (NS_FAILED(rv))
        mCallback->OnProxyAvailable(this, mChannel, nullptr, rv);

      // do not call onproxyavailable() in SUCCESS case - the newRequest will
      // take care of that
    } else {
      LOG(("pac thread callback did not provide information %" PRIX32 "\n",
           static_cast<uint32_t>(mStatus)));
      if (NS_SUCCEEDED(mStatus)) mPPS->MaybeDisableDNSPrefetch(mProxyInfo);
      EnsureResolveFlagsMatch();
      mCallback->OnProxyAvailable(this, mChannel, mProxyInfo, mStatus);
    }

    // We are on the main thread now and don't need these any more so
    // release them to avoid having to proxy them back to the main thread
    // in the dtor
    mCallback = nullptr;  // in case the callback holds an owning ref to us
    mPPS = nullptr;
    mXPComPPS = nullptr;
    mChannel = nullptr;
    mProxyInfo = nullptr;
  }

 private:
  nsresult mStatus;
  nsCString mPACString;
  nsCString mPACURL;
  bool mDispatched;
  uint32_t mResolveFlags;

  nsProtocolProxyService* mPPS;
  nsCOMPtr<nsIProtocolProxyService> mXPComPPS;
  nsCOMPtr<nsIChannel> mChannel;
  nsCOMPtr<nsIProtocolProxyCallback> mCallback;
  nsCOMPtr<nsIProxyInfo> mProxyInfo;

  RefPtr<AsyncApplyFilters> mAsyncFilterApplier;
};

NS_IMPL_ISUPPORTS(nsAsyncResolveRequest, nsICancelable, nsIRunnable)

NS_IMPL_ISUPPORTS(nsAsyncResolveRequest::AsyncApplyFilters,
                  nsIProxyProtocolFilterResult, nsICancelable, nsIRunnable)

nsAsyncResolveRequest::AsyncApplyFilters::AsyncApplyFilters(
    nsProtocolInfo& aInfo, Callback const& aCallback)
    : mInfo(aInfo),
      mCallback(aCallback),
      mNextFilterIndex(0),
      mProcessingInLoop(false),
      mFilterCalledBack(false) {
  LOG(("AsyncApplyFilters %p", this));
}

nsAsyncResolveRequest::AsyncApplyFilters::~AsyncApplyFilters() {
  LOG(("~AsyncApplyFilters %p", this));

  MOZ_ASSERT(!mRequest);
  MOZ_ASSERT(!mProxyInfo);
  MOZ_ASSERT(!mFiltersCopy.Length());
}

nsresult nsAsyncResolveRequest::AsyncApplyFilters::AsyncProcess(
    nsAsyncResolveRequest* aRequest) {
  LOG(("AsyncApplyFilters::AsyncProcess %p for req %p", this, aRequest));

  MOZ_ASSERT(!mRequest, "AsyncApplyFilters started more than once!");

  if (!(mInfo.flags & nsIProtocolHandler::ALLOWS_PROXY)) {
    // Calling the callback directly (not via Finish()) since we
    // don't want to prune.
    return mCallback(aRequest, aRequest->mProxyInfo, false);
  }

  mProcessingThread = NS_GetCurrentThread();

  mRequest = aRequest;
  mProxyInfo = aRequest->mProxyInfo;

  aRequest->mPPS->CopyFilters(mFiltersCopy);

  // We want to give filters a chance to process in a single loop to prevent
  // any current-thread dispatch delays when those are not needed.
  // This code is rather "loopy" than "recursive" to prevent long stack traces.
  do {
    MOZ_ASSERT(!mProcessingInLoop);

    mozilla::AutoRestore<bool> restore(mProcessingInLoop);
    mProcessingInLoop = true;

    nsresult rv = ProcessNextFilter();
    if (NS_FAILED(rv)) {
      return rv;
    }
  } while (mFilterCalledBack);

  return NS_OK;
}

nsresult nsAsyncResolveRequest::AsyncApplyFilters::ProcessNextFilter() {
  LOG(("AsyncApplyFilters::ProcessNextFilter %p ENTER pi=%p", this,
       mProxyInfo.get()));

  RefPtr<FilterLink> filter;
  do {
    mFilterCalledBack = false;

    if (!mRequest) {
      // We got canceled
      LOG(("  canceled"));
      return NS_OK;  // should we let the consumer know?
    }

    if (mNextFilterIndex == mFiltersCopy.Length()) {
      return Finish();
    }

    filter = mFiltersCopy[mNextFilterIndex++];

    // Loop until a call to a filter succeeded.  Other option is to recurse
    // but that would waste stack trace when a number of filters gets registered
    // and all from some reason tend to fail.
    // The !mFilterCalledBack part of the condition is there to protect us from
    // calling on another filter when the current one managed to call back and
    // then threw. We already have the result so take it and use it since
    // the next filter will be processed by the root loop or a call to
    // ProcessNextFilter has already been dispatched to this thread.
    LOG(("  calling filter %p pi=%p", filter.get(), mProxyInfo.get()));
  } while (!mRequest->mPPS->ApplyFilter(filter, mRequest->mChannel, mInfo,
                                        mProxyInfo, this) &&
           !mFilterCalledBack);

  LOG(("AsyncApplyFilters::ProcessNextFilter %p LEAVE pi=%p", this,
       mProxyInfo.get()));
  return NS_OK;
}

NS_IMETHODIMP
nsAsyncResolveRequest::AsyncApplyFilters::OnProxyFilterResult(
    nsIProxyInfo* aProxyInfo) {
  LOG(("AsyncApplyFilters::OnProxyFilterResult %p pi=%p", this, aProxyInfo));

  MOZ_ASSERT(mProcessingThread && mProcessingThread->IsOnCurrentThread());
  MOZ_ASSERT(!mFilterCalledBack);

  if (mFilterCalledBack) {
    LOG(("  duplicate notification?"));
    return NS_OK;
  }

  mFilterCalledBack = true;

  if (!mRequest) {
    // We got canceled
    LOG(("  canceled"));
    return NS_OK;
  }

  mProxyInfo = aProxyInfo;

  if (mProcessingInLoop) {
    // No need to call/dispatch ProcessNextFilter(), we are in a control
    // loop that will do this for us and save recursion/dispatching.
    LOG(("  in a root loop"));
    return NS_OK;
  }

  if (mNextFilterIndex == mFiltersCopy.Length()) {
    // We are done, all filters have been called on!
    Finish();
    return NS_OK;
  }

  // Redispatch, since we don't want long stacks when filters respond
  // synchronously.
  LOG(("  redispatching"));
  NS_DispatchToCurrentThread(this);
  return NS_OK;
}

NS_IMETHODIMP
nsAsyncResolveRequest::AsyncApplyFilters::Run() {
  LOG(("AsyncApplyFilters::Run %p", this));

  MOZ_ASSERT(mProcessingThread && mProcessingThread->IsOnCurrentThread());

  ProcessNextFilter();
  return NS_OK;
}

nsresult nsAsyncResolveRequest::AsyncApplyFilters::Finish() {
  LOG(("AsyncApplyFilters::Finish %p pi=%p", this, mProxyInfo.get()));

  MOZ_ASSERT(mRequest);

  mFiltersCopy.Clear();

  RefPtr<nsAsyncResolveRequest> request;
  request.swap(mRequest);

  nsCOMPtr<nsIProxyInfo> pi;
  pi.swap(mProxyInfo);

  request->mPPS->PruneProxyInfo(mInfo, pi);
  return mCallback(request, pi, !mProcessingInLoop);
}

NS_IMETHODIMP
nsAsyncResolveRequest::AsyncApplyFilters::Cancel(nsresult reason) {
  LOG(("AsyncApplyFilters::Cancel %p", this));

  MOZ_ASSERT(mProcessingThread && mProcessingThread->IsOnCurrentThread());

  // This will be called only from inside the request, so don't call
  // its's callback.  Dropping the members means we simply break the cycle.
  mFiltersCopy.Clear();
  mProxyInfo = nullptr;
  mRequest = nullptr;

  return NS_OK;
}

// Bug 1366133: make GetPACURI off-main-thread since it may hang on Windows
// platform
class AsyncGetPACURIRequest final : public nsIRunnable {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  using CallbackFunc = nsresult (nsProtocolProxyService::*)(bool, bool,
                                                            nsresult,
                                                            const nsACString&);

  AsyncGetPACURIRequest(nsProtocolProxyService* aService,
                        CallbackFunc aCallback,
                        nsISystemProxySettings* aSystemProxySettings,
                        bool aMainThreadOnly, bool aForceReload,
                        bool aResetPACThread)
      : mIsMainThreadOnly(aMainThreadOnly),
        mService(aService),
        mServiceHolder(do_QueryObject(aService)),
        mCallback(aCallback),
        mSystemProxySettings(aSystemProxySettings),
        mForceReload(aForceReload),
        mResetPACThread(aResetPACThread) {
    MOZ_ASSERT(NS_IsMainThread());
    Unused << mIsMainThreadOnly;
  }

  NS_IMETHOD Run() override {
    MOZ_ASSERT(NS_IsMainThread() == mIsMainThreadOnly);

    nsCString pacUri;
    nsresult rv = mSystemProxySettings->GetPACURI(pacUri);

    nsCOMPtr<nsIRunnable> event =
        NewNonOwningCancelableRunnableMethod<bool, bool, nsresult, nsCString>(
            "AsyncGetPACURIRequestCallback", mService, mCallback, mForceReload,
            mResetPACThread, rv, pacUri);

    return NS_DispatchToMainThread(event);
  }

 private:
  ~AsyncGetPACURIRequest() {
    NS_ReleaseOnMainThread("AsyncGetPACURIRequest::mServiceHolder",
                           mServiceHolder.forget());
  }

  bool mIsMainThreadOnly;

  nsProtocolProxyService* mService;  // ref-count is hold by mServiceHolder
  nsCOMPtr<nsIProtocolProxyService2> mServiceHolder;
  CallbackFunc mCallback;
  nsCOMPtr<nsISystemProxySettings> mSystemProxySettings;

  bool mForceReload;
  bool mResetPACThread;
};

NS_IMPL_ISUPPORTS(AsyncGetPACURIRequest, nsIRunnable)

//----------------------------------------------------------------------------

//
// apply mask to address (zeros out excluded bits).
//
// NOTE: we do the byte swapping here to minimize overall swapping.
//
static void proxy_MaskIPv6Addr(PRIPv6Addr& addr, uint16_t mask_len) {
  if (mask_len == 128) return;

  if (mask_len > 96) {
    addr.pr_s6_addr32[3] =
        PR_htonl(PR_ntohl(addr.pr_s6_addr32[3]) & (~0uL << (128 - mask_len)));
  } else if (mask_len > 64) {
    addr.pr_s6_addr32[3] = 0;
    addr.pr_s6_addr32[2] =
        PR_htonl(PR_ntohl(addr.pr_s6_addr32[2]) & (~0uL << (96 - mask_len)));
  } else if (mask_len > 32) {
    addr.pr_s6_addr32[3] = 0;
    addr.pr_s6_addr32[2] = 0;
    addr.pr_s6_addr32[1] =
        PR_htonl(PR_ntohl(addr.pr_s6_addr32[1]) & (~0uL << (64 - mask_len)));
  } else {
    addr.pr_s6_addr32[3] = 0;
    addr.pr_s6_addr32[2] = 0;
    addr.pr_s6_addr32[1] = 0;
    addr.pr_s6_addr32[0] =
        PR_htonl(PR_ntohl(addr.pr_s6_addr32[0]) & (~0uL << (32 - mask_len)));
  }
}

static void proxy_GetStringPref(nsIPrefBranch* aPrefBranch, const char* aPref,
                                nsCString& aResult) {
  nsAutoCString temp;
  nsresult rv = aPrefBranch->GetCharPref(aPref, temp);
  if (NS_FAILED(rv))
    aResult.Truncate();
  else {
    aResult.Assign(temp);
    // all of our string prefs are hostnames, so we should remove any
    // whitespace characters that the user might have unknowingly entered.
    aResult.StripWhitespace();
  }
}

static void proxy_GetIntPref(nsIPrefBranch* aPrefBranch, const char* aPref,
                             int32_t& aResult) {
  int32_t temp;
  nsresult rv = aPrefBranch->GetIntPref(aPref, &temp);
  if (NS_FAILED(rv))
    aResult = -1;
  else
    aResult = temp;
}

static void proxy_GetBoolPref(nsIPrefBranch* aPrefBranch, const char* aPref,
                              bool& aResult) {
  bool temp;
  nsresult rv = aPrefBranch->GetBoolPref(aPref, &temp);
  if (NS_FAILED(rv))
    aResult = false;
  else
    aResult = temp;
}

//----------------------------------------------------------------------------

static const int32_t PROXYCONFIG_DIRECT4X = 3;
static const int32_t PROXYCONFIG_COUNT = 6;

NS_IMPL_ADDREF(nsProtocolProxyService)
NS_IMPL_RELEASE(nsProtocolProxyService)
NS_IMPL_CLASSINFO(nsProtocolProxyService, nullptr, nsIClassInfo::SINGLETON,
                  NS_PROTOCOLPROXYSERVICE_CID)

// NS_IMPL_QUERY_INTERFACE_CI with the nsProtocolProxyService QI change
NS_INTERFACE_MAP_BEGIN(nsProtocolProxyService)
  NS_INTERFACE_MAP_ENTRY(nsIProtocolProxyService)
  NS_INTERFACE_MAP_ENTRY(nsIProtocolProxyService2)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(nsProtocolProxyService)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIProtocolProxyService)
  NS_IMPL_QUERY_CLASSINFO(nsProtocolProxyService)
NS_INTERFACE_MAP_END

NS_IMPL_CI_INTERFACE_GETTER(nsProtocolProxyService, nsIProtocolProxyService,
                            nsIProtocolProxyService2)

nsProtocolProxyService::nsProtocolProxyService()
    : mFilterLocalHosts(false),
      mProxyConfig(PROXYCONFIG_DIRECT),
      mHTTPProxyPort(-1),
      mHTTPSProxyPort(-1),
      mSOCKSProxyPort(-1),
      mSOCKSProxyVersion(4),
      mSOCKSProxyRemoteDNS(false),
      mProxyOverTLS(true),
      mWPADOverDHCPEnabled(false),
      mPACMan(nullptr),
      mSessionStart(PR_Now()),
      mFailedProxyTimeout(30 * 60)  // 30 minute default
      ,
      mIsShutdown(false) {}

nsProtocolProxyService::~nsProtocolProxyService() {
  // These should have been cleaned up in our Observe method.
  NS_ASSERTION(mHostFiltersArray.Length() == 0 && mFilters.Length() == 0 &&
                   mPACMan == nullptr,
               "what happened to xpcom-shutdown?");
}

// nsProtocolProxyService methods
nsresult nsProtocolProxyService::Init() {
  // failure to access prefs is non-fatal
  nsCOMPtr<nsIPrefBranch> prefBranch = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (prefBranch) {
    // monitor proxy prefs
    prefBranch->AddObserver(PROXY_PREF_BRANCH, this, false);

    // read all prefs
    PrefsChanged(prefBranch, nullptr);
  }

  nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
  if (obs) {
    // register for shutdown notification so we can clean ourselves up
    // properly.
    obs->AddObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID, false);
    obs->AddObserver(this, NS_NETWORK_LINK_TOPIC, false);
  }

  return NS_OK;
}

// ReloadNetworkPAC() checks if there's a non-networked PAC in use then avoids
// to call ReloadPAC()
nsresult nsProtocolProxyService::ReloadNetworkPAC() {
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefs) {
    return NS_OK;
  }

  int32_t type;
  nsresult rv = prefs->GetIntPref(PROXY_PREF("type"), &type);
  if (NS_FAILED(rv)) {
    return NS_OK;
  }

  if (type == PROXYCONFIG_PAC) {
    nsAutoCString pacSpec;
    prefs->GetCharPref(PROXY_PREF("autoconfig_url"), pacSpec);
    if (!pacSpec.IsEmpty()) {
      nsCOMPtr<nsIURI> pacURI;
      rv = NS_NewURI(getter_AddRefs(pacURI), pacSpec);
      if (!NS_SUCCEEDED(rv)) {
        return rv;
      }

      nsProtocolInfo pac;
      rv = GetProtocolInfo(pacURI, &pac);
      if (!NS_SUCCEEDED(rv)) {
        return rv;
      }

      if (!pac.scheme.EqualsLiteral("file") &&
          !pac.scheme.EqualsLiteral("data")) {
        LOG((": received network changed event, reload PAC"));
        ReloadPAC();
      }
    }
  } else if ((type == PROXYCONFIG_WPAD) || (type == PROXYCONFIG_SYSTEM)) {
    ReloadPAC();
  }

  return NS_OK;
}

nsresult nsProtocolProxyService::AsyncConfigureFromPAC(bool aForceReload,
                                                       bool aResetPACThread) {
  MOZ_ASSERT(NS_IsMainThread());

  bool mainThreadOnly;
  nsresult rv = mSystemProxySettings->GetMainThreadOnly(&mainThreadOnly);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  nsCOMPtr<nsIRunnable> req = new AsyncGetPACURIRequest(
      this, &nsProtocolProxyService::OnAsyncGetPACURI, mSystemProxySettings,
      mainThreadOnly, aForceReload, aResetPACThread);

  if (mainThreadOnly) {
    return req->Run();
  }

  return NS_DispatchBackgroundTask(req.forget(),
                                   nsIEventTarget::DISPATCH_NORMAL);
}

nsresult nsProtocolProxyService::OnAsyncGetPACURI(bool aForceReload,
                                                  bool aResetPACThread,
                                                  nsresult aResult,
                                                  const nsACString& aUri) {
  MOZ_ASSERT(NS_IsMainThread());

  if (aResetPACThread) {
    ResetPACThread();
  }

  if (NS_SUCCEEDED(aResult) && !aUri.IsEmpty()) {
    ConfigureFromPAC(PromiseFlatCString(aUri), aForceReload);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::Observe(nsISupports* aSubject, const char* aTopic,
                                const char16_t* aData) {
  if (strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID) == 0) {
    mIsShutdown = true;
    // cleanup
    mHostFiltersArray.Clear();
    mFilters.Clear();

    if (mPACMan) {
      mPACMan->Shutdown();
      mPACMan = nullptr;
    }

    if (mReloadPACTimer) {
      mReloadPACTimer->Cancel();
      mReloadPACTimer = nullptr;
    }

    nsCOMPtr<nsIObserverService> obs = services::GetObserverService();
    if (obs) {
      obs->RemoveObserver(this, NS_NETWORK_LINK_TOPIC);
      obs->RemoveObserver(this, NS_XPCOM_SHUTDOWN_OBSERVER_ID);
    }

  } else if (strcmp(aTopic, NS_NETWORK_LINK_TOPIC) == 0) {
    nsCString converted = NS_ConvertUTF16toUTF8(aData);
    const char* state = converted.get();
    if (!strcmp(state, NS_NETWORK_LINK_DATA_CHANGED)) {
      uint32_t delay = StaticPrefs::network_proxy_reload_pac_delay();
      LOG(("nsProtocolProxyService::Observe call ReloadNetworkPAC() delay=%u",
           delay));

      if (delay) {
        if (mReloadPACTimer) {
          mReloadPACTimer->Cancel();
          mReloadPACTimer = nullptr;
        }
        NS_NewTimerWithCallback(getter_AddRefs(mReloadPACTimer), this, delay,
                                nsITimer::TYPE_ONE_SHOT);
      } else {
        ReloadNetworkPAC();
      }
    }
  } else {
    NS_ASSERTION(strcmp(aTopic, NS_PREFBRANCH_PREFCHANGE_TOPIC_ID) == 0,
                 "what is this random observer event?");
    nsCOMPtr<nsIPrefBranch> prefs = do_QueryInterface(aSubject);
    if (prefs) PrefsChanged(prefs, NS_LossyConvertUTF16toASCII(aData).get());
  }
  return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::Notify(nsITimer* aTimer) {
  MOZ_ASSERT(aTimer == mReloadPACTimer);
  ReloadNetworkPAC();
  return NS_OK;
}

void nsProtocolProxyService::PrefsChanged(nsIPrefBranch* prefBranch,
                                          const char* pref) {
  nsresult rv = NS_OK;
  bool reloadPAC = false;
  nsAutoCString tempString;

  if (!pref || !strcmp(pref, PROXY_PREF("type"))) {
    int32_t type = -1;
    rv = prefBranch->GetIntPref(PROXY_PREF("type"), &type);
    if (NS_SUCCEEDED(rv)) {
      // bug 115720 - for ns4.x backwards compatibility
      if (type == PROXYCONFIG_DIRECT4X) {
        type = PROXYCONFIG_DIRECT;
        // Reset the type so that the dialog looks correct, and we
        // don't have to handle this case everywhere else
        // I'm paranoid about a loop of some sort - only do this
        // if we're enumerating all prefs, and ignore any error
        if (!pref) prefBranch->SetIntPref(PROXY_PREF("type"), type);
      } else if (type >= PROXYCONFIG_COUNT) {
        LOG(("unknown proxy type: %" PRId32 "; assuming direct\n", type));
        type = PROXYCONFIG_DIRECT;
      }
      mProxyConfig = type;
      reloadPAC = true;
    }

    if (mProxyConfig == PROXYCONFIG_SYSTEM) {
      mSystemProxySettings = do_GetService(NS_SYSTEMPROXYSETTINGS_CONTRACTID);
      if (!mSystemProxySettings) mProxyConfig = PROXYCONFIG_DIRECT;
      ResetPACThread();
    } else {
      if (mSystemProxySettings) {
        mSystemProxySettings = nullptr;
        ResetPACThread();
      }
    }
  }

  if (!pref || !strcmp(pref, PROXY_PREF("http")))
    proxy_GetStringPref(prefBranch, PROXY_PREF("http"), mHTTPProxyHost);

  if (!pref || !strcmp(pref, PROXY_PREF("http_port")))
    proxy_GetIntPref(prefBranch, PROXY_PREF("http_port"), mHTTPProxyPort);

  if (!pref || !strcmp(pref, PROXY_PREF("ssl")))
    proxy_GetStringPref(prefBranch, PROXY_PREF("ssl"), mHTTPSProxyHost);

  if (!pref || !strcmp(pref, PROXY_PREF("ssl_port")))
    proxy_GetIntPref(prefBranch, PROXY_PREF("ssl_port"), mHTTPSProxyPort);

  if (!pref || !strcmp(pref, PROXY_PREF("socks")))
    proxy_GetStringPref(prefBranch, PROXY_PREF("socks"), mSOCKSProxyTarget);

  if (!pref || !strcmp(pref, PROXY_PREF("socks_port")))
    proxy_GetIntPref(prefBranch, PROXY_PREF("socks_port"), mSOCKSProxyPort);

  if (!pref || !strcmp(pref, PROXY_PREF("socks_version"))) {
    int32_t version;
    proxy_GetIntPref(prefBranch, PROXY_PREF("socks_version"), version);
    // make sure this preference value remains sane
    if (version == 5)
      mSOCKSProxyVersion = 5;
    else
      mSOCKSProxyVersion = 4;
  }

  if (!pref || !strcmp(pref, PROXY_PREF("socks_remote_dns")))
    proxy_GetBoolPref(prefBranch, PROXY_PREF("socks_remote_dns"),
                      mSOCKSProxyRemoteDNS);

  if (!pref || !strcmp(pref, PROXY_PREF("proxy_over_tls"))) {
    proxy_GetBoolPref(prefBranch, PROXY_PREF("proxy_over_tls"), mProxyOverTLS);
  }

  if (!pref || !strcmp(pref, PROXY_PREF("enable_wpad_over_dhcp"))) {
    proxy_GetBoolPref(prefBranch, PROXY_PREF("enable_wpad_over_dhcp"),
                      mWPADOverDHCPEnabled);
    reloadPAC = reloadPAC || mProxyConfig == PROXYCONFIG_WPAD;
  }

  if (!pref || !strcmp(pref, PROXY_PREF("failover_timeout")))
    proxy_GetIntPref(prefBranch, PROXY_PREF("failover_timeout"),
                     mFailedProxyTimeout);

  if (!pref || !strcmp(pref, PROXY_PREF("no_proxies_on"))) {
    rv = prefBranch->GetCharPref(PROXY_PREF("no_proxies_on"), tempString);
    if (NS_SUCCEEDED(rv)) LoadHostFilters(tempString);
  }

  // We're done if not using something that could give us a PAC URL
  // (PAC, WPAD or System)
  if (mProxyConfig != PROXYCONFIG_PAC && mProxyConfig != PROXYCONFIG_WPAD &&
      mProxyConfig != PROXYCONFIG_SYSTEM)
    return;

  // OK, we need to reload the PAC file if:
  //  1) network.proxy.type changed, or
  //  2) network.proxy.autoconfig_url changed and PAC is configured

  if (!pref || !strcmp(pref, PROXY_PREF("autoconfig_url"))) reloadPAC = true;

  if (reloadPAC) {
    tempString.Truncate();
    if (mProxyConfig == PROXYCONFIG_PAC) {
      prefBranch->GetCharPref(PROXY_PREF("autoconfig_url"), tempString);
      if (mPACMan && !mPACMan->IsPACURI(tempString)) {
        LOG(("PAC Thread URI Changed - Reset Pac Thread"));
        ResetPACThread();
      }
    } else if (mProxyConfig == PROXYCONFIG_WPAD) {
      LOG(("Auto-detecting proxy - Reset Pac Thread"));
      ResetPACThread();
    } else if (mSystemProxySettings) {
      // Get System Proxy settings if available
      AsyncConfigureFromPAC(false, false);
    }
    if (!tempString.IsEmpty() || mProxyConfig == PROXYCONFIG_WPAD) {
      ConfigureFromPAC(tempString, false);
    }
  }
}

bool nsProtocolProxyService::CanUseProxy(nsIURI* aURI, int32_t defaultPort) {
  int32_t port;
  nsAutoCString host;

  nsresult rv = aURI->GetAsciiHost(host);
  if (NS_FAILED(rv) || host.IsEmpty()) return false;

  rv = aURI->GetPort(&port);
  if (NS_FAILED(rv)) return false;
  if (port == -1) port = defaultPort;

  PRNetAddr addr;
  bool is_ipaddr = (PR_StringToNetAddr(host.get(), &addr) == PR_SUCCESS);

  PRIPv6Addr ipv6;
  if (is_ipaddr) {
    // convert parsed address to IPv6
    if (addr.raw.family == PR_AF_INET) {
      // convert to IPv4-mapped address
      PR_ConvertIPv4AddrToIPv6(addr.inet.ip, &ipv6);
    } else if (addr.raw.family == PR_AF_INET6) {
      // copy the address
      memcpy(&ipv6, &addr.ipv6.ip, sizeof(PRIPv6Addr));
    } else {
      NS_WARNING("unknown address family");
      return true;  // allow proxying
    }
  }

  // Don't use proxy for local hosts (plain hostname, no dots)
  if ((!is_ipaddr && mFilterLocalHosts && !host.Contains('.')) ||
      // This method detects if we have network.proxy.allow_hijacking_localhost
      // pref enabled. If it's true then this method will always return false
      // otherwise it returns true if the host matches an address that's
      // hardcoded to the loopback address.
      (!StaticPrefs::network_proxy_allow_hijacking_localhost() &&
       nsMixedContentBlocker::IsPotentiallyTrustworthyLoopbackHost(host))) {
    LOG(("Not using proxy for this local host [%s]!\n", host.get()));
    return false;  // don't allow proxying
  }

  int32_t index = -1;
  while (++index < int32_t(mHostFiltersArray.Length())) {
    const auto& hinfo = mHostFiltersArray[index];

    if (is_ipaddr != hinfo->is_ipaddr) continue;
    if (hinfo->port && hinfo->port != port) continue;

    if (is_ipaddr) {
      // generate masked version of target IPv6 address
      PRIPv6Addr masked;
      memcpy(&masked, &ipv6, sizeof(PRIPv6Addr));
      proxy_MaskIPv6Addr(masked, hinfo->ip.mask_len);

      // check for a match
      if (memcmp(&masked, &hinfo->ip.addr, sizeof(PRIPv6Addr)) == 0)
        return false;  // proxy disallowed
    } else {
      uint32_t host_len = host.Length();
      uint32_t filter_host_len = hinfo->name.host_len;

      if (host_len >= filter_host_len) {
        //
        // compare last |filter_host_len| bytes of target hostname.
        //
        const char* host_tail = host.get() + host_len - filter_host_len;
        if (!PL_strncasecmp(host_tail, hinfo->name.host, filter_host_len)) {
          // If the tail of the host string matches the filter

          if (filter_host_len > 0 && hinfo->name.host[0] == '.') {
            // If the filter was of the form .foo.bar.tld, all such
            // matches are correct
            return false;  // proxy disallowed
          }

          // abc-def.example.org should not match def.example.org
          // however, *.def.example.org should match .def.example.org
          // We check that the filter doesn't start with a `.`. If it does,
          // then the strncasecmp above should suffice. If it doesn't,
          // then we should only consider it a match if the strncasecmp happened
          // at a subdomain boundary
          if (host_len > filter_host_len && *(host_tail - 1) == '.') {
            // If the host was something.foo.bar.tld and the filter
            // was foo.bar.tld, it's still a match.
            // the character right before the tail must be a
            // `.` for this to work
            return false;  // proxy disallowed
          }

          if (host_len == filter_host_len) {
            // If the host and filter are of the same length,
            // they should match
            return false;  // proxy disallowed
          }
        }
      }
    }
  }
  return true;
}

// kProxyType\* may be referred to externally in
// nsProxyInfo in order to compare by string pointer
const char kProxyType_HTTP[] = "http";
const char kProxyType_HTTPS[] = "https";
const char kProxyType_PROXY[] = "proxy";
const char kProxyType_SOCKS[] = "socks";
const char kProxyType_SOCKS4[] = "socks4";
const char kProxyType_SOCKS5[] = "socks5";
const char kProxyType_DIRECT[] = "direct";

const char* nsProtocolProxyService::ExtractProxyInfo(const char* start,
                                                     uint32_t aResolveFlags,
                                                     nsProxyInfo** result) {
  *result = nullptr;
  uint32_t flags = 0;

  // see BNF in ProxyAutoConfig.h and notes in nsISystemProxySettings.idl

  // find end of proxy info delimiter
  const char* end = start;
  while (*end && *end != ';') ++end;

  // find end of proxy type delimiter
  const char* sp = start;
  while (sp < end && *sp != ' ' && *sp != '\t') ++sp;

  uint32_t len = sp - start;
  const char* type = nullptr;
  switch (len) {
    case 4:
      if (PL_strncasecmp(start, kProxyType_HTTP, 4) == 0) {
        type = kProxyType_HTTP;
      }
      break;
    case 5:
      if (PL_strncasecmp(start, kProxyType_PROXY, 5) == 0) {
        type = kProxyType_HTTP;
      } else if (PL_strncasecmp(start, kProxyType_SOCKS, 5) == 0) {
        type = kProxyType_SOCKS4;  // assume v4 for 4x compat
      } else if (PL_strncasecmp(start, kProxyType_HTTPS, 5) == 0) {
        type = kProxyType_HTTPS;
      }
      break;
    case 6:
      if (PL_strncasecmp(start, kProxyType_DIRECT, 6) == 0)
        type = kProxyType_DIRECT;
      else if (PL_strncasecmp(start, kProxyType_SOCKS4, 6) == 0)
        type = kProxyType_SOCKS4;
      else if (PL_strncasecmp(start, kProxyType_SOCKS5, 6) == 0)
        // map "SOCKS5" to "socks" to match contract-id of registered
        // SOCKS-v5 socket provider.
        type = kProxyType_SOCKS;
      break;
  }
  if (type) {
    int32_t port = -1;

    // If it's a SOCKS5 proxy, do name resolution on the server side.
    // We could use this with SOCKS4a servers too, but they might not
    // support it.
    if (type == kProxyType_SOCKS || mSOCKSProxyRemoteDNS)
      flags |= nsIProxyInfo::TRANSPARENT_PROXY_RESOLVES_HOST;

    // extract host:port
    start = sp;
    while ((*start == ' ' || *start == '\t') && start < end) start++;

    // port defaults
    if (type == kProxyType_HTTP) {
      port = 80;
    } else if (type == kProxyType_HTTPS) {
      port = 443;
    } else {
      port = 1080;
    }

    RefPtr<nsProxyInfo> pi = new nsProxyInfo();
    pi->mType = type;
    pi->mFlags = flags;
    pi->mResolveFlags = aResolveFlags;
    pi->mTimeout = mFailedProxyTimeout;

    // www.foo.com:8080 and http://www.foo.com:8080
    nsDependentCSubstring maybeURL(start, end - start);
    nsCOMPtr<nsIURI> pacURI;

    nsAutoCString urlHost;
    // First assume the scheme is present, e.g. http://www.example.com:8080
    if (NS_FAILED(NS_NewURI(getter_AddRefs(pacURI), maybeURL)) ||
        NS_FAILED(pacURI->GetAsciiHost(urlHost)) || urlHost.IsEmpty()) {
      // It isn't, assume www.example.com:8080
      maybeURL.Insert("http://", 0);

      if (NS_SUCCEEDED(NS_NewURI(getter_AddRefs(pacURI), maybeURL))) {
        pacURI->GetAsciiHost(urlHost);
      }
    }

    if (!urlHost.IsEmpty()) {
      pi->mHost = urlHost;

      int32_t tPort;
      if (NS_SUCCEEDED(pacURI->GetPort(&tPort)) && tPort != -1) {
        port = tPort;
      }
      pi->mPort = port;
    }

    pi.forget(result);
  }

  while (*end == ';' || *end == ' ' || *end == '\t') ++end;
  return end;
}

void nsProtocolProxyService::GetProxyKey(nsProxyInfo* pi, nsCString& key) {
  key.AssignASCII(pi->mType);
  if (!pi->mHost.IsEmpty()) {
    key.Append(' ');
    key.Append(pi->mHost);
    key.Append(':');
    key.AppendInt(pi->mPort);
  }
}

uint32_t nsProtocolProxyService::SecondsSinceSessionStart() {
  PRTime now = PR_Now();

  // get time elapsed since session start
  int64_t diff = now - mSessionStart;

  // convert microseconds to seconds
  diff /= PR_USEC_PER_SEC;

  // return converted 32 bit value
  return uint32_t(diff);
}

void nsProtocolProxyService::EnableProxy(nsProxyInfo* pi) {
  nsAutoCString key;
  GetProxyKey(pi, key);
  mFailedProxies.Remove(key);
}

void nsProtocolProxyService::DisableProxy(nsProxyInfo* pi) {
  nsAutoCString key;
  GetProxyKey(pi, key);

  uint32_t dsec = SecondsSinceSessionStart();

  // Add timeout to interval (this is the time when the proxy can
  // be tried again).
  dsec += pi->mTimeout;

  // NOTE: The classic codebase would increase the timeout value
  //       incrementally each time a subsequent failure occurred.
  //       We could do the same, but it would require that we not
  //       remove proxy entries in IsProxyDisabled or otherwise
  //       change the way we are recording disabled proxies.
  //       Simpler is probably better for now, and at least the
  //       user can tune the timeout setting via preferences.

  LOG(("DisableProxy %s %d\n", key.get(), dsec));

  // If this fails, oh well... means we don't have enough memory
  // to remember the failed proxy.
  mFailedProxies.InsertOrUpdate(key, dsec);
}

bool nsProtocolProxyService::IsProxyDisabled(nsProxyInfo* pi) {
  nsAutoCString key;
  GetProxyKey(pi, key);

  uint32_t val;
  if (!mFailedProxies.Get(key, &val)) return false;

  uint32_t dsec = SecondsSinceSessionStart();

  // if time passed has exceeded interval, then try proxy again.
  if (dsec > val) {
    mFailedProxies.Remove(key);
    return false;
  }

  return true;
}

nsresult nsProtocolProxyService::SetupPACThread(
    nsISerialEventTarget* mainThreadEventTarget) {
  if (mIsShutdown) {
    return NS_ERROR_FAILURE;
  }

  if (mPACMan) return NS_OK;

  mPACMan = new nsPACMan(mainThreadEventTarget);

  bool mainThreadOnly;
  nsresult rv;
  if (mSystemProxySettings &&
      NS_SUCCEEDED(mSystemProxySettings->GetMainThreadOnly(&mainThreadOnly)) &&
      !mainThreadOnly) {
    rv = mPACMan->Init(mSystemProxySettings);
  } else {
    rv = mPACMan->Init(nullptr);
  }
  if (NS_FAILED(rv)) {
    mPACMan->Shutdown();
    mPACMan = nullptr;
  }
  return rv;
}

nsresult nsProtocolProxyService::ResetPACThread() {
  if (!mPACMan) return NS_OK;

  mPACMan->Shutdown();
  mPACMan = nullptr;
  return SetupPACThread();
}

nsresult nsProtocolProxyService::ConfigureFromPAC(const nsCString& spec,
                                                  bool forceReload) {
  nsresult rv = SetupPACThread();
  NS_ENSURE_SUCCESS(rv, rv);

  bool autodetect = spec.IsEmpty();
  if (!forceReload && ((!autodetect && mPACMan->IsPACURI(spec)) ||
                       (autodetect && mPACMan->IsUsingWPAD()))) {
    return NS_OK;
  }

  mFailedProxies.Clear();

  mPACMan->SetWPADOverDHCPEnabled(mWPADOverDHCPEnabled);
  return mPACMan->LoadPACFromURI(spec);
}

void nsProtocolProxyService::ProcessPACString(const nsCString& pacString,
                                              uint32_t aResolveFlags,
                                              nsIProxyInfo** result) {
  if (pacString.IsEmpty()) {
    *result = nullptr;
    return;
  }

  const char* proxies = pacString.get();

  nsProxyInfo *pi = nullptr, *first = nullptr, *last = nullptr;
  while (*proxies) {
    proxies = ExtractProxyInfo(proxies, aResolveFlags, &pi);
    if (pi && (pi->mType == kProxyType_HTTPS) && !mProxyOverTLS) {
      delete pi;
      pi = nullptr;
    }

    if (pi) {
      if (last) {
        NS_ASSERTION(last->mNext == nullptr, "leaking nsProxyInfo");
        last->mNext = pi;
      } else
        first = pi;
      last = pi;
    }
  }
  *result = first;
}

// nsIProtocolProxyService2
NS_IMETHODIMP
nsProtocolProxyService::ReloadPAC() {
  nsCOMPtr<nsIPrefBranch> prefs = do_GetService(NS_PREFSERVICE_CONTRACTID);
  if (!prefs) return NS_OK;

  int32_t type;
  nsresult rv = prefs->GetIntPref(PROXY_PREF("type"), &type);
  if (NS_FAILED(rv)) return NS_OK;

  nsAutoCString pacSpec;
  if (type == PROXYCONFIG_PAC)
    prefs->GetCharPref(PROXY_PREF("autoconfig_url"), pacSpec);
  else if (type == PROXYCONFIG_SYSTEM) {
    if (mSystemProxySettings) {
      AsyncConfigureFromPAC(true, true);
    } else {
      ResetPACThread();
    }
  }

  if (!pacSpec.IsEmpty() || type == PROXYCONFIG_WPAD)
    ConfigureFromPAC(pacSpec, true);
  return NS_OK;
}

// When sync interface is removed this can go away too
// The nsPACManCallback portion of this implementation should be run
// off the main thread, because it uses a condvar for signaling and
// the main thread is blocking on that condvar -
//  so call nsPACMan::AsyncGetProxyForURI() with
// a false mainThreadResponse parameter.
class nsAsyncBridgeRequest final : public nsPACManCallback {
  NS_DECL_THREADSAFE_ISUPPORTS

  nsAsyncBridgeRequest()
      : mMutex("nsDeprecatedCallback"),
        mCondVar(mMutex, "nsDeprecatedCallback"),
        mStatus(NS_OK),
        mCompleted(false) {}

  void OnQueryComplete(nsresult status, const nsACString& pacString,
                       const nsACString& newPACURL) override {
    MutexAutoLock lock(mMutex);
    mCompleted = true;
    mStatus = status;
    mPACString = pacString;
    mPACURL = newPACURL;
    mCondVar.Notify();
  }

  void Lock() { mMutex.Lock(); }
  void Unlock() { mMutex.Unlock(); }
  void Wait() { mCondVar.Wait(TimeDuration::FromSeconds(3)); }

 private:
  ~nsAsyncBridgeRequest() = default;

  friend class nsProtocolProxyService;

  Mutex mMutex;
  CondVar mCondVar;

  nsresult mStatus;
  nsCString mPACString;
  nsCString mPACURL;
  bool mCompleted;
};
NS_IMPL_ISUPPORTS0(nsAsyncBridgeRequest)

nsresult nsProtocolProxyService::AsyncResolveInternal(
    nsIChannel* channel, uint32_t flags, nsIProtocolProxyCallback* callback,
    nsICancelable** result, bool isSyncOK,
    nsISerialEventTarget* mainThreadEventTarget) {
  NS_ENSURE_ARG_POINTER(channel);
  NS_ENSURE_ARG_POINTER(callback);

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetProxyURI(channel, getter_AddRefs(uri));
  if (NS_FAILED(rv)) return rv;

  *result = nullptr;
  RefPtr<nsAsyncResolveRequest> ctx =
      new nsAsyncResolveRequest(this, channel, flags, callback);

  nsProtocolInfo info;
  rv = GetProtocolInfo(uri, &info);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIProxyInfo> pi;
  bool usePACThread;

  // adapt to realtime changes in the system proxy service
  if (mProxyConfig == PROXYCONFIG_SYSTEM) {
    nsCOMPtr<nsISystemProxySettings> sp2 =
        do_GetService(NS_SYSTEMPROXYSETTINGS_CONTRACTID);
    if (sp2 != mSystemProxySettings) {
      mSystemProxySettings = sp2;
      ResetPACThread();
    }
  }

  rv = SetupPACThread(mainThreadEventTarget);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // SystemProxySettings and PAC files can block the main thread
  // but if neither of them are in use, we can just do the work
  // right here and directly invoke the callback

  rv =
      Resolve_Internal(channel, info, flags, &usePACThread, getter_AddRefs(pi));
  if (NS_FAILED(rv)) return rv;

  if (!usePACThread || !mPACMan) {
    // we can do it locally
    rv = ctx->ProcessLocally(info, pi, isSyncOK);
    if (NS_SUCCEEDED(rv) && !isSyncOK) {
      ctx.forget(result);
    }
    return rv;
  }

  // else kick off a PAC thread query
  rv = mPACMan->AsyncGetProxyForURI(uri, ctx, flags, true);
  if (NS_SUCCEEDED(rv)) ctx.forget(result);
  return rv;
}

// nsIProtocolProxyService
NS_IMETHODIMP
nsProtocolProxyService::AsyncResolve2(
    nsIChannel* channel, uint32_t flags, nsIProtocolProxyCallback* callback,
    nsISerialEventTarget* mainThreadEventTarget, nsICancelable** result) {
  return AsyncResolveInternal(channel, flags, callback, result, true,
                              mainThreadEventTarget);
}

NS_IMETHODIMP
nsProtocolProxyService::AsyncResolve(
    nsISupports* channelOrURI, uint32_t flags,
    nsIProtocolProxyCallback* callback,
    nsISerialEventTarget* mainThreadEventTarget, nsICancelable** result) {
  nsresult rv;
  // Check if we got a channel:
  nsCOMPtr<nsIChannel> channel = do_QueryInterface(channelOrURI);
  if (!channel) {
    nsCOMPtr<nsIURI> uri = do_QueryInterface(channelOrURI);
    if (!uri) {
      return NS_ERROR_NO_INTERFACE;
    }

    // creating a temporary channel from the URI which is not
    // used to perform any network loads, hence its safe to
    // use systemPrincipal as the loadingPrincipal.
    rv = NS_NewChannel(getter_AddRefs(channel), uri,
                       nsContentUtils::GetSystemPrincipal(),
                       nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
                       nsIContentPolicy::TYPE_OTHER);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return AsyncResolveInternal(channel, flags, callback, result, false,
                              mainThreadEventTarget);
}

NS_IMETHODIMP
nsProtocolProxyService::NewProxyInfo(
    const nsACString& aType, const nsACString& aHost, int32_t aPort,
    const nsACString& aProxyAuthorizationHeader,
    const nsACString& aConnectionIsolationKey, uint32_t aFlags,
    uint32_t aFailoverTimeout, nsIProxyInfo* aFailoverProxy,
    nsIProxyInfo** aResult) {
  return NewProxyInfoWithAuth(aType, aHost, aPort, ""_ns, ""_ns,
                              aProxyAuthorizationHeader,
                              aConnectionIsolationKey, aFlags, aFailoverTimeout,
                              aFailoverProxy, aResult);
}

NS_IMETHODIMP
nsProtocolProxyService::NewProxyInfoWithAuth(
    const nsACString& aType, const nsACString& aHost, int32_t aPort,
    const nsACString& aUsername, const nsACString& aPassword,
    const nsACString& aProxyAuthorizationHeader,
    const nsACString& aConnectionIsolationKey, uint32_t aFlags,
    uint32_t aFailoverTimeout, nsIProxyInfo* aFailoverProxy,
    nsIProxyInfo** aResult) {
  static const char* types[] = {kProxyType_HTTP, kProxyType_HTTPS,
                                kProxyType_SOCKS, kProxyType_SOCKS4,
                                kProxyType_DIRECT};

  // resolve type; this allows us to avoid copying the type string into each
  // proxy info instance.  we just reference the string literals directly :)
  const char* type = nullptr;
  for (auto& t : types) {
    if (aType.LowerCaseEqualsASCII(t)) {
      type = t;
      break;
    }
  }
  NS_ENSURE_TRUE(type, NS_ERROR_INVALID_ARG);

  // We have only implemented username/password for SOCKS proxies.
  if ((!aUsername.IsEmpty() || !aPassword.IsEmpty()) &&
      !aType.LowerCaseEqualsASCII(kProxyType_SOCKS) &&
      !aType.LowerCaseEqualsASCII(kProxyType_SOCKS4)) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  return NewProxyInfo_Internal(type, aHost, aPort, aUsername, aPassword,
                               aProxyAuthorizationHeader,
                               aConnectionIsolationKey, aFlags,
                               aFailoverTimeout, aFailoverProxy, 0, aResult);
}

NS_IMETHODIMP
nsProtocolProxyService::GetFailoverForProxy(nsIProxyInfo* aProxy, nsIURI* aURI,
                                            nsresult aStatus,
                                            nsIProxyInfo** aResult) {
  // We only support failover when a PAC file is configured, either
  // directly or via system settings
  if (mProxyConfig != PROXYCONFIG_PAC && mProxyConfig != PROXYCONFIG_WPAD &&
      mProxyConfig != PROXYCONFIG_SYSTEM)
    return NS_ERROR_NOT_AVAILABLE;

  // Verify that |aProxy| is one of our nsProxyInfo objects.
  nsCOMPtr<nsProxyInfo> pi = do_QueryInterface(aProxy);
  NS_ENSURE_ARG(pi);
  // OK, the QI checked out.  We can proceed.

  // Remember that this proxy is down.
  DisableProxy(pi);

  // NOTE: At this point, we might want to prompt the user if we have
  //       not already tried going DIRECT.  This is something that the
  //       classic codebase supported; however, IE6 does not prompt.

  if (!pi->mNext) return NS_ERROR_NOT_AVAILABLE;

  LOG(("PAC failover from %s %s:%d to %s %s:%d\n", pi->mType, pi->mHost.get(),
       pi->mPort, pi->mNext->mType, pi->mNext->mHost.get(), pi->mNext->mPort));

  *aResult = do_AddRef(pi->mNext).take();
  return NS_OK;
}

namespace {  // anon

class ProxyFilterPositionComparator {
  typedef RefPtr<nsProtocolProxyService::FilterLink> FilterLinkRef;

 public:
  bool Equals(const FilterLinkRef& a, const FilterLinkRef& b) const {
    return a->position == b->position;
  }
  bool LessThan(const FilterLinkRef& a, const FilterLinkRef& b) const {
    return a->position < b->position;
  }
};

class ProxyFilterObjectComparator {
  typedef RefPtr<nsProtocolProxyService::FilterLink> FilterLinkRef;

 public:
  bool Equals(const FilterLinkRef& link, const nsISupports* obj) const {
    return obj == nsCOMPtr<nsISupports>(do_QueryInterface(link->filter)) ||
           obj == nsCOMPtr<nsISupports>(do_QueryInterface(link->channelFilter));
  }
};

}  // namespace

nsresult nsProtocolProxyService::InsertFilterLink(RefPtr<FilterLink>&& link) {
  LOG(("nsProtocolProxyService::InsertFilterLink filter=%p", link.get()));

  if (mIsShutdown) {
    return NS_ERROR_FAILURE;
  }

  mFilters.AppendElement(link);
  mFilters.Sort(ProxyFilterPositionComparator());
  return NS_OK;
}

NS_IMETHODIMP
nsProtocolProxyService::RegisterFilter(nsIProtocolProxyFilter* filter,
                                       uint32_t position) {
  UnregisterFilter(filter);  // remove this filter if we already have it

  RefPtr<FilterLink> link = new FilterLink(position, filter);
  return InsertFilterLink(std::move(link));
}

NS_IMETHODIMP
nsProtocolProxyService::RegisterChannelFilter(
    nsIProtocolProxyChannelFilter* channelFilter, uint32_t position) {
  UnregisterChannelFilter(
      channelFilter);  // remove this filter if we already have it

  RefPtr<FilterLink> link = new FilterLink(position, channelFilter);
  return InsertFilterLink(std::move(link));
}

nsresult nsProtocolProxyService::RemoveFilterLink(nsISupports* givenObject) {
  LOG(("nsProtocolProxyService::RemoveFilterLink target=%p", givenObject));

  return mFilters.RemoveElement(givenObject, ProxyFilterObjectComparator())
             ? NS_OK
             : NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP
nsProtocolProxyService::UnregisterFilter(nsIProtocolProxyFilter* filter) {
  // QI to nsISupports so we can safely test object identity.
  nsCOMPtr<nsISupports> givenObject = do_QueryInterface(filter);
  return RemoveFilterLink(givenObject);
}

NS_IMETHODIMP
nsProtocolProxyService::UnregisterChannelFilter(
    nsIProtocolProxyChannelFilter* channelFilter) {
  // QI to nsISupports so we can safely test object identity.
  nsCOMPtr<nsISupports> givenObject = do_QueryInterface(channelFilter);
  return RemoveFilterLink(givenObject);
}

NS_IMETHODIMP
nsProtocolProxyService::GetProxyConfigType(uint32_t* aProxyConfigType) {
  *aProxyConfigType = mProxyConfig;
  return NS_OK;
}

void nsProtocolProxyService::LoadHostFilters(const nsACString& aFilters) {
  if (mIsShutdown) {
    return;
  }

  // check to see the owners flag? /!?/ TODO
  if (mHostFiltersArray.Length() > 0) {
    mHostFiltersArray.Clear();
  }

  // Reset mFilterLocalHosts - will be set to true if "<local>" is in pref
  // string
  mFilterLocalHosts = false;

  if (aFilters.IsEmpty()) {
    return;
  }

  //
  // filter  = ( host | domain | ipaddr ["/" mask] ) [":" port]
  // filters = filter *( "," LWS filter)
  //
  mozilla::Tokenizer t(aFilters);
  mozilla::Tokenizer::Token token;
  bool eof = false;
  // while (*filters) {
  while (!eof) {
    // skip over spaces and ,
    t.SkipWhites();
    while (t.CheckChar(',')) {
      t.SkipWhites();
    }

    nsAutoCString portStr;
    nsAutoCString hostStr;
    nsAutoCString maskStr;
    t.Record();

    bool parsingIPv6 = false;
    bool parsingPort = false;
    bool parsingMask = false;
    while (t.Next(token)) {
      if (token.Equals(mozilla::Tokenizer::Token::EndOfFile())) {
        eof = true;
        break;
      }
      if (token.Equals(mozilla::Tokenizer::Token::Char(',')) ||
          token.Type() == mozilla::Tokenizer::TOKEN_WS) {
        break;
      }

      if (token.Equals(mozilla::Tokenizer::Token::Char('['))) {
        parsingIPv6 = true;
        continue;
      }

      if (!parsingIPv6 && token.Equals(mozilla::Tokenizer::Token::Char(':'))) {
        // Port is starting. Claim the previous as host.
        if (parsingMask) {
          t.Claim(maskStr);
        } else {
          t.Claim(hostStr);
        }
        t.Record();
        parsingPort = true;
        continue;
      }

      if (token.Equals(mozilla::Tokenizer::Token::Char('/'))) {
        t.Claim(hostStr);
        t.Record();
        parsingMask = true;
        continue;
      }

      if (token.Equals(mozilla::Tokenizer::Token::Char(']'))) {
        parsingIPv6 = false;
        continue;
      }
    }
    if (!parsingPort && !parsingMask) {
      t.Claim(hostStr);
    } else if (parsingPort) {
      t.Claim(portStr);
    } else if (parsingMask) {
      t.Claim(maskStr);
    } else {
      NS_WARNING("Could not parse this rule");
      continue;
    }

    if (hostStr.IsEmpty()) {
      continue;
    }

    // If the current host filter is "<local>", then all local (i.e.
    // no dots in the hostname) hosts should bypass the proxy
    if (hostStr.EqualsIgnoreCase("<local>")) {
      mFilterLocalHosts = true;
      LOG(
          ("loaded filter for local hosts "
           "(plain host names, no dots)\n"));
      // Continue to next host filter;
      continue;
    }

    // For all other host filters, create HostInfo object and add to list
    HostInfo* hinfo = new HostInfo();
    nsresult rv = NS_OK;

    int32_t port = portStr.ToInteger(&rv);
    if (NS_FAILED(rv)) {
      port = 0;
    }
    hinfo->port = port;

    int32_t maskLen = maskStr.ToInteger(&rv);
    if (NS_FAILED(rv)) {
      maskLen = 128;
    }

    // PR_StringToNetAddr can't parse brackets enclosed IPv6
    nsAutoCString addrString = hostStr;
    if (hostStr.First() == '[' && hostStr.Last() == ']') {
      addrString = Substring(hostStr, 1, hostStr.Length() - 2);
    }

    PRNetAddr addr;
    if (PR_StringToNetAddr(addrString.get(), &addr) == PR_SUCCESS) {
      hinfo->is_ipaddr = true;
      hinfo->ip.family = PR_AF_INET6;  // we always store address as IPv6
      hinfo->ip.mask_len = maskLen;

      if (hinfo->ip.mask_len == 0) {
        NS_WARNING("invalid mask");
        goto loser;
      }

      if (addr.raw.family == PR_AF_INET) {
        // convert to IPv4-mapped address
        PR_ConvertIPv4AddrToIPv6(addr.inet.ip, &hinfo->ip.addr);
        // adjust mask_len accordingly
        if (hinfo->ip.mask_len <= 32) hinfo->ip.mask_len += 96;
      } else if (addr.raw.family == PR_AF_INET6) {
        // copy the address
        memcpy(&hinfo->ip.addr, &addr.ipv6.ip, sizeof(PRIPv6Addr));
      } else {
        NS_WARNING("unknown address family");
        goto loser;
      }

      // apply mask to IPv6 address
      proxy_MaskIPv6Addr(hinfo->ip.addr, hinfo->ip.mask_len);
    } else {
      nsAutoCString host;
      if (hostStr.First() == '*') {
        host = Substring(hostStr, 1);
      } else {
        host = hostStr;
      }

      if (host.IsEmpty()) {
        hinfo->name.host = nullptr;
        goto loser;
      }

      hinfo->name.host_len = host.Length();

      hinfo->is_ipaddr = false;
      hinfo->name.host = ToNewCString(host, mozilla::fallible);

      if (!hinfo->name.host) goto loser;
    }

//#define DEBUG_DUMP_FILTERS
#ifdef DEBUG_DUMP_FILTERS
    printf("loaded filter[%zu]:\n", mHostFiltersArray.Length());
    printf("  is_ipaddr = %u\n", hinfo->is_ipaddr);
    printf("  port = %u\n", hinfo->port);
    printf("  host = %s\n", hostStr.get());
    if (hinfo->is_ipaddr) {
      printf("  ip.family = %x\n", hinfo->ip.family);
      printf("  ip.mask_len = %u\n", hinfo->ip.mask_len);

      PRNetAddr netAddr;
      PR_SetNetAddr(PR_IpAddrNull, PR_AF_INET6, 0, &netAddr);
      memcpy(&netAddr.ipv6.ip, &hinfo->ip.addr, sizeof(hinfo->ip.addr));

      char buf[256];
      PR_NetAddrToString(&netAddr, buf, sizeof(buf));

      printf("  ip.addr = %s\n", buf);
    } else {
      printf("  name.host = %s\n", hinfo->name.host);
    }
#endif

    mHostFiltersArray.AppendElement(hinfo);
    hinfo = nullptr;
  loser:
    delete hinfo;
  }
}

nsresult nsProtocolProxyService::GetProtocolInfo(nsIURI* uri,
                                                 nsProtocolInfo* info) {
  MOZ_ASSERT(uri, "URI is null");
  MOZ_ASSERT(info, "info is null");

  nsresult rv;

  rv = uri->GetScheme(info->scheme);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIIOService> ios = do_GetIOService(&rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIProtocolHandler> handler;
  rv = ios->GetProtocolHandler(info->scheme.get(), getter_AddRefs(handler));
  if (NS_FAILED(rv)) return rv;

  rv = handler->DoGetProtocolFlags(uri, &info->flags);
  if (NS_FAILED(rv)) return rv;

  rv = handler->GetDefaultPort(&info->defaultPort);
  return rv;
}

nsresult nsProtocolProxyService::NewProxyInfo_Internal(
    const char* aType, const nsACString& aHost, int32_t aPort,
    const nsACString& aUsername, const nsACString& aPassword,
    const nsACString& aProxyAuthorizationHeader,
    const nsACString& aConnectionIsolationKey, uint32_t aFlags,
    uint32_t aFailoverTimeout, nsIProxyInfo* aFailoverProxy,
    uint32_t aResolveFlags, nsIProxyInfo** aResult) {
  if (aPort <= 0) aPort = -1;

  nsCOMPtr<nsProxyInfo> failover;
  if (aFailoverProxy) {
    failover = do_QueryInterface(aFailoverProxy);
    NS_ENSURE_ARG(failover);
  }

  RefPtr<nsProxyInfo> proxyInfo = new nsProxyInfo();

  proxyInfo->mType = aType;
  proxyInfo->mHost = aHost;
  proxyInfo->mPort = aPort;
  proxyInfo->mUsername = aUsername;
  proxyInfo->mPassword = aPassword;
  proxyInfo->mFlags = aFlags;
  proxyInfo->mResolveFlags = aResolveFlags;
  proxyInfo->mTimeout =
      aFailoverTimeout == UINT32_MAX ? mFailedProxyTimeout : aFailoverTimeout;
  proxyInfo->mProxyAuthorizationHeader = aProxyAuthorizationHeader;
  proxyInfo->mConnectionIsolationKey = aConnectionIsolationKey;
  failover.swap(proxyInfo->mNext);

  proxyInfo.forget(aResult);
  return NS_OK;
}

nsresult nsProtocolProxyService::Resolve_Internal(nsIChannel* channel,
                                                  const nsProtocolInfo& info,
                                                  uint32_t flags,
                                                  bool* usePACThread,
                                                  nsIProxyInfo** result) {
  NS_ENSURE_ARG_POINTER(channel);

  *usePACThread = false;
  *result = nullptr;

  if (!(info.flags & nsIProtocolHandler::ALLOWS_PROXY))
    return NS_OK;  // Can't proxy this (filters may not override)

  nsCOMPtr<nsIURI> uri;
  nsresult rv = GetProxyURI(channel, getter_AddRefs(uri));
  if (NS_FAILED(rv)) return rv;

  // See bug #586908.
  // Avoid endless loop if |uri| is the current PAC-URI. Returning OK
  // here means that we will not use a proxy for this connection.
  if (mPACMan && mPACMan->IsPACURI(uri)) return NS_OK;

  // if proxies are enabled and this host:port combo is supposed to use a
  // proxy, check for a proxy.
  if ((mProxyConfig == PROXYCONFIG_DIRECT) ||
      !CanUseProxy(uri, info.defaultPort))
    return NS_OK;

  bool mainThreadOnly;
  if (mSystemProxySettings && mProxyConfig == PROXYCONFIG_SYSTEM &&
      NS_SUCCEEDED(mSystemProxySettings->GetMainThreadOnly(&mainThreadOnly)) &&
      !mainThreadOnly) {
    *usePACThread = true;
    return NS_OK;
  }

  if (mSystemProxySettings && mProxyConfig == PROXYCONFIG_SYSTEM) {
    // If the system proxy setting implementation is not threadsafe (e.g
    // linux gconf), we'll do it inline here. Such implementations promise
    // not to block
    // bug 1366133: this block uses GetPACURI & GetProxyForURI, which may
    // hang on Windows platform. Fortunately, current implementation on
    // Windows is not main thread only, so we are safe here.

    nsAutoCString PACURI;
    nsAutoCString pacString;

    if (NS_SUCCEEDED(mSystemProxySettings->GetPACURI(PACURI)) &&
        !PACURI.IsEmpty()) {
      // There is a PAC URI configured. If it is unchanged, then
      // just execute the PAC thread. If it is changed then load
      // the new value

      if (mPACMan && mPACMan->IsPACURI(PACURI)) {
        // unchanged
        *usePACThread = true;
        return NS_OK;
      }

      ConfigureFromPAC(PACURI, false);
      return NS_OK;
    }

    nsAutoCString spec;
    nsAutoCString host;
    nsAutoCString scheme;
    int32_t port = -1;

    uri->GetAsciiSpec(spec);
    uri->GetAsciiHost(host);
    uri->GetScheme(scheme);
    uri->GetPort(&port);

    if (flags & RESOLVE_PREFER_SOCKS_PROXY) {
      LOG(("Ignoring RESOLVE_PREFER_SOCKS_PROXY for system proxy setting\n"));
    } else if (flags & RESOLVE_PREFER_HTTPS_PROXY) {
      scheme.AssignLiteral("https");
    } else if (flags & RESOLVE_IGNORE_URI_SCHEME) {
      scheme.AssignLiteral("http");
    }

    // now try the system proxy settings for this particular url
    if (NS_SUCCEEDED(mSystemProxySettings->GetProxyForURI(spec, scheme, host,
                                                          port, pacString))) {
      nsCOMPtr<nsIProxyInfo> pi;
      ProcessPACString(pacString, 0, getter_AddRefs(pi));

      if (flags & RESOLVE_PREFER_SOCKS_PROXY &&
          flags & RESOLVE_PREFER_HTTPS_PROXY) {
        nsAutoCString type;
        pi->GetType(type);
        // DIRECT from ProcessPACString indicates that system proxy settings
        // are not configured to use SOCKS proxy. Try https proxy as a
        // secondary preferrable proxy. This is mainly for websocket whose
        // proxy precedence is SOCKS > HTTPS > DIRECT.
        if (type.EqualsLiteral(kProxyType_DIRECT)) {
          scheme.AssignLiteral(kProxyType_HTTPS);
          if (NS_SUCCEEDED(mSystemProxySettings->GetProxyForURI(
                  spec, scheme, host, port, pacString))) {
            ProcessPACString(pacString, 0, getter_AddRefs(pi));
          }
        }
      }
      pi.forget(result);
      return NS_OK;
    }
  }

  // if proxies are enabled and this host:port combo is supposed to use a
  // proxy, check for a proxy.
  if (mProxyConfig == PROXYCONFIG_DIRECT ||
      (mProxyConfig == PROXYCONFIG_MANUAL &&
       !CanUseProxy(uri, info.defaultPort)))
    return NS_OK;

  // Proxy auto config magic...
  if (mProxyConfig == PROXYCONFIG_PAC || mProxyConfig == PROXYCONFIG_WPAD) {
    // Do not query PAC now.
    *usePACThread = true;
    return NS_OK;
  }

  // If we aren't in manual proxy configuration mode then we don't
  // want to honor any manual specific prefs that might be still set
  if (mProxyConfig != PROXYCONFIG_MANUAL) return NS_OK;

  // proxy info values for manual configuration mode
  const char* type = nullptr;
  const nsACString* host = nullptr;
  int32_t port = -1;

  uint32_t proxyFlags = 0;

  if ((flags & RESOLVE_PREFER_SOCKS_PROXY) && !mSOCKSProxyTarget.IsEmpty() &&
      (IsHostLocalTarget(mSOCKSProxyTarget) || mSOCKSProxyPort > 0)) {
    host = &mSOCKSProxyTarget;
    if (mSOCKSProxyVersion == 4)
      type = kProxyType_SOCKS4;
    else
      type = kProxyType_SOCKS;
    port = mSOCKSProxyPort;
    if (mSOCKSProxyRemoteDNS)
      proxyFlags |= nsIProxyInfo::TRANSPARENT_PROXY_RESOLVES_HOST;
  } else if ((flags & RESOLVE_PREFER_HTTPS_PROXY) &&
             !mHTTPSProxyHost.IsEmpty() && mHTTPSProxyPort > 0) {
    host = &mHTTPSProxyHost;
    type = kProxyType_HTTP;
    port = mHTTPSProxyPort;
  } else if (!mHTTPProxyHost.IsEmpty() && mHTTPProxyPort > 0 &&
             ((flags & RESOLVE_IGNORE_URI_SCHEME) ||
              info.scheme.EqualsLiteral("http"))) {
    host = &mHTTPProxyHost;
    type = kProxyType_HTTP;
    port = mHTTPProxyPort;
  } else if (!mHTTPSProxyHost.IsEmpty() && mHTTPSProxyPort > 0 &&
             !(flags & RESOLVE_IGNORE_URI_SCHEME) &&
             info.scheme.EqualsLiteral("https")) {
    host = &mHTTPSProxyHost;
    type = kProxyType_HTTP;
    port = mHTTPSProxyPort;
  } else if (!mSOCKSProxyTarget.IsEmpty() &&
             (IsHostLocalTarget(mSOCKSProxyTarget) || mSOCKSProxyPort > 0)) {
    host = &mSOCKSProxyTarget;
    if (mSOCKSProxyVersion == 4)
      type = kProxyType_SOCKS4;
    else
      type = kProxyType_SOCKS;
    port = mSOCKSProxyPort;
    if (mSOCKSProxyRemoteDNS)
      proxyFlags |= nsIProxyInfo::TRANSPARENT_PROXY_RESOLVES_HOST;
  }

  if (type) {
    rv = NewProxyInfo_Internal(type, *host, port, ""_ns, ""_ns, ""_ns, ""_ns,
                               proxyFlags, UINT32_MAX, nullptr, flags, result);
    if (NS_FAILED(rv)) return rv;
  }

  return NS_OK;
}

void nsProtocolProxyService::MaybeDisableDNSPrefetch(nsIProxyInfo* aProxy) {
  // Disable Prefetch in the DNS service if a proxy is in use.
  if (!aProxy) return;

  nsCOMPtr<nsProxyInfo> pi = do_QueryInterface(aProxy);
  if (!pi || !pi->mType || pi->mType == kProxyType_DIRECT) return;

  nsCOMPtr<nsIDNSService> dns = do_GetService(NS_DNSSERVICE_CONTRACTID);
  if (!dns) return;
  nsCOMPtr<nsPIDNSService> pdns = do_QueryInterface(dns);
  if (!pdns) return;

  // We lose the prefetch optimization for the life of the dns service.
  pdns->SetPrefetchEnabled(false);
}

void nsProtocolProxyService::CopyFilters(nsTArray<RefPtr<FilterLink>>& aCopy) {
  MOZ_ASSERT(aCopy.Length() == 0);
  aCopy.AppendElements(mFilters);
}

bool nsProtocolProxyService::ApplyFilter(
    FilterLink const* filterLink, nsIChannel* channel,
    const nsProtocolInfo& info, nsCOMPtr<nsIProxyInfo> list,
    nsIProxyProtocolFilterResult* callback) {
  nsresult rv;

  // We prune the proxy list prior to invoking each filter.  This may be
  // somewhat inefficient, but it seems like a good idea since we want each
  // filter to "see" a valid proxy list.
  PruneProxyInfo(info, list);

  if (filterLink->filter) {
    nsCOMPtr<nsIURI> uri;
    Unused << GetProxyURI(channel, getter_AddRefs(uri));
    if (!uri) {
      return false;
    }

    rv = filterLink->filter->ApplyFilter(uri, list, callback);
    return NS_SUCCEEDED(rv);
  }

  if (filterLink->channelFilter) {
    rv = filterLink->channelFilter->ApplyFilter(channel, list, callback);
    return NS_SUCCEEDED(rv);
  }

  return false;
}

void nsProtocolProxyService::PruneProxyInfo(const nsProtocolInfo& info,
                                            nsIProxyInfo** list) {
  if (!*list) return;

  LOG(("nsProtocolProxyService::PruneProxyInfo ENTER list=%p", *list));

  nsProxyInfo* head = nullptr;
  CallQueryInterface(*list, &head);
  if (!head) {
    MOZ_ASSERT_UNREACHABLE("nsIProxyInfo must QI to nsProxyInfo");
    return;
  }
  NS_RELEASE(*list);

  // Pruning of disabled proxies works like this:
  //   - If all proxies are disabled, return the full list
  //   - Otherwise, remove the disabled proxies.
  //
  // Pruning of disallowed proxies works like this:
  //   - If the protocol handler disallows the proxy, then we disallow it.

  // Start by removing all disallowed proxies if required:
  if (!(info.flags & nsIProtocolHandler::ALLOWS_PROXY_HTTP)) {
    nsProxyInfo *last = nullptr, *iter = head;
    while (iter) {
      if ((iter->Type() == kProxyType_HTTP) ||
          (iter->Type() == kProxyType_HTTPS)) {
        // reject!
        if (last)
          last->mNext = iter->mNext;
        else
          head = iter->mNext;
        nsProxyInfo* next = iter->mNext;
        iter->mNext = nullptr;
        iter->Release();
        iter = next;
      } else {
        last = iter;
        iter = iter->mNext;
      }
    }
    if (!head) {
      return;
    }
  }

  // Scan to see if all remaining non-direct proxies are disabled. If so, then
  // we'll just bail and return them all.  Otherwise, we'll go and prune the
  // disabled ones.

  bool allNonDirectProxiesDisabled = true;

  nsProxyInfo* iter;
  for (iter = head; iter; iter = iter->mNext) {
    if (!IsProxyDisabled(iter) && iter->mType != kProxyType_DIRECT) {
      allNonDirectProxiesDisabled = false;
      break;
    }
  }

  if (allNonDirectProxiesDisabled) {
    LOG(("All proxies are disabled, so trying all again"));
  } else {
    // remove any disabled proxies.
    nsProxyInfo* last = nullptr;
    for (iter = head; iter;) {
      if (IsProxyDisabled(iter)) {
        // reject!
        nsProxyInfo* reject = iter;

        iter = iter->mNext;
        if (last)
          last->mNext = iter;
        else
          head = iter;

        reject->mNext = nullptr;
        NS_RELEASE(reject);
        continue;
      }

      // since we are about to use this proxy, make sure it is not on
      // the disabled proxy list.  we'll add it back to that list if
      // we have to (in GetFailoverForProxy).
      //
      // XXX(darin): It might be better to do this as a final pass.
      //
      EnableProxy(iter);

      last = iter;
      iter = iter->mNext;
    }
  }

  // if only DIRECT was specified then return no proxy info, and we're done.
  if (head && !head->mNext && head->mType == kProxyType_DIRECT)
    NS_RELEASE(head);

  *list = head;  // Transfer ownership

  LOG(("nsProtocolProxyService::PruneProxyInfo LEAVE list=%p", *list));
}

bool nsProtocolProxyService::GetIsPACLoading() {
  return mPACMan && mPACMan->IsLoading();
}

}  // namespace net
}  // namespace mozilla
