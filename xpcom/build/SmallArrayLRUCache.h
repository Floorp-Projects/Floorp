/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SmallArrayLRUCache_h
#define SmallArrayLRUCache_h

#include "mozilla/Mutex.h"

#include <algorithm>
#include <type_traits>
#include <utility>

// Uncomment the next line to get shutdown stats about cache usage.
// #define SMALLARRAYLRUCACHE_STATS

#ifdef SMALLARRAYLRUCACHE_STATS
#  include <cstdio>
#endif

namespace mozilla {

// Array-based LRU cache, thread-safe.
// Optimized for cases where `FetchOrAdd` is used with the same key most
// recently, and assuming the cost of running the value-builder function is much
// more expensive than going through the whole list.
// Note: No time limits on keeping items.
// TODO: Move to more public place, if this could be used elsewhere; make sure
// the cost/benefits are highlighted.
template <typename Key, typename Value, unsigned LRUCapacity>
class SmallArrayLRUCache {
 public:
  static_assert(std::is_default_constructible_v<Key>);
  static_assert(std::is_trivially_constructible_v<Key>);
  static_assert(std::is_trivially_copyable_v<Key>);
  static_assert(std::is_default_constructible_v<Value>);
  static_assert(LRUCapacity >= 2);
  static_assert(LRUCapacity <= 1024,
                "This seems a bit big, is this the right cache for your use?");

  // Reset all stored values to their default, and set cache size to zero.
  void Clear() {
    mozilla::OffTheBooksMutexAutoLock lock(mMutex);
    if (mSize == ShutdownSize) {
      return;
    }
    Clear(lock);
  }

  // Clear the cache, and then prevent further uses (other functions will do
  // nothing).
  void Shutdown() {
    mozilla::OffTheBooksMutexAutoLock lock(mMutex);
    if (mSize == ShutdownSize) {
      return;
    }
    Clear(lock);
    mSize = ShutdownSize;
  }

  // Add a key-value.
  template <typename ToValue>
  void Add(Key aKey, ToValue&& aValue) {
    mozilla::OffTheBooksMutexAutoLock lock(mMutex);

    if (mSize == ShutdownSize) {
      return;
    }

    // Quick add to the front, don't remove possible duplicate handles later in
    // the list, they will eventually drop off the end.
    KeyAndValue* const item0 = &mLRUArray[0];
    mSize = std::min(mSize + 1, LRUCapacity);
    if (MOZ_LIKELY(mSize != 1)) {
      // List is not empty.
      // Make a hole at the start.
      std::move_backward(item0, item0 + mSize - 1, item0 + mSize);
    }
    item0->mKey = aKey;
    item0->mValue = std::forward<ToValue>(aValue);
    return;
  }

  // Look for the value associated with `aKey` in the cache.
  // If not found, run `aValueFunction()`, add it in the cache before returning.
  // After shutdown, just run `aValueFunction()`.
  template <typename ValueFunction>
  Value FetchOrAdd(Key aKey, ValueFunction&& aValueFunction) {
    Value value;
    mozilla::OffTheBooksMutexAutoLock lock(mMutex);

    if (mSize == ShutdownSize) {
      value = std::forward<ValueFunction>(aValueFunction)();
      return value;
    }

    KeyAndValue* const item0 = &mLRUArray[0];
    if (MOZ_UNLIKELY(mSize == 0)) {
      // List is empty.
      value = std::forward<ValueFunction>(aValueFunction)();
      item0->mKey = aKey;
      item0->mValue = value;
      mSize = 1;
      return value;
    }

    if (MOZ_LIKELY(item0->mKey == aKey)) {
      // This is already at the beginning of the list, we're done.
#ifdef SMALLARRAYLRUCACHE_STATS
      ++mCacheFoundAt[0];
#endif  // SMALLARRAYLRUCACHE_STATS
      value = item0->mValue;
      return value;
    }

    for (KeyAndValue* item = item0 + 1; item != item0 + mSize; ++item) {
      if (item->mKey == aKey) {
        // Found handle in the middle.
#ifdef SMALLARRAYLRUCACHE_STATS
        ++mCacheFoundAt[unsigned(item - item0)];
#endif  // SMALLARRAYLRUCACHE_STATS
        value = item->mValue;
        // Move this item to the start of the list.
        std::rotate(item0, item, item + 1);
        return value;
      }
    }

    // Handle was not in the list.
#ifdef SMALLARRAYLRUCACHE_STATS
    ++mCacheFoundAt[LRUCapacity];
#endif  // SMALLARRAYLRUCACHE_STATS
    {
      // Don't lock while doing the potentially-expensive ValueFunction().
      // This means that the list could change when we lock again, but
      // it's okay because we'll want to add the new entry at the beginning
      // anyway, whatever else is in the list then.
      // In the worst case, it could be the same handle as another `FetchOrAdd`
      // in parallel, it just means it will be duplicated, so it's a little bit
      // less efficient (using the extra space), but not wrong (the next
      // `FetchOrAdd` will find the first one).
      mozilla::OffTheBooksMutexAutoUnlock unlock(mMutex);
      value = std::forward<ValueFunction>(aValueFunction)();
    }
    // Make a hole at the start, and put the value there.
    mSize = std::min(mSize + 1, LRUCapacity);
    std::move_backward(item0, item0 + mSize - 1, item0 + mSize);
    item0->mKey = aKey;
    item0->mValue = value;
    return value;
  }

#ifdef SMALLARRAYLRUCACHE_STATS
  ~SmallArrayLRUCache() {
    if (mSize != 0 && mSize != ShutdownSize) {
      fprintf(stderr,
              "***** SmallArrayLRUCache stats: (position -> hit count)\n");
      for (unsigned i = 0; i < mSize; ++i) {
        fprintf(stderr, "***** %3u -> %6u\n", i, mCacheFoundAt[i]);
      }
      fprintf(stderr, "***** not found -> %6u\n", mCacheFoundAt[LRUCapacity]);
    }
  }
#endif  // SMALLARRAYLRUCACHE_STATS

 private:
  void Clear(const mozilla::OffTheBooksMutexAutoLock&) MOZ_REQUIRES(mMutex) {
    for (KeyAndValue* item = &mLRUArray[0]; item != &mLRUArray[mSize]; ++item) {
      item->mValue = Value{};
    }
    mSize = 0;
  }

  struct KeyAndValue {
    Key mKey;
    Value mValue;

    KeyAndValue() = default;
    KeyAndValue(KeyAndValue&&) = default;
    KeyAndValue& operator=(KeyAndValue&&) = default;
  };

  // Special size value that indicates the cache is in shutdown mode.
  constexpr static unsigned ShutdownSize = unsigned(-1);

  mozilla::OffTheBooksMutex mMutex{"LRU cache"};
  unsigned mSize MOZ_GUARDED_BY(mMutex) = 0;
  KeyAndValue mLRUArray[LRUCapacity] MOZ_GUARDED_BY(mMutex);
#ifdef SMALLARRAYLRUCACHE_STATS
  // Hit count for each position in the case. +1 for counting not-found cases.
  unsigned mCacheFoundAt[LRUCapacity + 1] MOZ_GUARDED_BY(mMutex) = {0u};
#endif  // SMALLARRAYLRUCACHE_STATS
};

}  // namespace mozilla

#endif  // SmallArrayLRUCache_h
