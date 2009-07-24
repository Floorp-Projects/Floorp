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
 * Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Curtis Bartley <cbartley@mozilla.com> (Original Author)
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

// This file tests the Levenshtein Distance function we've registered.

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

function check_levenshtein(db, s, t, expectedDistance)
{
  var stmt = db.createStatement("SELECT levenshteinDistance(:s, :t) AS result");
  stmt.params.s = s;
  stmt.params.t = t;
  try {
    do_check_true(stmt.executeStep());
    do_check_eq(expectedDistance, stmt.row.result);
  } 
  finally {
    stmt.reset();
    stmt.finalize();
  }
}

function testLevenshtein(db)
{
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

function run_test()
{
  testLevenshtein(getOpenedDatabase());
  testLevenshtein(createUtf16Database());
}




