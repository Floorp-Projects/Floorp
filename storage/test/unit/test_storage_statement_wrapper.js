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

// This file tests the functions of mozIStorageStatementWrapper

function setup()
{
  getOpenedDatabase().createTable("test", "id INTEGER PRIMARY KEY, name TEXT," +
                                          "alt_name TEXT");
}

var wrapper = new Components.Constructor("@mozilla.org/storage/statement-wrapper;1",
                                         Ci.mozIStorageStatementWrapper,
                                         "initialize");

// we want to override the default function for this file
createStatement = function(aSQL) {
  return new wrapper(getOpenedDatabase().createStatement(aSQL));
}

function test_binding_params()
{
  var stmt = createStatement("INSERT INTO test (name) VALUES (:name)");

  const name = "foo";
  stmt.params.name = name;
  stmt.execute();

  stmt = createStatement("SELECT COUNT(*) AS number FROM test");
  do_check_true(stmt.step());
  do_check_eq(1, stmt.row.number);
  stmt.reset();

  stmt = createStatement("SELECT name FROM test WHERE id = 1");
  do_check_true(stmt.step());
  do_check_eq(name, stmt.row.name);
  stmt.reset();
}

function test_binding_multiple_params()
{
  var stmt = createStatement("INSERT INTO test (name, alt_name)" +
                             "VALUES (:name, :name)");
  const name = "me";
  stmt.params.name = name;
  stmt.execute();

  stmt = createStatement("SELECT COUNT(*) AS number FROM test");
  do_check_true(stmt.step());
  do_check_eq(2, stmt.row.number);
  stmt.reset();

  stmt = createStatement("SELECT name, alt_name FROM test WHERE id = 2");
  do_check_true(stmt.step());
  do_check_eq(name, stmt.row.name);
  do_check_eq(name, stmt.row.alt_name);
  stmt.reset();
}

var tests = [test_binding_params, test_binding_multiple_params];

function run_test()
{
  setup();

  for (var i = 0; i < tests.length; i++)
    tests[i]();
    
  cleanup();
}

