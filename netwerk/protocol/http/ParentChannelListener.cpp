/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "ParentChannelListener.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/ServiceWorkerInterceptController.h"
#include "mozilla/dom/ServiceWorkerUtils.h"
#include "mozilla/net/HttpChannelParent.h"
#include "mozilla/net/RedirectChannelRegistrar.h"
#include "mozilla/SchedulerGroup.h"
#include "mozilla/Unused.h"
#include "nsIHttpHeaderVisitor.h"
#include "nsIPrompt.h"
#include "nsISecureBrowserUI.h"
#include "nsIWindowWatcher.h"
#include "nsQueryObject.h"
#include "nsIAuthPrompt.h"
#include "nsIAuthPrompt2.h"
#include "nsIPromptFactory.h"
#include "Element.h"
#include "nsILoginManagerAuthPrompter.h"
#include "mozilla/dom/CanonicalBrowsingContext.h"
#include "mozilla/dom/LoadURIOptionsBinding.h"
#include "nsIWebNavigation.h"

using mozilla::Unused;
using mozilla::dom::ServiceWorkerInterceptController;
using mozilla::dom::ServiceWorkerParentInterceptEnabled;

namespace mozilla {
namespace net {

ParentChannelListener::ParentChannelListener(
    nsIStreamListener* aListener,
    dom::CanonicalBrowsingContext* aBrowsingContext, bool aUsePrivateBrowsing)
    : mNextListener(aListener),
      mSuspendedForDiversion(false),
      mShouldIntercept(false),
      mShouldSuspendIntercept(false),
      mInterceptCanceled(false),
      mBrowsingContext(aBrowsingContext),
      mUsePrivateBrowsing(aUsePrivateBrowsing) {
  LOG(("ParentChannelListener::ParentChannelListener [this=%p, next=%p]", this,
       aListener));

  if (ServiceWorkerParentInterceptEnabled()) {
    mInterceptController = new ServiceWorkerInterceptController();
  }
}

ParentChannelListener::~ParentChannelListener() {
  LOG(("ParentChannelListener::~ParentChannelListener %p", this));
}

//-----------------------------------------------------------------------------
// ParentChannelListener::nsISupports
//-----------------------------------------------------------------------------

NS_IMPL_ADDREF(ParentChannelListener)
NS_IMPL_RELEASE(ParentChannelListener)
NS_INTERFACE_MAP_BEGIN(ParentChannelListener)
  NS_INTERFACE_MAP_ENTRY(nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
  NS_INTERFACE_MAP_ENTRY(nsIMultiPartChannelListener)
  NS_INTERFACE_MAP_ENTRY(nsINetworkInterceptController)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAuthPromptProvider, mBrowsingContext)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIRemoteWindowContext, mBrowsingContext)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(ParentChannelListener)
NS_INTERFACE_MAP_END

//-----------------------------------------------------------------------------
// ParentChannelListener::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ParentChannelListener::OnStartRequest(nsIRequest* aRequest) {
  MOZ_RELEASE_ASSERT(!mSuspendedForDiversion,
                     "Cannot call OnStartRequest if suspended for diversion!");

  if (!mNextListener) return NS_ERROR_UNEXPECTED;

  // If we're not a multi-part channel, then we can drop mListener and break the
  // reference cycle. If we are, then this might be called again, so wait for
  // OnAfterLastPart instead.
  nsCOMPtr<nsIMultiPartChannel> multiPartChannel = do_QueryInterface(aRequest);
  if (multiPartChannel) {
    mIsMultiPart = true;
  }

  LOG(("ParentChannelListener::OnStartRequest [this=%p]\n", this));
  return mNextListener->OnStartRequest(aRequest);
}

NS_IMETHODIMP
ParentChannelListener::OnStopRequest(nsIRequest* aRequest,
                                     nsresult aStatusCode) {
  MOZ_RELEASE_ASSERT(!mSuspendedForDiversion,
                     "Cannot call OnStopRequest if suspended for diversion!");

  if (!mNextListener) return NS_ERROR_UNEXPECTED;

  LOG(("ParentChannelListener::OnStopRequest: [this=%p status=%" PRIu32 "]\n",
       this, static_cast<uint32_t>(aStatusCode)));
  nsresult rv = mNextListener->OnStopRequest(aRequest, aStatusCode);

  if (!mIsMultiPart) {
    mNextListener = nullptr;
  }
  return rv;
}

