/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);
const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

const ReferrerInfo = Components.Constructor(
  "@mozilla.org/referrer-info;1",
  "nsIReferrerInfo",
  "init"
);

let h2Port;

add_setup(async function setup() {
  trr_test_setup();

  h2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  Services.prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/httpssvc_as_altsvc"
  );

  Services.prefs.setBoolPref("network.dns.upgrade_with_https_rr", true);
  Services.prefs.setBoolPref("network.dns.use_https_rr_as_altsvc", true);

  Services.prefs.setBoolPref(
    "network.dns.use_https_rr_for_speculative_connection",
    true
  );

  registerCleanupFunction(async () => {
    trr_clear_prefs();
    Services.prefs.clearUserPref("network.dns.upgrade_with_https_rr");
    Services.prefs.clearUserPref("network.dns.use_https_rr_as_altsvc");
    Services.prefs.clearUserPref(
      "network.dns.use_https_rr_for_speculative_connection"
    );
    Services.prefs.clearUserPref("network.dns.notifyResolution");
    Services.prefs.clearUserPref("network.dns.disablePrefetch");
  });

  if (mozinfo.socketprocess_networking) {
    Services.dns; // Needed to trigger socket process.
    await TestUtils.waitForCondition(() => Services.io.socketProcessLaunched);
  }

  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);
});

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
    contentPolicyType: Ci.nsIContentPolicy.TYPE_DOCUMENT,
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
    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
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
  Services.dns.clearCache(true);
  // Do DNS resolution before creating the channel, so the HTTPSSVC record will
  // be resolved from the cache.
  await new TRRDNSListener("test.httpssvc.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

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
  Services.dns.clearCache(true);

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
  Services.dns.clearCache(true);

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
    observe(aSubject, aTopic) {
      if (aTopic == topic) {
        Services.obs.removeObserver(observer, topic);
        let channel = aSubject.QueryInterface(Ci.nsIChannel);
        channel.suspend();

        new TRRDNSListener("foo.notexisted.com", {
          type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
          expectedSuccess: false,
        }).then(() => channel.resume());
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

// Test the case that an HTTPS RR is available and the server returns a 307
// for redirecting back to http.
add_task(async function testEndlessUpgradeDowngrade() {
  Services.dns.clearCache(true);

  let httpserv = new HttpServer();
  let content = "okok";
  httpserv.start(-1);
  let port = httpserv.identity.primaryPort;
  httpserv.registerPathHandler(
    `/redirect_to_http`,
    function handler(metadata, response) {
      response.setHeader("Content-Length", `${content.length}`);
      response.bodyOutputStream.write(content, content.length);
    }
  );
  httpserv.identity.setPrimary("http", "test.httpsrr.redirect.com", port);

  let chan = makeChan(
    `http://test.httpsrr.redirect.com:${port}/redirect_to_http?port=${port}`
  );

  let [, response] = await channelOpenPromise(chan);
  Assert.equal(response, content);
  await new Promise(resolve => httpserv.stop(resolve));
});

add_task(async function testHttpRequestBlocked() {
  Services.dns.clearCache(true);

  let dnsRequestObserver = {
    register() {
      this.obs = Services.obs;
      this.obs.addObserver(this, "dns-resolution-request");
    },
    unregister() {
      if (this.obs) {
        this.obs.removeObserver(this, "dns-resolution-request");
      }
    },
    observe(subject, topic) {
      if (topic == "dns-resolution-request") {
        Assert.ok(false, "unreachable");
      }
    },
  };

  dnsRequestObserver.register();
  Services.prefs.setBoolPref("network.dns.notifyResolution", true);
  Services.prefs.setBoolPref("network.dns.disablePrefetch", true);

  let httpserv = new HttpServer();
  httpserv.registerPathHandler("/", function handler() {
    Assert.ok(false, "unreachable");
  });
  httpserv.start(-1);
  httpserv.identity.setPrimary(
    "http",
    "foo.blocked.com",
    httpserv.identity.primaryPort
  );

  let chan = makeChan(
    `http://foo.blocked.com:${httpserv.identity.primaryPort}/`
  );

  let topic = "http-on-modify-request";
  let observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(aSubject, aTopic) {
      if (aTopic == topic) {
        Services.obs.removeObserver(observer, topic);
        let channel = aSubject.QueryInterface(Ci.nsIChannel);
        channel.cancel(Cr.NS_BINDING_ABORTED);
      }
    },
  };

  let [request] = await channelOpenPromise(chan, CL_EXPECT_FAILURE, observer);
  request.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(request.status, Cr.NS_BINDING_ABORTED);
  dnsRequestObserver.unregister();
  await new Promise(resolve => httpserv.stop(resolve));
});

function createPrincipal(url) {
  return Services.scriptSecurityManager.createContentPrincipal(
    Services.io.newURI(url),
    {}
  );
}

// Test if the Origin header stays the same after an internal HTTPS upgrade
// caused by HTTPS RR.
add_task(async function testHTTPSRRUpgradeWithOriginHeader() {
  Services.dns.clearCache(true);

  const url = "http://test.httpssvc.com:80/origin_header";
  const originURL = "http://example.com";
  let chan = Services.io
    .newChannelFromURIWithProxyFlags(
      Services.io.newURI(url),
      null,
      Ci.nsIProtocolProxyService.RESOLVE_ALWAYS_TUNNEL,
      null,
      createPrincipal(originURL),
      createPrincipal(url),
      Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
      Ci.nsIContentPolicy.TYPE_DOCUMENT
    )
    .QueryInterface(Ci.nsIHttpChannel);
  chan.referrerInfo = new ReferrerInfo(
    Ci.nsIReferrerInfo.EMPTY,
    true,
    NetUtil.newURI(url)
  );
  chan.setRequestHeader("Origin", originURL, false);

  let [req, buf] = await channelOpenPromise(chan);

  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");
  Assert.equal(buf, originURL);
});

// See bug 1899841. Test the case when network.dns.use_https_rr_as_altsvc
// is disabled.
add_task(async function testPrefDisabled() {
  Services.prefs.setBoolPref("network.dns.use_https_rr_as_altsvc", false);

  let chan = makeChan(`https://test.httpssvc.com:${h2Port}/server-timing`);
  let [req] = await channelOpenPromise(chan);

  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");
});
