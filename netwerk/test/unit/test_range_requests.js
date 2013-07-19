//
// This test makes sure range-requests are sent and treated the way we want
// See bug #612135 for a thorough discussion on the subject
//
// Necko does a range-request for a partial cache-entry iff
//
//   1) size of the cached entry < value of the cached Content-Length header
//      (not tested here - see bug #612135 comments 108-110)
//   2) the size of the cached entry is > 0  (see bug #628607)
//   3) the cached entry does not have a "no-store" Cache-Control header
//   4) the cached entry does not have a Content-Encoding (see bug #613159)
//   5) the request does not have a conditional-request header set by client
//   6) nsHttpResponseHead::IsResumable() is true for the cached entry
//
//  The test has one handler for each case and run_tests() fires one request
//  for each. None of the handlers should see a Range-header.

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js");

var httpserver = null;

const clearTextBody = "This is a slightly longer test\n";
const encodedBody = [0x1f, 0x8b, 0x08, 0x08, 0xef, 0x70, 0xe6, 0x4c, 0x00, 0x03, 0x74, 0x65, 0x78, 0x74, 0x66, 0x69,
                     0x6c, 0x65, 0x2e, 0x74, 0x78, 0x74, 0x00, 0x0b, 0xc9, 0xc8, 0x2c, 0x56, 0x00, 0xa2, 0x44, 0x85,
                     0xe2, 0x9c, 0xcc, 0xf4, 0x8c, 0x92, 0x9c, 0x4a, 0x85, 0x9c, 0xfc, 0xbc, 0xf4, 0xd4, 0x22, 0x85,
                     0x92, 0xd4, 0xe2, 0x12, 0x2e, 0x2e, 0x00, 0x00, 0xe5, 0xe6, 0xf0, 0x20, 0x00, 0x00, 0x00];
const decodedBody = [0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x61, 0x20, 0x73, 0x6c, 0x69, 0x67, 0x68, 0x74,
                     0x6c, 0x79, 0x20, 0x6c, 0x6f, 0x6e, 0x67, 0x65, 0x72, 0x20, 0x74, 0x65, 0x73, 0x74, 0x0a, 0x0a];

const partial_data_length = 4;
var port = null; // set in run_test

function make_channel(url, callback, ctx) {
  var ios = Cc["@mozilla.org/network/io-service;1"].
            getService(Ci.nsIIOService);
  var chan = ios.newChannel(url, "", null);
  return chan.QueryInterface(Ci.nsIHttpChannel);
}

// StreamListener which cancels its request on first data available
function Canceler(continueFn) {
  this.continueFn = continueFn;
}
Canceler.prototype = {
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  onStartRequest: function(request, context) { },

  onDataAvailable: function(request, context, stream, offset, count) {
    request.QueryInterface(Ci.nsIChannel)
           .cancel(Components.results.NS_BINDING_ABORTED);
  },
  onStopRequest: function(request, context, status) {
    do_check_eq(status, Components.results.NS_BINDING_ABORTED);
    this.continueFn(request, null);
  }
};
// Simple StreamListener which performs no validations
function MyListener(continueFn) {
  this.continueFn = continueFn;
  this._buffer = null;
}
MyListener.prototype = {
  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  onStartRequest: function(request, context) { this._buffer = ""; },

  onDataAvailable: function(request, context, stream, offset, count) {
    this._buffer = this._buffer.concat(read_stream(stream, count));
  },
  onStopRequest: function(request, context, status) {
    this.continueFn(request, this._buffer);
  }
};

function received_cleartext(request, data) {
  do_check_eq(clearTextBody, data);
  testFinished();
}

function setStdHeaders(response, length) {
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("ETag", "Just testing");
  response.setHeader("Cache-Control", "max-age: 360000");
  response.setHeader("Accept-Ranges", "bytes");
  response.setHeader("Content-Length", "" + length);
}

function handler_2(metadata, response) {
  setStdHeaders(response, clearTextBody.length);
  do_check_false(metadata.hasHeader("Range"));
  response.bodyOutputStream.write(clearTextBody, clearTextBody.length);
}
function received_partial_2(request, data) {
  do_check_eq(data, undefined);
  var chan = make_channel("http://localhost:" + port + "/test_2");
  chan.asyncOpen(new ChannelListener(received_cleartext, null), null);
}

var case_3_request_no = 0;
function handler_3(metadata, response) {
  var body = clearTextBody;
  setStdHeaders(response, body.length);
  response.setHeader("Cache-Control", "no-store", false);
  switch (case_3_request_no) {
    case 0:
      do_check_false(metadata.hasHeader("Range"));
      body = body.slice(0, partial_data_length);
      response.processAsync();
      response.bodyOutputStream.write(body, body.length);
      response.finish();
      break;
    case 1:
      do_check_false(metadata.hasHeader("Range"));
      response.bodyOutputStream.write(body, body.length);
      break;
    default:
      response.setStatusLine(metadata.httpVersion, 404, "Not Found");
  }
  case_3_request_no++;
}
function received_partial_3(request, data) {
  do_check_eq(partial_data_length, data.length);
  var chan = make_channel("http://localhost:" + port + "/test_3");
  chan.asyncOpen(new ChannelListener(received_cleartext, null), null);
}

