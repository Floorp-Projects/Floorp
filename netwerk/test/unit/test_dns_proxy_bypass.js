/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

Cu.import("resource://gre/modules/Services.jsm");

var ioService = Cc["@mozilla.org/network/io-service;1"].
  getService(Ci.nsIIOService);

var prefs = Cc["@mozilla.org/preferences-service;1"].
  getService(Ci.nsIPrefBranch);

var url = "ws://dnsleak.example.com";

var dnsRequestObserver = {
  register: function() {
    this.obs = Cc["@mozilla.org/observer-service;1"].
      getService(Ci.nsIObserverService);
    this.obs.addObserver(this, "dns-resolution-request");
  },

  unregister: function() {
    if (this.obs) {
      this.obs.removeObserver(this, "dns-resolution-request");
    }
  },

  observe: function(subject, topic, data) {
    if (topic == "dns-resolution-request") {
      do_print(data);
      if (data.indexOf("dnsleak.example.com") > -1) {
        try {
          do_check_true(false);
        } catch (e) {}
      }
    }
  }
};

var listener = {
  onAcknowledge: function(aContext, aSize) {},
  onBinaryMessageAvailable: function(aContext, aMsg) {},
  onMessageAvailable: function(aContext, aMsg) {},
  onServerClose: function(aContext, aCode, aReason) {},
  onStart: function(aContext) {},
  onStop: function(aContext, aStatusCode) {
    prefs.clearUserPref("network.proxy.socks");
    prefs.clearUserPref("network.proxy.socks_port");
    prefs.clearUserPref("network.proxy.type");
    prefs.clearUserPref("network.proxy.socks_remote_dns");
    prefs.clearUserPref("network.dns.notifyResolution");
    dnsRequestObserver.unregister();
    do_test_finished();
  }
};

function run_test() {
  dnsRequestObserver.register();
  prefs.setBoolPref("network.dns.notifyResolution", true);
  prefs.setCharPref("network.proxy.socks", "127.0.0.1");
  prefs.setIntPref("network.proxy.socks_port", 9000);
  prefs.setIntPref("network.proxy.type", 1);
  prefs.setBoolPref("network.proxy.socks_remote_dns", true);
  var chan = Cc["@mozilla.org/network/protocol;1?name=ws"].
    createInstance(Components.interfaces.nsIWebSocketChannel);

  chan.initLoadInfo(null, // aLoadingNode
                    Services.scriptSecurityManager.getSystemPrincipal(),
                    null, // aTriggeringPrincipal
                    Ci.nsILoadInfo.SEC_ALLOW_CROSS_ORIGIN_DATA_IS_NULL,
                    Ci.nsIContentPolicy.TYPE_WEBSOCKET);

  var uri = ioService.newURI(url);
  chan.asyncOpen(uri, url, 0, listener, null);
  do_test_pending();
}

