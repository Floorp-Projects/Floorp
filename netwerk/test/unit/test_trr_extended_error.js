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

let trrServer;
add_task(async function setup() {
  trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  dump(`port = ${trrServer.port()}\n`);
  let chan = makeChan(`https://localhost:${trrServer.port()}/test?bla=some`);
  let [, resp] = await channelOpenPromise(chan);
  equal(resp, "<h1> 404 Path not found: /test</h1>");

  Services.dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 2);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
});

add_task(async function test_extended_error_bogus() {
  await trrServer.registerDoHAnswers("something.foo", "A", {
    answers: [
      {
        name: "something.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });

  await new TRRDNSListener("something.foo", { expectedAnswer: "1.2.3.4" });

  await trrServer.registerDoHAnswers("a.foo", "A", {
    answers: [],
    additionals: [
      {
        name: ".",
        type: "OPT",
        class: "IN",
        options: [
          {
            code: "EDNS_ERROR",
            extended_error: 6, // DNSSEC_BOGUS
            text: "DNSSec bogus",
          },
        ],
      },
    ],
    flags: 2, // SERVFAIL
  });

  // Check that we don't fall back to DNS
  let { inStatus } = await new TRRDNSListener("a.foo", {
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

add_task(async function test_extended_error_filtered() {
  await trrServer.registerDoHAnswers("b.foo", "A", {
    answers: [],
    additionals: [
      {
        name: ".",
        type: "OPT",
        class: "IN",
        options: [
          {
            code: "EDNS_ERROR",
            extended_error: 17, // Filtered
            text: "Filtered",
          },
        ],
      },
    ],
  });

  // Check that we don't fall back to DNS
  let { inStatus } = await new TRRDNSListener("b.foo", {
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});

add_task(async function test_extended_error_not_ready() {
  await trrServer.registerDoHAnswers("c.foo", "A", {
    answers: [],
    additionals: [
      {
        name: ".",
        type: "OPT",
        class: "IN",
        options: [
          {
            code: "EDNS_ERROR",
            extended_error: 14, // Not ready
            text: "Not ready",
          },
        ],
      },
    ],
  });

  // For this code it's OK to fallback
  await new TRRDNSListener("c.foo", { expectedAnswer: "127.0.0.1" });
});

add_task(async function ipv6_answer_and_delayed_ipv4_error() {
  // AAAA comes back immediately.
  // A EDNS_ERROR comes back later, with a delay
  await trrServer.registerDoHAnswers("delay1.com", "AAAA", {
    answers: [
      {
        name: "delay1.com",
        ttl: 55,
        type: "AAAA",
        flush: false,
        data: "::a:b:c:d",
      },
    ],
  });
  await trrServer.registerDoHAnswers("delay1.com", "A", {
    answers: [],
    additionals: [
      {
        name: ".",
        type: "OPT",
        class: "IN",
        options: [
          {
            code: "EDNS_ERROR",
            extended_error: 17, // Filtered
            text: "Filtered",
          },
        ],
      },
    ],
    delay: 200,
  });

  // Check that we don't fall back to DNS
  await new TRRDNSListener("delay1.com", { expectedAnswer: "::a:b:c:d" });
});

add_task(async function ipv4_error_and_delayed_ipv6_answer() {
  // AAAA comes back immediately delay
  // A EDNS_ERROR comes back immediately
  await trrServer.registerDoHAnswers("delay2.com", "AAAA", {
    answers: [
      {
        name: "delay2.com",
        ttl: 55,
        type: "AAAA",
        flush: false,
        data: "::a:b:c:d",
      },
    ],
    delay: 200,
  });
  await trrServer.registerDoHAnswers("delay2.com", "A", {
    answers: [],
    additionals: [
      {
        name: ".",
        type: "OPT",
        class: "IN",
        options: [
          {
            code: "EDNS_ERROR",
            extended_error: 17, // Filtered
            text: "Filtered",
          },
        ],
      },
    ],
  });

  // Check that we don't fall back to DNS
  await new TRRDNSListener("delay2.com", { expectedAnswer: "::a:b:c:d" });
});

add_task(async function ipv4_answer_and_delayed_ipv6_error() {
  // A comes back immediately.
  // AAAA EDNS_ERROR comes back later, with a delay
  await trrServer.registerDoHAnswers("delay3.com", "A", {
    answers: [
      {
        name: "delay3.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });
  await trrServer.registerDoHAnswers("delay3.com", "AAAA", {
    answers: [],
    additionals: [
      {
        name: ".",
        type: "OPT",
        class: "IN",
        options: [
          {
            code: "EDNS_ERROR",
            extended_error: 17, // Filtered
            text: "Filtered",
          },
        ],
      },
    ],
    delay: 200,
  });

  // Check that we don't fall back to DNS
  await new TRRDNSListener("delay3.com", { expectedAnswer: "1.2.3.4" });
});

add_task(async function delayed_ipv4_answer_and_ipv6_error() {
  // A comes back with delay.
  // AAAA EDNS_ERROR comes immediately
  await trrServer.registerDoHAnswers("delay4.com", "A", {
    answers: [
      {
        name: "delay4.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
    delay: 200,
  });
  await trrServer.registerDoHAnswers("delay4.com", "AAAA", {
    answers: [],
    additionals: [
      {
        name: ".",
        type: "OPT",
        class: "IN",
        options: [
          {
            code: "EDNS_ERROR",
            extended_error: 17, // Filtered
            text: "Filtered",
          },
        ],
      },
    ],
  });

  // Check that we don't fall back to DNS
  await new TRRDNSListener("delay4.com", { expectedAnswer: "1.2.3.4" });
});

add_task(async function test_only_ipv4_extended_error() {
  Services.prefs.setBoolPref("network.dns.disableIPv6", true);
  await trrServer.registerDoHAnswers("only.com", "A", {
    answers: [],
    additionals: [
      {
        name: ".",
        type: "OPT",
        class: "IN",
        options: [
          {
            code: "EDNS_ERROR",
            extended_error: 17, // Filtered
            text: "Filtered",
          },
        ],
      },
    ],
  });
  let { inStatus } = await new TRRDNSListener("only.com", {
    expectedSuccess: false,
  });
  Assert.ok(
    !Components.isSuccessCode(inStatus),
    `${inStatus} should be an error code`
  );
});
