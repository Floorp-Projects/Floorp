/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

let prefs;
let h2Port;
let listen;

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

  // 0 - off, 1 - race, 2 TRR first, 3 TRR only, 4 shadow
  prefs.setIntPref("network.trr.mode", 3); // TRR first
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

  // This is for skiping the security check for the odoh host.
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );
}

setup();
registerCleanupFunction(() => {
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
  prefs.clearUserPref("network.trr.odoh.enabled");
  prefs.clearUserPref("network.trr.odoh.target_path");
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
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

add_task(async function testODoHConfig() {
  // use the h2 server as DOH provider
  prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/odohconfig"
  );

  let dnsListener = new DNSListener();
  let request = dns.asyncResolve(
    "odoh_host.example.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    dnsListener,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus] = await dnsListener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
  let answer = inRecord.QueryInterface(Ci.nsIDNSHTTPSSVCRecord).records;
  Assert.equal(answer[0].priority, 1);
  Assert.equal(answer[0].name, "odoh_host.example.com");
  Assert.equal(answer[0].values.length, 1);
  let odohconfig = answer[0].values[0].QueryInterface(Ci.nsISVCParamODoHConfig)
    .ODoHConfig;
  Assert.equal(odohconfig.length, 46);
});

add_task(async function testODoHQueryA() {
  dns.clearCache(true);

  prefs.setBoolPref("network.trr.odoh.enabled", true);
  prefs.setCharPref(
    "network.trr.odoh.target_host",
    `https://odoh_host.example.com:${h2Port}`
  );
  const expectedIP = "8.8.8.8";
  prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=${expectedIP}`
  );

  prefs.setCharPref("network.dns.localDomains", "odoh_host.example.com");

  let dnsListener = new DNSListener();
  let request = dns.asyncResolve(
    "bar.example.com",
    dns.RESOLVE_TYPE_DEFAULT,
    0,
    null, // resolverInfo
    dnsListener,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus] = await dnsListener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
  inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
  let answer = inRecord.getNextAddrAsString();
  Assert.equal(answer, expectedIP);
  Assert.ok(dns.ODoHActivated);
});

add_task(async function testODoHQueryAAAA() {
  dns.clearCache(true);

  const expectedIP = "2020:2020::2020";
  prefs.setCharPref(
    "network.trr.odoh.target_path",
    `odoh?responseIP=${expectedIP}`
  );

  let dnsListener = new DNSListener();
  let request = dns.asyncResolve(
    "aaaa.example.com",
    dns.RESOLVE_TYPE_DEFAULT,
    0,
    null, // resolverInfo
    dnsListener,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus] = await dnsListener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
  inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
  let answer = inRecord.getNextAddrAsString();
  Assert.equal(answer, expectedIP);
  Assert.ok(dns.ODoHActivated);
});
