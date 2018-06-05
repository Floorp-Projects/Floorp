/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sw=2 ts=2 sts=2 et : */
/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

/**
 * This file tests that the JS language helpers in various ways.
 */

// Test Functions

function test_params_enumerate() {
  let stmt = createStatement(
    "SELECT * FROM test WHERE id IN (:a, :b, :c)"
  );

  // Make sure they are right.
  let expected = [0, 1, 2, "a", "b", "c", "length"];
  let index = 0;
  for (let name in stmt.params) {
    if (name == "QueryInterface")
      continue;
    Assert.equal(name, expected[index++]);
  }
  Assert.equal(index, 7);
}

function test_params_prototype() {
  let stmt = createStatement(
    "SELECT * FROM sqlite_master"
  );

  // Set a property on the prototype and make sure it exist (will not be a
  // bindable parameter, however).
  Object.getPrototypeOf(stmt.params).test = 2;
  Assert.equal(stmt.params.test, 2);

  delete Object.getPrototypeOf(stmt.params).test;
  stmt.finalize();
}

function test_row_prototype() {
  let stmt = createStatement(
    "SELECT * FROM sqlite_master"
  );

  Assert.ok(stmt.executeStep());

  // Set a property on the prototype and make sure it exists (will not be in the
  // results, however).
  Object.getPrototypeOf(stmt.row).test = 2;
  Assert.equal(stmt.row.test, 2);

  // Clean up after ourselves.
  delete Object.getPrototypeOf(stmt.row).test;
  stmt.finalize();
}

function test_row_enumerate() {
  let stmt = createStatement(
    "SELECT * FROM test"
  );

  Assert.ok(stmt.executeStep());

  let expected = ["id", "string"];
  let expected_values = [123, "foo"];
  let index = 0;
  for (let name in stmt.row) {
    Assert.equal(name, expected[index]);
    Assert.equal(stmt.row[name], expected_values[index]);
    index++;
  }
  Assert.equal(index, 2);

  // Save off the row helper, then forget the statement and trigger a GC.  We
  // want to ensure that if the row helper is retained but the statement is
  // destroyed, that no crash occurs and that the late access attempt simply
  // throws an error.
  let savedOffRow = stmt.row;
  stmt = null;
  Cu.forceGC();
  Assert.throws(() => { return savedOffRow.string; },
                /NS_ERROR_NOT_INITIALIZED/,
                "GC'ed statement should throw");
}

function test_params_gets_sync() {
  // Added for bug 562866.
  /*
  let stmt = createStatement(
    "SELECT * FROM test WHERE id IN (:a, :b, :c)"
  );

  // Make sure we do not assert in getting the value.
  let originalCount = Object.getOwnPropertyNames(stmt.params).length;
  let expected = ["a", "b", "c"];
  for (let name of expected) {
    stmt.params[name];
  }

  // Now make sure we didn't magically get any additional properties.
  let finalCount = Object.getOwnPropertyNames(stmt.params).length;
  do_check_eq(originalCount + expected.length, finalCount);
  */
}

function test_params_gets_async() {
  // Added for bug 562866.
  /*
  let stmt = createAsyncStatement(
    "SELECT * FROM test WHERE id IN (:a, :b, :c)"
  );

  // Make sure we do not assert in getting the value.
  let originalCount = Object.getOwnPropertyNames(stmt.params).length;
  let expected = ["a", "b", "c"];
  for (let name of expected) {
    stmt.params[name];
  }

  // Now make sure we didn't magically get any additional properties.
  let finalCount = Object.getOwnPropertyNames(stmt.params).length;
  do_check_eq(originalCount + expected.length, finalCount);
  */
}

// Test Runner

var tests = [
  test_params_enumerate,
  test_params_prototype,
  test_row_enumerate,
  test_row_prototype,
  test_params_gets_sync,
  test_params_gets_async,
];
function run_test() {
  cleanup();

  // Create our database.
  getOpenedDatabase().executeSimpleSQL(
    "CREATE TABLE test (" +
      "id INTEGER PRIMARY KEY, string TEXT" +
    ")"
  );
  getOpenedDatabase().executeSimpleSQL(
    "INSERT INTO test (id, string) VALUES (123, 'foo')"
  );

  // Run the tests.
  tests.forEach(test => test());
}
