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
const { HttpServer } = ChromeUtils.import("resource://testing-common/httpd.js");
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
  prefs.setIntPref("network.trr.mode", 2); // TRR first
  prefs.setBoolPref("network.trr.wait-for-portal", false);
  // don't confirm that TRR is working, just go!
  prefs.setCharPref("network.trr.confirmationNS", "skip");

  // So we can change the pref without clearing the cache to check a pushed
  // record with a TRR path that fails.
  Services.prefs.setBoolPref("network.trr.clear-cache-on-pref-change", false);

  Services.prefs.setBoolPref("network.dns.upgrade_with_https_rr", true);
  Services.prefs.setBoolPref("network.dns.use_https_rr_as_altsvc", true);

  Services.prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/httpssvc_as_altsvc"
  );

  // The moz-http2 cert is for foo.example.com and is signed by http2-ca.pem
  // so add that cert to the trust list as a signing cert.  // the foo.example.com domain name.
  const certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
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

// When observer is specified, the channel will be suspended when receiving
// "http-on-modify-request".
function channelOpenPromise(chan, flags, observer) {
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

    if (observer) {
      let topic = "http-on-modify-request";
      Services.obs.addObserver(observer, topic);
    }
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

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

  let dnsListener = new DNSListener();

  // Do DNS resolution before creating the channel, so the HTTPSSVC record will
  // be resolved from the cache.
  let request = dns.asyncResolve(
    "test.httpssvc.com",
    dns.RESOLVE_TYPE_HTTPSSVC,
    0,
    null, // resolverInfo
    dnsListener,
    mainThread,
    defaultOriginAttributes
  );

  let [inRequest, , inStatus] = await dnsListener;
  Assert.equal(inRequest, request, "correct request was used");
  Assert.equal(inStatus, Cr.NS_OK, "status OK");

  // Since the HTTPS RR should be served from cache, the DNS record is available
  // before nsHttpChannel::MaybeUseHTTPSRRForUpgrade() is called.
  let chan = makeChan(`http://test.httpssvc.com:80/server-timing`);
  let listener = new EventSinkListener();
  chan.notificationCallbacks = listener;

  let [req] = await channelOpenPromise(chan);

  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");

  chan = makeChan(`http://test.httpssvc.com:80/server-timing`);
  listener = new EventSinkListener();
  chan.notificationCallbacks = listener;

  [req] = await channelOpenPromise(chan);

  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");
});

// Test the case that we got an invalid DNS response. In this case,
// nsHttpChannel::OnHTTPSRRAvailable is called after
// nsHttpChannel::MaybeUseHTTPSRRForUpgrade.
add_task(async function testInvalidDNSResult() {
  dns.clearCache(true);

  let httpserv = new HttpServer();
  let content = "ok";
  httpserv.registerPathHandler("/", function handler(metadata, response) {
    response.setHeader("Content-Length", `${content.length}`);
    response.bodyOutputStream.write(content, content.length);
  });
  httpserv.start(-1);
  httpserv.identity.setPrimary(
    "http",
    "foo.notexisted.com",
    httpserv.identity.primaryPort
  );

  let chan = makeChan(
    `http://foo.notexisted.com:${httpserv.identity.primaryPort}/`
  );
  let [, response] = await channelOpenPromise(chan);
  Assert.equal(response, content);
  await new Promise(resolve => httpserv.stop(resolve));
});

// The same test as above, but nsHttpChannel::MaybeUseHTTPSRRForUpgrade is
// called after nsHttpChannel::OnHTTPSRRAvailable.
add_task(async function testInvalidDNSResult1() {
  dns.clearCache(true);

  let httpserv = new HttpServer();
  let content = "ok";
  httpserv.registerPathHandler("/", function handler(metadata, response) {
    response.setHeader("Content-Length", `${content.length}`);
    response.bodyOutputStream.write(content, content.length);
  });
  httpserv.start(-1);
  httpserv.identity.setPrimary(
    "http",
    "foo.notexisted.com",
    httpserv.identity.primaryPort
  );

  let chan = makeChan(
    `http://foo.notexisted.com:${httpserv.identity.primaryPort}/`
  );

  let topic = "http-on-modify-request";
  let observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(aSubject, aTopic, aData) {
      if (aTopic == topic) {
        Services.obs.removeObserver(observer, topic);
        let channel = aSubject.QueryInterface(Ci.nsIChannel);
        channel.suspend();
        let dnsListener = {
          QueryInterface: ChromeUtils.generateQI(["nsIDNSListener"]),
          onLookupComplete(inRequest, inRecord, inStatus) {
            channel.resume();
          },
        };
        dns.asyncResolve(
          "foo.notexisted.com",
          dns.RESOLVE_TYPE_HTTPSSVC,
          0,
          null, // resolverInfo
          dnsListener,
          mainThread,
          defaultOriginAttributes
        );
      }
    },
  };

  let [, response] = await channelOpenPromise(chan, 0, observer);
  Assert.equal(response, content);
  await new Promise(resolve => httpserv.stop(resolve));
});

add_task(async function testLiteralIP() {
  let httpserv = new HttpServer();
  let content = "ok";
  httpserv.registerPathHandler("/", function handler(metadata, response) {
    response.setHeader("Content-Length", `${content.length}`);
    response.bodyOutputStream.write(content, content.length);
  });
  httpserv.start(-1);

  let chan = makeChan(`http://127.0.0.1:${httpserv.identity.primaryPort}/`);
  let [, response] = await channelOpenPromise(chan);
  Assert.equal(response, content);
  await new Promise(resolve => httpserv.stop(resolve));
});
