/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const { TestUtils } = ChromeUtils.import(
  "resource://testing-common/TestUtils.jsm"
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
  dump(`port = ${trrServer.port}\n`);

  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
});

add_task(async function sanity_check() {
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
  // Simple check to see that TRR works.
  await new TRRDNSListener("example.com", { expectedAnswer: "1.2.3.4" });
});

add_task(async function cancel_immediately() {
  await trrServer.registerDoHAnswers("example.org", "A", {
    answers: [
      {
        name: "example.org",
        ttl: 55,
        type: "A",
        flush: false,
        data: "2.3.4.5",
      },
    ],
  });
  let r1 = new TRRDNSListener("example.org", { expectedSuccess: false });
  let r2 = new TRRDNSListener("example.org", { expectedAnswer: "2.3.4.5" });
  r1.cancel();
  let [, , inStatus] = await r1;
  equal(inStatus, Cr.NS_ERROR_ABORT);
  await r2;
  equal(await trrServer.requestCount("example.org", "A"), 1);

  // Now we cancel both of them
  dns.clearCache(true);
  r1 = new TRRDNSListener("example.org", { expectedSuccess: false });
  r2 = new TRRDNSListener("example.org", { expectedSuccess: false });
  r1.cancel();
  r2.cancel();
  [, , inStatus] = await r1;
  equal(inStatus, Cr.NS_ERROR_ABORT);
  [, , inStatus] = await r2;
  equal(inStatus, Cr.NS_ERROR_ABORT);
  await new Promise(resolve => do_timeout(50, resolve));
  equal(await trrServer.requestCount("example.org", "A"), 2);
});

add_task(async function cancel_delayed() {
  dns.clearCache(true);
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
    delay: 500,
  });
  let r1 = new TRRDNSListener("example.com", { expectedSuccess: false });
  let r2 = new TRRDNSListener("example.com", { expectedAnswer: "1.1.1.1" });
  await new Promise(resolve => do_timeout(50, resolve));
  r1.cancel();
  let [, , inStatus] = await r1;
  equal(inStatus, Cr.NS_ERROR_ABORT);
  await r2;
});

add_task(async function cancel_after_completed() {
  dns.clearCache(true);
  await trrServer.registerDoHAnswers("example.com", "A", {
    answers: [
      {
        name: "example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "2.2.2.2",
      },
    ],
  });
  let r1 = new TRRDNSListener("example.com", { expectedAnswer: "2.2.2.2" });
  await r1;
  let r2 = new TRRDNSListener("example.com", { expectedAnswer: "2.2.2.2" });
  // Check that cancelling r1 after it's complete does not affect r2 in any way.
  r1.cancel();
  await r2;
});

add_task(async function clearCacheWhileResolving() {
  dns.clearCache(true);
  await trrServer.registerDoHAnswers("example.com", "A", {
    answers: [
      {
        name: "example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "3.3.3.3",
      },
    ],
    delay: 500,
  });
  // Check that calling clearCache does not leave the request hanging.
  let r1 = new TRRDNSListener("example.com", { expectedAnswer: "3.3.3.3" });
  let r2 = new TRRDNSListener("example.com", { expectedAnswer: "3.3.3.3" });
  dns.clearCache(true);
  await r1;
  await r2;

  // Also check the same for HTTPS records
  await trrServer.registerDoHAnswers("httpsvc.com", "HTTPS", {
    answers: [
      {
        name: "httpsvc.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "test.p1.com",
          values: [{ key: "alpn", value: ["h2", "h3"] }],
        },
      },
    ],
    delay: 500,
  });
  let r3 = new TRRDNSListener("httpsvc.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });
  let r4 = new TRRDNSListener("httpsvc.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });
  dns.clearCache(true);
  await r3;
  await r4;
  equal(await trrServer.requestCount("httpsvc.com", "HTTPS"), 1);
  dns.clearCache(true);
  await new TRRDNSListener("httpsvc.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });
  equal(await trrServer.requestCount("httpsvc.com", "HTTPS"), 2);
});
