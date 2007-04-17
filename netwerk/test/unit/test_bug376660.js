var Cc = Components.classes;
var Ci = Components.interfaces;

var listener = {
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
    do_check_eq(status, Components.results.NS_OK);
    do_check_eq(data, null);
    do_check_neq(loader.channel, null);
    do_test_finished();
  }
};

function run_test() {
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
