/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManagerPrivate",
                                  "resource://gre/modules/AddonManager.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonRepository",
                                  "resource://gre/modules/addons/AddonRepository.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "GMPInstallManager",
                                  "resource://gre/modules/GMPInstallManager.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "EventDispatcher",
                                  "resource://gre/modules/Messaging.jsm");

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
const PREF_ADDON_UPDATE_INTERVAL = "extensions.autoupdate.interval";

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

    AddonManagerPrivate.backgroundUpdateCheck();

    let gmp = new GMPInstallManager();
    gmp.simpleCheckAndInstall().catch(() => {});

    let interval = 1000 * getPref("getIntPref", PREF_ADDON_UPDATE_INTERVAL, 86400);
    EventDispatcher.instance.sendRequest({
      type: "Gecko:ScheduleRun",
      action: "update-addons",
      trigger: interval,
      interval: interval,
    });
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AddonUpdateService]);

