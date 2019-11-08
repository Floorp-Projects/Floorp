/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "ExternalHelperAppParent.h"
#include "nsIContent.h"
#include "nsCExternalHandlerService.h"
#include "nsIExternalHelperAppService.h"
#include "mozilla/dom/ContentParent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/BrowserParent.h"
#include "nsIBrowserDOMWindow.h"
#include "nsStringStream.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsNetUtil.h"
#include "mozilla/dom/Document.h"
#include "mozilla/net/ChannelDiverterParent.h"

#include "mozilla/Unused.h"

using namespace mozilla::ipc;

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS_INHERITED(ExternalHelperAppParent, nsHashPropertyBag,
                            nsIRequest, nsIChannel, nsIMultiPartChannel,
                            nsIPrivateBrowsingChannel, nsIResumableChannel,
                            nsIStreamListener, nsIExternalHelperAppParent)

ExternalHelperAppParent::ExternalHelperAppParent(
    const Maybe<URIParams>& uri, const int64_t& aContentLength,
    const bool& aWasFileChannel, const nsCString& aContentDispositionHeader,
    const uint32_t& aContentDispositionHint,
    const nsString& aContentDispositionFilename)
    : mURI(DeserializeURI(uri)),
      mPending(false)
#ifdef DEBUG
      ,
      mDiverted(false)
#endif
      ,
      mIPCClosed(false),
      mLoadFlags(0),
      mStatus(NS_OK),
      mContentLength(aContentLength),
      mWasFileChannel(aWasFileChannel) {
  mContentDispositionHeader = aContentDispositionHeader;
  if (!mContentDispositionHeader.IsEmpty()) {
    NS_GetFilenameFromDisposition(mContentDispositionFilename,
                                  mContentDispositionHeader, mURI);
    mContentDisposition =
        NS_GetContentDispositionFromHeader(mContentDispositionHeader, this);
  } else {
    mContentDisposition = aContentDispositionHint;
    mContentDispositionFilename = aContentDispositionFilename;
  }
}

already_AddRefed<nsIInterfaceRequestor> GetWindowFromBrowserParent(
    PBrowserParent* aBrowser) {
  if (!aBrowser) {
    return nullptr;
  }

  nsCOMPtr<nsIInterfaceRequestor> window;
  BrowserParent* browserParent = BrowserParent::GetFrom(aBrowser);
  if (browserParent->GetOwnerElement()) {
    window = do_QueryInterface(
        browserParent->GetOwnerElement()->OwnerDoc()->GetWindow());
  }

  return window.forget();
}

void UpdateContentContext(nsIStreamListener* aListener,
                          PBrowserParent* aBrowser) {
  MOZ_ASSERT(aListener);
  nsCOMPtr<nsIInterfaceRequestor> window = GetWindowFromBrowserParent(aBrowser);
  static_cast<nsExternalAppHandler*>(aListener)->SetContentContext(window);
}

void ExternalHelperAppParent::Init(
    const Maybe<mozilla::net::LoadInfoArgs>& aLoadInfoArgs,
    const nsCString& aMimeContentType, const bool& aForceSave,
    const Maybe<URIParams>& aReferrer, PBrowserParent* aBrowser) {
  mozilla::ipc::LoadInfoArgsToLoadInfo(aLoadInfoArgs,
                                       getter_AddRefs(mLoadInfo));

  nsCOMPtr<nsIExternalHelperAppService> helperAppService =
      do_GetService(NS_EXTERNALHELPERAPPSERVICE_CONTRACTID);
  NS_ASSERTION(helperAppService, "No Helper App Service!");

  nsCOMPtr<nsIURI> referrer = DeserializeURI(aReferrer);
  if (referrer)
    SetPropertyAsInterface(NS_LITERAL_STRING("docshell.internalReferrer"),
                           referrer);

  nsCOMPtr<nsIInterfaceRequestor> window;
  if (aBrowser) {
    BrowserParent* browserParent = BrowserParent::GetFrom(aBrowser);
    if (browserParent->GetOwnerElement())
      window = do_QueryInterface(
          browserParent->GetOwnerElement()->OwnerDoc()->GetWindow());

    bool isPrivate = false;
    nsCOMPtr<nsILoadContext> loadContext = browserParent->GetLoadContext();
    loadContext->GetUsePrivateBrowsing(&isPrivate);
    SetPrivate(isPrivate);
  }

  helperAppService->DoContent(aMimeContentType, this, window, aForceSave,
                              nullptr, getter_AddRefs(mListener));
}

