/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

let trrServer;

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

const env = Cc["@mozilla.org/process/environment;1"].getService(
  Ci.nsIEnvironment
);

function setup() {
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
  Services.prefs.setBoolPref("network.dns.http3_echconfig.enabled", false);
  Services.prefs.setIntPref("network.http.speculative-parallel-limit", 0);
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);

  // Set the server to always select http/1.1
  env.set("MOZ_TLS_ECH_ALPN_FLAG", 1);

  add_tls_server_setup(
    "EncryptedClientHelloServer",
    "../../../security/manager/ssl/tests/unit/test_encrypted_client_hello"
  );
}

setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
  Services.prefs.clearUserPref("network.trr.mode");
  Services.prefs.clearUserPref("network.trr.uri");
  Services.prefs.clearUserPref("network.dns.upgrade_with_https_rr");
  Services.prefs.clearUserPref("network.dns.use_https_rr_as_altsvc");
  Services.prefs.clearUserPref("network.dns.echconfig.enabled");
  Services.prefs.clearUserPref("network.dns.http3_echconfig.enabled");
  Services.prefs.clearUserPref("network.dns.echconfig.fallback_to_origin");
  Services.prefs.clearUserPref("network.http.speculative-parallel-limit");
  Services.prefs.clearUserPref("network.dns.port_prefixed_qname_https_rr");
  env.set("MOZ_TLS_ECH_ALPN_FLAG", "");
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

function checkHttpActivities(activites) {
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
  Assert.equal(foundSettingECH, true, "Should have echConfig");
  Assert.equal(
    foundConnectionCreated,
    true,
    "Should have one connection created"
  );
}

async function testWrapper(alpnAdvertisement) {
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
    `https://foo.example.com:${trrServer.port}/dns-query`
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
            { key: "alpn", value: alpnAdvertisement },
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
  let securityInfo = chan.securityInfo.QueryInterface(
    Ci.nsITransportSecurityInfo
  );
  Assert.ok(securityInfo.isAcceptedEch, "This host should have accepted ECH");

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
  checkHttpActivities(filtered);
}

add_task(async function h1Advertised() {
  await testWrapper(["http/1.1"]);
});

add_task(async function h2Advertised() {
  await testWrapper(["h2"]);
});

add_task(async function h3Advertised() {
  await testWrapper(["h3"]);
});

add_task(async function h1h2Advertised() {
  await testWrapper(["http/1.1", "h2"]);
});

add_task(async function h2h3Advertised() {
  await testWrapper(["h3", "h2"]);
});

add_task(async function unknownAdvertised() {
  await testWrapper(["foo"]);
});
