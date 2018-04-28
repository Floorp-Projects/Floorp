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

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/AppConstants.jsm");

const PREF_APP_DISTRIBUTION           = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION   = "distribution.version";

ChromeUtils.defineModuleGetter(this, "UpdateUtils",
                               "resource://gre/modules/UpdateUtils.jsm");

function nsURLFormatterService() {
  XPCOMUtils.defineLazyGetter(this, "ABI", function UFS_ABI() {
    let ABI = "default";
    try {
      ABI = Services.appinfo.XPCOMABI;

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
    let defaults = Services.prefs.getDefaultBranch(null);
    let id = defaults.getCharPref(PREF_APP_DISTRIBUTION, "default");
    let version = defaults.getCharPref(PREF_APP_DISTRIBUTION_VERSION, "default");

    return {id, version};
  });
}

nsURLFormatterService.prototype = {
  classID: Components.ID("{e6156350-2be8-11db-a98b-0800200c9a66}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIURLFormatter]),

  _defaults: {
    LOCALE:           () => Services.locale.getAppLocaleAsLangTag(),
    REGION() {
      try {
        // When the geoip lookup failed to identify the region, we fallback to
        // the 'ZZ' region code to mean 'unknown'.
        return Services.prefs.getCharPref("browser.search.region") || "ZZ";
      } catch (e) {
        return "ZZ";
      }
    },
    VENDOR() { return Services.appinfo.vendor; },
    NAME() { return Services.appinfo.name; },
    ID() { return Services.appinfo.ID; },
    VERSION() { return Services.appinfo.version; },
    MAJOR_VERSION() { return Services.appinfo.version.replace(/^([^\.]+\.[0-9]+[a-z]*).*/gi, "$1"); },
    APPBUILDID() { return Services.appinfo.appBuildID; },
    PLATFORMVERSION() { return Services.appinfo.platformVersion; },
    PLATFORMBUILDID() { return Services.appinfo.platformBuildID; },
    APP() { return Services.appinfo.name.toLowerCase().replace(/ /, ""); },
    OS() { return Services.appinfo.OS; },
    XPCOMABI() { return this.ABI; },
    BUILD_TARGET() { return Services.appinfo.OS + "_" + this.ABI; },
    OS_VERSION() { return this.OSVersion; },
    CHANNEL:          () => UpdateUtils.UpdateChannel,
    MOZILLA_API_KEY:  () => AppConstants.MOZ_MOZILLA_API_KEY,
    GOOGLE_API_KEY:   () => AppConstants.MOZ_GOOGLE_API_KEY,
    BING_API_CLIENTID: () => AppConstants.MOZ_BING_API_CLIENTID,
    BING_API_KEY:     () => AppConstants.MOZ_BING_API_KEY,
    DISTRIBUTION() { return this.distribution.id; },
    DISTRIBUTION_VERSION() { return this.distribution.version; }
  },

  formatURL: function uf_formatURL(aFormat) {
    var _this = this;
    var replacementCallback = function(aMatch, aKey) {
      if (aKey in _this._defaults) {
        return _this._defaults[aKey].call(_this);
      }
      Cu.reportError("formatURL: Couldn't find value for key: " + aKey);
      return aMatch;
    };
    return aFormat.replace(/%([A-Z_]+)%/g, replacementCallback);
  },

  formatURLPref: function uf_formatURLPref(aPref) {
    var format = null;

    try {
      format = Services.prefs.getStringPref(aPref);
    } catch (ex) {
      Cu.reportError("formatURLPref: Couldn't get pref: " + aPref);
      return "about:blank";
    }

    if (!Services.prefs.prefHasUserValue(aPref) &&
        /^(data:text\/plain,.+=.+|chrome:\/\/.+\/locale\/.+\.properties)$/.test(format)) {
      // This looks as if it might be a localised preference
      try {
        format = Services.prefs.getComplexValue(aPref, Ci.nsIPrefLocalizedString).data;
      } catch (ex) {}
    }

    return this.formatURL(format);
  },

  trimSensitiveURLs: function uf_trimSensitiveURLs(aMsg) {
    // Only the google API key is sensitive for now.
    return AppConstants.MOZ_GOOGLE_API_KEY ? aMsg.replace(RegExp(AppConstants.MOZ_GOOGLE_API_KEY, "g"),
                                                 "[trimmed-google-api-key]")
                                  : aMsg;
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsURLFormatterService]);
