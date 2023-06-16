"use strict";

const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);
const mainThread = Services.tm.currentThread;

function makeListenerBlock(next) {
  return {
    onLookupComplete(inRequest, inRecord, inStatus) {
      Assert.ok(!Components.isSuccessCode(inStatus));
      next();
    },
  };
}

function makeListenerDontBlock(next, expectedAnswer) {
  return {
    onLookupComplete(inRequest, inRecord, inStatus) {
      Assert.equal(inStatus, Cr.NS_OK);
      inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
      var answer = inRecord.getNextAddrAsString();
      if (expectedAnswer) {
        Assert.equal(answer, expectedAnswer);
      } else {
        Assert.ok(answer == "127.0.0.1" || answer == "::1");
      }
      next();
    },
  };
}

function do_test({ dnsDisabled, mustBlock, testDomain, expectedAnswer }) {
  return new Promise(resolve => {
    Services.prefs.setBoolPref("network.dns.disabled", dnsDisabled);
    try {
      Services.dns.asyncResolve(
        testDomain,
        Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
        0,
        null, // resolverInfo
        mustBlock
          ? makeListenerBlock(resolve)
          : makeListenerDontBlock(resolve, expectedAnswer),
        mainThread,
        {} // Default origin attributes
      );
    } catch (e) {
      Assert.ok(mustBlock === true);
      resolve();
    }
  });
}

function setup() {
  override.addIPOverride("foo.bar", "127.0.0.1");
  registerCleanupFunction(function () {
    override.clearOverrides();
    Services.prefs.clearUserPref("network.dns.disabled");
  });
}
setup();

// IP literals should be resolved even if dns is disabled
add_task(async function testIPLiteral() {
  return do_test({
    dnsDisabled: true,
    mustBlock: false,
    testDomain: "0x01010101",
    expectedAnswer: "1.1.1.1",
  });
});

add_task(async function testBlocked() {
  return do_test({
    dnsDisabled: true,
    mustBlock: true,
    testDomain: "foo.bar",
  });
});

add_task(async function testNotBlocked() {
  return do_test({
    dnsDisabled: false,
    mustBlock: false,
    testDomain: "foo.bar",
  });
});
