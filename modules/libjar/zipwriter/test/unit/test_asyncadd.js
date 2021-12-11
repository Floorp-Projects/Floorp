/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

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

var size = 0;

var observer = {
  onStartRequest(request) {},

  onStopRequest(request, status) {
    Assert.equal(status, Cr.NS_OK);

    zipW.close();
    size += ZIP_EOCDR_HEADER_SIZE;

    Assert.equal(size, tmpFile.fileSize);

    // Test the stored data with the zipreader
    var zipR = new ZipReader(tmpFile);

    for (var i = 0; i < TESTS.length; i++) {
      var source = do_get_file(DATA_DIR + TESTS[i].name);
      for (let method in methods) {
        var entryName = method + "/" + TESTS[i].name;
        Assert.ok(zipR.hasEntry(entryName));

        var entry = zipR.getEntry(entryName);
        Assert.equal(entry.realSize, TESTS[i].size);
        Assert.equal(entry.size, TESTS[i].size);
        Assert.equal(entry.CRC32, TESTS[i].crc);
        Assert.equal(
          Math.floor(entry.lastModifiedTime / PR_USEC_PER_SEC),
          Math.floor(source.lastModifiedTime / PR_MSEC_PER_SEC)
        );

        zipR.test(entryName);
      }
    }

    zipR.close();
    do_test_finished();
  },
};

var methods = {
  file: function method_file(entry, source) {
    zipW.addEntryFile(entry, Ci.nsIZipWriter.COMPRESSION_NONE, source, true);
  },
  channel: function method_channel(entry, source) {
    zipW.addEntryChannel(
      entry,
      source.lastModifiedTime * PR_MSEC_PER_SEC,
      Ci.nsIZipWriter.COMPRESSION_NONE,
      NetUtil.newChannel({
        uri: ioSvc.newFileURI(source),
        loadUsingSystemPrincipal: true,
      }),
      true
    );
  },
  stream: function method_stream(entry, source) {
    zipW.addEntryStream(
      entry,
      source.lastModifiedTime * PR_MSEC_PER_SEC,
      Ci.nsIZipWriter.COMPRESSION_NONE,
      NetUtil.newChannel({
        uri: ioSvc.newFileURI(source),
        loadUsingSystemPrincipal: true,
      }).open(),
      true
    );
  },
};

function run_test() {
  zipW.open(tmpFile, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE);

  for (var i = 0; i < TESTS.length; i++) {
    var source = do_get_file(DATA_DIR + TESTS[i].name);
    for (let method in methods) {
      var entry = method + "/" + TESTS[i].name;
      methods[method](entry, source);
      size +=
        ZIP_FILE_HEADER_SIZE +
        ZIP_CDS_HEADER_SIZE +
        ZIP_EXTENDED_TIMESTAMP_SIZE * 2 +
        entry.length * 2 +
        TESTS[i].size;
    }
  }
  do_test_pending();
  zipW.processQueue(observer, null);
  Assert.ok(zipW.inQueue);
}
