/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * While the feature is in development, hide the feature behind a pref. See
 * modules/libpref/init/all.js and Bug 971044 for the status of enabling this project.
 */
function updateEnabledState() {
  if (Services.prefs.getBoolPref("browser.translations.enable")) {
    document.body.style.visibility = "visible";
  } else {
    document.body.style.visibility = "hidden";
  }
}

window.addEventListener("DOMContentLoaded", () => {
  updateEnabledState();
  Services.prefs.addObserver("browser.translations.enable", updateEnabledState);
});
