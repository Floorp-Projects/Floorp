/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "AddonManagerPrivate",
                               "resource://gre/modules/AddonManager.jsm");

ChromeUtils.defineModuleGetter(this, "GMPInstallManager",
                               "resource://gre/modules/GMPInstallManager.jsm");

ChromeUtils.defineModuleGetter(this, "EventDispatcher",
                               "resource://gre/modules/Messaging.jsm");

ChromeUtils.defineModuleGetter(this, "OS", "resource://gre/modules/osfile.jsm");

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

  QueryInterface: ChromeUtils.generateQI([Ci.nsITimerCallback]),

  notify: function aus_notify(aTimer) {
    if (aTimer && !Services.prefs.getBoolPref(PREF_ADDON_UPDATE_ENABLED, true))
      return;

    // If we already auto-upgraded and installed new versions, ignore this check
    if (gNeedsRestart)
      return;

    AddonManagerPrivate.backgroundUpdateCheck();

    let gmp = new GMPInstallManager();
    gmp.simpleCheckAndInstall().catch(() => {});

    let interval = 1000 * Services.prefs.getIntPref(PREF_ADDON_UPDATE_INTERVAL, 86400);
    EventDispatcher.instance.sendRequest({
      type: "Gecko:ScheduleRun",
      action: "update-addons",
      trigger: interval,
      interval: interval,
    });
  }
};

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([AddonUpdateService]);

