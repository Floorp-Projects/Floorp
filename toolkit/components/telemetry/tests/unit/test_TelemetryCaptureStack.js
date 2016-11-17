/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/AppConstants.jsm", this);

/**
 * Ensures that the sctucture of the javascript object used for capturing stacks
 * is as intended. The structure is expected to be as in this example:
 *
 * {
 *  "memoryMap": [
 *      [String, String],
 *      ...
 *   ],
 *   "stacks": [
 *      [
 *         [Integer, Integer], // Frame
 *         ...
 *      ],
 *      ...
 *   ],
 *   "captures": [
 *      [String, Integer, Integer],
 *      ...
 *   ]
 * }
 *
 * @param {Object} obj  abject to be inpected vor validity.
 *
 * @return {Boolean} True if the structure is valid. False - otherwise.
 */
function checkObjectStructure(obj) {
  // Ensuring an object is given.
  if (!obj || typeof obj !== "object") {
    return false;
  }

  // Ensuring all properties exist inside the object and are arrays.
  for (let property of ["memoryMap", "stacks", "captures"]) {
    if (!(property in obj) || !Array.isArray(obj[property]))
      return false;
  }

  return true;
}

/**
 * A helper for triggering a stack capture and returning the new state of stacks.
 *
 * @param {String}  key   The key for capturing stack.
 * @param {Boolean} clear True to reset captured stacks, False - otherwise.
 *
 * @return {Object} captured stacks.
 */
function captureStacks(key, clear = true) {
  Telemetry.captureStack(key);
  let stacks = Telemetry.snapshotCapturedStacks(clear);
  Assert.ok(checkObjectStructure(stacks));
  return stacks;
}

const TEST_STACK_KEYS = ["TEST-KEY1", "TEST-KEY2"];

/**
 * Ensures that captured stacks appear in pings, if any were captured.
 */
add_task({
  skip_if: () => !AppConstants.MOZ_ENABLE_PROFILER_SPS
}, function* test_capturedStacksAppearInPings() {
  yield TelemetryController.testSetup();
  captureStacks("DOES-NOT-MATTER", false);

  let ping = TelemetryController.getCurrentPingData();
  Assert.ok("capturedStacks" in ping.payload.processes.parent);

  let capturedStacks = ping.payload.processes.parent.capturedStacks;
  Assert.ok(checkObjectStructure(capturedStacks));
});

/**
 * Ensures that capturing a stack for a new key increases the number
 * of captured stacks and adds a new entry to captures.
 */
add_task({
  skip_if: () => !AppConstants.MOZ_ENABLE_PROFILER_SPS
}, function* test_CaptureStacksIncreasesNumberOfCapturedStacks() {
  // Construct a unique key for this test.
  let key = TEST_STACK_KEYS[0] + "-UNIQUE-KEY-1";

  // Ensure that no captures for the key exist.
  let original = Telemetry.snapshotCapturedStacks();
  Assert.equal(undefined, original.captures.find(capture => capture[0] === key));

  // Capture stack and find updated capture stats for TEST_STACK_KEYS[0].
  let updated = captureStacks(key);

  // Ensure that a new element has been appended to both stacks and captures.
  Assert.equal(original.stacks.length + 1, updated.stacks.length);
  Assert.equal(original.captures.length + 1, updated.captures.length);

  // Ensure that the capture info for the key exists and structured well.
  Assert.deepEqual(
    [key, original.stacks.length, 1],
    updated.captures.find(capture => capture[0] === key)
  );
});

/**
 * Ensures that stacks are grouped by the key. If a stack is captured
 * more than once for the key, the length of stacks does not increase.
 */
 add_task({
   skip_if: () => !AppConstants.MOZ_ENABLE_PROFILER_SPS
 }, function* test_CaptureStacksGroupsDuplicateStacks() {
  // Make sure that there are initial captures for TEST_STACK_KEYS[0].
  let stacks = captureStacks(TEST_STACK_KEYS[0], false);
  let original = {
    captures: stacks.captures.find(capture => capture[0] === TEST_STACK_KEYS[0]),
    stacks: stacks.stacks
  };

  // Capture stack and find updated capture stats for TEST_STACK_KEYS[0].
  stacks = captureStacks(TEST_STACK_KEYS[0]);
  let updated = {
    captures: stacks.captures.find(capture => capture[0] === TEST_STACK_KEYS[0]),
    stacks: stacks.stacks
  };

  // The length of captured stacks should remain same.
  Assert.equal(original.stacks.length, updated.stacks.length);

  // We expect the info on captures to look as original. Only
  // stack counter should be increased by one.
  let expectedCaptures = original.captures;
  expectedCaptures[2]++;
  Assert.deepEqual(expectedCaptures, updated.captures);
});

/**
 * Ensure that capturing the stack for a key does not affect info
 * for other keys.
 */
add_task({
  skip_if: () => !AppConstants.MOZ_ENABLE_PROFILER_SPS
}, function* test_CaptureStacksSeparatesInformationByKeys() {
  // Make sure that there are initial captures for TEST_STACK_KEYS[0].
  let stacks = captureStacks(TEST_STACK_KEYS[0], false);
  let original = {
    captures: stacks.captures.find(capture => capture[0] === TEST_STACK_KEYS[0]),
    stacks: stacks.stacks
  };

  // Capture stack for a new key.
  let uniqueKey = TEST_STACK_KEYS[1] + "-UNIQUE-KEY-2";
  updated = captureStacks(uniqueKey);

  // The length of captured stacks should increase to reflect the new capture.
  Assert.equal(original.stacks.length + 1, updated.stacks.length);

  // The information for TEST_STACK_KEYS[0] should remain same.
  Assert.deepEqual(
    original.captures,
    updated.captures.find(capture => capture[0] === TEST_STACK_KEYS[0])
  );
});

/**
 * Ensure that Telemetry does not allow weird keys.
 */
add_task({
  skip_if: () => !AppConstants.MOZ_ENABLE_PROFILER_SPS
}, function* test_CaptureStacksDoesNotAllowBadKey() {
  for (let badKey of [null, "KEY-!@\"#$%^&*()_"]) {
    let stacks = captureStacks(badKey);
    let captureData = stacks.captures.find(capture => capture[0] === badKey)
    Assert.ok(!captureData, `"${badKey}" should not be allowed as a key`);
  }
});

function run_test() {
  do_get_profile(true);
  Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
  run_next_test();
}
