/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Storage Test Code.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
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

// This file tests the functions of mozIStorageService except for
// openUnsharedDatabase, which is tested by test_storage_service_unshared.js.

const BACKUP_FILE_NAME = "test_storage.sqlite.backup";

function test_openSpecialDatabase_invalid_arg()
{
  try {
    getService().openSpecialDatabase("abcd");
    do_throw("We should not get here!");
  } catch (e) {
    print(e);
    print("e.result is " + e.result);
    do_check_eq(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
}

function test_openDatabase_null_file()
{
  try {
    getService().openDatabase(null);
    do_throw("We should not get here!");
  } catch (e) {
    print(e);
    print("e.result is " + e.result);
    do_check_eq(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
}

function test_openUnsharedDatabase_null_file()
{
  try {
    getService().openUnsharedDatabase(null);
    do_throw("We should not get here!");
  } catch (e) {
    print(e);
    print("e.result is " + e.result);
    do_check_eq(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
}

function test_openDatabase_file_DNE()
{
  // the file should be created after calling
  var db = getTestDB();
  do_check_false(db.exists());
  getService().openDatabase(db);
  do_check_true(db.exists());
}

function test_openDatabase_file_exists()
{
  // it should already exist from our last test
  var db = getTestDB();
  do_check_true(db.exists());
  getService().openDatabase(db);
  do_check_true(db.exists());
}

function test_corrupt_db_throws_with_openDatabase()
{
  try {
    getDatabase(getCorruptDB());
    do_throw("should not be here");
  }
  catch (e) {
    do_check_eq(Cr.NS_ERROR_FILE_CORRUPTED, e.result);
  }
}

function test_fake_db_throws_with_openDatabase()
{
  try {
    getDatabase(getFakeDB());
    do_throw("should not be here");
  }
  catch (e) {
    do_check_eq(Cr.NS_ERROR_FILE_CORRUPTED, e.result);
  }
}

function test_backup_not_new_filename()
{
  const fname = getTestDB().leafName;

  var backup = getService().backupDatabaseFile(getTestDB(), fname);
  do_check_neq(fname, backup.leafName);

  backup.remove(false);
}

function test_backup_new_filename()
{
  var backup = getService().backupDatabaseFile(getTestDB(), BACKUP_FILE_NAME);
  do_check_eq(BACKUP_FILE_NAME, backup.leafName);
  
  backup.remove(false);
}

function test_backup_new_folder()
{
  var parentDir = getTestDB().parent;
  parentDir.append("test_storage_temp");
  if (parentDir.exists())
    parentDir.remove(true);
  parentDir.create(Ci.nsIFile.DIRECTORY_TYPE, 0755);
  do_check_true(parentDir.exists());

  var backup = getService().backupDatabaseFile(getTestDB(), BACKUP_FILE_NAME,
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

function run_test()
{
  for (var i = 0; i < tests.length; i++)
    tests[i]();
    
  cleanup();
}

