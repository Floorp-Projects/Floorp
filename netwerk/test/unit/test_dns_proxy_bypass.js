/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var ioService = Cc["@mozilla.org/network/io-service;1"].getService(
  Ci.nsIIOService
);

var prefs = Cc["@mozilla.org/preferences-service;1"].getService(
  Ci.nsIPrefBranch
);

var url = "ws://dnsleak.example.com";

var dnsRequestObserver = {
  register() {
    this.obs = Cc["@mozilla.org/observer-service;1"].getService(
      Ci.nsIObserverService
    );
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

var listener = {
  onAcknowledge(aContext, aSize) {},
  onBinaryMessageAvailable(aContext, aMsg) {},
  onMessageAvailable(aContext, aMsg) {},
  onServerClose(aContext, aCode, aReason) {},
  onStart(aContext) {},
  onStop(aContext, aStatusCode) {
    prefs.clearUserPref("network.proxy.socks");
    prefs.clearUserPref("network.proxy.socks_port");
    prefs.clearUserPref("network.proxy.type");
    prefs.clearUserPref("network.proxy.socks_remote_dns");
    prefs.clearUserPref("network.dns.notifyResolution");
    dnsRequestObserver.unregister();
    do_test_finished();
  },
};

function run_test() {
  dnsRequestObserver.register();
  prefs.setBoolPref("network.dns.notifyResolution", true);
  prefs.setCharPref("network.proxy.socks", "127.0.0.1");
  prefs.setIntPref("network.proxy.socks_port", 9000);
  prefs.setIntPref("network.proxy.type", 1);
  prefs.setBoolPref("network.proxy.socks_remote_dns", true);
  var chan = Cc["@mozilla.org/network/protocol;1?name=ws"].createInstance(
    Ci.nsIWebSocketChannel
  );

  var uri = ioService.newURI(url);
  chan.initLoadInfo(
    null, // aLoadingNode
    Services.scriptSecurityManager.createContentPrincipal(uri, {}),
    null, // aTriggeringPrincipal
    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_SEC_CONTEXT_IS_NULL,
    Ci.nsIContentPolicy.TYPE_WEBSOCKET
  );

  chan.asyncOpen(uri, url, {}, 0, listener, null);
  do_test_pending();
}
