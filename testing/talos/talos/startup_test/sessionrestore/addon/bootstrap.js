/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

ChromeUtils.defineModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
ChromeUtils.defineModuleGetter(this, "setTimeout",
  "resource://gre/modules/Timer.jsm");
ChromeUtils.defineModuleGetter(this, "StartupPerformance",
  "resource:///modules/sessionstore/StartupPerformance.jsm");

// Observer Service topics.
const WINDOW_READY_TOPIC = "browser-delayed-startup-finished";

// Process Message Manager topics.
const MSG_REQUEST = "session-restore-test?duration";
const MSG_PROVIDE = "session-restore-test:duration";

const sessionRestoreTest = {
  QueryInterface: ChromeUtils.generateQI([Ci.nsIObserver]),

  // ////////////////////////////////////////////////////////////////////////////
  // // nsIObserver

  observe: function DS_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case StartupPerformance.RESTORED_TOPIC:
        this.onReady(true);
        break;
      case WINDOW_READY_TOPIC:
        Services.obs.removeObserver(this, WINDOW_READY_TOPIC);
        this.onWindow(aSubject);
        break;
      default:
        throw new Error(`Unknown topic ${aTopic}`);
    }
  },

  /**
   * Perform initialization on profile-after-change.
   */
  init() {
    if (StartupPerformance.isRestored) {
      this.onReady(true);
    } else {
      let sessionStartup = Cc["@mozilla.org/browser/sessionstartup;1"]
                                 .getService(Ci.nsISessionStartup);
      sessionStartup.onceInitialized.then(() => {
        if (sessionStartup.sessionType == Ci.nsISessionStartup.NO_SESSION
        || sessionStartup.sessionType == Ci.nsISessionStartup.DEFER_SESSION) {
          this.onReady(false);
        } else {
          Services.obs.addObserver(this, StartupPerformance.RESTORED_TOPIC);
        }
      });
    }
  },

  /**
   * Session Restore is complete, hurray.
   */
  onReady(hasRestoredTabs) {
    if (hasRestoredTabs) {
      Services.obs.removeObserver(this, StartupPerformance.RESTORED_TOPIC);
    }

    // onReady might fire before the browser window has finished initializing
    // or sometimes soon after. We handle both cases here.
    let win = Services.wm.getMostRecentWindow("navigator:browser");
    if (!win || !win.gBrowserInit || !win.gBrowserInit.delayedStartupFinished) {
      // We didn't have a window around yet, so we'll wait until one becomes
      // available before opening the result tab.
      Services.obs.addObserver(this, WINDOW_READY_TOPIC);
    } else {
      // We have a window, so we can open the result tab in it right away.
      this.onWindow(win);
    }

    try {
      setTimeout(function() {
        // `StartupPerformance.latestRestoredTimeStamp` actually becomes available only on the next tick.
        let startup_info = Services.startup.getStartupInfo();
        let duration =
          hasRestoredTabs
            ? StartupPerformance.latestRestoredTimeStamp - startup_info.sessionRestoreInit
            : startup_info.sessionRestored - startup_info.sessionRestoreInit;

        // Broadcast startup duration information immediately, in case the talos
        // page is already loaded.
        Services.ppmm.broadcastAsyncMessage(MSG_PROVIDE, {duration});

        // Now, in case the talos page isn't loaded yet, prepare to respond if it
        // requestions the duration information.
        Services.ppmm.addMessageListener(MSG_REQUEST, function listener() {
          Services.ppmm.removeMessageListener(MSG_REQUEST, listener);
          Services.ppmm.broadcastAsyncMessage(MSG_PROVIDE, {duration});
        });
      }, 0);
    } catch (ex) {
      dump(`SessionRestoreTalosTest: error ${ex}\n`);
      dump(ex.stack);
      dump("\n");
    }
  },

  /**
   * A window is ready for us to open the result tab in.
   */
  onWindow(win) {
    let args = win.arguments[0];
    let queryString = "";

    if (args && args instanceof Ci.nsIArray) {
      // For start-up tests Gecko Profiler arguments are passed to the first URL in
      // the query string, with the presumption that the tab that is being loaded at
      // start-up will want to use those arguments with the TalosContentProfiler scripts.
      //
      // Because we're actually loading the results (and the content that reports
      // the results) in a new tab _after_ the window has opened, we need to send
      // those arguments over to the new tab that we open so that the profiler scripts
      // will continue to work.
      Cu.importGlobalProperties(["URL"]);
      let url = new URL(args.queryElementAt(0, Ci.nsISupportsString).data);
      queryString = url.search;
    }

    win.gBrowser.addTab("chrome://session-restore-test/content/index.html" + queryString);
  }
};


function startup(data, reason) {
  sessionRestoreTest.init();
}

function shutdown(data, reason) {}
function install(data, reason) {}
function uninstall(data, reason) {}
