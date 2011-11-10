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

// This file tests the functions of mozIStorageConnection

////////////////////////////////////////////////////////////////////////////////
//// Test Functions

function test_connectionReady_open()
{
  // there doesn't seem to be a way for the connection to not be ready (unless
  // we close it with mozIStorageConnection::Close(), but we don't for this).
  // It can only fail if GetPath fails on the database file, or if we run out
  // of memory trying to use an in-memory database

  var msc = getOpenedDatabase();
  do_check_true(msc.connectionReady);
  run_next_test();
}

function test_connectionReady_closed()
{
  // This also tests mozIStorageConnection::Close()

  var msc = getOpenedDatabase();
  msc.close();
  do_check_false(msc.connectionReady);
  gDBConn = null; // this is so later tests don't start to fail.
  run_next_test();
}

function test_databaseFile()
{
  var msc = getOpenedDatabase();
  do_check_true(getTestDB().equals(msc.databaseFile));
  run_next_test();
}

function test_tableExists_not_created()
{
  var msc = getOpenedDatabase();
  do_check_false(msc.tableExists("foo"));
  run_next_test();
}

function test_indexExists_not_created()
{
  var msc = getOpenedDatabase();
  do_check_false(msc.indexExists("foo"));
  run_next_test();
}

function test_createTable_not_created()
{
  var msc = getOpenedDatabase();
  msc.createTable("test", "id INTEGER PRIMARY KEY, name TEXT");
  do_check_true(msc.tableExists("test"));
  run_next_test();
}

function test_indexExists_created()
{
  var msc = getOpenedDatabase();
  msc.executeSimpleSQL("CREATE INDEX name_ind ON test (name)");
  do_check_true(msc.indexExists("name_ind"));
  run_next_test();
}

function test_createTable_already_created()
{
  var msc = getOpenedDatabase();
  do_check_true(msc.tableExists("test"));
  try {
    msc.createTable("test", "id INTEGER PRIMARY KEY, name TEXT");
    do_throw("We shouldn't get here!");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_FAILURE, e.result);
  }
  run_next_test();
}

function test_lastInsertRowID()
{
  var msc = getOpenedDatabase();
  msc.executeSimpleSQL("INSERT INTO test (name) VALUES ('foo')");
  do_check_eq(1, msc.lastInsertRowID);
  run_next_test();
}

function test_transactionInProgress_no()
{
  var msc = getOpenedDatabase();
  do_check_false(msc.transactionInProgress);
  run_next_test();
}

function test_transactionInProgress_yes()
{
  var msc = getOpenedDatabase();
  msc.beginTransaction();
  do_check_true(msc.transactionInProgress);
  msc.commitTransaction();
  do_check_false(msc.transactionInProgress);

  msc.beginTransaction();
  do_check_true(msc.transactionInProgress);
  msc.rollbackTransaction();
  do_check_false(msc.transactionInProgress);
  run_next_test();
}

function test_commitTransaction_no_transaction()
{
  var msc = getOpenedDatabase();
  do_check_false(msc.transactionInProgress);
  try {
    msc.commitTransaction();
    do_throw("We should not get here!");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_UNEXPECTED, e.result);
  }
  run_next_test();
}

function test_rollbackTransaction_no_transaction()
{
  var msc = getOpenedDatabase();
  do_check_false(msc.transactionInProgress);
  try {
    msc.rollbackTransaction();
    do_throw("We should not get here!");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_UNEXPECTED, e.result);
  }
  run_next_test();
}

function test_get_schemaVersion_not_set()
{
  do_check_eq(0, getOpenedDatabase().schemaVersion);
  run_next_test();
}

function test_set_schemaVersion()
{
  var msc = getOpenedDatabase();
  const version = 1;
  msc.schemaVersion = version;
  do_check_eq(version, msc.schemaVersion);
  run_next_test();
}

function test_set_schemaVersion_same()
{
  var msc = getOpenedDatabase();
  const version = 1;
  msc.schemaVersion = version; // should still work ok
  do_check_eq(version, msc.schemaVersion);
  run_next_test();
}

