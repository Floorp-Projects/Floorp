/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);

trr_test_setup();
registerCleanupFunction(async () => {
  Services.prefs.clearUserPref("network.connectivity-service.nat64-prefix");
  override.clearOverrides();
  trr_clear_prefs();
});

/**
 * Waits for an observer notification to fire.
 *
 * @param {String} topic The notification topic.
 * @returns {Promise} A promise that fulfills when the notification is fired.
 */
function promiseObserverNotification(topic, matchFunc) {
  return new Promise((resolve, reject) => {
    Services.obs.addObserver(function observe(subject, topic1, data) {
      let matches = typeof matchFunc != "function" || matchFunc(subject, data);
      if (!matches) {
        return;
      }
      Services.obs.removeObserver(observe, topic);
      resolve({ subject, data });
    }, topic);
  });
}

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
    chan.asyncOpen(new ChannelListener(finish));
  });
}

add_task(async function test_add_nat64_prefix_to_trr() {
  let trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  dump(`port = ${trrServer.port()}\n`);
  let chan = makeChan(`https://localhost:${trrServer.port()}/test?bla=some`);
  let [, resp] = await channelOpenPromise(chan);
  equal(resp, "<h1> 404 Path not found: /test</h1>");
  Services.dns.clearCache(true);
  override.addIPOverride("ipv4only.arpa", "fe80::9b2b:c000:00aa");
  Services.prefs.setCharPref(
    "network.connectivity-service.nat64-prefix",
    "ae80::3b1b:c343:1133"
  );

  let topic = "network:connectivity-service:dns-checks-complete";
  if (mozinfo.socketprocess_networking) {
    topic += "-from-socket-process";
  }
  let notification = promiseObserverNotification(topic);
  Services.obs.notifyObservers(null, "network:captive-portal-connectivity");
  await notification;

  Services.prefs.setIntPref("network.trr.mode", 2);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  await trrServer.registerDoHAnswers("xyz.foo", "A", {
    answers: [
      {
        name: "xyz.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });
  let { inRecord } = await new TRRDNSListener("xyz.foo", {
    expectedSuccess: false,
  });

  inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
  Assert.equal(
    inRecord.getNextAddrAsString(),
    "1.2.3.4",
    `Checking that native IPv4 addresses have higher priority.`
  );

  Assert.equal(
    inRecord.getNextAddrAsString(),
    "ae80::3b1b:102:304",
    `Checking the manually entered NAT64-prefixed address is in the middle.`
  );

  Assert.equal(
    inRecord.getNextAddrAsString(),
    "fe80::9b2b:102:304",
    `Checking that the NAT64-prefixed address is appended at the back.`
  );

  await trrServer.stop();
});
