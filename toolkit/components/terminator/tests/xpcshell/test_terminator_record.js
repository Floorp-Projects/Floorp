/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

// Test that the Shutdown Terminator records durations correctly

const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

var PATH;
var PATH_TMP;
var terminator;

var HEARTBEAT_MS = 100;

let KEYS = [
  "quit-application",
  "profile-change-net-teardown",
  "profile-change-teardown",
  "profile-before-change",
  "profile-before-change-qm",
  "xpcom-will-shutdown",
  "xpcom-shutdown",
  "xpcom-shutdown-threads",
  "XPCOMShutdownFinal",
  "CCPostLastCycleCollection",
];

let DATA = [];
let MeasuredDurations = [];

add_task(async function init() {
  do_get_profile();
  PATH = PathUtils.join(PathUtils.localProfileDir, "ShutdownDuration.json");
  PATH_TMP = PATH + ".tmp";

  // Initialize the terminator
  // (normally, this is done through the manifest file, but xpcshell
  // doesn't take them into account).
  info("Initializing the Terminator");
  terminator = Cc["@mozilla.org/toolkit/shutdown-terminator;1"].createInstance(
    Ci.nsIObserver
  );
});

var promiseShutdownDurationData = async function () {
  // Wait until PATH exists.
  // Timeout if it is never created.
  while (true) {
    if (await IOUtils.exists(PATH)) {
      break;
    }

    // Wait just a very short period to not increase measured values.
    // Usually the file should appear almost immediately.
    await new Promise(resolve => setTimeout(resolve, 50));
  }

  return IOUtils.readJSON(PATH);
};

var currentPhase = 0;

var advancePhase = async function () {
  let key = "terminator-test-" + KEYS[currentPhase];
  let msDuration = 200 + HEARTBEAT_MS * currentPhase;

  info("Advancing shutdown phase to " + KEYS[currentPhase]);
  terminator.observe(null, key, null);
  await new Promise(resolve => setTimeout(resolve, msDuration));

  let data = await promiseShutdownDurationData();

  Assert.ok(KEYS[currentPhase] in data, "The file contains the expected key");
  Assert.equal(
    Object.keys(data).length,
    currentPhase + 1,
    "File does not contain more durations than expected"
  );

  DATA[currentPhase] = data;
  currentPhase++;
  if (currentPhase < KEYS.length) {
    return true;
  }
  return false;
};

// This is a timing affected test, as we want to check if the time measurements
// from the terminator are reasonable. Bug 1768795 assumes that they tend to
// be lower than wall-clock, in particular on MacOS, confirmed by the logs on
// intermittent bug 1760094. This is not a big deal for the terminator's
// general functionality (timeouts might just come a little later than
// expected nominally), but it makes testing harder and the transferred
// telemetry data slightly less reliable (shutdowns might appear shorter than
// they really were). So this test is just happy if there is any data that
// is not too long wrt what we expect. If we ever want to fix bug 1768795,
// we can check for a more reasonable lower boundary, too.
add_task(async function test_record() {
  info("Collecting duration data for all known phases");

  let morePhases = true;
  while (morePhases) {
    let beforeWait = Date.now();

    morePhases = await advancePhase();

    await IOUtils.remove(PATH);
    await IOUtils.remove(PATH_TMP);

    // We measure the effective time that passed as wall-clock and include all
    // file IO overhead as the terminator will do so in its measurement, too.
    MeasuredDurations[currentPhase - 1] = Math.floor(
      (Date.now() - beforeWait) / HEARTBEAT_MS
    );
  }

  Assert.equal(DATA.length, KEYS.length, "We have data for each phase");

  for (let i = 0; i < KEYS.length; i++) {
    let lastDuration = DATA[KEYS.length - 1][KEYS[i]];
    Assert.equal(
      typeof lastDuration,
      "number",
      "Duration of phase " + i + ":" + KEYS[i] + " is a number"
    );

    // The durations are only meaningful after we advanced to the next phase.
    if (i < KEYS.length - 1) {
      // So we read it from the data written for the following phase.
      let ticksDuration = DATA[i + 1][KEYS[i]];
      let measuredDuration = MeasuredDurations[i];
      info(
        "measuredDuration:" + measuredDuration + " - " + typeof measuredDuration
      );
      Assert.lessOrEqual(
        ticksDuration,
        measuredDuration + 2,
        "Duration of phase " + i + ":" + KEYS[i] + " is not too long"
      );
      Assert.greaterOrEqual(
        ticksDuration,
        0, // TODO: Raise the lower boundary after bug 1768795.
        "Duration of phase " + i + ":" + KEYS[i] + " is not too short"
      );
    }
    // This check is done only for phases <= xpcom-shutdown-threads
    // where we have two data points.
    if (i < KEYS.length - 2) {
      let ticksDuration = DATA[i + 1][KEYS[i]];
      Assert.equal(
        ticksDuration,
        DATA[KEYS.length - 1][KEYS[i]],
        "Duration of phase " + i + ":" + KEYS[i] + " hasn't changed"
      );
    }
  }

  // Note that after this check the KEYS array remains sorted, so this
  // must be the last check to not get confused.
  Assert.equal(
    Object.keys(DATA[KEYS.length - 1])
      .sort()
      .join(", "),
    KEYS.sort().join(", "),
    "The last file contains all expected keys"
  );
});
