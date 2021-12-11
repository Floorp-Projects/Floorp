/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

// Test that the Shutdown Terminator records durations correctly

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

var { Path, File, Constants } = OS;

var PATH;
var PATH_TMP;
var terminator;

let KEYS = [
  "quit-application",
  "profile-change-net-teardown",
  "profile-change-teardown",
  "profile-before-change",
  "profile-before-change-qm",
  "profile-before-change-telemetry",
  "xpcom-will-shutdown",
  "xpcom-shutdown",
];

add_task(async function init() {
  do_get_profile();
  PATH = Path.join(Constants.Path.localProfileDir, "ShutdownDuration.json");
  PATH_TMP = PATH + ".tmp";

  // Initialize the terminator
  // (normally, this is done through the manifest file, but xpcshell
  // doesn't take them into account).
  info("Initializing the Terminator");
  terminator = Cc["@mozilla.org/toolkit/shutdown-terminator;1"].createInstance(
    Ci.nsIObserver
  );
});

var promiseShutdownDurationData = async function() {
  // Wait until PATH exists.
  // Timeout if it is never created.
  info("Waiting for file creation: " + PATH);
  while (true) {
    if (await OS.File.exists(PATH)) {
      break;
    }

    info("The file does not exist yet. Waiting 1 second.");
    await new Promise(resolve => setTimeout(resolve, 1000));
  }

  info("The file has been created");
  let raw = await OS.File.read(PATH, { encoding: "utf-8" });
  info(raw);
  return JSON.parse(raw);
};

add_task(async function test_record() {
  let t0 = Date.now();

  info("Starting shutdown");
  terminator.observe(null, "terminator-test-" + KEYS[2], null);
  await new Promise(resolve => setTimeout(resolve, 200));

  info("Moving to next phase");
  terminator.observe(null, "terminator-test-" + KEYS[3], null);
  await new Promise(resolve => setTimeout(resolve, 100));

  let data = await promiseShutdownDurationData();

  let t1 = Date.now();

  Assert.ok(KEYS[2] in data, "The file contains the expected key");
  let duration = data[KEYS[2]];
  Assert.equal(typeof duration, "number");
  Assert.ok(duration >= 0, "Duration is a non-negative number");
  Assert.ok(
    duration <= Math.ceil((t1 - t0) / 1000) + 1,
    "Duration is reasonable"
  );

  Assert.equal(
    Object.keys(data).length,
    2,
    "Data does not contain other durations"
  );

  info("Cleaning up and moving to next phase");
  await File.remove(PATH);
  await File.remove(PATH_TMP);

  info("Waiting at least one tick");
  let WAIT_MS = 2000;
  await new Promise(resolve => setTimeout(resolve, WAIT_MS));

  terminator.observe(null, "terminator-test-" + KEYS[4], null);
  data = await promiseShutdownDurationData();

  let t2 = Date.now();

  Assert.equal(
    Object.keys(data)
      .sort()
      .join(", "),
    [KEYS[2], KEYS[3], KEYS[4]].sort().join(", "),
    "The file contains the expected keys"
  );
  Assert.equal(data[KEYS[2]], duration, "Duration of phase 0 hasn't changed");
  let duration2 = data[KEYS[3]];
  Assert.equal(typeof duration2, "number");
  // XXX: It seems, we have quite some jitter here, so we cannot rely
  // ticks to be an accurate measure of time and thus accept lower values.
  Assert.ok(
    duration2 >= WAIT_MS / 100 / 2,
    "We have waited at least " + WAIT_MS / 100 / 2 + " ticks"
  );
  Assert.ok(
    duration2 <= Math.ceil((t2 - t1) / 100) + 1,
    "Duration is reasonable"
  );
});
