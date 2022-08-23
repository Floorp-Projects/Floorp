// This testcase verifies that channels can't be reopened
// See https://bugzilla.mozilla.org/show_bug.cgi?id=372486

"use strict";

const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream"
);

const NS_ERROR_IN_PROGRESS = 0x804b000f;
const NS_ERROR_ALREADY_OPENED = 0x804b0049;

var chan = null;
var httpserv = null;

[test_data_channel, test_http_channel, test_file_channel, end].forEach(f =>
  add_test(f)
);

// Utility functions

function makeChan(url) {
  return (chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIChannel));
}

function new_file_channel(file) {
  return NetUtil.newChannel({
    uri: Services.io.newFileURI(file),
    loadUsingSystemPrincipal: true,
  });
}

function check_throws(closure, error) {
  var thrown = false;
  try {
    closure();
  } catch (e) {
    if (error instanceof Array) {
      Assert.notEqual(error.indexOf(e.result), -1);
    } else {
      Assert.equal(e.result, error);
    }
    thrown = true;
  }
  Assert.ok(thrown);
}

function check_open_throws(error) {
  check_throws(function() {
    chan.open(listener);
  }, error);
}

function check_async_open_throws(error) {
  check_throws(function() {
    chan.asyncOpen(listener);
  }, error);
}

var listener = {
  onStartRequest: function test_onStartR(request) {
    check_async_open_throws(NS_ERROR_IN_PROGRESS);
  },

  onDataAvailable: function test_ODA(request, inputStream, offset, count) {
    new BinaryInputStream(inputStream).readByteArray(count); // required by API
    check_async_open_throws(NS_ERROR_IN_PROGRESS);
  },

  onStopRequest: function test_onStopR(request, status) {
    // Once onStopRequest is reached, the channel is marked as having been
    // opened
    check_async_open_throws(NS_ERROR_ALREADY_OPENED);
    do_timeout(0, after_channel_closed);
  },
};

function after_channel_closed() {
  check_async_open_throws(NS_ERROR_ALREADY_OPENED);

  run_next_test();
}

function test_channel(createChanClosure) {
  // First, synchronous reopening test
  chan = createChanClosure();
  chan.open();
  check_open_throws(NS_ERROR_IN_PROGRESS);
  check_async_open_throws([NS_ERROR_IN_PROGRESS, NS_ERROR_ALREADY_OPENED]);

  // Then, asynchronous one
  chan = createChanClosure();
  chan.asyncOpen(listener);
  check_open_throws(NS_ERROR_IN_PROGRESS);
  check_async_open_throws(NS_ERROR_IN_PROGRESS);
}

function test_data_channel() {
  test_channel(function() {
    return makeChan("data:text/plain,foo");
  });
}

function test_http_channel() {
  test_channel(function() {
    return makeChan("http://localhost:" + httpserv.identity.primaryPort + "/");
  });
}

function test_file_channel() {
  var file = do_get_file("data/test_readline1.txt");
  test_channel(function() {
    return new_file_channel(file);
  });
}

function end() {
  httpserv.stop(do_test_finished);
}

function run_test() {
  // start server
  httpserv = new HttpServer();
  httpserv.start(-1);

  run_next_test();
}
