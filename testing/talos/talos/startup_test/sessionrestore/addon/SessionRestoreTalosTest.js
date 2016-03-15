/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
  "resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "setTimeout",
  "resource://gre/modules/Timer.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "StartupPerformance",
  "resource:///modules/sessionstore/StartupPerformance.jsm");

// Observer Service topics.
const STARTUP_TOPIC = "profile-after-change";

// Process Message Manager topics.
const MSG_REQUEST = "session-restore-test?duration";
const MSG_PROVIDE = "session-restore-test:duration";

function nsSessionRestoreTalosTest() { }

nsSessionRestoreTalosTest.prototype = {
  classID: Components.ID("{716346e5-0c45-4aa2-b601-da36f3c74bd8}"),

  _xpcom_factory: XPCOMUtils.generateSingletonFactory(nsSessionRestoreTalosTest),

  //////////////////////////////////////////////////////////////////////////////
  //// nsISupports

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  //////////////////////////////////////////////////////////////////////////////
  //// nsIObserver

  observe: function DS_observe(aSubject, aTopic, aData) {
    switch (aTopic) {
      case STARTUP_TOPIC:
        this.init();
        break;
      case StartupPerformance.RESTORED_TOPIC:
        this.onReady(true);
        break;
      default:
        throw new Error(`Unknown topic ${aTopic}`);
    }
  },

  /**
   * Perform initialization on profile-after-change.
   */
  init: function() {
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
          Services.obs.addObserver(this, StartupPerformance.RESTORED_TOPIC, false);
        }
      });
    }
  },

  /**
   * Session Restore is complete, hurray.
   */
  onReady: function(hasRestoredTabs) {
    if (hasRestoredTabs) {
      Services.obs.removeObserver(this, StartupPerformance.RESTORED_TOPIC);
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
};

////////////////////////////////////////////////////////////////////////////////
//// Module

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([nsSessionRestoreTalosTest]);
