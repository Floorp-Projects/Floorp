/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

trr_test_setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
});

let trrServer = null;
add_task(async function setup() {
  trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  dump(`port = ${trrServer.port()}\n`);

  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);
});

add_task(async function check_ttl_works() {
  await trrServer.registerDoHAnswers("example.com", "A", {
    answers: [
      {
        name: "example.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });
  let { inRecord } = await new TRRDNSListener("example.com", {
    expectedAnswer: "1.2.3.4",
  });
  equal(inRecord.QueryInterface(Ci.nsIDNSAddrRecord).ttl, 55);
  await trrServer.registerDoHAnswers("example.org", "A", {
    answers: [
      {
        name: "example.org",
        ttl: 999,
        type: "A",
        flush: false,
        data: "1.2.3.4",
      },
    ],
  });
  // Simple check to see that TRR works.
  ({ inRecord } = await new TRRDNSListener("example.org", {
    expectedAnswer: "1.2.3.4",
  }));
  equal(inRecord.QueryInterface(Ci.nsIDNSAddrRecord).ttl, 999);
});
