/*
 *  Copyright (c) 2014 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

// Borrowed from Chromium's src/base/threading/thread_checker_unittest.cc.

#include "testing/gtest/include/gtest/gtest.h"
#include "webrtc/base/checks.h"
#include "webrtc/base/thread.h"
#include "webrtc/base/thread_checker.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/test/testsupport/gtest_disable.h"

// Duplicated from base/threading/thread_checker.h so that we can be
// good citizens there and undef the macro.
#if !defined(NDEBUG) || defined(DCHECK_ALWAYS_ON)
#define ENABLE_THREAD_CHECKER 1
#else
#define ENABLE_THREAD_CHECKER 0
#endif

namespace rtc {

namespace {

// Simple class to exercise the basics of ThreadChecker.
// Both the destructor and DoStuff should verify that they were
// called on the same thread as the constructor.
class ThreadCheckerClass : public ThreadChecker {
 public:
  ThreadCheckerClass() {}

  // Verifies that it was called on the same thread as the constructor.
  void DoStuff() {
    DCHECK(CalledOnValidThread());
  }

  void DetachFromThread() {
    ThreadChecker::DetachFromThread();
  }

  static void MethodOnDifferentThreadImpl();
  static void DetachThenCallFromDifferentThreadImpl();

 private:
  DISALLOW_COPY_AND_ASSIGN(ThreadCheckerClass);
};

// Calls ThreadCheckerClass::DoStuff on another thread.
class CallDoStuffOnThread : public Thread {
 public:
  explicit CallDoStuffOnThread(ThreadCheckerClass* thread_checker_class)
      : Thread(),
        thread_checker_class_(thread_checker_class) {
    SetName("call_do_stuff_on_thread", NULL);
  }

  virtual void Run() OVERRIDE {
    thread_checker_class_->DoStuff();
  }

  // New method. Needed since Thread::Join is protected, and it is called by
  // the TEST.
  void Join() {
    Thread::Join();
  }

 private:
  ThreadCheckerClass* thread_checker_class_;

  DISALLOW_COPY_AND_ASSIGN(CallDoStuffOnThread);
};

// Deletes ThreadCheckerClass on a different thread.
class DeleteThreadCheckerClassOnThread : public Thread {
 public:
  explicit DeleteThreadCheckerClassOnThread(
      ThreadCheckerClass* thread_checker_class)
      : Thread(),
        thread_checker_class_(thread_checker_class) {
    SetName("delete_thread_checker_class_on_thread", NULL);
  }

  virtual void Run() OVERRIDE {
    thread_checker_class_.reset();
  }

  // New method. Needed since Thread::Join is protected, and it is called by
  // the TEST.
  void Join() {
    Thread::Join();
  }

 private:
  scoped_ptr<ThreadCheckerClass> thread_checker_class_;

  DISALLOW_COPY_AND_ASSIGN(DeleteThreadCheckerClassOnThread);
};

}  // namespace

TEST(ThreadCheckerTest, CallsAllowedOnSameThread) {
  scoped_ptr<ThreadCheckerClass> thread_checker_class(
      new ThreadCheckerClass);

  // Verify that DoStuff doesn't assert.
  thread_checker_class->DoStuff();

  // Verify that the destructor doesn't assert.
  thread_checker_class.reset();
}

TEST(ThreadCheckerTest, DestructorAllowedOnDifferentThread) {
  scoped_ptr<ThreadCheckerClass> thread_checker_class(
      new ThreadCheckerClass);

  // Verify that the destructor doesn't assert
  // when called on a different thread.
  DeleteThreadCheckerClassOnThread delete_on_thread(
      thread_checker_class.release());

  delete_on_thread.Start();
  delete_on_thread.Join();
}

TEST(ThreadCheckerTest, DetachFromThread) {
  scoped_ptr<ThreadCheckerClass> thread_checker_class(
      new ThreadCheckerClass);

  // Verify that DoStuff doesn't assert when called on a different thread after
  // a call to DetachFromThread.
  thread_checker_class->DetachFromThread();
  CallDoStuffOnThread call_on_thread(thread_checker_class.get());

  call_on_thread.Start();
  call_on_thread.Join();
}

#if GTEST_HAS_DEATH_TEST || !ENABLE_THREAD_CHECKER

void ThreadCheckerClass::MethodOnDifferentThreadImpl() {
  scoped_ptr<ThreadCheckerClass> thread_checker_class(
      new ThreadCheckerClass);

  // DoStuff should assert in debug builds only when called on a
  // different thread.
  CallDoStuffOnThread call_on_thread(thread_checker_class.get());

  call_on_thread.Start();
  call_on_thread.Join();
}

#if ENABLE_THREAD_CHECKER
TEST(ThreadCheckerDeathTest, MethodNotAllowedOnDifferentThreadInDebug) {
  ASSERT_DEATH({
      ThreadCheckerClass::MethodOnDifferentThreadImpl();
    }, "");
}
#else
TEST(ThreadCheckerTest, MethodAllowedOnDifferentThreadInRelease) {
  ThreadCheckerClass::MethodOnDifferentThreadImpl();
}
#endif  // ENABLE_THREAD_CHECKER

void ThreadCheckerClass::DetachThenCallFromDifferentThreadImpl() {
  scoped_ptr<ThreadCheckerClass> thread_checker_class(
      new ThreadCheckerClass);

  // DoStuff doesn't assert when called on a different thread
  // after a call to DetachFromThread.
  thread_checker_class->DetachFromThread();
  CallDoStuffOnThread call_on_thread(thread_checker_class.get());

  call_on_thread.Start();
  call_on_thread.Join();

  // DoStuff should assert in debug builds only after moving to
  // another thread.
  thread_checker_class->DoStuff();
}

#if ENABLE_THREAD_CHECKER
TEST(ThreadCheckerDeathTest, DetachFromThreadInDebug) {
  ASSERT_DEATH({
    ThreadCheckerClass::DetachThenCallFromDifferentThreadImpl();
    }, "");
}
#else
TEST(ThreadCheckerTest, DetachFromThreadInRelease) {
  ThreadCheckerClass::DetachThenCallFromDifferentThreadImpl();
}
#endif  // ENABLE_THREAD_CHECKER

#endif  // GTEST_HAS_DEATH_TEST || !ENABLE_THREAD_CHECKER

// Just in case we ever get lumped together with other compilation units.
#undef ENABLE_THREAD_CHECKER

}  // namespace rtc
