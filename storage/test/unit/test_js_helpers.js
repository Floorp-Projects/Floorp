/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et : */
/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests that the JS language helpers in various ways.
 */

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

function test_params_enumerate()
{
  let stmt = createStatement(
    "SELECT * FROM test WHERE id IN (:a, :b, :c)"
  );

  // Make sure they are right.
  let expected = ["a", "b", "c"];
  let index = 0;
  for (let name in stmt.params)
    do_check_eq(name, expected[index++]);
}

function test_params_prototype()
{
  let stmt = createStatement(
    "SELECT * FROM sqlite_master"
  );

  // Set a property on the prototype and make sure it exist (will not be a
  // bindable parameter, however).
  Object.getPrototypeOf(stmt.params).test = 2;
  do_check_eq(stmt.params.test, 2);
  stmt.finalize();
}

function test_row_prototype()
{
  let stmt = createStatement(
    "SELECT * FROM sqlite_master"
  );

  do_check_true(stmt.executeStep());

  // Set a property on the prototype and make sure it exists (will not be in the
  // results, however).
  Object.getPrototypeOf(stmt.row).test = 2;
  do_check_eq(stmt.row.test, 2);

  // Clean up after ourselves.
  delete Object.getPrototypeOf(stmt.row).test;
  stmt.finalize();
}

function test_params_gets_sync()
{
  // Added for bug 562866.
  /*
  let stmt = createStatement(
    "SELECT * FROM test WHERE id IN (:a, :b, :c)"
  );

  // Make sure we do not assert in getting the value.
  let originalCount = Object.getOwnPropertyNames(stmt.params).length;
  let expected = ["a", "b", "c"];
  for each (let name in expected) {
    stmt.params[name];
  }

  // Now make sure we didn't magically get any additional properties.
  let finalCount = Object.getOwnPropertyNames(stmt.params).length;
  do_check_eq(originalCount + expected.length, finalCount);
  */
}

function test_params_gets_async()
{
  // Added for bug 562866.
  /*
  let stmt = createAsyncStatement(
    "SELECT * FROM test WHERE id IN (:a, :b, :c)"
  );

  // Make sure we do not assert in getting the value.
  let originalCount = Object.getOwnPropertyNames(stmt.params).length;
  let expected = ["a", "b", "c"];
  for each (let name in expected) {
    stmt.params[name];
  }

  // Now make sure we didn't magically get any additional properties.
  let finalCount = Object.getOwnPropertyNames(stmt.params).length;
  do_check_eq(originalCount + expected.length, finalCount);
  */
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

let tests = [
  test_params_enumerate,
  test_params_prototype,
  test_row_prototype,
  test_params_gets_sync,
  test_params_gets_async,
];
function run_test()
{
  cleanup();

  // Create our database.
  getOpenedDatabase().executeSimpleSQL(
    "CREATE TABLE test (" +
      "id INTEGER PRIMARY KEY " +
    ")"
  );

  // Run the tests.
  tests.forEach(function(test) test());
}