void ExternalHelperAppParent::ActorDestroy(ActorDestroyReason why) {
  mIPCClosed = true;
}

void ExternalHelperAppParent::Delete() {
  if (!mIPCClosed) {
    Unused << Send__delete__(this);
  }
}

mozilla::ipc::IPCResult ExternalHelperAppParent::RecvOnStartRequest(
    const nsCString& entityID, PBrowserParent* contentContext) {
  MOZ_ASSERT(!mDiverted,
             "child forwarding callbacks after request was diverted");

  UpdateContentContext(mListener, contentContext);

  mEntityID = entityID;
  mPending = true;
  mStatus = mListener->OnStartRequest(this);
  return IPC_OK();
}

mozilla::ipc::IPCResult ExternalHelperAppParent::RecvOnDataAvailable(
    const nsCString& data, const uint64_t& offset, const uint32_t& count) {
  if (NS_FAILED(mStatus)) return IPC_OK();

  MOZ_ASSERT(!mDiverted,
             "child forwarding callbacks after request was diverted");
  MOZ_ASSERT(mPending, "must be pending!");

  nsCOMPtr<nsIInputStream> stringStream;
  DebugOnly<nsresult> rv =
      NS_NewByteInputStream(getter_AddRefs(stringStream),
                            MakeSpan(data).To(count), NS_ASSIGNMENT_DEPEND);
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to create dependent string!");
  mStatus = mListener->OnDataAvailable(this, stringStream, offset, count);

  return IPC_OK();
}

mozilla::ipc::IPCResult ExternalHelperAppParent::RecvOnStopRequest(
    const nsresult& code) {
  MOZ_ASSERT(!mDiverted,
             "child forwarding callbacks after request was diverted");

  mPending = false;
  mListener->OnStopRequest(
      this, (NS_SUCCEEDED(code) && NS_FAILED(mStatus)) ? mStatus : code);
  Delete();
  return IPC_OK();
}

mozilla::ipc::IPCResult ExternalHelperAppParent::RecvDivertToParentUsing(
    PChannelDiverterParent* diverter, PBrowserParent* contentContext) {
  MOZ_ASSERT(diverter);
  UpdateContentContext(mListener, contentContext);
  auto p = static_cast<mozilla::net::ChannelDiverterParent*>(diverter);
  p->DivertTo(this);
#ifdef DEBUG
  mDiverted = true;
#endif
  Unused << mozilla::net::ChannelDiverterParent::Send__delete__(p);
  return IPC_OK();
}

//
// nsIStreamListener
//

NS_IMETHODIMP
ExternalHelperAppParent::OnDataAvailable(nsIRequest* request,
                                         nsIInputStream* input, uint64_t offset,
                                         uint32_t count) {
  MOZ_ASSERT(mDiverted);
  return mListener->OnDataAvailable(request, input, offset, count);
}

NS_IMETHODIMP
ExternalHelperAppParent::OnStartRequest(nsIRequest* request) {
  MOZ_ASSERT(mDiverted);
  return mListener->OnStartRequest(request);
}

NS_IMETHODIMP
ExternalHelperAppParent::OnStopRequest(nsIRequest* request, nsresult status) {
  MOZ_ASSERT(mDiverted);
  nsresult rv = mListener->OnStopRequest(request, status);
  Delete();
  return rv;
}

ExternalHelperAppParent::~ExternalHelperAppParent() {}

//
// nsIRequest implementation...
//