//-----------------------------------------------------------------------------
// ParentChannelListener::nsIStreamListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ParentChannelListener::OnDataAvailable(nsIRequest* aRequest,
                                       nsIInputStream* aInputStream,
                                       uint64_t aOffset, uint32_t aCount) {
  MOZ_RELEASE_ASSERT(!mSuspendedForDiversion,
                     "Cannot call OnDataAvailable if suspended for diversion!");

  if (!mNextListener) return NS_ERROR_UNEXPECTED;

  LOG(("ParentChannelListener::OnDataAvailable [this=%p]\n", this));
  return mNextListener->OnDataAvailable(aRequest, aInputStream, aOffset,
                                        aCount);
}

//-----------------------------------------------------------------------------
// ParentChannelListener::nsIMultiPartChannelListener
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ParentChannelListener::OnAfterLastPart(nsresult aStatus) {
  nsCOMPtr<nsIMultiPartChannelListener> multiListener =
      do_QueryInterface(mNextListener);
  if (multiListener) {
    multiListener->OnAfterLastPart(aStatus);
  }

  mNextListener = nullptr;
  return NS_OK;
}

//-----------------------------------------------------------------------------
// ParentChannelListener::nsIInterfaceRequestor
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ParentChannelListener::GetInterface(const nsIID& aIID, void** result) {
  if (aIID.Equals(NS_GET_IID(nsINetworkInterceptController)) ||
      aIID.Equals(NS_GET_IID(nsIRemoteWindowContext))) {
    return QueryInterface(aIID, result);
  }

  if (mBrowsingContext && aIID.Equals(NS_GET_IID(nsIPrompt))) {
    nsCOMPtr<dom::Element> frameElement =
        mBrowsingContext->Top()->GetEmbedderElement();
    if (frameElement) {
      nsCOMPtr<nsPIDOMWindowOuter> win = frameElement->OwnerDoc()->GetWindow();
      NS_ENSURE_TRUE(win, NS_ERROR_NO_INTERFACE);

      nsresult rv;
      nsCOMPtr<nsIWindowWatcher> wwatch =
          do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);

      if (NS_WARN_IF(!NS_SUCCEEDED(rv))) {
        return NS_ERROR_NO_INTERFACE;
      }

      nsCOMPtr<nsIPrompt> prompt;
      rv = wwatch->GetNewPrompter(win, getter_AddRefs(prompt));
      if (NS_WARN_IF(!NS_SUCCEEDED(rv))) {
        return NS_ERROR_NO_INTERFACE;
      }

      prompt.forget(result);
      return NS_OK;
    }
  }

  if (mBrowsingContext && (aIID.Equals(NS_GET_IID(nsIAuthPrompt)) ||
                           aIID.Equals(NS_GET_IID(nsIAuthPrompt2)))) {
    nsresult rv =
        GetAuthPrompt(nsIAuthPromptProvider::PROMPT_NORMAL, aIID, result);
    if (NS_FAILED(rv)) {
      return NS_ERROR_NO_INTERFACE;
    }
    return NS_OK;
  }

  nsCOMPtr<nsIInterfaceRequestor> ir;
  if (mNextListener && NS_SUCCEEDED(CallQueryInterface(mNextListener.get(),
                                                       getter_AddRefs(ir)))) {
    return ir->GetInterface(aIID, result);
  }

  return NS_NOINTERFACE;
}

void ParentChannelListener::SetListenerAfterRedirect(
    nsIStreamListener* aListener) {
  mInterceptCanceled = false;
  mNextListener = aListener;
}

//-----------------------------------------------------------------------------
// ParentChannelListener::nsINetworkInterceptController
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ParentChannelListener::ShouldPrepareForIntercept(nsIURI* aURI,
                                                 nsIChannel* aChannel,
                                                 bool* aShouldIntercept) {
  // If parent-side interception is enabled just forward to the real
  // network controler.
  if (mInterceptController) {
    return mInterceptController->ShouldPrepareForIntercept(aURI, aChannel,
                                                           aShouldIntercept);
  }
  *aShouldIntercept = mShouldIntercept;
  return NS_OK;
}

