/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the custom functions

var testNums = [1, 2, 3, 4];

function setup()
{
  getOpenedDatabase().createTable("function_tests", "id INTEGER PRIMARY KEY");

  var stmt = createStatement("INSERT INTO function_tests (id) VALUES(?1)");
  for(var i = 0; i < testNums.length; ++i) {
    stmt.bindByIndex(0, testNums[i]);
    stmt.execute();
  }
  stmt.reset();
  stmt.finalize();
}

var testSquareFunction = {
  calls: 0,

  onFunctionCall: function(val) {
    ++this.calls;
    return val.getInt32(0) * val.getInt32(0);
  }
};

function test_function_registration()
{
  var msc = getOpenedDatabase();
  msc.createFunction("test_square", 1, testSquareFunction);
}

function test_function_no_double_registration()
{
  var msc = getOpenedDatabase();
  try {
    msc.createFunction("test_square", 2, testSquareFunction);
    do_throw("We shouldn't get here!");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_FAILURE, e.result);
  }
}

function test_function_removal()
{
  var msc = getOpenedDatabase();
  msc.removeFunction("test_square");
  // Should be Ok now
  msc.createFunction("test_square", 1, testSquareFunction);
}

function test_function_aliases()
{
  var msc = getOpenedDatabase();
  msc.createFunction("test_square2", 1, testSquareFunction);
}

function test_function_call()
{
  var stmt = createStatement("SELECT test_square(id) FROM function_tests");
  while(stmt.executeStep());
  do_check_eq(testNums.length, testSquareFunction.calls);
  testSquareFunction.calls = 0;
  stmt.finalize();
}

function test_function_result()
{
  var stmt = createStatement("SELECT test_square(42) FROM function_tests");
  stmt.executeStep();
  do_check_eq(42*42, stmt.getInt32(0));
  testSquareFunction.calls = 0;
  stmt.finalize();
}

var tests = [test_function_registration, test_function_no_double_registration,
             test_function_removal, test_function_aliases, test_function_call,
             test_function_result];

function run_test()
{
  setup();

  for (var i = 0; i < tests.length; i++) {
    tests[i]();
  }

  cleanup();
}
