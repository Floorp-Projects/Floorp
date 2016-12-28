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
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/scopedptrcollection.h"
#include "webrtc/base/thread.h"

namespace rtc {

namespace {

const int kLongTime = 10000;  // 10 seconds
const int kNumThreads = 16;
const int kOperationsToRun = 1000;

class UniqueValueVerifier {
 public:
  void Verify(const std::vector<int>& values) {
    for (size_t i = 0; i < values.size(); ++i) {
      std::pair<std::set<int>::iterator, bool> result =
          all_values_.insert(values[i]);
      // Each value should only be taken by one thread, so if this value
      // has already been added, something went wrong.
      EXPECT_TRUE(result.second)
          << " Thread=" << Thread::Current() << " value=" << values[i];
    }
  }

  void Finalize() {}

 private:
  std::set<int> all_values_;
};

class CompareAndSwapVerifier {
 public:
  CompareAndSwapVerifier() : zero_count_(0) {}

  void Verify(const std::vector<int>& values) {
    for (auto v : values) {
      if (v == 0) {
        EXPECT_EQ(0, zero_count_) << "Thread=" << Thread::Current();
        ++zero_count_;
      } else {
        EXPECT_EQ(1, v) << " Thread=" << Thread::Current();
      }
    }
  }

  void Finalize() {
    EXPECT_EQ(1, zero_count_);
  }
 private:
  int zero_count_;
};

class RunnerBase : public MessageHandler {
 public:
  explicit RunnerBase(int value)
      : threads_active_(0),
        start_event_(true, false),
        done_event_(true, false),
        shared_value_(value) {}

  bool Run() {
    // Signal all threads to start.
    start_event_.Set();

    // Wait for all threads to finish.
    return done_event_.Wait(kLongTime);
  }

  void SetExpectedThreadCount(int count) {
    threads_active_ = count;
  }

  int shared_value() const { return shared_value_; }

 protected:
  // Derived classes must override OnMessage, and call BeforeStart and AfterEnd
  // at the beginning and the end of OnMessage respectively.
  void BeforeStart() {
    ASSERT_TRUE(start_event_.Wait(kLongTime));
  }

  // Returns true if all threads have finished.
  bool AfterEnd() {
    if (AtomicOps::Decrement(&threads_active_) == 0) {
      done_event_.Set();
      return true;
    }
    return false;
  }

  int threads_active_;
  Event start_event_;
  Event done_event_;
  int shared_value_;
};

class LOCKABLE CriticalSectionLock {
 public:
  void Lock() EXCLUSIVE_LOCK_FUNCTION() {
    cs_.Enter();
  }
  void Unlock() UNLOCK_FUNCTION() {
    cs_.Leave();
  }

 private:
  CriticalSection cs_;
};

template <class Lock>
class LockRunner : public RunnerBase {
 public:
  LockRunner() : RunnerBase(0) {}

  void OnMessage(Message* msg) override {
    BeforeStart();

    lock_.Lock();

    EXPECT_EQ(0, shared_value_);
    int old = shared_value_;

    // Use a loop to increase the chance of race.
    for (int i = 0; i < kOperationsToRun; ++i) {
      ++shared_value_;
    }
    EXPECT_EQ(old + kOperationsToRun, shared_value_);
    shared_value_ = 0;

    lock_.Unlock();

    AfterEnd();
  }

 private:
  Lock lock_;
};

template <class Op, class Verifier>
class AtomicOpRunner : public RunnerBase {
 public:
  explicit AtomicOpRunner(int initial_value) : RunnerBase(initial_value) {}

  void OnMessage(Message* msg) override {
    BeforeStart();

    std::vector<int> values;
    values.reserve(kOperationsToRun);

    // Generate a bunch of values by updating shared_value_ atomically.
    for (int i = 0; i < kOperationsToRun; ++i) {
      values.push_back(Op::AtomicOp(&shared_value_));
    }

    { // Add them all to the set.
      CritScope cs(&all_values_crit_);
      verifier_.Verify(values);
    }

    if (AfterEnd()) {
      verifier_.Finalize();
    }
  }

