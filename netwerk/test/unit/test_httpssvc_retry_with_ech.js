/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

let trrServer;
let h3Port;
let h3EchConfig;

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

function checkSecurityInfo(chan, expectPrivateDNS, expectAcceptedECH) {
  let securityInfo = chan.securityInfo;
  Assert.equal(
    securityInfo.isAcceptedEch,
    expectAcceptedECH,
    "ECH Status == Expected Status"
  );
  Assert.equal(
    securityInfo.usedPrivateDNS,
    expectPrivateDNS,
    "Private DNS Status == Expected Status"
  );
}

add_setup(async function setup() {
  // Allow telemetry probes which may otherwise be disabled for some
  // applications (e.g. Thunderbird).
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  trr_test_setup();

  Services.prefs.setBoolPref("network.dns.upgrade_with_https_rr", true);
  Services.prefs.setBoolPref("network.dns.use_https_rr_as_altsvc", true);
  Services.prefs.setBoolPref("network.dns.echconfig.enabled", true);
  Services.prefs.setBoolPref("network.dns.http3_echconfig.enabled", true);
  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 0);
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);

  await asyncStartTLSTestServer(
    "EncryptedClientHelloServer",
    "../../../security/manager/ssl/tests/unit/test_encrypted_client_hello"
  );

  h3Port = Services.env.get("MOZHTTP3_PORT_ECH");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");

  h3EchConfig = Services.env.get("MOZHTTP3_ECH");
  Assert.notEqual(h3EchConfig, null);
  Assert.notEqual(h3EchConfig, "");
});

registerCleanupFunction(async () => {
  trr_clear_prefs();
  Services.prefs.clearUserPref("network.trr.mode");
  Services.prefs.clearUserPref("network.trr.uri");
  Services.prefs.clearUserPref("network.dns.upgrade_with_https_rr");
  Services.prefs.clearUserPref("network.dns.use_https_rr_as_altsvc");
  Services.prefs.clearUserPref("network.dns.echconfig.enabled");
  Services.prefs.clearUserPref("network.dns.http3_echconfig.enabled");
  Services.prefs.clearUserPref(
    "network.dns.echconfig.fallback_to_origin_when_all_failed"
  );
  Services.prefs.clearUserPref("network.http.speculative-parallel-limit");
  Services.prefs.clearUserPref("network.dns.port_prefixed_qname_https_rr");
  Services.prefs.clearUserPref("security.tls.ech.grease_http3");
  Services.prefs.clearUserPref("security.tls.ech.grease_probability");
  if (trrServer) {
    await trrServer.stop();
  }
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
      certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
        false
      );
      resolve([req, buffer]);
    }
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      true
    );
    let internal = chan.QueryInterface(Ci.nsIHttpChannelInternal);
    internal.setWaitForHTTPSSVCRecord();
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

function ActivityObserver() {}

ActivityObserver.prototype = {
  activites: [],
  observeConnectionActivity(
    aHost,
    aPort,
    aSSL,
    aHasECH,
    aIsHttp3,
    aActivityType,
    aActivitySubtype,
    aTimestamp,
    aExtraStringData
  ) {
    dump(
      "*** Connection Activity 0x" +
        aActivityType.toString(16) +
        " 0x" +
        aActivitySubtype.toString(16) +
        " " +
        aExtraStringData +
        "\n"
    );
    this.activites.push({ host: aHost, subType: aActivitySubtype });
  },
};

function checkHttpActivities(activites, expectECH) {
  let foundDNSAndSocket = false;
  let foundSettingECH = false;
  let foundConnectionCreated = false;
  for (let activity of activites) {
    switch (activity.subType) {
      case Ci.nsIHttpActivityObserver.ACTIVITY_SUBTYPE_DNSANDSOCKET_CREATED:
      case Ci.nsIHttpActivityObserver
        .ACTIVITY_SUBTYPE_SPECULATIVE_DNSANDSOCKET_CREATED:
        foundDNSAndSocket = true;
        break;
      case Ci.nsIHttpActivityDistributor.ACTIVITY_SUBTYPE_ECH_SET:
        foundSettingECH = true;
        break;
      case Ci.nsIHttpActivityDistributor.ACTIVITY_SUBTYPE_CONNECTION_CREATED:
        foundConnectionCreated = true;
        break;
      default:
        break;
    }
  }

  Assert.equal(foundDNSAndSocket, true, "Should have one DnsAndSock created");
  Assert.equal(foundSettingECH, expectECH, "Should have echConfig");
  Assert.equal(
    foundConnectionCreated,
    true,
    "Should have one connection created"
  );
}

