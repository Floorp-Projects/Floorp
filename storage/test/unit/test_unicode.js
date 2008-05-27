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

// This file tests the unicode functions that we have added

const LATIN1_AE = "\xc6"; // "Æ"
const LATIN1_ae = "\xe6";  // "æ"

function setup()
{
  getOpenedDatabase().createTable("test", "id INTEGER PRIMARY KEY, name TEXT");

  var stmt = createStatement("INSERT INTO test (name, id) VALUES (?1, ?2)");
  stmt.bindStringParameter(0, LATIN1_AE);
  stmt.bindInt32Parameter(1, 1);
  stmt.execute();
  stmt.bindStringParameter(0, "A");
  stmt.bindInt32Parameter(1, 2);
  stmt.execute();
  stmt.bindStringParameter(0, "b");
  stmt.bindInt32Parameter(1, 3);
  stmt.execute();
  stmt.bindStringParameter(0, LATIN1_ae);
  stmt.bindInt32Parameter(1, 4);
  stmt.execute();
  stmt.finalize();
}

function test_upper_ascii()
{
  var stmt = createStatement("SELECT name, id FROM test WHERE name = upper('a')");
  do_check_true(stmt.executeStep());
  do_check_eq("A", stmt.getString(0));
  do_check_eq(2, stmt.getInt32(1));
  stmt.reset();
  stmt.finalize();
}

function test_upper_non_ascii()
{
  var stmt = createStatement("SELECT name, id FROM test WHERE name = upper(?1)");
  stmt.bindStringParameter(0, LATIN1_ae);
  do_check_true(stmt.executeStep());
  do_check_eq(LATIN1_AE, stmt.getString(0));
  do_check_eq(1, stmt.getInt32(1));
  stmt.reset();
  stmt.finalize();
}

function test_lower_ascii()
{
  var stmt = createStatement("SELECT name, id FROM test WHERE name = lower('B')");
  do_check_true(stmt.executeStep());
  do_check_eq("b", stmt.getString(0));
  do_check_eq(3, stmt.getInt32(1));
  stmt.reset();
  stmt.finalize();
}

function test_lower_non_ascii()
{
  var stmt = createStatement("SELECT name, id FROM test WHERE name = lower(?1)");
  stmt.bindStringParameter(0, LATIN1_AE);
  do_check_true(stmt.executeStep());
  do_check_eq(LATIN1_ae, stmt.getString(0));
  do_check_eq(4, stmt.getInt32(1));
  stmt.reset();
  stmt.finalize();
}

function test_like_search_different()
{
  var stmt = createStatement("SELECT COUNT(*) FROM test WHERE name LIKE ?1");
  stmt.bindStringParameter(0, LATIN1_AE);
  do_check_true(stmt.executeStep());
  do_check_eq(2, stmt.getInt32(0));
  stmt.finalize();
}

function test_like_search_same()
{
  var stmt = createStatement("SELECT COUNT(*) FROM test WHERE name LIKE ?1");
  stmt.bindStringParameter(0, LATIN1_ae);
  do_check_true(stmt.executeStep());
  do_check_eq(2, stmt.getInt32(0));
  stmt.finalize();
}

var tests = [test_upper_ascii, test_upper_non_ascii, test_lower_ascii,
             test_lower_non_ascii, test_like_search_different,
             test_like_search_same];

function run_test()
{
  setup();

  for (var i = 0; i < tests.length; i++)
    tests[i]();
    
  cleanup();
}

