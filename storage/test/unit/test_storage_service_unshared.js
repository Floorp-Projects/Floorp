/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the openUnsharedDatabase function of mozIStorageService.

function test_openUnsharedDatabase_null_file() {
  try {
    Services.storage.openUnsharedDatabase(null);
    do_throw("We should not get here!");
  } catch (e) {
    print(e);
    print("e.result is " + e.result);
    Assert.equal(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
}

function test_openUnsharedDatabase_file_DNE() {
  // the file should be created after calling
  var db = getTestDB();
  Assert.ok(!db.exists());
  Services.storage.openUnsharedDatabase(db);
  Assert.ok(db.exists());
}

function test_openUnsharedDatabase_file_exists() {
  // it should already exist from our last test
  var db = getTestDB();
  Assert.ok(db.exists());
  Services.storage.openUnsharedDatabase(db);
  Assert.ok(db.exists());
}

var tests = [
  test_openUnsharedDatabase_null_file,
  test_openUnsharedDatabase_file_DNE,
  test_openUnsharedDatabase_file_exists,
];

function run_test() {
  for (var i = 0; i < tests.length; i++) {
    tests[i]();
  }

  cleanup();
}
