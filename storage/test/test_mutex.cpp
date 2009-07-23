/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is storage test code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Shawn Wilsher <me@shawnwilsher.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "storage_test_harness.h"

#include "SQLiteMutex.h"

using namespace mozilla::storage;

/**
 * This file test our sqlite3_mutex wrapper in SQLiteMutex.h.
 */

void
test_AutoLock()
{
  int lockTypes[] = {
    SQLITE_MUTEX_FAST,
    SQLITE_MUTEX_RECURSIVE,
  };
  for (size_t i = 0; i < NS_ARRAY_LENGTH(lockTypes); i++) {
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

void
test_AutoUnlock()
{
  int lockTypes[] = {
    SQLITE_MUTEX_FAST,
    SQLITE_MUTEX_RECURSIVE,
  };
  for (size_t i = 0; i < NS_ARRAY_LENGTH(lockTypes); i++) {
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

void (*gTests[])(void) = {
  test_AutoLock,
  test_AutoUnlock,
};

const char *file = __FILE__;
#define TEST_NAME "SQLiteMutex"
#define TEST_FILE file
#include "storage_test_harness_tail.h"
