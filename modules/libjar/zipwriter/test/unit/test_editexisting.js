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
    time: Date.UTC(2007, 4, 1, 20, 44, 55)
  },
  {
    name: "test.png",
    size: 3402,
    crc: 0x504a5c30,
    time: Date.UTC(2007, 4, 1, 20, 49, 39)
  }
];
var BADENTRY = "unknown.txt";

function run_test()
{
  // Copy our test zip to the tmp dir so we can modify it
  var testzip = do_get_file(DATA_DIR + "test.zip");
  testzip.copyTo(tmpDir, tmpFile.leafName);

  Assert.ok(tmpFile.exists());

  zipW.open(tmpFile, PR_RDWR);

  for (var i = 0; i < TESTS.length; i++) {
    Assert.ok(zipW.hasEntry(TESTS[i].name));
    var entry = zipW.getEntry(TESTS[i].name);
    Assert.ok(entry != null);

    Assert.equal(entry.realSize, TESTS[i].size);
    Assert.equal(entry.CRC32, TESTS[i].crc);
    Assert.equal(entry.lastModifiedTime / PR_USEC_PER_MSEC, TESTS[i].time);
  }

  try {
    zipW.removeEntry(BADENTRY, false);
    do_throw("shouldn't be able to remove an entry that doesn't exist");
  }
  catch (e) {
    Assert.equal(e.result, Components.results.NS_ERROR_FILE_NOT_FOUND);
  }

  for (var i = 0; i < TESTS.length; i++) {
    zipW.removeEntry(TESTS[i].name, false);
  }

  zipW.close();

  // Certain platforms cache the file size so get a fresh file to check.
  tmpFile = tmpFile.clone();

  // Empty zip file should just be the end of central directory marker
  Assert.equal(tmpFile.fileSize, ZIP_EOCDR_HEADER_SIZE);
}
