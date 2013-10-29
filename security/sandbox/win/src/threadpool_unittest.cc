// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "sandbox/win/src/win2k_threadpool.h"
#include "testing/gtest/include/gtest/gtest.h"

void __stdcall EmptyCallBack(void*, unsigned char) {
}

void __stdcall TestCallBack(void* context, unsigned char) {
  HANDLE event = reinterpret_cast<HANDLE>(context);
  ::SetEvent(event);
}

namespace sandbox {

// Test that register and unregister work, part 1.
TEST(IPCTest, ThreadPoolRegisterTest1) {
  Win2kThreadPool thread_pool;

  EXPECT_EQ(0, thread_pool.OutstandingWaits());

  HANDLE event1 = ::CreateEventW(NULL, FALSE, FALSE, NULL);
  HANDLE event2 = ::CreateEventW(NULL, FALSE, FALSE, NULL);

  uint32 context = 0;
  EXPECT_FALSE(thread_pool.RegisterWait(0, event1, EmptyCallBack, &context));
  EXPECT_EQ(0, thread_pool.OutstandingWaits());

  EXPECT_TRUE(thread_pool.RegisterWait(this, event1, EmptyCallBack, &context));
  EXPECT_EQ(1, thread_pool.OutstandingWaits());
  EXPECT_TRUE(thread_pool.RegisterWait(this, event2, EmptyCallBack, &context));
  EXPECT_EQ(2, thread_pool.OutstandingWaits());

  EXPECT_TRUE(thread_pool.UnRegisterWaits(this));
  EXPECT_EQ(0, thread_pool.OutstandingWaits());

  EXPECT_EQ(TRUE, ::CloseHandle(event1));
  EXPECT_EQ(TRUE, ::CloseHandle(event2));
}

// Test that register and unregister work, part 2.
TEST(IPCTest, ThreadPoolRegisterTest2) {
  Win2kThreadPool thread_pool;

  HANDLE event1 = ::CreateEventW(NULL, FALSE, FALSE, NULL);
  HANDLE event2 = ::CreateEventW(NULL, FALSE, FALSE, NULL);

  uint32 context = 0;
  uint32 c1 = 0;
  uint32 c2 = 0;

  EXPECT_TRUE(thread_pool.RegisterWait(&c1, event1, EmptyCallBack, &context));
  EXPECT_EQ(1, thread_pool.OutstandingWaits());
  EXPECT_TRUE(thread_pool.RegisterWait(&c2, event2, EmptyCallBack, &context));
  EXPECT_EQ(2, thread_pool.OutstandingWaits());

  EXPECT_TRUE(thread_pool.UnRegisterWaits(&c2));
  EXPECT_EQ(1, thread_pool.OutstandingWaits());
  EXPECT_TRUE(thread_pool.UnRegisterWaits(&c2));
  EXPECT_EQ(1, thread_pool.OutstandingWaits());

  EXPECT_TRUE(thread_pool.UnRegisterWaits(&c1));
  EXPECT_EQ(0, thread_pool.OutstandingWaits());

  EXPECT_EQ(TRUE, ::CloseHandle(event1));
  EXPECT_EQ(TRUE, ::CloseHandle(event2));
}

// Test that the thread pool has at least a thread that services an event.
// Test that when the event is un-registered is no longer serviced.
TEST(IPCTest, ThreadPoolSignalAndWaitTest) {
  Win2kThreadPool thread_pool;

  // The events are auto reset and start not signaled.
  HANDLE event1 = ::CreateEventW(NULL, FALSE, FALSE, NULL);
  HANDLE event2 = ::CreateEventW(NULL, FALSE, FALSE, NULL);

  EXPECT_TRUE(thread_pool.RegisterWait(this, event1, TestCallBack, event2));

  EXPECT_EQ(WAIT_OBJECT_0, ::SignalObjectAndWait(event1, event2, 5000, FALSE));
  EXPECT_EQ(WAIT_OBJECT_0, ::SignalObjectAndWait(event1, event2, 5000, FALSE));

  EXPECT_TRUE(thread_pool.UnRegisterWaits(this));
  EXPECT_EQ(0, thread_pool.OutstandingWaits());

  EXPECT_EQ(WAIT_TIMEOUT, ::SignalObjectAndWait(event1, event2, 1000, FALSE));

  EXPECT_EQ(TRUE, ::CloseHandle(event1));
  EXPECT_EQ(TRUE, ::CloseHandle(event2));
}

}  // namespace sandbox