add_task(async function testConnectWithECH() {
  const ECH_CONFIG_FIXED =
    "AEn+DQBFTQAgACCKB1Y5SfrGIyk27W82xPpzWTDs3q72c04xSurDWlb9CgAEAAEAA2QWZWNoLXB1YmxpYy5leGFtcGxlLmNvbQAA";
  trrServer = new TRRServer();
  await trrServer.start();

  let observerService = Cc[
    "@mozilla.org/network/http-activity-distributor;1"
  ].getService(Ci.nsIHttpActivityDistributor);
  let observer = new ActivityObserver();
  observerService.addObserver(observer);
  observerService.observeConnection = true;

  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  // Only the last record is valid to use.
  await trrServer.registerDoHAnswers("ech-private.example.com", "HTTPS", {
    answers: [
      {
        name: "ech-private.example.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "ech-private.example.com",
          values: [
            { key: "alpn", value: "http/1.1" },
            { key: "port", value: 8443 },
            {
              key: "echconfig",
              value: ECH_CONFIG_FIXED,
              needBase64Decode: true,
            },
          ],
        },
      },
    ],
  });

  await trrServer.registerDoHAnswers("ech-private.example.com", "A", {
    answers: [
      {
        name: "ech-private.example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "127.0.0.1",
      },
    ],
  });

  await new TRRDNSListener("ech-private.example.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

  HandshakeTelemetryHelpers.resetHistograms();
  let chan = makeChan(`https://ech-private.example.com`);
  await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  checkSecurityInfo(chan, true, true);
  // Only check telemetry if network process is disabled.
  if (!mozinfo.socketprocess_networking) {
    HandshakeTelemetryHelpers.checkSuccess(["", "_ECH", "_FIRST_TRY"]);
    HandshakeTelemetryHelpers.checkEmpty(["_CONSERVATIVE", "_ECH_GREASE"]);
  }

  await trrServer.stop();
  observerService.removeObserver(observer);
  observerService.observeConnection = false;

  let filtered = observer.activites.filter(
    activity => activity.host === "ech-private.example.com"
  );
  checkHttpActivities(filtered, true);
});

add_task(async function testEchRetry() {
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  Services.dns.clearCache(true);

  const ECH_CONFIG_TRUSTED_RETRY =
    "AEn+DQBFTQAgACCKB1Y5SfrGIyk27W82xPpzWTDs3q72c04xSurDWlb9CgAEAAMAA2QWZWNoLXB1YmxpYy5leGFtcGxlLmNvbQAA";
  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  // Only the last record is valid to use.
  await trrServer.registerDoHAnswers("ech-private.example.com", "HTTPS", {
    answers: [
      {
        name: "ech-private.example.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "ech-private.example.com",
          values: [
            { key: "alpn", value: "http/1.1" },
            { key: "port", value: 8443 },
            {
              key: "echconfig",
              value: ECH_CONFIG_TRUSTED_RETRY,
              needBase64Decode: true,
            },
          ],
        },
      },
    ],
  });

  await trrServer.registerDoHAnswers("ech-private.example.com", "A", {
    answers: [
      {
        name: "ech-private.example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "127.0.0.1",
      },
    ],
  });

  await new TRRDNSListener("ech-private.example.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

  Services.prefs.setBoolPref("network.dns.echconfig.enabled", true);

  HandshakeTelemetryHelpers.resetHistograms();
  let chan = makeChan(`https://ech-private.example.com`);
  await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  checkSecurityInfo(chan, true, true);
  // Only check telemetry if network process is disabled.
  if (!mozinfo.socketprocess_networking) {
    for (let hName of ["SSL_HANDSHAKE_RESULT", "SSL_HANDSHAKE_RESULT_ECH"]) {
      let h = Services.telemetry.getHistogramById(hName);
      HandshakeTelemetryHelpers.assertHistogramMap(
        h.snapshot(),
        new Map([
          ["0", 1],
          ["188", 1],
        ])
      );
    }
    HandshakeTelemetryHelpers.checkEntry(["_FIRST_TRY"], 188, 1);
    HandshakeTelemetryHelpers.checkEmpty(["_CONSERVATIVE", "_ECH_GREASE"]);
  }

  await trrServer.stop();
});

