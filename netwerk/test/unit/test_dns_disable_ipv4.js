//
// Tests that calling asyncResolve with the RESOLVE_DISABLE_IPV4 flag doesn't
// return any IPv4 addresses.
//

var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
var ioService = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

var listener = {
  onLookupComplete: function(inRequest, inRecord, inStatus) {
    if (inStatus != Cr.NS_OK) {
      do_check_eq(inStatus, Cr.NS_ERROR_UNKNOWN_HOST);
      do_test_finished();
      return;
    }

    while (true) {
      try {
        var answer = inRecord.getNextAddrAsString();
        // If there is an answer it should be an IPv6  address
        dump(answer);
        do_check_true(answer.indexOf(':') != -1);
      } catch (e) {
        break;
      }
    }
    do_test_finished();
  }
};

function run_test() {
  do_test_pending();
  try {
    dns.asyncResolve("example.org", Ci.nsIDNSService.RESOLVE_DISABLE_IPV4, listener, null);
  } catch (e) {
    dump(e);
    do_check_true(false);
    do_test_finished();
  }
}
