/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/NetUtil.jsm");

var ios = Cc["@mozilla.org/network/io-service;1"]
             .getService(Components.interfaces.nsIIOService);

var pps = Cc["@mozilla.org/network/protocol-proxy-service;1"].getService();

var prefs = Cc["@mozilla.org/preferences-service;1"]
               .getService(Components.interfaces.nsIPrefBranch);

var pgen = Cc["@mozilla.org/pac-generator;1"]
              .getService(Components.interfaces.nsIPACGenerator);

const TARGET_HOST ="www.mozilla.org";
const HTTP_HOST = "httpHost";
const HTTP_PORT = 1111;
const HTTPS_HOST = "httpsHost";
const HTTPS_PORT = 2222;
const FTP_HOST= "ftpHost";
const FTP_PORT = 3333;
const MY_APP_ID = 10;
const MY_APP_ORIGIN = "apps://browser.gaiamobile.com";
const APP_ORIGINS_LIST = "apps://test1.com, apps://browser.gaiamobile.com";
const BROWSING_HOST = "browsingHost";
const BROWSING_PORT = 4444;
const PROXY_TYPE_PAC = Ci.nsIProtocolProxyService.PROXYCONFIG_PAC;

const proxyTypes = {
  "http": {"pref": "http", "host": HTTP_HOST, "port": HTTP_PORT},
  "https": {"pref": "ssl", "host": HTTPS_HOST, "port": HTTPS_PORT},
  "ftp": {"pref": "ftp", "host": FTP_HOST, "port": FTP_PORT}
};

function default_proxy_settings() {
  prefs.setBoolPref("network.proxy.pac_generator", true);
  prefs.setIntPref("network.proxy.type", 0);
  prefs.setCharPref("network.proxy.autoconfig_url", "");
  for (let i in proxyTypes) {
    let p = proxyTypes[i];
    prefs.setCharPref("network.proxy." + p["pref"], p["host"]);
    prefs.setIntPref("network.proxy." + p["pref"] + "_port", p["port"]);
  }
}

