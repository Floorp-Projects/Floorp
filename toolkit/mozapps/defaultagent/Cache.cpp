/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "Cache.h"

#include <algorithm>

#include "common.h"
#include "EventLog.h"
#include "mozilla/Unused.h"

namespace mozilla::default_agent {

// Cache entry version documentation:
//   Version 1:
//     The version number is written explicitly when version 1 cache entries are
//     migrated, but in their original location there is no version key.
//     Required Keys:
//       CacheEntryVersion: <DWORD>
//       NotificationType: <string>
//       NotificationShown: <string>
//       NotificationAction: <string>
//   Version 2:
//     Required Keys:
//       CacheEntryVersion: <DWORD>
//       NotificationType: <string>
//       NotificationShown: <string>
//       NotificationAction: <string>
//       PrevNotificationAction: <string>

static std::wstring MakeVersionedRegSubKey(const wchar_t* baseKey) {
  std::wstring key;
  if (baseKey) {
    key = baseKey;
  } else {
    key = Cache::kDefaultPingCacheRegKey;
  }
  key += L"\\version";
  key += std::to_wstring(Cache::kVersion);
  return key;
}

Cache::Cache(const wchar_t* cacheRegKey /* = nullptr */)
    : mCacheRegKey(MakeVersionedRegSubKey(cacheRegKey)),
      mInitializeResult(mozilla::Nothing()),
      mCapacity(Cache::kDefaultCapacity),
      mFront(0),
      mSize(0) {}

Cache::~Cache() {}

VoidResult Cache::Init() {
  if (mInitializeResult.isSome()) {
    HRESULT hr = mInitializeResult.value();
    if (FAILED(hr)) {
      return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
    } else {
      return mozilla::Ok();
    }
  }

  VoidResult result = SetupCache();
  if (result.isErr()) {
    HRESULT hr = result.inspectErr().AsHResult();
    mInitializeResult = mozilla::Some(hr);
    return result;
  }

  // At this point, the cache is ready to use, so mark the initialization as
  // complete. This is important so that when we attempt migration, below,
  // the migration's attempts to write to the cache don't try to initialize
  // the cache again.
  mInitializeResult = mozilla::Some(S_OK);

  // Ignore the result of the migration. If we failed to migrate, there may be
  // some data loss. But that's better than failing to ever use the new cache
  // just because there's something wrong with the old one.
  mozilla::Unused << MaybeMigrateVersion1();

  return mozilla::Ok();
}

// If the setting does not exist, the default value is written and returned.
DwordResult Cache::EnsureDwordSetting(const wchar_t* regName,
                                      uint32_t defaultValue) {
  MaybeDwordResult readResult = RegistryGetValueDword(
      IsPrefixed::Unprefixed, regName, mCacheRegKey.c_str());
  if (readResult.isErr()) {
    HRESULT hr = readResult.unwrapErr().AsHResult();
    LOG_ERROR_MESSAGE(L"Failed to read setting \"%s\": %#X", regName, hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }
  mozilla::Maybe<uint32_t> maybeValue = readResult.unwrap();
  if (maybeValue.isSome()) {
    return maybeValue.value();
  }

  VoidResult writeResult = RegistrySetValueDword(
      IsPrefixed::Unprefixed, regName, defaultValue, mCacheRegKey.c_str());
  if (writeResult.isErr()) {
    HRESULT hr = writeResult.unwrapErr().AsHResult();
    LOG_ERROR_MESSAGE(L"Failed to write setting \"%s\": %#X", regName, hr);
    return mozilla::Err(mozilla::WindowsError::FromHResult(hr));
  }
  return defaultValue;
}

// This function does two things:
//  1. It creates and sets the registry values used by the cache, if they don't
//     already exist.
//  2. If the the values already existed, it reads the settings of the cache
//     into their member variables.
VoidResult Cache::SetupCache() {
  DwordResult result =
      EnsureDwordSetting(Cache::kCapacityRegName, Cache::kDefaultCapacity);
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr());
  }
  mCapacity = std::min(result.unwrap(), Cache::kMaxCapacity);

  result = EnsureDwordSetting(Cache::kFrontRegName, 0);
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr());
  }
  mFront = std::min(result.unwrap(), Cache::kMaxCapacity - 1);

  result = EnsureDwordSetting(Cache::kSizeRegName, 0);
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr());
  }
  mSize = std::min(result.unwrap(), mCapacity);

  return mozilla::Ok();
}

