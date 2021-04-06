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

  prefs.setBoolPref("network.http.spdy.enabled", true);
  prefs.setBoolPref("network.http.spdy.enabled.http2", true);
  // the TRR server is on 127.0.0.1
  prefs.setCharPref("network.trr.bootstrapAddress", "127.0.0.1");

  // make all native resolve calls "secretly" resolve localhost instead
  prefs.setBoolPref("network.dns.native-is-localhost", true);

  prefs.setBoolPref("network.trr.wait-for-portal", false);
  // don't confirm that TRR is working, just go!
  prefs.setCharPref("network.trr.confirmationNS", "skip");

  // So we can change the pref without clearing the cache to check a pushed
  // record with a TRR path that fails.
  Services.prefs.setBoolPref("network.trr.clear-cache-on-pref-change", false);

  Services.prefs.setBoolPref("network.dns.upgrade_with_https_rr", true);
  Services.prefs.setBoolPref("network.dns.use_https_rr_as_altsvc", true);

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert.  // the foo.example.com domain name.
  const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
}

setup();
registerCleanupFunction(async () => {
  prefs.clearUserPref("network.trr.mode");
  prefs.clearUserPref("network.http.spdy.enabled");
  prefs.clearUserPref("network.http.spdy.enabled.http2");
  prefs.clearUserPref("network.dns.localDomains");
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
  prefs.clearUserPref("network.dns.native-is-localhost");
  prefs.clearUserPref("network.dns.disablePrefetch");
  await trrServer.stop();
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
    `https://foo.example.com:${trrServer.port}/dns-query`
  );

  await trrServer.registerDoHAnswers("test.IPHint.com", "HTTPS", {
    answers: [
      {
        name: "test.IPHint.com",
        ttl: 55,
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

  let listener = new DNSListener();

  let request = dns.asyncResolve(
    "test.IPHint.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");

  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
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

  async function verifyAnswer(flags, answer) {
    let listener = new DNSListener();
    let request = dns.asyncResolve(
      "test.IPHint.com",
      dns.RESOLVE_TYPE_DEFAULT,
      flags,
      null, // resolverInfo
      listener,
      mainThread,
      defaultOriginAttributes
    );

    let [inRequest, inRecord, inStatus] = await listener;
    Assert.equal(inRequest, request, "correct request was used");
    Assert.equal(inStatus, Cr.NS_OK, "status OK");
    inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
    let addresses = [];
    while (inRecord.hasMore()) {
      addresses.push(inRecord.getNextAddrAsString());
    }
    Assert.deepEqual(addresses, answer);
  }

  await verifyAnswer(Ci.nsIDNSService.RESOLVE_IP_HINT, [
    "1.2.3.4",
    "5.6.7.8",
    "::1",
    "fe80::794f:6d2c:3d5e:7836",
  ]);

  await verifyAnswer(
    Ci.nsIDNSService.RESOLVE_IP_HINT | Ci.nsIDNSService.RESOLVE_DISABLE_IPV4,
    ["::1", "fe80::794f:6d2c:3d5e:7836"]
  );

  await verifyAnswer(
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
  dns.clearCache(true);
  prefs.setIntPref("network.trr.mode", 3);
  prefs.setCharPref(
    "network.trr.uri",
    "https://127.0.0.1:" + h2Port + "/httpssvc_use_iphint"
  );

  // Resolving test.iphint.com should be failed.
  let listener = new DNSListener();
  let request = dns.asyncResolve(
    "test.iphint.com",
    dns.RESOLVE_TYPE_DEFAULT,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, , inStatus] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
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
    `https://foo.example.com:${trrServer.port}/dns-query`
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

  let listener = new DNSListener();
  let request = dns.asyncResolve(
    "test.iphint.org",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
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
