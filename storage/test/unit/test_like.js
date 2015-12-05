/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

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
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE ?;");
  stmt.bindByIndex(0, 'abc');
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
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE ?;");
  stmt.bindByIndex(0, 'ABC');
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
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE ?;");
  stmt.bindByIndex(0, 'aBc');
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
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE ?;");
  stmt.bindByIndex(0, 'abc%');
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
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE ?;");
  stmt.bindByIndex(0, 'a_c');
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
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE ?;");
  stmt.bindByIndex(0, 'ab%d');
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
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE ?;");
  stmt.bindByIndex(0, 'a_c%');
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
  var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE ?;");
  stmt.bindByIndex(0, '%bcd');
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

  for (var i = 0; i < tests.length; i++) {
    tests[i]();
  }

  cleanup();
}

