// This testcase verifies that channels can't be reopened
// See https://bugzilla.mozilla.org/show_bug.cgi?id=372486

do_import_script("netwerk/test/httpserver/httpd.js");

const NS_ERROR_IN_PROGRESS = 0x804b000f;
const NS_ERROR_ALREADY_OPENED = 0x804b0049;

var chan = null;
var httpserv = null;

var test_index = 0;
var test_array = [
  test_data_channel,
  test_http_channel,
  test_file_channel,
  // Commented by default as it relies on external ressources
  //test_ftp_channel,
  end
];

// Utility functions

var BinaryInputStream =
          Components.Constructor("@mozilla.org/binaryinputstream;1",
                                 "nsIBinaryInputStream", "setInputStream");

function makeChan(url) {
  var ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);
  return chan = ios.newChannel(url, null, null)
                   .QueryInterface(Ci.nsIChannel);
}

function new_file_channel(file) {
  var ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);
  return ios.newChannelFromURI(ios.newFileURI(file));
}


function check_throws(closure, error) {
  var thrown = false;
  try {
    closure();
  } catch (e) {
    do_check_eq(e.result, error);
    thrown = true;
  }
  do_check_true(thrown);
}

function check_open_throws(error) {
  check_throws(function() {
    chan.open(listener, null);
  }, error);
}

function check_async_open_throws(error) {
  check_throws(function() {
    chan.asyncOpen(listener, null);
  }, error);
}

var listener = {
  onStartRequest: function test_onStartR(request, ctx) {
    check_async_open_throws(NS_ERROR_IN_PROGRESS);
  },

  onDataAvailable: function test_ODA(request, cx, inputStream,
                                     offset, count) {
    new BinaryInputStream(inputStream).readByteArray(count); // required by API
    check_async_open_throws(NS_ERROR_IN_PROGRESS);
  },

  onStopRequest: function test_onStopR(request, ctx, status) {
    // Once onStopRequest is reached, the channel is marked as having been
    // opened
    check_async_open_throws(NS_ERROR_ALREADY_OPENED);
    do_timeout(0, "after_channel_closed()");
  }
};

function after_channel_closed() {
  check_async_open_throws(NS_ERROR_ALREADY_OPENED);

  run_next_test();
}

function run_next_test() {
  test_array[test_index++]();
}

function test_channel(createChanClosure) {
  // First, synchronous reopening test
  chan = createChanClosure();
  var inputStream = chan.open();
  check_open_throws(NS_ERROR_IN_PROGRESS);
  check_async_open_throws(NS_ERROR_ALREADY_OPENED);
  
  // Then, asynchronous one
  chan = createChanClosure();
  chan.asyncOpen(listener, null);
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
    return makeChan("http://localhost:4444/");
  });
}

function test_file_channel() {
  var file = do_get_file("netwerk/test/Makefile.in");
  test_channel(function() {
    return new_file_channel(file);
  });
}

// Uncomment test_ftp_channel in test_array to test this
function test_ftp_channel() {
  test_channel(function() {
    return makeChan("ftp://ftp.mozilla.org/pub/mozilla.org/README");
  });
}

function end() {
  httpserv.stop();
  do_test_finished();
}

function run_test() {
  // start server
  httpserv = new nsHttpServer();
  httpserv.start(4444);
  
  do_test_pending();
  run_next_test();
}
