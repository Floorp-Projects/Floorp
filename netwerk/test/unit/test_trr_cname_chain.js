/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const dns = Cc["@mozilla.org/network/dns-service;1"].getService(
  Ci.nsIDNSService
);
let trrServer;

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

add_setup(async function setup() {
  trr_test_setup();
  registerCleanupFunction(async () => {
    trr_clear_prefs();
  });

  trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  dump(`port = ${trrServer.port}\n`);
  let chan = makeChan(`https://localhost:${trrServer.port}/test?bla=some`);
  let [, resp] = await channelOpenPromise(chan);
  equal(resp, "<h1> 404 Path not found: /test?bla=some</h1>");

  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );
});

add_task(async function test_follow_cnames_same_response() {
  await trrServer.registerDoHAnswers("something.foo", "A", {
    answers: [
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
    ],
  });
  let { inRecord } = await new TRRDNSListener("something.foo", {
    expectedAnswer: "1.2.3.4",
    flags: Ci.nsIDNSService.RESOLVE_CANONICAL_NAME,
  });
  equal(inRecord.QueryInterface(Ci.nsIDNSAddrRecord).canonicalName, "xyz.foo");

  await trrServer.registerDoHAnswers("a.foo", "A", {
    answers: [
      {
        name: "a.foo",
        ttl: 55,
        type: "CNAME",
        flush: false,
        data: "b.foo",
      },
    ],
  });
  await trrServer.registerDoHAnswers("b.foo", "A", {
    answers: [
      {
        name: "b.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "2.3.4.5",
      },
    ],
  });
  await new TRRDNSListener("a.foo", { expectedAnswer: "2.3.4.5" });
});

add_task(async function test_cname_nodata() {
  // Test that we don't needlessly follow cname chains when the RA flag is set
  // on the response.

  await trrServer.registerDoHAnswers("first.foo", "A", {
    flags: 0x80,
    answers: [
      {
        name: "first.foo",
        ttl: 55,
        type: "CNAME",
        flush: false,
        data: "second.foo",
      },
      {
        name: "second.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });
  await trrServer.registerDoHAnswers("first.foo", "AAAA", {
    flags: 0x80,
    answers: [
      {
        name: "first.foo",
        ttl: 55,
        type: "CNAME",
        flush: false,
        data: "second.foo",
      },
    ],
  });

  await new TRRDNSListener("first.foo", { expectedAnswer: "1.2.3.4" });
  equal(await trrServer.requestCount("first.foo", "A"), 1);
  equal(await trrServer.requestCount("first.foo", "AAAA"), 1);
  equal(await trrServer.requestCount("second.foo", "A"), 0);
  equal(await trrServer.requestCount("second.foo", "AAAA"), 0);

  await trrServer.registerDoHAnswers("first.bar", "A", {
    answers: [
      {
        name: "first.bar",
        ttl: 55,
        type: "CNAME",
        flush: false,
        data: "second.bar",
      },
      {
        name: "second.bar",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });
  await trrServer.registerDoHAnswers("first.bar", "AAAA", {
    answers: [
      {
        name: "first.bar",
        ttl: 55,
        type: "CNAME",
        flush: false,
        data: "second.bar",
      },
    ],
  });

  await new TRRDNSListener("first.bar", { expectedAnswer: "1.2.3.4" });
  equal(await trrServer.requestCount("first.bar", "A"), 1);
  equal(await trrServer.requestCount("first.bar", "AAAA"), 1);
  equal(await trrServer.requestCount("second.bar", "A"), 0); // addr included in first response
  equal(await trrServer.requestCount("second.bar", "AAAA"), 1); // will follow cname because no flag is set

  // Check that it also works for HTTPS records

  await trrServer.registerDoHAnswers("first.bar", "HTTPS", {
    flags: 0x80,
    answers: [
      {
        name: "second.bar",
        ttl: 55,
        type: "HTTPS",
        flush: false,
        data: {
          priority: 1,
          name: "h3pool",
          values: [
            { key: "alpn", value: ["h2", "h3"] },
            { key: "no-default-alpn" },
            { key: "port", value: 8888 },
            { key: "ipv4hint", value: "1.2.3.4" },
            { key: "echconfig", value: "123..." },
            { key: "ipv6hint", value: "::1" },
          ],
        },
      },
      {
        name: "first.bar",
        ttl: 55,
        type: "CNAME",
        flush: false,
        data: "second.bar",
      },
    ],
  });

  let { inStatus } = await new TRRDNSListener("first.bar", {
    type: dns.RESOLVE_TYPE_HTTPSSVC,
  });
  Assert.ok(Components.isSuccessCode(inStatus), `${inStatus} should work`);
  equal(await trrServer.requestCount("first.bar", "HTTPS"), 1);
  equal(await trrServer.requestCount("second.bar", "HTTPS"), 0);
});
