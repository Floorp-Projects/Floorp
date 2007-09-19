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

// This file tests the functions of mozIStorageStatement

function setup()
{
  getOpenedDatabase().createTable("test", "id INTEGER PRIMARY KEY, name TEXT");
}

function test_parameterCount_none()
{
  var stmt = createStatement("SELECT * FROM test");
  do_check_eq(0, stmt.parameterCount);
  stmt.reset();
  stmt.finalize();
}

function test_parameterCount_one()
{
  var stmt = createStatement("SELECT * FROM test WHERE id = ?1");
  do_check_eq(1, stmt.parameterCount);
  stmt.reset();
  stmt.finalize();
}

function test_getParameterName()
{
  var stmt = createStatement("SELECT * FROM test WHERE id = :id");
  do_check_eq(":id", stmt.getParameterName(0));
  stmt.reset();
  stmt.finalize();
}

function test_getParameterIndex_different()
{
  var stmt = createStatement("SELECT * FROM test WHERE id = :id OR name = :name");
  do_check_eq(0, stmt.getParameterIndex(":id"));
  do_check_eq(1, stmt.getParameterIndex(":name"));
  stmt.reset();
  stmt.finalize();
}

function test_getParameterIndex_same()
{
  var stmt = createStatement("SELECT * FROM test WHERE id = @test OR name = @test");
  do_check_eq(0, stmt.getParameterIndex("@test"));
  stmt.reset();
  stmt.finalize();
}

function test_columnCount()
{
  var stmt = createStatement("SELECT * FROM test WHERE id = ?1 OR name = ?2");
  do_check_eq(2, stmt.columnCount);
  stmt.reset();
  stmt.finalize();
}

function test_getColumnName()
{
  var stmt = createStatement("SELECT name, id FROM test");
  do_check_eq("id", stmt.getColumnName(1));
  do_check_eq("name", stmt.getColumnName(0));
  stmt.reset();
  stmt.finalize();
}

function test_getColumnIndex_same_case()
{
  var stmt = createStatement("SELECT name, id FROM test");
  do_check_eq(0, stmt.getColumnIndex("name"));
  do_check_eq(1, stmt.getColumnIndex("id"));
  stmt.reset();
  stmt.finalize();
}

function test_getColumnIndex_different_case()
{
  var stmt = createStatement("SELECT name, id FROM test");
  try {
    do_check_eq(0, stmt.getColumnIndex("NaMe"));
    do_throw("should not get here");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
  try {
  do_check_eq(1, stmt.getColumnIndex("Id"));
    do_throw("should not get here");
  } catch (e) {
    do_check_eq(Cr.NS_ERROR_INVALID_ARG, e.result);
  }
  stmt.reset();
  stmt.finalize();
}

function test_state_ready()
{
  var stmt = createStatement("SELECT name, id FROM test");
  do_check_eq(Ci.mozIStorageStatement.MOZ_STORAGE_STATEMENT_READY, stmt.state);
  stmt.reset();
  stmt.finalize();
}

function test_state_executing()
{
  var stmt = createStatement("INSERT INTO test (name) VALUES ('foo')");
  stmt.execute();
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("SELECT name, id FROM test");
  stmt.executeStep();
  do_check_eq(Ci.mozIStorageStatement.MOZ_STORAGE_STATEMENT_EXECUTING,
              stmt.state);
  stmt.executeStep();
  do_check_eq(Ci.mozIStorageStatement.MOZ_STORAGE_STATEMENT_EXECUTING,
              stmt.state);
  stmt.reset();
  do_check_eq(Ci.mozIStorageStatement.MOZ_STORAGE_STATEMENT_READY, stmt.state);
  stmt.finalize();
}

function test_state_after_finalize()
{
  var stmt = createStatement("SELECT name, id FROM test");
  stmt.executeStep();
  stmt.finalize();
  do_check_eq(Ci.mozIStorageStatement.MOZ_STORAGE_STATEMENT_INVALID, stmt.state);
}

var tests = [test_parameterCount_none, test_parameterCount_one,
             test_getParameterName, test_getParameterIndex_different,
             test_getParameterIndex_same, test_columnCount,
             test_getColumnName, test_getColumnIndex_same_case,
             test_getColumnIndex_different_case, test_state_ready,
             test_state_executing, test_state_after_finalize];

function run_test()
{
  setup();

  for (var i = 0; i < tests.length; i++)
    tests[i]();
    
  cleanup();
}