static MaybeStringResult ReadVersion1CacheKey(const wchar_t* baseRegKeyName,
                                              uint32_t index) {
  std::wstring regName = Cache::kVersion1KeyPrefix;
  regName += baseRegKeyName;
  regName += std::to_wstring(index);

  MaybeStringResult result =
      RegistryGetValueString(IsPrefixed::Unprefixed, regName.c_str());
  if (result.isErr()) {
    HRESULT hr = result.inspectErr().AsHResult();
    LOG_ERROR_MESSAGE(L"Failed to read \"%s\": %#X", regName.c_str(), hr);
  }
  return result;
}

static VoidResult DeleteVersion1CacheKey(const wchar_t* baseRegKeyName,
                                         uint32_t index) {
  std::wstring regName = Cache::kVersion1KeyPrefix;
  regName += baseRegKeyName;
  regName += std::to_wstring(index);

  VoidResult result =
      RegistryDeleteValue(IsPrefixed::Unprefixed, regName.c_str());
  if (result.isErr()) {
    HRESULT hr = result.inspectErr().AsHResult();
    LOG_ERROR_MESSAGE(L"Failed to delete \"%s\": %#X", regName.c_str(), hr);
  }
  return result;
}

static VoidResult DeleteVersion1CacheEntry(uint32_t index) {
  VoidResult typeResult =
      DeleteVersion1CacheKey(Cache::kNotificationTypeKey, index);
  VoidResult shownResult =
      DeleteVersion1CacheKey(Cache::kNotificationShownKey, index);
  VoidResult actionResult =
      DeleteVersion1CacheKey(Cache::kNotificationActionKey, index);

  if (typeResult.isErr()) {
    return typeResult;
  }
  if (shownResult.isErr()) {
    return shownResult;
  }
  return actionResult;
}

VoidResult Cache::MaybeMigrateVersion1() {
  for (uint32_t index = 0; index < Cache::kVersion1MaxSize; ++index) {
    MaybeStringResult typeResult =
        ReadVersion1CacheKey(Cache::kNotificationTypeKey, index);
    if (typeResult.isErr()) {
      return mozilla::Err(typeResult.unwrapErr());
    }
    MaybeString maybeType = typeResult.unwrap();

    MaybeStringResult shownResult =
        ReadVersion1CacheKey(Cache::kNotificationShownKey, index);
    if (shownResult.isErr()) {
      return mozilla::Err(shownResult.unwrapErr());
    }
    MaybeString maybeShown = shownResult.unwrap();

    MaybeStringResult actionResult =
        ReadVersion1CacheKey(Cache::kNotificationActionKey, index);
    if (actionResult.isErr()) {
      return mozilla::Err(actionResult.unwrapErr());
    }
    MaybeString maybeAction = actionResult.unwrap();

    if (maybeType.isSome() && maybeShown.isSome() && maybeAction.isSome()) {
      // If something goes wrong, we'd rather lose a little data than migrate
      // over and over again. So delete the old entry before we add the new one.
      VoidResult result = DeleteVersion1CacheEntry(index);
      if (result.isErr()) {
        return result;
      }

      VersionedEntry entry = VersionedEntry{
          .entryVersion = 1,
          .notificationType = maybeType.value(),
          .notificationShown = maybeShown.value(),
          .notificationAction = maybeAction.value(),
          .prevNotificationAction = mozilla::Nothing(),
      };
      result = VersionedEnqueue(entry);
      if (result.isErr()) {
        // We already deleted the version 1 cache entry. No real reason to abort
        // now. May as well keep attempting to migrate.
        LOG_ERROR_MESSAGE(L"Warning: Version 1 cache entry %u dropped: %#X",
                          index, result.unwrapErr().AsHResult());
      }
    } else if (maybeType.isNothing() && maybeShown.isNothing() &&
               maybeAction.isNothing()) {
      // Looks like we've reached the end of the version 1 cache.
      break;
    } else {
      // This cache entry seems to be missing a key. Just drop it.
      LOG_ERROR_MESSAGE(
          L"Warning: Version 1 cache entry %u dropped due to missing keys",
          index);
      mozilla::Unused << DeleteVersion1CacheEntry(index);
    }
  }
  return mozilla::Ok();
}

std::wstring Cache::MakeEntryRegKeyName(uint32_t index) {
  std::wstring regName = mCacheRegKey;
  regName += L'\\';
  regName += std::to_wstring(index);
  return regName;
}

