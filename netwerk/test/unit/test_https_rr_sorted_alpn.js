/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

let trrServer;

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);

function setup() {
  trr_test_setup();
}

setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
  Services.prefs.clearUserPref("network.http.http3.support_version1");
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

add_task(async function testSortedAlpnH3() {
  dns.clearCache(true);

  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );
  Services.prefs.setBoolPref("network.http.http3.support_version1", true);
  await trrServer.registerDoHAnswers("test.alpn.com", "HTTPS", {
    answers: [
      {
        name: "test.alpn.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "test.alpn.com",
          values: [{ key: "alpn", value: ["h2", "http/1.1", "h3-30", "h3"] }],
        },
      },
    ],
  });

  let { inRecord } = await new TRRDNSListener("test.alpn.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
  });

  checkResult(inRecord, false, false, {
    expectedPriority: 1,
    expectedName: "test.alpn.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, false, true, {
    expectedPriority: 1,
    expectedName: "test.alpn.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, true, false, {
    expectedPriority: 1,
    expectedName: "test.alpn.com",
    expectedAlpn: "h3",
  });
  checkResult(inRecord, true, true, {
    expectedPriority: 1,
    expectedName: "test.alpn.com",
    expectedAlpn: "http/1.1",
  });

  Services.prefs.setBoolPref("network.http.http3.support_version1", false);
  checkResult(inRecord, false, false, {
    expectedPriority: 1,
    expectedName: "test.alpn.com",
    expectedAlpn: "h3-30",
  });
  checkResult(inRecord, false, true, {
    expectedPriority: 1,
    expectedName: "test.alpn.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, true, false, {
    expectedPriority: 1,
    expectedName: "test.alpn.com",
    expectedAlpn: "h3-30",
  });
  checkResult(inRecord, true, true, {
    expectedPriority: 1,
    expectedName: "test.alpn.com",
    expectedAlpn: "http/1.1",
  });
});

add_task(async function testSortedAlpnH2() {
  dns.clearCache(true);

  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );
  await trrServer.registerDoHAnswers("test.alpn_2.com", "HTTPS", {
    answers: [
      {
        name: "test.alpn_2.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "test.alpn_2.com",
          values: [{ key: "alpn", value: ["http/1.1", "h2"] }],
        },
      },
    ],
  });

  let { inRecord } = await new TRRDNSListener("test.alpn_2.com", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
  });

  checkResult(inRecord, false, false, {
    expectedPriority: 1,
    expectedName: "test.alpn_2.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, false, true, {
    expectedPriority: 1,
    expectedName: "test.alpn_2.com",
    expectedAlpn: "h2",
  });
  checkResult(inRecord, true, false, {
    expectedPriority: 1,
    expectedName: "test.alpn_2.com",
    expectedAlpn: "http/1.1",
  });
  checkResult(inRecord, true, true, {
    expectedPriority: 1,
    expectedName: "test.alpn_2.com",
    expectedAlpn: "http/1.1",
  });

  await trrServer.stop();
  trrServer = null;
});
