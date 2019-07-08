/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

const PATH = "/file.meh";
var httpserver = new HttpServer();

// Each time, the data consist in a string that should be sniffed as Ogg.
const data = "OggS\0meeeh.";
var testRan = 0;

// If the content-type is not present, or if it's application/octet-stream, it
// should be sniffed to application/ogg by the media sniffer. Otherwise, it
// should not be changed.
const tests = [
  // Those three first case are the case of a media loaded in a media element.
  // All three should be sniffed.
  {
    contentType: "",
    expectedContentType: "application/ogg",
    flags:
      Ci.nsIChannel.LOAD_CALL_CONTENT_SNIFFERS |
      Ci.nsIChannel.LOAD_MEDIA_SNIFFER_OVERRIDES_CONTENT_TYPE,
  },
  {
    contentType: "application/octet-stream",
    expectedContentType: "application/ogg",
    flags:
      Ci.nsIChannel.LOAD_CALL_CONTENT_SNIFFERS |
      Ci.nsIChannel.LOAD_MEDIA_SNIFFER_OVERRIDES_CONTENT_TYPE,
  },
  {
    contentType: "application/something",
    expectedContentType: "application/ogg",
    flags:
      Ci.nsIChannel.LOAD_CALL_CONTENT_SNIFFERS |
      Ci.nsIChannel.LOAD_MEDIA_SNIFFER_OVERRIDES_CONTENT_TYPE,
  },
  // This last cases test the case of a channel opened while allowing content
  // sniffers to override the content-type, like in the docshell.
  {
    contentType: "application/octet-stream",
    expectedContentType: "application/ogg",
    flags: Ci.nsIChannel.LOAD_CALL_CONTENT_SNIFFERS,
  },
  {
    contentType: "",
    expectedContentType: "application/ogg",
    flags: Ci.nsIChannel.LOAD_CALL_CONTENT_SNIFFERS,
  },
  {
    contentType: "application/something",
    expectedContentType: "application/something",
    flags: Ci.nsIChannel.LOAD_CALL_CONTENT_SNIFFERS,
  },
];

// A basic listener that reads checks the if we sniffed properly.
var listener = {
  onStartRequest(request) {
    Assert.equal(
      request.QueryInterface(Ci.nsIChannel).contentType,
      tests[testRan].expectedContentType
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

function setupChannel(url, flags) {
  let uri = "http://localhost:" + httpserver.identity.primaryPort + url;
  var chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_MEDIA,
  });
  chan.loadFlags |= flags;
  var httpChan = chan.QueryInterface(Ci.nsIHttpChannel);
  return httpChan;
}

function runNext() {
  if (testRan == tests.length) {
    do_test_finished();
    return;
  }
  var channel = setupChannel(PATH, tests[testRan].flags);
  httpserver.registerPathHandler(PATH, function(request, response) {
    response.setHeader("Content-Type", tests[testRan].contentType, false);
    response.bodyOutputStream.write(data, data.length);
  });
  channel.asyncOpen(listener);
}

function run_test() {
  httpserver.start(-1);
  do_test_pending();
  try {
    runNext();
  } catch (e) {
    print("ERROR - " + e + "\n");
  }
}
