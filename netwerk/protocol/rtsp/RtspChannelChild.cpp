/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "RtspChannelChild.h"
#include "nsIURI.h"
#include "nsAutoPtr.h"
#include "nsStandardURL.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS_INHERITED1(RtspChannelChild,
                             nsBaseChannel,
                             nsIChannel)

//-----------------------------------------------------------------------------
// RtspChannelChild::nsIChannel
//-----------------------------------------------------------------------------

NS_IMETHODIMP
RtspChannelChild::AsyncOpen(nsIStreamListener *aListener, nsISupports *aContext)
{
  MOZ_ASSERT(aListener);

  nsCOMPtr<nsIURI> uri = nsBaseChannel::URI();
  NS_ENSURE_TRUE(uri, NS_ERROR_ILLEGAL_VALUE);

  nsAutoCString uriSpec;
  uri->GetSpec(uriSpec);

  mListener = aListener;
  mListenerContext = aContext;

  // Call OnStartRequest directly. mListener is expected to create/load an
  // RtspMediaResource which will create an RtspMediaController. This controller
  // manages the control and data streams to and from the network.
  mListener->OnStartRequest(this, aContext);
  return NS_OK;
}

NS_IMETHODIMP
RtspChannelChild::GetContentType(nsACString& aContentType)
{
  aContentType.AssignLiteral("RTSP");
  return NS_OK;
}

NS_IMETHODIMP
RtspChannelChild::Init(nsIURI* aUri)
{
  MOZ_ASSERT(aUri);

  nsBaseChannel::Init();
  nsBaseChannel::SetURI(aUri);
  return NS_OK;
}

} // namespace mozilla::net
} // namespace mozilla
