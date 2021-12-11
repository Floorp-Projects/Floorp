/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

registerCleanupFunction(async () => {
  http3_clear_prefs();
});

let wssUri;
let httpsUri;

add_task(async function pre_setup() {
  let env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
  );

  let h2Port = env.get("MOZHTTP2_PORT");
  Assert.notEqual(h2Port, null);
  Assert.notEqual(h2Port, "");

  wssUri = "wss://foo.example.com:" + h2Port + "/websocket";
  httpsUri = "https://foo.example.com:" + h2Port + "/";
  Services.prefs.setBoolPref("network.http.http3.support_version1", true);
});

add_task(async function setup() {
  await http3_setup_tests("h3");
});

let WebSocketListener = function() {};

WebSocketListener.prototype = {
  onAcknowledge(aContext, aSize) {},
  onBinaryMessageAvailable(aContext, aMsg) {},
  onMessageAvailable(aContext, aMsg) {},
  onServerClose(aContext, aCode, aReason) {},
  onStart(aContext) {
    this.finish();
  },
  onStop(aContext, aStatusCode) {},
};

function makeH2Chan() {
  let chan = NetUtil.newChannel({
    uri: httpsUri,
    loadUsingSystemPrincipal: true,
  }).QueryInterface(Ci.nsIHttpChannel);
  chan.loadFlags = Ci.nsIChannel.LOAD_INITIAL_DOCUMENT_URI;
  return chan;
}

add_task(async function open_wss_when_h3_is_active() {
  // Make an active connection using HTTP/3
  let chanHttp1 = makeH2Chan(httpsUri);
  await new Promise(resolve => {
    chanHttp1.asyncOpen(
      new ChannelListener(request => {
        let httpVersion = "";
        try {
          httpVersion = request.protocolVersion;
        } catch (e) {}
        Assert.equal(httpVersion, "h3");
        resolve();
      })
    );
  });

  // Now try to connect ot a WebSocket on the same port -> this should not loop
  // see bug 1717360.
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

  var uri = Services.io.newURI(wssUri);
  var wsListener = new WebSocketListener();
  await new Promise(resolve => {
    wsListener.finish = resolve;
    chan.asyncOpen(uri, wssUri, {}, 0, wsListener, null);
  });

  // Try to use https protocol, it should sttill use HTTP/3
  let chanHttp2 = makeH2Chan(httpsUri);
  await new Promise(resolve => {
    chanHttp2.asyncOpen(
      new ChannelListener(request => {
        let httpVersion = "";
        try {
          httpVersion = request.protocolVersion;
        } catch (e) {}
        Assert.equal(httpVersion, "h3");
        resolve();
      })
    );
  });
});
