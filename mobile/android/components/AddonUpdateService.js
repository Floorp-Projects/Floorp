/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyGetter(this, "AddonManager", function() {
  Components.utils.import("resource://gre/modules/AddonManager.jsm");
  return AddonManager;
});

XPCOMUtils.defineLazyGetter(this, "AddonRepository", function() {
  Components.utils.import("resource://gre/modules/AddonRepository.jsm");
  return AddonRepository;
});

XPCOMUtils.defineLazyGetter(this, "NetUtil", function() {
  Components.utils.import("resource://gre/modules/NetUtil.jsm");
  return NetUtil;
});


function getPref(func, preference, defaultValue) {
  try {
    return Services.prefs[func](preference);
  }
  catch (e) {}
  return defaultValue;
}

// -----------------------------------------------------------------------
// Add-on auto-update management service
// -----------------------------------------------------------------------

const PREF_ADDON_UPDATE_ENABLED  = "extensions.autoupdate.enabled";

var gNeedsRestart = false;

function AddonUpdateService() {}

AddonUpdateService.prototype = {
  classDescription: "Add-on auto-update management",
  classID: Components.ID("{93c8824c-9b87-45ae-bc90-5b82a1e4d877}"),
  
  QueryInterface: XPCOMUtils.generateQI([Ci.nsITimerCallback]),

  notify: function aus_notify(aTimer) {
    if (aTimer && !getPref("getBoolPref", PREF_ADDON_UPDATE_ENABLED, true))
      return;

    // If we already auto-upgraded and installed new versions, ignore this check
    if (gNeedsRestart)
      return;

    Services.io.offline = false;

    // Assume we are doing a periodic update check
    let reason = AddonManager.UPDATE_WHEN_PERIODIC_UPDATE;
    if (!aTimer)
      reason = AddonManager.UPDATE_WHEN_USER_REQUESTED;

    AddonManager.getAddonsByTypes(null, function(aAddonList) {
      aAddonList.forEach(function(aAddon) {
        if (aAddon.permissions & AddonManager.PERM_CAN_UPGRADE) {
          let data = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
          data.data = JSON.stringify({ id: aAddon.id, name: aAddon.name });
          Services.obs.notifyObservers(data, "addon-update-started", null);

          let listener = new UpdateCheckListener();
          aAddon.findUpdates(listener, reason);
        }
      });
    });

    RecommendedSearchResults.search();
  }
};

// -----------------------------------------------------------------------
// Add-on update listener. Starts a download for any add-on with a viable
// update waiting
// -----------------------------------------------------------------------

function UpdateCheckListener() {
  this._status = null;
  this._version = null;
}

UpdateCheckListener.prototype = {
  onCompatibilityUpdateAvailable: function(aAddon) {
    this._status = "compatibility";
  },

  onUpdateAvailable: function(aAddon, aInstall) {
    this._status = "update";
    this._version = aInstall.version;
    aInstall.install();
  },

  onNoUpdateAvailable: function(aAddon) {
    if (!this._status)
      this._status = "no-update";
  },

  onUpdateFinished: function(aAddon, aError) {
    let data = Cc["@mozilla.org/supports-string;1"].createInstance(Ci.nsISupportsString);
    if (this._version)
      data.data = JSON.stringify({ id: aAddon.id, name: aAddon.name, version: this._version });
    else
      data.data = JSON.stringify({ id: aAddon.id, name: aAddon.name });

    if (aError)
      this._status = "error";

    Services.obs.notifyObservers(data, "addon-update-ended", this._status);
  }
};

// -----------------------------------------------------------------------
// RecommendedSearchResults fetches add-on data and saves it to a cache
// -----------------------------------------------------------------------

var RecommendedSearchResults = {
  _getFile: function() {
    let dirService = Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
    let file = dirService.get("ProfD", Ci.nsILocalFile);
    file.append("recommended-addons.json");
    return file;
  },

  _writeFile: function (aFile, aData) {
    if (!aData)
      return;

    // Initialize the file output stream.
    let ostream = Cc["@mozilla.org/network/safe-file-output-stream;1"].createInstance(Ci.nsIFileOutputStream);
    ostream.init(aFile, 0x02 | 0x08 | 0x20, 0600, ostream.DEFER_OPEN);

    // Obtain a converter to convert our data to a UTF-8 encoded input stream.
    let converter = Cc["@mozilla.org/intl/scriptableunicodeconverter"].createInstance(Ci.nsIScriptableUnicodeConverter);
    converter.charset = "UTF-8";

    // Asynchronously copy the data to the file.
    let istream = converter.convertToInputStream(aData);
    NetUtil.asyncCopy(istream, ostream, function(rc) {
      if (Components.isSuccessCode(rc))
        Services.obs.notifyObservers(null, "recommended-addons-cache-updated", "");
    });
  },
  
  searchSucceeded: function(aAddons, aAddonCount, aTotalResults) {
    let self = this;

    // Filter addons already installed
    AddonManager.getAllAddons(function(aAllAddons) {
      let addons = aAddons.filter(function(addon) {
        for (let i = 0; i < aAllAddons.length; i++)
          if (addon.id == aAllAddons[i].id)
            return false;

        return true;
      });

      let json = {
        addons: []
      };

      // Avoid any NSS costs. Convert https to http.
      addons.forEach(function(aAddon){
        json.addons.push({
          id: aAddon.id,
          name: aAddon.name,
          version: aAddon.version,
          homepageURL: aAddon.homepageURL.replace(/^https/, "http"),
          iconURL: aAddon.iconURL.replace(/^https/, "http")
        })
      });

      let file = self._getFile();
      self._writeFile(file, JSON.stringify(json));
    });
  },
  
  searchFailed: function searchFailed() { },
  
  search: function() {
    const kAddonsMaxDisplay = 2;

    if (AddonRepository.isSearching)
      AddonRepository.cancelSearch();
    AddonRepository.retrieveRecommendedAddons(kAddonsMaxDisplay, RecommendedSearchResults);
  }
}

const NSGetFactory = XPCOMUtils.generateNSGetFactory([AddonUpdateService]);

