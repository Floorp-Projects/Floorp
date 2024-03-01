"use strict";

function run_test() {
  do_test_pending();

  function StreamListener() {}

  StreamListener.prototype = {
    QueryInterface: ChromeUtils.generateQI([
      "nsIStreamListener",
      "nsIRequestObserver",
    ]),

    onStartRequest() {},

    onStopRequest(aRequest, aStatusCode) {
      // Make sure we can catch the error NS_ERROR_FILE_NOT_FOUND here.
      Assert.equal(aStatusCode, Cr.NS_ERROR_FILE_NOT_FOUND);
      do_test_finished();
    },

    onDataAvailable() {
      do_throw("The channel must not call onDataAvailable().");
    },
  };

  let listener = new StreamListener();

  // This file does not exist.
  let file = do_get_file("_NOT_EXIST_.txt", true);
  Assert.ok(!file.exists());
  let channel = NetUtil.newChannel({
    uri: Services.io.newFileURI(file),
    loadUsingSystemPrincipal: true,
  });
  channel.asyncOpen(listener);
}
