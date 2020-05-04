/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

let prefs;
let h2Port;
let listen;

function inChildProcess() {
  return (
    Cc["@mozilla.org/xre/app-info;1"].getService(Ci.nsIXULRuntime)
      .processType != Ci.nsIXULRuntime.PROCESS_TYPE_DEFAULT
  );
}

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
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

if (!inChildProcess()) {
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
    prefs.clearUserPref("network.trr.blacklist-duration");
    prefs.clearUserPref("network.trr.request-timeout");
    prefs.clearUserPref("network.trr.clear-cache-on-pref-change");
  });
}

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
  Ci.nsIDNSListener,
]);

add_task(async function testHTTPSSVC() {
  // use the h2 server as DOH provider
  if (!inChildProcess()) {
    prefs.setCharPref(
      "network.trr.uri",
      "https://foo.example.com:" + h2Port + "/httpssvc"
    );
  }

  let listenerEsni = new DNSListener();
  let request = dns.asyncResolveByType(
    "test.httpssvc.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    listenerEsni,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus] = await listenerEsni;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  Assert.equal(answer[0].priority, 1);
  Assert.equal(answer[0].name, "h3pool");
  Assert.equal(answer[0].values.length, 6);
  Assert.equal(
    answer[0].values[0].QueryInterface(Ci.nsISVCParamAlpn).alpn,
    "h2,h3",
    "got correct answer"
  );
  Assert.ok(
    answer[0].values[1].QueryInterface(Ci.nsISVCParamNoDefaultAlpn),
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[2].QueryInterface(Ci.nsISVCParamPort).port,
    8888,
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[3].QueryInterface(Ci.nsISVCParamIPv4Hint).ipv4Hint[0]
      .address,
    "1.2.3.4",
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[4].QueryInterface(Ci.nsISVCParamEsniConfig).esniConfig,
    "123...",
    "got correct answer"
  );
  Assert.equal(
    answer[0].values[5].QueryInterface(Ci.nsISVCParamIPv6Hint).ipv6Hint[0]
      .address,
    "::1",
    "got correct answer"
  );
  Assert.equal(answer[1].priority, 2);
  Assert.equal(answer[1].name, "");
  Assert.equal(answer[1].values.length, 4);
  Assert.equal(
    answer[1].values[0].QueryInterface(Ci.nsISVCParamAlpn).alpn,
    "h2",
    "got correct answer"
  );
  Assert.equal(
    answer[1].values[1].QueryInterface(Ci.nsISVCParamIPv4Hint).ipv4Hint[0]
      .address,
    "1.2.3.4",
    "got correct answer"
  );
  Assert.equal(
    answer[1].values[1].QueryInterface(Ci.nsISVCParamIPv4Hint).ipv4Hint[1]
      .address,
    "5.6.7.8",
    "got correct answer"
  );
  Assert.equal(
    answer[1].values[2].QueryInterface(Ci.nsISVCParamEsniConfig).esniConfig,
    "abc...",
    "got correct answer"
  );
  Assert.equal(
    answer[1].values[3].QueryInterface(Ci.nsISVCParamIPv6Hint).ipv6Hint[0]
      .address,
    "::1",
    "got correct answer"
  );
  Assert.equal(
    answer[1].values[3].QueryInterface(Ci.nsISVCParamIPv6Hint).ipv6Hint[1]
      .address,
    "fe80::794f:6d2c:3d5e:7836",
    "got correct answer"
  );
  Assert.equal(answer[2].priority, 3);
  Assert.equal(answer[2].name, "hello");
  Assert.equal(answer[2].values.length, 0);
});
