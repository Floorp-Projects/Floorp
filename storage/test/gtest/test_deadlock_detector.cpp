/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=2 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Note: This file is essentially a copy of
// xpcom/tests/gtest/TestDeadlockDetector.cpp, but all mutexes were turned into
// SQLiteMutexes. We use #include and some macros to avoid actual source code
// duplication.

#include "mozilla/CondVar.h"
#include "mozilla/RecursiveMutex.h"
#include "mozilla/ReentrantMonitor.h"
#include "SQLiteMutex.h"

// We need this one so _gdb_sleep_duration is also in "storage" namespace
#include "mozilla/gtest/MozHelpers.h"

#include "gtest/gtest.h"

using namespace mozilla;

/**
 * Helper class to allocate a sqlite3_mutex for our SQLiteMutex.
 */
class TestMutex : public mozilla::storage::SQLiteMutex {
 public:
  explicit TestMutex(const char* aName)
      : mozilla::storage::SQLiteMutex(aName),
        mInner(sqlite3_mutex_alloc(SQLITE_MUTEX_FAST)) {
    NS_ASSERTION(mInner, "could not allocate a sqlite3_mutex");
    initWithMutex(mInner);
  }

  ~TestMutex() { sqlite3_mutex_free(mInner); }

  void Lock() { lock(); }

  void Unlock() { unlock(); }

 private:
  sqlite3_mutex* mInner;
};

// These are the two macros that differentiate this file from the XPCOM one.
#define MUTEX TestMutex
#define TESTNAME(name) storage_##name

// Bug 1473531: the test storage_DeadlockDetectorTest.storage_Sanity5DeathTest
// times out on macosx ccov builds
#if defined(XP_MACOSX) && defined(MOZ_CODE_COVERAGE)
#  define DISABLE_STORAGE_SANITY5_DEATH_TEST
#endif

#include "../../../xpcom/tests/gtest/TestDeadlockDetector.cpp"
