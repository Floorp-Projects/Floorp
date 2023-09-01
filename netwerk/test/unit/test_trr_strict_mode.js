/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function setup() {
  trr_test_setup();
}
setup();

add_task(async function checkBlocklisting() {
  Services.prefs.setBoolPref("network.trr.temp_blocklist", true);
  Services.prefs.setBoolPref("network.trr.strict_native_fallback", true);

  let trrServer = new TRRServer();
  registerCleanupFunction(async () => {
    await trrServer.stop();
  });
  await trrServer.start();
  info(`port = ${trrServer.port()}\n`);

  Services.dns.clearCache(true);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );
  Services.prefs.setIntPref("network.trr.mode", Ci.nsIDNSService.MODE_TRRFIRST);

  // Check that we properly fallback to native DNS for a variety of DNS rcodes
  for (let i = 0; i <= 5; i++) {
    info(`testing rcode=${i}`);
    await trrServer.registerDoHAnswers(`sub${i}.blocklisted.com`, "A", {
      flags: i,
    });
    await trrServer.registerDoHAnswers(`sub${i}.blocklisted.com`, "AAAA", {
      flags: i,
    });

    await new TRRDNSListener(`sub${i}.blocklisted.com`, {
      expectedAnswer: "127.0.0.1",
    });
    Services.dns.clearCache(true);
    await new TRRDNSListener(`sub${i}.blocklisted.com`, {
      expectedAnswer: "127.0.0.1",
    });
    await new TRRDNSListener(`sub.sub${i}.blocklisted.com`, {
      expectedAnswer: "127.0.0.1",
    });
    Services.dns.clearCache(true);
  }
});
