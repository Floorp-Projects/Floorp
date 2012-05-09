/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
From mozilla/toolkit/content
These files did not have a license
*/

function canQuitApplication() {
  var os = Components.classes["@mozilla.org/observer-service;1"]
    .getService(Components.interfaces.nsIObserverService);
  if (!os) {
    return true;
  }

  try {
    var cancelQuit = Components.classes["@mozilla.org/supports-PRBool;1"]
      .createInstance(Components.interfaces.nsISupportsPRBool);
    os.notifyObservers(cancelQuit, "quit-application-requested", null);

    // Something aborted the quit process.
    if (cancelQuit.data) {
      return false;
    }
  }
  catch (ex) {
  }
  return true;
}

function goQuitApplication() {
  if (!canQuitApplication()) {
    return false;
  }

  const kAppStartup = '@mozilla.org/toolkit/app-startup;1';
  const kAppShell = '@mozilla.org/appshell/appShellService;1';
  var appService;
  var forceQuit;

  if (kAppStartup in Components.classes) {
    appService = Components.classes[kAppStartup].
      getService(Components.interfaces.nsIAppStartup);
    forceQuit = Components.interfaces.nsIAppStartup.eForceQuit;
  } else if (kAppShell in Components.classes) {
    appService = Components.classes[kAppShell].
      getService(Components.interfaces.nsIAppShellService);
    forceQuit = Components.interfaces.nsIAppShellService.eForceQuit;
  } else {
    throw 'goQuitApplication: no AppStartup/appShell';
  }

  try {
    appService.quit(forceQuit);
  }
  catch(ex) {
    throw('goQuitApplication: ' + ex);
  }
  return true;
}
