/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";


// Test that the Shutdown Terminator records durations correctly

const Cu = Components.utils;
const Cc = Components.classes;
const Ci = Components.interfaces;

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);

let {Path, File, Constants} = OS;

let PATH;
let PATH_TMP;
let terminator;

add_task(function* init() {
  do_get_profile();
  PATH = Path.join(Constants.Path.localProfileDir, "ShutdownDuration.json");
  PATH_TMP = PATH + ".tmp";

  // Initialize the terminator
  // (normally, this is done through the manifest file, but xpcshell
  // doesn't take them into account).
  do_print("Initializing the Terminator");
  terminator = Cc["@mozilla.org/toolkit/shutdown-terminator;1"].
    createInstance(Ci.nsIObserver);
  terminator.observe(null, "profile-after-change", null);
});

let promiseShutdownDurationData = Task.async(function*() {
  // Wait until PATH exists.
  // Timeout if it is never created.
  do_print("Waiting for file creation: " + PATH);
  while (true) {
    if ((yield OS.File.exists(PATH))) {
      break;
    }

    do_print("The file does not exist yet. Waiting 1 second.");
    yield new Promise(resolve => setTimeout(resolve, 1000));
  }

  do_print("The file has been created");
  let raw = yield OS.File.read(PATH, { encoding: "utf-8"} );
  do_print(raw);
  return JSON.parse(raw);
});

add_task(function* test_record() {
  let PHASE0 = "profile-change-teardown";
  let PHASE1 = "profile-before-change";
  let PHASE2 = "xpcom-will-shutdown";
  let t0 = Date.now();

  do_print("Starting shutdown");
  terminator.observe(null, "profile-change-teardown", null);

  do_print("Moving to next phase");
  terminator.observe(null, PHASE1, null);

  let data = yield promiseShutdownDurationData();

  let t1 = Date.now();

  Assert.ok(PHASE0 in data, "The file contains the expected key");
  let duration = data[PHASE0];
  Assert.equal(typeof duration, "number");
  Assert.ok(duration >= 0, "Duration is a non-negative number");
  Assert.ok(duration <= Math.ceil((t1 - t0) / 1000) + 1,
    "Duration is reasonable");

  Assert.equal(Object.keys(data).length, 1, "Data does not contain other durations");

  do_print("Cleaning up and moving to next phase");
  yield File.remove(PATH);
  yield File.remove(PATH_TMP);

  do_print("Waiting at least one tick");
  let WAIT_MS = 2000;
  yield new Promise(resolve => setTimeout(resolve, WAIT_MS));

  terminator.observe(null, PHASE2, null);
  data = yield promiseShutdownDurationData();

  let t2 = Date.now();

  Assert.equal(Object.keys(data).sort().join(", "),
               [PHASE0, PHASE1].sort().join(", "),
               "The file contains the expected keys");
  Assert.equal(data[PHASE0], duration, "Duration of phase 0 hasn't changed");
  let duration2 = data[PHASE1];
  Assert.equal(typeof duration2, "number");
  Assert.ok(duration2 >= WAIT_MS / 2000, "We have waited at least " + (WAIT_MS / 2000) + " ticks");
  Assert.ok(duration2 <= Math.ceil((t2 - t1) / 1000) + 1,
    "Duration is reasonable");
});

function run_test() {
  run_next_test();
}
