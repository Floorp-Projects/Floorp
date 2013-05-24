/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Ci = Components.interfaces;
const Cu = Components.utils;
const Cc = Components.classes;
const CC = Components.Constructor;

var BinaryOutputStream = CC("@mozilla.org/binaryoutputstream;1",
                            "nsIBinaryOutputStream",
                            "setOutputStream");

Cu.import("resource://testing-common/httpd.js");

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
  // Padding bit flipped in the first header: sniffing should fail.
  { path: "data/notags-bad.mp3", expected: "application/octet-stream" },
];

// A basic listener that reads checks the if we sniffed properly.
var listener = {
  onStartRequest: function(request, context) {
    do_print("Sniffing " + tests[testRan].path);
    do_check_eq(request.QueryInterface(Ci.nsIChannel).contentType, tests[testRan].expected);
  },

  onDataAvailable: function(request, context, stream, offset, count) {
    try {
      var bis = Components.classes["@mozilla.org/binaryinputstream;1"]
                          .createInstance(Components.interfaces.nsIBinaryInputStream);
      bis.setInputStream(stream);
      var array = bis.readByteArray(bis.available());
    } catch (ex) {
      do_throw("Error in onDataAvailable: " + ex);
    }
  },

  onStopRequest: function(request, context, status) {
    testRan++;
    runNext();
  }
};

function setupChannel(url) {
  var ios = Components.classes["@mozilla.org/network/io-service;1"].
                       getService(Ci.nsIIOService);
  var chan = ios.newChannel("http://localhost:4444" + url, "", null);
  var httpChan = chan.QueryInterface(Components.interfaces.nsIHttpChannel);
  return httpChan;
}

function runNext() {
  if (testRan == tests.length) {
    do_test_finished();
    return;
  }
  var channel = setupChannel("/");
  channel.asyncOpen(listener, channel, null);
}

function getFileContents(aFile) {
  const PR_RDONLY = 0x01;
  var fileStream = Cc["@mozilla.org/network/file-input-stream;1"]
                      .createInstance(Ci.nsIFileInputStream);
  fileStream.init(aFile, 1, -1, null);
  var bis = Components.classes["@mozilla.org/binaryinputstream;1"]
                      .createInstance(Components.interfaces.nsIBinaryInputStream);
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
  bos.writeByteArray(body, body.length);
}

function run_test() {
  // We use a custom handler so we can change the header to force sniffing.
  httpserver.registerPathHandler("/", handler);
  httpserver.start(4444);
  do_test_pending();
  try {
    runNext();
  } catch (e) {
    print("ERROR - " + e + "\n");
  }
}
