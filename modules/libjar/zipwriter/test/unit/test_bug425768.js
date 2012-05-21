/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const DIRNAME = "test/";
const time = Date.now();

function run_test()
{
  // Copy in the test file.
  var source = do_get_file("data/test.zip");
  source.copyTo(tmpFile.parent, tmpFile.leafName);

  // Open it and add something so the CDS is rewritten.
  zipW.open(tmpFile, PR_RDWR | PR_APPEND);
  zipW.addEntryDirectory(DIRNAME, time * PR_USEC_PER_MSEC, false);
  do_check_true(zipW.hasEntry(DIRNAME));
  zipW.close();

  var zipR = new ZipReader(tmpFile);
  do_check_true(zipR.hasEntry(DIRNAME));
  zipR.close();

  // Adding the directory would have added a fixed amount to the file size.
  // Any difference suggests the CDS was written out incorrectly.
  var extra = ZIP_FILE_HEADER_SIZE + ZIP_CDS_HEADER_SIZE +
              (DIRNAME.length * 2) + (ZIP_EXTENDED_TIMESTAMP_SIZE * 2);

  do_check_eq(source.fileSize + extra, tmpFile.fileSize);
}
