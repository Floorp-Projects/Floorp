/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim:set ts=2 sts=2 sw=2 et cin:
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/dom/ContentChild.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/ScopeExit.h"
#include "nsIURI.h"
#include "nsExternalProtocolHandler.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsCOMPtr.h"
#include "nsContentUtils.h"
#include "nsServiceManagerUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIRedirectHistoryEntry.h"
#include "nsNetUtil.h"
#include "nsContentSecurityManager.h"
#include "nsExternalHelperAppService.h"

// used to dispatch urls to default protocol handlers
#include "nsCExternalHandlerService.h"
#include "nsIExternalProtocolService.h"
#include "nsIChildChannel.h"
#include "nsIParentChannel.h"

class nsILoadInfo;

////////////////////////////////////////////////////////////////////////
// a stub channel implemenation which will map calls to AsyncRead and
// OpenInputStream to calls in the OS for loading the url.
////////////////////////////////////////////////////////////////////////

class nsExtProtocolChannel : public nsIChannel,
                             public nsIChildChannel,
                             public nsIParentChannel {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSICHANNEL
  NS_DECL_NSIREQUESTOBSERVER
  NS_DECL_NSISTREAMLISTENER
  NS_DECL_NSIREQUEST
  NS_DECL_NSICHILDCHANNEL
  NS_DECL_NSIPARENTCHANNEL

  nsExtProtocolChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo);

 private:
  virtual ~nsExtProtocolChannel();

  nsresult OpenURL();
  void Finish(nsresult aResult);

  nsCOMPtr<nsIURI> mUrl;
  nsCOMPtr<nsIURI> mOriginalURI;
  nsresult mStatus;
  nsLoadFlags mLoadFlags;
  bool mWasOpened;
  bool mCanceled;
  // Set true (as a result of ConnectParent invoked from child process)
  // when this channel is on the parent process and is being used as
  // a redirect target channel.  It turns AsyncOpen into a no-op since
  // we do it on the child.
  bool mConnectedParent;

  nsCOMPtr<nsIInterfaceRequestor> mCallbacks;
  nsCOMPtr<nsILoadGroup> mLoadGroup;
  nsCOMPtr<nsILoadInfo> mLoadInfo;
  nsCOMPtr<nsIStreamListener> mListener;
};

NS_IMPL_ADDREF(nsExtProtocolChannel)
NS_IMPL_RELEASE(nsExtProtocolChannel)

NS_INTERFACE_MAP_BEGIN(nsExtProtocolChannel)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIChannel)
  NS_INTERFACE_MAP_ENTRY(nsIRequest)
  NS_INTERFACE_MAP_ENTRY(nsIChildChannel)
  NS_INTERFACE_MAP_ENTRY(nsIParentChannel)
  NS_INTERFACE_MAP_ENTRY(nsIStreamListener)
  NS_INTERFACE_MAP_ENTRY(nsIRequestObserver)
NS_INTERFACE_MAP_END

nsExtProtocolChannel::nsExtProtocolChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo)
    : mUrl(aURI),
      mOriginalURI(aURI),
      mStatus(NS_OK),
      mLoadFlags(nsIRequest::LOAD_NORMAL),
      mWasOpened(false),
      mCanceled(false),
      mConnectedParent(false),
      mLoadInfo(aLoadInfo) {}

nsExtProtocolChannel::~nsExtProtocolChannel() {}

