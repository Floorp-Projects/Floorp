/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable mozilla/no-arbitrary-setTimeout */

"use strict";

// Test that the Shutdown Terminator records durations correctly

const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

var { Path, File, Constants } = OS;

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
  while (true) {
    if (await OS.File.exists(PATH)) {
      break;
    }

    // Wait just a very short period to not increase measured values.
    // Usually the file should appear almost immediately.
    await new Promise(resolve => setTimeout(resolve, 50));
  }

  let raw = await OS.File.read(PATH, { encoding: "utf-8" });
  info("Read file: " + raw);
  return JSON.parse(raw);
};

var currentPhase = 0;

var advancePhase = async function() {
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

add_task(async function test_record() {
  info("Collecting duration data for all known phases");
  let morePhases = true;
  while (morePhases) {
    morePhases = await advancePhase();

    await File.remove(PATH);
    await File.remove(PATH_TMP);
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
      let msNominalDuration = 200 + HEARTBEAT_MS * i;
      Assert.lessOrEqual(
        ticksDuration,
        Math.ceil(msNominalDuration / HEARTBEAT_MS) + 1,
        "Duration of phase " + i + ":" + KEYS[i] + " is not too long"
      );
      // It seems that the heartbeat based measure is not very accurate
      // (ticks tend to be less than expected), so we give us some tolerance.
      // See bug 1768795.
      let halfNominalTicks = Math.floor(msNominalDuration / 2 / HEARTBEAT_MS);
      Assert.greaterOrEqual(
        ticksDuration,
        halfNominalTicks,
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
