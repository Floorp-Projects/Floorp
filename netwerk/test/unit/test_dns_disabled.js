"use strict";

var dns = Cc["@mozilla.org/network/dns-service;1"].getService(Ci.nsIDNSService);
var threadManager = Cc["@mozilla.org/thread-manager;1"].getService(
  Ci.nsIThreadManager
);
var mainThread = threadManager.currentThread;

var disabledPref;
var localdomainPref;
var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
  Ci.nsIPrefBranch
);
var defaultOriginAttributes = {};

function makeListenerBlock(next) {
  return {
    onLookupComplete(inRequest, inRecord, inStatus) {
      Assert.ok(!Components.isSuccessCode(inStatus));
      next();
    },
    QueryInterface: ChromeUtils.generateQI(["nsIDNSListener"]),
  };
}

function makeListenerDontBlock(next, expectedAnswer) {
  return {
    onLookupComplete(inRequest, inRecord, inStatus) {
      var answer = inRecord.getNextAddrAsString();
      if (expectedAnswer) {
        Assert.equal(answer, expectedAnswer);
      } else {
        Assert.ok(answer == "127.0.0.1" || answer == "::1");
      }
      next();
    },
    QueryInterface: ChromeUtils.generateQI(["nsIDNSListener"]),
  };
}

function do_test({
  dnsDisabled,
  mustBlock,
  testDomain,
  nextCallback,
  expectedAnswer,
}) {
  prefs.setBoolPref("network.dns.disabled", dnsDisabled);
  try {
    dns.asyncResolve(
      testDomain,
      0,
      mustBlock
        ? makeListenerBlock(nextCallback)
        : makeListenerDontBlock(nextCallback, expectedAnswer),
      mainThread,
      defaultOriginAttributes
    );
  } catch (e) {
    Assert.ok(mustBlock === true);
    nextCallback();
  }
}

function all_done() {
  // Reset locally modified prefs
  prefs.setCharPref("network.dns.localDomains", localdomainPref);
  prefs.setBoolPref("network.dns.disabled", disabledPref);
  do_test_finished();
}

// Cached hostnames should be resolved even if dns is disabled
function testCached() {
  do_test({
    dnsDisabled: true,
    mustBlock: false,
    testDomain: "foo.bar",
    nextCallback: all_done,
  });
}

function testNotBlocked() {
  do_test({
    dnsDisabled: false,
    mustBlock: false,
    testDomain: "foo.bar",
    nextCallback: testCached,
  });
}

function testBlocked() {
  do_test({
    dnsDisabled: true,
    mustBlock: true,
    testDomain: "foo.bar",
    nextCallback: testNotBlocked,
  });
}

// IP literals should be resolved even if dns is disabled
function testIPLiteral() {
  do_test({
    dnsDisabled: true,
    mustBlock: false,
    testDomain: "0x01010101",
    nextCallback: testBlocked,
    expectedAnswer: "1.1.1.1",
  });
}

function run_test() {
  do_test_pending();
  disabledPref = prefs.getBoolPref("network.dns.disabled");
  localdomainPref = prefs.getCharPref("network.dns.localDomains");
  prefs.setCharPref("network.dns.localDomains", "foo.bar");
  testIPLiteral();
}