NS_IMETHODIMP nsExtProtocolChannel::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  NS_IF_ADDREF(*aLoadGroup = mLoadGroup);
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  mLoadGroup = aLoadGroup;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetNotificationCallbacks(
    nsIInterfaceRequestor** aCallbacks) {
  NS_IF_ADDREF(*aCallbacks = mCallbacks);
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetNotificationCallbacks(
    nsIInterfaceRequestor* aCallbacks) {
  mCallbacks = aCallbacks;
  return NS_OK;
}

NS_IMETHODIMP
nsExtProtocolChannel::GetSecurityInfo(
    nsITransportSecurityInfo** aSecurityInfo) {
  *aSecurityInfo = nullptr;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetOriginalURI(nsIURI** aURI) {
  NS_ADDREF(*aURI = mOriginalURI);
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetOriginalURI(nsIURI* aURI) {
  NS_ENSURE_ARG_POINTER(aURI);
  mOriginalURI = aURI;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetURI(nsIURI** aURI) {
  *aURI = mUrl;
  NS_IF_ADDREF(*aURI);
  return NS_OK;
}

nsresult nsExtProtocolChannel::OpenURL() {
  nsresult rv = NS_ERROR_FAILURE;
  nsCOMPtr<nsIExternalProtocolService> extProtService(
      do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));

  auto cleanup = mozilla::MakeScopeExit([&] {
    mCallbacks = nullptr;
    mListener = nullptr;
  });

  if (extProtService) {
    nsAutoCString urlScheme;
    mUrl->GetScheme(urlScheme);
    bool haveHandler = false;
    extProtService->ExternalProtocolHandlerExists(urlScheme.get(),
                                                  &haveHandler);
    if (!haveHandler) {
      return NS_ERROR_UNKNOWN_PROTOCOL;
    }

    RefPtr<mozilla::dom::BrowsingContext> ctx;
    rv = mLoadInfo->GetTargetBrowsingContext(getter_AddRefs(ctx));
    if (NS_FAILED(rv)) {
      return rv;
    }

    RefPtr<nsIPrincipal> triggeringPrincipal = mLoadInfo->TriggeringPrincipal();
    RefPtr<nsIPrincipal> redirectPrincipal;
    if (!mLoadInfo->RedirectChain().IsEmpty()) {
      mLoadInfo->RedirectChain().LastElement()->GetPrincipal(
          getter_AddRefs(redirectPrincipal));
    }
    rv = extProtService->LoadURI(mUrl, triggeringPrincipal, redirectPrincipal,
                                 ctx, mLoadInfo->GetLoadTriggeredFromExternal(),
                                 mLoadInfo->GetHasValidUserGestureActivation(),
                                 mLoadInfo->GetIsNewWindowTarget());

    if (NS_SUCCEEDED(rv) && mListener) {
      mStatus = NS_ERROR_NO_CONTENT;

      RefPtr<nsExtProtocolChannel> self = this;
      nsCOMPtr<nsIStreamListener> listener = mListener;
      MessageLoop::current()->PostTask(NS_NewRunnableFunction(
          "nsExtProtocolChannel::OpenURL", [self, listener]() {
            listener->OnStartRequest(self);
            listener->OnStopRequest(self, self->mStatus);
          }));
    }
  }

  return rv;
}

NS_IMETHODIMP nsExtProtocolChannel::Open(nsIInputStream** aStream) {
  nsCOMPtr<nsIStreamListener> listener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  NS_ENSURE_SUCCESS(rv, rv);

  return OpenURL();
}

NS_IMETHODIMP nsExtProtocolChannel::AsyncOpen(nsIStreamListener* aListener) {
  nsCOMPtr<nsIStreamListener> listener = aListener;
  nsresult rv =
      nsContentSecurityManager::doContentSecurityCheck(this, listener);
  if (NS_FAILED(rv)) {
    mCallbacks = nullptr;
    return rv;
  }

  if (mConnectedParent) {
    return NS_OK;
  }

  MOZ_ASSERT(
      mLoadInfo->GetSecurityMode() == 0 ||
          mLoadInfo->GetInitialSecurityCheckDone() ||
          (mLoadInfo->GetSecurityMode() ==
               nsILoadInfo::SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL &&
           mLoadInfo->GetLoadingPrincipal() &&
           mLoadInfo->GetLoadingPrincipal()->IsSystemPrincipal()),
      "security flags in loadInfo but doContentSecurityCheck() not called");

  NS_ENSURE_ARG_POINTER(listener);
  NS_ENSURE_TRUE(!mWasOpened, NS_ERROR_ALREADY_OPENED);

  mWasOpened = true;
  mListener = listener;

  return OpenURL();
}

NS_IMETHODIMP nsExtProtocolChannel::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetLoadFlags(nsLoadFlags aLoadFlags) {
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetTRRMode(nsIRequest::TRRMode* aTRRMode) {
  return GetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP nsExtProtocolChannel::SetTRRMode(nsIRequest::TRRMode aTRRMode) {
  return SetTRRModeImpl(aTRRMode);
}

NS_IMETHODIMP nsExtProtocolChannel::GetIsDocument(bool* aIsDocument) {
  return NS_GetIsDocumentChannel(this, aIsDocument);
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentType(nsACString& aContentType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::SetContentType(
    const nsACString& aContentType) {
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentCharset(
    nsACString& aContentCharset) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::SetContentCharset(
    const nsACString& aContentCharset) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentDisposition(
    uint32_t* aContentDisposition) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExtProtocolChannel::SetContentDisposition(
    uint32_t aContentDisposition) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExtProtocolChannel::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP nsExtProtocolChannel::GetContentLength(int64_t* aContentLength) {
  *aContentLength = -1;
  return NS_OK;
}

NS_IMETHODIMP
nsExtProtocolChannel::SetContentLength(int64_t aContentLength) {
  MOZ_ASSERT_UNREACHABLE("SetContentLength");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::GetOwner(nsISupports** aPrincipal) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::SetOwner(nsISupports* aPrincipal) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  MOZ_RELEASE_ASSERT(aLoadInfo, "loadinfo can't be null");
  mLoadInfo = aLoadInfo;
  return NS_OK;
}

////////////////////////////////////////////////////////////////////////////////
// From nsIRequest
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsExtProtocolChannel::GetName(nsACString& result) {
  return mUrl->GetSpec(result);
}

NS_IMETHODIMP nsExtProtocolChannel::IsPending(bool* result) {
  *result = false;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetStatus(nsresult* status) {
  *status = mStatus;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetCanceledReason(
    const nsACString& aReason) {
  return SetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP nsExtProtocolChannel::GetCanceledReason(nsACString& aReason) {
  return GetCanceledReasonImpl(aReason);
}

NS_IMETHODIMP nsExtProtocolChannel::CancelWithReason(
    nsresult aStatus, const nsACString& aReason) {
  return CancelWithReasonImpl(aStatus, aReason);
}

NS_IMETHODIMP nsExtProtocolChannel::Cancel(nsresult status) {
  if (NS_SUCCEEDED(mStatus)) {
    mStatus = status;
  }
  mCanceled = true;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetCanceled(bool* aCanceled) {
  *aCanceled = mCanceled;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::Suspend() {
  MOZ_ASSERT_UNREACHABLE("Suspend");
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::Resume() {
  MOZ_ASSERT_UNREACHABLE("Resume");
  return NS_ERROR_NOT_IMPLEMENTED;
}

///////////////////////////////////////////////////////////////////////
// From nsIChildChannel
//////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsExtProtocolChannel::ConnectParent(uint32_t registrarId) {
  mozilla::dom::ContentChild::GetSingleton()
      ->SendExtProtocolChannelConnectParent(registrarId);
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::CompleteRedirectSetup(
    nsIStreamListener* listener) {
  // For redirects to external protocols we AsyncOpen on the child
  // (not the parent) because child channel has the right docshell
  // (which is needed for the select dialog).
  return AsyncOpen(listener);
}

///////////////////////////////////////////////////////////////////////
// From nsIParentChannel (derives from nsIStreamListener)
//////////////////////////////////////////////////////////////////////

NS_IMETHODIMP nsExtProtocolChannel::SetParentListener(
    mozilla::net::ParentChannelListener* aListener) {
  // This is called as part of the connect parent operation from
  // ContentParent::RecvExtProtocolChannelConnectParent.  Setting
  // this flag tells this channel to not proceed and makes AsyncOpen
  // just no-op.  Actual operation will happen from the child process
  // via CompleteRedirectSetup call on the child channel.
  mConnectedParent = true;
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetClassifierMatchedInfo(
    const nsACString& aList, const nsACString& aProvider,
    const nsACString& aFullHash) {
  // nothing to do
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::SetClassifierMatchedTrackingInfo(
    const nsACString& aLists, const nsACString& aFullHashes) {
  // nothing to do
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::NotifyClassificationFlags(
    uint32_t aClassificationFlags, bool aIsThirdParty) {
  // nothing to do
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::Delete() {
  // nothing to do
  return NS_OK;
}

NS_IMETHODIMP nsExtProtocolChannel::GetRemoteType(nsACString& aRemoteType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsExtProtocolChannel::OnStartRequest(nsIRequest* aRequest) {
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsExtProtocolChannel::OnStopRequest(nsIRequest* aRequest,
                                                  nsresult aStatusCode) {
  MOZ_ASSERT(NS_FAILED(aStatusCode));
  return NS_ERROR_UNEXPECTED;
}

NS_IMETHODIMP nsExtProtocolChannel::OnDataAvailable(
    nsIRequest* aRequest, nsIInputStream* aInputStream, uint64_t aOffset,
    uint32_t aCount) {
  // no data is expected
  MOZ_CRASH("No data expected from external protocol channel");
  return NS_ERROR_UNEXPECTED;
}

///////////////////////////////////////////////////////////////////////
// the default protocol handler implementation
//////////////////////////////////////////////////////////////////////

nsExternalProtocolHandler::nsExternalProtocolHandler() {
  m_schemeName = "default";
}

nsExternalProtocolHandler::~nsExternalProtocolHandler() {}

NS_IMPL_ADDREF(nsExternalProtocolHandler)
NS_IMPL_RELEASE(nsExternalProtocolHandler)

NS_INTERFACE_MAP_BEGIN(nsExternalProtocolHandler)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIProtocolHandler)
  NS_INTERFACE_MAP_ENTRY(nsIProtocolHandler)
  NS_INTERFACE_MAP_ENTRY(nsIExternalProtocolHandler)
  NS_INTERFACE_MAP_ENTRY(nsISupportsWeakReference)
NS_INTERFACE_MAP_END

NS_IMETHODIMP nsExternalProtocolHandler::GetScheme(nsACString& aScheme) {
  aScheme = m_schemeName;
  return NS_OK;
}

NS_IMETHODIMP
nsExternalProtocolHandler::AllowPort(int32_t port, const char* scheme,
                                     bool* _retval) {
  // don't override anything.
  *_retval = false;
  return NS_OK;
}

NS_IMETHODIMP
nsExternalProtocolHandler::NewChannel(nsIURI* aURI, nsILoadInfo* aLoadInfo,
                                      nsIChannel** aRetval) {
  NS_ENSURE_TRUE(aURI, NS_ERROR_UNKNOWN_PROTOCOL);
  NS_ENSURE_TRUE(aRetval, NS_ERROR_UNKNOWN_PROTOCOL);

  nsCOMPtr<nsIChannel> channel = new nsExtProtocolChannel(aURI, aLoadInfo);
  channel.forget(aRetval);
  return NS_OK;
}

///////////////////////////////////////////////////////////////////////
// External protocol handler interface implementation
//////////////////////////////////////////////////////////////////////
NS_IMETHODIMP nsExternalProtocolHandler::ExternalAppExistsForScheme(
    const nsACString& aScheme, bool* _retval) {
  nsCOMPtr<nsIExternalProtocolService> extProtSvc(
      do_GetService(NS_EXTERNALPROTOCOLSERVICE_CONTRACTID));
  if (extProtSvc)
    return extProtSvc->ExternalProtocolHandlerExists(
        PromiseFlatCString(aScheme).get(), _retval);

  // In case we don't have external protocol service.
  *_retval = false;
  return NS_OK;
}