 private:
  CriticalSection all_values_crit_;
  Verifier verifier_;
};

struct IncrementOp {
  static int AtomicOp(int* i) { return AtomicOps::Increment(i); }
};

struct DecrementOp {
  static int AtomicOp(int* i) { return AtomicOps::Decrement(i); }
};

struct CompareAndSwapOp {
  static int AtomicOp(int* i) { return AtomicOps::CompareAndSwap(i, 0, 1); }
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

TEST(AtomicOpsTest, SimplePtr) {
  class Foo {};
  Foo* volatile foo = nullptr;
  scoped_ptr<Foo> a(new Foo());
  scoped_ptr<Foo> b(new Foo());
  // Reading the initial value should work as expected.
  EXPECT_TRUE(rtc::AtomicOps::AcquireLoadPtr(&foo) == nullptr);
  // Setting using compare and swap should work.
  EXPECT_TRUE(rtc::AtomicOps::CompareAndSwapPtr(
                  &foo, static_cast<Foo*>(nullptr), a.get()) == nullptr);
  EXPECT_TRUE(rtc::AtomicOps::AcquireLoadPtr(&foo) == a.get());
  // Setting another value but with the wrong previous pointer should fail
  // (remain a).
  EXPECT_TRUE(rtc::AtomicOps::CompareAndSwapPtr(
                  &foo, static_cast<Foo*>(nullptr), b.get()) == a.get());
  EXPECT_TRUE(rtc::AtomicOps::AcquireLoadPtr(&foo) == a.get());
  // Replacing a with b should work.
  EXPECT_TRUE(rtc::AtomicOps::CompareAndSwapPtr(&foo, a.get(), b.get()) ==
              a.get());
  EXPECT_TRUE(rtc::AtomicOps::AcquireLoadPtr(&foo) == b.get());
}

TEST(AtomicOpsTest, Increment) {
  // Create and start lots of threads.
  AtomicOpRunner<IncrementOp, UniqueValueVerifier> runner(0);
  ScopedPtrCollection<Thread> threads;
  StartThreads(&threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);

  // Release the hounds!
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(kOperationsToRun * kNumThreads, runner.shared_value());
}

TEST(AtomicOpsTest, Decrement) {
  // Create and start lots of threads.
  AtomicOpRunner<DecrementOp, UniqueValueVerifier> runner(
      kOperationsToRun * kNumThreads);
  ScopedPtrCollection<Thread> threads;
  StartThreads(&threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);

  // Release the hounds!
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(0, runner.shared_value());
}

TEST(AtomicOpsTest, CompareAndSwap) {
  // Create and start lots of threads.
  AtomicOpRunner<CompareAndSwapOp, CompareAndSwapVerifier> runner(0);
  ScopedPtrCollection<Thread> threads;
  StartThreads(&threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);

  // Release the hounds!
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(1, runner.shared_value());
}

TEST(GlobalLockTest, Basic) {
  // Create and start lots of threads.
  LockRunner<GlobalLock> runner;
  ScopedPtrCollection<Thread> threads;
  StartThreads(&threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);

  // Release the hounds!
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(0, runner.shared_value());
}

TEST(CriticalSectionTest, Basic) {
  // Create and start lots of threads.
  LockRunner<CriticalSectionLock> runner;
  ScopedPtrCollection<Thread> threads;
  StartThreads(&threads, &runner);
  runner.SetExpectedThreadCount(kNumThreads);

  // Release the hounds!
  EXPECT_TRUE(runner.Run());
  EXPECT_EQ(0, runner.shared_value());
}

#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
TEST(CriticalSectionTest, IsLocked) {
  // Simple single-threaded test of IsLocked.
  CriticalSection cs;
  EXPECT_FALSE(cs.IsLocked());
  cs.Enter();
  EXPECT_TRUE(cs.IsLocked());
  cs.Leave();
  EXPECT_FALSE(cs.IsLocked());
  if (!cs.TryEnter())
    FAIL();
  EXPECT_TRUE(cs.IsLocked());
  cs.Leave();
  EXPECT_FALSE(cs.IsLocked());
}
#endif

}  // namespace rtc
