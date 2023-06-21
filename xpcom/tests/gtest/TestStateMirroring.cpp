/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/gtest/WaitFor.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StateMirroring.h"
#include "mozilla/SynchronizedEventQueue.h"
#include "mozilla/TaskQueue.h"
#include "mozilla/Unused.h"
#include "nsISupportsImpl.h"
#include "nsThreadUtils.h"
#include "VideoUtils.h"

namespace TestStateMirroring {

using namespace mozilla;

class StateMirroringTest : public ::testing::Test {
 public:
  using ValueType = int;
  using Promise = MozPromise<ValueType, bool, /*IsExclusive =*/true>;

  StateMirroringTest()
      : mTarget(
            TaskQueue::Create(GetMediaThreadPool(MediaThreadType::SUPERVISOR),
                              "TestStateMirroring",
                              /*aSupportsTailDispatch =*/true)),
        mCanonical(AbstractThread::GetCurrent(), 0, "TestCanonical"),
        mMirror(mTarget, 0, "TestMirror") {}

  void TearDown() override {
    mTarget->BeginShutdown();
    mTarget->AwaitShutdownAndIdle();

    // Make sure to run any events just dispatched from mTarget to main thread
    // before continuing on to the next test.
    NS_ProcessPendingEvents(nullptr);
  }

  RefPtr<Promise> ReadMirrorAsync() {
    return InvokeAsync(mTarget, __func__, [&] {
      return Promise::CreateAndResolve(mMirror, "ReadMirrorAsync::Resolve");
    });
  }

 protected:
  const RefPtr<TaskQueue> mTarget;
  Canonical<int> mCanonical;
  Mirror<int> mMirror;
};

TEST_F(StateMirroringTest, MirrorInitiatedEventOrdering) {
  // Note that this requires the tail dispatcher, which is not available until
  // we are processing events.
  ASSERT_FALSE(AbstractThread::GetCurrent()->IsTailDispatcherAvailable());

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchAndSpinEventLoopUntilComplete(
      "NeedTailDispatcher"_ns, GetCurrentSerialEventTarget(),
      NS_NewRunnableFunction(__func__, [&] {
        ASSERT_TRUE(AbstractThread::GetCurrent()->IsTailDispatcherAvailable());

        RefPtr<Promise> mirrorPromise;

        // This will ensure the tail dispatcher fires to dispatch the group
        // runnable holding the Mirror::Connect task, before we continue the
        // test.
        MOZ_ALWAYS_SUCCEEDS(NS_DispatchAndSpinEventLoopUntilComplete(
            "NeedTailDispatcher"_ns, GetCurrentSerialEventTarget(),
            NS_NewRunnableFunction(__func__, [&] {
              // Show that mirroring does not take effect until async init is
              // done.

              MOZ_ALWAYS_SUCCEEDS(mTarget->Dispatch(NS_NewRunnableFunction(
                  __func__, [&] { mMirror.Connect(&mCanonical); })));
              mCanonical = 1;
              mirrorPromise = ReadMirrorAsync();
            })));

        // Make sure Mirror::Connect has run on mTarget.
        mTarget->AwaitIdle();

        // Make sure Canonical::AddMirror has run on this thread.
        NS_ProcessPendingEvents(nullptr);

        // Prior to establishing the connection, a value has not been mirrored.
        EXPECT_EQ(WaitFor(mirrorPromise).unwrap(), 0);

        // Once a connection has been establised, event ordering is as expected.
        mCanonical = 2;
        EXPECT_EQ(WaitFor(ReadMirrorAsync()).unwrap(), 2);

        MOZ_ALWAYS_SUCCEEDS(mTarget->Dispatch(NS_NewRunnableFunction(
            __func__, [&] { mMirror.DisconnectIfConnected(); })));
      })));
}

TEST_F(StateMirroringTest, CanonicalInitiatedEventOrdering) {
  // Note that this requires the tail dispatcher, which is not available until
  // we are processing events.
  ASSERT_FALSE(AbstractThread::GetCurrent()->IsTailDispatcherAvailable());

  MOZ_ALWAYS_SUCCEEDS(NS_DispatchAndSpinEventLoopUntilComplete(
      "NeedTailDispatcher"_ns, GetCurrentSerialEventTarget(),
      NS_NewRunnableFunction(__func__, [&] {
        ASSERT_TRUE(AbstractThread::GetCurrent()->IsTailDispatcherAvailable());

        mCanonical.ConnectMirror(&mMirror);

        // Event ordering is as expected immediately.
        mCanonical = 1;
        EXPECT_EQ(WaitFor(ReadMirrorAsync()).unwrap(), 1);

        mCanonical.DisconnectAll();
      })));
}

}  // namespace TestStateMirroring
