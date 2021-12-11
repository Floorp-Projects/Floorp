/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=4 sw=2 sts=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "SimpleChannelParent.h"
#include "mozilla/Assertions.h"
#include "nsNetUtil.h"
#include "nsIChannel.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(SimpleChannelParent, nsIParentChannel, nsIStreamListener)

bool SimpleChannelParent::Init(const uint64_t& aChannelId) {
  nsCOMPtr<nsIChannel> channel;
  MOZ_ALWAYS_SUCCEEDS(
      NS_LinkRedirectChannels(aChannelId, this, getter_AddRefs(channel)));

  return true;
}

NS_IMETHODIMP
SimpleChannelParent::SetParentListener(ParentChannelListener* aListener) {
  // Nothing to do.
  return NS_OK;
}

NS_IMETHODIMP
SimpleChannelParent::NotifyClassificationFlags(uint32_t aClassificationFlags,
                                               bool aIsThirdParty) {
  // Nothing to do.
  return NS_OK;
}

NS_IMETHODIMP
SimpleChannelParent::NotifyFlashPluginStateChanged(
    nsIHttpChannel::FlashPluginState aState) {
  // Nothing to do.
  return NS_OK;
}

NS_IMETHODIMP
SimpleChannelParent::SetClassifierMatchedInfo(const nsACString& aList,
                                              const nsACString& aProvider,
                                              const nsACString& aPrefix) {
  // nothing to do
  return NS_OK;
}

NS_IMETHODIMP
SimpleChannelParent::SetClassifierMatchedTrackingInfo(
    const nsACString& aLists, const nsACString& aFullHashes) {
  // nothing to do
  return NS_OK;
}

NS_IMETHODIMP
SimpleChannelParent::Delete() {
  // Nothing to do.
  return NS_OK;
}

NS_IMETHODIMP
SimpleChannelParent::GetRemoteType(nsACString& aRemoteType) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

void SimpleChannelParent::ActorDestroy(ActorDestroyReason aWhy) {}

NS_IMETHODIMP
SimpleChannelParent::OnStartRequest(nsIRequest* aRequest) {
  // We don't have a way to prevent nsBaseChannel from calling AsyncOpen on
  // the created nsSimpleChannel. We don't have anywhere to send the data in the
  // parent, so abort the binding.
  return NS_BINDING_ABORTED;
}

NS_IMETHODIMP
SimpleChannelParent::OnStopRequest(nsIRequest* aRequest, nsresult aStatusCode) {
  // See above.
  MOZ_ASSERT(NS_FAILED(aStatusCode));
  return NS_OK;
}

NS_IMETHODIMP
SimpleChannelParent::OnDataAvailable(nsIRequest* aRequest,
                                     nsIInputStream* aInputStream,
                                     uint64_t aOffset, uint32_t aCount) {
  // See above.
  MOZ_CRASH("Should never be called");
}

}  // namespace net
}  // namespace mozilla