class HeaderVisitor final : public nsIHttpHeaderVisitor {
  nsCOMPtr<nsIInterceptedChannel> mChannel;
  ~HeaderVisitor() = default;

 public:
  explicit HeaderVisitor(nsIInterceptedChannel* aChannel)
      : mChannel(aChannel) {}

  NS_DECL_ISUPPORTS

  NS_IMETHOD VisitHeader(const nsACString& aHeader,
                         const nsACString& aValue) override {
    mChannel->SynthesizeHeader(aHeader, aValue);
    return NS_OK;
  }
};

NS_IMPL_ISUPPORTS(HeaderVisitor, nsIHttpHeaderVisitor)

class FinishSynthesizedResponse : public Runnable {
  nsCOMPtr<nsIInterceptedChannel> mChannel;

 public:
  explicit FinishSynthesizedResponse(nsIInterceptedChannel* aChannel)
      : Runnable("net::FinishSynthesizedResponse"), mChannel(aChannel) {}

  NS_IMETHOD Run() override {
    // The URL passed as an argument here doesn't matter, since the child will
    // receive a redirection notification as a result of this synthesized
    // response.
    mChannel->StartSynthesizedResponse(nullptr, nullptr, nullptr,
                                       EmptyCString(), false);
    mChannel->FinishSynthesizedResponse();
    return NS_OK;
  }
};

NS_IMETHODIMP
ParentChannelListener::ChannelIntercepted(nsIInterceptedChannel* aChannel) {
  // If parent-side interception is enabled just forward to the real
  // network controler.
  if (mInterceptController) {
    return mInterceptController->ChannelIntercepted(aChannel);
  }

  // Its possible for the child-side interception to complete and tear down
  // the actor before we even get this parent-side interception notification.
  // In this case we want to let the interception succeed, but then immediately
  // cancel it.  If we return an error code from here then it might get
  // propagated back to the child process where the interception did not
  // encounter an error.  Therefore cancel the new channel asynchronously from a
  // runnable.
  if (mInterceptCanceled) {
    nsCOMPtr<nsIRunnable> r = NewRunnableMethod<nsresult>(
        "ParentChannelListener::CancelInterception", aChannel,
        &nsIInterceptedChannel::CancelInterception, NS_BINDING_ABORTED);
    MOZ_ALWAYS_SUCCEEDS(
        SchedulerGroup::Dispatch(TaskCategory::Other, r.forget()));
    return NS_OK;
  }

  if (mShouldSuspendIntercept) {
    mInterceptedChannel = aChannel;
    return NS_OK;
  }

  nsAutoCString statusText;
  mSynthesizedResponseHead->StatusText(statusText);
  aChannel->SynthesizeStatus(mSynthesizedResponseHead->Status(), statusText);
  nsCOMPtr<nsIHttpHeaderVisitor> visitor = new HeaderVisitor(aChannel);
  DebugOnly<nsresult> rv = mSynthesizedResponseHead->VisitHeaders(
      visitor, nsHttpHeaderArray::eFilterResponse);
  MOZ_ASSERT(NS_SUCCEEDED(rv));

  nsCOMPtr<nsIRunnable> event = new FinishSynthesizedResponse(aChannel);
  NS_DispatchToCurrentThread(event);

  mSynthesizedResponseHead = nullptr;

  MOZ_ASSERT(mNextListener);
  RefPtr<HttpChannelParent> channel = do_QueryObject(mNextListener);
  MOZ_ASSERT(channel);
  channel->ResponseSynthesized();

  return NS_OK;
}

//-----------------------------------------------------------------------------

nsresult ParentChannelListener::SuspendForDiversion() {
  if (NS_WARN_IF(mSuspendedForDiversion)) {
    MOZ_ASSERT(!mSuspendedForDiversion, "Cannot SuspendForDiversion twice!");
    return NS_ERROR_UNEXPECTED;
  }

  // While this is set, no OnStart/OnData/OnStop callbacks should be forwarded
  // to mNextListener.
  mSuspendedForDiversion = true;

  return NS_OK;
}

void ParentChannelListener::ResumeForDiversion() {
  MOZ_RELEASE_ASSERT(mSuspendedForDiversion, "Must already be suspended!");
  // Allow OnStart/OnData/OnStop callbacks to be forwarded to mNextListener.
  mSuspendedForDiversion = false;
}

