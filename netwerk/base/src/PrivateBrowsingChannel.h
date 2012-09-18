/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim:set ts=4 sts=4 sw=4 et cin: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_net_PrivateBrowsingChannel_h__
#define mozilla_net_PrivateBrowsingChannel_h__

#include "nsIPrivateBrowsingChannel.h"
#include "nsCOMPtr.h"
#include "nsILoadGroup.h"
#include "nsILoadContext.h"

namespace mozilla {
namespace net {

template <class Channel>
class PrivateBrowsingChannel : public nsIPrivateBrowsingChannel
{
public:
  PrivateBrowsingChannel() :
    mPrivateBrowsingOverriden(false),
    mPrivateBrowsing(false)
  {
  }

  NS_IMETHOD SetPrivate(bool aPrivate)
  {
      // Make sure that we don't have a load group or a load context
      // This is a fatal error in debug builds, and a runtime error in release
      // builds.
      nsILoadGroup* loadGroup = static_cast<Channel*>(this)->mLoadGroup;
      nsCOMPtr<nsILoadContext> loadContext;
      if (!loadGroup) {
        NS_QueryNotificationCallbacks(static_cast<Channel*>(this), loadContext);
      }
      MOZ_ASSERT(!loadGroup && !loadContext);
      if (loadGroup || loadContext) {
        return NS_ERROR_FAILURE;
      }

      mPrivateBrowsingOverriden = true;
      mPrivateBrowsing = aPrivate;
      return NS_OK;
  }

  NS_IMETHOD IsPrivateModeOverriden(bool* aValue, bool *aResult)
  {
      NS_ENSURE_ARG_POINTER(aValue);
      NS_ENSURE_ARG_POINTER(aResult);
      *aResult = mPrivateBrowsingOverriden;
      if (mPrivateBrowsingOverriden) {
          *aValue = mPrivateBrowsing;
      }
      return NS_OK;
  }

  bool CanSetCallbacks() const
  {
      // Make sure that the private bit override flag is not set.
      // This is a fatal error in debug builds, and a runtime error in release
      // builds.
      MOZ_ASSERT(!mPrivateBrowsingOverriden);
      return !mPrivateBrowsingOverriden;
  }

  bool CanSetLoadGroup() const
  {
      // We can set a load group whenever we can set a callback
      return CanSetCallbacks();
  }

protected:
  bool mPrivateBrowsingOverriden;
  bool mPrivateBrowsing;
};

}
}

#endif

