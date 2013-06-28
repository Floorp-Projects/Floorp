/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Promise",
    "resource://gre/modules/commonjs/sdk/core/promise.js");


do_get_profile();
var dirSvc = Cc["@mozilla.org/file/directory_service;1"].
             getService(Ci.nsIProperties);

function getTestDB()
{
  var db = dirSvc.get("ProfD", Ci.nsIFile);
  db.append("test_storage.sqlite");
  return db;
}

/**
 * Obtains a corrupt database to test against.
 */
function getCorruptDB()
{
  return do_get_file("corruptDB.sqlite");
}

/**
 * Obtains a fake (non-SQLite format) database to test against.
 */
function getFakeDB()
{
  return do_get_file("fakeDB.sqlite");
}

function cleanup()
{
  // close the connection
  print("*** Storage Tests: Trying to close!");
  getOpenedDatabase().close();

  // we need to null out the database variable to get a new connection the next
  // time getOpenedDatabase is called
  gDBConn = null;

  // removing test db
  print("*** Storage Tests: Trying to remove file!");
  var dbFile = getTestDB();
  if (dbFile.exists())
    try { dbFile.remove(false); } catch(e) { /* stupid windows box */ }
}

/**
 * Use asyncClose to cleanup a connection.  Synchronous by means of internally
 * spinning an event loop.
 */
function asyncCleanup()
{
  let closed = false;

  // close the connection
  print("*** Storage Tests: Trying to asyncClose!");
  getOpenedDatabase().asyncClose(function() { closed = true; });

  let curThread = Components.classes["@mozilla.org/thread-manager;1"]
                            .getService().currentThread;
  while (!closed)
    curThread.processNextEvent(true);

  // we need to null out the database variable to get a new connection the next
  // time getOpenedDatabase is called
  gDBConn = null;

  // removing test db
  print("*** Storage Tests: Trying to remove file!");
  var dbFile = getTestDB();
  if (dbFile.exists())
    try { dbFile.remove(false); } catch(e) { /* stupid windows box */ }
}

function getService()
{
  return Cc["@mozilla.org/storage/service;1"].getService(Ci.mozIStorageService);
}

var gDBConn = null;

/**
 * Get a connection to the test database.  Creates and caches the connection
 * if necessary, otherwise reuses the existing cached connection. This
 * connection shares its cache.
 *
 * @returns the mozIStorageConnection for the file.
 */
function getOpenedDatabase()
{
  if (!gDBConn) {
    gDBConn = getService().openDatabase(getTestDB());
  }
  return gDBConn;
}

/**
 * Get a connection to the test database.  Creates and caches the connection
 * if necessary, otherwise reuses the existing cached connection. This
 * connection doesn't share its cache.
 *
 * @returns the mozIStorageConnection for the file.
 */
function getOpenedUnsharedDatabase()
{
  if (!gDBConn) {
    gDBConn = getService().openUnsharedDatabase(getTestDB());
  }
  return gDBConn;
}

/**
 * Obtains a specific database to use.
 *
 * @param aFile
 *        The nsIFile representing the db file to open.
 * @returns the mozIStorageConnection for the file.
 */
function getDatabase(aFile)
{
  return getService().openDatabase(aFile);
}

function createStatement(aSQL)
{
  return getOpenedDatabase().createStatement(aSQL);
}

/**
 * Creates an asynchronous SQL statement.
 *
 * @param aSQL
 *        The SQL to parse into a statement.
 * @returns a mozIStorageAsyncStatement from aSQL.
 */
function createAsyncStatement(aSQL)
{
  return getOpenedDatabase().createAsyncStatement(aSQL);
}

/**
 * Invoke the given function and assert that it throws an exception expressing
 * the provided error code in its 'result' attribute.  JS function expressions
 * can be used to do this concisely.
 *
 * Example:
 *  expectError(Cr.NS_ERROR_INVALID_ARG, function() explodingFunction());
 *
 * @param aErrorCode
 *        The error code to expect from invocation of aFunction.
 * @param aFunction
 *        The function to invoke and expect an XPCOM-style error from.
 */
function expectError(aErrorCode, aFunction)
{
  let exceptionCaught = false;
  try {
    aFunction();
  }
  catch(e) {
    if (e.result != aErrorCode) {
      do_throw("Got an exception, but the result code was not the expected " +
               "one.  Expected " + aErrorCode + ", got " + e.result);
    }
    exceptionCaught = true;
  }
  if (!exceptionCaught)
    do_throw(aFunction + " should have thrown an exception but did not!");
}

/**
 * Run a query synchronously and verify that we get back the expected results.
 *
 * @param aSQLString
 *        The SQL string for the query.
 * @param aBind
 *        The value to bind at index 0.
 * @param aResults
 *        A list of the expected values returned in the sole result row.
 *        Express blobs as lists.
 */
function verifyQuery(aSQLString, aBind, aResults)
{
  let stmt = getOpenedDatabase().createStatement(aSQLString);
  stmt.bindByIndex(0, aBind);
  try {
    do_check_true(stmt.executeStep());
    let nCols = stmt.numEntries;
    if (aResults.length != nCols)
      do_throw("Expected " + aResults.length + " columns in result but " +
               "there are only " + aResults.length + "!");
    for (let iCol = 0; iCol < nCols; iCol++) {
      let expectedVal = aResults[iCol];
      let valType = stmt.getTypeOfIndex(iCol);
      if (expectedVal === null) {
        do_check_eq(stmt.VALUE_TYPE_NULL, valType);
        do_check_true(stmt.getIsNull(iCol));
      }
      else if (typeof(expectedVal) == "number") {
        if (Math.floor(expectedVal) == expectedVal) {
          do_check_eq(stmt.VALUE_TYPE_INTEGER, valType);
          do_check_eq(expectedVal, stmt.getInt32(iCol));
        }
        else {
          do_check_eq(stmt.VALUE_TYPE_FLOAT, valType);
          do_check_eq(expectedVal, stmt.getDouble(iCol));
        }
      }
      else if (typeof(expectedVal) == "string") {
        do_check_eq(stmt.VALUE_TYPE_TEXT, valType);
        do_check_eq(expectedVal, stmt.getUTF8String(iCol));
      }
      else { // blob
        do_check_eq(stmt.VALUE_TYPE_BLOB, valType);
        let count = { value: 0 }, blob = { value: null };
        stmt.getBlob(iCol, count, blob);
        do_check_eq(count.value, expectedVal.length);
        for (let i = 0; i < count.value; i++) {
          do_check_eq(expectedVal[i], blob.value[i]);
        }
      }
    }
  }
  finally {
    stmt.finalize();
  }
}

/**
 * Return the number of rows in the able with the given name using a synchronous
 * query.
 *
 * @param aTableName
 *        The name of the table.
 * @return The number of rows.
 */
function getTableRowCount(aTableName)
{
  var currentRows = 0;
  var countStmt = getOpenedDatabase().createStatement(
    "SELECT COUNT(1) AS count FROM " + aTableName
  );
  try {
    do_check_true(countStmt.executeStep());
    currentRows = countStmt.row.count;
  }
  finally {
    countStmt.finalize();
  }
  return currentRows;
}

cleanup();