function test_set_schemaVersion_negative()
{
  var msc = getOpenedDatabase();
  const version = -1;
  msc.schemaVersion = version;
  do_check_eq(version, msc.schemaVersion);
  run_next_test();
}

function test_createTable(){
  var temp = getTestDB().parent;
  temp.append("test_db_table");
  try {
    var con = getService().openDatabase(temp);
    con.createTable("a","");
  } catch (e) {
    if (temp.exists()) try {
      temp.remove(false);
    } catch (e2) {}
    do_check_true(e.result==Cr.NS_ERROR_NOT_INITIALIZED ||
                  e.result==Cr.NS_ERROR_FAILURE);
  }
  run_next_test();
}

function test_defaultSynchronousAtNormal()
{
  var msc = getOpenedDatabase();
  var stmt = createStatement("PRAGMA synchronous;");
  try {
    stmt.executeStep();
    do_check_eq(1, stmt.getInt32(0));
  }
  finally {
    stmt.reset();
    stmt.finalize();
  }
  run_next_test();
}

function test_close_does_not_spin_event_loop()
{
  // We want to make sure that the event loop on the calling thread does not
  // spin when close is called.
  let event = {
    ran: false,
    run: function()
    {
      this.ran = true;
    },
  };

  // Post the event before we call close, so it would run if the event loop was
  // spun during close.
  let thread = Cc["@mozilla.org/thread-manager;1"].
               getService(Ci.nsIThreadManager).
               currentThread;
  thread.dispatch(event, Ci.nsIThread.DISPATCH_NORMAL);

  // Sanity check, then close the database.  Afterwards, we should not have ran!
  do_check_false(event.ran);
  getOpenedDatabase().close();
  do_check_false(event.ran);

  // Reset gDBConn so that later tests will get a new connection object.
  gDBConn = null;
  run_next_test();
}

function test_asyncClose_succeeds_with_finalized_async_statement()
{
  // XXX this test isn't perfect since we can't totally control when events will
  //     run.  If this paticular function fails randomly, it means we have a
  //     real bug.

  // We want to make sure we create a cached async statement to make sure that
  // when we finalize our statement, we end up finalizing the async one too so
  // close will succeed.
  let stmt = createStatement("SELECT * FROM test");
  stmt.executeAsync();
  stmt.finalize();

  getOpenedDatabase().asyncClose(function() {
    // Reset gDBConn so that later tests will get a new connection object.
    gDBConn = null;
    run_next_test();
  });
}

function test_close_fails_with_async_statement_ran()
{
  let stmt = createStatement("SELECT * FROM test");
  stmt.executeAsync();
  stmt.finalize();

  let db = getOpenedDatabase();
  try {
    db.close();
    do_throw("should have thrown");
  }
  catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_UNEXPECTED);
  }
  finally {
    // Clean up after ourselves.
    db.asyncClose(function() {
      // Reset gDBConn so that later tests will get a new connection object.
      gDBConn = null;
      run_next_test();
    });
  }
}

function test_clone_optional_param()
{
  let db1 = getService().openUnsharedDatabase(getTestDB());
  let db2 = db1.clone();
  do_check_true(db2.connectionReady);

  // A write statement should not fail here.
  let stmt = db2.createStatement("INSERT INTO test (name) VALUES (:name)");
  stmt.params.name = "dwitte";
  stmt.execute();
  stmt.finalize();

  // And a read statement should succeed.
  stmt = db2.createStatement("SELECT * FROM test");
  do_check_true(stmt.executeStep());
  stmt.finalize();

  // Additionally check that it is a connection on the same database.
  do_check_true(db1.databaseFile.equals(db2.databaseFile));

  run_next_test();
}

function test_clone_readonly()
{
  let db1 = getService().openUnsharedDatabase(getTestDB());
  let db2 = db1.clone(true);
  do_check_true(db2.connectionReady);

  // A write statement should fail here.
  let stmt = db2.createStatement("INSERT INTO test (name) VALUES (:name)");
  stmt.params.name = "reed";
  expectError(Cr.NS_ERROR_FILE_READ_ONLY, function() stmt.execute());
  stmt.finalize();

  // And a read statement should succeed.
  stmt = db2.createStatement("SELECT * FROM test");
  do_check_true(stmt.executeStep());
  stmt.finalize();

  run_next_test();
}

