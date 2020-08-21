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
  prefs.clearUserPref("network.dns.upgrade_with_https_rr");
  prefs.clearUserPref("network.dns.use_https_rr_as_altsvc");
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
  }).QueryInterface(Ci.nsIHttpChannel);
  return chan;
}

function channelOpenPromise(chan) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    let internal = chan.QueryInterface(Ci.nsIHttpChannelInternal);
    internal.setWaitForHTTPSSVCRecord();
    chan.asyncOpen(new ChannelListener(finish, null, CL_ALLOW_UNKNOWN_CL));
  });
}

// This is for testing when the HTTPSSVC record is already available before
// the transaction is added in connection manager.
add_task(async function testUseHTTPSSVC() {
  // use the h2 server as DOH provider
  prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/httpssvc_as_altsvc"
  );

  let listener = new DNSListener();

  // Do DNS resolution before creating the channel, so the HTTPSSVC record will
  // be resolved from the cache.
  let request = dns.asyncResolve(
    "test.httpssvc.com",
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

  // We need to skip the security check, since our test cert is signed for
  // foo.example.com, not test.httpssvc.com.
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  let chan = makeChan(`https://test.httpssvc.com:8888`);
  let [req, resp] = await channelOpenPromise(chan);
  // Test if this request is done by h2.
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
});

// This is for testing when the HTTPSSVC record is not available when
// the transaction is added in connection manager.
add_task(async function testUseHTTPSSVC1() {
  dns.clearCache(true);

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  let chan = makeChan(`https://test.httpssvc.com:8080/`);
  let [req, resp] = await channelOpenPromise(chan);
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
});

class EventSinkListener {
  getInterface(iid) {
    if (iid.equals(Ci.nsIChannelEventSink)) {
      return this;
    }
  }
  asyncOnChannelRedirect(oldChan, newChan, flags, callback) {
    Assert.equal(oldChan.URI.hostPort, newChan.URI.hostPort);
    Assert.equal(oldChan.URI.scheme, "http");
    Assert.equal(newChan.URI.scheme, "https");
    callback.onRedirectVerifyCallback(Cr.NS_OK);
  }
}

EventSinkListener.prototype.QueryInterface = ChromeUtils.generateQI([
  "nsIInterfaceRequestor",
  "nsIChannelEventSink",
]);

// Test if the request is upgraded to https with a HTTPSSVC record.
add_task(async function testUseHTTPSSVCAsHSTS() {
  dns.clearCache(true);

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  let chan = makeChan(`http://test.httpssvc.com:80/`);
  let listener = new EventSinkListener();
  chan.notificationCallbacks = listener;

  let [req, resp] = await channelOpenPromise(chan);

  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
});

// Test if we can successfully fallback to the original host and port.
add_task(async function testFallback() {
  let trrServer = new TRRServer();
  registerCleanupFunction(async () => trrServer.stop());
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );

  await trrServer.registerDoHAnswers("test.fallback.com", "A", [
    {
      name: "test.fallback.com",
      ttl: 55,
      type: "A",
      flush: false,
      data: "127.0.0.1",
    },
  ]);
  // Use a wrong port number 8888, so this connection will be refused.
  await trrServer.registerDoHAnswers("test.fallback.com", "HTTPS", [
    {
      name: "test.fallback.com",
      ttl: 55,
      type: "HTTPS",
      flush: false,
      data: {
        priority: 1,
        name: "foo.example.com",
        values: [{ key: "port", value: 8888 }],
      },
    },
  ]);

  let listener = new DNSListener();

  let request = dns.asyncResolve(
    "test.fallback.com",
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

  let record = inRecord
    .QueryInterface(Ci.nsIDNSHTTPSSVCRecord)
    .GetServiceModeRecord(false, false);
  Assert.equal(record.priority, 1);
  Assert.equal(record.name, "foo.example.com");

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  // When the connection with port 8888 failed, the correct h2Port will be used
  // to connect again.
  let chan = makeChan(`https://test.fallback.com:${h2Port}`);
  let [req, resp] = await channelOpenPromise(chan);
  // Test if this request is done by h2.
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
});
