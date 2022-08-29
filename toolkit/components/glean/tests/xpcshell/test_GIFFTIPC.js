/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const Telemetry = Services.telemetry;
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

function sleep(ms) {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  return new Promise(resolve => setTimeout(resolve, ms));
}

function scalarValue(aScalarName, aProcessName) {
  let snapshot = Telemetry.getSnapshotForScalars();
  return aProcessName in snapshot
    ? snapshot[aProcessName][aScalarName]
    : undefined;
}

function keyedScalarValue(aScalarName, aProcessName) {
  let snapshot = Telemetry.getSnapshotForKeyedScalars();
  return aProcessName in snapshot
    ? snapshot[aProcessName][aScalarName]
    : undefined;
}

add_setup({ skip_if: () => !runningInParent }, function test_setup() {
  // Give FOG a temp profile to init within.
  do_get_profile();

  // Allows these tests to properly run on e.g. Thunderbird
  Services.prefs.setBoolPref(
    "toolkit.telemetry.testing.overrideProductsCheck",
    true
  );

  // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
  // on Android FOG is set up through head.js
  if (AppConstants.platform != "android") {
    Services.fog.initializeFOG();
  }
});

const COUNT = 42;
const CHEESY_STRING = "a very cheesy string!";
const CHEESIER_STRING = "a much cheesier string!";
const CUSTOM_SAMPLES = [3, 4];
const EVENT_EXTRA = { extra1: "so very extra" };
const MEMORIES = [13, 31];
const MEMORY_BUCKETS = ["13193", "31378"]; // buckets are strings : |
const A_LABEL_COUNT = 3;
const ANOTHER_LABEL_COUNT = 5;
const INVALID_COUNTERS = 7;
const IRATE_NUMERATOR = 44;
const IRATE_DENOMINATOR = 14;

add_task({ skip_if: () => runningInParent }, async function run_child_stuff() {
  let oldCanRecordBase = Telemetry.canRecordBase;
  Telemetry.canRecordBase = true; // Ensure we're able to record things.

  Glean.testOnlyIpc.aCounter.add(COUNT);
  Glean.testOnlyIpc.aStringList.add(CHEESY_STRING);
  Glean.testOnlyIpc.aStringList.add(CHEESIER_STRING);

  Glean.testOnlyIpc.noExtraEvent.record();
  Glean.testOnlyIpc.anEvent.record(EVENT_EXTRA);

  for (let memory of MEMORIES) {
    Glean.testOnlyIpc.aMemoryDist.accumulate(memory);
  }

  let t1 = Glean.testOnlyIpc.aTimingDist.start();
  let t2 = Glean.testOnlyIpc.aTimingDist.start();

  await sleep(5);

  let t3 = Glean.testOnlyIpc.aTimingDist.start();
  Glean.testOnlyIpc.aTimingDist.cancel(t1);

  await sleep(5);

  Glean.testOnlyIpc.aTimingDist.stopAndAccumulate(t2); // 10ms
  Glean.testOnlyIpc.aTimingDist.stopAndAccumulate(t3); // 5ms

  Glean.testOnlyIpc.aCustomDist.accumulateSamples(CUSTOM_SAMPLES);

  Glean.testOnlyIpc.aLabeledCounter.a_label.add(A_LABEL_COUNT);
  Glean.testOnlyIpc.aLabeledCounter.another_label.add(ANOTHER_LABEL_COUNT);

  // Has to be different from aLabeledCounter so the error we record doesn't
  // get in the way.
  Glean.testOnlyIpc.anotherLabeledCounter.InvalidLabel.add(INVALID_COUNTERS);

  Glean.testOnlyIpc.irate.addToNumerator(IRATE_NUMERATOR);
  Glean.testOnlyIpc.irate.addToDenominator(IRATE_DENOMINATOR);
  Telemetry.canRecordBase = oldCanRecordBase;
});

