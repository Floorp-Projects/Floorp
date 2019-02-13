const {NetUtil} = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

function run_test() {
  do_test_pending();

  function StreamListener() {}

  StreamListener.prototype = {
    QueryInterface: function(aIID) {
      if (aIID.equals(Ci.nsIStreamListener) ||
          aIID.equals(Ci.nsIRequestObserver) ||
          aIID.equals(Ci.nsISupports))
        return this;
      throw Cr.NS_NOINTERFACE;
    },

    onStartRequest: function(aRequest, aContext) {},

    onStopRequest: function(aRequest, aContext, aStatusCode) {
      // Make sure we can catch the error NS_ERROR_FILE_NOT_FOUND here.
      Assert.equal(aStatusCode, Cr.NS_ERROR_FILE_NOT_FOUND);
      do_test_finished();
    },

    onDataAvailable: function(aRequest, aContext, aStream, aOffset, aCount) {
      do_throw("The channel must not call onDataAvailable().");
    }
  };

  let listener = new StreamListener();
  let ios = Cc["@mozilla.org/network/io-service;1"]
              .getService(Ci.nsIIOService);

  // This file does not exist.
  let file = do_get_file("_NOT_EXIST_.txt", true);
  Assert.ok(!file.exists());
  let channel = NetUtil.newChannel({
    uri: ios.newFileURI(file),
    loadUsingSystemPrincipal: true
  });
  channel.asyncOpen(listener);
}
