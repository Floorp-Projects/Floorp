/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/ClearOnShutdown.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/RLBoxSandboxPool.h"
#ifdef MOZ_USING_WASM_SANDBOXING
#  include "mozilla/rlbox/rlbox_config.h"
#  include "mozilla/rlbox/rlbox_wasm2c_sandbox.hpp"
#endif

using namespace mozilla;

NS_IMPL_ISUPPORTS(RLBoxSandboxPool, nsITimerCallback, nsINamed)

void RLBoxSandboxPool::StartTimer() {
  mMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(!mTimer, "timer already initialized");
  if (NS_IsMainThread() &&
      PastShutdownPhase(ShutdownPhase::AppShutdownConfirmed)) {
    // If we're shutting down, setting the time might fail, and we don't need it
    // (since all the memory will be cleaned up soon anyway). Note that
    // PastShutdownPhase() can only be called on the main thread, but that's
    // fine, because other threads will have joined already by the point timers
    // start failing to register.
    mPool.Clear();
    return;
  }
  DebugOnly<nsresult> rv = NS_NewTimerWithCallback(
      getter_AddRefs(mTimer), this, mDelaySeconds * 1000,
      nsITimer::TYPE_ONE_SHOT, GetMainThreadEventTarget());
  MOZ_ASSERT(NS_SUCCEEDED(rv), "failed to create timer");
}

void RLBoxSandboxPool::CancelTimer() {
  mMutex.AssertCurrentThreadOwns();
  if (mTimer) {
    mTimer->Cancel();
    mTimer = nullptr;
  }
}

NS_IMETHODIMP RLBoxSandboxPool::Notify(nsITimer* aTimer) {
  MutexAutoLock lock(mMutex);

  mPool.Clear();
  mTimer = nullptr;

  return NS_OK;
}

NS_IMETHODIMP RLBoxSandboxPool::GetName(nsACString& aName) {
  aName.AssignLiteral("RLBoxSandboxPool");
  return NS_OK;
}

void RLBoxSandboxPool::Push(UniquePtr<RLBoxSandboxDataBase> sbxData) {
  MutexAutoLock lock(mMutex);

  mPool.AppendElement(std::move(sbxData));
  if (!mTimer) {
    StartTimer();
  }
}

UniquePtr<RLBoxSandboxPoolData> RLBoxSandboxPool::PopOrCreate(
    uint64_t aMinSize) {
  MutexAutoLock lock(mMutex);

  UniquePtr<RLBoxSandboxDataBase> sbxData;

  if (!mPool.IsEmpty()) {
    const int64_t lastIndex = ReleaseAssertedCast<int64_t>(mPool.Length()) - 1;
    for (int64_t i = lastIndex; i >= 0; i--) {
      if (mPool[i]->mSize >= aMinSize) {
        sbxData = std::move(mPool[i]);
        mPool.RemoveElementAt(i);

        // If we reuse a sandbox from the pool, reset the timer to clear the
        // pool
        CancelTimer();
        if (!mPool.IsEmpty()) {
          StartTimer();
        }
        break;
      }
    }
  }

  if (!sbxData) {
#ifdef MOZ_USING_WASM_SANDBOXING
    // RLBox's wasm sandboxes have a limited platform dependent capacity. We
    // track this capacity in this pool. Note the noop sandboxes have no
    // capacity limit but this design assumes that all sandboxes use the wasm
    // sandbox limit.
    const uint64_t defaultCapacityForSandbox =
        wasm_rt_get_default_max_linear_memory_size();
    const uint64_t minSandboxCapacity =
        std::max(aMinSize, defaultCapacityForSandbox);
    const uint64_t chosenAdjustedCapacity =
        rlbox::rlbox_wasm2c_sandbox::rlbox_wasm2c_get_adjusted_heap_size(
            minSandboxCapacity);
#else
    // If sandboxing is disabled altogether we just set a limit of 4gb.
    // This is not actually enforced by the noop sandbox.
    const uint64_t chosenAdjustedCapacity = static_cast<uint64_t>(1) << 32;
#endif
    sbxData = CreateSandboxData(chosenAdjustedCapacity);
    NS_ENSURE_TRUE(sbxData, nullptr);
  }

  return MakeUnique<RLBoxSandboxPoolData>(std::move(sbxData), this);
}
