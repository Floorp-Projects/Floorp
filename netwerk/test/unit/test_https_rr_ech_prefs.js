/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let trrServer;

function setup() {
  trr_test_setup();

  Services.prefs.setBoolPref("network.dns.upgrade_with_https_rr", true);
  Services.prefs.setBoolPref("network.dns.use_https_rr_as_altsvc", true);
  Services.prefs.setBoolPref("network.dns.echconfig.enabled", true);
}

setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
  Services.prefs.clearUserPref("network.dns.upgrade_with_https_rr");
  Services.prefs.clearUserPref("network.dns.use_https_rr_as_altsvc");
  Services.prefs.clearUserPref("network.dns.echconfig.enabled");
  Services.prefs.clearUserPref("network.dns.http3_echconfig.enabled");
  Services.prefs.clearUserPref("network.http.http3.enable");
  Services.prefs.clearUserPref("network.http.http2.enabled");
  if (trrServer) {
    await trrServer.stop();
  }
});

function checkResult(inRecord, noHttp2, noHttp3, result) {
  if (!result) {
    Assert.throws(
      () => {
        inRecord
          .QueryInterface(Ci.nsIDNSHTTPSSVCRecord)
          .GetServiceModeRecord(noHttp2, noHttp3);
      },
      /NS_ERROR_NOT_AVAILABLE/,
      "Should get an error"
    );
    return;
  }

  let record = inRecord
    .QueryInterface(Ci.nsIDNSHTTPSSVCRecord)
    .GetServiceModeRecord(noHttp2, noHttp3);
  Assert.equal(record.priority, result.expectedPriority);
  Assert.equal(record.name, result.expectedName);
  Assert.equal(record.selectedAlpn, result.expectedAlpn);
}

