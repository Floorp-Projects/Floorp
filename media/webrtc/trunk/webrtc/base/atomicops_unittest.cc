/*
 *  Copyright 2011 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#if !defined(__arm__)
// For testing purposes, define faked versions of the atomic operations
#include "webrtc/base/basictypes.h"
namespace rtc {
typedef uint32 Atomic32;
static inline void MemoryBarrier() { }
static inline void AtomicIncrement(volatile Atomic32* ptr) {
  *ptr = *ptr + 1;
}
}
#define SKIP_ATOMIC_CHECK
#endif

#include "webrtc/base/atomicops.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/helpers.h"
#include "webrtc/base/logging.h"

TEST(FixedSizeLockFreeQueueTest, TestDefaultConstruct) {
  rtc::FixedSizeLockFreeQueue<int> queue;
  EXPECT_EQ(0u, queue.capacity());
  EXPECT_EQ(0u, queue.Size());
  EXPECT_FALSE(queue.PushBack(1));
  int val;
  EXPECT_FALSE(queue.PopFront(&val));
}

TEST(FixedSizeLockFreeQueueTest, TestConstruct) {
  rtc::FixedSizeLockFreeQueue<int> queue(5);
  EXPECT_EQ(5u, queue.capacity());
  EXPECT_EQ(0u, queue.Size());
  int val;
  EXPECT_FALSE(queue.PopFront(&val));
}

TEST(FixedSizeLockFreeQueueTest, TestPushPop) {
  rtc::FixedSizeLockFreeQueue<int> queue(2);
  EXPECT_EQ(2u, queue.capacity());
  EXPECT_EQ(0u, queue.Size());
  EXPECT_TRUE(queue.PushBack(1));
  EXPECT_EQ(1u, queue.Size());
  EXPECT_TRUE(queue.PushBack(2));
  EXPECT_EQ(2u, queue.Size());
  EXPECT_FALSE(queue.PushBack(3));
  EXPECT_EQ(2u, queue.Size());
  int val;
  EXPECT_TRUE(queue.PopFront(&val));
  EXPECT_EQ(1, val);
  EXPECT_EQ(1u, queue.Size());
  EXPECT_TRUE(queue.PopFront(&val));
  EXPECT_EQ(2, val);
  EXPECT_EQ(0u, queue.Size());
  EXPECT_FALSE(queue.PopFront(&val));
  EXPECT_EQ(0u, queue.Size());
}

TEST(FixedSizeLockFreeQueueTest, TestResize) {
  rtc::FixedSizeLockFreeQueue<int> queue(2);
  EXPECT_EQ(2u, queue.capacity());
  EXPECT_EQ(0u, queue.Size());
  EXPECT_TRUE(queue.PushBack(1));
  EXPECT_EQ(1u, queue.Size());

  queue.ClearAndResizeUnsafe(5);
  EXPECT_EQ(5u, queue.capacity());
  EXPECT_EQ(0u, queue.Size());
  int val;
  EXPECT_FALSE(queue.PopFront(&val));
}
