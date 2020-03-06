/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_contentblockingallowlistcache_h
#define mozilla_contentblockingallowlistcache_h

#include "mozilla/HashFunctions.h"
#include "mozilla/MruCache.h"
#include "nsPIDOMWindow.h"
#include "nsIHttpChannel.h"

namespace mozilla {

struct ContentBlockingAllowListKey {
  ContentBlockingAllowListKey() : mHash(mozilla::HashGeneric(uintptr_t(0))) {}

  // Ensure that we compute a different hash for window and channel pointers of
  // the same numeric value, in the off chance that we get unlucky and encounter
  // a case where the allocator reallocates a window object where a channel used
  // to live and vice versa.
  explicit ContentBlockingAllowListKey(nsPIDOMWindowInner* aWindow)
      : mHash(mozilla::AddToHash(aWindow->WindowID(),
                                 mozilla::HashString("window"))) {}
  explicit ContentBlockingAllowListKey(nsIHttpChannel* aChannel)
      : mHash(mozilla::AddToHash(aChannel->ChannelId(),
                                 mozilla::HashString("channel"))) {}

  ContentBlockingAllowListKey(const ContentBlockingAllowListKey& aRHS) =
      default;

  bool operator==(const ContentBlockingAllowListKey& aRHS) const {
    return mHash == aRHS.mHash;
  }

  HashNumber GetHash() const { return mHash; }

 private:
  HashNumber mHash;
};

struct ContentBlockingAllowListEntry {
  ContentBlockingAllowListEntry() : mResult(false) {}
  ContentBlockingAllowListEntry(nsPIDOMWindowInner* aWindow, bool aResult)
      : mKey(aWindow), mResult(aResult) {}
  ContentBlockingAllowListEntry(nsIHttpChannel* aChannel, bool aResult)
      : mKey(aChannel), mResult(aResult) {}

  ContentBlockingAllowListKey mKey;
  bool mResult;
};

struct ContentBlockingAllowListCache
    : MruCache<ContentBlockingAllowListKey, ContentBlockingAllowListEntry,
               ContentBlockingAllowListCache> {
  static HashNumber Hash(const ContentBlockingAllowListKey& aKey) {
    return aKey.GetHash();
  }
  static bool Match(const ContentBlockingAllowListKey& aKey,
                    const ContentBlockingAllowListEntry& aValue) {
    return aValue.mKey == aKey;
  }
};

}  // namespace mozilla

#endif  // mozilla_contentblockingallowlistcache_h
