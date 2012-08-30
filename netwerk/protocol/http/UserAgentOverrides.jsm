/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = [ "UserAgentOverrides" ];

const Ci = Components.interfaces;
const Cc = Components.classes;

Components.utils.import("resource://gre/modules/Services.jsm");

const PREF_OVERRIDES_ENABLED = "general.useragent.site_specific_overrides";

var gPrefBranch;
var gOverrides;
var gInitialized = false;

var UserAgentOverrides = {
  init: function uao_init() {
    if (gInitialized)
      return;
    gInitialized = true;

    gPrefBranch = Services.prefs.getBranch("general.useragent.override.");
    gPrefBranch.addObserver("", buildOverrides, false);

    Services.prefs.addObserver(PREF_OVERRIDES_ENABLED, buildOverrides, false);

    Services.obs.addObserver(HTTP_on_modify_request, "http-on-modify-request", false);

    buildOverrides();
  },

  uninit: function uao_uninit() {
    if (!gInitialized)
      return;
    gInitialized = false;

    gPrefBranch.removeObserver("", buildOverrides);

    Services.prefs.removeObserver(PREF_OVERRIDES_ENABLED, buildOverrides);

    Services.obs.removeObserver(HTTP_on_modify_request, "http-on-modify-request");
  }
};

function buildOverrides() {
  gOverrides = {};

  if (!Services.prefs.getBoolPref(PREF_OVERRIDES_ENABLED))
    return;

  const defaultUA = Cc["@mozilla.org/network/protocol;1?name=http"]
                      .getService(Ci.nsIHttpProtocolHandler)
                      .userAgent;
  let domains = gPrefBranch.getChildList("");

  for (let domain of domains) {
    let override = gPrefBranch.getCharPref(domain);

    let [search, replace] = override.split("#", 2);
    if (search && replace) {
      gOverrides[domain] = defaultUA.replace(new RegExp(search, "g"), replace);
    } else {
      gOverrides[domain] = override;
    }
  }
}

function HTTP_on_modify_request(aSubject, aTopic, aData) {
  let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);
  let host = channel.URI.asciiHost;

  for (let domain in gOverrides) {
    if (host == domain ||
        host.endsWith("." + domain)) {
      channel.setRequestHeader("User-Agent", gOverrides[domain], false);
      break;
    }
  }
}
