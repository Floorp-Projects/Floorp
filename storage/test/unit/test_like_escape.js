/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const LATIN1_AE = "\xc6";
const LATIN1_ae = "\xe6";

function setup() {
  getOpenedDatabase().createTable("t1", "x TEXT");

  var stmt = createStatement(
    "INSERT INTO t1 (x) VALUES ('foo/bar_baz%20cheese')"
  );
  stmt.execute();
  stmt.finalize();

  stmt = createStatement("INSERT INTO t1 (x) VALUES (?1)");
  // insert LATIN_ae, but search on LATIN_AE
  stmt.bindByIndex(0, "foo%20" + LATIN1_ae + "/_bar");
  stmt.execute();
  stmt.finalize();
}

function test_escape_for_like_ascii() {
  const paramForLike = "oo/bar_baz%20chees";
  const escapeChar = "/";

  for (const utf8 of [false, true]) {
    var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE ?1 ESCAPE '/'");
    var paramForLikeEscaped = utf8
      ? stmt.escapeUTF8StringForLIKE(paramForLike, escapeChar)
      : stmt.escapeStringForLIKE(paramForLike, escapeChar);
    // verify that we escaped / _ and %
    Assert.equal(paramForLikeEscaped, "oo//bar/_baz/%20chees");
    // prepend and append with % for "contains"
    stmt.bindByIndex(0, "%" + paramForLikeEscaped + "%");
    stmt.executeStep();
    Assert.equal("foo/bar_baz%20cheese", stmt.getString(0));
    stmt.finalize();
  }
}

function test_escape_for_like_non_ascii() {
  const paramForLike = "oo%20" + LATIN1_AE + "/_ba";
  const escapeChar = "/";

  for (const utf8 of [false, true]) {
    var stmt = createStatement("SELECT x FROM t1 WHERE x LIKE ?1 ESCAPE '/'");
    var paramForLikeEscaped = utf8
      ? stmt.escapeUTF8StringForLIKE(paramForLike, escapeChar)
      : stmt.escapeStringForLIKE(paramForLike, escapeChar);
    // verify that we escaped / _ and %
    Assert.equal(paramForLikeEscaped, "oo/%20" + LATIN1_AE + "///_ba");
    // prepend and append with % for "contains"
    stmt.bindByIndex(0, "%" + paramForLikeEscaped + "%");
    stmt.executeStep();
    Assert.equal("foo%20" + LATIN1_ae + "/_bar", stmt.getString(0));
    stmt.finalize();
  }
}

var tests = [test_escape_for_like_ascii, test_escape_for_like_non_ascii];

function run_test() {
  setup();

  for (var i = 0; i < tests.length; i++) {
    tests[i]();
  }

  cleanup();
}
