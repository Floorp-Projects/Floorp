/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let h2Port;
let trrServer;

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

add_setup(async function setup() {
  h2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  trr_test_setup();

  Services.prefs.setBoolPref("network.dns.upgrade_with_https_rr", true);
  Services.prefs.setBoolPref("network.dns.use_https_rr_as_altsvc", true);

  registerCleanupFunction(async () => {
    trr_clear_prefs();
    Services.prefs.clearUserPref("network.dns.upgrade_with_https_rr");
    Services.prefs.clearUserPref("network.dns.use_https_rr_as_altsvc");
    Services.prefs.clearUserPref("network.dns.disablePrefetch");
    await trrServer.stop();
  });

  if (mozinfo.socketprocess_networking) {
    Services.dns; // Needed to trigger socket process.
    await TestUtils.waitForCondition(() => Services.io.socketProcessLaunched);
  }
});

// Test if IP hint addresses can be accessed as regular A/AAAA records.
add_task(async function testStoreIPHint() {
  trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  await trrServer.registerDoHAnswers("test.IPHint.com", "HTTPS", {
    answers: [
      {
        name: "test.IPHint.com",
        ttl: 999,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "test.IPHint.com",
          values: [
            { key: "alpn", value: ["h2", "h3"] },
            { key: "port", value: 8888 },
            { key: "ipv4hint", value: ["1.2.3.4", "5.6.7.8"] },
            { key: "ipv6hint", value: ["::1", "fe80::794f:6d2c:3d5e:7836"] },
          ],
        },
      },
    ],
  });

  let { inRecord } = await new TRRDNSListener("test.IPHint.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  Assert.equal(inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).ttl, 999);
  Assert.equal(answer[0].priority, 1);
  Assert.equal(answer[0].name, "test.IPHint.com");
  Assert.equal(answer[0].values.length, 4);
  Assert.deepEqual(
    answer[0].values[0].QueryInterface(Ci.nsISVCParamAlpn).alpn,
    ["h2", "h3"],
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[1].QueryInterface(Ci.nsISVCParamPort).port,
    8888,
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[2].QueryInterface(Ci.nsISVCParamIPv4Hint).ipv4Hint[0]
      .address,
    "1.2.3.4",
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[2].QueryInterface(Ci.nsISVCParamIPv4Hint).ipv4Hint[1]
      .address,
    "5.6.7.8",
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[3].QueryInterface(Ci.nsISVCParamIPv6Hint).ipv6Hint[0]
      .address,
    "::1",
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[3].QueryInterface(Ci.nsISVCParamIPv6Hint).ipv6Hint[1]
      .address,
    "fe80::794f:6d2c:3d5e:7836",
    "got correct answer"
  );

  async function verifyAnswer(domain, flags, expectedAddresses) {
    // eslint-disable-next-line no-shadow
    let { inRecord } = await new TRRDNSListener(domain, {
      flags,
      expectedSuccess: false,
    });
    Assert.ok(inRecord);
    inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
    let addresses = [];
    while (inRecord.hasMore()) {
      addresses.push(inRecord.getNextAddrAsString());
    }
    Assert.deepEqual(addresses, expectedAddresses);
    Assert.equal(inRecord.ttl, 999);
  }

  await verifyAnswer("test.IPHint.com", Ci.nsIDNSService.RESOLVE_IP_HINT, [
    "1.2.3.4",
    "5.6.7.8",
    "::1",
    "fe80::794f:6d2c:3d5e:7836",
  ]);

  await verifyAnswer(
    "test.IPHint.com",
    Ci.nsIDNSService.RESOLVE_IP_HINT | Ci.nsIDNSService.RESOLVE_DISABLE_IPV4,
    ["::1", "fe80::794f:6d2c:3d5e:7836"]
  );

  await verifyAnswer(
    "test.IPHint.com",
    Ci.nsIDNSService.RESOLVE_IP_HINT | Ci.nsIDNSService.RESOLVE_DISABLE_IPV6,
    ["1.2.3.4", "5.6.7.8"]
  );

  info("checking that IPv6 hints are ignored when disableIPv6 is true");
  Services.prefs.setBoolPref("network.dns.disableIPv6", true);
  await trrServer.registerDoHAnswers("testv6.IPHint.com", "HTTPS", {
    answers: [
      {
        name: "testv6.IPHint.com",
        ttl: 999,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "testv6.IPHint.com",
          values: [
            { key: "alpn", value: ["h2", "h3"] },
            { key: "port", value: 8888 },
            { key: "ipv4hint", value: ["1.2.3.4", "5.6.7.8"] },
            { key: "ipv6hint", value: ["::1", "fe80::794f:6d2c:3d5e:7836"] },
          ],
        },
      },
    ],
  });

  ({ inRecord } = await new TRRDNSListener("testv6.IPHint.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  }));
  Services.prefs.setBoolPref("network.dns.disableIPv6", false);

  await verifyAnswer("testv6.IPHint.com", Ci.nsIDNSService.RESOLVE_IP_HINT, [
    "1.2.3.4",
    "5.6.7.8",
  ]);

  await verifyAnswer(
    "testv6.IPHint.com",
    Ci.nsIDNSService.RESOLVE_IP_HINT | Ci.nsIDNSService.RESOLVE_DISABLE_IPV6,
    ["1.2.3.4", "5.6.7.8"]
  );

  await trrServer.stop();
});

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
  }).QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    let internal = chan.QueryInterface(Ci.nsIHttpChannelInternal);
    internal.setWaitForHTTPSSVCRecord();
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

