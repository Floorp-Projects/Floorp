/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

function setup() {
  trr_test_setup();
  Services.prefs.setBoolPref("network.trr.fetch_off_main_thread", true);
}

setup();
registerCleanupFunction(async () => {
  trr_clear_prefs();
});

function AuthPrompt() {}

AuthPrompt.prototype = {
  user: "guest",
  pass: "guest",

  QueryInterface: ChromeUtils.generateQI(["nsIAuthPrompt2"]),

  promptAuth: function ap_promptAuth(channel, level, authInfo) {
    authInfo.username = this.user;
    authInfo.password = this.pass;

    return true;
  },

  asyncPromptAuth: function ap_async(chan, cb, ctx, lvl, info) {
    throw Components.Exception("", Cr.NS_ERROR_NOT_IMPLEMENTED);
  },
};

function Requestor() {}

Requestor.prototype = {
  QueryInterface: ChromeUtils.generateQI(["nsIInterfaceRequestor"]),

  getInterface: function requestor_gi(iid) {
    if (iid.equals(Ci.nsIAuthPrompt2)) {
      // Allow the prompt to store state by caching it here
      if (!this.prompt) {
        this.prompt = new AuthPrompt();
      }
      return this.prompt;
    }

    throw Components.Exception("", Cr.NS_ERROR_NO_INTERFACE);
  },

  prompt: null,
};

// Test if we successfully retry TRR request on main thread.
add_task(async function test_trr_proxy_auth() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
  addCertFromFile(certdb, "proxy-ca.pem", "CTu,u,u");

  let trrServer = new TRRServer();
  await trrServer.start();
  Services.prefs.setIntPref("network.trr.mode", 3);
  Services.prefs.setCharPref(
    "network.trr.uri",
    `https://foo.example.com:${trrServer.port()}/dns-query`
  );

  await trrServer.registerDoHAnswers("test.proxy.com", "A", {
    answers: [
      {
        name: "test.proxy.com",
        ttl: 55,
        type: "A",
        flush: false,
        data: "3.3.3.3",
      },
    ],
  });

  await new TRRDNSListener("test.proxy.com", "3.3.3.3");

  let proxy = new NodeHTTP2ProxyServer();
  await proxy.start(0, true);
  registerCleanupFunction(async () => {
    await proxy.stop();
    await trrServer.stop();
  });

  let authTriggered = false;
  let observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),
    observe(aSubject, aTopic, aData) {
      if (aTopic == "http-on-examine-response") {
        Services.obs.removeObserver(observer, "http-on-examine-response");
        let channel = aSubject.QueryInterface(Ci.nsIChannel);
        channel.notificationCallbacks = new Requestor();
        if (
          channel.URI.spec.startsWith(
            `https://foo.example.com:${trrServer.port()}/dns-query`
          )
        ) {
          authTriggered = true;
        }
      }
    },
  };
  Services.obs.addObserver(observer, "http-on-examine-response");

  Services.dns.clearCache(true);
  await new TRRDNSListener("test.proxy.com", "3.3.3.3");
  Assert.ok(authTriggered);
});
