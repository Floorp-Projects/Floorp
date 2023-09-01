/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

trr_test_setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
});

let trrServer = null;
add_setup(async function start_trr_server() {
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
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRONLY);
});

add_task(async function ignoreUnknownTypes() {
  Services.dns.clearCache(true);
  await trrServer.registerDoHAnswers("abc.def.ced.com", "A", {
    answers: [
      {
        name: "abc.def.ced.com",
        ttl: 55,
        type: "DNAME",
        flush: false,
        data: "def.ced.com.test",
      },
      {
        name: "abc.def.ced.com",
        ttl: 55,
        type: "CNAME",
        flush: false,
        data: "abc.def.ced.com.test",
      },
      {
        name: "abc.def.ced.com.test",
        ttl: 55,
        type: "A",
        flush: false,
        data: "3.3.3.3",
      },
    ],
  });
  await new TRRDNSListener("abc.def.ced.com", { expectedAnswer: "3.3.3.3" });
});
