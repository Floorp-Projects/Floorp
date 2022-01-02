/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// HttpLog.h should generally be included first
#include "HttpLog.h"

#include "ParentChannelListener.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Document.h"
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

namespace mozilla {
namespace net {

ParentChannelListener::ParentChannelListener(
    nsIStreamListener* aListener,
    dom::CanonicalBrowsingContext* aBrowsingContext, bool aUsePrivateBrowsing)
    : mNextListener(aListener), mBrowsingContext(aBrowsingContext) {
  LOG(("ParentChannelListener::ParentChannelListener [this=%p, next=%p]", this,
       aListener));

  mInterceptController = new ServiceWorkerInterceptController();
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
  NS_INTERFACE_MAP_ENTRY(nsIThreadRetargetableStreamListener)
  NS_INTERFACE_MAP_ENTRY_CONDITIONAL(nsIAuthPromptProvider, mBrowsingContext)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIInterfaceRequestor)
  NS_INTERFACE_MAP_ENTRY_CONCRETE(ParentChannelListener)
NS_INTERFACE_MAP_END

//-----------------------------------------------------------------------------
// ParentChannelListener::nsIRequestObserver
//-----------------------------------------------------------------------------

NS_IMETHODIMP
ParentChannelListener::OnStartRequest(nsIRequest* aRequest) {
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
  if (aIID.Equals(NS_GET_IID(nsINetworkInterceptController))) {
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
  *aShouldIntercept = false;
  return NS_OK;
}

NS_IMETHODIMP
ParentChannelListener::ChannelIntercepted(nsIInterceptedChannel* aChannel) {
  // If parent-side interception is enabled just forward to the real
  // network controler.
  if (mInterceptController) {
    return mInterceptController->ChannelIntercepted(aChannel);
  }

  return NS_OK;
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
// ParentChannelListener::nsIThreadRetargetableStreamListener
//

NS_IMETHODIMP
ParentChannelListener::CheckListenerChain() {
  MOZ_ASSERT(NS_IsMainThread());

  nsCOMPtr<nsIThreadRetargetableStreamListener> listener =
      do_QueryInterface(mNextListener);
  if (!listener) {
    return NS_ERROR_NO_INTERFACE;
  }

  return listener->CheckListenerChain();
}

}  // namespace net
}  // namespace mozilla
