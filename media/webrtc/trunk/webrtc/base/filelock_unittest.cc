/*
 *  Copyright 2009 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <string>

#include "webrtc/base/event.h"
#include "webrtc/base/filelock.h"
#include "webrtc/base/fileutils.h"
#include "webrtc/base/gunit.h"
#include "webrtc/base/pathutils.h"
#include "webrtc/base/scoped_ptr.h"
#include "webrtc/base/thread.h"
#include "webrtc/test/testsupport/gtest_disable.h"

namespace rtc {

const static std::string kLockFile = "TestLockFile";
const static int kTimeoutMS = 5000;

class FileLockTest : public testing::Test, public Runnable {
 public:
  FileLockTest() : done_(false, false), thread_lock_failed_(false) {
  }

  virtual void Run(Thread* t) {
    scoped_ptr<FileLock> lock(FileLock::TryLock(temp_file_.pathname()));
    // The lock is already owned by the main thread of
    // this test, therefore the TryLock(...) call should fail.
    thread_lock_failed_ = lock.get() == NULL;
    done_.Set();
  }

 protected:
  virtual void SetUp() {
    thread_lock_failed_ = false;
    Pathname temp_dir;
    Filesystem::GetAppTempFolder(&temp_dir);
    temp_file_.SetPathname(rtc::Filesystem::TempFilename(temp_dir, kLockFile));
  }

  void LockOnThread() {
    locker_.Start(this);
    done_.Wait(kTimeoutMS);
  }

  Event done_;
  Thread locker_;
  bool thread_lock_failed_;
  Pathname temp_file_;
};

TEST_F(FileLockTest, TestLockFileDeleted) {
  scoped_ptr<FileLock> lock(FileLock::TryLock(temp_file_.pathname()));
  EXPECT_TRUE(lock.get() != NULL);
  EXPECT_FALSE(Filesystem::IsAbsent(temp_file_.pathname()));
  lock->Unlock();
  EXPECT_TRUE(Filesystem::IsAbsent(temp_file_.pathname()));
}

TEST_F(FileLockTest, TestLock) {
  scoped_ptr<FileLock> lock(FileLock::TryLock(temp_file_.pathname()));
  EXPECT_TRUE(lock.get() != NULL);
}

TEST_F(FileLockTest, TestLockX2) {
  scoped_ptr<FileLock> lock1(FileLock::TryLock(temp_file_.pathname()));
  EXPECT_TRUE(lock1.get() != NULL);

  scoped_ptr<FileLock> lock2(FileLock::TryLock(temp_file_.pathname()));
  EXPECT_TRUE(lock2.get() == NULL);
}

TEST_F(FileLockTest, TestThreadedLock) {
  scoped_ptr<FileLock> lock(FileLock::TryLock(temp_file_.pathname()));
  EXPECT_TRUE(lock.get() != NULL);

  LockOnThread();
  EXPECT_TRUE(thread_lock_failed_);
}

}  // namespace rtc
