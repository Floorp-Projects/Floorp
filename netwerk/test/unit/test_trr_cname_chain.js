/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);

trr_test_setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
});

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

add_task(async function test_follow_cnames_same_response() {
  let trrServer = new TRRServer();
  registerCleanupFunction(async () => trrServer.stop());
  await trrServer.start();
  dump(`port = ${trrServer.port}\n`);
  let chan = makeChan(`https://localhost:${trrServer.port}/test?bla=some`);
  let [req, resp] = await channelOpenPromise(chan);
  equal(resp, "<h1> 404 Path not found: /test?bla=some</h1>");

  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );

  await trrServer.registerDoHAnswers("something.foo", "A", [
    {
      name: "something.foo",
      ttl: 55,
      type: "CNAME",
      flush: false,
      data: "other.foo",
    },
    {
      name: "other.foo",
      ttl: 55,
      type: "CNAME",
      flush: false,
      data: "bla.foo",
    },
    {
      name: "bla.foo",
      ttl: 55,
      type: "CNAME",
      flush: false,
      data: "xyz.foo",
    },
    {
      name: "xyz.foo",
      ttl: 55,
      type: "A",
      flush: false,
      data: "1.2.3.4",
    },
  ]);
  let [inRequest, inRecord, inStatus] = await new TRRDNSListener(
    "something.foo",
    {
      expectedAnswer: "1.2.3.4",
      flags: Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
    }
  );
  equal(inRecord.QueryInterface(Ci.nsIDNSAddrRecord).canonicalName, "xyz.foo");

  await trrServer.registerDoHAnswers("a.foo", "A", [
    {
      name: "a.foo",
      ttl: 55,
      type: "CNAME",
      flush: false,
      data: "b.foo",
    },
  ]);
  await trrServer.registerDoHAnswers("b.foo", "A", [
    {
      name: "b.foo",
      ttl: 55,
      type: "A",
      flush: false,
      data: "2.3.4.5",
    },
  ]);
  await new TRRDNSListener("a.foo", { expectedAnswer: "2.3.4.5" });

  await trrServer.stop();
});
