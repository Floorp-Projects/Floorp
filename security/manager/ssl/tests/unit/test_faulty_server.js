/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* based on netwerk/test/unit/test_retry_0rtt.js */

"use strict";

/* import-globals-from ../../../../../netwerk/test/unit/head_channels.js */
load("../../../../../netwerk/test/unit/head_channels.js");

var httpServer = null;

let handlerCallbacks = {};

function listenHandler(metadata) {
  info(metadata.path);
  handlerCallbacks[metadata.path] = (handlerCallbacks[metadata.path] || 0) + 1;
}

function handlerCount(path) {
  return handlerCallbacks[path] || 0;
}

ChromeUtils.importESModule("resource://gre/modules/AppConstants.sys.mjs");

// Bug 1805371: Tests that require FaultyServer can't currently be built
// with system NSS.
add_setup(
  {
    skip_if: () => AppConstants.MOZ_SYSTEM_NSS,
  },
  async () => {
    do_get_profile();
    Services.fog.initializeFOG();

    httpServer = new HttpServer();
    httpServer.registerPrefixHandler("/callback/", listenHandler);
    httpServer.start(-1);

    registerCleanupFunction(async () => {
      await httpServer.stop();
    });

    Services.env.set(
      "FAULTY_SERVER_CALLBACK_PORT",
      httpServer.identity.primaryPort
    );
    await asyncStartTLSTestServer("FaultyServer", "test_faulty_server");
  }
);

function makeChan(url) {
  let chan = NetUtil.newChannel({
    uri: url,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);

  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

function channelOpenPromise(chan, flags) {
  return new Promise(resolve => {
    chan.asyncOpen(
      new ChannelListener((req, buffer) => resolve([req, buffer]), null, flags)
    );
  });
}

add_task(
  {
    skip_if: () => AppConstants.MOZ_SYSTEM_NSS,
  },
  async function testRetryXyber() {
    const retryDomain = "xyber-net-interrupt.example.com";

    Services.prefs.setBoolPref("security.tls.enable_kyber", true);
    Services.prefs.setCharPref("network.dns.localDomains", [retryDomain]);
    Services.prefs.setIntPref("network.http.speculative-parallel-limit", 0);

    // Get the number of xyber / x25519 callbacks prior to making the request
    // ssl_grp_kem_xyber768d00 = 25497
    // ssl_grp_ec_curve25519 = 29
    let countOfXyber = handlerCount("/callback/25497");
    let countOfX25519 = handlerCount("/callback/29");
    let chan = makeChan(`https://${retryDomain}:8443`);
    let [, buf] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
    ok(buf);
    // The server will make a xyber768d00 callback for the initial request, and
    // then an x25519 callback for the retry. Both callback counts should
    // increment by one.
    equal(
      handlerCount("/callback/25497"),
      countOfXyber + 1,
      "negotiated xyber768d00"
    );
    equal(handlerCount("/callback/29"), countOfX25519 + 1, "negotiated x25519");
    if (!mozinfo.socketprocess_networking) {
      // Bug 1824574
      equal(
        1,
        await Glean.tls.xyberIntoleranceReason.PR_END_OF_FILE_ERROR.testGetValue(),
        "PR_END_OF_FILE_ERROR telemetry accumulated"
      );
    }
  }
);

add_task(
  {
    skip_if: () => AppConstants.MOZ_SYSTEM_NSS,
  },
  async function testNoRetryXyber() {
    const retryDomain = "xyber-alert-after-server-hello.example.com";

    Services.prefs.setBoolPref("security.tls.enable_kyber", true);
    Services.prefs.setCharPref("network.dns.localDomains", [retryDomain]);
    Services.prefs.setIntPref("network.http.speculative-parallel-limit", 0);

    // Get the number of xyber / x25519 / p256 callbacks prior to making the request
    // ssl_grp_kem_xyber768d00 = 25497
    // ssl_grp_ec_curve25519 = 29
    let countOfXyber = handlerCount("/callback/25497");
    let countOfX25519 = handlerCount("/callback/29");
    let chan = makeChan(`https://${retryDomain}:8443`);
    let [req] = await channelOpenPromise(chan, CL_EXPECT_FAILURE);
    equal(req.status, 0x805a2f4d); // psm::GetXPCOMFromNSSError(SSL_ERROR_HANDSHAKE_FAILED)
    // The server will make a xyber768d00 callback for the initial request and
    // the client should not retry.
    equal(
      handlerCount("/callback/25497"),
      countOfXyber + 1,
      "negotiated xyber768d00"
    );
    equal(
      handlerCount("/callback/29"),
      countOfX25519,
      "did not negotiate x25519"
    );
  }
);