// Test if we can connect to the server with the IP hint address.
add_task(async function testConnectionWithIPHint() {
  Services.dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    "https://127.0.0.1:" + h2Port + "/httpssvc_use_iphint"
  );

  // Resolving test.iphint.com should be failed.
  let { inStatus } = await new TRRDNSListener("test.iphint.com", {
    expectedSuccess: false,
  });
  Assert.equal(
    inStatus,
    Cr.NS_ERROR_UNKNOWN_HOST,
    "status is NS_ERROR_UNKNOWN_HOST"
  );

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  // The connection should be succeeded since the IP hint is 127.0.0.1.
  let chan = makeChan(`http://test.iphint.com:8080/server-timing`);
  // Note that the partitionKey stored in DNS cache would be
  // "%28https%2Ciphint.com%29". The http request to test.iphint.com will be
  // upgraded to https and the ip hint address will be used by the https
  // request in the end.
  let [req] = await channelOpenPromise(chan);
  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );

  await trrServer.stop();
});

// Test the case that we failed to use IP Hint address because DNS cache
// is bypassed.
add_task(async function testIPHintWithFreshDNS() {
  trrServer = new TRRServer();
  await trrServer.start();
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
  // To make sure NS_HTTP_REFRESH_DNS not be cleared.
  Services.prefs.setBoolPref("network.dns.disablePrefetch", true);

  await trrServer.registerDoHAnswers("test.iphint.org", "HTTPS", {
    answers: [
      {
        name: "test.iphint.org",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 0,
          name: "svc.iphint.net",
          values: [],
        },
      },
    ],
  });

  await trrServer.registerDoHAnswers("svc.iphint.net", "HTTPS", {
    answers: [
      {
        name: "svc.iphint.net",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "svc.iphint.net",
          values: [
            { key: "alpn", value: "h2" },
            { key: "port", value: h2Port },
            { key: "ipv4hint", value: "127.0.0.1" },
          ],
        },
      },
    ],
  });

  let { inRecord } = await new TRRDNSListener("test.iphint.org", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  Assert.equal(answer[0].priority, 1);
  Assert.equal(answer[0].name, "svc.iphint.net");

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  let chan = makeChan(`https://test.iphint.org/server-timing`);
  chan.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
  let [req] = await channelOpenPromise(
    chan,
    CL_EXPECT_FAILURE | CL_ALLOW_UNKNOWN_CL
  );
  // Failed because there no A record for "svc.iphint.net".
  Assert.equal(req.status, Cr.NS_ERROR_UNKNOWN_HOST);

  await trrServer.registerDoHAnswers("svc.iphint.net", "A", {
    answers: [
      {
        name: "svc.iphint.net",
        ttl: 55,
        type: "A",
        flush: false,
        data: "127.0.0.1",
      },
    ],
  });

  chan = makeChan(`https://test.iphint.org/server-timing`);
  chan.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
  [req] = await channelOpenPromise(chan);
  Assert.equal(req.protocolVersion, "h2");
  let internal = req.QueryInterface(Ci.nsIHttpChannelInternal);
  Assert.equal(internal.remotePort, h2Port);

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
  await trrServer.stop();
});
