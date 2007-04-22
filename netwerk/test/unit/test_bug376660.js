var Cc = Components.classes;
var Ci = Components.interfaces;

var listener = {
  expect_failure: false,
  QueryInterface: function listener_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIUnicharStreamLoaderObserver)) {
      return this;
    }
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },
  onDetermineCharset : function onDetermineCharset(loader, context,
                                                   data, length)
  {
    return "us-ascii";
  },
  onStreamComplete : function onStreamComplete (loader, context, status, data)
  {
    try {
      if (this.expect_failure)
        do_check_false(Components.isSuccessCode(status));
      else
        do_check_eq(status, Components.results.NS_OK);
      do_check_eq(data, null);
      do_check_neq(loader.channel, null);
      tests[current_test++]();
    } finally {
      do_test_finished();
    }
  }
};

var current_test = 0;
var tests = [test1, test2, done];

function run_test() {
  tests[current_test++]();
}

function test1() {
  var f =
      Cc["@mozilla.org/network/unichar-stream-loader;1"].
      createInstance(Ci.nsIUnicharStreamLoader);
  f.init(listener, 4096);

  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  var chan = ios.newChannel("data:text/plain,", null, null);
  chan.asyncOpen(f, null);
  do_test_pending();
}

function test2() {
  var f =
      Cc["@mozilla.org/network/unichar-stream-loader;1"].
      createInstance(Ci.nsIUnicharStreamLoader);
  f.init(listener, 4096);

  var ios = Components.classes["@mozilla.org/network/io-service;1"]
                      .getService(Components.interfaces.nsIIOService);
  var chan = ios.newChannel("http://localhost:0/", null, null);
  listener.expect_failure = true;
  chan.asyncOpen(f, null);
  do_test_pending();
}

function done() {
}
