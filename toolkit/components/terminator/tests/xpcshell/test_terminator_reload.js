/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Test that the Shutdown Terminator reloads durations correctly

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

var { Path, Constants } = OS;

var PATH;

var HISTOGRAMS = {
  "quit-application": "SHUTDOWN_PHASE_DURATION_TICKS_QUIT_APPLICATION",
  "profile-change-net-teardown":
    "SHUTDOWN_PHASE_DURATION_TICKS_PROFILE_CHANGE_NET_TEARDOWN",
  "profile-change-teardown":
    "SHUTDOWN_PHASE_DURATION_TICKS_PROFILE_CHANGE_TEARDOWN",
  "profile-before-change":
    "SHUTDOWN_PHASE_DURATION_TICKS_PROFILE_BEFORE_CHANGE",
  "profile-before-change-qm":
    "SHUTDOWN_PHASE_DURATION_TICKS_PROFILE_BEFORE_CHANGE_QM",
  "xpcom-will-shutdown": "SHUTDOWN_PHASE_DURATION_TICKS_XPCOM_WILL_SHUTDOWN",
  "xpcom-shutdown": "SHUTDOWN_PHASE_DURATION_TICKS_XPCOM_SHUTDOWN",
};

add_task(async function init() {
  do_get_profile();
  PATH = Path.join(Constants.Path.localProfileDir, "ShutdownDuration.json");
});

add_task(async function test_reload() {
  info("Forging data");
  let data = {};
  let telemetrySnapshots = Services.telemetry.getSnapshotForHistograms(
    "main",
    false /* clear */
  ).parent;
  let i = 0;
  for (let k of Object.keys(HISTOGRAMS)) {
    let id = HISTOGRAMS[k];
    data[k] = i++;
    Assert.equal(
      telemetrySnapshots[id] || undefined,
      undefined,
      "Histogram " + id + " is empty"
    );
  }

  await OS.File.writeAtomic(PATH, JSON.stringify(data));

  const TOPIC = "shutdown-terminator-telemetry-updated";

  let wait = new Promise(resolve =>
    Services.obs.addObserver(function observer() {
      info("Telemetry has been updated");
      Services.obs.removeObserver(observer, TOPIC);
      resolve();
    }, TOPIC)
  );

  info("Starting nsTerminatorTelemetry");
  let tt = Cc[
    "@mozilla.org/toolkit/shutdown-terminator-telemetry;1"
  ].createInstance(Ci.nsIObserver);
  tt.observe(null, "profile-after-change", "");

  info("Waiting until telemetry is updated");
  // Now wait until Telemetry is updated
  await wait;

  telemetrySnapshots = Services.telemetry.getSnapshotForHistograms(
    "main",
    false /* clear */
  ).parent;
  for (let k of Object.keys(HISTOGRAMS)) {
    let id = HISTOGRAMS[k];
    info("Testing histogram " + id);
    let snapshot = telemetrySnapshots[id];
    let count = 0;
    for (let x of Object.values(snapshot.values)) {
      count += x;
    }
    Assert.equal(count, 1, "We have added one item");
  }
});
