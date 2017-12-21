function run_test() {
  var ioService = Components.classes["@mozilla.org/network/io-service;1"]
                            .getService(Components.interfaces.nsIIOService);

  try {
    var linkService = Components.classes["@mozilla.org/network/network-link-service;1"]
                                .getService(Components.interfaces.nsINetworkLinkService);

    // The offline status should depends on the link status
    Assert.notEqual(ioService.offline, linkService.isLinkUp);
  } catch (e) {
    // The network link service might not be available
    Assert.equal(ioService.offline, false);
  }
}
