/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the unicode functions that we have added

const LATIN1_AE = "\xc6"; // "Æ"
const LATIN1_ae = "\xe6";  // "æ"

add_task(async function setup() {
  getOpenedDatabase().createTable("test", "id INTEGER PRIMARY KEY, name TEXT");

  var stmt = createStatement("INSERT INTO test (name, id) VALUES (?1, ?2)");
  stmt.bindByIndex(0, LATIN1_AE);
  stmt.bindByIndex(1, 1);
  stmt.execute();
  stmt.bindByIndex(0, "A");
  stmt.bindByIndex(1, 2);
  stmt.execute();
  stmt.bindByIndex(0, "b");
  stmt.bindByIndex(1, 3);
  stmt.execute();
  stmt.bindByIndex(0, LATIN1_ae);
  stmt.bindByIndex(1, 4);
  stmt.execute();
  stmt.finalize();

  do_register_cleanup(cleanup);
});

add_task(async function test_upper_ascii() {
  var stmt = createStatement("SELECT name, id FROM test WHERE name = upper('a')");
  do_check_true(stmt.executeStep());
  do_check_eq("A", stmt.getString(0));
  do_check_eq(2, stmt.getInt32(1));
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_upper_non_ascii() {
  var stmt = createStatement("SELECT name, id FROM test WHERE name = upper(?1)");
  stmt.bindByIndex(0, LATIN1_ae);
  do_check_true(stmt.executeStep());
  do_check_eq(LATIN1_AE, stmt.getString(0));
  do_check_eq(1, stmt.getInt32(1));
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_lower_ascii() {
  var stmt = createStatement("SELECT name, id FROM test WHERE name = lower('B')");
  do_check_true(stmt.executeStep());
  do_check_eq("b", stmt.getString(0));
  do_check_eq(3, stmt.getInt32(1));
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_lower_non_ascii() {
  var stmt = createStatement("SELECT name, id FROM test WHERE name = lower(?1)");
  stmt.bindByIndex(0, LATIN1_AE);
  do_check_true(stmt.executeStep());
  do_check_eq(LATIN1_ae, stmt.getString(0));
  do_check_eq(4, stmt.getInt32(1));
  stmt.reset();
  stmt.finalize();
});

add_task(async function test_like_search_different() {
  var stmt = createStatement("SELECT COUNT(*) FROM test WHERE name LIKE ?1");
  stmt.bindByIndex(0, LATIN1_AE);
  do_check_true(stmt.executeStep());
  do_check_eq(2, stmt.getInt32(0));
  stmt.finalize();
});

add_task(async function test_like_search_same() {
  var stmt = createStatement("SELECT COUNT(*) FROM test WHERE name LIKE ?1");
  stmt.bindByIndex(0, LATIN1_ae);
  do_check_true(stmt.executeStep());
  do_check_eq(2, stmt.getInt32(0));
  stmt.finalize();
});
