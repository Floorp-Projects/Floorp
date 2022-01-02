/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the functions of mozIStorageStatement

function setup() {
  getOpenedDatabase().createTable("test", "id INTEGER PRIMARY KEY, name TEXT");
}

function test_parameterCount_none() {
  var stmt = createStatement("SELECT * FROM test");
  Assert.equal(0, stmt.parameterCount);
  stmt.reset();
  stmt.finalize();
}

function test_parameterCount_one() {
  var stmt = createStatement("SELECT * FROM test WHERE id = ?1");
  Assert.equal(1, stmt.parameterCount);
  stmt.reset();
  stmt.finalize();
}

function test_getParameterName() {
  var stmt = createStatement("SELECT * FROM test WHERE id = :id");
  Assert.equal(":id", stmt.getParameterName(0));
  stmt.reset();
  stmt.finalize();
}

function test_getParameterIndex_different() {
  var stmt = createStatement(
    "SELECT * FROM test WHERE id = :id OR name = :name"
  );
  Assert.equal(0, stmt.getParameterIndex("id"));
  Assert.equal(1, stmt.getParameterIndex("name"));
  stmt.reset();
  stmt.finalize();
}

function test_getParameterIndex_same() {
  var stmt = createStatement(
    "SELECT * FROM test WHERE id = :test OR name = :test"
  );
  Assert.equal(0, stmt.getParameterIndex("test"));
  stmt.reset();
  stmt.finalize();
}

function test_columnCount() {
  var stmt = createStatement("SELECT * FROM test WHERE id = ?1 OR name = ?2");
  Assert.equal(2, stmt.columnCount);
  stmt.reset();
  stmt.finalize();
}

function test_getColumnName() {
  var stmt = createStatement("SELECT name, id FROM test");
  Assert.equal("id", stmt.getColumnName(1));
  Assert.equal("name", stmt.getColumnName(0));
  stmt.reset();
  stmt.finalize();
}

function test_getColumnIndex_same_case() {
  var stmt = createStatement("SELECT name, id FROM test");
  Assert.equal(0, stmt.getColumnIndex("name"));
  Assert.equal(1, stmt.getColumnIndex("id"));
  stmt.reset();
  stmt.finalize();
}

function test_getColumnIndex_different_case() {
  var stmt = createStatement("SELECT name, id FROM test");
  try {
    Assert.equal(0, stmt.getColumnIndex("NaMe"));
    do_throw("should not get here");
  } catch (e) {
    Assert.equal(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
  try {
    Assert.equal(1, stmt.getColumnIndex("Id"));
    do_throw("should not get here");
  } catch (e) {
    Assert.equal(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
  stmt.reset();
  stmt.finalize();
}

function test_state_ready() {
  var stmt = createStatement("SELECT name, id FROM test");
  Assert.equal(Ci.mozIStorageStatement.MOZ_STORAGE_STATEMENT_READY, stmt.state);
  stmt.reset();
  stmt.finalize();
}

function test_state_executing() {
  var stmt = createStatement("INSERT INTO test (name) VALUES ('foo')");
  stmt.execute();
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("SELECT name, id FROM test");
  stmt.executeStep();
  Assert.equal(
    Ci.mozIStorageStatement.MOZ_STORAGE_STATEMENT_EXECUTING,
    stmt.state
  );
  stmt.executeStep();
  Assert.equal(
    Ci.mozIStorageStatement.MOZ_STORAGE_STATEMENT_EXECUTING,
    stmt.state
  );
  stmt.reset();
  Assert.equal(Ci.mozIStorageStatement.MOZ_STORAGE_STATEMENT_READY, stmt.state);
  stmt.finalize();
}

function test_state_after_finalize() {
  var stmt = createStatement("SELECT name, id FROM test");
  stmt.executeStep();
  stmt.finalize();
  Assert.equal(
    Ci.mozIStorageStatement.MOZ_STORAGE_STATEMENT_INVALID,
    stmt.state
  );
}

function test_failed_execute() {
  var stmt = createStatement("INSERT INTO test (name) VALUES ('foo')");
  stmt.execute();
  stmt.finalize();
  var id = getOpenedDatabase().lastInsertRowID;
  stmt = createStatement("INSERT INTO test(id, name) VALUES(:id, 'bar')");
  stmt.params.id = id;
  try {
    // Should throw a constraint error
    stmt.execute();
    do_throw("Should have seen a constraint error");
  } catch (e) {
    Assert.equal(getOpenedDatabase().lastError, Ci.mozIStorageError.CONSTRAINT);
  }
  Assert.equal(Ci.mozIStorageStatement.MOZ_STORAGE_STATEMENT_READY, stmt.state);
  // Should succeed without needing to reset the statement manually
  stmt.finalize();
}

function test_bind_undefined() {
  var stmt = createStatement("INSERT INTO test (name) VALUES ('foo')");

  expectError(Cr.NS_ERROR_ILLEGAL_VALUE, () => stmt.bindParameters(undefined));

  stmt.finalize();
}

var tests = [
  test_parameterCount_none,
  test_parameterCount_one,
  test_getParameterName,
  test_getParameterIndex_different,
  test_getParameterIndex_same,
  test_columnCount,
  test_getColumnName,
  test_getColumnIndex_same_case,
  test_getColumnIndex_different_case,
  test_state_ready,
  test_state_executing,
  test_state_after_finalize,
  test_failed_execute,
  test_bind_undefined,
];

function run_test() {
  setup();

  for (var i = 0; i < tests.length; i++) {
    tests[i]();
  }

  cleanup();
}
