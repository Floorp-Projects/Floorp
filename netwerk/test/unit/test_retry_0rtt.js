/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);
var httpServer = null;

let handlerCallbacks = {};

function listenHandler(metadata, response) {
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
    Services.env.set("MOZ_TLS_SERVER_0RTT", "1");
    await asyncStartTLSTestServer(
      "FaultyServer",
      "../../../security/manager/ssl/tests/unit/test_faulty_server"
    );
    let nssComponent = Cc["@mozilla.org/psm;1"].getService(Ci.nsINSSComponent);
    await nssComponent.asyncClearSSLExternalAndInternalSessionCache();
  }
);

async function sleep(time) {
  return new Promise(resolve => {
    do_timeout(time * 1000, resolve);
  });
}

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
  async function testRetry0Rtt() {
    var retryDomains = [
      "0rtt-alert-bad-mac.example.com",
      "0rtt-alert-protocol-version.example.com",
      //"0rtt-alert-unexpected.example.com", // TODO(bug 1753204): uncomment this
    ];

    Services.prefs.setCharPref("network.dns.localDomains", retryDomains);

    Services.prefs.setBoolPref("network.ssl_tokens_cache_enabled", true);

    for (var i = 0; i < retryDomains.length; i++) {
      {
        let countOfEarlyData = handlerCount("/callback/1");
        let chan = makeChan(`https://${retryDomains[i]}:8443`);
        let [, buf] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
        ok(buf);
        equal(
          handlerCount("/callback/1"),
          countOfEarlyData,
          "no early data sent"
        );
      }

      // The server has an anti-replay mechanism that prohibits it from
      // accepting 0-RTT connections immediately at startup.
      await sleep(1);

      {
        let countOfEarlyData = handlerCount("/callback/1");
        let chan = makeChan(`https://${retryDomains[i]}:8443`);
        let [, buf] = await channelOpenPromise(chan, CL_ALLOW_UNKNOWN_CL);
        ok(buf);
        equal(
          handlerCount("/callback/1"),
          countOfEarlyData + 1,
          "got early data"
        );
      }
    }
  }
);
