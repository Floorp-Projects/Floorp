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

let trrServer = new TRRServer();
registerCleanupFunction(async () => trrServer.stop());
add_task(async function setup_server() {
  await trrServer.start();
  dump(`port = ${trrServer.port}\n`);
  let chan = makeChan(`https://localhost:${trrServer.port}/test?bla=some`);
  let [req, resp] = await channelOpenPromise(chan);
  equal(resp, "<h1> 404 Path not found: /test?bla=some</h1>");
});

add_task(async function test_parse_additional_section() {
  dns.clearCache(true);
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port}/dns-query`
  );

  await trrServer.registerDoHAnswers(
    "something.foo",
    "A",
    [
      {
        name: "something.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
    [
      {
        name: "else.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "2.3.4.5",
      },
    ]
  );

  await new TRRDNSListener("something.foo", { expectedAnswer: "1.2.3.4" });
  await new TRRDNSListener("else.foo", { expectedAnswer: "2.3.4.5" });

  await trrServer.registerDoHAnswers(
    "a.foo",
    "A",
    [
      {
        name: "a.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
    [
      {
        name: "b.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "2.3.4.5",
      },
    ]
  );
  await trrServer.registerDoHAnswers("b.foo", "A", [
    {
      name: "b.foo",
      ttl: 55,
      type: "A",
      flush: false,
      data: "3.4.5.6",
    },
  ]);

  let req1 = new TRRDNSListener("a.foo", { expectedAnswer: "1.2.3.4" });

  // A request for b.foo will be in progress by the time we parse the additional
  // record. To keep things simple we don't end up saving the record, instead
  // we wait for the in-progress request to complete.
  // This check is also racy - if the response for a.foo completes before we make
  // this request, we'll put the other IP in the cache. But that is very unlikely.
  let req2 = new TRRDNSListener("b.foo", { expectedAnswer: "3.4.5.6" });

  await Promise.all([req1, req2]);

  // IPv6 additional
  await trrServer.registerDoHAnswers(
    "xyz.foo",
    "A",
    [
      {
        name: "xyz.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
    [
      {
        name: "abc.foo",
        ttl: 55,
        type: "AAAA",
        flush: false,
        data: "::1:2:3:4",
      },
    ]
  );

  await new TRRDNSListener("xyz.foo", { expectedAnswer: "1.2.3.4" });
  await new TRRDNSListener("abc.foo", { expectedAnswer: "::1:2:3:4" });

  // IPv6 additional
  await trrServer.registerDoHAnswers(
    "ipv6.foo",
    "AAAA",
    [
      {
        name: "ipv6.foo",
        ttl: 55,
        type: "AAAA",
        flush: false,
        data: "2001::a:b:c:d",
      },
    ],
    [
      {
        name: "def.foo",
        ttl: 55,
        type: "AAAA",
        flush: false,
        data: "::a:b:c:d",
      },
    ]
  );

  await new TRRDNSListener("ipv6.foo", { expectedAnswer: "2001::a:b:c:d" });
  await new TRRDNSListener("def.foo", { expectedAnswer: "::a:b:c:d" });

  // IPv6 additional
  await trrServer.registerDoHAnswers(
    "ipv6b.foo",
    "AAAA",
    [
      {
        name: "ipv6b.foo",
        ttl: 55,
        type: "AAAA",
        flush: false,
        data: "2001::a:b:c:d",
      },
    ],
    [
      {
        name: "qqqq.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "9.8.7.6",
      },
    ]
  );

  await new TRRDNSListener("ipv6b.foo", { expectedAnswer: "2001::a:b:c:d" });
  await new TRRDNSListener("qqqq.foo", { expectedAnswer: "9.8.7.6" });

  // Multiple IPs and multiple additional records
  await trrServer.registerDoHAnswers(
    "multiple.foo",
    "A",
    [
      {
        name: "multiple.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "9.9.9.9",
      },
    ],
    [
      {
        // Should be ignored, because it should be in the answer section
        name: "multiple.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.1.1.1",
      },
      {
        // Is ignored, because it should be in the answer section
        name: "multiple.foo",
        ttl: 55,
        type: "AAAA",
        flush: false,
        data: "2001::a:b:c:d",
      },
      {
        name: "yuiop.foo",
        ttl: 55,
        type: "AAAA",
        flush: false,
        data: "2001::a:b:c:d",
      },
      {
        name: "yuiop.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ]
  );

  let [inRequest, inRecord, inStatus] = await new TRRDNSListener(
    "multiple.foo",
    { expectedAnswer: "9.9.9.9" }
  );
  let IPs = [];
  inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
  inRecord.rewind();
  while (inRecord.hasMore()) {
    IPs.push(inRecord.getNextAddrAsString());
  }
  equal(IPs.length, 1);
  equal(IPs[0], "9.9.9.9");
  IPs = [];
  [inRequest, inRecord, inStatus] = await new TRRDNSListener("yuiop.foo", {
    expectedSuccess: false,
  });
  inRecord.QueryInterface(Ci.nsIDNSAddrRecord);
  inRecord.rewind();
  while (inRecord.hasMore()) {
    IPs.push(inRecord.getNextAddrAsString());
  }
  equal(IPs.length, 2);
  equal(IPs[0], "2001::a:b:c:d");
  equal(IPs[1], "1.2.3.4");
});

add_task(async function test_additional_after_resolve() {
  await trrServer.registerDoHAnswers("first.foo", "A", [
    {
      name: "first.foo",
      ttl: 55,
      type: "A",
      flush: false,
      data: "3.4.5.6",
    },
  ]);
  await new TRRDNSListener("first.foo", { expectedAnswer: "3.4.5.6" });

  await trrServer.registerDoHAnswers(
    "second.foo",
    "A",
    [
      {
        name: "second.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
    [
      {
        name: "first.foo",
        ttl: 55,
        type: "A",
        flush: false,
        data: "2.3.4.5",
      },
    ]
  );

  await new TRRDNSListener("second.foo", { expectedAnswer: "1.2.3.4" });
  await new TRRDNSListener("first.foo", { expectedAnswer: "2.3.4.5" });
});
