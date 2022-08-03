/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SECURITY_RLBOX_SANDBOX_POOL_H_
#define SECURITY_RLBOX_SANDBOX_POOL_H_

#include "nsCOMPtr.h"
#include "nsITimer.h"
#include "nsTArray.h"
#include "nsINamed.h"

#include "mozilla/Mutex.h"
#include "mozilla/rlbox/rlbox_types.hpp"

namespace mozilla {

class RLBoxSandboxDataBase;
class RLBoxSandboxPoolData;

// The RLBoxSandboxPool class is used to manage a pool of sandboxes that are
// reused -- to save sandbox creation time and memory -- and automatically
// destroyed when no longer in used. The sandbox pool is threadsafe and can be
// used to share unused sandboxes across a thread pool.
//
// Each sandbox pool manages a particular kind of sandbox (e.g., expat
// sandboxes, woff2 sandboxes, etc.); this is largely because different
// sandboxes might have different callbacks and attacker assumptions. Hence,
// RLBoxSandboxPool is intended to be subclassed for the different kinds of
// sandbox pools. Each sandbox pool class needs to implement the
// CreateSandboxData() method, which returns a pointer to a RLBoxSandboxDataBase
// object.  RLBoxSandboxDataBase itself should be subclassed to implement
// sandbox-specific details.
class RLBoxSandboxPool : public nsITimerCallback, public nsINamed {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS
  NS_DECL_NSITIMERCALLBACK
  NS_DECL_NSINAMED

  RLBoxSandboxPool(size_t aDelaySeconds = 10)
      : mPool(),
        mDelaySeconds(aDelaySeconds),
        mMutex("RLBoxSandboxPool::mMutex"){};

  void Push(UniquePtr<RLBoxSandboxDataBase> sbx);
  // PopOrCreate returns a sandbox from the pool if the pool is not empty and
  // tries to mint a new one otherwise. If creating a new sandbox fails, the
  // function returns a nullptr. The parameter aMinSize is the minimum size of
  // the sandbox memory.
  UniquePtr<RLBoxSandboxPoolData> PopOrCreate(uint64_t aMinSize = 0);

 protected:
  // CreateSandboxData takes a parameter which is the size of the sandbox memory
  virtual UniquePtr<RLBoxSandboxDataBase> CreateSandboxData(uint64_t aSize) = 0;
  virtual ~RLBoxSandboxPool() = default;

 private:
  void StartTimer() MOZ_REQUIRES(mMutex);
  void CancelTimer() MOZ_REQUIRES(mMutex);

  nsTArray<UniquePtr<RLBoxSandboxDataBase>> mPool MOZ_GUARDED_BY(mMutex);
  const size_t mDelaySeconds MOZ_GUARDED_BY(mMutex);
  nsCOMPtr<nsITimer> mTimer MOZ_GUARDED_BY(mMutex);
  mozilla::Mutex mMutex;
};

// The RLBoxSandboxDataBase class serves as the subclass for all sandbox data
// classes, which keep track of the RLBox sandbox and any relevant sandbox data
// (e.g., callbacks).
class RLBoxSandboxDataBase {
 public:
  const uint64_t mSize;
  explicit RLBoxSandboxDataBase(uint64_t aSize) : mSize(aSize) {}
  virtual ~RLBoxSandboxDataBase() = default;
};

// This class is used wrap sandbox data objects (RLBoxSandboxDataBase) when they
// are popped from sandbox pools. The wrapper destructor pushes the sandbox back
// into the pool.
class RLBoxSandboxPoolData {
 public:
  RLBoxSandboxPoolData(UniquePtr<RLBoxSandboxDataBase> aSbxData,
                       RefPtr<RLBoxSandboxPool> aPool) {
    mSbxData = std::move(aSbxData);
    mPool = aPool;
    MOZ_COUNT_CTOR(RLBoxSandboxPoolData);
  }

  RLBoxSandboxDataBase* SandboxData() const { return mSbxData.get(); };

  ~RLBoxSandboxPoolData() {
    mPool->Push(std::move(mSbxData));
    MOZ_COUNT_DTOR(RLBoxSandboxPoolData);
  };

 private:
  UniquePtr<RLBoxSandboxDataBase> mSbxData;
  RefPtr<RLBoxSandboxPool> mPool;
};

}  // namespace mozilla

#endif
