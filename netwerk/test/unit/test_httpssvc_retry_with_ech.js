/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");
var { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

let trrServer;
let h3Port;
let h3EchConfig;

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

function setup() {
  trr_test_setup();
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);

  Services.prefs.setBoolPref("network.dns.upgrade_with_https_rr", true);
  Services.prefs.setBoolPref("network.dns.use_https_rr_as_altsvc", true);
  Services.prefs.setBoolPref("network.dns.echconfig.enabled", true);
  Services.prefs.setBoolPref("network.dns.http3_echconfig.enabled", true);

  add_tls_server_setup(
    "EncryptedClientHelloServer",
    "../../../security/manager/ssl/tests/unit/test_encrypted_client_hello"
  );

  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  h3Port = env.get("MOZHTTP3_PORT_ECH");
  Assert.notEqual(h3Port, null);
  Assert.notEqual(h3Port, "");

  h3EchConfig = env.get("MOZHTTP3_ECH");
  Assert.notEqual(h3EchConfig, null);
  Assert.notEqual(h3EchConfig, "");
}

setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
  Services.prefs.clearUserPref("network.dns.upgrade_with_https_rr");
  Services.prefs.clearUserPref("network.dns.use_https_rr_as_altsvc");
  Services.prefs.clearUserPref("network.dns.echconfig.enabled");
  Services.prefs.clearUserPref("network.dns.http3_echconfig.enabled");
  Services.prefs.clearUserPref("network.dns.echconfig.fallback_to_origin");
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

add_task(async function testConnectWithECH() {
  const ECH_CONFIG_FIXED =
    "AEv+CgBHTQAgACCKB1Y5SfrGIyk27W82xPpzWTDs3q72c04xSurDWlb9CgAEAAEAAwBkABZlY2gtcHVibGljLmV4YW1wbGUuY29tAAA=";
  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", 3);
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

  let chan = makeChan(`https://ech-private.example.com`);
  await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  let securityInfo = chan.securityInfo.QueryInterface(
    Ci.nsITransportSecurityInfo
  );
  Assert.ok(securityInfo.isAcceptedEch, "This host should have accepted ECH");

  await trrServer.stop();
});

add_task(async function testEchRetry() {
  dns.clearCache(true);

  const ECH_CONFIG_TRUSTED_RETRY =
    "AEv+CgBHTQAgACCKB1Y5SfrGIyk27W82xPpzWTDs3q72c04xSurDWlb9CgAEAAEAAQBkABZlY2gtcHVibGljLmV4YW1wbGUuY29tAAA=";
  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", 3);
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
  let chan = makeChan(`https://ech-private.example.com`);
  await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  let securityInfo = chan.securityInfo.QueryInterface(
    Ci.nsITransportSecurityInfo
  );
  Assert.ok(securityInfo.isAcceptedEch, "This host should have accepted ECH");

  await trrServer.stop();
});

async function H3ECHTest(echConfig) {
  trrServer = new TRRServer();
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );

  // Only the last record is valid to use.
  await trrServer.registerDoHAnswers("public.example.com", "HTTPS", {
    answers: [
      {
        name: "public.example.com",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "public.example.com",
          values: [
            { key: "alpn", value: "h3-29" },
            { key: "port", value: h3Port },
            {
              key: "echconfig",
              value: echConfig,
              needBase64Decode: true,
            },
          ],
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
  });

  let chan = makeChan(`https://public.example.com`);
  let [req] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.protocolVersion, "h3-29");
  let securityInfo = chan.securityInfo.QueryInterface(
    Ci.nsITransportSecurityInfo
  );
  Assert.ok(securityInfo.isAcceptedEch, "This host should have accepted ECH");

  await trrServer.stop();
}

add_task(async function testH3ConnectWithECH() {
  await H3ECHTest(h3EchConfig);
});

add_task(async function testH3ConnectWithECHRetry() {
  dns.clearCache(true);
  Services.obs.notifyObservers(null, "net:cancel-all-connections");
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
  await H3ECHTest(encoded);
});
