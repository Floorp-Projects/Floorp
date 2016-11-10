/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests for AddonWatcher.jsm

"use strict";

requestLongerTimeout(2);

Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/AddonManager.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);

const ADDON_URL = "http://example.com/browser/toolkit/components/perfmonitoring/tests/browser/browser_Addons_sample.xpi";
const ADDON_ID = "addonwatcher-test@mozilla.com";

add_task(function* init() {
  info("Installing test add-on");
  let installer = yield new Promise(resolve => AddonManager.getInstallForURL(ADDON_URL, resolve, "application/x-xpinstall"));
  if (installer.error) {
    throw installer.error;
  }
  let installed = new Promise((resolve, reject) => installer.addListener({
    onInstallEnded: (_, addon) => resolve(addon),
    onInstallFailed: reject,
    onDownloadFailed: reject
  }));

  // We also need to wait for the add-on to report that it's ready
  // to be used in the test.
  let ready = TestUtils.topicObserved("test-addonwatcher-ready");
  installer.install();

  info("Waiting for installation to terminate");
  let addon = yield installed;

  yield ready;

  registerCleanupFunction(() => {
    info("Uninstalling test add-on");
    addon.uninstall()
  });

  Preferences.set("browser.addon-watch.warmup-ms", 0);
  Preferences.set("browser.addon-watch.freeze-threshold-micros", 0);
  Preferences.set("browser.addon-watch.jank-threshold-micros", 0);
  Preferences.set("browser.addon-watch.occurrences-between-alerts", 0);
  Preferences.set("browser.addon-watch.delay-between-alerts-ms", 0);
  Preferences.set("browser.addon-watch.delay-between-freeze-alerts-ms", 0);
  Preferences.set("browser.addon-watch.max-simultaneous-reports", 10000);
  Preferences.set("browser.addon-watch.deactivate-after-idle-ms", 100000000);
  registerCleanupFunction(() => {
    for (let k of [
      "browser.addon-watch.warmup-ms",
      "browser.addon-watch.freeze-threshold-micros",
      "browser.addon-watch.jank-threshold-micros",
      "browser.addon-watch.occurrences-between-alerts",
      "browser.addon-watch.delay-between-alerts-ms",
      "browser.addon-watch.delay-between-freeze-alerts-ms",
      "browser.addon-watch.max-simultaneous-reports",
      "browser.addon-watch.deactivate-after-idle-ms"
    ]) {
      Preferences.reset(k);
    }
  });

  let oldCanRecord = Services.telemetry.canRecordExtended;
  Services.telemetry.canRecordExtended = true;
  AddonWatcher.init();

  registerCleanupFunction(function () {
    AddonWatcher.paused = true;
    Services.telemetry.canRecordExtended = oldCanRecord;
  });
});

// Utility function to burn some resource, trigger a reaction of the add-on watcher
// and check both its notification and telemetry.
let burn_rubber = Task.async(function*({histogramName, topic, expectedMinSum}) {
  let detected = false;
  let observer = (_, topic, id) => {
    Assert.equal(id, ADDON_ID, "The add-on watcher has detected the misbehaving addon");
    detected = true;
  };

  try {
    info("Preparing add-on watcher");

    Services.obs.addObserver(observer, AddonWatcher.TOPIC_SLOW_ADDON_DETECTED, false);

    let histogram = Services.telemetry.getKeyedHistogramById(histogramName);
    histogram.clear();
    let snap1 = histogram.snapshot(ADDON_ID);
    Assert.equal(snap1.sum, 0, `Histogram ${histogramName} is initially empty for the add-on`);

    let histogramUpdated = false;
    do {
      info(`Burning some CPU with ${topic}. This should cause an add-on watcher notification`);
      yield new Promise(resolve => setTimeout(resolve, 100));
      Services.obs.notifyObservers(null, topic, "");
      yield new Promise(resolve => setTimeout(resolve, 100));

      let snap2 = histogram.snapshot(ADDON_ID);
      histogramUpdated = snap2.sum > 0;
      info(`For the moment, histogram ${histogramName} shows ${snap2.sum} => ${histogramUpdated}`);
      info(`For the moment, we have ${detected ? "" : "NOT "}detected the slow add-on`);
    } while (!histogramUpdated || !detected);

    let snap3 = histogram.snapshot(ADDON_ID);
    Assert.ok(snap3.sum >= expectedMinSum, `Histogram ${histogramName} recorded a gravity of ${snap3.sum}, expecting at least ${expectedMinSum}.`);
  } finally {
    Services.obs.removeObserver(observer, AddonWatcher.TOPIC_SLOW_ADDON_DETECTED);
  }
});

// Test that burning CPU will cause the add-on watcher to notice that
// the add-on is misbehaving.
add_task(function* test_burn_CPU() {
  yield burn_rubber({
    histogramName: "PERF_MONITORING_SLOW_ADDON_JANK_US",
    topic: "test-addonwatcher-burn-some-cpu",
    expectedMinSum: 7,
  });
});

// Test that burning content CPU will cause the add-on watcher to notice that
// the add-on is misbehaving.
/*
Blocked by bug 1227283.
add_task(function* test_burn_content_CPU() {
  yield burn_rubber({
    histogramName: "PERF_MONITORING_SLOW_ADDON_JANK_US",
    topic: "test-addonwatcher-burn-some-content-cpu",
    expectedMinSum: 7,
  });
});
*/

// Test that burning CPOW will cause the add-on watcher to notice that
// the add-on is misbehaving.
add_task(function* test_burn_CPOW() {
  if (!gMultiProcessBrowser) {
    info("This is a single-process Firefox, we can't test for CPOW");
    return;
  }
  yield burn_rubber({
    histogramName: "PERF_MONITORING_SLOW_ADDON_CPOW_US",
    topic: "test-addonwatcher-burn-some-cpow",
    expectedMinSum: 400,
  });
});
