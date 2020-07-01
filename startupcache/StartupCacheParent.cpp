/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/scache/StartupCacheParent.h"

#include "mozilla/scache/StartupCache.h"
#include "mozilla/dom/ContentParent.h"

namespace mozilla {
namespace scache {

IPCResult StartupCacheParent::Recv__delete__(nsTArray<EntryData>&& entries) {
  if (!mWantCacheData && entries.Length()) {
    return IPC_FAIL(this, "UnexpectedScriptData");
  }

  mWantCacheData = false;

  if (entries.Length()) {
    auto* cache = StartupCache::GetSingleton();
    for (auto& entry : entries) {
      auto buffer = MakeUnique<char[]>(entry.data().Length());
      memcpy(buffer.get(), entry.data().Elements(), entry.data().Length());
      cache->PutBuffer(entry.key().get(), std::move(buffer),
                       entry.data().Length(), /* isFromChildProcess:*/ true);
    }
  }

  return IPC_OK();
}

void StartupCacheParent::ActorDestroy(ActorDestroyReason aWhy) {}

}  // namespace scache
}  // namespace mozilla