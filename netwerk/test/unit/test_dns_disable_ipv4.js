//
// Tests that calling asyncResolve with the RESOLVE_DISABLE_IPV4 flag doesn't
// return any IPv4 addresses.
//

"use strict";

var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
var ioService = Cc["@mozilla.org/network/io-service;1"].getService(
  Ci.nsIIOService
);

var listener = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    if (inStatus != Cr.NS_OK) {
      Assert.equal(inStatus, Cr.NS_ERROR_UNKNOWN_HOST);
      do_test_finished();
      return;
    }

    while (true) {
      try {
        var answer = inRecord.getNextAddrAsString();
        // If there is an answer it should be an IPv6  address
        dump(answer);
        Assert.ok(answer.includes(":"));
      } catch (e) {
        break;
      }
    }
    do_test_finished();
  },
};

const defaultOriginAttributes = {};

function run_test() {
  do_test_pending();
  try {
    dns.asyncResolve(
      "example.org",
      Ci.nsIDNSService.RESOLVE_DISABLE_IPV4,
      listener,
      null,
      defaultOriginAttributes
    );
  } catch (e) {
    dump(e);
    Assert.ok(false);
    do_test_finished();
  }
}