void ParentChannelListener::DivertTo(nsIStreamListener* aListener) {
  MOZ_ASSERT(aListener);
  MOZ_RELEASE_ASSERT(mSuspendedForDiversion, "Must already be suspended!");

  // Reset mInterceptCanceled back to false every time a new listener is set.
  // We only want to cancel the interception if our current listener has
  // signaled its cleaning up.
  mInterceptCanceled = false;

  mNextListener = aListener;
  ResumeForDiversion();
}

void ParentChannelListener::SetupInterception(
    const nsHttpResponseHead& aResponseHead) {
  mSynthesizedResponseHead = MakeUnique<nsHttpResponseHead>(aResponseHead);
  mShouldIntercept = true;
}

void ParentChannelListener::SetupInterceptionAfterRedirect(
    bool aShouldIntercept) {
  mShouldIntercept = aShouldIntercept;
  if (mShouldIntercept) {
    // When an interception occurs, this channel should suspend all further
    // activity. It will be torn down and recreated if necessary.
    mShouldSuspendIntercept = true;
  }
}

void ParentChannelListener::ClearInterceptedChannel(
    nsIStreamListener* aListener) {
  // Only cancel the interception if this is from our current listener.  We
  // can get spurious calls here from other HttpChannelParent instances being
  // destroyed asynchronously.
  if (!SameCOMIdentity(mNextListener, aListener)) {
    return;
  }
  if (mInterceptedChannel) {
    mInterceptedChannel->CancelInterception(NS_ERROR_INTERCEPTION_FAILED);
    mInterceptedChannel = nullptr;
  }
  // Note that channel interception has been canceled.  If we got this before
  // the interception even occured we will trigger the cancel later.
  mInterceptCanceled = true;
}

//-----------------------------------------------------------------------------
// ParentChannelListener::nsIAuthPromptProvider
//

NS_IMETHODIMP
ParentChannelListener::GetAuthPrompt(uint32_t aPromptReason, const nsIID& iid,
                                     void** aResult) {
  if (!mBrowsingContext) {
    return NS_ERROR_NOT_AVAILABLE;
  }
  // we're either allowing auth, or it's a proxy request
  nsresult rv;
  nsCOMPtr<nsIPromptFactory> wwatch =
      do_GetService(NS_WINDOWWATCHER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsPIDOMWindowOuter> window;
  RefPtr<dom::Element> frame = mBrowsingContext->Top()->GetEmbedderElement();
  if (frame) window = frame->OwnerDoc()->GetWindow();

  // Get an auth prompter for our window so that the parenting
  // of the dialogs works as it should when using tabs.
  nsCOMPtr<nsISupports> prompt;
  rv = wwatch->GetPrompt(window, iid, getter_AddRefs(prompt));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILoginManagerAuthPrompter> prompter = do_QueryInterface(prompt);
  if (prompter) {
    prompter->SetBrowser(frame);
  }

  *aResult = prompt.forget().take();
  return NS_OK;
}

//-----------------------------------------------------------------------------
// ParentChannelListener::nsIRemoteWindowContext
//

NS_IMETHODIMP
ParentChannelListener::OpenURI(nsIURI* aURI) {
  nsCString spec;
  aURI->GetSpec(spec);

  RefPtr<dom::CanonicalBrowsingContext> bc = mBrowsingContext;

  NS_DispatchToMainThread(
      NS_NewRunnableFunction("ParentChannelListener::OpenURI", [spec, bc]() {
        dom::LoadURIOptions loadURIOptions;
        loadURIOptions.mTriggeringPrincipal =
            nsContentUtils::GetSystemPrincipal();
        loadURIOptions.mLoadFlags =
            nsIWebNavigation::LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP |
            nsIWebNavigation::LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL;

        ErrorResult rv;
        bc->LoadURI(NS_ConvertUTF8toUTF16(spec), loadURIOptions, rv);
      }));
  return NS_OK;
}

NS_IMETHODIMP
ParentChannelListener::GetUsePrivateBrowsing(bool* aUsePrivateBrowsing) {
  *aUsePrivateBrowsing = mUsePrivateBrowsing;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla
