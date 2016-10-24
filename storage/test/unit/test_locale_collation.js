/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Bug 499990 - Locale-aware collation
 *
 * Tests our custom, locale-aware collating sequences.
 */

// The name of the file containing the strings we'll sort during this test.
// The file's data is taken from intl/locale/tests/sort/us-ascii_base.txt and
// and intl/locale/tests/sort/us-ascii_sort.txt.
const DATA_BASENAME = "locale_collation.txt";

// The test data from DATA_BASENAME is read into this array.
var gStrings;

// A collation created from the application's locale.  Used by localeCompare().
var gLocaleCollation;

// A connection to our in-memory UTF-16-encoded database.
var gUtf16Conn;

// /////////////////////////////////////////////////////////////////////////////
// // Helper Functions

/**
 * Since we create a UTF-16 database we have to clean it up, in addition to
 * the normal cleanup of Storage tests.
 */
function cleanupLocaleTests()
{
  print("-- Cleaning up test_locale_collation.js suite.");
  gUtf16Conn.close();
  cleanup();
}

/**
 * Creates a test database similar to the default one created in
 * head_storage.js, except that this one uses UTF-16 encoding.
 *
 * @return A connection to the database.
 */
function createUtf16Database()
{
  print("Creating the in-memory UTF-16-encoded database.");
  let conn = getService().openSpecialDatabase("memory");
  conn.executeSimpleSQL("PRAGMA encoding = 'UTF-16'");

  print("Make sure the encoding was set correctly and is now UTF-16.");
  let stmt = conn.createStatement("PRAGMA encoding");
  do_check_true(stmt.executeStep());
  let enc = stmt.getString(0);
  stmt.finalize();

  // The value returned will actually be UTF-16le or UTF-16be.
  do_check_true(enc === "UTF-16le" || enc === "UTF-16be");

  return conn;
}

/**
 * Compares aActual to aExpected, ensuring that the numbers and orderings of
 * the two arrays' elements are the same.
 *
 * @param aActual
 *        An array of strings retrieved from the database.
 * @param aExpected
 *        An array of strings to which aActual should be equivalent.
 */
function ensureResultsAreCorrect(aActual, aExpected)
{
  print("Actual results:   " + aActual);
  print("Expected results: " + aExpected);

  do_check_eq(aActual.length, aExpected.length);
  for (let i = 0; i < aActual.length; i++)
    do_check_eq(aActual[i], aExpected[i]);
}

/**
 * Synchronously SELECTs all rows from the test table of the given database
 * using the given collation.
 *
 * @param  aCollation
 *         The name of one of our custom locale collations.  The rows are
 *         ordered by this collation.
 * @param  aConn
 *         A connection to either the UTF-8 database or the UTF-16 database.
 * @return The resulting strings in an array.
 */
function getResults(aCollation, aConn)
{
  let results = [];
  let stmt = aConn.createStatement("SELECT t FROM test " +
                                   "ORDER BY t COLLATE " + aCollation + " ASC");
  while (stmt.executeStep())
    results.push(stmt.row.t);
  stmt.finalize();
  return results;
}

/**
 * Inserts strings into our test table of the given database in the order given.
 *
 * @param aStrings
 *        An array of strings.
 * @param aConn
 *        A connection to either the UTF-8 database or the UTF-16 database.
 */
function initTableWithStrings(aStrings, aConn)
{
  print("Initializing test table.");

  aConn.executeSimpleSQL("DROP TABLE IF EXISTS test");
  aConn.createTable("test", "t TEXT");
  let stmt = aConn.createStatement("INSERT INTO test (t) VALUES (:t)");
  aStrings.forEach(function (str) {
    stmt.params.t = str;
    stmt.execute();
    stmt.reset();
  });
  stmt.finalize();
}

/**
 * Returns a sorting function suitable for passing to Array.prototype.sort().
 * The returned function uses the application's locale to compare strings.
 *
 * @param  aCollation
 *         The name of one of our custom locale collations.  The sorting
 *         strength is computed from this value.
 * @return A function to use as a sorting callback.
 */
