/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for AddonWatcher.jsm

"use strict";

Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/AddonManager.jsm", this);
Cu.import("resource://gre/modules/AddonWatcher.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);

const ADDON_URL = "http://example.com/browser/toolkit/components/perfmonitoring/tests/browser/browser_Addons_sample.xpi";
const ADDON_ID = "addonwatcher-test@mozilla.com";

add_task(function* init() {
  AddonWatcher.uninit();

  info("Installing test add-on");
  let installer = yield new Promise(resolve => AddonManager.getInstallForURL(ADDON_URL, resolve, "application/x-xpinstall"));
  if (installer.error) {
    throw installer.error;
  }
  let ready = new Promise((resolve, reject) => installer.addListener({
    onInstallEnded: (_, addon) => resolve(addon),
    onInstallFailed: reject,
    onDownloadFailed: reject
  }));
  installer.install();

  info("Waiting for installation to terminate");
  let addon = yield ready;

  registerCleanupFunction(() => {
    info("Uninstalling test add-on");
    addon.uninstall()
  });

  Services.prefs.setIntPref("browser.addon-watch.interval", 1000);
  registerCleanupFunction(() => {
    Services.prefs.clearUserPref("browser.addon-watch.interval");
  });

  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  registerCleanupFunction(function () {
    Services.telemetry.canRecordExtended = oldCanRecord;
  });
});

// Utility function to burn some resource, trigger a reaction of the add-on watcher
// and check both its notification and telemetry.
var burn_rubber = Task.async(function*({histogramName, topic, expectedReason, prefs, expectedMinSum}) {
  try {
    for  (let key of Object.keys(prefs)) {
      Services.prefs.setIntPref(key, prefs[key]);
    }

    // We let the add-on watcher a few chances to get things right, as it may
    // detect jank before it detects cpow.
    for (let i = 0; i < 5; ++i) {
      info(`Preparing add-on watcher for attempt ${i}`);
      let wait = new Promise(resolve => AddonWatcher.init((id, reason) => {
        // Ensure we're only looking for this add-on.
        if (id != ADDON_ID) {
          return;
        }
        Assert.equal(id, ADDON_ID, "The add-on watcher has detected the misbehaving addon");
        info(`Reason: ${reason}, expected ${expectedReason}`);
        if (reason == expectedReason) {
          resolve(reason);
        }
      }));
      let done = false;
      wait = wait.then(result => {
        done = true;
        return result;
      });

      let histogram = Services.telemetry.getKeyedHistogramById(histogramName);
      histogram.clear();
      let snap1 = histogram.snapshot(ADDON_ID);
      Assert.equal(snap1.sum, 0, `Histogram ${histogramName} is initially empty for the add-on`);
      while (!done) {
        yield new Promise(resolve => setTimeout(resolve, 100));
        info("Burning some CPU. This should cause an add-on watcher notification");
        Services.obs.notifyObservers(null, topic, "");
      }
      let reason = yield wait;

      Assert.equal(reason, expectedReason, "Reason is valid");
      let snap2 = histogram.snapshot(ADDON_ID);

      if (snap2.sum >= expectedMinSum) {
        // Success.
        return;
      } else {
        info(`Histogram ${histogramName} recorded a gravity of ${snap2.sum}, expecting at least ${expectedMinSum}.`);
      }

      AddonWatcher.uninit();
    }
    Assert.ok(false, "I should have recorded the expected gravity by now.");
  } finally {
    AddonWatcher.uninit();
    for  (let key of Object.keys(prefs)) {
      Services.prefs.clearUserPref(key);
    }
  }
});

// Test that burning CPU will cause the add-on watcher to notice that
// the add-on is misbehaving.
add_task(function* test_burn_CPU() {
  yield burn_rubber({
    prefs: {
      "browser.addon-watch.limits.longestDuration": 2,
      "browser.addon-watch.limits.totalCPOWTime": -1,
    },
    histogramName: "MISBEHAVING_ADDONS_JANK_LEVEL",
    topic: "test-addonwatcher-burn-some-cpu",
    expectedReason: "longestDuration",
    expectedMinSum: 7,
  });
});

// Test that burning CPOW will cause the add-on watcher to notice that
// the add-on is misbehaving.
add_task(function* test_burn_CPOW() {
  if (!gMultiProcessBrowser) {
    info("This is a single-process Firefox, we can't test for CPOW");
    return;
  }
  yield burn_rubber({
    prefs: {
      "browser.addon-watch.limits.longestDuration": -1,
      "browser.addon-watch.limits.totalCPOWTime": 100,
    },
    histogramName: "MISBEHAVING_ADDONS_CPOW_TIME_MS",
    topic: "test-addonwatcher-burn-some-cpow",
    expectedReason: "longestDuration",
    expectedMinSum: 400,
  });
});