async function H3ECHTest(
  echConfig,
  expectedHistKey,
  expectedHistEntries,
  advertiseECH
) {
  Services.dns.clearCache(true);
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 1000));
  resetEchTelemetry();
  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
  Services.prefs.setBoolPref("network.dns.port_prefixed_qname_https_rr", true);

  let observerService = Cc[
    "@mozilla.org/network/http-activity-distributor;1"
  ].getService(Ci.nsIHttpActivityDistributor);
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  let observer = new ActivityObserver();
  observerService.addObserver(observer);
  observerService.observeConnection = true;
  // Clear activities for past connections
  observer.activites = [];

  let portPrefixedName = `_${h3Port}._https.public.example.com`;
  let vals = [
    { key: "alpn", value: "h3-29" },
    { key: "port", value: h3Port },
  ];
  if (advertiseECH) {
    vals.push({
      key: "echconfig",
      value: echConfig,
      needBase64Decode: true,
    });
  }
  // Only the last record is valid to use.

  await trrServer.registerDoHAnswers(portPrefixedName, "HTTPS", {
    answers: [
      {
        name: portPrefixedName,
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: ".",
          values: vals,
        },
      },
    ],
  });

  await trrServer.registerDoHAnswers("public.example.com", "A", {
    answers: [
      {
        name: "public.example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "127.0.0.1",
      },
    ],
  });

  await new TRRDNSListener("public.example.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
    port: h3Port,
  });

  let chan = makeChan(`https://public.example.com:${h3Port}`);
  let [req] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.protocolVersion, "h3-29");
  checkSecurityInfo(chan, true, advertiseECH);

  await trrServer.stop();

  observerService.removeObserver(observer);
  observerService.observeConnection = false;

  let filtered = observer.activites.filter(
    activity => activity.host === "public.example.com"
  );
  checkHttpActivities(filtered, advertiseECH);
  await checkEchTelemetry(expectedHistKey, expectedHistEntries);
}

function resetEchTelemetry() {
  Services.telemetry.getKeyedHistogramById("HTTP3_ECH_OUTCOME").clear();
}

async function checkEchTelemetry(histKey, histEntries) {
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  await new Promise(resolve => setTimeout(resolve, 1000));
  let values = Services.telemetry
    .getKeyedHistogramById("HTTP3_ECH_OUTCOME")
    .snapshot()[histKey];
  if (!mozinfo.socketprocess_networking) {
    HandshakeTelemetryHelpers.assertHistogramMap(values, histEntries);
  }
}

add_task(async function testH3WithNoEch() {
  Services.prefs.setBoolPref("security.tls.ech.grease_http3", false);
  Services.prefs.setIntPref("security.tls.ech.grease_probability", 0);
  await H3ECHTest(
    h3EchConfig,
    "NONE",
    new Map([
      ["0", 1],
      ["1", 0],
    ]),
    false
  );
});

add_task(async function testH3WithECH() {
  await H3ECHTest(
    h3EchConfig,
    "REAL",
    new Map([
      ["0", 1],
      ["1", 0],
    ]),
    true
  );
});

add_task(async function testH3WithGreaseEch() {
  Services.prefs.setBoolPref("security.tls.ech.grease_http3", true);
  Services.prefs.setIntPref("security.tls.ech.grease_probability", 100);
  await H3ECHTest(
    h3EchConfig,
    "GREASE",
    new Map([
      ["0", 1],
      ["1", 0],
    ]),
    false
  );
});

add_task(async function testH3WithECHRetry() {
  Services.dns.clearCache(true);
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
  // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
  await new Promise(resolve => setTimeout(resolve, 1000));

  function base64ToArray(base64) {
    var binary_string = atob(base64);
    var len = binary_string.length;
    var bytes = new Uint8Array(len);
    for (var i = 0; i < len; i++) {
      bytes[i] = binary_string.charCodeAt(i);
    }
    return bytes;
  }

  let decodedConfig = base64ToArray(h3EchConfig);
  decodedConfig[6] ^= 0x94;
  let encoded = btoa(String.fromCharCode.apply(null, decodedConfig));
  await H3ECHTest(
    encoded,
    "REAL",
    new Map([
      ["0", 1],
      ["1", 1],
    ]),
    true
  );
});
