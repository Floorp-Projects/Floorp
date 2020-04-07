/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

let prefs;
let h2Port;
let listen;

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

let test_answer = "bXkgdm9pY2UgaXMgbXkgcGFzc3dvcmQ=";
let test_answer_addr = "127.0.0.1";

class DNSListener {
  constructor() {
    this.promise = new Promise(resolve => {
      this.resolve = resolve;
    });
  }
  onLookupComplete(inRequest, inRecord, inStatus) {
    let txtRec;
    try {
      txtRec = inRecord.QueryInterface(Ci.nsIDNSByTypeRecord);
    } catch (e) {}
    if (txtRec) {
      this.resolve([inRequest, txtRec, inStatus, "onLookupByTypeComplete"]);
    } else {
      this.resolve([inRequest, inRecord, inStatus, "onLookupComplete"]);
    }
  }
  // So we can await this as a promise.
  then() {
    return this.promise.then.apply(this.promise, arguments);
  }
}

DNSListener.prototype.QueryInterface = ChromeUtils.generateQI([
  Ci.nsIDNSListener,
]);

add_task(async function testEsniRequest() {
  // use the h2 server as DOH provider
  prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/esni-dns"
  );

  let listenerEsni = new DNSListener();
  let request = dns.asyncResolveByType(
    "_esni.example.com",
    dns.RESOLVE_TYPE_TXT,
    0,
    listenerEsni,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus, inType] = await listenerEsni;
  Assert.equal(inType, "onLookupByTypeComplete", "check correct type");
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
  let answer = inRecord.getRecordsAsOneString();
  Assert.equal(answer, test_answer, "got correct answer");
});

// verify esni record pushed on a A record request
add_task(async function testEsniPushPart1() {
  prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/esni-dns-push"
  );
  let listenerAddr = new DNSListener();
  let request = dns.asyncResolve(
    "_esni_push.example.com",
    0,
    listenerAddr,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus, inType] = await listenerAddr;
  Assert.equal(inType, "onLookupComplete", "check correct type");
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
  let answer = inRecord.getNextAddrAsString();
  Assert.equal(answer, test_answer_addr, "got correct answer");
});

// verify the esni pushed record
add_task(async function testEsniPushPart2() {
  // At this point the second host name should've been pushed and we can resolve it using
  // cache only. Set back the URI to a path that fails.
  prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/404"
  );
  let listenerEsni = new DNSListener();
  let request = dns.asyncResolveByType(
    "_esni_push.example.com",
    dns.RESOLVE_TYPE_TXT,
    0,
    listenerEsni,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, inRecord, inStatus, inType] = await listenerEsni;
  Assert.equal(inType, "onLookupByTypeComplete", "check correct type");
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");
  let answer = inRecord.getRecordsAsOneString();
  Assert.equal(answer, test_answer, "got correct answer");
});