function localeCompare(aCollation)
{
  var strength;

  switch (aCollation) {
    case "locale":
      strength = Ci.nsICollation.kCollationCaseInSensitive;
      break;
    case "locale_case_sensitive":
      strength = Ci.nsICollation.kCollationAccentInsenstive;
      break;
    case "locale_accent_sensitive":
      strength = Ci.nsICollation.kCollationCaseInsensitiveAscii;
      break;
    case "locale_case_accent_sensitive":
      strength = Ci.nsICollation.kCollationCaseSensitive;
      break;
    default:
      do_throw("Error in test: unknown collation '" + aCollation + "'");
      break;
  }
  return function (aStr1, aStr2) {
    return gLocaleCollation.compareString(strength, aStr1, aStr2);
  };
}

/**
 * Reads in the test data from the file DATA_BASENAME and returns it as an array
 * of strings.
 *
 * @return The test data as an array of strings.
 */
function readTestData()
{
  print("Reading in test data.");

  let file = do_get_file(DATA_BASENAME);

  let istream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  istream.init(file, -1, -1, 0);
  istream.QueryInterface(Components.interfaces.nsILineInputStream);

  let line = {};
  let lines = [];
  while (istream.readLine(line)) {
    lines.push(line.value);
  }
  istream.close();

  return lines;
}

/**
 * Gets the results from the given database using the given collation and
 * ensures that they match gStrings sorted by the same collation.
 *
 * @param aCollation
 *        The name of one of our custom locale collations.  The rows from the
 *        database and the expected results are ordered by this collation.
 * @param aConn
 *        A connection to either the UTF-8 database or the UTF-16 database.
 */
function runTest(aCollation, aConn)
{
  ensureResultsAreCorrect(getResults(aCollation, aConn),
                          gStrings.slice(0).sort(localeCompare(aCollation)));
}

/**
 * Gets the results from the UTF-8 database using the given collation and
 * ensures that they match gStrings sorted by the same collation.
 *
 * @param aCollation
 *        The name of one of our custom locale collations.  The rows from the
 *        database and the expected results are ordered by this collation.
 */
function runUtf8Test(aCollation)
{
  runTest(aCollation, getOpenedDatabase());
}

/**
 * Gets the results from the UTF-16 database using the given collation and
 * ensures that they match gStrings sorted by the same collation.
 *
 * @param aCollation
 *        The name of one of our custom locale collations.  The rows from the
 *        database and the expected results are ordered by this collation.
 */
function runUtf16Test(aCollation)
{
  runTest(aCollation, gUtf16Conn);
}

/**
 * Sets up the test suite.
 */
function setup()
{
  print("-- Setting up the test_locale_collation.js suite.");

  gStrings = readTestData();

  initTableWithStrings(gStrings, getOpenedDatabase());

  gUtf16Conn = createUtf16Database();
  initTableWithStrings(gStrings, gUtf16Conn);

  let localeSvc = Cc["@mozilla.org/intl/nslocaleservice;1"].
                  getService(Ci.nsILocaleService);
  let collFact = Cc["@mozilla.org/intl/collation-factory;1"].
                 createInstance(Ci.nsICollationFactory);
  gLocaleCollation = collFact.CreateCollation(localeSvc.getApplicationLocale());
}

// /////////////////////////////////////////////////////////////////////////////
// // Test Runs

var gTests = [
  {
    desc: "Case and accent sensitive UTF-8",
    run:   () => runUtf8Test("locale_case_accent_sensitive")
  },

  {
    desc: "Case sensitive, accent insensitive UTF-8",
    run:   () => runUtf8Test("locale_case_sensitive")
  },

  {
    desc: "Case insensitive, accent sensitive UTF-8",
    run:   () => runUtf8Test("locale_accent_sensitive")
  },

  {
    desc: "Case and accent insensitive UTF-8",
    run:   () => runUtf8Test("locale")
  },

  {
    desc: "Case and accent sensitive UTF-16",
    run:   () => runUtf16Test("locale_case_accent_sensitive")
  },

  {
    desc: "Case sensitive, accent insensitive UTF-16",
    run:   () => runUtf16Test("locale_case_sensitive")
  },

  {
    desc: "Case insensitive, accent sensitive UTF-16",
    run:   () => runUtf16Test("locale_accent_sensitive")
  },

  {
    desc: "Case and accent insensitive UTF-16",
    run:   () => runUtf16Test("locale")
  },
];

function run_test()
{
  setup();
  gTests.forEach(function (test) {
    print("-- Running test: " + test.desc);
    test.run();
  });
}
