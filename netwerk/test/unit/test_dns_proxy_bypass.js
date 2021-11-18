/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var prefs = Services.prefs;

function setup() {
  prefs.setBoolPref("network.dns.notifyResolution", true);
  prefs.setCharPref("network.proxy.socks", "127.0.0.1");
  prefs.setIntPref("network.proxy.socks_port", 9000);
  prefs.setIntPref("network.proxy.type", 1);
  prefs.setBoolPref("network.proxy.socks_remote_dns", true);
}

setup();
registerCleanupFunction(async () => {
  prefs.clearUserPref("network.proxy.socks");
  prefs.clearUserPref("network.proxy.socks_port");
  prefs.clearUserPref("network.proxy.type");
  prefs.clearUserPref("network.proxy.socks_remote_dns");
  prefs.clearUserPref("network.dns.notifyResolution");
});

var url = "ws://dnsleak.example.com";

var dnsRequestObserver = {
  register() {
    this.obs = Services.obs;
    this.obs.addObserver(this, "dns-resolution-request");
  },

  unregister() {
    if (this.obs) {
      this.obs.removeObserver(this, "dns-resolution-request");
    }
  },

  observe(subject, topic, data) {
    if (topic == "dns-resolution-request") {
      Assert.ok(!data.includes("dnsleak.example.com"), `no dnsleak: ${data}`);
    }
  },
};

function WSListener(closure) {
  this._closure = closure;
}
WSListener.prototype = {
  onAcknowledge(aContext, aSize) {},
  onBinaryMessageAvailable(aContext, aMsg) {},
  onMessageAvailable(aContext, aMsg) {},
  onServerClose(aContext, aCode, aReason) {},
  onStart(aContext) {},
  onStop(aContext, aStatusCode) {
    dnsRequestObserver.unregister();
    this._closure();
  },
};

add_task(async function test_dns_websocket_channel() {
  dnsRequestObserver.register();

  var chan = Cc["@mozilla.org/network/protocol;1?name=ws"].createInstance(
    Ci.nsIWebSocketChannel
  );

  var uri = Services.io.newURI(url);
  chan.initLoadInfo(
    null, // aLoadingNode
    Services.scriptSecurityManager.createContentPrincipal(uri, {}),
    null, // aTriggeringPrincipal
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    Ci.nsIContentPolicy.TYPE_WEBSOCKET
  );

  await new Promise(resolve =>
    chan.asyncOpen(uri, url, {}, 0, new WSListener(resolve), null)
  );
});

add_task(async function test_dns_resolve_proxy() {
  dnsRequestObserver.register();

  let { error } = await new TRRDNSListener("dnsleak.example.com", {
    expectEarlyFail: true,
  });
  Assert.equal(
    error.result,
    Cr.NS_ERROR_UNKNOWN_PROXY_HOST,
    "error is NS_ERROR_UNKNOWN_PROXY_HOST"
  );
  dnsRequestObserver.unregister();
});
