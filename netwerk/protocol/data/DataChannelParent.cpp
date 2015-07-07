/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set ts=4 sw=4 sts=4 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DataChannelParent.h"
#include "mozilla/Assertions.h"
#include "nsNetUtil.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(DataChannelParent, nsIParentChannel, nsIStreamListener)

DataChannelParent::~DataChannelParent()
{
}

bool
DataChannelParent::Init(const uint32_t &channelId)
{
    nsCOMPtr<nsIChannel> channel;
    MOZ_ALWAYS_TRUE(NS_SUCCEEDED(
        NS_LinkRedirectChannels(channelId, this, getter_AddRefs(channel))));

    return true;
}

NS_IMETHODIMP
DataChannelParent::SetParentListener(HttpChannelParentListener* aListener)
{
    // Nothing to do.
    return NS_OK;
}

NS_IMETHODIMP
DataChannelParent::NotifyTrackingProtectionDisabled()
{
    // Nothing to do.
    return NS_OK;
}

NS_IMETHODIMP
DataChannelParent::Delete()
{
    // Nothing to do.
    return NS_OK;
}

void
DataChannelParent::ActorDestroy(ActorDestroyReason why)
{
}

NS_IMETHODIMP
DataChannelParent::OnStartRequest(nsIRequest *aRequest,
                                  nsISupports *aContext)
{
    // We don't have a way to prevent nsBaseChannel from calling AsyncOpen on
    // the created nsDataChannel. We don't have anywhere to send the data in the
    // parent, so abort the binding.
    return NS_BINDING_ABORTED;
}

NS_IMETHODIMP
DataChannelParent::OnStopRequest(nsIRequest *aRequest,
                                 nsISupports *aContext,
                                 nsresult aStatusCode)
{
    // See above.
    MOZ_ASSERT(NS_FAILED(aStatusCode));
    return NS_OK;
}

NS_IMETHODIMP
DataChannelParent::OnDataAvailable(nsIRequest *aRequest,
                                   nsISupports *aContext,
                                   nsIInputStream *aInputStream,
                                   uint64_t aOffset,
                                   uint32_t aCount)
{
    // See above.
    MOZ_CRASH("Should never be called");
}

} // namespace mozilla
} // namespace net
