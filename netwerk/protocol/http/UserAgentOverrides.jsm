/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = [ "UserAgentOverrides" ];

const Ci = Components.interfaces;
const Cc = Components.classes;

Components.utils.import("resource://gre/modules/Services.jsm");
Components.utils.import("resource://gre/modules/UserAgentUpdates.jsm");

const PREF_OVERRIDES_ENABLED = "general.useragent.site_specific_overrides";
const DEFAULT_UA = Cc["@mozilla.org/network/protocol;1?name=http"]
                     .getService(Ci.nsIHttpProtocolHandler)
                     .userAgent;
const MAX_OVERRIDE_FOR_HOST_CACHE_SIZE = 250;

var gPrefBranch;
var gOverrides = new Map;
var gUpdatedOverrides;
var gOverrideForHostCache = new Map;
var gInitialized = false;
var gOverrideFunctions = [
  function (aHttpChannel) UserAgentOverrides.getOverrideForURI(aHttpChannel.URI)
];

this.UserAgentOverrides = {
  init: function uao_init() {
    if (gInitialized)
      return;

    gPrefBranch = Services.prefs.getBranch("general.useragent.override.");
    gPrefBranch.addObserver("", buildOverrides, false);

    Services.prefs.addObserver(PREF_OVERRIDES_ENABLED, buildOverrides, false);

    try {
      Services.obs.addObserver(HTTP_on_modify_request, "http-on-modify-request", false);
    } catch (x) {
      // The http-on-modify-request notification is disallowed in content processes.
    }

    UserAgentUpdates.init(function(overrides) {
      gOverrideForHostCache.clear();
      if (overrides) {
        overrides.get = function(key) this[key];
      }
      gUpdatedOverrides = overrides;
    });

    buildOverrides();
    gInitialized = true;
  },

  addComplexOverride: function uao_addComplexOverride(callback) {
    gOverrideFunctions.push(callback);
  },

  getOverrideForURI: function uao_getOverrideForURI(aURI) {
    if (!gInitialized ||
        (!gOverrides.size && !gUpdatedOverrides) ||
        !(aURI instanceof Ci.nsIStandardURL))
      return null;

    let host = aURI.asciiHost;

    let override = gOverrideForHostCache.get(host);
    if (override !== undefined)
      return override;

    function findOverride(overrides) {
      let searchHost = host;
      let userAgent = overrides.get(searchHost);

      while (!userAgent) {
        let dot = searchHost.indexOf('.');
        if (dot === -1) {
          return null;
        }
        searchHost = searchHost.slice(dot + 1);
        userAgent = overrides.get(searchHost);
      }
      return userAgent;
    }

    override = (gOverrides.size && findOverride(gOverrides))
            || (gUpdatedOverrides && findOverride(gUpdatedOverrides));

    if (gOverrideForHostCache.size >= MAX_OVERRIDE_FOR_HOST_CACHE_SIZE) {
      gOverrideForHostCache.clear();
    }
    gOverrideForHostCache.set(host, override);

    return override;
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
  gOverrides.clear();
  gOverrideForHostCache.clear();

  if (!Services.prefs.getBoolPref(PREF_OVERRIDES_ENABLED))
    return;

  let builtUAs = new Map;
  let domains = gPrefBranch.getChildList("");

  for (let domain of domains) {
    let override = gPrefBranch.getCharPref(domain);
    let userAgent = builtUAs.get(override);

    if (userAgent === undefined) {
      let [search, replace] = override.split("#", 2);
      if (search && replace) {
        userAgent = DEFAULT_UA.replace(new RegExp(search, "g"), replace);
      } else {
        userAgent = override;
      }
      builtUAs.set(override, userAgent);
    }

    if (userAgent != DEFAULT_UA) {
      gOverrides.set(domain, userAgent);
    }
  }
}

function HTTP_on_modify_request(aSubject, aTopic, aData) {
  let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);

  for (let callback of gOverrideFunctions) {
    let modifiedUA = callback(channel, DEFAULT_UA);
    if (modifiedUA) {
      channel.setRequestHeader("User-Agent", modifiedUA, false);
      return;
    }
  }
}
