"use strict";

var mainThread = Services.tm.currentThread;

var onionPref;
var localdomainPref;
var prefs = Services.prefs;

// check that we don't lookup .onion
var listenerBlock = {
  onLookupComplete(inRequest, inRecord, inStatus) {
    Assert.ok(!Components.isSuccessCode(inStatus));
    do_test_dontBlock();
  },
  QueryInterface: ChromeUtils.generateQI(["nsIDNSListener"]),
};

// check that we do lookup .onion (via pref)
var listenerDontBlock = {
  onLookupComplete(inRequest, inRecord) {
    inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
    var answer = inRecord.getNextAddrAsString();
    Assert.ok(answer == "127.0.0.1" || answer == "::1");
    all_done();
  },
  QueryInterface: ChromeUtils.generateQI(["nsIDNSListener"]),
};

const defaultOriginAttributes = {};

function do_test_dontBlock() {
  prefs.setBoolPref("network.dns.blockDotOnion", false);
  Services.dns.asyncResolve(
    "private.onion",
    Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
    0,
    null, // resolverInfo
    listenerDontBlock,
    mainThread,
    defaultOriginAttributes
  );
}

function do_test_block() {
  prefs.setBoolPref("network.dns.blockDotOnion", true);
  try {
    Services.dns.asyncResolve(
      "private.onion",
      Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
      0,
      null, // resolverInfo
      listenerBlock,
      mainThread,
      defaultOriginAttributes
    );
  } catch (e) {
    // it is ok for this negative test to fail fast
    Assert.ok(true);
    do_test_dontBlock();
  }
}

function all_done() {
  // reset locally modified prefs
  prefs.setCharPref("network.dns.localDomains", localdomainPref);
  prefs.setBoolPref("network.dns.blockDotOnion", onionPref);
  do_test_finished();
}

function run_test() {
  onionPref = prefs.getBoolPref("network.dns.blockDotOnion");
  localdomainPref = prefs.getCharPref("network.dns.localDomains");
  prefs.setCharPref("network.dns.localDomains", "private.onion");
  do_test_block();
  do_test_pending();
}
