/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const override = Cc["@mozilla.org/network/native-dns-override;1"].getService(
  Ci.nsINativeDNSResolverOverride
);
const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);

trr_test_setup();
registerCleanupFunction(async () => {
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
    Services.obs.addObserver(function observe(subject, topic, data) {
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

let processId;

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
  registerCleanupFunction(async () => trrServer.stop());
  await trrServer.start();
  dump(`port = ${trrServer.port}\n`);
  let chan = makeChan(`https://localhost:${trrServer.port}/test?bla=some`);
  let [req, resp] = await channelOpenPromise(chan);
  equal(resp, "<h1> 404 Path not found: /test?bla=some</h1>");
  dns.clearCache(true);
  override.addIPOverride("ipv4only.arpa", "fe80::6a99:9b2b:c000:00aa");

  Services.obs.notifyObservers(null, "network:captive-portal-connectivity");
  await promiseObserverNotification(
    "network:connectivity-service:dns-checks-complete"
  );

  Services.prefs.setIntPref("network.trr.mode", 2);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );

  await trrServer.registerDoHAnswers("xyz.foo", "A", [
    {
      name: "xyz.foo",
      ttl: 55,
      type: "A",
      flush: false,
      data: "1.2.3.4",
    },
  ]);
  let [, inRecord] = await new TRRDNSListener("xyz.foo", undefined, false);

  inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
  inRecord.getNextAddrAsString();
  Assert.equal(
    inRecord.getNextAddrAsString(),
    "fe80::6a99:9b2b:102:304",
    `Checking that the NAT64-prefixed address is appended at the back.`
  );

  await trrServer.stop();

  override.clearOverrides();
});
