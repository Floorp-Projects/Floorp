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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Brad Lassey <brad@lassey.us>
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
#include "nsILocalFile.h"

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

  nsCOMPtr<nsILocalFile> localFile = do_QueryInterface(dbFile);
  do_check_true(localFile);

  PRUint32 perms = 0;
  do_check_success(localFile->GetPermissions(&perms));

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
