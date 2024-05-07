/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "gtest/gtest.h"

#include "storage_test_harness.h"

#include "mozilla/Atomics.h"
#include "mozilla/SpinEventLoopUntil.h"

class SynchronousConnectionInterruptionTest : public ::testing::Test {
 protected:
  void SetUp() override {
    mConnection =
        getDatabase(nullptr, mozIStorageService::CONNECTION_INTERRUPTIBLE);
    ASSERT_TRUE(mConnection);

    ASSERT_EQ(NS_OK, NS_NewNamedThread("Test Thread", getter_AddRefs(mThread)));
  }

  void TearDown() override {
    // We might close the database connection early in test cases.
    mozilla::Unused << mConnection->Close();

    ASSERT_EQ(NS_OK, mThread->Shutdown());
  }

  nsCOMPtr<mozIStorageConnection> mConnection;

  nsCOMPtr<nsIThread> mThread;

  mozilla::Atomic<nsresult> mRv = mozilla::Atomic<nsresult>(NS_ERROR_FAILURE);

  mozilla::Atomic<bool> mDone{false};
};

TEST_F(SynchronousConnectionInterruptionTest,
       shouldBeAbleToInterruptInfiniteOperation) {
  // Delay is modest because we don't want to get interrupted by
  // some unrelated hang or memory guard
  const uint32_t delayMs = 500;

  ASSERT_EQ(NS_OK, mThread->DelayedDispatch(
                       NS_NewRunnableFunction("InterruptRunnable",
                                              [this]() {
                                                mRv = mConnection->Interrupt();
                                                mDone = true;
                                              }),
                       delayMs));

  const nsCString infiniteQuery =
      "WITH RECURSIVE test(n) "
      "AS (VALUES(1) UNION ALL SELECT n + 1 FROM test) "
      "SELECT t.n FROM test, test AS t;"_ns;
  nsCOMPtr<mozIStorageStatement> stmt;
  ASSERT_EQ(NS_OK,
            mConnection->CreateStatement(infiniteQuery, getter_AddRefs(stmt)));

  ASSERT_EQ(NS_ERROR_ABORT, stmt->Execute());
  ASSERT_EQ(NS_OK, stmt->Finalize());

  ASSERT_TRUE(mDone);
  ASSERT_EQ(NS_OK, mRv);

  ASSERT_EQ(NS_OK, mConnection->Close());
}

TEST_F(SynchronousConnectionInterruptionTest, interruptAfterCloseWillFail) {
  ASSERT_EQ(NS_OK, mConnection->Close());

  ASSERT_EQ(NS_OK, mThread->Dispatch(
                       NS_NewRunnableFunction("InterruptRunnable", [this]() {
                         mRv = mConnection->Interrupt();
                         mDone = true;
                       })));

  ASSERT_TRUE(mozilla::SpinEventLoopUntil("interruptAfterCloseWillFail"_ns,
                                          [this]() -> bool { return mDone; }));

  ASSERT_EQ(NS_ERROR_NOT_INITIALIZED, mRv);

  ASSERT_EQ(NS_ERROR_NOT_INITIALIZED, mConnection->Close());
}
