/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

let h2Port;

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);
const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

add_setup(async function setup() {
  trr_test_setup();

  h2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  Services.prefs.setBoolPref("network.dns.upgrade_with_https_rr", true);
  Services.prefs.setBoolPref("network.dns.use_https_rr_as_altsvc", true);

  registerCleanupFunction(() => {
    trr_clear_prefs();
    Services.prefs.clearUserPref("network.dns.upgrade_with_https_rr");
    Services.prefs.clearUserPref("network.dns.use_https_rr_as_altsvc");
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

function channelOpenPromise(chan) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    chan.asyncOpen(new ChannelListener(finish, null, CL_ALLOW_UNKNOWN_CL));
  });
}

// This is for testing when the HTTPSSVC record is not available when
// the transaction is added in connection manager.
add_task(async function testUseHTTPSSVCForHttpsUpgrade() {
  // use the h2 server as DOH provider
  Services.prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/httpssvc_as_altsvc"
  );
  Services.dns.clearCache(true);

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  let chan = makeChan(`https://test.httpssvc.com:8080/`);
  let [req] = await channelOpenPromise(chan);
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
  // use the h2 server as DOH provider
  Services.prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/httpssvc_as_altsvc"
  );
  Services.dns.clearCache(true);

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  // At this time, the DataStorage is not ready, so MaybeUseHTTPSRRForUpgrade()
  // is called from the callback of NS_ShouldSecureUpgrade().
  let chan = makeChan(`http://test.httpssvc.com:80/`);
  let listener = new EventSinkListener();
  chan.notificationCallbacks = listener;

  let [req] = await channelOpenPromise(chan);

  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");

  // At this time, the DataStorage is ready, so MaybeUseHTTPSRRForUpgrade()
  // is called from nsHttpChannel::OnBeforeConnect().
  chan = makeChan(`http://test.httpssvc.com:80/`);
  listener = new EventSinkListener();
  chan.notificationCallbacks = listener;

  [req] = await channelOpenPromise(chan);

  req.QueryInterface(Ci.nsIHttpChannel);
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
});

// This is for testing when the HTTPSSVC record is already available before
// the transaction is added in connection manager.
add_task(async function testUseHTTPSSVC() {
  // use the h2 server as DOH provider
  Services.prefs.setCharPref(
    "network.trr.uri",
    "https://foo.example.com:" + h2Port + "/httpssvc_as_altsvc"
  );

  // Do DNS resolution before creating the channel, so the HTTPSSVC record will
  // be resolved from the cache.
  await new TRRDNSListener("test.httpssvc.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

  // We need to skip the security check, since our test cert is signed for
  // foo.example.com, not test.httpssvc.com.
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  let chan = makeChan(`https://test.httpssvc.com:8888`);
  let [req] = await channelOpenPromise(chan);
  // Test if this request is done by h2.
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
});

// Test if we can successfully fallback to the original host and port.
add_task(async function testFallback() {
  let trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();

  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  await trrServer.registerDoHAnswers("test.fallback.com", "A", {
    answers: [
      {
        name: "test.fallback.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "127.0.0.1",
      },
    ],
  });
  // Use a wrong port number 8888, so this connection will be refused.
  await trrServer.registerDoHAnswers("test.fallback.com", "HTTPS", {
    answers: [
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
    ],
  });

  let { inRecord } = await new TRRDNSListener("test.fallback.com", {
    type: Ci.nsIDNSService.RESOLVE_TYPE_HTTPSSVC,
  });

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
  let [req] = await channelOpenPromise(chan);
  // Test if this request is done by h2.
  Assert.equal(req.getResponseHeader("x-connection-http2"), "yes");

  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    false
  );
});
