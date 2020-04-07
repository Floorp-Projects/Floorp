// Tests bug 304414

"use strict";

const PR_RDONLY = 0x1; // see prio.h

// Does some sanity checks on the stream and returns the number of bytes read
// when the checks passed.
function test_stream(stream) {
  // This test only handles blocking streams; that's desired for file streams
  // anyway.
  Assert.equal(stream.isNonBlocking(), false);

  // Check that the stream is not buffered
  Assert.equal(
    Cc["@mozilla.org/io-util;1"]
      .getService(Ci.nsIIOUtil)
      .inputStreamIsBuffered(stream),
    false
  );

  // Wrap it in a binary stream (to avoid wrong results that
  // scriptablestream would produce with binary content)
  var binstream = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  binstream.setInputStream(stream);

  var numread = 0;
  for (;;) {
    Assert.equal(stream.available(), binstream.available());
    var avail = stream.available();
    Assert.notEqual(avail, -1);

    // PR_UINT32_MAX and PR_INT32_MAX; the files we're testing with aren't that
    // large.
    Assert.notEqual(avail, Math.pow(2, 32) - 1);
    Assert.notEqual(avail, Math.pow(2, 31) - 1);

    if (!avail) {
      // For blocking streams, available() only returns 0 on EOF
      // Make sure that there is really no data left
      var could_read = false;
      try {
        binstream.readByteArray(1);
        could_read = true;
      } catch (e) {
        // We expect the exception, so do nothing here
      }
      if (could_read) {
        do_throw("Data readable when available indicated EOF!");
      }
      return numread;
    }

    dump("Trying to read " + avail + " bytes\n");
    // Note: Verification that this does return as much bytes as we asked for is
    // done in the binarystream implementation
    var data = binstream.readByteArray(avail);

    numread += avail;
  }
}

function stream_for_file(file) {
  var str = Cc["@mozilla.org/network/file-input-stream;1"].createInstance(
    Ci.nsIFileInputStream
  );
  str.init(file, PR_RDONLY, 0, 0);
  return str;
}

function stream_from_channel(file) {
  var ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);
  var uri = ios.newFileURI(file);
  return NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).open();
}

function run_test() {
  // Get a file and a directory in order to do some testing
  var file = do_get_file("../unit/data/test_readline6.txt");
  var len = file.fileSize;
  Assert.equal(test_stream(stream_for_file(file)), len);
  Assert.equal(test_stream(stream_from_channel(file)), len);
  var dir = file.parent;
  test_stream(stream_from_channel(dir)); // Can't do size checking
}
