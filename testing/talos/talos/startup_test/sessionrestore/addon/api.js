/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80 filetype=javascript: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* globals Services, XPCOMUtils */

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  SessionStartup: "resource:///modules/sessionstore/SessionStartup.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  StartupPerformance: "resource:///modules/sessionstore/StartupPerformance.jsm",
});

/* globals ExtensionAPI */

this.sessionrestore = class extends ExtensionAPI {
  onStartup() {
    // run() is async but we don't want to await or return it here,
    // since the extension should be considered started even before
    // run() has finished all its work.
    this.run();
  }

  async ensureTalosParentProfiler() {
    // TalosParentProfiler is part of TalosPowers, which is a separate WebExtension
    // that may or may not already have finished loading at this point. getTalosParentProfiler
    // is used to wait for TalosPowers to be around before continuing. This should
    // not be used to delay the execution of the test, but to delay the dumping of
    // the profile to disk.
    async function getTalosParentProfiler() {
      try {
        var { TalosParentProfiler } = ChromeUtils.import(
          "resource://talos-powers/TalosParentProfiler.jsm"
        );
        return TalosParentProfiler;
      } catch (err) {
        await new Promise(resolve => setTimeout(resolve, 500));
        return getTalosParentProfiler();
      }
    }

    this.TalosParentProfiler = await getTalosParentProfiler();
  }

  async finishProfiling(msg) {
    await this.ensureTalosParentProfiler();

    // In rare circumstances, it's possible for the session file to be read
    // into memory and for SessionStore to report itself as initialized before
    // the initial window has had a chance to finish setting itself up. To
    // account for this, we ensure that BrowserWindowTracker.windowCount is
    // not zero - and if it is, we wait for the first window to finish opening
    // before proceeding.
    if (!BrowserWindowTracker.windowCount) {
      const BROWSER_WINDOW_READY_TOPIC = "browser-delayed-startup-finished";
      await new Promise(resolve => {
        let observe = async () => {
          Services.obs.removeObserver(observe, BROWSER_WINDOW_READY_TOPIC);
          resolve();
        };
        Services.obs.addObserver(observe, BROWSER_WINDOW_READY_TOPIC);
      });
    }

    let win = BrowserWindowTracker.getTopWindow();
    let args = win.arguments[0];
    if (args && args instanceof Ci.nsIArray) {
      // For start-up tests Gecko Profiler arguments are passed to the first URL in
      // the query string, with the presumption that some tab that is loaded at start-up
      // will want to use them in the TalosContentProfiler.js script.
      //
      // Because we're running this part of the test in the parent process, we
      // pull those arguments from the top window directly. This is mainly so
      // that TalosParentProfiler knows where to save the profile.
      // eslint-disable-next-line mozilla/reject-importGlobalProperties
      Cu.importGlobalProperties(["URL"]);
      let url = new URL(args.queryElementAt(0, Ci.nsISupportsString).data);
      this.TalosParentProfiler.initFromURLQueryParams(url.search);
    }

    await this.TalosParentProfiler.pause(msg);
    await this.TalosParentProfiler.finishStartupProfiling();
  }

  async run() {
    try {
      let didRestore = true;

      if (!StartupPerformance.isRestored) {
        await SessionStartup.onceInitialized;

        if (
          SessionStartup.sessionType == SessionStartup.NO_SESSION ||
          SessionStartup.sessionType == SessionStartup.DEFER_SESSION
        ) {
          // We should only hit this patch in sessionrestore_no_auto_restore
          if (
            !Services.prefs.getBoolPref("talos.sessionrestore.norestore", false)
          ) {
            throw new Error("Session was not restored!");
          }
          await this.finishProfiling(
            "This test measures the time between process " +
              "creation and sessionRestored."
          );

          didRestore = false;
        } else {
          await new Promise(resolve => {
            let observe = async () => {
              Services.obs.removeObserver(
                observe,
                StartupPerformance.RESTORED_TOPIC
              );
              await this.finishProfiling(
                "This test measures the time between process " +
                  "creation and the last restored tab."
              );

              resolve();
            };
            Services.obs.addObserver(
              observe,
              StartupPerformance.RESTORED_TOPIC
            );
          });
        }
      }

      let startup_info = Services.startup.getStartupInfo();
      let restoreTime = didRestore
        ? StartupPerformance.latestRestoredTimeStamp
        : startup_info.sessionRestored;
      let duration = restoreTime - startup_info.process;
      let win = BrowserWindowTracker.getTopWindow();

      // Report data to Talos, if possible.
      dump("__start_report" + duration + "__end_report\n\n");

      // Next one is required by the test harness but not used.
      dump("__startTimestamp" + win.performance.now() + "__endTimestamp\n\n");
    } catch (ex) {
      dump(`SessionRestoreTalosTest: error ${ex}\n`);
      dump(ex.stack);
      dump("\n");
    }

    Services.startup.quit(Services.startup.eForceQuit);
  }
};
