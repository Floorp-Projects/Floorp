/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const gOverride = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

trr_test_setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
});

let trrServer = null;
add_task(async function start_trr_server() {
  trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  dump(`port = ${trrServer.port()}\n`);
  Services.prefs.setBoolPref("network.trr.skip-AAAA-when-not-supported", false);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
});

add_task(async function unspec_first() {
  gOverride.clearOverrides();
  Services.dns.clearCache(true);

  gOverride.addIPOverride("example.org", "1.1.1.1");
  gOverride.addIPOverride("example.org", "::1");

  await trrServer.registerDoHAnswers("example.org", "A", {
    answers: [
      {
        name: "example.org",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });
  // This first request gets cached. IPv6 response gets served from the cache
  await new TRRDNSListener("example.org", { expectedAnswer: "1.2.3.4" });
  await new TRRDNSListener("example.org", {
    flags: Ci.nsIDNSService.RESOLVE_DISABLE_IPV6,
    expectedAnswer: "1.2.3.4",
  });
  let { inStatus } = await new TRRDNSListener("example.org", {
    flags: Ci.nsIDNSService.RESOLVE_DISABLE_IPV4,
    expectedSuccess: false,
  });
  equal(inStatus, Cr.NS_ERROR_UNKNOWN_HOST);
});

add_task(async function A_then_AAAA_fails() {
  gOverride.clearOverrides();
  Services.dns.clearCache(true);

  gOverride.addIPOverride("example.org", "1.1.1.1");
  gOverride.addIPOverride("example.org", "::1");

  await trrServer.registerDoHAnswers("example.org", "A", {
    answers: [
      {
        name: "example.org",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });
  // We do individual IPv4/IPv6 requests - we expect IPv6 not to fallback to Do53 because we have an IPv4 record
  await new TRRDNSListener("example.org", {
    flags: Ci.nsIDNSService.RESOLVE_DISABLE_IPV6,
    expectedAnswer: "1.2.3.4",
  });
  let { inStatus } = await new TRRDNSListener("example.org", {
    flags: Ci.nsIDNSService.RESOLVE_DISABLE_IPV4,
    expectedSuccess: false,
  });
  equal(inStatus, Cr.NS_ERROR_UNKNOWN_HOST);
});

add_task(async function just_AAAA_fails() {
  gOverride.clearOverrides();
  Services.dns.clearCache(true);

  gOverride.addIPOverride("example.org", "1.1.1.1");
  gOverride.addIPOverride("example.org", "::1");

  await trrServer.registerDoHAnswers("example.org", "A", {
    answers: [
      {
        name: "example.org",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });
  // We only do an IPv6 req - we expect IPv6 not to fallback to Do53 because we have an IPv4 record
  let { inStatus } = await new TRRDNSListener("example.org", {
    flags: Ci.nsIDNSService.RESOLVE_DISABLE_IPV4,
    expectedSuccess: false,
  });
  equal(inStatus, Cr.NS_ERROR_UNKNOWN_HOST);
});
