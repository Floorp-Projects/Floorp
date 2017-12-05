/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the functions of mozIStorageService except for
// openUnsharedDatabase, which is tested by test_storage_service_unshared.js.

const BACKUP_FILE_NAME = "test_storage.sqlite.backup";

function test_openSpecialDatabase_invalid_arg() {
  try {
    Services.storage.openSpecialDatabase("abcd");
    do_throw("We should not get here!");
  } catch (e) {
    print(e);
    print("e.result is " + e.result);
    do_check_eq(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
}

function test_openDatabase_null_file() {
  try {
    Services.storage.openDatabase(null);
    do_throw("We should not get here!");
  } catch (e) {
    print(e);
    print("e.result is " + e.result);
    do_check_eq(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
}

function test_openUnsharedDatabase_null_file() {
  try {
    Services.storage.openUnsharedDatabase(null);
    do_throw("We should not get here!");
  } catch (e) {
    print(e);
    print("e.result is " + e.result);
    do_check_eq(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
}

function test_openDatabase_file_DNE() {
  // the file should be created after calling
  var db = getTestDB();
  do_check_false(db.exists());
  Services.storage.openDatabase(db);
  do_check_true(db.exists());
}

function test_openDatabase_file_exists() {
  // it should already exist from our last test
  var db = getTestDB();
  do_check_true(db.exists());
  Services.storage.openDatabase(db);
  do_check_true(db.exists());
}

function test_corrupt_db_throws_with_openDatabase() {
  try {
    getDatabase(getCorruptDB());
    do_throw("should not be here");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_FILE_CORRUPTED, e.result);
  }
}

function test_fake_db_throws_with_openDatabase() {
  try {
    getDatabase(getFakeDB());
    do_throw("should not be here");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_FILE_CORRUPTED, e.result);
  }
}

function test_backup_not_new_filename() {
  const fname = getTestDB().leafName;

  var backup = Services.storage.backupDatabaseFile(getTestDB(), fname);
  do_check_neq(fname, backup.leafName);

  backup.remove(false);
}

function test_backup_new_filename() {
  var backup = Services.storage.backupDatabaseFile(getTestDB(), BACKUP_FILE_NAME);
  do_check_eq(BACKUP_FILE_NAME, backup.leafName);

  backup.remove(false);
}

function test_backup_new_folder() {
  var parentDir = getTestDB().parent;
  parentDir.append("test_storage_temp");
  if (parentDir.exists())
    parentDir.remove(true);
  parentDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0o755);
  do_check_true(parentDir.exists());

  var backup = Services.storage.backupDatabaseFile(getTestDB(), BACKUP_FILE_NAME,
                                                   parentDir);
  do_check_eq(BACKUP_FILE_NAME, backup.leafName);
  do_check_true(parentDir.equals(backup.parent));

  parentDir.remove(true);
}

var tests = [
  test_openSpecialDatabase_invalid_arg,
  test_openDatabase_null_file,
  test_openUnsharedDatabase_null_file,
  test_openDatabase_file_DNE,
  test_openDatabase_file_exists,
  test_corrupt_db_throws_with_openDatabase,
  test_fake_db_throws_with_openDatabase,
  test_backup_not_new_filename,
  test_backup_new_filename,
  test_backup_new_folder,
];

function run_test() {
  for (var i = 0; i < tests.length; i++) {
    tests[i]();
  }

  cleanup();
}