function test_clone_shared_readonly()
{
  let db1 = getService().openDatabase(getTestDB());
  let db2 = db1.clone(true);
  do_check_true(db2.connectionReady);

  // A write statement should fail here.
  let stmt = db2.createStatement("INSERT INTO test (name) VALUES (:name)");
  stmt.params.name = "reed";
  // TODO currently SQLite does not actually work correctly here.  The behavior
  //      we want is commented out, and the current behavior is being tested
  //      for.  Our IDL comments will have to be updated when this starts to
  //      work again.
  stmt.execute(); // This should not throw!
  //expectError(Cr.NS_ERROR_FILE_READ_ONLY, function() stmt.execute());
  stmt.finalize();

  // And a read statement should succeed.
  stmt = db2.createStatement("SELECT * FROM test");
  do_check_true(stmt.executeStep());
  stmt.finalize();

  run_next_test();
}

function test_close_clone_fails()
{
  let calls = [
    "openDatabase",
    "openUnsharedDatabase",
  ];
  calls.forEach(function(methodName) {
    let db = getService()[methodName](getTestDB());
    db.close();
    expectError(Cr.NS_ERROR_NOT_INITIALIZED, function() db.clone());
  });

  run_next_test();
}

function test_memory_clone_fails()
{
  let db = getService().openSpecialDatabase("memory");
  db.close();
  expectError(Cr.NS_ERROR_NOT_INIALIZED, function() db.clone());

  run_next_test();
}

function test_clone_copies_functions()
{
  const FUNC_NAME = "test_func";
  let calls = [
    "openDatabase",
    "openUnsharedDatabase",
  ];
  let functionMethods = [
    "createFunction",
    "createAggregateFunction",
  ];
  calls.forEach(function(methodName) {
    [true, false].forEach(function(readOnly) {
      functionMethods.forEach(function(functionMethod) {
        let db1 = getService()[methodName](getTestDB());
        // Create a function for db1.
        db1[functionMethod](FUNC_NAME, 1, {
          onFunctionCall: function() 0,
          onStep: function() 0,
          onFinal: function() 0,
        });

        // Clone it, and make sure the function exists still.
        let db2 = db1.clone(readOnly);
        // Note: this would fail if the function did not exist.
        let stmt = db2.createStatement("SELECT " + FUNC_NAME + "(id) FROM test");
        stmt.finalize();
        db1.close();
        db2.close();
      });
    });
  });

  run_next_test();
}

function test_clone_copies_overridden_functions()
{
  const FUNC_NAME = "lower";
  function test_func() {
    this.called = false;
  }
  test_func.prototype = {
    onFunctionCall: function() {
      this.called = true;
    },
    onStep: function() {
      this.called = true;
    },
    onFinal: function() 0,
  };

  let calls = [
    "openDatabase",
    "openUnsharedDatabase",
  ];
  let functionMethods = [
    "createFunction",
    "createAggregateFunction",
  ];
  calls.forEach(function(methodName) {
    [true, false].forEach(function(readOnly) {
      functionMethods.forEach(function(functionMethod) {
        let db1 = getService()[methodName](getTestDB());
        // Create a function for db1.
        let func = new test_func();
        db1[functionMethod](FUNC_NAME, 1, func);
        do_check_false(func.called);

        // Clone it, and make sure the function gets called.
        let db2 = db1.clone(readOnly);
        let stmt = db2.createStatement("SELECT " + FUNC_NAME + "(id) FROM test");
        stmt.executeStep();
        do_check_true(func.called);
        stmt.finalize();
        db1.close();
        db2.close();
      });
    });
  });

  run_next_test();
}

