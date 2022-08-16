/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* import-globals-from head_cache.js */
/* import-globals-from head_cookies.js */
/* import-globals-from head_channels.js */
/* import-globals-from head_servers.js */

const certOverrideService = Cc[
  "@mozilla.org/security/certoverride;1"
].getService(Ci.nsICertOverrideService);

add_setup(async function setup() {
  certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
    true
  );

  registerCleanupFunction(async () => {
    certOverrideService.setDisableAllSecurityChecksAndLetAttackersInterceptMyData(
      false
    );
  });
});

function WebSocketListener(closure, ws, sentMsg) {
  this._closure = closure;
  this._ws = ws;
  this._sentMsg = sentMsg;
}

WebSocketListener.prototype = {
  _closure: null,
  _ws: null,
  _sentMsg: null,
  _received: null,
  QueryInterface: ChromeUtils.generateQI(["nsIWebSocketListener"]),

  onAcknowledge(aContext, aSize) {},
  onBinaryMessageAvailable(aContext, aMsg) {
    this._received = aMsg;
    this._ws.close(0, null);
  },
  onMessageAvailable(aContext, aMsg) {},
  onServerClose(aContext, aCode, aReason) {},
  onSWebSocketListenertart(aContext) {},
  onStart(aContext) {
    this._ws.sendMsg(this._sentMsg);
  },
  onStop(aContext, aStatusCode) {
    try {
      this._closure(aStatusCode, this._received);
      this._ws = null;
    } catch (ex) {
      do_throw("Error in closure function: " + ex);
    }
  },
};

function makeWebSocketChan() {
  let chan = Cc["@mozilla.org/network/protocol;1?name=wss"].createInstance(
    Ci.nsIWebSocketChannel
  );
  chan.initLoadInfo(
    null, // aLoadingNode
    Services.scriptSecurityManager.getSystemPrincipal(),
    null, // aTriggeringPrincipal
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    Ci.nsIContentPolicy.TYPE_WEBSOCKET
  );
  return chan;
}

function channelOpenPromise(chan, url, msg) {
  let uri = Services.io.newURI(url);
  return new Promise(resolve => {
    function finish(status, result) {
      resolve([status, result]);
    }
    chan.asyncOpen(
      uri,
      url,
      {},
      0,
      new WebSocketListener(finish, chan, msg),
      null
    );
  });
}

add_task(async function test_websocket() {
  let wss = new NodeWebSocketServer();
  await wss.start();
  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler(data => {
    global.ws.send(data);
  });
  let chan = makeWebSocketChan();
  let url = `wss://localhost:${wss.port()}`;
  const msg = "test websocket";
  let [status, res] = await channelOpenPromise(chan, url, msg);
  Assert.equal(status, Cr.NS_OK);
  Assert.equal(res, msg);
  await wss.stop();
});

add_task(async function test_ws_through_https_proxy() {
  let certdb = Cc["@mozilla.org/security/x509certdb;1"].getService(
    Ci.nsIX509CertDB
  );
  addCertFromFile(certdb, "http2-ca.pem", "CTu,u,u");

  let proxy = new NodeHTTPSProxyServer();
  await proxy.start();

  let wss = new NodeWebSocketServer();
  await wss.start();
  Assert.notEqual(wss.port(), null);
  await wss.registerMessageHandler(data => {
    global.ws.send(data);
  });

  let chan = makeWebSocketChan();
  let url = `wss://localhost:${wss.port()}`;
  const msg = "test websocket through proxy";
  let [status, res] = await channelOpenPromise(chan, url, msg);
  Assert.equal(status, Cr.NS_OK);
  Assert.equal(res, msg);

  await proxy.stop();
  await wss.stop();
});
