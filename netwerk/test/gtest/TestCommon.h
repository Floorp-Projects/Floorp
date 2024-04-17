/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef TestCommon_h__
#define TestCommon_h__

#include <stdlib.h>
#include "nsThreadUtils.h"
#include "mozilla/Attributes.h"
#include "mozilla/SpinEventLoopUntil.h"

//-----------------------------------------------------------------------------

class WaitForCondition final : public nsIRunnable {
 public:
  NS_DECL_THREADSAFE_ISUPPORTS

  void Wait(int pending) {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    MOZ_RELEASE_ASSERT(mPending == 0);

    mPending = pending;
    mozilla::SpinEventLoopUntil("TestCommon.h:WaitForCondition::Wait"_ns,
                                [&]() { return !mPending; });
    NS_ProcessPendingEvents(nullptr);
  }

  void Notify() { NS_DispatchToMainThread(this); }

 private:
  virtual ~WaitForCondition() = default;

  NS_IMETHOD Run() override {
    MOZ_RELEASE_ASSERT(NS_IsMainThread());
    MOZ_RELEASE_ASSERT(mPending);

    --mPending;
    return NS_OK;
  }

  uint32_t mPending = 0;
};

#endif
