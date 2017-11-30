/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 *vim: sw=2 ts=2 et lcs=trail\:.,tab\:>~ :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file tests to make sure that SQLite was compiled with
 * SQLITE_SECURE_DELETE=1.
 */

// Helper Methods

/**
 * Reads the contents of a file and returns it as a string.
 *
 * @param aFile
 *        The file to return from.
 * @return the contents of the file in the form of a string.
 */
function getFileContents(aFile) {
  let fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(aFile, -1, 0, 0);

  let bstream = Cc["@mozilla.org/binaryinputstream;1"].
                createInstance(Ci.nsIBinaryInputStream);
  bstream.setInputStream(fstream);
  return bstream.readBytes(bstream.available());
}

// Tests

add_test(function test_delete_removes_data() {
  const TEST_STRING = "SomeRandomStringToFind";

  let file = getTestDB();
  let db = Services.storage.openDatabase(file);

  // Create the table and insert the data.
  db.createTable("test", "data TEXT");
  let stmt = db.createStatement("INSERT INTO test VALUES(:data)");
  stmt.params.data = TEST_STRING;
  try {
    stmt.execute();
  } finally {
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
  } finally {
    stmt.finalize();
  }
  db.close();

  // Check the file to see if the string can be found.
  contents = getFileContents(file);
  do_check_eq(-1, contents.indexOf(TEST_STRING));

  run_next_test();
});

function run_test() {
  cleanup();
  run_next_test();
}
