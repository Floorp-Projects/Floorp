/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <set>
#include <vector>

#include "webrtc/base/criticalsection.h"
#include "webrtc/base/event.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/scopedptrcollection.h"
#include "webrtc/base/thread.h"
#include "webrtc/test/testsupport/gtest_disable.h"

namespace rtc {

namespace {

const int kLongTime = 10000;  // 10 seconds
const int kNumThreads = 16;
const int kOperationsToRun = 1000;

template <class T>
class AtomicOpRunner : public MessageHandler {
 public:
  explicit AtomicOpRunner(int initial_value)
      : value_(initial_value),
        threads_active_(0),
        start_event_(true, false),
        done_event_(true, false) {}

  int value() const { return value_; }

  bool Run() {
    // Signal all threads to start.
    start_event_.Set();

    // Wait for all threads to finish.
    return done_event_.Wait(kLongTime);
  }

  void SetExpectedThreadCount(int count) {
    threads_active_ = count;
  }

  virtual void OnMessage(Message* msg) {
    std::vector<int> values;
    values.reserve(kOperationsToRun);

    // Wait to start.
    ASSERT_TRUE(start_event_.Wait(kLongTime));

    // Generate a bunch of values by updating value_ atomically.
    for (int i = 0; i < kOperationsToRun; ++i) {
      values.push_back(T::AtomicOp(&value_));
    }

    { // Add them all to the set.
      CritScope cs(&all_values_crit_);
      for (size_t i = 0; i < values.size(); ++i) {
        std::pair<std::set<int>::iterator, bool> result =
            all_values_.insert(values[i]);
        // Each value should only be taken by one thread, so if this value
        // has already been added, something went wrong.
        EXPECT_TRUE(result.second)
            << "Thread=" << Thread::Current() << " value=" << values[i];
      }
    }

    // Signal that we're done.
    if (AtomicOps::Decrement(&threads_active_) == 0) {
      done_event_.Set();
    }
  }

 private:
  int value_;
  int threads_active_;
  CriticalSection all_values_crit_;
  std::set<int> all_values_;
  Event start_event_;
  Event done_event_;
};

struct IncrementOp {
  static int AtomicOp(int* i) { return AtomicOps::Increment(i); }
};

struct DecrementOp {
  static int AtomicOp(int* i) { return AtomicOps::Decrement(i); }
};

void StartThreads(ScopedPtrCollection<Thread>* threads,
                  MessageHandler* handler) {
  for (int i = 0; i < kNumThreads; ++i) {
    Thread* thread = new Thread();
    thread->Start();
    thread->Post(handler);
    threads->PushBack(thread);
  }
}

}  // namespace

TEST(AtomicOpsTest, Simple) {
  int value = 0;
  EXPECT_EQ(1, AtomicOps::Increment(&value));
  EXPECT_EQ(1, value);
  EXPECT_EQ(2, AtomicOps::Increment(&value));
  EXPECT_EQ(2, value);
  EXPECT_EQ(1, AtomicOps::Decrement(&value));
  EXPECT_EQ(1, value);
  EXPECT_EQ(0, AtomicOps::Decrement(&value));
  EXPECT_EQ(0, value);
}

TEST(AtomicOpsTest, Increment) {
  // Create and start lots of threads.
  AtomicOpRunner<IncrementOp> runner(0);
  ScopedPtrCollection<Thread> threads;
  StartThreads(&threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);

  // Release the hounds!
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(kOperationsToRun * kNumThreads, runner.value());
}

TEST(AtomicOpsTest, Decrement) {
  // Create and start lots of threads.
  AtomicOpRunner<DecrementOp> runner(kOperationsToRun * kNumThreads);
  ScopedPtrCollection<Thread> threads;
  StartThreads(&threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);

  // Release the hounds!
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(0, runner.value());
}

}  // namespace rtc
