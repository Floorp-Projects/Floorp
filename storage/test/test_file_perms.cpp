/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "storage_test_harness.h"
#include "nsIFile.h"
#include "prio.h"

/**
 * This file tests that the file permissions of the sqlite files match what
 * we request they be
 */

void
test_file_perms()
{
  nsCOMPtr<mozIStorageConnection> db(getDatabase());
  nsCOMPtr<nsIFile> dbFile;
  do_check_success(db->GetDatabaseFile(getter_AddRefs(dbFile)));

  uint32_t perms = 0;
  do_check_success(dbFile->GetPermissions(&perms));

  // This reflexts the permissions defined by SQLITE_DEFAULT_FILE_PERMISSIONS in
  // db/sqlite3/src/Makefile.in and must be kept in sync with that
#ifdef ANDROID
  do_check_true(perms == (PR_IRUSR | PR_IWUSR));
#elif defined(XP_WIN)
  do_check_true(perms == (PR_IRUSR | PR_IWUSR | PR_IRGRP | PR_IWGRP | PR_IROTH | PR_IWOTH));
#else
  do_check_true(perms == (PR_IRUSR | PR_IWUSR | PR_IRGRP | PR_IROTH));
#endif
}

void (*gTests[])(void) = {
  test_file_perms,
};

const char *file = __FILE__;
#define TEST_NAME "file perms"
#define TEST_FILE file
#include "storage_test_harness_tail.h"