var case_4_request_no = 0;
function handler_4(metadata, response) {
  switch (case_4_request_no) {
    case 0:
      do_check_false(metadata.hasHeader("Range"));
      var body = encodedBody;
      setStdHeaders(response, body.length);
      response.setHeader("Content-Encoding", "gzip", false);
      body = body.slice(0, partial_data_length);
	  var bos = Cc["@mozilla.org/binaryoutputstream;1"]
	              .createInstance(Ci.nsIBinaryOutputStream);
	  bos.setOutputStream(response.bodyOutputStream);
      response.processAsync();
      bos.writeByteArray(body, body.length);
      response.finish();
      break;
    case 1:
      do_check_false(metadata.hasHeader("Range"));
      setStdHeaders(response, clearTextBody.length);
      response.bodyOutputStream.write(clearTextBody, clearTextBody.length);
      break;
    default:
      response.setStatusLine(metadata.httpVersion, 404, "Not Found");
  }
  case_4_request_no++;
}
function received_partial_4(request, data) {
// checking length does not work with encoded data
//  do_check_eq(partial_data_length, data.length);
  var chan = make_channel("http://localhost:" + port + "/test_4");
  chan.asyncOpen(new MyListener(received_cleartext), null);
}

var case_5_request_no = 0;
function handler_5(metadata, response) {
  var body = clearTextBody;
  setStdHeaders(response, body.length);
  switch (case_5_request_no) {
    case 0:
      do_check_false(metadata.hasHeader("Range"));
      body = body.slice(0, partial_data_length);
      response.processAsync();
      response.bodyOutputStream.write(body, body.length);
      response.finish();
      break;
    case 1:
      do_check_false(metadata.hasHeader("Range"));
      response.bodyOutputStream.write(body, body.length);
      break;
    default:
      response.setStatusLine(metadata.httpVersion, 404, "Not Found");
  }
  case_5_request_no++;
}
function received_partial_5(request, data) {
  do_check_eq(partial_data_length, data.length);
  var chan = make_channel("http://localhost:" + port + "/test_5");
  chan.setRequestHeader("If-Match", "Some eTag", false);
  chan.asyncOpen(new ChannelListener(received_cleartext, null), null);
}

var case_6_request_no = 0;
function handler_6(metadata, response) {
  switch (case_6_request_no) {
    case 0:
      do_check_false(metadata.hasHeader("Range"));
      var body = clearTextBody;
      setStdHeaders(response, body.length);
      response.setHeader("Accept-Ranges", "", false);
      body = body.slice(0, partial_data_length);
      response.processAsync();
      response.bodyOutputStream.write(body, body.length);
      response.finish();
      break;
    case 1:
      do_check_false(metadata.hasHeader("Range"));
      setStdHeaders(response, clearTextBody.length);
      response.bodyOutputStream.write(clearTextBody, clearTextBody.length);
      break;
    default:
      response.setStatusLine(metadata.httpVersion, 404, "Not Found");
  }
  case_6_request_no++;
}
function received_partial_6(request, data) {
// would like to verify that the response does not have Accept-Ranges
  do_check_eq(partial_data_length, data.length);
  var chan = make_channel("http://localhost:" + port + "/test_6");
  chan.asyncOpen(new ChannelListener(received_cleartext, null), null);
}

// Simple mechanism to keep track of tests and stop the server
var numTestsFinished = 0;
function testFinished() {
  if (++numTestsFinished == 5)
    httpserver.stop(do_test_finished);
}

function run_test() {
  httpserver = new HttpServer();
  httpserver.registerPathHandler("/test_2", handler_2);
  httpserver.registerPathHandler("/test_3", handler_3);
  httpserver.registerPathHandler("/test_4", handler_4);
  httpserver.registerPathHandler("/test_5", handler_5);
  httpserver.registerPathHandler("/test_6", handler_6);
  httpserver.start(-1);

  port = httpserver.identity.primaryPort;

  // wipe out cached content
  evict_cache_entries();

  // Case 2: zero-length partial entry must not trigger range-request
  var chan = make_channel("http://localhost:" + port + "/test_2");
  chan.asyncOpen(new Canceler(received_partial_2), null);

  // Case 3: no-store response must not trigger range-request
  var chan = make_channel("http://localhost:" + port + "/test_3");
  chan.asyncOpen(new MyListener(received_partial_3), null);

  // Case 4: response with content-encoding must not trigger range-request
  var chan = make_channel("http://localhost:" + port + "/test_4");
  chan.asyncOpen(new MyListener(received_partial_4), null);

  // Case 5: conditional request-header set by client
  var chan = make_channel("http://localhost:" + port + "/test_5");
  chan.asyncOpen(new MyListener(received_partial_5), null);

  // Case 6: response is not resumable (drop the Accept-Ranges header)
  var chan = make_channel("http://localhost:" + port + "/test_6");
  chan.asyncOpen(new MyListener(received_partial_6), null);

  do_test_pending();
}
