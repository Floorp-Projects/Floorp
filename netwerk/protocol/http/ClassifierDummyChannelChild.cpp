/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ClassifierDummyChannelChild.h"
#include "mozilla/ContentBlocking.h"
#include "mozilla/ipc/BackgroundUtils.h"
#include "mozilla/ipc/URIUtils.h"
#include "nsIURI.h"
#include "mozilla/AntiTrackingUtils.h"

namespace mozilla {
namespace net {

/* static */
bool ClassifierDummyChannelChild::Create(
    nsIHttpChannel* aChannel, nsIURI* aURI,
    const std::function<void(bool)>& aCallback) {
  MOZ_ASSERT(NS_IsMainThread());

  MOZ_ASSERT(aChannel);
  MOZ_ASSERT(aURI);

  nsCOMPtr<nsIHttpChannelInternal> httpChannelInternal =
      do_QueryInterface(aChannel);
  if (!httpChannelInternal) {
    // Any non-http channel is allowed.
    return true;
  }

  nsCOMPtr<nsIURI> topWindowURI;
  nsresult topWindowURIResult =
      httpChannelInternal->GetTopWindowURI(getter_AddRefs(topWindowURI));

  nsCOMPtr<nsILoadInfo> loadInfo = aChannel->LoadInfo();
  Maybe<LoadInfoArgs> loadInfoArgs;
  mozilla::ipc::LoadInfoToLoadInfoArgs(loadInfo, &loadInfoArgs);

  PClassifierDummyChannelChild* actor =
      gNeckoChild->SendPClassifierDummyChannelConstructor(
          aURI, topWindowURI, topWindowURIResult, loadInfoArgs);
  if (!actor) {
    return false;
  }

  bool isThirdParty = AntiTrackingUtils::IsThirdPartyChannel(aChannel);

  static_cast<ClassifierDummyChannelChild*>(actor)->Initialize(
      aChannel, aURI, isThirdParty, aCallback);
  return true;
}

void ClassifierDummyChannelChild::Initialize(
    nsIHttpChannel* aChannel, nsIURI* aURI, bool aIsThirdParty,
    const std::function<void(bool)>& aCallback) {
  MOZ_ASSERT(NS_IsMainThread());

  mChannel = aChannel;
  mURI = aURI;
  mIsThirdParty = aIsThirdParty;
  mCallback = aCallback;
}

mozilla::ipc::IPCResult ClassifierDummyChannelChild::Recv__delete__(
    const uint32_t& aClassificationFlags) {
  MOZ_ASSERT(NS_IsMainThread());

  if (!mChannel) {
    return IPC_OK();
  }

  nsCOMPtr<nsIHttpChannel> channel = std::move(mChannel);

  RefPtr<HttpBaseChannel> httpChannel = do_QueryObject(channel);
  httpChannel->AddClassificationFlags(aClassificationFlags, mIsThirdParty);

  bool storageGranted =
      ContentBlocking::ShouldAllowAccessFor(httpChannel, mURI, nullptr);
  mCallback(storageGranted);
  return IPC_OK();
}

}  // namespace net
}  // namespace mozilla
