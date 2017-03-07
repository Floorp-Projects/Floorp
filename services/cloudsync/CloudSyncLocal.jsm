/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["Local"];

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://services-common/utils.js");
Cu.import("resource://services-crypto/utils.js");
Cu.import("resource://gre/modules/Preferences.jsm");

function lazyStrings(name) {
  return () => Services.strings.createBundle(
    `chrome://weave/locale/services//${name}.properties`);
}

this.Str = {};
XPCOMUtils.defineLazyGetter(Str, "errors", lazyStrings("errors"));
XPCOMUtils.defineLazyGetter(Str, "sync", lazyStrings("sync"));

function makeGUID() {
  return CommonUtils.encodeBase64URL(CryptoUtils.generateRandomBytes(9));
}

this.Local = function() {
  let prefs = new Preferences("services.cloudsync.");
  this.__defineGetter__("prefs", function() {
    return prefs;
  });
};

Local.prototype = {
  get id() {
    let clientId = this.prefs.get("client.GUID", "");
    return clientId == "" ? this.id = makeGUID() : clientId;
  },

  set id(value) {
    this.prefs.set("client.GUID", value);
  },

  get name() {
    let clientName = this.prefs.get("client.name", "");

    if (clientName != "") {
      return clientName;
    }

    // Generate a client name if we don't have a useful one yet
    let env = Cc["@mozilla.org/process/environment;1"]
                .getService(Ci.nsIEnvironment);
    let user = env.get("USER") || env.get("USERNAME");
    let appName;
    let brand = Services.strings.createBundle(
      "chrome://branding/locale/brand.properties");
    let brandName = brand.GetStringFromName("brandShortName");

    try {
      let syncStrings = Services.strings.createBundle("chrome://browser/locale/sync.properties");
      appName = syncStrings.formatStringFromName("sync.defaultAccountApplication", [brandName], 1);
    } catch (ex) {
    }

    appName = appName || brandName;

    let system =
      // 'device' is defined on unix systems
      Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2).get("device") ||
      // hostname of the system, usually assigned by the user or admin
      Cc["@mozilla.org/system-info;1"].getService(Ci.nsIPropertyBag2).get("host") ||
      // fall back on ua info string
      Cc["@mozilla.org/network/protocol;1?name=http"].getService(Ci.nsIHttpProtocolHandler).oscpu;

    return this.name = Str.sync.formatStringFromName("client.name2", [user, appName, system], 3);
  },

  set name(value) {
    this.prefs.set("client.name", value);
  },
};