function TestResolveCallback(type, host, callback) {
  this.type = type;
  this.host = host;
  this.callback = callback;
}
TestResolveCallback.prototype = {
  QueryInterface:
  function TestResolveCallback_QueryInterface(iid) {
    if (iid.equals(Components.interfaces.nsIProtocolProxyCallback) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onProxyAvailable:
  function TestResolveCallback_onProxyAvailable(req, channel, pi, status) {
    if (this.type) {
      // Check for localhost.
      if (this.host == "localhost" || this.host == "127.0.0.1") {
          do_check_eq(pi, null);
          this.callback();
          return;
      }

      // Check for browsing proxy.
      let browsingEnabled;
      try {
        browsingEnabled = prefs.getBoolPref("network.proxy.browsing.enabled");
      } catch (ex) {}

      if (browsingEnabled) {
        let proxyHost, proxyPort;
        try {
          proxyHost = prefs.getCharPref("network.proxy.browsing.host");
          proxyPort = prefs.getIntPref("network.proxy.browsing.port");
        } catch (ex) {}

        if (proxyHost) {
          do_check_eq(pi.host, proxyHost);
          do_check_eq(pi.port, proxyPort);
          this.callback();
          return;
        }
      }

      // Check for system proxy.
      let share;
      try {
        share = prefs.getBoolPref("network.proxy.share_proxy_settings");
      } catch (ex) {}

      let p = share ? proxyTypes["http"] : proxyTypes[this.type];
      if (p) {
        let proxyHost, proxyPort;
        try {
          proxyHost = prefs.getCharPref("network.proxy." + p["pref"]);
          proxyPort = prefs.getIntPref("network.proxy." + p["pref"] + "_port");
        } catch (ex) {}

        if (proxyHost) {
          // Connection through proxy.
          do_check_neq(pi, null);
          do_check_eq(pi.host, proxyHost);
          do_check_eq(pi.port, proxyPort);
        } else {
          // Direct connection.
          do_check_eq(pi, null);
        }
      }
    }

    this.callback();
  }
};

function test_resolve_type(type, host, callback) {
  // We have to setup a profile, otherwise indexed db used by webapps
  // will throw random exception when trying to get profile folder.
  do_get_profile();

  // We also need a valid nsIXulAppInfo service as Webapps.jsm is querying it.
  Cu.import("resource://testing-common/AppInfo.jsm");
  updateAppInfo();

  // Mock getAppByLocalId() to return testing app origin.
  Cu.import("resource://gre/modules/AppsUtils.jsm");
  AppsUtils.getAppByLocalId = function(aAppId) {
    let app = { origin: MY_APP_ORIGIN};
    return app;
  };

  let channel = NetUtil.newChannel({
    uri: type + "://" + host + "/",
    loadUsingSystemPrincipal: true
  });
  channel.loadInfo.originAttributes = { appId: MY_APP_ID,
                                        inIsolatedMozBrowser: true
                                      };

  let req = pps.asyncResolve(channel, 0, new TestResolveCallback(type, host, callback));
}

function test_resolve(host, callback) {
  test_resolve_type("http", host, function() {
    test_resolve_type("https", host, function() {
      test_resolve_type("ftp", host, run_next_test);
    });
  });
}

add_test(function test_localhost() {
  default_proxy_settings();
  prefs.setBoolPref("network.proxy.share_proxy_settings", true);
  Services.prefs.setCharPref("network.proxy.autoconfig_url", pgen.generate());
  Services.prefs.setIntPref("network.proxy.type", PROXY_TYPE_PAC);
  test_resolve("localhost", run_next_test);
});

add_test(function test_share_on_proxy() {
  default_proxy_settings();
  prefs.setBoolPref("network.proxy.share_proxy_settings", true);
  Services.prefs.setCharPref("network.proxy.autoconfig_url", pgen.generate());
  Services.prefs.setIntPref("network.proxy.type", PROXY_TYPE_PAC);
  test_resolve(TARGET_HOST, run_next_test);
});

add_test(function test_share_on_direct() {
  default_proxy_settings();
  prefs.setBoolPref("network.proxy.share_proxy_settings", true);
  prefs.setCharPref("network.proxy.http", "");
  prefs.setCharPref("network.proxy.ssl", "");
  prefs.setCharPref("network.proxy.ftp", "");
  Services.prefs.setCharPref("network.proxy.autoconfig_url", pgen.generate());
  Services.prefs.setIntPref("network.proxy.type", PROXY_TYPE_PAC);
  test_resolve(TARGET_HOST, run_next_test);
});

add_test(function test_share_off_proxy() {
  default_proxy_settings();
  prefs.setBoolPref("network.proxy.share_proxy_settings", false);
  Services.prefs.setCharPref("network.proxy.autoconfig_url", pgen.generate());
  Services.prefs.setIntPref("network.proxy.type", PROXY_TYPE_PAC);
  test_resolve(TARGET_HOST, run_next_test);
});

add_test(function test_share_off_direct() {
  default_proxy_settings();
  prefs.setBoolPref("network.proxy.share_proxy_settings", false);
  prefs.setCharPref("network.proxy.http", "");
  prefs.setCharPref("network.proxy.ssl", "");
  prefs.setCharPref("network.proxy.ftp", "");
  Services.prefs.setCharPref("network.proxy.autoconfig_url", pgen.generate());
  Services.prefs.setIntPref("network.proxy.type", PROXY_TYPE_PAC);
  test_resolve(TARGET_HOST, run_next_test);
});

add_test(function test_browsing_proxy() {
  default_proxy_settings();
  prefs.setBoolPref("network.proxy.browsing.enabled", true);
  prefs.setCharPref("network.proxy.browsing.host", BROWSING_HOST);
  prefs.setIntPref("network.proxy.browsing.port", BROWSING_PORT);
  prefs.setCharPref("network.proxy.browsing.app_origins", APP_ORIGINS_LIST);
  Services.prefs.setCharPref("network.proxy.autoconfig_url", pgen.generate());
  Services.prefs.setIntPref("network.proxy.type", PROXY_TYPE_PAC);
  test_resolve(TARGET_HOST, run_next_test);
});

function run_test(){
  do_register_cleanup(() => {
    prefs.clearUserPref("network.proxy.pac_generator");
    prefs.clearUserPref("network.proxy.type");
    prefs.clearUserPref("network.proxy.autoconfig_url");
    prefs.clearUserPref("network.proxy.share_proxy_settings");
    prefs.clearUserPref("network.proxy.http");
    prefs.clearUserPref("network.proxy.http_port");
    prefs.clearUserPref("network.proxy.ssl");
    prefs.clearUserPref("network.proxy.ssl_port");
    prefs.clearUserPref("network.proxy.ftp");
    prefs.clearUserPref("network.proxy.ftp_port");
    prefs.clearUserPref("network.proxy.browsing.enabled");
    prefs.clearUserPref("network.proxy.browsing.host");
    prefs.clearUserPref("network.proxy.browsing.port");
    prefs.clearUserPref("network.proxy.browsing.app_origins");
  });
  run_next_test();
}
