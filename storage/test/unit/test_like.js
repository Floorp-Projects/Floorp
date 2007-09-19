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
 * This code is based off of like.test from the sqlite code
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@mozilla.org> (Original Author)
 *   Shawn Wilsher <me@shawnwilsher.com>
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

// This file tests our LIKE implementation since we override it for unicode

function setup()
{
  getOpenedDatabase().createTable("t1", "x TEXT");

  var stmt = createStatement("INSERT INTO t1 (x) VALUES ('a')");
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES ('ab')");
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES ('abc')");
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES ('abcd')");
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES ('acd')");
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES ('abd')");
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES ('bc')");
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES ('bcd')");
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES ('xyz')");
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES ('ABC')");
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES ('CDE')");
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES ('ABC abc xyz')");
  stmt.execute();
  stmt.finalize();
}

function test_count()
{
  var stmt = createStatement("SELECT count(*) FROM t1;");
  do_check_true(stmt.executeStep());
  do_check_eq(stmt.getInt32(0), 12);
  stmt.reset();
  stmt.finalize();
}

function test_like_1()
{
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE 'abc';");
  var solutions = ["abc", "ABC"];
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_false(stmt.executeStep());
  stmt.reset();
  stmt.finalize();
}

function test_like_2()
{
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE 'ABC';");
  var solutions = ["abc", "ABC"];
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_false(stmt.executeStep());
  stmt.reset();
  stmt.finalize();
}
    
function test_like_3()
{
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE 'aBc';");
  var solutions = ["abc", "ABC"];
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_false(stmt.executeStep());
  stmt.reset();
  stmt.finalize();
}
   
function test_like_4()
{
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE 'abc%';");
  var solutions = ["abc", "abcd", "ABC", "ABC abc xyz"];
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_false(stmt.executeStep());
  stmt.reset();
  stmt.finalize();
}

function test_like_5()
{
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE 'a_c';");
  var solutions = ["abc", "ABC"];
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_false(stmt.executeStep());
  stmt.reset();
  stmt.finalize();
}

function test_like_6()
{
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE 'ab%d';");
  var solutions = ["abcd", "abd"];
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_false(stmt.executeStep());
  stmt.reset();
  stmt.finalize();
}
    
function test_like_7()
{
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE 'a_c%';");
  var solutions = ["abc", "abcd", "ABC", "ABC abc xyz"];
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_false(stmt.executeStep());
  stmt.reset();
  stmt.finalize();
}

function test_like_8()
{
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE '%bcd';");
  var solutions = ["abcd", "bcd"];
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_true(stmt.executeStep());
  do_check_true(solutions.indexOf(stmt.getString(0)) != -1);
  do_check_false(stmt.executeStep());
  stmt.reset();
  stmt.finalize();
}
    
var tests = [test_count, test_like_1, test_like_2, test_like_3, test_like_4, 
             test_like_5, test_like_6, test_like_7, test_like_8];

function run_test()
{
  setup();

  for (var i = 0; i < tests.length; i++)
    tests[i]();
    
  cleanup();
}

