/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test()
{
  // In this test we try to open some files that aren't archives:
  //  - An empty file, that is certainly not an archive.
  //  - A file that couldn't be mistaken for archive, since it is too small.
  //  - A file that could be mistaken for archive, if we checked only the file
  //     size, but is invalid since it contains no ZIP signature.
  var invalidArchives = ["emptyfile.txt", "smallfile.txt", "test.png"];

  invalidArchives.forEach(function(invalidArchive) {
    // Get a reference to the invalid file
    var invalidFile = do_get_file(DATA_DIR + invalidArchive);

    // Opening the invalid file should fail (but not crash)
    try {
      zipW.open(invalidFile, PR_RDWR);
      do_throw("Should have thrown NS_ERROR_FILE_CORRUPTED on " +
               invalidArchive + " !");
    } catch (e if (e instanceof Ci.nsIException &&
                   e.result == Components.results.NS_ERROR_FILE_CORRUPTED)) {
      // do nothing
    }
  });
}