NS_IMETHODIMP
ExternalHelperAppParent::GetName(nsACString& aResult) {
  if (!mURI) {
    aResult.Truncate();
    return NS_ERROR_NOT_AVAILABLE;
  }
  mURI->GetAsciiSpec(aResult);
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::IsPending(bool* aResult) {
  *aResult = mPending;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetStatus(nsresult* aResult) {
  *aResult = mStatus;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::Cancel(nsresult aStatus) {
  mStatus = aStatus;
  Unused << SendCancel(aStatus);
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::Suspend() { return NS_ERROR_NOT_IMPLEMENTED; }

NS_IMETHODIMP
ExternalHelperAppParent::Resume() { return NS_ERROR_NOT_IMPLEMENTED; }

//
// nsIChannel implementation
//

NS_IMETHODIMP
ExternalHelperAppParent::GetOriginalURI(nsIURI** aURI) {
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetOriginalURI(nsIURI* aURI) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetURI(nsIURI** aURI) {
  NS_IF_ADDREF(*aURI = mURI);
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::Open(nsIInputStream** aResult) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::AsyncOpen(nsIStreamListener* aListener) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetLoadFlags(nsLoadFlags* aLoadFlags) {
  *aLoadFlags = mLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetLoadFlags(nsLoadFlags aLoadFlags) {
  mLoadFlags = aLoadFlags;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetIsDocument(bool* aIsDocument) {
  return NS_GetIsDocumentChannel(this, aIsDocument);
}

NS_IMETHODIMP
ExternalHelperAppParent::GetLoadGroup(nsILoadGroup** aLoadGroup) {
  *aLoadGroup = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetLoadGroup(nsILoadGroup* aLoadGroup) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetOwner(nsISupports** aOwner) {
  *aOwner = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetOwner(nsISupports* aOwner) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetLoadInfo(nsILoadInfo** aLoadInfo) {
  NS_IF_ADDREF(*aLoadInfo = mLoadInfo);
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetLoadInfo(nsILoadInfo* aLoadInfo) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetNotificationCallbacks(
    nsIInterfaceRequestor** aCallbacks) {
  *aCallbacks = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetNotificationCallbacks(
    nsIInterfaceRequestor* aCallbacks) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetSecurityInfo(nsISupports** aSecurityInfo) {
  *aSecurityInfo = nullptr;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetContentType(nsACString& aContentType) {
  aContentType.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetContentType(const nsACString& aContentType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetContentCharset(nsACString& aContentCharset) {
  aContentCharset.Truncate();
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetContentCharset(const nsACString& aContentCharset) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetContentDisposition(uint32_t* aContentDisposition) {
  // NB: mContentDisposition may or may not be set to a non UINT32_MAX value in
  // nsExternalHelperAppService::DoContentContentProcessHelper
  if (mContentDispositionHeader.IsEmpty() && mContentDisposition == UINT32_MAX)
    return NS_ERROR_NOT_AVAILABLE;

  *aContentDisposition = mContentDisposition;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetContentDisposition(uint32_t aContentDisposition) {
  mContentDisposition = aContentDisposition;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetContentDispositionFilename(
    nsAString& aContentDispositionFilename) {
  if (mContentDispositionFilename.IsEmpty()) return NS_ERROR_NOT_AVAILABLE;

  aContentDispositionFilename = mContentDispositionFilename;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetContentDispositionFilename(
    const nsAString& aContentDispositionFilename) {
  mContentDispositionFilename = aContentDispositionFilename;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetContentDispositionHeader(
    nsACString& aContentDispositionHeader) {
  if (mContentDispositionHeader.IsEmpty()) return NS_ERROR_NOT_AVAILABLE;

  aContentDispositionHeader = mContentDispositionHeader;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetContentLength(int64_t* aContentLength) {
  if (mContentLength < 0)
    *aContentLength = -1;
  else
    *aContentLength = mContentLength;
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppParent::SetContentLength(int64_t aContentLength) {
  mContentLength = aContentLength;
  return NS_OK;
}

//
// nsIResumableChannel implementation
//

NS_IMETHODIMP
ExternalHelperAppParent::ResumeAt(uint64_t startPos,
                                  const nsACString& entityID) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetEntityID(nsACString& aEntityID) {
  aEntityID = mEntityID;
  return NS_OK;
}

//
// nsIMultiPartChannel implementation
//

NS_IMETHODIMP
ExternalHelperAppParent::GetBaseChannel(nsIChannel** aChannel) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetPartID(uint32_t* aPartID) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
ExternalHelperAppParent::GetIsLastPart(bool* aIsLastPart) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

}  // namespace dom
}  // namespace mozilla