function test_clone_copies_pragmas()
{
  const PRAGMAS = [
    { name: "cache_size", value: 500, copied: true },
    { name: "temp_store", value: 2, copied: true },
    { name: "foreign_keys", value: 1, copied: true },
    { name: "journal_size_limit", value: 524288, copied: true },
    { name: "synchronous", value: 2, copied: true },
    { name: "wal_autocheckpoint", value: 16, copied: true },
    { name: "ignore_check_constraints", value: 1, copied: false },
  ];

  let db1 = getService().openUnsharedDatabase(getTestDB());

  // Sanity check initial values are different from enforced ones.
  PRAGMAS.forEach(function (pragma) {
    let stmt = db1.createStatement("PRAGMA " + pragma.name);
    do_check_true(stmt.executeStep());
    do_check_neq(pragma.value, stmt.getInt32(0));
    stmt.finalize();
  });
  // Execute pragmas.
  PRAGMAS.forEach(function (pragma) {
    db1.executeSimpleSQL("PRAGMA " + pragma.name + " = " + pragma.value);
  });

  let db2 = db1.clone();
  do_check_true(db2.connectionReady);

  // Check cloned connection inherited pragma values.
  PRAGMAS.forEach(function (pragma) {
    let stmt = db2.createStatement("PRAGMA " + pragma.name);
    do_check_true(stmt.executeStep());
    let validate = pragma.copied ? do_check_eq : do_check_neq;
    validate(pragma.value, stmt.getInt32(0));
    stmt.finalize();
  });

  run_next_test();
}

function test_readonly_clone_copies_pragmas()
{
  const PRAGMAS = [
    { name: "cache_size", value: 500, copied: true },
    { name: "temp_store", value: 2, copied: true },
    { name: "foreign_keys", value: 1, copied: false },
    { name: "journal_size_limit", value: 524288, copied: false },
    { name: "synchronous", value: 2, copied: false },
    { name: "wal_autocheckpoint", value: 16, copied: false },
    { name: "ignore_check_constraints", value: 1, copied: false },
  ];

  let db1 = getService().openUnsharedDatabase(getTestDB());

  // Sanity check initial values are different from enforced ones.
  PRAGMAS.forEach(function (pragma) {
    let stmt = db1.createStatement("PRAGMA " + pragma.name);
    do_check_true(stmt.executeStep());
    do_check_neq(pragma.value, stmt.getInt32(0));
    stmt.finalize();
  });
  // Execute pragmas.
  PRAGMAS.forEach(function (pragma) {
    db1.executeSimpleSQL("PRAGMA " + pragma.name + " = " + pragma.value);
  });

  let db2 = db1.clone(true);
  do_check_true(db2.connectionReady);

  // Check cloned connection inherited pragma values.
  PRAGMAS.forEach(function (pragma) {
    let stmt = db2.createStatement("PRAGMA " + pragma.name);
    do_check_true(stmt.executeStep());
    let validate = pragma.copied ? do_check_eq : do_check_neq;
    validate(pragma.value, stmt.getInt32(0));
    stmt.finalize();
  });

  run_next_test();
}

function test_getInterface()
{
  let db = getOpenedDatabase();
  let target = db.QueryInterface(Ci.nsIInterfaceRequestor)
                 .getInterface(Ci.nsIEventTarget);
  // Just check that target is non-null.  Other tests will ensure that it has
  // the correct value.
  do_check_true(target != null);

  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

[
  test_connectionReady_open,
  test_connectionReady_closed,
  test_databaseFile,
  test_tableExists_not_created,
  test_indexExists_not_created,
  test_createTable_not_created,
  test_indexExists_created,
  test_createTable_already_created,
  test_lastInsertRowID,
  test_transactionInProgress_no,
  test_transactionInProgress_yes,
  test_commitTransaction_no_transaction,
  test_rollbackTransaction_no_transaction,
  test_get_schemaVersion_not_set,
  test_set_schemaVersion,
  test_set_schemaVersion_same,
  test_set_schemaVersion_negative,
  test_createTable,
  test_defaultSynchronousAtNormal,
  test_close_does_not_spin_event_loop, // must be ran before executeAsync tests
  test_asyncClose_succeeds_with_finalized_async_statement,
  test_close_fails_with_async_statement_ran,
  test_clone_optional_param,
  test_clone_readonly,
  test_close_clone_fails,
  test_clone_copies_functions,
  test_clone_copies_overridden_functions,
  test_clone_copies_pragmas,
  test_readonly_clone_copies_pragmas,
  test_getInterface,
].forEach(add_test);

function run_test()
{
  run_next_test();
}
