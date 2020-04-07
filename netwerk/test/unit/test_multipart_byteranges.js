"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

var httpserver = null;

XPCOMUtils.defineLazyGetter(this, "uri", function() {
  return "http://localhost:" + httpserver.identity.primaryPort + "/multipart";
});

function make_channel(url) {
  return NetUtil.newChannel({ uri: url, loadUsingSystemPrincipal: true });
}

var multipartBody =
  "--boundary\r\n" +
  "Content-type: text/plain\r\n" +
  "Content-range: bytes 0-2/10\r\n" +
  "\r\n" +
  "aaa\r\n" +
  "--boundary\r\n" +
  "Content-type: text/plain\r\n" +
  "Content-range: bytes 3-7/10\r\n" +
  "\r\n" +
  "bbbbb" +
  "\r\n" +
  "--boundary\r\n" +
  "Content-type: text/plain\r\n" +
  "Content-range: bytes  8-9/10\r\n" +
  "\r\n" +
  "cc" +
  "\r\n" +
  "--boundary--";

function contentHandler(metadata, response) {
  response.setHeader(
    "Content-Type",
    'multipart/byteranges; boundary="boundary"'
  );
  response.bodyOutputStream.write(multipartBody, multipartBody.length);
}

var numTests = 2;
var testNum = 0;

var testData = [
  {
    data: "aaa",
    type: "text/plain",
    isByteRangeRequest: true,
    startRange: 0,
    endRange: 2,
  },
  {
    data: "bbbbb",
    type: "text/plain",
    isByteRangeRequest: true,
    startRange: 3,
    endRange: 7,
  },
  {
    data: "cc",
    type: "text/plain",
    isByteRangeRequest: true,
    startRange: 8,
    endRange: 9,
  },
];

function responseHandler(request, buffer) {
  Assert.equal(buffer, testData[testNum].data);
  Assert.equal(
    request.QueryInterface(Ci.nsIChannel).contentType,
    testData[testNum].type
  );
  Assert.equal(
    request.QueryInterface(Ci.nsIByteRangeRequest).isByteRangeRequest,
    testData[testNum].isByteRangeRequest
  );
  Assert.equal(
    request.QueryInterface(Ci.nsIByteRangeRequest).startRange,
    testData[testNum].startRange
  );
  Assert.equal(
    request.QueryInterface(Ci.nsIByteRangeRequest).endRange,
    testData[testNum].endRange
  );
  if (++testNum == numTests) {
    httpserver.stop(do_test_finished);
  }
}

var multipartListener = {
  _buffer: "",

  QueryInterface: ChromeUtils.generateQI([
    "nsIStreamListener",
    "nsIRequestObserver",
  ]),

  onStartRequest(request) {
    this._buffer = "";
  },

  onDataAvailable(request, stream, offset, count) {
    try {
      this._buffer = this._buffer.concat(read_stream(stream, count));
      dump("BUFFEEE: " + this._buffer + "\n\n");
    } catch (ex) {
      do_throw("Error in onDataAvailable: " + ex);
    }
  },

  onStopRequest(request, status) {
    try {
      responseHandler(request, this._buffer);
    } catch (ex) {
      do_throw("Error in closure function: " + ex);
    }
  },
};

function run_test() {
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/multipart", contentHandler);
  httpserver.start(-1);

  var streamConv = Cc["@mozilla.org/streamConverters;1"].getService(
    Ci.nsIStreamConverterService
  );
  var conv = streamConv.asyncConvertData(
    "multipart/byteranges",
    "*/*",
    multipartListener,
    null
  );

  var chan = make_channel(uri);
  chan.asyncOpen(conv, null);
  do_test_pending();
}
