"use strict";

function run_test() {
  try {
    var linkService = Cc[
      "@mozilla.org/network/network-link-service;1"
    ].getService(Ci.nsINetworkLinkService);

    // The offline status should depends on the link status
    Assert.notEqual(Services.io.offline, linkService.isLinkUp);
  } catch (e) {
    // The network link service might not be available
    Assert.equal(Services.io.offline, false);
  }
}
