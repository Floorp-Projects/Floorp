/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var CC = Components.Constructor;

var BinaryOutputStream = CC(
  "@mozilla.org/binaryoutputstream;1",
  "nsIBinaryOutputStream",
  "setOutputStream"
);

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

var httpserver = new HttpServer();

var testRan = 0;

// The tests files we want to test, and the type we should have after sniffing.
const tests = [
  // Real webm and mkv files truncated to 512 bytes.
  { path: "data/file.webm", expected: "video/webm" },
  { path: "data/file.mkv", expected: "application/octet-stream" },
  // MP3 files with and without id3 headers truncated to 512 bytes.
  // NB these have 208/209 byte frames, but mp3 can require up to
  // 1445 bytes to detect with our method.
  { path: "data/id3tags.mp3", expected: "audio/mpeg" },
  { path: "data/notags.mp3", expected: "audio/mpeg" },
  // MPEG-2 mp3 files.
  { path: "data/detodos.mp3", expected: "audio/mpeg" },
  // Padding bit flipped in the first header: sniffing should fail.
  { path: "data/notags-bad.mp3", expected: "application/octet-stream" },
  // Garbage before header: sniffing should fail.
  { path: "data/notags-scan.mp3", expected: "application/octet-stream" },
  // VBR from the layer III test patterns. We can't sniff this.
  { path: "data/he_free.mp3", expected: "application/octet-stream" },
  // Make sure we reject mp2, which has a similar header.
  { path: "data/fl10.mp2", expected: "application/octet-stream" },
  // Truncated ff installer regression test for bug 875769.
  { path: "data/ff-inst.exe", expected: "application/octet-stream" },
  // MP4 with invalid box size (0) for "ftyp".
  { path: "data/bug1079747.mp4", expected: "application/octet-stream" },
  // An MP3 bytestream in a RIFF container, truncated to 512 bytes.
  { path: "data/mp3-in-riff.wav", expected: "audio/mpeg" },
  // The sniffing-relevant portion of a Canon raw image
  { path: "data/bug1725190.cr3", expected: "application/octet-stream" },
];

// A basic listener that reads checks the if we sniffed properly.
var listener = {
  onStartRequest(request) {
    info("Sniffing " + tests[testRan].path);
    Assert.equal(
      request.QueryInterface(Ci.nsIChannel).contentType,
      tests[testRan].expected
    );
  },

  onDataAvailable(request, stream, offset, count) {
    try {
      var bis = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
        Ci.nsIBinaryInputStream
      );
      bis.setInputStream(stream);
      bis.readByteArray(bis.available());
    } catch (ex) {
      do_throw("Error in onDataAvailable: " + ex);
    }
  },

  onStopRequest(request, status) {
    testRan++;
    runNext();
  },
};

function setupChannel(url) {
  var chan = NetUtil.newChannel({
    uri: "http://localhost:" + httpserver.identity.primaryPort + url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_MEDIA,
  });
  var httpChan = chan.QueryInterface(Ci.nsIHttpChannel);
  return httpChan;
}

function runNext() {
  if (testRan == tests.length) {
    do_test_finished();
    return;
  }
  var channel = setupChannel("/");
  channel.asyncOpen(listener);
}

function getFileContents(aFile) {
  var fileStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  fileStream.init(aFile, 1, -1, null);
  var bis = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  bis.setInputStream(fileStream);

  var data = bis.readByteArray(bis.available());

  return data;
}

function handler(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  // Send an empty Content-Type, so we are guaranteed to sniff.
  response.setHeader("Content-Type", "", false);
  var body = getFileContents(do_get_file(tests[testRan].path));
  var bos = new BinaryOutputStream(response.bodyOutputStream);
  bos.writeByteArray(body);
}

function run_test() {
  // We use a custom handler so we can change the header to force sniffing.
  httpserver.registerPathHandler("/", handler);
  httpserver.start(-1);
  do_test_pending();
  try {
    runNext();
  } catch (e) {
    print("ERROR - " + e + "\n");
  }
}
