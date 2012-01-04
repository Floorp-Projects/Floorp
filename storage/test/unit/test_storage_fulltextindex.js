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
 *   Myk Melez <myk@mozilla.org> (Original Author)
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

// This file tests support for the fts3 (full-text index) module.

// Example statements in these tests are taken from the Full Text Index page
// on the SQLite wiki: http://www.sqlite.org/cvstrac/wiki?p=FullTextIndex

function test_table_creation()
{
  var msc = getOpenedUnsharedDatabase();

  msc.executeSimpleSQL(
    "CREATE VIRTUAL TABLE recipe USING fts3(name, ingredients)");

  do_check_true(msc.tableExists("recipe"));
}

function test_insertion()
{
  var msc = getOpenedUnsharedDatabase();

  msc.executeSimpleSQL("INSERT INTO recipe (name, ingredients) VALUES " +
                       "('broccoli stew', 'broccoli peppers cheese tomatoes')");
  msc.executeSimpleSQL("INSERT INTO recipe (name, ingredients) VALUES " +
                       "('pumpkin stew', 'pumpkin onions garlic celery')");
  msc.executeSimpleSQL("INSERT INTO recipe (name, ingredients) VALUES " +
                       "('broccoli pie', 'broccoli cheese onions flour')");
  msc.executeSimpleSQL("INSERT INTO recipe (name, ingredients) VALUES " +
                       "('pumpkin pie', 'pumpkin sugar flour butter')");

  var stmt = msc.createStatement("SELECT COUNT(*) FROM recipe");
  stmt.executeStep();

  do_check_eq(stmt.getInt32(0), 4);

  stmt.reset();
  stmt.finalize();
}

function test_selection()
{
  var msc = getOpenedUnsharedDatabase();

  var stmt = msc.createStatement(
    "SELECT rowid, name, ingredients FROM recipe WHERE name MATCH 'pie'");

  do_check_true(stmt.executeStep());
  do_check_eq(stmt.getInt32(0), 3);
  do_check_eq(stmt.getString(1), "broccoli pie");
  do_check_eq(stmt.getString(2), "broccoli cheese onions flour");

  do_check_true(stmt.executeStep());
  do_check_eq(stmt.getInt32(0), 4);
  do_check_eq(stmt.getString(1), "pumpkin pie");
  do_check_eq(stmt.getString(2), "pumpkin sugar flour butter");

  do_check_false(stmt.executeStep());

  stmt.reset();
  stmt.finalize();
}

var tests = [test_table_creation, test_insertion, test_selection];

function run_test()
{
  // It's extra important to start from scratch, since these tests won't work
  // with an existing shared cache connection, so we do it even though the last
  // test probably did it already.
  cleanup();

  try {
    for (var i = 0; i < tests.length; i++) {
      tests[i]();
    }
  }
  // It's extra important to clean up afterwards, since later tests that use
  // a shared cache connection will not be able to read the database we create,
  // so we do this in a finally block to ensure it happens even if some of our
  // tests fail.
  finally {
    cleanup();
  }
}
