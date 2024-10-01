/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

const CONFIRM_OK = 2;
const CONFIRM_DISABLED = 5;

function WaitHTTPSRR(input) {
  const NS_HTTP_FORCE_WAIT_HTTP_RR = 1 << 22;
  return (input & NS_HTTP_FORCE_WAIT_HTTP_RR) !== 0;
}

async function waitForConfirmationState(state, msToWait = 0) {
  await TestUtils.waitForCondition(
    () => Services.dns.currentTrrConfirmationState == state,
    `Timed out waiting for ${state}. Currently ${Services.dns.currentTrrConfirmationState}`,
    1,
    msToWait
  );
  equal(
    Services.dns.currentTrrConfirmationState,
    state,
    "expected confirmation state"
  );
}

let h2Port;

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
    chan.asyncOpen(new ChannelListener(finish, null, flags));
  });
}

add_setup(async function setup() {
  h2Port = Services.env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  trr_test_setup();
  registerCleanupFunction(async () => {
    trr_clear_prefs();
  });
});

function ActivityObserver() {}

ActivityObserver.prototype = {
  caps: 0,
  observeActivity(aChannel, aType, aSubtype) {
    try {
      aChannel.QueryInterface(Ci.nsIChannel);
      if (
        aChannel.URI.spec ===
          `https://foo.example.com:${h2Port}/server-timing` &&
        aType === Ci.nsIHttpActivityObserver.ACTIVITY_TYPE_HTTP_TRANSACTION &&
        aSubtype === Ci.nsIHttpActivityObserver.ACTIVITY_SUBTYPE_REQUEST_HEADER
      ) {
        this.caps = aChannel.QueryInterface(Ci.nsIHttpChannelInternal).caps;
      }
    } catch (_) {}
  },
};

// Test in TRRFIRST mode, channel only wait for HTTPS RR when TRR is confirmed.
add_task(async function test_caps_in_trr_first() {
  Services.prefs.setCharPref("network.trr.confirmationNS", "skip");
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_DISABLED);

  let observerService = Cc[
    "@mozilla.org/network/http-activity-distributor;1"
  ].getService(Ci.nsIHttpActivityDistributor);
  let observer = new ActivityObserver();
  observerService.addObserver(observer);

  let chan = makeChan(`https://foo.example.com:${h2Port}/server-timing`);
  await channelOpenPromise(chan);

  Assert.ok(!WaitHTTPSRR(observer.caps));

  let trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  await trrServer.registerDoHAnswers("confirm.example.com", "NS", {
    answers: [
      {
        name: "confirm.example.com",
        ttl: 55,
        type: "NS",
        flush: false,
        data: "test.com",
      },
    ],
  });

  Services.dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );

  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
  await waitForConfirmationState(CONFIRM_OK, 1000);

  observer.caps = 0;
  chan = makeChan(`https://foo.example.com:${h2Port}/server-timing`);
  await channelOpenPromise(chan);

  Assert.ok(WaitHTTPSRR(observer.caps));
  await trrServer.stop();
});

// Test in TRRONLY mode, channel always wait for HTTPS RR.
add_task(async function test_caps_in_trr_only() {
  Services.prefs.setCharPref(
    "network.trr.confirmationNS",
    "confirm.example.com"
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);
  equal(Services.dns.currentTrrConfirmationState, CONFIRM_DISABLED);

  let observerService = Cc[
    "@mozilla.org/network/http-activity-distributor;1"
  ].getService(Ci.nsIHttpActivityDistributor);
  let observer = new ActivityObserver();
  observerService.addObserver(observer);

  let chan = makeChan(`https://foo.example.com:${h2Port}/server-timing`);
  await channelOpenPromise(chan);

  Assert.ok(WaitHTTPSRR(observer.caps));
});