VoidResult Cache::WriteEntryKeys(uint32_t index, const VersionedEntry& entry) {
  std::wstring subKey = MakeEntryRegKeyName(index);

  VoidResult result =
      RegistrySetValueDword(IsPrefixed::Unprefixed, Cache::kEntryVersionKey,
                            entry.entryVersion, subKey.c_str());
  if (result.isErr()) {
    LOG_ERROR_MESSAGE(L"Unable to write entry version to index %u: %#X", index,
                      result.inspectErr().AsHResult());
    return result;
  }

  result = RegistrySetValueString(
      IsPrefixed::Unprefixed, Cache::kNotificationTypeKey,
      entry.notificationType.c_str(), subKey.c_str());
  if (result.isErr()) {
    LOG_ERROR_MESSAGE(L"Unable to write notification type to index %u: %#X",
                      index, result.inspectErr().AsHResult());
    return result;
  }

  result = RegistrySetValueString(
      IsPrefixed::Unprefixed, Cache::kNotificationShownKey,
      entry.notificationShown.c_str(), subKey.c_str());
  if (result.isErr()) {
    LOG_ERROR_MESSAGE(L"Unable to write notification shown to index %u: %#X",
                      index, result.inspectErr().AsHResult());
    return result;
  }

  result = RegistrySetValueString(
      IsPrefixed::Unprefixed, Cache::kNotificationActionKey,
      entry.notificationAction.c_str(), subKey.c_str());
  if (result.isErr()) {
    LOG_ERROR_MESSAGE(L"Unable to write notification type to index %u: %#X",
                      index, result.inspectErr().AsHResult());
    return result;
  }

  if (entry.prevNotificationAction.isSome()) {
    result = RegistrySetValueString(
        IsPrefixed::Unprefixed, Cache::kPrevNotificationActionKey,
        entry.prevNotificationAction.value().c_str(), subKey.c_str());
    if (result.isErr()) {
      LOG_ERROR_MESSAGE(
          L"Unable to write prev notification type to index %u: %#X", index,
          result.inspectErr().AsHResult());
      return result;
    }
  }

  return mozilla::Ok();
}

// Returns success on an attempt to delete a non-existent entry.
VoidResult Cache::DeleteEntry(uint32_t index) {
  std::wstring key = AGENT_REGKEY_NAME;
  key += L'\\';
  key += MakeEntryRegKeyName(index);
  // We could probably just delete they key here, rather than use this function,
  // which deletes keys recursively. But this mechanism allows future entry
  // versions to contain sub-keys without causing problems for older versions.
  LSTATUS ls = RegDeleteTreeW(HKEY_CURRENT_USER, key.c_str());
  if (ls != ERROR_SUCCESS && ls != ERROR_FILE_NOT_FOUND) {
    return mozilla::Err(mozilla::WindowsError::FromWin32Error(ls));
  }
  return mozilla::Ok();
}

VoidResult Cache::SetFront(uint32_t newFront) {
  VoidResult result =
      RegistrySetValueDword(IsPrefixed::Unprefixed, Cache::kFrontRegName,
                            newFront, mCacheRegKey.c_str());
  if (result.isOk()) {
    mFront = newFront;
  }
  return result;
}

VoidResult Cache::SetSize(uint32_t newSize) {
  VoidResult result =
      RegistrySetValueDword(IsPrefixed::Unprefixed, Cache::kSizeRegName,
                            newSize, mCacheRegKey.c_str());
  if (result.isOk()) {
    mSize = newSize;
  }
  return result;
}

// The entry passed to this function MUST already be valid. This function does
// not do any validation internally. We must not, for example, pass an entry
// to it with a version of 2 and a prevNotificationAction of mozilla::Nothing()
// because a version 2 entry requires that key.
VoidResult Cache::VersionedEnqueue(const VersionedEntry& entry) {
  VoidResult result = Init();
  if (result.isErr()) {
    return result;
  }

  if (mSize >= mCapacity) {
    LOG_ERROR_MESSAGE(L"Attempted to add an entry to the cache, but it's full");
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_BOUNDS));
  }

  uint32_t index = (mFront + mSize) % mCapacity;

  // We really don't want to write to a location that has stale cache entry data
  // already lying around.
  result = DeleteEntry(index);
  if (result.isErr()) {
    LOG_ERROR_MESSAGE(L"Unable to remove stale entry: %#X",
                      result.inspectErr().AsHResult());
    return result;
  }

  result = WriteEntryKeys(index, entry);
  if (result.isErr()) {
    // We might have written a partial key. Attempt to clean up after ourself.
    mozilla::Unused << DeleteEntry(index);
    return result;
  }

  result = SetSize(mSize + 1);
  if (result.isErr()) {
    // If we failed to write the size, the new entry was not added successfully.
    // Attempt to clean up after ourself.
    mozilla::Unused << DeleteEntry(index);
    return result;
  }

  return mozilla::Ok();
}

