/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');
Cu.import('resource://gre/modules/Services.jsm');

var DEBUG = false;

if (DEBUG) {
  debug = function (s) { dump("-*- PACGenerator: " + s + "\n"); };
}
else {
  debug = function (s) {};
}

const PACGENERATOR_CONTRACTID = "@mozilla.org/pac-generator;1";
const PACGENERATOR_CID = Components.ID("{788507c4-eb5f-4de8-b19b-e0d531974e8a}");

//
// RFC 2396 section 3.2.2:
//
// host = hostname | IPv4address
// hostname = *( domainlabel "." ) toplabel [ "." ]
// domainlabel = alphanum | alphanum *( alphanum | "-" ) alphanum
// toplabel = alpha | alpha *( alphanum | "-" ) alphanum
// IPv4address = 1*digit "." 1*digit "." 1*digit "." 1*digit
//
const HOST_REGEX =
  new RegExp("^(?:" +
               // *( domainlabel "." )
               "(?:[a-z0-9](?:[a-z0-9-]*[a-z0-9])?\\.)*" +
               // toplabel
               "[a-z](?:[a-z0-9-]*[a-z0-9])?" +
             "|" +
               // IPv4 address
               "\\d+\\.\\d+\\.\\d+\\.\\d+" +
             ")$",
             "i");

function PACGenerator() {
  debug("Starting PAC Generator service.");
}

PACGenerator.prototype = {

  classID : PACGENERATOR_CID,

  QueryInterface : XPCOMUtils.generateQI([Ci.nsIPACGenerator]),

  classInfo : XPCOMUtils.generateCI({classID: PACGENERATOR_CID,
                                     contractID: PACGENERATOR_CONTRACTID,
                                     classDescription: "PACGenerator",
                                     interfaces: [Ci.nsIPACGenerator]}),

  /**
   * Validate the the host.
   */
  isValidHost: function isValidHost(host) {
    if (!HOST_REGEX.test(host)) {
      debug("Unexpected host: '" + host + "'");
      return false;
    }
    return true;
  },

  /**
   * Returns a PAC string based on proxy settings in the preference.
   * Only effective when the network.proxy.pac_generator preference is true.
   */
  generate: function generate() {
    let enabled, host, port, proxy;

    try {
      enabled = Services.prefs.getBoolPref("network.proxy.pac_generator");
    } catch (ex) {}
    if (!enabled) {
      debug("PAC Generator disabled.");
      return "";
    }

    let pac = "data:text/plain,function FindProxyForURL(url, host) { ";

    // Direct connection for localhost.
    pac += "if (shExpMatch(host, 'localhost') || " +
           "shExpMatch(host, '127.0.0.1')) {" +
           " return 'DIRECT'; } ";

    // Rules for browsing proxy.
    try {
      enabled = Services.prefs.getBoolPref("network.proxy.browsing.enabled");
      host = Services.prefs.getCharPref("network.proxy.browsing.host");
      port = Services.prefs.getIntPref("network.proxy.browsing.port");
    } catch (ex) {}

    if (enabled && host && this.isValidHost(host)) {
      proxy = host + ":" + ((port && port !== 0) ? port : 8080);
      let appOrigins;
      try {
        appOrigins = Services.prefs.getCharPref("network.proxy.browsing.app_origins");
      } catch (ex) {}

      pac += "var origins ='" + appOrigins +
             "'.split(/[ ,]+/).filter(Boolean); " +
             "if ((origins.indexOf('*') > -1 || origins.indexOf(myAppOrigin()) > -1)" +
             " && isInIsolatedMozBrowser()) { return 'PROXY " + proxy + "'; } ";
    }

    // Rules for system proxy.
    let share;
    try {
      share = Services.prefs.getBoolPref("network.proxy.share_proxy_settings");
    } catch (ex) {}

    if (share) {
      // Add rules for all protocols.
      try {
        host = Services.prefs.getCharPref("network.proxy.http");
        port = Services.prefs.getIntPref("network.proxy.http_port");
      } catch (ex) {}

      if (host && this.isValidHost(host)) {
        proxy = host + ":" + ((port && port !== 0) ? port : 8080);
        pac += "return 'PROXY " + proxy + "'; "
      } else {
        pac += "return 'DIRECT'; ";
      }
    } else {
      // Add rules for specific protocols.
      const proxyTypes = [
        {"scheme": "http:", "pref": "http"},
        {"scheme": "https:", "pref": "ssl"},
        {"scheme": "ftp:", "pref": "ftp"}
      ];
      for (let i = 0; i < proxyTypes.length; i++) {
       try {
          host = Services.prefs.getCharPref("network.proxy." +
                                            proxyTypes[i]["pref"]);
          port = Services.prefs.getIntPref("network.proxy." +
                                            proxyTypes[i]["pref"] + "_port");
        } catch (ex) {}

        if (host && this.isValidHost(host)) {
          proxy = host + ":" + (port === 0 ? 8080 : port);
          pac += "if (url.substring(0, " + (proxyTypes[i]["scheme"]).length +
                 ") == '" + proxyTypes[i]["scheme"] + "') { return 'PROXY " +
                 proxy + "'; } ";
         }
      }
      pac += "return 'DIRECT'; ";
    }

    pac += "}";

    debug("PAC: " + pac);

    return pac;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([PACGenerator]);
