/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const DATA = "ZIP WRITER TEST DATA";
const FILENAME = "test.txt";
const CRC = 0xe6164331;
// XXX Must use a constant time here away from DST changes. See bug 402434.
const time = 1199145600000; // Jan 1st 2008

function run_test()
{
  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  // Shouldn't be there to start with.
  Assert.ok(!zipW.hasEntry(FILENAME));

  Assert.ok(!zipW.inQueue);

  var stream = Cc["@mozilla.org/io/string-input-stream;1"]
                .createInstance(Ci.nsIStringInputStream);
  stream.setData(DATA, DATA.length);
  zipW.addEntryStream(FILENAME, time * PR_USEC_PER_MSEC,
                      Ci.nsIZipWriter.COMPRESSION_BEST, stream, false);

  var entry = zipW.getEntry(FILENAME);

  Assert.ok(entry != null);

  // Check entry seems right.
  Assert.equal(entry.compression, ZIP_METHOD_DEFLATE);
  Assert.equal(entry.CRC32, CRC);
  Assert.equal(entry.realSize, DATA.length);
  Assert.equal(entry.lastModifiedTime / PR_USEC_PER_MSEC, time);

  zipW.close();

  // Test the stored data with the zipreader
  var zipR = new ZipReader(tmpFile);
  Assert.ok(zipR.hasEntry(FILENAME));

  zipR.test(FILENAME);

  var stream = Cc["@mozilla.org/scriptableinputstream;1"]
                .createInstance(Ci.nsIScriptableInputStream);
  stream.init(zipR.getInputStream(FILENAME));
  var result = stream.read(DATA.length);
  stream.close();
  zipR.close();

  Assert.equal(result, DATA);
}