// Test configuration:
//   There are two records: one has a echConfig and the other doesn't.
// We want to test if the record with echConfig is preferred.
add_task(async function testEchConfigEnabled() {
  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
  Services.prefs.setBoolPref("network.dns.echconfig.enabled", false);

  await trrServer.registerDoHAnswers("test.bar.com", "HTTPS", {
    answers: [
      {
        name: "test.bar.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "test.bar_1.com",
          values: [{ key: "alpn", value: ["h3-29"] }],
        },
      },
      {
        name: "test.bar.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 2,
          name: "test.bar_2.com",
          values: [
            { key: "alpn", value: ["h2"] },
            { key: "echconfig", value: "456..." },
          ],
        },
      },
    ],
  });

  let { inRecord } = await new TRRDNSListener("test.bar.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

  checkResult(inRecord, false, false, {
    expectedPriority: 1,
    expectedName: "test.bar_1.com",
    expectedAlpn: "h3-29",
  });
  checkResult(inRecord, false, true, {
    expectedPriority: 2,
    expectedName: "test.bar_2.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, true, false, {
    expectedPriority: 1,
    expectedName: "test.bar_1.com",
    expectedAlpn: "h3-29",
  });
  checkResult(inRecord, true, true);

  Services.prefs.setBoolPref("network.dns.echconfig.enabled", true);
  Services.dns.clearCache(true);

  ({ inRecord } = await new TRRDNSListener("test.bar.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  }));

  checkResult(inRecord, false, false, {
    expectedPriority: 2,
    expectedName: "test.bar_2.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, false, true, {
    expectedPriority: 2,
    expectedName: "test.bar_2.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, true, false, {
    expectedPriority: 1,
    expectedName: "test.bar_1.com",
    expectedAlpn: "h3-29",
  });
  checkResult(inRecord, true, true);

  await trrServer.stop();
  trrServer = null;
});

// Test configuration:
//   There are two records: both have echConfigs, and only one supports h3.
// This test is about testing which record should we get when
// network.dns.http3_echconfig.enabled is true and false.
// When network.dns.http3_echconfig.enabled is false, we should try to
// connect with h2 and echConfig.
add_task(async function testTwoRecordsHaveEchConfig() {
  Services.dns.clearCache(true);

  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setBoolPref("network.dns.echconfig.enabled", true);
  Services.prefs.setBoolPref("network.dns.http3_echconfig.enabled", false);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  await trrServer.registerDoHAnswers("test.foo.com", "HTTPS", {
    answers: [
      {
        name: "test.foo.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "test.foo_h3.com",
          values: [
            { key: "alpn", value: ["h3"] },
            { key: "echconfig", value: "456..." },
          ],
        },
      },
      {
        name: "test.foo.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 2,
          name: "test.foo_h2.com",
          values: [
            { key: "alpn", value: ["h2"] },
            { key: "echconfig", value: "456..." },
          ],
        },
      },
    ],
  });

  let { inRecord } = await new TRRDNSListener("test.foo.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

  checkResult(inRecord, false, false, {
    expectedPriority: 2,
    expectedName: "test.foo_h2.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, false, true, {
    expectedPriority: 2,
    expectedName: "test.foo_h2.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, true, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, true, true);

  Services.prefs.setBoolPref("network.dns.http3_echconfig.enabled", true);
  Services.dns.clearCache(true);
  ({ inRecord } = await new TRRDNSListener("test.foo.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  }));

  checkResult(inRecord, false, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, false, true, {
    expectedPriority: 2,
    expectedName: "test.foo_h2.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, true, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, true, true);

  await trrServer.stop();
  trrServer = null;
});

// Test configuration:
//   There are two records: both have echConfigs, and one supports h3 and h2.
// When network.dns.http3_echconfig.enabled is false, we should use the record
// that supports h3 and h2 (the alpn is h2).
add_task(async function testTwoRecordsHaveEchConfig1() {
  Services.dns.clearCache(true);

  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setBoolPref("network.dns.echconfig.enabled", true);
  Services.prefs.setBoolPref("network.dns.http3_echconfig.enabled", false);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  await trrServer.registerDoHAnswers("test.foo.com", "HTTPS", {
    answers: [
      {
        name: "test.foo.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "test.foo_h3.com",
          values: [
            { key: "alpn", value: ["h3", "h2"] },
            { key: "echconfig", value: "456..." },
          ],
        },
      },
      {
        name: "test.foo.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 2,
          name: "test.foo_h2.com",
          values: [
            { key: "alpn", value: ["h2", "http/1.1"] },
            { key: "echconfig", value: "456..." },
          ],
        },
      },
    ],
  });

  let { inRecord } = await new TRRDNSListener("test.foo.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

  checkResult(inRecord, false, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, false, true, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, true, false, {
    expectedPriority: 2,
    expectedName: "test.foo_h2.com",
    expectedAlpn: "http/1.1",
  });
  checkResult(inRecord, true, true, {
    expectedPriority: 2,
    expectedName: "test.foo_h2.com",
    expectedAlpn: "http/1.1",
  });

  Services.prefs.setBoolPref("network.dns.http3_echconfig.enabled", true);
  Services.dns.clearCache(true);
  ({ inRecord } = await new TRRDNSListener("test.foo.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  }));

  checkResult(inRecord, false, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, false, true, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, true, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, true, true, {
    expectedPriority: 2,
    expectedName: "test.foo_h2.com",
    expectedAlpn: "http/1.1",
  });

  await trrServer.stop();
  trrServer = null;
});

// Test configuration:
//   There are two records: only one support h3 and only one has echConfig.
// This test is about never usng the record without echConfig.
add_task(async function testOneRecordsHasEchConfig() {
  Services.dns.clearCache(true);

  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setBoolPref("network.dns.echconfig.enabled", true);
  Services.prefs.setBoolPref("network.dns.http3_echconfig.enabled", false);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  await trrServer.registerDoHAnswers("test.foo.com", "HTTPS", {
    answers: [
      {
        name: "test.foo.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "test.foo_h3.com",
          values: [
            { key: "alpn", value: ["h3"] },
            { key: "echconfig", value: "456..." },
          ],
        },
      },
      {
        name: "test.foo.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 2,
          name: "test.foo_h2.com",
          values: [{ key: "alpn", value: ["h2"] }],
        },
      },
    ],
  });

  let { inRecord } = await new TRRDNSListener("test.foo.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

  checkResult(inRecord, false, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, false, true, {
    expectedPriority: 2,
    expectedName: "test.foo_h2.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, true, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, true, true);

  Services.prefs.setBoolPref("network.dns.http3_echconfig.enabled", true);
  Services.dns.clearCache(true);
  ({ inRecord } = await new TRRDNSListener("test.foo.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  }));

  checkResult(inRecord, false, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, false, true, {
    expectedPriority: 2,
    expectedName: "test.foo_h2.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, true, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, true, true);

  await trrServer.stop();
  trrServer = null;
});

// Test the case that "network.http.http3.enable" and
// "network.http.http2.enabled" are true/false.
add_task(async function testHttp3AndHttp2Pref() {
  Services.dns.clearCache(true);

  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setBoolPref("network.http.http3.enable", false);
  Services.prefs.setBoolPref("network.dns.echconfig.enabled", false);
  Services.prefs.setBoolPref("network.dns.http3_echconfig.enabled", false);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  await trrServer.registerDoHAnswers("test.foo.com", "HTTPS", {
    answers: [
      {
        name: "test.foo.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "test.foo_h3.com",
          values: [
            { key: "alpn", value: ["h3", "h2"] },
            { key: "echconfig", value: "456..." },
          ],
        },
      },
      {
        name: "test.foo.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 2,
          name: "test.foo_h2.com",
          values: [
            { key: "alpn", value: ["h2"] },
            { key: "echconfig", value: "456..." },
          ],
        },
      },
    ],
  });

  let { inRecord } = await new TRRDNSListener("test.foo.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

  checkResult(inRecord, false, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, false, true, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, true, false);
  checkResult(inRecord, true, true);

  Services.prefs.setBoolPref("network.http.http2.enabled", false);
  checkResult(inRecord, false, false);

  Services.prefs.setBoolPref("network.http.http3.enable", true);
  checkResult(inRecord, false, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, false, true);
  checkResult(inRecord, true, false, {
    expectedPriority: 1,
    expectedName: "test.foo_h3.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, true, true);

  await trrServer.stop();
  trrServer = null;
});
