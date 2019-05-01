/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExternalHelperAppChild.h"
#include "mozilla/net/ChannelDiverterChild.h"
#include "mozilla/dom/BrowserChild.h"
#include "nsIDivertableChannel.h"
#include "nsIInputStream.h"
#include "nsIFTPChannel.h"
#include "nsIRequest.h"
#include "nsIResumableChannel.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS(ExternalHelperAppChild, nsIStreamListener, nsIRequestObserver)

ExternalHelperAppChild::ExternalHelperAppChild() : mStatus(NS_OK) {}

ExternalHelperAppChild::~ExternalHelperAppChild() {}

//-----------------------------------------------------------------------------
// nsIStreamListener
//-----------------------------------------------------------------------------
NS_IMETHODIMP
ExternalHelperAppChild::OnDataAvailable(nsIRequest* request,
                                        nsIInputStream* input, uint64_t offset,
                                        uint32_t count) {
  if (NS_FAILED(mStatus)) return mStatus;

  static uint32_t const kCopyChunkSize = 128 * 1024;
  uint32_t toRead = std::min<uint32_t>(count, kCopyChunkSize);

  nsCString data;

  while (count) {
    nsresult rv = NS_ReadInputStreamToString(input, data, toRead);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      return rv;
    }

    if (NS_WARN_IF(!SendOnDataAvailable(data, offset, toRead))) {
      return NS_ERROR_UNEXPECTED;
    }

    count -= toRead;
    offset += toRead;
    toRead = std::min<uint32_t>(count, kCopyChunkSize);
  }

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
// nsIRequestObserver
//////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
ExternalHelperAppChild::OnStartRequest(nsIRequest* request) {
  nsresult rv = mHandler->OnStartRequest(request);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

  // Calling OnStartRequest could cause mHandler to close the window it was
  // loaded for. In that case, the BrowserParent in the parent context might
  // then point to the wrong window. Re-send the window context along with
  // either DivertToParent or SendOnStartRequest just in case.
  nsCOMPtr<nsPIDOMWindowOuter> window =
      do_GetInterface(mHandler->GetDialogParent());
  NS_ENSURE_TRUE(window, NS_ERROR_NOT_AVAILABLE);

  BrowserChild* browserChild = mozilla::dom::BrowserChild::GetFrom(window);
  NS_ENSURE_TRUE(browserChild, NS_ERROR_NOT_AVAILABLE);

  nsCOMPtr<nsIDivertableChannel> divertable = do_QueryInterface(request);
  if (divertable) {
    return DivertToParent(divertable, request, browserChild);
  }

  nsCString entityID;
  nsCOMPtr<nsIResumableChannel> resumable(do_QueryInterface(request));
  if (resumable) {
    resumable->GetEntityID(entityID);
  }
  SendOnStartRequest(entityID, browserChild);
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppChild::OnStopRequest(nsIRequest* request, nsresult status) {
  // mHandler can be null if we diverted the request to the parent
  if (mHandler) {
    nsresult rv = mHandler->OnStopRequest(request, status);
    SendOnStopRequest(status);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);
  }

  return NS_OK;
}

nsresult ExternalHelperAppChild::DivertToParent(
    nsIDivertableChannel* divertable, nsIRequest* request,
    BrowserChild* browserChild) {
  // nsIDivertable must know about content conversions before being diverted.
  MOZ_ASSERT(mHandler);
  mHandler->MaybeApplyDecodingForExtension(request);

  mozilla::net::ChannelDiverterChild* diverter = nullptr;
  nsresult rv = divertable->DivertToParent(&diverter);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }
  MOZ_ASSERT(diverter);

  if (SendDivertToParentUsing(diverter, browserChild)) {
    mHandler->DidDivertRequest(request);
    mHandler = nullptr;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

mozilla::ipc::IPCResult ExternalHelperAppChild::RecvCancel(
    const nsresult& aStatus) {
  mStatus = aStatus;
  return IPC_OK();
}

}  // namespace dom
}  // namespace mozilla
