/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsThreadUtils.h"
#include "mozilla/DebugOnly.h"
#include "mozilla/RLBoxSandboxPool.h"

using namespace mozilla;

NS_IMPL_ISUPPORTS(RLBoxSandboxPool, nsITimerCallback, nsINamed)

void RLBoxSandboxPool::StartTimer() {
  mMutex.AssertCurrentThreadOwns();
  MOZ_ASSERT(!mTimer, "timer already initialized");
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

UniquePtr<RLBoxSandboxPoolData> RLBoxSandboxPool::PopOrCreate() {
  MutexAutoLock lock(mMutex);

  UniquePtr<RLBoxSandboxDataBase> sbxData;
  if (!mPool.IsEmpty()) {
    sbxData = mPool.PopLastElement();
    CancelTimer();
    if (!mPool.IsEmpty()) {
      StartTimer();
    }
  } else {
    sbxData = CreateSandboxData();
    NS_ENSURE_TRUE(sbxData, nullptr);
  }

  return MakeUnique<RLBoxSandboxPoolData>(std::move(sbxData), this);
}