VoidResult Cache::Enqueue(const Cache::Entry& entry) {
  Cache::VersionedEntry vEntry = Cache::VersionedEntry{
      .entryVersion = Cache::kEntryVersion,
      .notificationType = entry.notificationType,
      .notificationShown = entry.notificationShown,
      .notificationAction = entry.notificationAction,
      .prevNotificationAction = mozilla::Some(entry.prevNotificationAction),
  };
  return VersionedEnqueue(vEntry);
}

VoidResult Cache::DiscardFront() {
  if (mSize < 1) {
    LOG_ERROR_MESSAGE(L"Attempted to discard entry from an empty cache");
    return mozilla::Err(mozilla::WindowsError::FromHResult(E_BOUNDS));
  }
  // It's not a huge deal if we can't delete this. Moving mFront will result in
  // it being excluded from the cache anyways. We'll try to delete it again
  // anyways if we try to write to this index again.
  mozilla::Unused << DeleteEntry(mFront);

  VoidResult result = SetSize(mSize - 1);
  // We don't really need to bother moving mFront to the next index if the cache
  // is empty.
  if (result.isErr() || mSize == 0) {
    return result;
  }
  result = SetFront((mFront + 1) % mCapacity);
  if (result.isErr()) {
    // If we failed to set the front after we set the size, the cache is
    // in an inconsistent state.
    // But, even if the cache is inconsistent, we'll likely lose some data, but
    // we should eventually be able to recover. Any expected entries with no
    // data will be discarded and any unexpected entries with data will be
    // cleared out before we write data there.
    LOG_ERROR_MESSAGE(L"Cache inconsistent: Updated Size but not Front: %#X",
                      result.inspectErr().AsHResult());
  }
  return result;
}

/**
 * This function reads a DWORD cache key's value and returns it. If the expected
 * argument is true and the key is missing, this will delete the entire entry
 * and return mozilla::Nothing().
 */
MaybeDwordResult Cache::ReadEntryKeyDword(const std::wstring& regKey,
                                          const wchar_t* regName,
                                          bool expected) {
  MaybeDwordResult result =
      RegistryGetValueDword(IsPrefixed::Unprefixed, regName, regKey.c_str());
  if (result.isErr()) {
    LOG_ERROR_MESSAGE(L"Failed to read \"%s\" from \"%s\": %#X", regName,
                      regKey.c_str(), result.inspectErr().AsHResult());
    return mozilla::Err(result.unwrapErr());
  }
  MaybeDword maybeValue = result.unwrap();
  if (expected && maybeValue.isNothing()) {
    LOG_ERROR_MESSAGE(L"Missing expected value \"%s\" from \"%s\"", regName,
                      regKey.c_str());
    VoidResult result = DiscardFront();
    if (result.isErr()) {
      return mozilla::Err(result.unwrapErr());
    }
  }
  return maybeValue;
}

/**
 * This function reads a string cache key's value and returns it. If the
 * expected argument is true and the key is missing, this will delete the entire
 * entry and return mozilla::Nothing().
 */
MaybeStringResult Cache::ReadEntryKeyString(const std::wstring& regKey,
                                            const wchar_t* regName,
                                            bool expected) {
  MaybeStringResult result =
      RegistryGetValueString(IsPrefixed::Unprefixed, regName, regKey.c_str());
  if (result.isErr()) {
    LOG_ERROR_MESSAGE(L"Failed to read \"%s\" from \"%s\": %#X", regName,
                      regKey.c_str(), result.inspectErr().AsHResult());
    return mozilla::Err(result.unwrapErr());
  }
  MaybeString maybeValue = result.unwrap();
  if (expected && maybeValue.isNothing()) {
    LOG_ERROR_MESSAGE(L"Missing expected value \"%s\" from \"%s\"", regName,
                      regKey.c_str());
    VoidResult result = DiscardFront();
    if (result.isErr()) {
      return mozilla::Err(result.unwrapErr());
    }
  }
  return maybeValue;
}

