/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const DATA = "";
const FILENAME = "test.txt";
const CRC = 0x00000000;
const time = Date.now();

function testpass(source)
{
  // Should exist.
  do_check_true(source.hasEntry(FILENAME));

  var entry = source.getEntry(FILENAME);
  do_check_neq(entry, null);

  do_check_false(entry.isDirectory);

  // Should be stored
  do_check_eq(entry.compression, ZIP_METHOD_DEFLATE);

  // File size should match our data size.
  do_check_eq(entry.realSize, DATA.length);

  // Check that the CRC is accurate
  do_check_eq(entry.CRC32, CRC);
}

function run_test()
{
  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  // Shouldn't be there to start with.
  do_check_false(zipW.hasEntry(FILENAME));

  do_check_false(zipW.inQueue);

  var file = do_get_file(DATA_DIR + "emptyfile.txt");
  zipW.addEntryFile(FILENAME, Ci.nsIZipWriter.COMPRESSION_BEST, file, false);

  // Check that zip state is right at this stage.
  testpass(zipW);
  zipW.close();

  // Check to see if we get the same results loading afresh.
  zipW.open(tmpFile, PR_RDWR);
  testpass(zipW);
  zipW.close();

  // Test the stored data with the zipreader
  var zipR = new ZipReader(tmpFile);
  testpass(zipR);
  zipR.test(FILENAME);
  var stream = Cc["@mozilla.org/scriptableinputstream;1"]
                .createInstance(Ci.nsIScriptableInputStream);
  stream.init(zipR.getInputStream(FILENAME));
  var result = stream.read(DATA.length);
  stream.close();
  zipR.close();

  do_check_eq(result, DATA);
}
