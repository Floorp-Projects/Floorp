/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef __DEFAULT_BROWSER_AGENT_CACHE_H__
#define __DEFAULT_BROWSER_AGENT_CACHE_H__

#include <cstdint>
#include <string>
#include <windows.h>

#include "Registry.h"

namespace mozilla::default_agent {

using DwordResult = mozilla::WindowsErrorResult<uint32_t>;

/**
 * This cache functions as a FIFO queue which writes its data to the Windows
 * registry.
 *
 * Note that the cache is not thread-safe, so it is recommended that the WDBA's
 * RegistryMutex be acquired before accessing it.
 *
 * Some of the terminology used in this module is a easy to mix up, so let's
 * just be clear about it:
 *  - registry key/sub-key
 *       A registry key is sort of like the registry's equivalent of a
 *       directory. It can contain values, each of which is made up of a name
 *       and corresponding data. We may also refer to a "sub-key", meaning a
 *       registry key nested in a registry key.
 *  - cache key/entry key
 *       A cache key refers to the string that we use to look up a single
 *       element of cache entry data. Example: "CacheEntryVersion"
 *  - entry
 *      This refers to an entire record stored using Cache::Enqueue or retrieved
 *      using Cache::Dequeue. It consists of numerous cache keys and their
 *      corresponding data.
 *
 * The first version of this cache was problematic because of how hard it was to
 * extend. This version attempts to overcome this. It first migrates all data
 * out of the version 1 cache. This means that the stored ping data will not
 * be accessible to out-of-date clients, but presumably they will eventually
 * be updated or the up-to-date client that performed the migration will send
 * the pings itself. Because the WDBA telemetry has no client ID, all analysis
 * is stateless, so even if the other clients send some pings before the stored
 * ones get sent, that's ok. The ordering isn't really important.
 *
 * This version of the cache attempts to correct the problem of how hard it was
 * to extend the old cache. The biggest problem that the old cache had was that
 * when it dequeued data it had to shift data, but it wouldn't shift keys that
 * it didn't know about, causing them to become associated with the wrong cache
 * entries.
 *
 * Version 2 of the cache will make 4 improvements to attempt to avoid problems
 * like this in the future:
 *  1. Each cache entry will get its own registry key. This will help to keep
 *     cache entries isolated from each other.
 *  2. Each cache entry will include version data so that we know what cache
 *     keys to expect when we read it.
 *  3. Rather than having to shift every entry every time we dequeue, we will
 *     implement a circular queue so that we just have to update what index
 *     currently represents the front
 *  4. We will store the cache capacity in the cache so that we can expand the
 *     cache later, if we want, without breaking previous versions.
 */
class Cache {
 public:
  // cacheRegKey is the registry sub-key that the cache will be stored in. If
  // null is passed (the default), we will use the default cache name. This is
  // what ought to be used in production. When testing, we will pass a different
  // key in so that our testing caches don't conflict with each other or with
  // a possible production cache on the test machine.
  explicit Cache(const wchar_t* cacheRegKey = nullptr);
  ~Cache();

  // The version of the cache (not to be confused with the version of the cache
  // entries). This should only be incremented if we need to make breaking
  // changes that require migration to a new cache location, like we did between
  // versions 1 and 2. This value will be used as part of the sub-key that the
  // cache is stored in (ex: "PingCache\version2").
  static constexpr const uint32_t kVersion = 2;
  // This value will be written into each entry. This allows us to know what
  // cache keys to expect in the event that additional cache keys are added in
  // later entry versions.
  static constexpr const uint32_t kEntryVersion = 2;
  static constexpr const uint32_t kDefaultCapacity = 2;
  // We want to allow the cache to be expandable, but we don't really want it to
  // be infinitely expandable. So we'll set an upper bound.
  static constexpr const uint32_t kMaxCapacity = 100;
  static constexpr const wchar_t* kDefaultPingCacheRegKey = L"PingCache";

  // Used to read the version 1 cache entries during data migration. Full cache
  // key names are formatted like: "<keyPrefix><baseKeyName><cacheIndex>"
  // For example: "PingCacheNotificationType0"
  static constexpr const wchar_t* kVersion1KeyPrefix = L"PingCache";
  static constexpr const uint32_t kVersion1MaxSize = 2;

