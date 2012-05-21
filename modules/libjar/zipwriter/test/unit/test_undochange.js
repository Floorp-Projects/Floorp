/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Values taken from using zipinfo to list the test.zip contents
var TESTS = [
  "test.txt",
  "test.png"
];

function run_test()
{
  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  for (var i = 0; i < TESTS.length; i++) {
    var source = do_get_file(DATA_DIR + TESTS[i]);
    zipW.addEntryFile(TESTS[i], Ci.nsIZipWriter.COMPRESSION_NONE, source,
                      false);
  }

  try {
    var source = do_get_file(DATA_DIR + TESTS[0]);
    zipW.addEntryFile(TESTS[0], Ci.nsIZipWriter.COMPRESSION_NONE, source,
                      false);
    do_throw("Should not be able to add the same file twice");
  }
  catch (e) {
    do_check_eq(e.result, Components.results.NS_ERROR_FILE_ALREADY_EXISTS);
  }

  // Remove all the tests and see if we are left with an empty zip
  for (var i = 0; i < TESTS.length; i++) {
    zipW.removeEntry(TESTS[i], false);
  }

  zipW.close();

  // Empty zip file should just be the end of central directory marker
  var newTmpFile = tmpFile.clone();
  do_check_eq(newTmpFile.fileSize, ZIP_EOCDR_HEADER_SIZE);
}
