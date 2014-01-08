/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository",
                                  "resource://gre/modules/addons/AddonRepository.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

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

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AddonUpdateService]);

