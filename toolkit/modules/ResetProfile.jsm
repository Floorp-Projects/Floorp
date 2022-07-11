/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

var EXPORTED_SYMBOLS = ["ResetProfile"];

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

const MOZ_APP_NAME = AppConstants.MOZ_APP_NAME;
const MOZ_BUILD_APP = AppConstants.MOZ_BUILD_APP;

var ResetProfile = {
  /**
   * Check if reset is supported for the currently running profile.
   *
   * @return boolean whether reset is supported.
   */
  resetSupported() {
    if (Services.policies && !Services.policies.isAllowed("profileRefresh")) {
      return false;
    }
    // Reset is only supported if the self-migrator used for reset exists.
    let migrator =
      "@mozilla.org/profile/migrator;1?app=" +
      MOZ_BUILD_APP +
      "&type=" +
      MOZ_APP_NAME;
    if (!(migrator in Cc)) {
      return false;
    }
    // We also need to be using a profile the profile manager knows about.
    let profileService = Cc[
      "@mozilla.org/toolkit/profile-service;1"
    ].getService(Ci.nsIToolkitProfileService);
    let currentProfileDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
    for (let profile of profileService.profiles) {
      if (profile.rootDir && profile.rootDir.equals(currentProfileDir)) {
        return true;
      }
    }
    return false;
  },

  /**
   * Ask the user if they wish to restart the application to reset the profile.
   */
  async openConfirmationDialog(window) {
    let win = window;
    // If we are, for instance, on an about page, get the chrome window to
    // access its gDialogBox.
    if (win.docShell.chromeEventHandler) {
      win = win.browsingContext?.topChromeWindow;
    }

    let params = {
      learnMore: false,
      reset: false,
    };

    if (win.gDialogBox) {
      await win.gDialogBox.open(
        "chrome://global/content/resetProfile.xhtml",
        params
      );
    } else {
      win.openDialog(
        "chrome://global/content/resetProfile.xhtml",
        null,
        "modal,centerscreen,titlebar",
        params
      );
    }

    if (params.learnMore) {
      win.openTrustedLinkIn(
        "https://support.mozilla.org/kb/refresh-firefox-reset-add-ons-and-settings",
        "tab",
        {
          fromChrome: true,
        }
      );
      return;
    }

    if (!params.reset) {
      return;
    }

    // Set the reset profile environment variable.
    let env = Cc["@mozilla.org/process/environment;1"].getService(
      Ci.nsIEnvironment
    );
    env.set("MOZ_RESET_PROFILE_RESTART", "1");

    Services.startup.quit(
      Ci.nsIAppStartup.eForceQuit | Ci.nsIAppStartup.eRestart
    );
  },
};
