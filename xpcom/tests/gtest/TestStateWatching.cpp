/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/StateWatching.h"
#include "mozilla/TaskQueue.h"
#include "nsISupportsImpl.h"
#include "VideoUtils.h"

namespace TestStateWatching {

using namespace mozilla;

struct Foo {
  NS_INLINE_DECL_THREADSAFE_REFCOUNTING(Foo)
  void Notify() { mNotified = true; }
  bool mNotified = false;
private:
  ~Foo() {}
};

TEST(WatchManager, Shutdown)
{
  RefPtr<TaskQueue> queue = new TaskQueue(
    GetMediaThreadPool(MediaThreadType::PLAYBACK));

  RefPtr<Foo> p = new Foo;
  WatchManager<Foo> manager(p, queue);
  Watchable<bool> notifier(false, "notifier");

  queue->Dispatch(NS_NewRunnableFunction(
    "TestStateWatching::WatchManager_Shutdown_Test::TestBody", [&]() {
      manager.Watch(notifier, &Foo::Notify);
      notifier = true;    // Trigger the call to Foo::Notify().
      manager.Shutdown(); // Shutdown() should cancel the call.
    }));

  queue->BeginShutdown();
  queue->AwaitShutdownAndIdle();
  EXPECT_FALSE(p->mNotified);
}

} // namespace TestStateWatching
