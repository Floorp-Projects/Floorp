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
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ondrej Brablc <ondrej@allpeers.com> (Original Author)
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

// List of connections to be tested. Shared property means that the
// connection should be created with shared cache.
// The name of the property is used as a key in the hash arrays of 
// databases and connections to them. Any key name can be used here.
const CONN_LIST =
{
  S1: { shared: true },
  P1: { shared: false },
  S2: { shared: true },
  P2: { shared: false }
};

/**
* New testing class. It holds database connections.
*/
function TestBody()
{
  this.conn = new Array();
  this.dbFile = new Array();

  for (var curConn in CONN_LIST) {
    var db = dirSvc.get("ProfD", Ci.nsIFile);
    db.append("test_storage_" + curConn + ".sqlite");
    this.dbFile[curConn] = db;
  }

  this.cleanUp();
}

/**
* Remove databases if they exist
*/
TestBody.prototype.cleanUp = function cleanUp()
{
  for (var curConn in CONN_LIST) {
    if (this.dbFile[curConn].exists()) {
      try {
        this.dbFile[curConn].remove(false);
      }
      catch(e) {
      /* stupid windows box */
      }
    }
  }
}

/**
* Open connection to database (create database)
*/
TestBody.prototype.test_initialize_database = 
function test_initialize_database()
{
  for (var curConn in CONN_LIST) {
    if (CONN_LIST[curConn].shared)
      this.conn[curConn] = getService().openDatabase(this.dbFile[curConn]);
    else
      this.conn[curConn] = getService().openUnsharedDatabase(
                                          this.dbFile[curConn]);
    do_check_true(this.conn[curConn].connectionReady);
  }
}

/**
* Create normal table "test" and table "book" - if possible as virtual table.
* Creating virtual table in shared database is expected to fail and throw.
* This is the most important test in this module. The rest of the test just
* tests that it is possible to use tables properly.
*/
TestBody.prototype.test_create_tables = 
function test_create_tables()
{
  var realSql = "CREATE TABLE book (author TEXT, title TEXT)";
  var virtSql = "CREATE VIRTUAL TABLE book USING fts3(author, title)";

  for (var curConn in CONN_LIST) {
    this.conn[curConn].createTable("test", "id INTEGER PRIMARY KEY, name TEXT");
    do_check_true(this.conn[curConn].tableExists("test"));

    try {
      this.conn[curConn].executeSimpleSQL(virtSql);
      if (CONN_LIST[curConn].shared) // Statement above must throw in this case
        do_throw("We shouldn't be able to create virtual tables on " +
                 curConn + " database!");
    }
    catch (e) {
      // If the try threw, we have shared database and create the
      // table as non virtual.
      this.conn[curConn].executeSimpleSQL(realSql);
    }

    do_check_true(this.conn[curConn].tableExists("book"));
  }
}

/**
* Open transaction to all our databases, perform INSERT followed by SELECT
* and commit. Uses the normal table "test".
*/
TestBody.prototype.test_real_table_insert_select = 
function test_real_table_insert_select()
{
  var stmts = new Array();

  for (var curConn in CONN_LIST)
    this.conn[curConn].beginTransaction();

  for (var curConn in CONN_LIST)
    this.conn[curConn].executeSimpleSQL(
        "INSERT INTO test (name) VALUES ('Test')");

  for (var curConn in CONN_LIST)
    stmts[curConn] = this.conn[curConn].createStatement("SELECT * FROM test");

  for (var curConn in CONN_LIST)
    stmts[curConn].executeStep();

  for (var curConn in CONN_LIST)
    do_check_eq(1, stmts[curConn].getInt32(0));

  for (var curConn in CONN_LIST)
    stmts[curConn].reset();

  for (var curConn in CONN_LIST)
    stmts[curConn].finalize();

  for (var curConn in CONN_LIST)
    this.conn[curConn].commitTransaction();
}

/**
* Open transaction to all our databases, perform INSERT followed by SELECT
* and commit. Uses the table "book" which is virtual for non-shared databases.
*/
TestBody.prototype.test_virtual_table_insert_select = 
function test_virtual_table_insert_select()
{
  var stmts = new Array();

  for (var curConn in CONN_LIST)
    this.conn[curConn].beginTransaction();

  for (var curConn in CONN_LIST)
    this.conn[curConn].executeSimpleSQL(
        "INSERT INTO book VALUES ('Frank Herbert', 'The Dune')");

  for (var curConn in CONN_LIST)
    stmts[curConn] = this.conn[curConn].createStatement(
                         "SELECT * FROM book WHERE author >= 'Frank'");

  for (var curConn in CONN_LIST)
    stmts[curConn].executeStep();

  for (var curConn in CONN_LIST)
    do_check_eq("Frank Herbert", stmts[curConn].getString(0));

  for (var curConn in CONN_LIST)
    stmts[curConn].reset();

  for (var curConn in CONN_LIST)
    stmts[curConn].finalize();

  for (var curConn in CONN_LIST)
    this.conn[curConn].commitTransaction();
}

var tests = [
  "test_initialize_database",
  "test_create_tables",
  "test_real_table_insert_select",
  "test_virtual_table_insert_select"
];

function run_test()
{
  var tb = new TestBody;

  try {
    for (var i = 0; i < tests.length; ++i)
      tb[tests[i]]();
  }
  finally {
    for (var curConn in CONN_LIST) {
      var errStr = tb.conn[curConn].lastErrorString;
      if (errStr != "not an error") // If we had an error, we want to see it
        print("*** Database error: " + errStr);
      tb.conn[curConn].close();
    }
    tb.cleanUp(); // We always want to cleanup our databases
  }
}
