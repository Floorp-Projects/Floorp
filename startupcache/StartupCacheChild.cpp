/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/scache/StartupCacheChild.h"

#include "mozilla/scache/StartupCache.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace scache {

void StartupCacheChild::Init() {
  auto* cache = StartupCache::GetSingleton();
  if (cache) {
    Unused << cache->InitChild(this);
  } else {
    Send__delete__(this, AutoTArray<EntryData, 0>());
  }
}

void StartupCacheChild::SendEntriesAndFinalize(StartupCache::Table& entries) {
  nsTArray<EntryData> dataArray;
  for (auto iter = entries.iter(); !iter.done(); iter.next()) {
    const auto& key = iter.get().key();
    auto& value = iter.get().value();

    if (!value.mData ||
        value.mRequestedOrder == kStartupCacheEntryNotRequested) {
      continue;
    }

    auto data = dataArray.AppendElement();

    MOZ_ASSERT(strnlen(key.get(), kStartupCacheKeyLengthCap) <
                   kStartupCacheKeyLengthCap,
               "StartupCache key over the size limit.");
    data->key() = nsCString(key.get());
    if (value.mFlags.contains(StartupCacheEntryFlags::AddedThisSession)) {
      data->data().AppendElements(
          reinterpret_cast<const uint8_t*>(value.mData.get()),
          value.mUncompressedSize);
    }
  }

  Send__delete__(this, dataArray);

  for (auto iter = entries.iter(); !iter.done(); iter.next()) {
    auto& value = iter.get().value();
    if (!value.mFlags.contains(StartupCacheEntryFlags::DoNotFree)) {
      value.mData = nullptr;
    }
  }
}

void StartupCacheChild::ActorDestroy(ActorDestroyReason aWhy) {
  auto* cache = StartupCache::GetSingleton();
  if (cache) {
    cache->mChildActor = nullptr;
  }
}

}  // namespace scache
}  // namespace mozilla