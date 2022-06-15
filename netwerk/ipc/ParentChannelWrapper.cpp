/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set sw=2 ts=8 et tw=80 : */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ParentChannelWrapper.h"
#include "mozilla/net/HttpBaseChannel.h"
#include "mozilla/net/UrlClassifierCommon.h"
#include "mozilla/net/RedirectChannelRegistrar.h"
#include "nsIViewSourceChannel.h"
#include "nsNetUtil.h"
#include "nsQueryObject.h"
#include "mozilla/dom/RemoteType.h"

namespace mozilla {
namespace net {

NS_IMPL_ISUPPORTS(ParentChannelWrapper, nsIParentChannel, nsIStreamListener,
                  nsIRequestObserver);

void ParentChannelWrapper::Register(uint64_t aRegistrarId) {
  nsCOMPtr<nsIRedirectChannelRegistrar> registrar =
      RedirectChannelRegistrar::GetOrCreate();
  nsCOMPtr<nsIChannel> dummy;
  MOZ_ALWAYS_SUCCEEDS(
      NS_LinkRedirectChannels(aRegistrarId, this, getter_AddRefs(dummy)));

#ifdef DEBUG
  // The channel registered with the RedirectChannelRegistrar will be the inner
  // channel when dealing with view-source loads.
  if (nsCOMPtr<nsIViewSourceChannel> viewSource = do_QueryInterface(mChannel)) {
    MOZ_ASSERT(dummy == viewSource->GetInnerChannel());
  } else {
    MOZ_ASSERT(dummy == mChannel);
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
// nsIParentChannel
////////////////////////////////////////////////////////////////////////////////

NS_IMETHODIMP
ParentChannelWrapper::SetParentListener(
    mozilla::net::ParentChannelListener* listener) {
  return NS_OK;
}

NS_IMETHODIMP
ParentChannelWrapper::SetClassifierMatchedInfo(const nsACString& aList,
                                               const nsACString& aProvider,
                                               const nsACString& aFullHash) {
  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(mChannel);
  if (classifiedChannel) {
    classifiedChannel->SetMatchedInfo(aList, aProvider, aFullHash);
  }
  return NS_OK;
}

NS_IMETHODIMP
ParentChannelWrapper::SetClassifierMatchedTrackingInfo(
    const nsACString& aLists, const nsACString& aFullHash) {
  nsCOMPtr<nsIClassifiedChannel> classifiedChannel =
      do_QueryInterface(mChannel);
  if (classifiedChannel) {
    nsTArray<nsCString> lists, fullhashes;
    for (const nsACString& token : aLists.Split(',')) {
      lists.AppendElement(token);
    }
    for (const nsACString& token : aFullHash.Split(',')) {
      fullhashes.AppendElement(token);
    }
    classifiedChannel->SetMatchedTrackingInfo(lists, fullhashes);
  }
  return NS_OK;
}

NS_IMETHODIMP
ParentChannelWrapper::NotifyClassificationFlags(uint32_t aClassificationFlags,
                                                bool aIsThirdParty) {
  UrlClassifierCommon::SetClassificationFlagsHelper(
      mChannel, aClassificationFlags, aIsThirdParty);
  return NS_OK;
}

NS_IMETHODIMP
ParentChannelWrapper::Delete() { return NS_OK; }

NS_IMETHODIMP
ParentChannelWrapper::GetRemoteType(nsACString& aRemoteType) {
  aRemoteType = NOT_REMOTE_TYPE;
  return NS_OK;
}

}  // namespace net
}  // namespace mozilla

#undef LOG
