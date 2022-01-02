/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

// Values taken from using zipinfo to list the test.zip contents
var TESTS = [
  {
    name: "test.txt",
    size: 232,
    crc: 0x0373ac26,
  },
  {
    name: "test.png",
    size: 3402,
    crc: 0x504a5c30,
  },
];

function run_test() {
  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  var size = 0;
  for (let i = 0; i < TESTS.length; i++) {
    let source = do_get_file(DATA_DIR + TESTS[i].name);
    zipW.addEntryFile(
      TESTS[i].name,
      Ci.nsIZipWriter.COMPRESSION_NONE,
      source,
      false
    );
    size +=
      ZIP_FILE_HEADER_SIZE +
      ZIP_CDS_HEADER_SIZE +
      ZIP_EXTENDED_TIMESTAMP_SIZE * 2 +
      TESTS[i].name.length * 2 +
      TESTS[i].size;
  }

  zipW.close();
  size += ZIP_EOCDR_HEADER_SIZE;

  Assert.equal(size, tmpFile.fileSize);

  // Test the stored data with the zipreader
  var zipR = new ZipReader(tmpFile);

  for (let i = 0; i < TESTS.length; i++) {
    let source = do_get_file(DATA_DIR + TESTS[i].name);
    Assert.ok(zipR.hasEntry(TESTS[i].name));

    var entry = zipR.getEntry(TESTS[i].name);
    Assert.equal(entry.realSize, TESTS[i].size);
    Assert.equal(entry.size, TESTS[i].size);
    Assert.equal(entry.CRC32, TESTS[i].crc);
    Assert.equal(
      Math.floor(entry.lastModifiedTime / PR_USEC_PER_SEC),
      Math.floor(source.lastModifiedTime / PR_MSEC_PER_SEC)
    );

    zipR.test(TESTS[i].name);
  }

  zipR.close();
}
