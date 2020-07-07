/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StartupCacheChild_h
#define StartupCacheChild_h

#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/PStartupCacheChild.h"
#include "mozilla/scache/PStartupCacheParent.h"

namespace mozilla {
namespace ipc {
class FileDescriptor;
}

namespace scache {

using mozilla::ipc::FileDescriptor;

class StartupCacheChild final : public PStartupCacheChild {
  friend class mozilla::scache::StartupCache;
  friend class mozilla::scache::StartupCacheListener;

 public:
  StartupCacheChild() = default;

  void Init(bool wantCacheData);

 protected:
  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

  void SendEntriesAndFinalize(StartupCache::Table& entries);

 private:
  bool mWantCacheData = false;
};

}  // namespace scache
}  // namespace mozilla

#endif  // StartupCacheChild_h
