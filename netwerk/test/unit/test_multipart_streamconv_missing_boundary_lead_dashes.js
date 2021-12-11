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
  "\r\nboundary\r\n\r\nSome text\r\nboundary\r\n\r\n<?xml version='1.0'?><root/>\r\nboundary--";

function contentHandler(metadata, response) {
  response.setHeader("Content-Type", 'multipart/mixed; boundary="boundary"');
  response.bodyOutputStream.write(multipartBody, multipartBody.length);
}

var numTests = 2;
var testNum = 0;

var testData = [
  { data: "Some text", type: "text/plain" },
  { data: "<?xml version='1.0'?><root/>", type: "text/xml" },
];

function responseHandler(request, buffer) {
  Assert.equal(buffer, testData[testNum].data);
  Assert.equal(
    request.QueryInterface(Ci.nsIChannel).contentType,
    testData[testNum].type
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
    "multipart/mixed",
    "*/*",
    multipartListener,
    null
  );

  var chan = make_channel(uri);
  chan.asyncOpen(conv);
  do_test_pending();
}
