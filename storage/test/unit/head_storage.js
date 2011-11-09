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

const Ci = Components.interfaces;
const Cc = Components.classes;
const Cr = Components.results;

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
 * if necessary, otherwise reuses the existing cached connection.
 *
 * @param unshared {boolean}
 *        whether or not to open a connection to the database that doesn't share
 *        its cache; if true, we use mozIStorageService::openUnsharedDatabase
 *        to create the connection; otherwise we use openDatabase.
 * @returns the mozIStorageConnection for the file.
 */
function getOpenedDatabase(unshared)
{
  if (!gDBConn) {
    gDBConn = getService()
              [unshared ? "openUnsharedDatabase" : "openDatabase"]
              (getTestDB());
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

