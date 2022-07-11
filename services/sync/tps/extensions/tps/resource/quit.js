/* -*- indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  From mozilla/toolkit/content
  These files did not have a license
*/
var EXPORTED_SYMBOLS = ["goQuitApplication"];

function canQuitApplication() {
  try {
    var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"].createInstance(
      Ci.nsISupportsPRBool
    );
    Services.obs.notifyObservers(cancelQuit, "quit-application-requested");

    // Something aborted the quit process.
    if (cancelQuit.data) {
      return false;
    }
  } catch (ex) {}

  return true;
}

function goQuitApplication() {
  if (!canQuitApplication()) {
    return false;
  }

  try {
    Services.startup.quit(Ci.nsIAppStartup.eForceQuit);
  } catch (ex) {
    throw new Error(`goQuitApplication: ${ex.message}`);
  }

  return true;
}
