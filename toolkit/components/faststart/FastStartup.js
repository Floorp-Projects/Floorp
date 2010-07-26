/* -*- Mode: js2; js2-basic-offset: 4; indent-tabs-mode: nil; -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is FastStartup.js
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Brian Crowder <crowder@fiverocks.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const Timer = Components.Constructor("@mozilla.org/timer;1", "nsITimer", "initWithCallback");

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const RESTART_ENV_VAR = "FASTSTART_INITIATED_RESTART";

function setenv(key, value) {
  let env = Cc["@mozilla.org/process/environment;1"].
            getService(Components.interfaces.nsIEnvironment);
  env.set(key, value);
}

function getenv(key) {
  let env = Cc["@mozilla.org/process/environment;1"].
            getService(Components.interfaces.nsIEnvironment);
  return env.get(key);
}

function nsFastStartupObserver() {
  let _browserWindowCount = 0;
  let _memCleanupTimer;
  let _restartTimer;
  let _isShuttingDown;

  function stopMemoryCleanup() {
    if (_memCleanupTimer) {
      _memCleanupTimer.cancel();
      _memCleanupTimer = null;
    }

    if (_restartTimer) {
      _restartTimer.cancel();
      _restartTimer = null;
    }
  }

  function scheduleMemoryCleanup() {
    if (_isShuttingDown)
      return;

    stopMemoryCleanup();

    function restart() {
      setenv(RESTART_ENV_VAR, "1");
      setenv("NO_SPLASH", "1");
      let appstartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                       getService(Ci.nsIAppStartup);
      appstartup.quit(Ci.nsIAppStartup.eRestart | Ci.nsIAppStartup.eAttemptQuit);
      
      _restartTimer = null;
    }

    function memoryCleanup() {
      var os = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);

      // Do this 3 times, because of a comment in TestGtkEmbed that says this
      // gives the cycle collector the best chance at purging things.
      os.notifyObservers(null, "memory-pressure", "heap-minimize");
      os.notifyObservers(null, "memory-pressure", "heap-minimize");
      os.notifyObservers(null, "memory-pressure", "heap-minimize");

      _memCleanupTimer = null; 
    }

    // wait 30s until firing off the memory cleanup, in case the user
    // opens another window right away
    _memCleanupTimer = new Timer(memoryCleanup, 30000, Ci.nsITimer.TYPE_ONE_SHOT);

    // Also, begin a countdown to restart the stub in 15 minutes
    _restartTimer = new Timer(restart, 60000 * 15, Ci.nsITimer.TYPE_ONE_SHOT);
  }

  //
  // nsIObserver
  //
  this.observe = function observe(subject, topic, data) {
    var win = subject;

    // XXX the window at this point won't actually have its document loaded --
    // win.document.documentURI will pretty much always be about:blank.  We need
    // to attach a load handler to actually figure out which document gets loaded.
    if (topic == "domwindowopened") {
      stopMemoryCleanup();
      _browserWindowCount++;
    } else if (topic == "domwindowclosed") {
      if (_browserWindowCount > 0)
        _browserWindowCount--;
      if (_browserWindowCount == 0) {
        scheduleMemoryCleanup();
        let prefservice = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefService);
        prefservice.savePrefFile(null);
      }
    } else if (topic == "quit-application-granted") {
      stopMemoryCleanup();
      _isShuttingDown = true;
      let appstartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                       getService(Ci.nsIAppStartup);
      appstartup.exitLastWindowClosingSurvivalArea();
    }
  }

  /*
   * QueryInterface
   * We expect the WindowWatcher service to retain a strong reference to us, so supporting
   * weak references is fine.
   */
  this.QueryInterface = XPCOMUtils.generateQI([Ci.nsIObserver, Ci.nsISupportsWeakReference]);
}

function nsFastStartupCLH() { }

nsFastStartupCLH.prototype = {
  //
  // nsICommandLineHandler
  //
  handle: function fs_handle(cmdLine) {
    // the rest of this only handles -faststart here

    if (cmdLine.handleFlag("shutdown-faststart", false)) {
      cmdLine.preventDefault = true;
      // Shutdown this service
      let appstartup = Cc["@mozilla.org/toolkit/app-startup;1"].getService(Ci.nsIAppStartup);
      appstartup.quit(Ci.nsIAppStartup.eAttemptQuit);
      return;
    }

    if (cmdLine.handleFlag("faststart-hidden", false) || (getenv(RESTART_ENV_VAR) == "1"))
      cmdLine.preventDefault = true;

    try {
      // did we already initialize faststart?  if so,
      // nothing to do here.
      if (this.inited)
        return;

      this.inited = true;

      // Clear this environment variable for subsequent runs
      setenv(RESTART_ENV_VAR, "0");

      let fsobs = new nsFastStartupObserver();
      let wwatch = Cc["@mozilla.org/embedcomp/window-watcher;1"].
                   getService(Ci.nsIWindowWatcher);
      wwatch.registerNotification(fsobs);

      let obsService = Cc["@mozilla.org/observer-service;1"].getService(Ci.nsIObserverService);
      obsService.addObserver(fsobs, "quit-application-granted", true);

      let appstartup = Cc["@mozilla.org/toolkit/app-startup;1"].
                       getService(Ci.nsIAppStartup);
      appstartup.enterLastWindowClosingSurvivalArea();
    } catch (e) {
      Cu.reportError(e);
    }
  },

  helpInfo: "    -faststart-hidden      Start the FastStart service\n" +
            "    -shutdown-faststart    Shutdown the FastStart service\n",

  // QI
  QueryInterface: XPCOMUtils.generateQI([Ci.nsICommandLineHandler]),

  // XPCOMUtils factory
  classID: Components.ID("{580c6c51-f690-4ce1-9ecc-b678e0c031c7}"),
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([nsFastStartupCLH]);
