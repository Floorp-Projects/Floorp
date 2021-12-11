//
// Tests that calling asyncResolve with the RESOLVE_DISABLE_IPV6 flag doesn't
// return any IPv6 addresses.
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
        inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
        var answer = inRecord.getNextAddrAsString();
        // If there is an answer it should be an IPv4  address
        dump(answer);
        Assert.ok(!answer.includes(":"));
        Assert.ok(answer.includes("."));
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
      "example.com",
      Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
      Ci.nsIDNSService.RESOLVE_DISABLE_IPV6,
      null, // resolverInfo
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
