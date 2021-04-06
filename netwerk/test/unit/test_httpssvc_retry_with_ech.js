/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

let prefs;
let h2Port;
let listen;
let trrServer;

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);
const threadManager = Cc["@mozilla.org/thread-manager;1"].getService(
  Ci.nsIThreadManager
);
const mainThread = threadManager.currentThread;

const defaultOriginAttributes = {};

function setup() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );
  h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  // Set to allow the cert presented by our H2 server
  do_get_profile();
  prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  prefs.setBoolPref("network.security.esni.enabled", false);
  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  // the TRR server is on 127.0.0.1
  prefs.setCharPref("network.trr.bootstrapAddress", "127.0.0.1");

  // make all native resolve calls "secretly" resolve localhost instead
  prefs.setBoolPref("network.dns.native-is-localhost", true);

  // 0 - off, 1 - race, 2 TRR first, 3 TRR only, 4 shadow
  prefs.setIntPref("network.trr.mode", 2); // TRR first
  prefs.setBoolPref("network.trr.wait-for-portal", false);
  // don't confirm that TRR is working, just go!
  prefs.setCharPref("network.trr.confirmationNS", "skip");

  // So we can change the pref without clearing the cache to check a pushed
  // record with a TRR path that fails.
  Services.prefs.setBoolPref("network.trr.clear-cache-on-pref-change", false);

  Services.prefs.setBoolPref("network.dns.upgrade_with_https_rr", true);
  Services.prefs.setBoolPref("network.dns.use_https_rr_as_altsvc", true);
  Services.prefs.setBoolPref("network.dns.echconfig.enabled", true);

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert.  // the foo.example.com domain name.
  const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  add_tls_server_setup(
    "EncryptedClientHelloServer",
    "../../../security/manager/ssl/tests/unit/test_encrypted_client_hello"
  );
}

setup();
registerCleanupFunction(async () => {
  prefs.clearUserPref("network.security.esni.enabled");
  prefs.clearUserPref("network.http.spdy.enabled");
  prefs.clearUserPref("network.http.spdy.enabled.http2");
  prefs.clearUserPref("network.dns.localDomains");
  prefs.clearUserPref("network.dns.native-is-localhost");
  prefs.clearUserPref("network.trr.mode");
  prefs.clearUserPref("network.trr.uri");
  prefs.clearUserPref("network.trr.credentials");
  prefs.clearUserPref("network.trr.wait-for-portal");
  prefs.clearUserPref("network.trr.allow-rfc1918");
  prefs.clearUserPref("network.trr.useGET");
  prefs.clearUserPref("network.trr.confirmationNS");
  prefs.clearUserPref("network.trr.bootstrapAddress");
  prefs.clearUserPref("network.trr.request-timeout");
  prefs.clearUserPref("network.trr.clear-cache-on-pref-change");
  prefs.clearUserPref("network.dns.upgrade_with_https_rr");
  prefs.clearUserPref("network.dns.use_https_rr_as_altsvc");
  prefs.clearUserPref("network.dns.echconfig.enabled");
  prefs.clearUserPref("network.dns.echconfig.fallback_to_origin");
  if (trrServer) {
    await trrServer.stop();
  }
});

class DNSListener {
  constructor() {
    this.promise = new Promise(resolve => {
      this.resolve = resolve;
    });
  }
  onLookupComplete(inRequest, inRecord, inStatus) {
    this.resolve([inRequest, inRecord, inStatus]);
  }
  // So we can await this as a promise.
  then() {
    return this.promise.then.apply(this.promise, arguments);
  }
}

DNSListener.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIDNSListener",
]);

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
    "AEr+CQBGABZlY2gtcHVibGljLmV4YW1wbGUuY29tACCKB1Y5SfrGIyk27W82xPpzWTDs3q72c04xSurDWlb9CgAgAAQAAQADADIAAA==";
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

  let listener = new DNSListener();

  let request = dns.asyncResolve(
    "ech-private.example.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, , inStatus] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");

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
    "AEr+CQBGABZlY2gtcHVibGljLmV4YW1wbGUuY29tACCKB1Y5SfrGIyk27W82xPpzWTDs3q72c04xSurDWlb9CgAgAAQAAQABADIAAA==";
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

  let listener = new DNSListener();

  let request = dns.asyncResolve(
    "ech-private.example.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, , inStatus] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");

  Services.prefs.setBoolPref("network.dns.echconfig.enabled", true);
  let chan = makeChan(`https://ech-private.example.com`);
  await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
  let securityInfo = chan.securityInfo.QueryInterface(
    Ci.nsITransportSecurityInfo
  );
  Assert.ok(securityInfo.isAcceptedEch, "This host should have accepted ECH");

  await trrServer.stop();
});
