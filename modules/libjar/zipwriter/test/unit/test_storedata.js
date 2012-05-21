/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const DATA = "ZIP WRITER TEST DATA";
const FILENAME = "test.txt";
const FILENAME2 = "test2.txt";
const CRC = 0xe6164331;
// XXX Must use a constant time here away from DST changes. See bug 402434.
const time = 1199145600000; // Jan 1st 2008

function testpass(source)
{
  // Should exist.
  do_check_true(source.hasEntry(FILENAME));

  var entry = source.getEntry(FILENAME);
  do_check_neq(entry, null);

  do_check_false(entry.isDirectory);

  // Should be stored
  do_check_eq(entry.compression, ZIP_METHOD_STORE);

  do_check_eq(entry.lastModifiedTime / PR_USEC_PER_MSEC, time);

  // File size should match our data size.
  do_check_eq(entry.realSize, DATA.length);
  // When stored sizes should match.
  do_check_eq(entry.size, entry.realSize);

  // Check that the CRC is accurate
  do_check_eq(entry.CRC32, CRC);
}

function run_test()
{
  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  // Shouldn't be there to start with.
  do_check_false(zipW.hasEntry(FILENAME));

  do_check_false(zipW.inQueue);

  var stream = Cc["@mozilla.org/io/string-input-stream;1"]
                .createInstance(Ci.nsIStringInputStream);
  stream.setData(DATA, DATA.length);
  zipW.addEntryStream(FILENAME, time * PR_USEC_PER_MSEC,
                      Ci.nsIZipWriter.COMPRESSION_NONE, stream, false);

  // Check that zip state is right at this stage.
  testpass(zipW);
  zipW.close();

  do_check_eq(tmpFile.fileSize,
              DATA.length + ZIP_FILE_HEADER_SIZE + ZIP_CDS_HEADER_SIZE +
              (ZIP_EXTENDED_TIMESTAMP_SIZE * 2) +
              (FILENAME.length * 2) + ZIP_EOCDR_HEADER_SIZE);

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
