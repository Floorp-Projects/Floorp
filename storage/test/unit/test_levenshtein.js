/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file tests the Levenshtein Distance function we've registered.

function createUtf16Database() {
  print("Creating the in-memory UTF-16-encoded database.");
  let conn = Services.storage.openSpecialDatabase("memory");
  conn.executeSimpleSQL("PRAGMA encoding = 'UTF-16'");

  print("Make sure the encoding was set correctly and is now UTF-16.");
  let stmt = conn.createStatement("PRAGMA encoding");
  Assert.ok(stmt.executeStep());
  let enc = stmt.getString(0);
  stmt.finalize();

  // The value returned will actually be UTF-16le or UTF-16be.
  Assert.ok(enc === "UTF-16le" || enc === "UTF-16be");

  return conn;
}

function check_levenshtein(db, s, t, expectedDistance) {
  var stmt = db.createStatement("SELECT levenshteinDistance(:s, :t) AS result");
  stmt.params.s = s;
  stmt.params.t = t;
  try {
    Assert.ok(stmt.executeStep());
    Assert.equal(expectedDistance, stmt.row.result);
  } finally {
    stmt.reset();
    stmt.finalize();
  }
}

function testLevenshtein(db) {
  // Basic tests.
  check_levenshtein(db, "", "", 0);
  check_levenshtein(db, "foo", "", 3);
  check_levenshtein(db, "", "bar", 3);
  check_levenshtein(db, "yellow", "hello", 2);
  check_levenshtein(db, "gumbo", "gambol", 2);
  check_levenshtein(db, "kitten", "sitten", 1);
  check_levenshtein(db, "sitten", "sittin", 1);
  check_levenshtein(db, "sittin", "sitting", 1);
  check_levenshtein(db, "kitten", "sitting", 3);
  check_levenshtein(db, "Saturday", "Sunday", 3);
  check_levenshtein(db, "YHCQPGK", "LAHYQQKPGKA", 6);

  // Test SQL NULL handling.
  check_levenshtein(db, "foo", null, null);
  check_levenshtein(db, null, "bar", null);
  check_levenshtein(db, null, null, null);

  // The levenshteinDistance function allocates temporary memory on the stack
  // if it can.  Test some strings long enough to force a heap allocation.
  var dots1000 = Array(1001).join(".");
  var dashes1000 = Array(1001).join("-");
  check_levenshtein(db, dots1000, dashes1000, 1000);
}

function run_test() {
  testLevenshtein(getOpenedDatabase());
  testLevenshtein(createUtf16Database());
}
