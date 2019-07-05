/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests support for the fts3 (full-text index) module.

// Example statements in these tests are taken from the Full Text Index page
// on the SQLite wiki: http://www.sqlite.org/cvstrac/wiki?p=FullTextIndex

function test_table_creation() {
  var msc = getOpenedUnsharedDatabase();

  msc.executeSimpleSQL(
    "CREATE VIRTUAL TABLE recipe USING fts3(name, ingredients)"
  );

  Assert.ok(msc.tableExists("recipe"));
}

function test_insertion() {
  var msc = getOpenedUnsharedDatabase();

  msc.executeSimpleSQL(
    "INSERT INTO recipe (name, ingredients) VALUES " +
      "('broccoli stew', 'broccoli peppers cheese tomatoes')"
  );
  msc.executeSimpleSQL(
    "INSERT INTO recipe (name, ingredients) VALUES " +
      "('pumpkin stew', 'pumpkin onions garlic celery')"
  );
  msc.executeSimpleSQL(
    "INSERT INTO recipe (name, ingredients) VALUES " +
      "('broccoli pie', 'broccoli cheese onions flour')"
  );
  msc.executeSimpleSQL(
    "INSERT INTO recipe (name, ingredients) VALUES " +
      "('pumpkin pie', 'pumpkin sugar flour butter')"
  );

  var stmt = msc.createStatement("SELECT COUNT(*) FROM recipe");
  stmt.executeStep();

  Assert.equal(stmt.getInt32(0), 4);

  stmt.reset();
  stmt.finalize();
}

function test_selection() {
  var msc = getOpenedUnsharedDatabase();

  var stmt = msc.createStatement(
    "SELECT rowid, name, ingredients FROM recipe WHERE name MATCH 'pie'"
  );

  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.getInt32(0), 3);
  Assert.equal(stmt.getString(1), "broccoli pie");
  Assert.equal(stmt.getString(2), "broccoli cheese onions flour");

  Assert.ok(stmt.executeStep());
  Assert.equal(stmt.getInt32(0), 4);
  Assert.equal(stmt.getString(1), "pumpkin pie");
  Assert.equal(stmt.getString(2), "pumpkin sugar flour butter");

  Assert.ok(!stmt.executeStep());

  stmt.reset();
  stmt.finalize();
}

var tests = [test_table_creation, test_insertion, test_selection];

function run_test() {
  // It's extra important to start from scratch, since these tests won't work
  // with an existing shared cache connection, so we do it even though the last
  // test probably did it already.
  cleanup();

  try {
    for (var i = 0; i < tests.length; i++) {
      tests[i]();
    }
  } finally {
    // It's extra important to clean up afterwards, since later tests that use
    // a shared cache connection will not be able to read the database we create,
    // so we do this in a finally block to ensure it happens even if some of our
    // tests fail.
    cleanup();
  }
}
