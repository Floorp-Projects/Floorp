/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

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

  onAcknowledge() {},
  onBinaryMessageAvailable(aContext, aMsg) {
    info("WsListener::onBinaryMessageAvailable");
    this._received = aMsg;
    this._ws.close(0, null);
  },
  onMessageAvailable() {},
  onServerClose() {},
  onWebSocketListenerStart() {},
  onStart() {
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

function openWebSocketChannelPromise(chan, url, msg) {
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
