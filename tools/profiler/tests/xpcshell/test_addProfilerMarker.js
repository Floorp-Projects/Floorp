/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test that ChromeUtils.addProfilerMarker is working correctly.
 */

const markerNamePrefix = "test_addProfilerMarker";
const markerText = "Text payload";
// The same startTime will be used for all markers with a duration,
// and we store this value globally so that expectDuration and
// expectNoDuration can access it. The value isn't set here as we
// want a start time after the profiler has started
var startTime;

function expectNoDuration(marker) {
  Assert.equal(
    typeof marker.startTime,
    "number",
    "startTime should be a number"
  );
  Assert.greater(
    marker.startTime,
    startTime,
    "startTime should be after the begining of the test"
  );
  Assert.equal(typeof marker.endTime, "number", "endTime should be a number");
  Assert.equal(marker.endTime, 0, "endTime should be 0");
}

function expectDuration(marker) {
  Assert.equal(
    typeof marker.startTime,
    "number",
    "startTime should be a number"
  );
  // Floats can cause rounding issues. We've seen up to a 4.17e-5 difference in
  // intermittent failures, so we are permissive and accept up to 5e-5.
  Assert.less(
    Math.abs(marker.startTime - startTime),
    5e-5,
    "startTime should be the expected time"
  );
  Assert.equal(typeof marker.endTime, "number", "endTime should be a number");
  Assert.greater(
    marker.endTime,
    startTime,
    "endTime should be after startTime"
  );
}

function expectNoData(marker) {
  Assert.equal(
    typeof marker.data,
    "undefined",
    "The data property should be undefined"
  );
}

function expectText(marker) {
  Assert.equal(
    typeof marker.data,
    "object",
    "The data property should be an object"
  );
  Assert.equal(marker.data.type, "Text", "Should be a Text marker");
  Assert.equal(
    marker.data.name,
    markerText,
    "The payload should contain the expected text"
  );
}

function expectNoStack(marker) {
  Assert.ok(!marker.data || !marker.data.stack, "There should be no stack");
}

function expectStack(marker, thread) {
  let stack = marker.data.stack;
  Assert.ok(!!stack, "There should be a stack");

  // Marker stacks are recorded as a profile of a thread with a single sample,
  // get the stack id.
  stack = stack.samples.data[0][stack.samples.schema.stack];

  const stackPrefixCol = thread.stackTable.schema.prefix;
  const stackFrameCol = thread.stackTable.schema.frame;
  const frameLocationCol = thread.frameTable.schema.location;

  // Get the entire stack in an array for easier processing.
  let result = [];
  while (stack != null) {
    let stackEntry = thread.stackTable.data[stack];
    let frame = thread.frameTable.data[stackEntry[stackFrameCol]];
    result.push(thread.stringTable[frame[frameLocationCol]]);
    stack = stackEntry[stackPrefixCol];
  }

  Assert.greaterOrEqual(
    result.length,
    1,
    "There should be at least one frame in the stack"
  );

  Assert.ok(
    result.some(frame => frame.includes("testMarker")),
    "the 'testMarker' function should be visible in the stack"
  );

  Assert.ok(
    !result.some(frame => frame.includes("ChromeUtils.addProfilerMarker")),
    "the 'ChromeUtils.addProfilerMarker' label frame should not be visible in the stack"
  );
}

add_task(async () => {
  startProfilerForMarkerTests();
  startTime = Cu.now();
  while (Cu.now() < startTime + 1) {
    // Busy wait for 1ms to ensure the intentionally set start time of markers
    // will be significantly different from the time at which the marker is
    // recorded.
  }
  info("startTime used for markers with durations: " + startTime);

  /* Each call to testMarker will record a marker with a unique name.
   * The testFunctions and testCases objects contain respectively test
   * functions to verify that the marker found in the captured profile
   * matches expectations, and a string that can be printed to describe
   * in which way ChromeUtils.addProfilerMarker was called. */
  let testFunctions = {};
  let testCases = {};
  let markerId = 0;
  function testMarker(args, checks) {
    let name = markerNamePrefix + markerId++;
    ChromeUtils.addProfilerMarker(name, ...args);
    testFunctions[name] = checks;
    testCases[name] = `ChromeUtils.addProfilerMarker(${[name, ...args]
      .toSource()
      .slice(1, -1)})`;
  }

  info("Record markers without options object.");
  testMarker([], m => {
    expectNoDuration(m);
    expectNoData(m);
  });
  testMarker([startTime], m => {
    expectDuration(m);
    expectNoData(m);
  });
  testMarker([undefined, markerText], m => {
    expectNoDuration(m);
    expectText(m);
  });
  testMarker([startTime, markerText], m => {
    expectDuration(m);
    expectText(m);
  });

  info("Record markers providing the duration as the startTime property.");
  testMarker([{ startTime }], m => {
    expectDuration(m);
    expectNoData(m);
  });
  testMarker([{}, markerText], m => {
    expectNoDuration(m);
    expectText(m);
  });
  testMarker([{ startTime }, markerText], m => {
    expectDuration(m);
    expectText(m);
  });

  info("Record markers to test the captureStack property.");
  const captureStack = true;
  testMarker([], expectNoStack);
  testMarker([startTime, markerText], expectNoStack);
  testMarker([{ captureStack: false }], expectNoStack);
  testMarker([{ captureStack }], expectStack);
  testMarker([{ startTime, captureStack }], expectStack);
  testMarker([{ captureStack }, markerText], expectStack);
  testMarker([{ startTime, captureStack }, markerText], expectStack);

  info("Record markers to test the category property");
  function testCategory(args, expectedCategory) {
    testMarker(args, marker => {
      Assert.equal(marker.category, expectedCategory);
    });
  }
  testCategory([], "JavaScript");
  testCategory([{ category: "Test" }], "Test");
  testCategory([{ category: "Test" }, markerText], "Test");
  testCategory([{ category: "JavaScript" }], "JavaScript");
  testCategory([{ category: "Other" }], "Other");
  testCategory([{ category: "DOM" }], "DOM");
  testCategory([{ category: "does not exist" }], "Other");

  info("Capture the profile");
  const profile = await stopNowAndGetProfile();
  const mainThread = profile.threads.find(({ name }) => name === "GeckoMain");
  const markers = getInflatedMarkerData(mainThread).filter(m =>
    m.name.startsWith(markerNamePrefix)
  );
  Assert.equal(
    markers.length,
    Object.keys(testFunctions).length,
    `Found ${markers.length} test markers in the captured profile`
  );

  for (let marker of markers) {
    marker.category = profile.meta.categories[marker.category].name;
    info(`${testCases[marker.name]} -> ${marker.toSource()}`);

    testFunctions[marker.name](marker, mainThread);
    delete testFunctions[marker.name];
  }

  Assert.equal(0, Object.keys(testFunctions).length, "all markers were found");
});
