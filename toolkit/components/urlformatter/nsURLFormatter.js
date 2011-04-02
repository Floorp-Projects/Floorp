# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the nsURLFormatterService.
#
# The Initial Developer of the Original Code is
# Axel Hecht <axel@mozilla.com>
# Portions created by the Initial Developer are Copyright (C) 2006
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Dietrich Ayala <dietrich@mozilla.com>
#   Marco Bonardo <mak77@bonardo.net>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
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

const PREF_APP_UPDATE_CHANNEL         = "app.update.channel";
const PREF_PARTNER_BRANCH             = "app.partner.";
const PREF_APP_DISTRIBUTION           = "distribution.id";
const PREF_APP_DISTRIBUTION_VERSION   = "distribution.version";

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

      // Mac universal build should report a different ABI than either macppc
      // or mactel.
      let macutils = Cc["@mozilla.org/xpcom/mac-utils;1"].
                      getService(Ci.nsIMacUtils);
      if (macutils && macutils.isUniversalBinary) {
        ABI = "Universal-gcc3";
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

  XPCOMUtils.defineLazyGetter(this, "updateChannel", function UFS_updateChannel() {
    // Read the update channel from defaults only.  We do this to ensure that
    // the channel is tightly coupled with the application and does not apply
    // to other instances of the application that may use the same profile.
    let channel = "default";
    let defaults = Services.prefs.getDefaultBranch(null);
    try {
      channel = defaults.getCharPref(PREF_APP_UPDATE_CHANNEL);
    } catch (e) {}

    try {
      let partners = Services.prefs.getChildList(PREF_PARTNER_BRANCH).sort();
      if (partners.length) {
        channel += "-cck";
        partners.forEach(function (prefName) {
          channel += "-" + Services.prefs.getCharPref(prefName);
        });
      }
    } catch (e) {}

    return channel;
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
    LOCALE:           function() Cc["@mozilla.org/chrome/chrome-registry;1"].
                                 getService(Ci.nsIXULChromeRegistry).
                                 getSelectedLocale('global'),
    VENDOR:           function() this.appInfo.vendor,
    NAME:             function() this.appInfo.name,
    ID:               function() this.appInfo.ID,
    VERSION:          function() this.appInfo.version,
    APPBUILDID:       function() this.appInfo.appBuildID,
    PLATFORMVERSION:  function() this.appInfo.platformVersion,
    PLATFORMBUILDID:  function() this.appInfo.platformBuildID,
    APP:              function() this.appInfo.name.toLowerCase().replace(/ /, ""),
    OS:               function() this.appInfo.OS,
    XPCOMABI:         function() this.ABI,
    BUILD_TARGET:     function() this.appInfo.OS + "_" + this.ABI,
    OS_VERSION:       function() this.OSVersion,
    CHANNEL:          function() this.updateChannel,
    DISTRIBUTION:     function() this.distribution.id,
    DISTRIBUTION_VERSION: function() this.distribution.version
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

var NSGetFactory = XPCOMUtils.generateNSGetFactory([nsURLFormatterService]);
