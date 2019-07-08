/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* This is temporary until bug 1521632 is fixed */

"use strict";

/* import-globals-from ../../../content/contentAreaUtils.js */

/* globals Services */
ChromeUtils.import("resource://gre/modules/Services.jsm", this);

const gUpdateElevationDialog = {
  openUpdateURL(event) {
    if (event.button == 0) {
      openURL(event.target.getAttribute("url"));
    }
  },
  getAUSString(key, strings) {
    if (strings) {
      return this.strings.getFormattedString(key, strings);
    }
    return this.strings.getString(key);
  },
  _setButton(button, string) {
    var label = this.getAUSString(string);
    if (label.includes("%S")) {
      label = label.replace(/%S/, this.brandName);
    }
    button.label = label;
    button.setAttribute("accesskey", this.getAUSString(string + ".accesskey"));
  },
  onLoad() {
    this.strings = document.getElementById("updateStrings");
    this.brandName = document
      .getElementById("brandStrings")
      .getString("brandShortName");

    let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    let update = um.activeUpdate;
    let updateFinishedName = document.getElementById("updateFinishedName");
    updateFinishedName.value = update.name;

    let link = document.getElementById("detailsLinkLabel");
    if (update.detailsURL) {
      link.setAttribute("url", update.detailsURL);
      // The details link is stealing focus so it is disabled by default and
      // should only be enabled after onPageShow has been called.
      link.disabled = false;
    } else {
      link.hidden = true;
    }

    let manualLinkLabel = document.getElementById("manualLinkLabel");
    let manualURL = Services.urlFormatter.formatURLPref(
      "app.update.url.manual"
    );
    manualLinkLabel.value = manualURL;
    manualLinkLabel.setAttribute("url", manualURL);

    let button = document.getElementById("elevateExtra2");
    this._setButton(button, "restartLaterButton");
    button = document.getElementById("elevateExtra1");
    this._setButton(button, "noThanksButton");
    button = document.getElementById("elevateAccept");
    this._setButton(button, "restartNowButton");
    button.focus();
  },
  onRestartLater() {
    window.close();
  },
  onNoThanks() {
    Services.obs.notifyObservers(null, "update-canceled");
    let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    let update = um.activeUpdate;
    um.cleanupActiveUpdate();
    // Since the user has clicked "No Thanks", we should not prompt them to update to
    // this version again unless they manually select "Check for Updates..."
    // which will clear app.update.elevate.never preference.
    let aus = Cc["@mozilla.org/updates/update-service;1"].getService(
      Ci.nsIApplicationUpdateService
    );
    if (aus.elevationRequired && update) {
      Services.prefs.setCharPref("app.update.elevate.never", update.appVersion);
    }
    window.close();
  },
  onRestartNow() {
    // disable the "finish" (Restart) and "extra1" (Later) buttons
    // because the Software Update wizard is still up at the point,
    // and will remain up until we return and we close the
    // window with a |window.close()| in wizard.xml
    // (it was the firing the "wizardfinish" event that got us here.)
    // This prevents the user from switching back
    // to the Software Update dialog and clicking "Restart" or "Later"
    // when dealing with the "confirm close" prompts.
    // See bug #350299 for more details.
    document.getElementById("elevateExtra2").disabled = true;
    document.getElementById("elevateExtra1").disabled = true;
    document.getElementById("elevateAccept").disabled = true;

    // This dialog was shown because elevation was required so there is no need
    // to check if elevation is required again.
    let um = Cc["@mozilla.org/updates/update-manager;1"].getService(
      Ci.nsIUpdateManager
    );
    um.elevationOptedIn();

    // Notify all windows that an application quit has been requested.
    let cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool
    );
    Services.obs.notifyObservers(
      cancelQuit,
      "quit-application-requested",
      "restart"
    );

    // Something aborted the quit process.
    if (cancelQuit.data) {
      return;
    }

    // If already in safe mode restart in safe mode (bug 327119)
    if (Services.appinfo.inSafeMode) {
      let env = Cc["@mozilla.org/process/environment;1"].getService(
        Ci.nsIEnvironment
      );
      env.set("MOZ_SAFE_MODE_RESTART", "1");
    }

    // Restart the application
    Services.startup.quit(
      Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
    );
  },
};
