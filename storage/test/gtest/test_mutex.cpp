/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"

#include "SQLiteMutex.h"

using namespace mozilla;
using namespace mozilla::storage;

/**
 * This file test our sqlite3_mutex wrapper in SQLiteMutex.h.
 */

TEST(storage_mutex, AutoLock)
{
  int lockTypes[] = {
    SQLITE_MUTEX_FAST,
    SQLITE_MUTEX_RECURSIVE,
  };
  for (size_t i = 0; i < ArrayLength(lockTypes); i++) {
    // Get our test mutex (we have to allocate a SQLite mutex of the right type
    // too!).
    SQLiteMutex mutex("TestMutex");
    sqlite3_mutex *inner = sqlite3_mutex_alloc(lockTypes[i]);
    do_check_true(inner);
    mutex.initWithMutex(inner);

    // And test that our automatic locking wrapper works as expected.
    mutex.assertNotCurrentThreadOwns();
    {
      SQLiteMutexAutoLock lockedScope(mutex);
      mutex.assertCurrentThreadOwns();
    }
    mutex.assertNotCurrentThreadOwns();

    // Free the wrapped mutex - we don't need it anymore.
    sqlite3_mutex_free(inner);
  }
}

TEST(storage_mutex, AutoUnlock)
{
  int lockTypes[] = {
    SQLITE_MUTEX_FAST,
    SQLITE_MUTEX_RECURSIVE,
  };
  for (size_t i = 0; i < ArrayLength(lockTypes); i++) {
    // Get our test mutex (we have to allocate a SQLite mutex of the right type
    // too!).
    SQLiteMutex mutex("TestMutex");
    sqlite3_mutex *inner = sqlite3_mutex_alloc(lockTypes[i]);
    do_check_true(inner);
    mutex.initWithMutex(inner);

    // And test that our automatic unlocking wrapper works as expected.
    {
      SQLiteMutexAutoLock lockedScope(mutex);

      {
        SQLiteMutexAutoUnlock unlockedScope(mutex);
        mutex.assertNotCurrentThreadOwns();
      }
      mutex.assertCurrentThreadOwns();
    }

    // Free the wrapped mutex - we don't need it anymore.
    sqlite3_mutex_free(inner);
  }
}

