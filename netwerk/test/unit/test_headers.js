//
//  cleaner HTTP header test infrastructure
//
//  tests bugs:  589292, [add more here: see hg log for definitive list]
//
//  TO ADD NEW TESTS:
//  1) Increment up 'lastTest' to new number (say, "99")
//  2) Add new test 'handler99' and 'completeTest99' functions.
//  3) If your test should fail the necko channel, set
//     test_flags[99] = CL_EXPECT_FAILURE.
//
// TO DEBUG JUST ONE TEST: temporarily change firstTest and lastTest to equal
//                         the test # you're interested in.
//
//  For tests that need duplicate copies of headers to be sent, see
//  test_duplicate_headers.js

"use strict";

var firstTest = 1; // set to test of interest when debugging
var lastTest = 4; // set to test of interest when debugging
////////////////////////////////////////////////////////////////////////////////

// Note: sets Cc and Ci variables

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

XPCOMUtils.defineLazyGetter(this, "URL", function() {
  return "http://localhost:" + httpserver.identity.primaryPort;
});

var httpserver = new HttpServer();
var nextTest = firstTest;
var test_flags = [];
var testPathBase = "/test_headers";

function run_test() {
  httpserver.start(-1);

  do_test_pending();
  run_test_number(nextTest);
}

function runNextTest() {
  if (nextTest == lastTest) {
    endTests();
    return;
  }
  nextTest++;
  // Make sure test functions exist
  if (globalThis["handler" + nextTest] == undefined) {
    do_throw("handler" + nextTest + " undefined!");
  }
  if (globalThis["completeTest" + nextTest] == undefined) {
    do_throw("completeTest" + nextTest + " undefined!");
  }

  run_test_number(nextTest);
}

function run_test_number(num) {
  let testPath = testPathBase + num;
  httpserver.registerPathHandler(testPath, globalThis["handler" + num]);

  var channel = setupChannel(testPath);
  let flags = test_flags[num]; // OK if flags undefined for test
  channel.asyncOpen(
    new ChannelListener(globalThis["completeTest" + num], channel, flags)
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
// Test 1: test Content-Disposition channel attributes
// eslint-disable-next-line no-unused-vars
function handler1(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Disposition", "attachment; filename=foo");
  response.setHeader("Content-Type", "text/plain", false);
}

// eslint-disable-next-line no-unused-vars
function completeTest1(request, data, ctx) {
  try {
    var chan = request.QueryInterface(Ci.nsIChannel);
    Assert.equal(chan.contentDisposition, chan.DISPOSITION_ATTACHMENT);
    Assert.equal(chan.contentDispositionFilename, "foo");
    Assert.equal(chan.contentDispositionHeader, "attachment; filename=foo");
  } catch (ex) {
    do_throw("error parsing Content-Disposition: " + ex);
  }
  runNextTest();
}

////////////////////////////////////////////////////////////////////////////////
// Test 2: no filename
// eslint-disable-next-line no-unused-vars
function handler2(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Disposition", "attachment");
  var body = "foo";
  response.bodyOutputStream.write(body, body.length);
}

// eslint-disable-next-line no-unused-vars
function completeTest2(request, data, ctx) {
  try {
    var chan = request.QueryInterface(Ci.nsIChannel);
    Assert.equal(chan.contentDisposition, chan.DISPOSITION_ATTACHMENT);
    Assert.equal(chan.contentDispositionHeader, "attachment");
    chan.contentDispositionFilename; // should barf
    do_throw("Should have failed getting Content-Disposition filename");
  } catch (ex) {
    info("correctly ate exception");
  }
  runNextTest();
}

////////////////////////////////////////////////////////////////////////////////
// Test 3: filename missing
// eslint-disable-next-line no-unused-vars
function handler3(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Disposition", "attachment; filename=");
  var body = "foo";
  response.bodyOutputStream.write(body, body.length);
}

// eslint-disable-next-line no-unused-vars
function completeTest3(request, data, ctx) {
  try {
    var chan = request.QueryInterface(Ci.nsIChannel);
    Assert.equal(chan.contentDisposition, chan.DISPOSITION_ATTACHMENT);
    Assert.equal(chan.contentDispositionHeader, "attachment; filename=");
    chan.contentDispositionFilename; // should barf

    do_throw("Should have failed getting Content-Disposition filename");
  } catch (ex) {
    info("correctly ate exception");
  }
  runNextTest();
}

////////////////////////////////////////////////////////////////////////////////
// Test 4: inline
// eslint-disable-next-line no-unused-vars
function handler4(metadata, response) {
  response.setStatusLine(metadata.httpVersion, 200, "OK");
  response.setHeader("Content-Type", "text/plain", false);
  response.setHeader("Content-Disposition", "inline");
  var body = "foo";
  response.bodyOutputStream.write(body, body.length);
}

// eslint-disable-next-line no-unused-vars
function completeTest4(request, data, ctx) {
  try {
    var chan = request.QueryInterface(Ci.nsIChannel);
    Assert.equal(chan.contentDisposition, chan.DISPOSITION_INLINE);
    Assert.equal(chan.contentDispositionHeader, "inline");

    chan.contentDispositionFilename; // should barf
    do_throw("Should have failed getting Content-Disposition filename");
  } catch (ex) {
    info("correctly ate exception");
  }
  runNextTest();
}
