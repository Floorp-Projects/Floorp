#filter substitution

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

 /**
 * @class nsURLFormatterService
 *
 * nsURLFormatterService exposes methods to substitute variables in URL formats.
 *
 * Mozilla Applications linking to Mozilla websites are strongly encouraged to use
 * URLs of the following format:
 *
 *   http[s]://%SERVICE%.mozilla.[com|org]/%LOCALE%/
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

const PREF_APP_DISTRIBUTION           = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION   = "distribution.version";

XPCOMUtils.defineLazyModuleGetter(this, "UpdateUtils",
                                  "resource://gre/modules/UpdateUtils.jsm");

function nsURLFormatterService() {
  XPCOMUtils.defineLazyGetter(this, "appInfo", function UFS_appInfo() {
    return Cc["@mozilla.org/xre/app-info;1"].
           getService(Ci.nsIXULAppInfo).
           QueryInterface(Ci.nsIXULRuntime);
  });

  XPCOMUtils.defineLazyGetter(this, "ABI", function UFS_ABI() {
    let ABI = "default";
    try {
      ABI = this.appInfo.XPCOMABI;

      if ("@mozilla.org/xpcom/mac-utils;1" in Cc) {
        // Mac universal build should report a different ABI than either macppc
        // or mactel.
        let macutils = Cc["@mozilla.org/xpcom/mac-utils;1"]
                         .getService(Ci.nsIMacUtils);
        if (macutils && macutils.isUniversalBinary) {
          ABI = "Universal-gcc3";
        }
      }
    } catch (e) {}

    return ABI;
  });

  XPCOMUtils.defineLazyGetter(this, "OSVersion", function UFS_OSVersion() {
    let OSVersion = "default";
    let sysInfo = Cc["@mozilla.org/system-info;1"].
                  getService(Ci.nsIPropertyBag2);
    try {
      OSVersion = sysInfo.getProperty("name") + " " +
                  sysInfo.getProperty("version");
      OSVersion += " (" + sysInfo.getProperty("secondaryLibrary") + ")";
    } catch (e) {}

    return encodeURIComponent(OSVersion);
  });

  XPCOMUtils.defineLazyGetter(this, "distribution", function UFS_distribution() {
    let distribution = { id: "default", version: "default" };

    let defaults = Services.prefs.getDefaultBranch(null);
    try {
      distribution.id = defaults.getCharPref(PREF_APP_DISTRIBUTION);
    } catch (e) {}
    try {
      distribution.version = defaults.getCharPref(PREF_APP_DISTRIBUTION_VERSION);
    } catch (e) {}

    return distribution;
  });
}

nsURLFormatterService.prototype = {
  classID: Components.ID("{e6156350-2be8-11db-a98b-0800200c9a66}"),
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIURLFormatter]),

  _defaults: {
    LOCALE:           () => Cc["@mozilla.org/chrome/chrome-registry;1"].
                            getService(Ci.nsIXULChromeRegistry).
                            getSelectedLocale('global'),
    REGION:           function() {
      try {
        // When the geoip lookup failed to identify the region, we fallback to
        // the 'ZZ' region code to mean 'unknown'.
        return Services.prefs.getCharPref("browser.search.region") || "ZZ";
      } catch(e) {
        return "ZZ";
      }
    },
    VENDOR:           function() { return this.appInfo.vendor; },
    NAME:             function() { return this.appInfo.name; },
    ID:               function() { return this.appInfo.ID; },
    VERSION:          function() { return this.appInfo.version; },
    APPBUILDID:       function() { return this.appInfo.appBuildID; },
    PLATFORMVERSION:  function() { return this.appInfo.platformVersion; },
    PLATFORMBUILDID:  function() { return this.appInfo.platformBuildID; },
    APP:              function() { return this.appInfo.name.toLowerCase().replace(/ /, ""); },
    OS:               function() { return this.appInfo.OS; },
    XPCOMABI:         function() { return this.ABI; },
    BUILD_TARGET:     function() { return this.appInfo.OS + "_" + this.ABI; },
    OS_VERSION:       function() { return this.OSVersion; },
    CHANNEL:          () => UpdateUtils.UpdateChannel,
    MOZILLA_API_KEY:  () => "@MOZ_MOZILLA_API_KEY@",
    GOOGLE_API_KEY:   () => "@MOZ_GOOGLE_API_KEY@",
    GOOGLE_OAUTH_API_CLIENTID:() => "@MOZ_GOOGLE_OAUTH_API_CLIENTID@",
    GOOGLE_OAUTH_API_KEY:     () => "@MOZ_GOOGLE_OAUTH_API_KEY@",
    BING_API_CLIENTID:() => "@MOZ_BING_API_CLIENTID@",
    BING_API_KEY:     () => "@MOZ_BING_API_KEY@",
    DISTRIBUTION:     function() { return this.distribution.id; },
    DISTRIBUTION_VERSION: function() { return this.distribution.version; }
  },

  formatURL: function uf_formatURL(aFormat) {
    var _this = this;
    var replacementCallback = function(aMatch, aKey) {
      if (aKey in _this._defaults) {
        return _this._defaults[aKey].call(_this);
      }
      Cu.reportError("formatURL: Couldn't find value for key: " + aKey);
      return aMatch;
    }
    return aFormat.replace(/%([A-Z_]+)%/g, replacementCallback);
  },

  formatURLPref: function uf_formatURLPref(aPref) {
    var format = null;
    var PS = Cc['@mozilla.org/preferences-service;1'].
             getService(Ci.nsIPrefBranch);

    try {
      format = PS.getComplexValue(aPref, Ci.nsISupportsString).data;
    } catch(ex) {
      Cu.reportError("formatURLPref: Couldn't get pref: " + aPref);
      return "about:blank";
    }

    if (!PS.prefHasUserValue(aPref) &&
        /^(data:text\/plain,.+=.+|chrome:\/\/.+\/locale\/.+\.properties)$/.test(format)) {
      // This looks as if it might be a localised preference
      try {
        format = PS.getComplexValue(aPref, Ci.nsIPrefLocalizedString).data;
      } catch(ex) {}
    }

    return this.formatURL(format);
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsURLFormatterService]);
