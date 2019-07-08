/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals ExtensionAPI */

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.jsm",
  Services: "resource://gre/modules/Services.jsm",
  setTimeout: "resource://gre/modules/Timer.jsm",
  TalosParentProfiler: "resource://talos-powers/TalosParentProfiler.jsm",
});

const SCALAR_KEY = "timestamps.about_home_topsites_first_paint";

const MAX_ATTEMPTS = 10;

let gAttempts = 0;

this.startup_about_home_paint = class extends ExtensionAPI {
  onStartup() {
    Services.obs.addObserver(this, "browser-idle-startup-tasks-finished");
  }

  async wait(aMs) {
    return new Promise(resolve => {
      setTimeout(resolve, aMs);
    });
  }

  observe(subject, topic, data) {
    if (topic == "browser-idle-startup-tasks-finished") {
      this.checkForTelemetry();
    }
  }

  async checkForTelemetry() {
    let snapshot = Services.telemetry.getSnapshotForScalars("main");
    let measurement = snapshot.parent[SCALAR_KEY];
    if (!measurement) {
      if (gAttempts == MAX_ATTEMPTS) {
        dump(`Failed to get ${SCALAR_KEY} scalar probe in time.\n`);
        await this.quit();
        return;
      }
      gAttempts++;

      await this.wait(1000);
      ChromeUtils.idleDispatch(() => {
        this.checkForTelemetry();
      });
    } else {
      // Got our measurement.
      dump("__start_report" + measurement + "__end_report\n\n");
      // eslint-disable-next-line mozilla/avoid-Date-timing
      dump("__startTimestamp" + Date.now() + "__endTimestamp\n");

      let env = Cc["@mozilla.org/process/environment;1"].getService(
        Ci.nsIEnvironment
      );

      if (env.exists("TPPROFILINGINFO")) {
        let profilingInfo = env.get("TPPROFILINGINFO");
        if (profilingInfo !== null) {
          TalosParentProfiler.initFromObject(JSON.parse(profilingInfo));
          await TalosParentProfiler.finishStartupProfiling();
        }
      }

      await this.quit();
    }
  }

  async quit() {
    for (let domWindow of Services.wm.getEnumerator(null)) {
      domWindow.close();
    }

    try {
      await this.wait(0);
      Services.startup.quit(Services.startup.eForceQuit);
    } catch (e) {
      dump("Force Quit failed: " + e);
    }
  }

  onShutdown() {}
};
