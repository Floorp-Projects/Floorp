/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["UserAgentOverrides"];

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { UserAgentUpdates } = ChromeUtils.import(
  "resource://gre/modules/UserAgentUpdates.jsm"
);

const PREF_OVERRIDES_ENABLED = "general.useragent.site_specific_overrides";
const MAX_OVERRIDE_FOR_HOST_CACHE_SIZE = 250;

// lazy load nsHttpHandler to improve startup performance.
XPCOMUtils.defineLazyGetter(this, "DEFAULT_UA", function() {
  return Cc["@mozilla.org/network/protocol;1?name=http"].getService(
    Ci.nsIHttpProtocolHandler
  ).userAgent;
});

var gPrefBranch;
var gOverrides = new Map();
var gUpdatedOverrides;
var gOverrideForHostCache = new Map();
var gInitialized = false;
var gOverrideFunctions = [
  function(aHttpChannel) {
    return UserAgentOverrides.getOverrideForURI(aHttpChannel.URI);
  },
];
var gBuiltUAs = new Map();

var UserAgentOverrides = {
  init: function uao_init() {
    if (gInitialized) {
      return;
    }

    gPrefBranch = Services.prefs.getBranch("general.useragent.override.");
    gPrefBranch.addObserver("", buildOverrides);

    Services.prefs.addObserver(PREF_OVERRIDES_ENABLED, buildOverrides);

    try {
      Services.obs.addObserver(
        HTTP_on_useragent_request,
        "http-on-useragent-request"
      );
    } catch (x) {
      // The http-on-useragent-request notification is disallowed in content processes.
    }

    try {
      UserAgentUpdates.init(function(overrides) {
        gOverrideForHostCache.clear();
        if (overrides) {
          for (let domain in overrides) {
            overrides[domain] = getUserAgentFromOverride(overrides[domain]);
          }
          overrides.get = function(key) {
            return this[key];
          };
        }
        gUpdatedOverrides = overrides;
      });

      buildOverrides();
    } catch (e) {
      // UserAgentOverrides is initialized before profile is ready.
      // UA override might not work correctly.
    }

    Services.obs.notifyObservers(null, "useragentoverrides-initialized");
    gInitialized = true;
  },

  addComplexOverride: function uao_addComplexOverride(callback) {
    // Add to front of array so complex overrides have precedence
    gOverrideFunctions.unshift(callback);
  },

  getOverrideForURI: function uao_getOverrideForURI(aURI) {
    let host = aURI.asciiHost;
    if (!gInitialized || (!gOverrides.size && !gUpdatedOverrides) || !host) {
      return null;
    }

    let override = gOverrideForHostCache.get(host);
    if (override !== undefined) {
      return override;
    }

    function findOverride(overrides) {
      let searchHost = host;
      let userAgent = overrides.get(searchHost);

      while (!userAgent) {
        let dot = searchHost.indexOf(".");
        if (dot === -1) {
          return null;
        }
        searchHost = searchHost.slice(dot + 1);
        userAgent = overrides.get(searchHost);
      }
      return userAgent;
    }

    override =
      (gOverrides.size && findOverride(gOverrides)) ||
      (gUpdatedOverrides && findOverride(gUpdatedOverrides));

    if (gOverrideForHostCache.size >= MAX_OVERRIDE_FOR_HOST_CACHE_SIZE) {
      gOverrideForHostCache.clear();
    }
    gOverrideForHostCache.set(host, override);

    return override;
  },

  uninit: function uao_uninit() {
    if (!gInitialized) {
      return;
    }
    gInitialized = false;

    gPrefBranch.removeObserver("", buildOverrides);

    Services.prefs.removeObserver(PREF_OVERRIDES_ENABLED, buildOverrides);

    Services.obs.removeObserver(
      HTTP_on_useragent_request,
      "http-on-useragent-request"
    );
  },
};

function getUserAgentFromOverride(override) {
  let userAgent = gBuiltUAs.get(override);
  if (userAgent !== undefined) {
    return userAgent;
  }
  let [search, replace] = override.split("#", 2);
  if (search && replace) {
    userAgent = DEFAULT_UA.replace(new RegExp(search, "g"), replace);
  } else {
    userAgent = override;
  }
  gBuiltUAs.set(override, userAgent);
  return userAgent;
}

function buildOverrides() {
  gOverrides.clear();
  gOverrideForHostCache.clear();

  if (!Services.prefs.getBoolPref(PREF_OVERRIDES_ENABLED)) {
    return;
  }

  let domains = gPrefBranch.getChildList("");

  for (let domain of domains) {
    let override = gPrefBranch.getCharPref(domain);
    let userAgent = getUserAgentFromOverride(override);

    if (userAgent != DEFAULT_UA) {
      gOverrides.set(domain, userAgent);
    }
  }
}

function HTTP_on_useragent_request(aSubject, aTopic, aData) {
  let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);

  for (let callback of gOverrideFunctions) {
    let modifiedUA = callback(channel, DEFAULT_UA);
    if (modifiedUA) {
      channel.setRequestHeader("User-Agent", modifiedUA, false);
      return;
    }
  }
}
