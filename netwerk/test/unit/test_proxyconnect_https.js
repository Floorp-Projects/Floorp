/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

const { TestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TestUtils.sys.mjs"
);

function makeChan(uri) {
  let chan = NetUtil.newChannel({
    uri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

let gotTransport = false;
var upgradeListener = {
  onTransportAvailable: () => {
    gotTransport = true;
  },
  QueryInterface: ChromeUtils.generateQI(["nsIHttpUpgradeListener"]),
};

add_task(async function test_connect_only_https() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");
  addCertFromFile(certdb, "proxy-ca.pem", "CTu,u,u");

  let proxy = new NodeHTTPSProxyServer();
  await proxy.start();
  let server = new NodeHTTPSServer();
  await server.start();
  registerCleanupFunction(async () => {
    await proxy.stop();
    await server.stop();
  });

  let chan = makeChan(`https://localhost:${server.port()}/test`);
  var internal = chan.QueryInterface(Ci.nsIHttpChannelInternal);
  internal.HTTPUpgrade("webrtc", upgradeListener);
  internal.setConnectOnly(false);
  await new Promise(resolve => {
    chan.asyncOpen(new ChannelListener(resolve, null, CL_ALLOW_UNKNOWN_CL));
  });

  await TestUtils.waitForCondition(() => gotTransport);
  Assert.ok(gotTransport);

  await proxy.stop();
  await server.stop();
});
