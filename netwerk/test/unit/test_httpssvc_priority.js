/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

let prefs;
let h2Port;

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

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert.  // the foo.example.com domain name.
  const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
}

setup();
registerCleanupFunction(() => {
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
  prefs.clearUserPref("network.dns.echconfig.enabled");
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

add_task(async function testPriorityAndECHConfig() {
  let trrServer = new TRRServer();
  registerCleanupFunction(async () => trrServer.stop());
  await trrServer.start();

  Services.prefs.setBoolPref("network.dns.echconfig.enabled", false);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );

  await trrServer.registerDoHAnswers("test.priority.com", "HTTPS", [
    {
      name: "test.priority.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: "test.p1.com",
        values: [{ key: "alpn", value: "h2,h3" }],
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
  ]);

  let listener = new DNSListener();

  let request = dns.asyncResolve(
    "test.priority.com",
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
  dns.clearCache(true);
  listener = new DNSListener();

  request = dns.asyncResolve(
    "test.priority.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    listener,
    mainThread,
    defaultOriginAttributes
  );

  [inRequest, inRecord, inStatus] = await listener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");

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
