/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ExternalHelperAppChild.h"
#include "mozilla/net/ChannelDiverterChild.h"
#include "nsIDivertableChannel.h"
#include "nsIInputStream.h"
#include "nsIFTPChannel.h"
#include "nsIRequest.h"
#include "nsIResumableChannel.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace dom {

NS_IMPL_ISUPPORTS2(ExternalHelperAppChild,
                   nsIStreamListener,
                   nsIRequestObserver)

ExternalHelperAppChild::ExternalHelperAppChild()
  : mStatus(NS_OK)
{
}

ExternalHelperAppChild::~ExternalHelperAppChild()
{
}

//-----------------------------------------------------------------------------
// nsIStreamListener
//-----------------------------------------------------------------------------
NS_IMETHODIMP
ExternalHelperAppChild::OnDataAvailable(nsIRequest *request,
                                        nsISupports *ctx,
                                        nsIInputStream *input,
                                        uint64_t offset,
                                        uint32_t count)
{
  if (NS_FAILED(mStatus))
    return mStatus;

  nsCString data;
  nsresult rv = NS_ReadInputStreamToString(input, data, count);
  if (NS_FAILED(rv))
    return rv;

  if (!SendOnDataAvailable(data, offset, count))
    return NS_ERROR_UNEXPECTED;

  return NS_OK;
}

//////////////////////////////////////////////////////////////////////////////
// nsIRequestObserver
//////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
ExternalHelperAppChild::OnStartRequest(nsIRequest *request, nsISupports *ctx)
{
  nsCOMPtr<nsIDivertableChannel> divertable = do_QueryInterface(request);
  if (divertable) {
    return DivertToParent(divertable, request);
  }

  nsresult rv = mHandler->OnStartRequest(request, ctx);
  NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);

  nsCString entityID;
  nsCOMPtr<nsIResumableChannel> resumable(do_QueryInterface(request));
  if (resumable) {
    resumable->GetEntityID(entityID);
  }
  SendOnStartRequest(entityID);
  return NS_OK;
}

NS_IMETHODIMP
ExternalHelperAppChild::OnStopRequest(nsIRequest *request,
                                      nsISupports *ctx,
                                      nsresult status)
{
  // mHandler can be null if we diverted the request to the parent
  if (mHandler) {
    nsresult rv = mHandler->OnStopRequest(request, ctx, status);
    SendOnStopRequest(status);
    NS_ENSURE_SUCCESS(rv, NS_ERROR_UNEXPECTED);
  }

  return NS_OK;
}

nsresult
ExternalHelperAppChild::DivertToParent(nsIDivertableChannel *divertable, nsIRequest *request)
{
  mozilla::net::ChannelDiverterChild *diverter = nullptr;
  nsresult rv = divertable->DivertToParent(&diverter);
  if (NS_WARN_IF(NS_FAILED(rv))) {
    return rv;
  }

  MOZ_ASSERT(diverter);
  if (SendDivertToParentUsing(diverter)) {
    mHandler->DidDivertRequest(request);
    mHandler = nullptr;
    return NS_OK;
  }

  return NS_ERROR_FAILURE;
}

bool
ExternalHelperAppChild::RecvCancel(const nsresult& aStatus)
{
  mStatus = aStatus;
  return true;
}

} // namespace dom
} // namespace mozilla
