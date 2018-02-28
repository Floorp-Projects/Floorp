/* -*- indent-tabs-mode: nil -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
  From mozilla/toolkit/content
  These files did not have a license
*/
var EXPORTED_SYMBOLS = ["goQuitApplication"];

ChromeUtils.import("resource://gre/modules/Services.jsm");

function canQuitApplication() {
  try {
    var cancelQuit = Cc["@mozilla.org/supports-PRBool;1"]
                     .createInstance(Ci.nsISupportsPRBool);
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

  const kAppStartup = "@mozilla.org/toolkit/app-startup;1";
  const kAppShell   = "@mozilla.org/appshell/appShellService;1";
  var appService;
  var forceQuit;

  if (kAppStartup in Cc) {
    appService = Services.startup;
    forceQuit  = Ci.nsIAppStartup.eForceQuit;
  } else if (kAppShell in Cc) {
    appService = Services.appShell;
    forceQuit = Ci.nsIAppShellService.eForceQuit;
  } else {
    throw new Error("goQuitApplication: no AppStartup/appShell");
  }

  try {
    appService.quit(forceQuit);
  } catch (ex) {
    throw new Error(`goQuitApplication: ${ex.message}`);
  }

  return true;
}
