/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2; -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef StartupCacheParent_h
#define StartupCacheParent_h

#include "mozilla/scache/StartupCache.h"
#include "mozilla/scache/PStartupCacheParent.h"

namespace mozilla {

namespace scache {

using mozilla::ipc::IPCResult;

class StartupCacheParent final : public PStartupCacheParent {
  friend class PStartupCacheParent;

 public:
  explicit StartupCacheParent(bool wantCacheData)
      : mWantCacheData(wantCacheData) {}

 protected:
  IPCResult Recv__delete__(nsTArray<EntryData>&& entries);

  virtual void ActorDestroy(ActorDestroyReason aWhy) override;

 private:
  bool mWantCacheData;
};

}  // namespace scache
}  // namespace mozilla

#endif  // StartupCacheParent_h
