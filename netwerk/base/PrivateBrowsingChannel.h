/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=4 sts=2 sw=2 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_PrivateBrowsingChannel_h__
#define mozilla_net_PrivateBrowsingChannel_h__

#include "nsIPrivateBrowsingChannel.h"
#include "nsCOMPtr.h"
#include "nsILoadGroup.h"
#include "nsILoadContext.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIInterfaceRequestor.h"
#include "nsNetUtil.h"
#include "mozilla/Unused.h"

namespace mozilla {
namespace net {

template <class Channel>
class PrivateBrowsingChannel : public nsIPrivateBrowsingChannel {
 public:
  PrivateBrowsingChannel()
      : mPrivateBrowsingOverriden(false), mPrivateBrowsing(false) {}

  NS_IMETHOD SetPrivate(bool aPrivate) override {
    // Make sure that we don't have a load context
    // This is a fatal error in debug builds, and a runtime error in release
    // builds.
    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(static_cast<Channel*>(this), loadContext);
    MOZ_ASSERT(!loadContext);
    if (loadContext) {
      return NS_ERROR_FAILURE;
    }

    mPrivateBrowsingOverriden = true;
    mPrivateBrowsing = aPrivate;
    return NS_OK;
  }

  NS_IMETHOD GetIsChannelPrivate(bool* aResult) override {
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = mPrivateBrowsing;
    return NS_OK;
  }

  NS_IMETHOD IsPrivateModeOverriden(bool* aValue, bool* aResult) override {
    NS_ENSURE_ARG_POINTER(aValue);
    NS_ENSURE_ARG_POINTER(aResult);
    *aResult = mPrivateBrowsingOverriden;
    if (mPrivateBrowsingOverriden) {
      *aValue = mPrivateBrowsing;
    }
    return NS_OK;
  }

  // Must be called every time the channel's callbacks or loadGroup is updated
  void UpdatePrivateBrowsing() {
    // once marked as private we never go un-private
    if (mPrivateBrowsing) {
      return;
    }

    auto channel = static_cast<Channel*>(this);

    nsCOMPtr<nsILoadContext> loadContext;
    NS_QueryNotificationCallbacks(channel, loadContext);
    if (loadContext) {
      mPrivateBrowsing = loadContext->UsePrivateBrowsing();
      return;
    }

    nsCOMPtr<nsILoadInfo> loadInfo = channel->LoadInfo();
    OriginAttributes attrs = loadInfo->GetOriginAttributes();
    mPrivateBrowsing = attrs.mPrivateBrowsingId > 0;
  }

  bool CanSetCallbacks(nsIInterfaceRequestor* aCallbacks) const {
    // Make sure that the private bit override flag is not set.
    // This is a fatal error in debug builds, and a runtime error in release
    // builds.
    if (!aCallbacks) {
      return true;
    }
    nsCOMPtr<nsILoadContext> loadContext = do_GetInterface(aCallbacks);
    if (!loadContext) {
      return true;
    }
    MOZ_ASSERT(!mPrivateBrowsingOverriden);
    return !mPrivateBrowsingOverriden;
  }

  bool CanSetLoadGroup(nsILoadGroup* aLoadGroup) const {
    // Make sure that the private bit override flag is not set.
    // This is a fatal error in debug builds, and a runtime error in release
    // builds.
    if (!aLoadGroup) {
      return true;
    }
    nsCOMPtr<nsIInterfaceRequestor> callbacks;
    aLoadGroup->GetNotificationCallbacks(getter_AddRefs(callbacks));
    // From this point on, we just hand off the work to CanSetCallbacks,
    // because the logic is exactly the same.
    return CanSetCallbacks(callbacks);
  }

 protected:
  bool mPrivateBrowsingOverriden;
  bool mPrivateBrowsing;
};

}  // namespace net
}  // namespace mozilla

#endif