add_task(
  { skip_if: () => !runningInParent },
  async function test_child_metrics() {
    Telemetry.setEventRecordingEnabled("telemetry.test", true);

    // Clear any stray Telemetry data
    Telemetry.clearScalars();
    Telemetry.getSnapshotForHistograms("main", true);
    Telemetry.clearEvents();

    await run_test_in_child("test_GIFFTIPC.js");

    // Wait for both IPC mechanisms to flush.
    await Services.fog.testFlushAllChildren();
    await ContentTaskUtils.waitForCondition(() => {
      let snapshot = Telemetry.getSnapshotForKeyedScalars();
      return (
        "content" in snapshot &&
        "telemetry.test.mirror_for_rate" in snapshot.content
      );
    }, "failed to find content telemetry in parent");

    // boolean
    // Doesn't work over IPC

    // counter
    Assert.equal(Glean.testOnlyIpc.aCounter.testGetValue(), COUNT);
    Assert.equal(
      scalarValue("telemetry.test.mirror_for_counter", "content"),
      COUNT,
      "content-process Scalar has expected count"
    );

    // custom_distribution
    const customSampleSum = CUSTOM_SAMPLES.reduce((acc, a) => acc + a, 0);
    const customData = Glean.testOnlyIpc.aCustomDist.testGetValue("store1");
    Assert.equal(customSampleSum, customData.sum, "Sum's correct");
    for (let [bucket, count] of Object.entries(customData.values)) {
      Assert.ok(
        count == 0 || (count == CUSTOM_SAMPLES.length && bucket == 1), // both values in the low bucket
        `Only two buckets have a sample ${bucket} ${count}`
      );
    }
    const histSnapshot = Telemetry.getSnapshotForHistograms(
      "main",
      false,
      false
    );
    const histData = histSnapshot.content.TELEMETRY_TEST_MIRROR_FOR_CUSTOM;
    Assert.equal(customSampleSum, histData.sum, "Sum in histogram's correct");
    Assert.equal(2, histData.values["1"], "Two samples in the first bucket");

    // datetime
    // Doesn't work over IPC

    // event
    var events = Glean.testOnlyIpc.noExtraEvent.testGetValue();
    Assert.equal(1, events.length);
    Assert.equal("test_only.ipc", events[0].category);
    Assert.equal("no_extra_event", events[0].name);

    events = Glean.testOnlyIpc.anEvent.testGetValue();
    Assert.equal(1, events.length);
    Assert.equal("test_only.ipc", events[0].category);
    Assert.equal("an_event", events[0].name);
    Assert.deepEqual(EVENT_EXTRA, events[0].extra);

    TelemetryTestUtils.assertEvents(
      [
        [
          "telemetry.test",
          "not_expired_optout",
          "object1",
          undefined,
          undefined,
        ],
        ["telemetry.test", "mirror_with_extra", "object1", null, EVENT_EXTRA],
      ],
      { category: "telemetry.test" },
      { process: "content" }
    );

    // labeled_boolean
    // Doesn't work over IPC

    // labeled_counter
    const counters = Glean.testOnlyIpc.aLabeledCounter;
    Assert.equal(counters.a_label.testGetValue(), A_LABEL_COUNT);
    Assert.equal(counters.another_label.testGetValue(), ANOTHER_LABEL_COUNT);

    Assert.throws(
      () => Glean.testOnlyIpc.anotherLabeledCounter.__other__.testGetValue(),
      /NS_ERROR_LOSS_OF_SIGNIFICANT_DATA/,
      "Invalid labels record errors, which throw"
    );

    let value = keyedScalarValue(
      "telemetry.test.another_mirror_for_labeled_counter",
      "content"
    );
    Assert.deepEqual(
      {
        a_label: A_LABEL_COUNT,
        another_label: ANOTHER_LABEL_COUNT,
      },
      value
    );
    value = keyedScalarValue(
      "telemetry.test.mirror_for_labeled_counter",
      "content"
    );
    Assert.deepEqual(
      {
        InvalidLabel: INVALID_COUNTERS,
      },
      value
    );

    // labeled_string
    // Doesn't work over IPC

    // memory_distribution
    const memoryData = Glean.testOnlyIpc.aMemoryDist.testGetValue();
    const memorySum = MEMORIES.reduce((acc, a) => acc + a, 0);
    // The sum's in bytes, but the metric's in KB
    Assert.equal(memorySum * 1024, memoryData.sum);
    for (let [bucket, count] of Object.entries(memoryData.values)) {
      // We could assert instead, but let's skip to save the logspam.
      if (count == 0) {
        continue;
      }
      Assert.ok(count == 1 && MEMORY_BUCKETS.includes(bucket));
    }

    const memoryHist = histSnapshot.content.TELEMETRY_TEST_LINEAR;
    Assert.equal(
      memorySum,
      memoryHist.sum,
      "Histogram's in `memory_unit` units"
    );
    Assert.equal(2, memoryHist.values["1"], "Samples are in the right bucket");

    // quantity
    // Doesn't work over IPC

    // rate
    Assert.deepEqual(
      { numerator: IRATE_NUMERATOR, denominator: IRATE_DENOMINATOR },
      Glean.testOnlyIpc.irate.testGetValue()
    );
    Assert.deepEqual(
      { numerator: IRATE_NUMERATOR, denominator: IRATE_DENOMINATOR },
      keyedScalarValue("telemetry.test.mirror_for_rate", "content")
    );

    // string
    // Doesn't work over IPC

    // string_list
    // Note: this will break if string list ever rearranges its items.
    const cheesyStrings = Glean.testOnlyIpc.aStringList.testGetValue();
    Assert.deepEqual(cheesyStrings, [CHEESY_STRING, CHEESIER_STRING]);
    // Note: this will break if keyed scalars rearrange their items.
    Assert.deepEqual(
      {
        [CHEESY_STRING]: true,
        [CHEESIER_STRING]: true,
      },
      keyedScalarValue("telemetry.test.keyed_boolean_kind", "content")
    );

    // timespan
    // Doesn't work over IPC

    // timing_distribution
    const NANOS_IN_MILLIS = 1e6;
    const EPSILON = 40000; // bug 1701949
    const times = Glean.testOnlyIpc.aTimingDist.testGetValue();
    Assert.greater(times.sum, 15 * NANOS_IN_MILLIS - EPSILON);
    // We can't guarantee any specific time values (thank you clocks),
    // but we can assert there are only two samples.
    Assert.equal(
      2,
      Object.entries(times.values).reduce(
        (acc, [bucket, count]) => acc + count,
        0
      )
    );
    const timingHist = histSnapshot.content.TELEMETRY_TEST_EXPONENTIAL;
    Assert.greaterOrEqual(timingHist.sum, 13, "Histogram's in milliseconds.");
    // Both values, 10 and 5, are truncated by a cast in AccumulateTimeDelta
    // Minimally downcast 9. + 4. could realistically result in 13.
    Assert.equal(
      2,
      Object.entries(timingHist.values).reduce(
        (acc, [bucket, count]) => acc + count,
        0
      ),
      "Only two samples"
    );

    // uuid
    // Doesn't work over IPC
  }
);
