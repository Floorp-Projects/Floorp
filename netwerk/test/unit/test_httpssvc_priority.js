/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

trr_test_setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
  Services.prefs.clearUserPref("network.dns.echconfig.enabled");
});

add_task(async function testPriorityAndECHConfig() {
  let trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();

  Services.prefs.setBoolPref("network.dns.echconfig.enabled", false);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  await trrServer.registerDoHAnswers("test.priority.com", "HTTPS", {
    answers: [
      {
        name: "test.priority.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "test.p1.com",
          values: [{ key: "alpn", value: ["h2", "h3"] }],
        },
      },
      {
        name: "test.priority.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 4,
          name: "test.p4.com",
          values: [{ key: "echconfig", value: "456..." }],
        },
      },
      {
        name: "test.priority.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 3,
          name: "test.p3.com",
          values: [{ key: "ipv4hint", value: "1.2.3.4" }],
        },
      },
      {
        name: "test.priority.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 2,
          name: "test.p2.com",
          values: [{ key: "echconfig", value: "123..." }],
        },
      },
    ],
  });

  let { inRecord } = await new TRRDNSListener("test.priority.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  Assert.equal(answer.length, 4);

  Assert.equal(answer[0].priority, 1);
  Assert.equal(answer[0].name, "test.p1.com");

  Assert.equal(answer[1].priority, 2);
  Assert.equal(answer[1].name, "test.p2.com");

  Assert.equal(answer[2].priority, 3);
  Assert.equal(answer[2].name, "test.p3.com");

  Assert.equal(answer[3].priority, 4);
  Assert.equal(answer[3].name, "test.p4.com");

  Services.prefs.setBoolPref("network.dns.echconfig.enabled", true);
  Services.dns.clearCache(true);
  ({ inRecord } = await new TRRDNSListener("test.priority.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  }));

  answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  Assert.equal(answer.length, 4);

  Assert.equal(answer[0].priority, 2);
  Assert.equal(answer[0].name, "test.p2.com");
  Assert.equal(
    answer[0].values[0].QueryInterface(Ci.nsISVCParamEchConfig).echconfig,
    "123...",
    "got correct answer"
  );

  Assert.equal(answer[1].priority, 4);
  Assert.equal(answer[1].name, "test.p4.com");
  Assert.equal(
    answer[1].values[0].QueryInterface(Ci.nsISVCParamEchConfig).echconfig,
    "456...",
    "got correct answer"
  );

  Assert.equal(answer[2].priority, 1);
  Assert.equal(answer[2].name, "test.p1.com");

  Assert.equal(answer[3].priority, 3);
  Assert.equal(answer[3].name, "test.p3.com");
});
