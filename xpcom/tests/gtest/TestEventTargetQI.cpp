/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/LazyIdleThread.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/ThrottledEventQueue.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsThreadPool.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCIDInternal.h"
#include "gtest/gtest.h"

using namespace mozilla;

// Cast the pointer to nsISupports* through nsIEventTarget* before doing the QI
// in order to avoid a static assert intended to prevent trivial QIs, while also
// avoiding ambiguous base errors.
template <typename TargetInterface, typename SourcePtr>
bool TestQITo(SourcePtr& aPtr1) {
  nsCOMPtr<TargetInterface> aPtr2 = do_QueryInterface(
      static_cast<nsISupports*>(static_cast<nsIEventTarget*>(aPtr1.get())));
  return (bool)aPtr2;
}

TEST(TestEventTargetQI, ThreadPool)
{
  nsCOMPtr<nsIThreadPool> thing = new nsThreadPool();

  EXPECT_FALSE(TestQITo<nsISerialEventTarget>(thing));

  EXPECT_TRUE(TestQITo<nsIEventTarget>(thing));

  thing->Shutdown();
}

TEST(TestEventTargetQI, SharedThreadPool)
{
  nsCOMPtr<nsIThreadPool> thing = SharedThreadPool::Get("TestPool"_ns, 1);
  EXPECT_TRUE(thing);

  EXPECT_FALSE(TestQITo<nsISerialEventTarget>(thing));

  EXPECT_TRUE(TestQITo<nsIEventTarget>(thing));
}

TEST(TestEventTargetQI, Thread)
{
  nsCOMPtr<nsIThread> thing = do_GetCurrentThread();
  EXPECT_TRUE(thing);

  EXPECT_TRUE(TestQITo<nsISerialEventTarget>(thing));

  EXPECT_TRUE(TestQITo<nsIEventTarget>(thing));
}

TEST(TestEventTargetQI, ThrottledEventQueue)
{
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
  RefPtr<ThrottledEventQueue> thing =
      ThrottledEventQueue::Create(thread, "test queue");
  EXPECT_TRUE(thing);

  EXPECT_TRUE(TestQITo<nsISerialEventTarget>(thing));

  EXPECT_TRUE(TestQITo<nsIEventTarget>(thing));
}

TEST(TestEventTargetQI, LazyIdleThread)
{
  RefPtr<LazyIdleThread> thing = new LazyIdleThread(0, "TestThread");
  EXPECT_TRUE(thing);

  EXPECT_TRUE(TestQITo<nsISerialEventTarget>(thing));

  EXPECT_TRUE(TestQITo<nsIEventTarget>(thing));

  thing->Shutdown();
}