  static constexpr const wchar_t* kCapacityRegName = L"Capacity";
  static constexpr const wchar_t* kFrontRegName = L"Front";
  static constexpr const wchar_t* kSizeRegName = L"Size";

  // Cache Entry keys
  static constexpr const wchar_t* kEntryVersionKey = L"CacheEntryVersion";
  // Note that the next 3 must also match the base key names from version 1
  // since we use them to construct those key names.
  static constexpr const wchar_t* kNotificationTypeKey = L"NotificationType";
  static constexpr const wchar_t* kNotificationShownKey = L"NotificationShown";
  static constexpr const wchar_t* kNotificationActionKey =
      L"NotificationAction";
  static constexpr const wchar_t* kPrevNotificationActionKey =
      L"PrevNotificationAction";

  // The version key wasn't added until version 2, but we add it to the version
  // 1 entries when migrating them to the cache.
  static constexpr const uint32_t kInitialVersionEntryVersionKey = 1;
  static constexpr const uint32_t kInitialVersionNotificationTypeKey = 1;
  static constexpr const uint32_t kInitialVersionNotificationShownKey = 1;
  static constexpr const uint32_t kInitialVersionNotificationActionKey = 1;
  static constexpr const uint32_t kInitialVersionPrevNotificationActionKey = 2;

  // We have two cache entry structs: one for the current version, and one
  // generic one that can handle any version. There are a couple of reasons
  // for this:
  //  - We only want to support writing the current version, but we want to
  //    support reading any version.
  //  - It makes things a bit nicer for the caller when Enqueue-ing, since
  //    they don't have to set the version or wrap values that were added
  //    later in a mozilla::Maybe.
  //  - It keeps us from having to worry about writing an invalid cache entry,
  //    such as one that claims to be version 2, but doesn't have
  //    prevNotificationAction.
  // Note that the entry struct for the current version does not contain a
  // version member value because we already know that its version is equal to
  // Cache::kEntryVersion.
  struct Entry {
    std::string notificationType;
    std::string notificationShown;
    std::string notificationAction;
    std::string prevNotificationAction;
  };
  struct VersionedEntry {
    uint32_t entryVersion;
    std::string notificationType;
    std::string notificationShown;
    std::string notificationAction;
    mozilla::Maybe<std::string> prevNotificationAction;
  };

  using MaybeEntry = mozilla::Maybe<VersionedEntry>;
  using MaybeEntryResult = mozilla::WindowsErrorResult<MaybeEntry>;

  VoidResult Init();
  VoidResult Enqueue(const Entry& entry);
  MaybeEntryResult Dequeue();

 private:
  const std::wstring mCacheRegKey;

  // We can't easily copy a VoidResult, so just store the raw HRESULT here.
  mozilla::Maybe<HRESULT> mInitializeResult;
  // How large the cache will grow before it starts rejecting new entries.
  uint32_t mCapacity;
  // The index of the first present cache entry.
  uint32_t mFront;
  // How many entries are present in the cache.
  uint32_t mSize;

  DwordResult EnsureDwordSetting(const wchar_t* regName, uint32_t defaultValue);
  VoidResult SetupCache();
  VoidResult MaybeMigrateVersion1();
  std::wstring MakeEntryRegKeyName(uint32_t index);
  VoidResult WriteEntryKeys(uint32_t index, const VersionedEntry& entry);
  VoidResult DeleteEntry(uint32_t index);
  VoidResult SetFront(uint32_t newFront);
  VoidResult SetSize(uint32_t newSize);
  VoidResult VersionedEnqueue(const VersionedEntry& entry);
  VoidResult DiscardFront();
  MaybeDwordResult ReadEntryKeyDword(const std::wstring& regKey,
                                     const wchar_t* regName, bool expected);
  MaybeStringResult ReadEntryKeyString(const std::wstring& regKey,
                                       const wchar_t* regName, bool expected);
};

}  // namespace mozilla::default_agent

#endif  // __DEFAULT_BROWSER_AGENT_CACHE_H__
