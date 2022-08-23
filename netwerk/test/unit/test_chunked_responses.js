/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Test Chunked-Encoded response parsing.
 */

////////////////////////////////////////////////////////////////////////////////
// Test infrastructure

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var test_flags = [];
var testPathBase = "/chunked_hdrs";

function run_test() {
  httpserver.start(-1);

  do_test_pending();
  run_test_number(1);
}

function run_test_number(num) {
  var testPath = testPathBase + num;
  // eslint-disable-next-line no-eval
  httpserver.registerPathHandler(testPath, eval("handler" + num));

  var channel = setupChannel(testPath);
  var flags = test_flags[num]; // OK if flags undefined for test
  channel.asyncOpen(
    // eslint-disable-next-line no-eval
    new ChannelListener(eval("completeTest" + num), channel, flags)
  );
}

function setupChannel(url) {
  var chan = NetUtil.newChannel({
    uri: URL + url,
    loadUsingSystemPrincipal: true,
  });
  var httpChan = chan.QueryInterface(Ci.nsIHttpChannel);
  return httpChan;
}

function endTests() {
  httpserver.stop(do_test_finished);
}

////////////////////////////////////////////////////////////////////////////////
// Test 1: FAIL because of overflowed chunked size. The parser uses long so
//         the test case uses >64bit to fail on all platforms.
test_flags[1] = CL_EXPECT_LATE_FAILURE | CL_ALLOW_UNKNOWN_CL;

// eslint-disable-next-line no-unused-vars
function handler1(metadata, response) {
  var body = "12345678123456789\r\ndata never reached";

  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Transfer-Encoding: chunked\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

// eslint-disable-next-line no-unused-vars
function completeTest1(request, data, ctx) {
  Assert.equal(request.status, Cr.NS_ERROR_UNEXPECTED);

  run_test_number(2);
}

////////////////////////////////////////////////////////////////////////////////
// Test 2: FAIL because of non-hex in chunked length

test_flags[2] = CL_EXPECT_LATE_FAILURE | CL_ALLOW_UNKNOWN_CL;

// eslint-disable-next-line no-unused-vars
function handler2(metadata, response) {
  var body = "junkintheway 123\r\ndata never reached";

  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Transfer-Encoding: chunked\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

// eslint-disable-next-line no-unused-vars
function completeTest2(request, data, ctx) {
  Assert.equal(request.status, Cr.NS_ERROR_UNEXPECTED);
  run_test_number(3);
}

////////////////////////////////////////////////////////////////////////////////
// Test 3: OK in spite of non-hex digits after size in the length field

test_flags[3] = CL_ALLOW_UNKNOWN_CL;

// eslint-disable-next-line no-unused-vars
function handler3(metadata, response) {
  var body = "c junkafter\r\ndata reached\r\n0\r\n\r\n";

  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Transfer-Encoding: chunked\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

// eslint-disable-next-line no-unused-vars
function completeTest3(request, data, ctx) {
  Assert.equal(request.status, 0);
  run_test_number(4);
}

////////////////////////////////////////////////////////////////////////////////
// Test 4: Verify a fully compliant chunked response.

test_flags[4] = CL_ALLOW_UNKNOWN_CL;

// eslint-disable-next-line no-unused-vars
function handler4(metadata, response) {
  var body = "c\r\ndata reached\r\n3\r\nhej\r\n0\r\n\r\n";

  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Transfer-Encoding: chunked\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

// eslint-disable-next-line no-unused-vars
function completeTest4(request, data, ctx) {
  Assert.equal(request.status, 0);
  run_test_number(5);
}

////////////////////////////////////////////////////////////////////////////////
// Test 5: A chunk size larger than 32 bit but smaller than 64bit also fails
// This is probabaly subject to get improved at some point.

test_flags[5] = CL_EXPECT_LATE_FAILURE | CL_ALLOW_UNKNOWN_CL;

// eslint-disable-next-line no-unused-vars
function handler5(metadata, response) {
  var body = "123456781\r\ndata never reached";

  response.seizePower();
  response.write("HTTP/1.1 200 OK\r\n");
  response.write("Content-Type: text/plain\r\n");
  response.write("Transfer-Encoding: chunked\r\n");
  response.write("\r\n");
  response.write(body);
  response.finish();
}

// eslint-disable-next-line no-unused-vars
function completeTest5(request, data, ctx) {
  Assert.equal(request.status, Cr.NS_ERROR_UNEXPECTED);
  endTests();
  //  run_test_number(6);
}
