"use strict";

function run_test() {
  var ioService = Cc["@mozilla.org/network/io-service;1"].getService(
    Ci.nsIIOService
  );

  try {
    var linkService = Cc[
      "@mozilla.org/network/network-link-service;1"
    ].getService(Ci.nsINetworkLinkService);

    // The offline status should depends on the link status
    Assert.notEqual(ioService.offline, linkService.isLinkUp);
  } catch (e) {
    // The network link service might not be available
    Assert.equal(ioService.offline, false);
  }
}
