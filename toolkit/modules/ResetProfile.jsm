/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

this.EXPORTED_SYMBOLS = ["ResetProfile"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;
#expand const MOZ_APP_NAME = "__MOZ_APP_NAME__";
#expand const MOZ_BUILD_APP = "__MOZ_BUILD_APP__";

Cu.import("resource://gre/modules/Services.jsm");

this.ResetProfile = {
  /**
   * Check if reset is supported for the currently running profile.
   *
   * @return boolean whether reset is supported.
   */
  resetSupported: function() {
    let profileService = Cc["@mozilla.org/toolkit/profile-service;1"].
                         getService(Ci.nsIToolkitProfileService);
    let currentProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);

    // Reset is only supported for the default profile if the self-migrator used for reset exists.
    try {
      return currentProfileDir.equals(profileService.selectedProfile.rootDir) &&
        ("@mozilla.org/profile/migrator;1?app=" + MOZ_BUILD_APP + "&type=" + MOZ_APP_NAME in Cc);
    } catch (e) {
      // Catch exception when there is no selected profile.
      Cu.reportError(e);
    }
    return false;
  },

  getMigratedData: function() {
    Cu.import("resource:///modules/MigrationUtils.jsm");

    // From migration.properties
    const MIGRATED_TYPES = [
      128,// Windows/Tabs
      4,  // History and Bookmarks
      16, // Passwords
      8,  // Form History
      2,  // Cookies
    ];

    // Loop over possible data to migrate to give the user a list of what will be preserved.
    let dataTypes = [];
    for (let itemID of MIGRATED_TYPES) {
      try {
        let typeName = MigrationUtils.getLocalizedString(itemID + "_" + MOZ_APP_NAME);
        dataTypes.push(typeName);
      } catch (x) {
        // Catch exceptions when the string for a data type doesn't exist.
        Cu.reportError(x);
      }
    }
    return dataTypes;
  },

  /**
   * Ask the user if they wish to restart the application to reset the profile.
   */
  openConfirmationDialog: function(window) {
    // Prompt the user to confirm.
    let params = {
      reset: false,
    };
    window.openDialog("chrome://global/content/resetProfile.xul", null,
                      "chrome,modal,centerscreen,titlebar,dialog=yes", params);
    if (!params.reset)
      return;

    // Set the reset profile environment variable.
    let env = Cc["@mozilla.org/process/environment;1"]
                .getService(Ci.nsIEnvironment);
    env.set("MOZ_RESET_PROFILE_RESTART", "1");

    let appStartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
    appStartup.quit(Ci.nsIAppStartup.eForceQuit | Ci.nsIAppStartup.eRestart);
  },
};
