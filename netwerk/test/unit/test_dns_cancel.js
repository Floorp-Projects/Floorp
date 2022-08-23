"use strict";

var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);

var hostname1 = "";
var hostname2 = "";
var possible = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";

for (var i = 0; i < 20; i++) {
  hostname1 += possible.charAt(Math.floor(Math.random() * possible.length));
  hostname2 += possible.charAt(Math.floor(Math.random() * possible.length));
}

var requestList1Canceled2;
var requestList1NotCanceled;

var requestList2Canceled;
var requestList2NotCanceled;

var listener1 = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    // One request should be resolved and two request should be canceled.
    if (inRequest == requestList1NotCanceled) {
      // This request should not be canceled.
      Assert.notEqual(inStatus, Cr.NS_ERROR_ABORT);

      do_test_finished();
    }
  },
  QueryInterface: ChromeUtils.generateQI(["nsIDNSListener"]),
};

var listener2 = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    // One request should be resolved and the other canceled.
    if (inRequest == requestList2NotCanceled) {
      // The request should not be canceled.
      Assert.notEqual(inStatus, Cr.NS_ERROR_ABORT);

      do_test_finished();
    }
  },
  QueryInterface: ChromeUtils.generateQI(["nsIDNSListener"]),
};

const defaultOriginAttributes = {};

function run_test() {
  var mainThread = Services.tm.currentThread;

  var flags = Ci.nsIDNSService.RESOLVE_BYPASS_CACHE;

  // This one will be canceled with cancelAsyncResolve.
  dns.asyncResolve(
    hostname2,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    flags,
    null, // resolverInfo
    listener1,
    mainThread,
    defaultOriginAttributes
  );
  dns.cancelAsyncResolve(
    hostname2,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    flags,
    null, // resolverInfo
    listener1,
    Cr.NS_ERROR_ABORT,
    defaultOriginAttributes
  );

  // This one will not be canceled.
  requestList1NotCanceled = dns.asyncResolve(
    hostname1,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    flags,
    null, // resolverInfo
    listener1,
    mainThread,
    defaultOriginAttributes
  );

  // This one will be canceled with cancel(Cr.NS_ERROR_ABORT).
  requestList1Canceled2 = dns.asyncResolve(
    hostname1,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    flags,
    null, // resolverInfo
    listener1,
    mainThread,
    defaultOriginAttributes
  );
  requestList1Canceled2.cancel(Cr.NS_ERROR_ABORT);

  // This one will not be canceled.
  requestList2NotCanceled = dns.asyncResolve(
    hostname1,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    flags,
    null, // resolverInfo
    listener2,
    mainThread,
    defaultOriginAttributes
  );

  // This one will be canceled with cancel(Cr.NS_ERROR_ABORT).
  requestList2Canceled = dns.asyncResolve(
    hostname2,
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    flags,
    null, // resolverInfo
    listener2,
    mainThread,
    defaultOriginAttributes
  );
  requestList2Canceled.cancel(Cr.NS_ERROR_ABORT);

  do_test_pending();
  do_test_pending();
}
