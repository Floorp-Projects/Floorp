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
    do_check_eq(Cr.NS_ERROR_FAILURE, e.result);
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
    do_check_eq(Cr.NS_ERROR_FAILURE, e.result);
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

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

var tests = [
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
];
let index = 0;

function run_next_test()
{
  function _run_next_test() {
    if (index < tests.length) {
      do_test_pending();
      print("Running the next test: " + tests[index].name);

      // Asynchronous tests means that exceptions don't kill the test.
      try {
        tests[index++]();
      }
      catch (e) {
        do_throw(e);
      }
    }

    do_test_finished();
  }

  // For saner stacks, we execute this code RSN.
  do_execute_soon(_run_next_test);
}

function run_test()
{
  cleanup();

  do_test_pending();
  run_next_test();
}
