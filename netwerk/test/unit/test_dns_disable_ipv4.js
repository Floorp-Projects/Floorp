//
// Tests that calling asyncResolve with the RESOLVE_DISABLE_IPV4 flag doesn't
// return any IPv4 addresses.
//

"use strict";

const gOverride = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

const defaultOriginAttributes = {};

add_task(async function test_none() {
  let [, inRecord] = await new Promise(resolve => {
    let listener = {
      onLookupComplete(inRequest, inRecord1, inStatus) {
        resolve([inRequest, inRecord1, inStatus]);
      },
      QueryInterface: ChromeUtils.generateQI(["nsIDNSListener"]),
    };

    Services.dns.asyncResolve(
      "example.org",
      Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
      Ci.nsIDNSService.RESOLVE_DISABLE_IPV4,
      null, // resolverInfo
      listener,
      null,
      defaultOriginAttributes
    );
  });

  if (inRecord && inRecord.QueryInterface(Ci.nsIDNSAddrRecord)) {
    while (inRecord.hasMore()) {
      let nextIP = inRecord.getNextAddrAsString();
      ok(nextIP.includes(":"), `${nextIP} should be IPv6`);
    }
  }
});

add_task(async function test_some() {
  Services.dns.clearCache(true);
  gOverride.addIPOverride("example.com", "1.1.1.1");
  gOverride.addIPOverride("example.org", "::1:2:3");
  let [, inRecord] = await new Promise(resolve => {
    let listener = {
      onLookupComplete(inRequest, inRecord1, inStatus) {
        resolve([inRequest, inRecord1, inStatus]);
      },
      QueryInterface: ChromeUtils.generateQI(["nsIDNSListener"]),
    };

    Services.dns.asyncResolve(
      "example.org",
      Ci.nsIDNSService.RESOLVE_TYPE_DEFAULT,
      Ci.nsIDNSService.RESOLVE_DISABLE_IPV4,
      null, // resolverInfo
      listener,
      null,
      defaultOriginAttributes
    );
  });

  ok(inRecord.QueryInterface(Ci.nsIDNSAddrRecord));
  equal(inRecord.getNextAddrAsString(), "::1:2:3");
  equal(inRecord.hasMore(), false);
});
