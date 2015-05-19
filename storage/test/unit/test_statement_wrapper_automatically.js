/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
   vim:set ts=2 sw=2 sts=2 et:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the functions of mozIStorageStatementWrapper

function setup()
{
  getOpenedDatabase().createTable("test", "id INTEGER PRIMARY KEY, val NONE," +
                                          "alt_val NONE");
}

/**
 * A convenience wrapper for do_check_eq.  Calls do_check_eq on aActualVal
 * and aReturnedVal, with one caveat.
 *
 * Date objects are converted before parameter binding to PRTime's (microsecs
 * since epoch).  They are not reconverted when retrieved from the database.
 * This function abstracts away this reconversion so that you can pass in,
 * for example:
 *
 *   checkVal(new Date(), aReturnedVal)                    // this
 *   checkVal(new Date().valueOf() * 1000.0, aReturnedVal) // instead of this
 *
 * Should any other types require conversion in the future, their conversions
 * may also be abstracted away here.
 *
 * @param aActualVal
 *        the value inserted into the database
 * @param aReturnedVal
 *        the value retrieved from the database
 */
function checkVal(aActualVal, aReturnedVal)
{
  if (aActualVal instanceof Date) aActualVal = aActualVal.valueOf() * 1000.0;
  do_check_eq(aActualVal, aReturnedVal);
}

/**
 * Removes all rows from our test table.
 */
function clearTable()
{
  var stmt = createStatement("DELETE FROM test");
  stmt.execute();
  stmt.finalize();
  ensureNumRows(0);
}

/**
 * Ensures that the number of rows in our test table is equal to aNumRows.
 * Calls do_check_eq on aNumRows and the value retrieved by SELECT'ing COUNT(*).
 *
 * @param aNumRows
 *        the number of rows our test table should contain
 */
function ensureNumRows(aNumRows)
{
  var stmt = createStatement("SELECT COUNT(*) AS number FROM test");
  do_check_true(stmt.step());
  do_check_eq(aNumRows, stmt.row.number);
  stmt.reset();
  stmt.finalize();
}

/**
 * Inserts aVal into our test table and checks that insertion was successful by
 * retrieving the newly inserted value from the database and comparing it
 * against aVal.  aVal is bound to a single parameter.
 *
 * @param aVal
 *        value to insert into our test table and check
 */
function insertAndCheckSingleParam(aVal)
{
  clearTable();

  var stmt = createStatement("INSERT INTO test (val) VALUES (:val)");
  stmt.params.val = aVal;
  stmt.execute();
  stmt.finalize();

  ensureNumRows(1);

  stmt = createStatement("SELECT val FROM test WHERE id = 1");
  do_check_true(stmt.step());
  checkVal(aVal, stmt.row.val);
  stmt.reset();
  stmt.finalize();
}

/**
 * Inserts aVal into our test table and checks that insertion was successful by
 * retrieving the newly inserted value from the database and comparing it
 * against aVal.  aVal is bound to two separate parameters, both of which are
 * checked against aVal.
 *
 * @param aVal
 *        value to insert into our test table and check
 */
function insertAndCheckMultipleParams(aVal)
{
  clearTable();

  var stmt = createStatement("INSERT INTO test (val, alt_val) " +
                             "VALUES (:val, :val)");
  stmt.params.val = aVal;
  stmt.execute();
  stmt.finalize();

  ensureNumRows(1);

  stmt = createStatement("SELECT val, alt_val FROM test WHERE id = 1");
  do_check_true(stmt.step());
  checkVal(aVal, stmt.row.val);
  checkVal(aVal, stmt.row.alt_val);
  stmt.reset();
  stmt.finalize();
}

/**
 * A convenience function that prints out a description of aVal using
 * aVal.toString and aVal.toSource.  Output is useful when the test fails.
 *
 * @param aVal
 *        a value inserted or to be inserted into our test table
 */
function printValDesc(aVal)
{
  try
  {
    var toSource = aVal.toSource();
  }
  catch (exc)
  {
    toSource = "";
  }
  print("Testing value: toString=" + aVal +
        (toSource ? " toSource=" + toSource : ""));
}

function run_test()
{
  setup();

  // function JSValStorageStatementBinder in
  // storage/mozStorageStatementParams.cpp tells us that the following types
  // and only the following types are valid as statement parameters:
  var vals = [
    1337,       // int
    3.1337,     // double
    "foo",      // string
    true,       // boolean
    null,       // null
    new Date(), // Date object
  ];

  vals.forEach(function (val)
  {
    printValDesc(val);
    print("Single parameter");
    insertAndCheckSingleParam(val);
    print("Multiple parameters");
    insertAndCheckMultipleParams(val)
  });

  cleanup();
}
