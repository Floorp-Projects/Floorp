/*-*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * ***** BEGIN LICENSE BLOCK *****
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
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
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

/**
 * This file tests to make sure that SQLite was compiled with
 * SQLITE_SECURE_DELETE=1.
 */

////////////////////////////////////////////////////////////////////////////////
//// Helper Methods

/**
 * Reads the contents of a file and returns it as a string.
 *
 * @param aFile
 *        The file to return from.
 * @return the contents of the file in the form of a string.
 */
function getFileContents(aFile)
{
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(aFile, -1, 0, 0);

  let bstream = Cc["@mozilla.org/binaryinputstream;1"].
                createInstance(Ci.nsIBinaryInputStream);
  bstream.setInputStream(fstream);
  return bstream.readBytes(bstream.available());
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

function test_delete_removes_data()
{
  const TEST_STRING = "SomeRandomStringToFind";

  let file = getTestDB();
  let db = getService().openDatabase(file);

  // Create the table and insert the data.
  db.createTable("test", "data TEXT");
  let stmt = db.createStatement("INSERT INTO test VALUES(:data)");
  stmt.params.data = TEST_STRING;
  try {
    stmt.execute();
  }
  finally {
    stmt.finalize();
  }

  // Make sure this test is actually testing what it thinks by making sure the
  // string shows up in the database.  Because the previous statement was
  // automatically wrapped in a transaction, the contents are already on disk.
  let contents = getFileContents(file);
  do_check_neq(-1, contents.indexOf(TEST_STRING));

  // Delete the data, and then close the database.
  stmt = db.createStatement("DELETE FROM test WHERE data = :data");
  stmt.params.data = TEST_STRING;
  try {
    stmt.execute();
  }
  finally {
    stmt.finalize();
  }
  db.close();

  // Check the file to see if the string can be found.
  contents = getFileContents(file);
  do_check_eq(-1, contents.indexOf(TEST_STRING));

  run_next_test();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Runner

[
  test_delete_removes_data,
 ].forEach(add_test);

function run_test()
{
  cleanup();
  run_next_test();
}
