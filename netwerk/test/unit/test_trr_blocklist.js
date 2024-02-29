/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

function setup() {
  trr_test_setup();
  Services.prefs.setBoolPref("network.trr.temp_blocklist", true);
}
setup();

// Waits until a predicate returns true or re-tries the predicate calls
// |retry| times, we wait for 100ms between each calls.
async function waitUntil(predicate, retry = 20) {
  let count = 0;
  while (count++ < retry) {
    if (await predicate()) {
      return true;
    }
    // Wait for 100 milliseconds.
    await new Promise(resolve => do_timeout(100, resolve));
  }
  // Timed out after trying too many times.
  return false;
}

add_task(async function checkBlocklisting() {
  let trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  info(`port = ${trrServer.port()}\n`);

  Services.dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);

  await trrServer.registerDoHAnswers("top.test.com", "NS", {});

  override.addIPOverride("sub.top.test.com", "2.2.2.2");
  override.addIPOverride("sub2.top.test.com", "2.2.2.2");
  await new TRRDNSListener("sub.top.test.com", {
    expectedAnswer: "2.2.2.2",
  });
  equal(await trrServer.requestCount("sub.top.test.com", "A"), 1);

  // Clear the cache so that we need to consult the blocklist and not simply
  // return the cached DNS record.
  Services.dns.clearCache(true);
  await new TRRDNSListener("sub.top.test.com", {
    expectedAnswer: "2.2.2.2",
  });
  equal(
    await trrServer.requestCount("sub.top.test.com", "A"),
    1,
    "Request should go directly to native because result is still in blocklist"
  );

  // XXX(valentin): if this ever starts intermittently failing we need to add
  // a sleep here. But the check for the parent NS should normally complete
  // before the second subdomain request.
  equal(
    await trrServer.requestCount("top.test.com", "NS"),
    1,
    "Should have checked parent domain"
  );
  await new TRRDNSListener("sub2.top.test.com", {
    expectedAnswer: "2.2.2.2",
  });
  equal(await trrServer.requestCount("sub2.top.test.com", "A"), 0);

  // The blocklist should instantly expire.
  Services.prefs.setIntPref("network.trr.temp_blocklist_duration_sec", 0);
  Services.dns.clearCache(true);
  await new TRRDNSListener("sub.top.test.com", {
    expectedAnswer: "2.2.2.2",
  });
  // blocklist expired. Do another check.
  equal(
    await trrServer.requestCount("sub.top.test.com", "A"),
    2,
    "We should do another TRR request because the bloclist expired"
  );
});

add_task(async function test_blocklist_cname() {
  let trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  info(`port = ${trrServer.port()}\n`);

  Services.dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);

  await trrServer.registerDoHAnswers(`top.test.com`, "NS", {
    answers: [
      {
        name: "top.test.com",
        ttl: 55,
        type: "CNAME",
        flush: false,
        data: "other.foo",
      },
    ],
  });

  await trrServer.registerDoHAnswers(`other.foo`, "NS", {
    answers: [
      {
        name: "other.foo",
        ttl: 55,
        type: "NS",
        flush: false,
        data: "ns.other.foo",
      },
    ],
  });

  override.addIPOverride("sub.top.test.com", "2.2.2.2");
  await new TRRDNSListener("sub.top.test.com", {
    expectedAnswer: "2.2.2.2",
  });

  await waitUntil(async () => {
    return (await trrServer.requestCount("top.test.com", "NS")) == 1;
  });
});
