/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Testcase for bug 393952:  crash when I try to VACUUM and one of the tables
// has a UNIQUE text column.   StorageUnicodeFunctions::likeFunction()
// needs to handle null aArgv[0] and aArgv[1]

function setup() {
  getOpenedDatabase().createTable("t1", "x TEXT UNIQUE");

  var stmt = createStatement("INSERT INTO t1 (x) VALUES ('a')");
  stmt.execute();
  stmt.reset();
  stmt.finalize();
}

function test_vacuum() {
  var stmt = createStatement("VACUUM;");
  stmt.executeStep();
  stmt.reset();
  stmt.finalize();
}

var tests = [test_vacuum];

function run_test() {
  setup();

  for (var i = 0; i < tests.length; i++)
    tests[i]();

  cleanup();
}

