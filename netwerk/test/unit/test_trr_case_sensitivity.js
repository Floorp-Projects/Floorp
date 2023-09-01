/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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

function channelOpenPromise(chan) {
  return new Promise(resolve => {
    function finish(req, buffer) {
      resolve([req, buffer]);
    }
    chan.asyncOpen(new ChannelListener(finish));
  });
}

add_task(async function test_trr_casing() {
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
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  // This CNAME response goes to B.example.com (uppercased)
  // It should be lowercased by the code
  await trrServer.registerDoHAnswers("a.example.com", "A", {
    answers: [
      {
        name: "a.example.com",
        ttl: 55,
        type: "CNAME",
        flush: false,
        data: "B.example.com",
      },
    ],
  });
  // Like in bug 1635566, the response for B.example.com will be lowercased
  // by the server too -> b.example.com
  // Requesting this resource would case the browser to reject the resource
  await trrServer.registerDoHAnswers("B.example.com", "A", {
    answers: [
      {
        name: "b.example.com",
        ttl: 55,
        type: "CNAME",
        flush: false,
        data: "c.example.com",
      },
    ],
  });

  // The browser should request this one
  await trrServer.registerDoHAnswers("b.example.com", "A", {
    answers: [
      {
        name: "b.example.com",
        ttl: 55,
        type: "CNAME",
        flush: false,
        data: "c.example.com",
      },
    ],
  });
  // Finally, it gets an IP
  await trrServer.registerDoHAnswers("c.example.com", "A", {
    answers: [
      {
        name: "c.example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });
  await new TRRDNSListener("a.example.com", { expectedAnswer: "1.2.3.4" });

  await trrServer.registerDoHAnswers("a.test.com", "A", {
    answers: [
      {
        name: "a.test.com",
        ttl: 55,
        type: "CNAME",
        flush: false,
        data: "B.test.com",
      },
    ],
  });
  // We try this again, this time we explicitly make sure this resource
  // is never used
  await trrServer.registerDoHAnswers("B.test.com", "A", {
    answers: [
      {
        name: "B.test.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "9.9.9.9",
      },
    ],
  });
  await trrServer.registerDoHAnswers("b.test.com", "A", {
    answers: [
      {
        name: "b.test.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "8.8.8.8",
      },
    ],
  });
  await new TRRDNSListener("a.test.com", { expectedAnswer: "8.8.8.8" });

  await trrServer.registerDoHAnswers("CAPITAL.COM", "A", {
    answers: [
      {
        name: "capital.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "2.2.2.2",
      },
    ],
  });
  await new TRRDNSListener("CAPITAL.COM", { expectedAnswer: "2.2.2.2" });
  await new TRRDNSListener("CAPITAL.COM.", { expectedAnswer: "2.2.2.2" });

  await trrServer.stop();
});