Cache::MaybeEntryResult Cache::Dequeue() {
  VoidResult result = Init();
  if (result.isErr()) {
    return mozilla::Err(result.unwrapErr());
  }

  std::wstring subKey = MakeEntryRegKeyName(mFront);

  // We are going to read within a loop so that if we find incomplete entries,
  // we can just discard them and try to read the next entry. We'll put a limit
  // on the maximum number of times this loop can possibly run so that if
  // something goes horribly wrong, we don't loop forever. If we exit this loop
  // without returning, it means that not only were we not able to read
  // anything, but something very unexpected happened.
  // We are going to potentially loop over this mCapacity + 1 times so that if
  // we end up discarding every item in the cache, we return mozilla::Nothing()
  // rather than an error.
  for (uint32_t i = 0; i <= mCapacity; ++i) {
    if (mSize == 0) {
      return MaybeEntry(mozilla::Nothing());
    }

    Cache::VersionedEntry entry;

    // CacheEntryVersion
    MaybeDwordResult dResult =
        ReadEntryKeyDword(subKey, Cache::kEntryVersionKey, true);
    if (dResult.isErr()) {
      return mozilla::Err(dResult.unwrapErr());
    }
    MaybeDword maybeDValue = dResult.unwrap();
    if (maybeDValue.isNothing()) {
      // Note that we only call continue in this function after DiscardFront()
      // has been called (either directly, or by one of the ReadEntryKey.*
      // functions). So the continue call results in attempting to read the
      // next entry in the cache.
      continue;
    }
    entry.entryVersion = maybeDValue.value();
    if (entry.entryVersion < 1) {
      LOG_ERROR_MESSAGE(L"Invalid entry version of %u in \"%s\"",
                        entry.entryVersion, subKey.c_str());
      VoidResult result = DiscardFront();
      if (result.isErr()) {
        return mozilla::Err(result.unwrapErr());
      }
      continue;
    }

    // NotificationType
    MaybeStringResult sResult =
        ReadEntryKeyString(subKey, Cache::kNotificationTypeKey, true);
    if (sResult.isErr()) {
      return mozilla::Err(sResult.unwrapErr());
    }
    MaybeString maybeSValue = sResult.unwrap();
    if (maybeSValue.isNothing()) {
      continue;
    }
    entry.notificationType = maybeSValue.value();

    // NotificationShown
    sResult = ReadEntryKeyString(subKey, Cache::kNotificationShownKey, true);
    if (sResult.isErr()) {
      return mozilla::Err(sResult.unwrapErr());
    }
    maybeSValue = sResult.unwrap();
    if (maybeSValue.isNothing()) {
      continue;
    }
    entry.notificationShown = maybeSValue.value();

    // NotificationAction
    sResult = ReadEntryKeyString(subKey, Cache::kNotificationActionKey, true);
    if (sResult.isErr()) {
      return mozilla::Err(sResult.unwrapErr());
    }
    maybeSValue = sResult.unwrap();
    if (maybeSValue.isNothing()) {
      continue;
    }
    entry.notificationAction = maybeSValue.value();

    // PrevNotificationAction
    bool expected =
        entry.entryVersion >= Cache::kInitialVersionPrevNotificationActionKey;
    sResult =
        ReadEntryKeyString(subKey, Cache::kPrevNotificationActionKey, expected);
    if (sResult.isErr()) {
      return mozilla::Err(sResult.unwrapErr());
    }
    maybeSValue = sResult.unwrap();
    if (expected && maybeSValue.isNothing()) {
      continue;
    }
    entry.prevNotificationAction = maybeSValue;

    // We successfully read the entry. Now we need to remove it from the cache.
    VoidResult result = DiscardFront();
    if (result.isErr()) {
      // If we aren't able to remove the entry from the cache, don't return it.
      // We don't want to return the same item over and over again if we get
      // into a bad state.
      return mozilla::Err(result.unwrapErr());
    }

    return mozilla::Some(entry);
  }

  LOG_ERROR_MESSAGE(L"Unexpected: This line shouldn't be reached");
  return mozilla::Err(mozilla::WindowsError::FromHResult(E_FAIL));
}

}  // namespace mozilla::default_agent
