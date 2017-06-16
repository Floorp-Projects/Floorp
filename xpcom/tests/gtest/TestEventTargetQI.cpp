/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/LazyIdleThread.h"
#include "mozilla/SharedThreadPool.h"
#include "mozilla/SystemGroup.h"
#include "mozilla/ThrottledEventQueue.h"
#include "nsCOMPtr.h"
#include "nsIThreadPool.h"
#include "nsThreadUtils.h"
#include "nsXPCOM.h"
#include "nsXPCOMCIDInternal.h"
#include "gtest/gtest.h"

using namespace mozilla;

TEST(TestEventTargetQI, ThreadPool)
{
  nsCOMPtr<nsIThreadPool> thing = do_CreateInstance(NS_THREADPOOL_CONTRACTID);
  EXPECT_TRUE(thing);

  nsCOMPtr<nsISerialEventTarget> serial = do_QueryInterface(thing);
  EXPECT_FALSE(serial);

  nsCOMPtr<nsIEventTarget> target = do_QueryInterface(thing);
  EXPECT_TRUE(target);

  thing->Shutdown();
}

TEST(TestEventTargetQI, SharedThreadPool)
{
  nsCOMPtr<nsIThreadPool> thing = SharedThreadPool::Get(NS_LITERAL_CSTRING("TestPool"), 1);
  EXPECT_TRUE(thing);

  nsCOMPtr<nsISerialEventTarget> serial = do_QueryInterface(thing);
  EXPECT_FALSE(serial);

  nsCOMPtr<nsIEventTarget> target = do_QueryInterface(thing);
  EXPECT_TRUE(target);
}

TEST(TestEventTargetQI, Thread)
{
  nsCOMPtr<nsIThread> thing = do_GetCurrentThread();
  EXPECT_TRUE(thing);

  nsCOMPtr<nsISerialEventTarget> serial = do_QueryInterface(thing);
  EXPECT_TRUE(serial);

  nsCOMPtr<nsIEventTarget> target = do_QueryInterface(thing);
  EXPECT_TRUE(target);
}

TEST(TestEventTargetQI, ThrottledEventQueue)
{
  nsCOMPtr<nsIThread> thread = do_GetCurrentThread();
  RefPtr<ThrottledEventQueue> thing = ThrottledEventQueue::Create(thread);
  EXPECT_TRUE(thing);

  nsCOMPtr<nsISerialEventTarget> serial = do_QueryInterface(thing);
  EXPECT_TRUE(serial);

  nsCOMPtr<nsIEventTarget> target = do_QueryInterface(thing);
  EXPECT_TRUE(target);
}

TEST(TestEventTargetQI, LazyIdleThread)
{
  nsCOMPtr<nsIThread> thing = new LazyIdleThread(0, NS_LITERAL_CSTRING("TestThread"));
  EXPECT_TRUE(thing);

  nsCOMPtr<nsISerialEventTarget> serial = do_QueryInterface(thing);
  EXPECT_TRUE(serial);

  nsCOMPtr<nsIEventTarget> target = do_QueryInterface(thing);
  EXPECT_TRUE(target);

  thing->Shutdown();
}

TEST(TestEventTargetQI, SchedulerGroup)
{
  nsCOMPtr<nsIEventTarget> thing = SystemGroup::EventTargetFor(TaskCategory::Other);
  EXPECT_TRUE(thing);

  nsCOMPtr<nsISerialEventTarget> serial = do_QueryInterface(thing);
  EXPECT_TRUE(serial);

  nsCOMPtr<nsIEventTarget> target = do_QueryInterface(thing);
  EXPECT_TRUE(target);
}
