/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";


// Test that the Shutdown Terminator reloads durations correctly

var Cu = Components.utils;
var Cc = Components.classes;
var Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);

var {Path, File, Constants} = OS;

var PATH;

var HISTOGRAMS = {
  "quit-application": "SHUTDOWN_PHASE_DURATION_TICKS_QUIT_APPLICATION",
  "profile-change-teardown": "SHUTDOWN_PHASE_DURATION_TICKS_PROFILE_CHANGE_TEARDOWN",
  "profile-before-change":  "SHUTDOWN_PHASE_DURATION_TICKS_PROFILE_BEFORE_CHANGE",
  "xpcom-will-shutdown": "SHUTDOWN_PHASE_DURATION_TICKS_XPCOM_WILL_SHUTDOWN",
};

add_task(function* init() {
  do_get_profile();
  PATH = Path.join(Constants.Path.localProfileDir, "ShutdownDuration.json");
});

add_task(function* test_reload() {
  do_print("Forging data");
  let data = {};
  let telemetrySnapshots = Services.telemetry.histogramSnapshots;
  let i = 0;
  for (let k of Object.keys(HISTOGRAMS)) {
    let id = HISTOGRAMS[k];
    data[k] = i++;
    Assert.equal(telemetrySnapshots[id] || undefined, undefined, "Histogram " + id + " is empty");
  }


  yield OS.File.writeAtomic(PATH, JSON.stringify(data));

  const TOPIC = "shutdown-terminator-telemetry-updated";

  let wait = new Promise(resolve =>
    Services.obs.addObserver(
      function observer() {
        do_print("Telemetry has been updated");
        Services.obs.removeObserver(observer, TOPIC);
        resolve();
      },
      TOPIC,
      false));

  do_print("Starting nsTerminatorTelemetry");
  let tt = Cc["@mozilla.org/toolkit/shutdown-terminator-telemetry;1"].
    createInstance(Ci.nsIObserver);
  tt.observe(null, "profile-after-change", "");

  do_print("Waiting until telemetry is updated");
  // Now wait until Telemetry is updated
  yield wait;

  telemetrySnapshots = Services.telemetry.histogramSnapshots;
  for (let k of Object.keys(HISTOGRAMS)) {
    let id = HISTOGRAMS[k];
    do_print("Testing histogram " + id);
    let snapshot = telemetrySnapshots[id];
    let count = 0;
    for (let x of snapshot.counts) {
      count += x;
    }
    Assert.equal(count, 1, "We have added one item");
  }

});

function run_test() {
  run_next_test();
}
