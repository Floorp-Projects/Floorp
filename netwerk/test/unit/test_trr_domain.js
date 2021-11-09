/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

// This test simmulates intermittent native DNS functionality.
// We verify that we don't use the negative DNS record for the DoH server.
// The first resolve of foo.example.com fails, so we expect TRR not to work.
// Immediately after the native DNS starts working, it should connect to the
// TRR server and start working.

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

function setup() {
  trr_test_setup();
  Services.prefs.clearUserPref("network.trr.bootstrapAddr");
  Services.prefs.clearUserPref("network.dns.native-is-localhost");
}
setup();

registerCleanupFunction(async () => {
  trr_clear_prefs();
  override.clearOverrides();
});

add_task(async function intermittent_dns_mode3() {
  override.addIPOverride("foo.example.com", "N/A");
  let trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  info(`port = ${trrServer.port}\n`);
  dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);
  await trrServer.registerDoHAnswers("example.com", "A", {
    answers: [
      {
        name: "example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });
  let [, , inStatus] = await new TRRDNSListener("example.com", {
    expectedSuccess: false,
  });
  equal(inStatus, Cr.NS_ERROR_UNKNOWN_HOST);
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
  override.addIPOverride("foo.example.com", "127.0.0.1");
  await new TRRDNSListener("example.org", { expectedAnswer: "1.2.3.4" });
  await trrServer.stop();
});

add_task(async function intermittent_dns_mode2() {
  override.addIPOverride("foo.example.com", "N/A");
  let trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  info(`port = ${trrServer.port}\n`);

  dns.clearCache(true);
  Services.prefs.setIntPref(
    "network.trr.mode",
    Ci.nsIDNSService.MODE_NATIVEONLY
  );
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  await trrServer.registerDoHAnswers("example.com", "A", {
    answers: [
      {
        name: "example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.1.1.1",
      },
    ],
  });
  override.addIPOverride("example.com", "2.2.2.2");
  await new TRRDNSListener("example.com", {
    expectedAnswer: "2.2.2.2",
  });
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
  override.addIPOverride("example.org", "3.3.3.3");
  override.addIPOverride("foo.example.com", "127.0.0.1");
  await new TRRDNSListener("example.org", { expectedAnswer: "1.2.3.4" });
  await trrServer.stop();
});
