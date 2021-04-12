/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* FIXME: Remove these global markers.
 * FOG doesn't follow the stricter naming patterns as expected by tool configuration yet.
 * See https://searchfox.org/mozilla-central/source/.eslintrc.js#24
 * Reorganizing the directory structure will take this into account.
 */
/* global add_task, Assert, do_get_profile */
"use strict";

Cu.importGlobalProperties(["Glean"]);
const { ObjectUtils } = ChromeUtils.import(
  "resource://gre/modules/ObjectUtils.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryTestUtils } = ChromeUtils.import(
  "resource://testing-common/TelemetryTestUtils.jsm"
);

const Telemetry = Services.telemetry;

function sleep(ms) {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  return new Promise(resolve => setTimeout(resolve, ms));
}

function scalarValue(aScalarName) {
  return Telemetry.getSnapshotForScalars().parent[aScalarName];
}

function keyedScalarValue(aScalarName) {
  return Telemetry.getSnapshotForKeyedScalars().parent[aScalarName];
}

add_task(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
  let FOG = Cc["@mozilla.org/toolkit/glean;1"].createInstance(Ci.nsIFOG);
  FOG.initializeFOG();
});

add_task(function test_gifft_counter() {
  Glean.testOnlyIpc.aCounter.add(20);
  Assert.equal(20, Glean.testOnlyIpc.aCounter.testGetValue());
  Assert.equal(20, scalarValue("telemetry.test.unsigned_int_kind"));
});

add_task(function test_gifft_boolean() {
  Glean.testOnlyIpc.aBool.set(false);
  Assert.equal(false, Glean.testOnlyIpc.aBool.testGetValue());
  Assert.equal(false, scalarValue("telemetry.test.boolean_kind"));
});

add_task(function test_gifft_datetime() {
  const dateStr = "2021-03-22T16:06:00";
  const value = new Date(dateStr);
  Glean.testOnlyIpc.aDate.set(value.getTime() * 1000);

  let received = Glean.testOnlyIpc.aDate.testGetValue();
  Assert.ok(received.startsWith(dateStr));
  Assert.ok(scalarValue("telemetry.test.mirror_for_date").startsWith(dateStr));
});

add_task(function test_gifft_string() {
  const value = "a string!";
  Glean.testOnlyIpc.aString.set(value);

  Assert.equal(value, Glean.testOnlyIpc.aString.testGetValue());
  Assert.equal(value, scalarValue("telemetry.test.multiple_stores_string"));
});

add_task(function test_gifft_memory_dist() {
  Glean.testOnlyIpc.aMemoryDist.accumulate(7);
  Glean.testOnlyIpc.aMemoryDist.accumulate(17);

  let data = Glean.testOnlyIpc.aMemoryDist.testGetValue();
  // `data.sum` is in bytes, but the metric is in KB.
  Assert.equal(24 * 1024, data.sum, "Sum's correct");
  for (let [bucket, count] of Object.entries(data.values)) {
    Assert.ok(
      count == 0 || (count == 1 && (bucket == 6888 || bucket == 17109)),
      `Only two buckets have a sample ${bucket} ${count}`
    );
  }

  data = Telemetry.getHistogramById("TELEMETRY_TEST_LINEAR").snapshot();
  Assert.equal(24, data.sum, "Histogram's in `memory_unit` units");
  Assert.equal(2, data.values["1"], "Both samples in a low bucket");
});

add_task(async function test_gifft_timing_dist() {
  let t1 = Glean.testOnlyIpc.aTimingDist.start();
  let t2 = Glean.testOnlyIpc.aTimingDist.start();

  await sleep(5);

  let t3 = Glean.testOnlyIpc.aTimingDist.start();
  Glean.testOnlyIpc.aTimingDist.cancel(t1);

  await sleep(5);

  Glean.testOnlyIpc.aTimingDist.stopAndAccumulate(t2); // 10ms
  Glean.testOnlyIpc.aTimingDist.stopAndAccumulate(t3); // 5ms

  let data = Glean.testOnlyIpc.aTimingDist.testGetValue();
  const NANOS_IN_MILLIS = 1e6;
  // bug 1701949 - Sleep gets close, but sometimes doesn't wait long enough.
  const EPSILON = 40000;

  // Variance in timing makes getting the sum impossible to know.
  Assert.greater(data.sum, 15 * NANOS_IN_MILLIS - EPSILON);

  // No guarantees from timers means no guarantees on buckets.
  // But we can guarantee it's only two samples.
  Assert.equal(
    2,
    Object.entries(data.values).reduce(
      (acc, [bucket, count]) => acc + count,
      0
    ),
    "Only two buckets with samples"
  );

  data = Telemetry.getHistogramById("TELEMETRY_TEST_EXPONENTIAL").snapshot();
  Assert.greaterOrEqual(data.sum, 15, "Histogram's in milliseconds");
  Assert.equal(
    2,
    Object.entries(data.values).reduce(
      (acc, [bucket, count]) => acc + count,
      0
    ),
    "Only two samples"
  );
});

add_task(function test_gifft_string_list_works() {
  const value = "a string!";
  const value2 = "another string!";
  const value3 = "yet another string.";

  // `set` doesn't work in the mirror, so use `add`
  Glean.testOnlyIpc.aStringList.add(value);
  Glean.testOnlyIpc.aStringList.add(value2);
  Glean.testOnlyIpc.aStringList.add(value3);

  let val = Glean.testOnlyIpc.aStringList.testGetValue();
  // Note: This is incredibly fragile and will break if we ever rearrange items
  // in the string list.
  Assert.deepEqual([value, value2, value3], val);

  val = keyedScalarValue("telemetry.test.keyed_boolean_kind");
  // This too may be fragile.
  Assert.deepEqual(
    {
      [value]: true,
      [value2]: true,
      [value3]: true,
    },
    val
  );
});

add_task(function test_gifft_events() {
  Telemetry.setEventRecordingEnabled("telemetry.test", true);

  Glean.testOnlyIpc.noExtraEvent.record();
  var events = Glean.testOnlyIpc.noExtraEvent.testGetValue();
  Assert.equal(1, events.length);
  Assert.equal("test_only.ipc", events[0].category);
  Assert.equal("no_extra_event", events[0].name);

  let extra = { extra1: "can set extras", extra2: "passing more data" };
  Glean.testOnlyIpc.anEvent.record(extra);
  events = Glean.testOnlyIpc.anEvent.testGetValue();
  Assert.equal(1, events.length);
  Assert.equal("test_only.ipc", events[0].category);
  Assert.equal("an_event", events[0].name);
  Assert.deepEqual(extra, events[0].extra);

  TelemetryTestUtils.assertEvents(
    [
      ["telemetry.test", "not_expired_optout", "object1", undefined, undefined],
      ["telemetry.test", "mirror_with_extra", "object1", null, extra],
    ],
    { category: "telemetry.test" }
  );
});

add_task(function test_gifft_uuid() {
  const kTestUuid = "decafdec-afde-cafd-ecaf-decafdecafde";
  Glean.testOnlyIpc.aUuid.set(kTestUuid);
  Assert.equal(kTestUuid, Glean.testOnlyIpc.aUuid.testGetValue());
  Assert.equal(kTestUuid, scalarValue("telemetry.test.string_kind"));
});

add_task(function test_gifft_labeled_counter() {
  Assert.equal(
    undefined,
    Glean.testOnlyIpc.aLabeledCounter.a_label.testGetValue(),
    "New labels with no values should return undefined"
  );
  Glean.testOnlyIpc.aLabeledCounter.a_label.add(1);
  Glean.testOnlyIpc.aLabeledCounter.another_label.add(2);
  Assert.equal(1, Glean.testOnlyIpc.aLabeledCounter.a_label.testGetValue());
  Assert.equal(
    2,
    Glean.testOnlyIpc.aLabeledCounter.another_label.testGetValue()
  );
  // What about invalid/__other__?
  Assert.equal(
    undefined,
    Glean.testOnlyIpc.aLabeledCounter.__other__.testGetValue()
  );
  Glean.testOnlyIpc.aLabeledCounter.InvalidLabel.add(3);
  Assert.equal(3, Glean.testOnlyIpc.aLabeledCounter.__other__.testGetValue());

  let value = keyedScalarValue("telemetry.test.keyed_unsigned_int");
  Assert.deepEqual(
    {
      a_label: 1,
      another_label: 2,
      InvalidLabel: 3,
    },
    value
  );
});

add_task(async function test_gifft_timespan() {
  // We start, briefly sleep and then stop.
  // That guarantees some time to measure.
  Glean.testOnly.mirrorTime.start();
  await sleep(10);
  Glean.testOnly.mirrorTime.stop();

  const NANOS_IN_MILLIS = 1e6;
  // bug 1701949 - Sleep gets close, but sometimes doesn't wait long enough.
  const EPSILON = 40000;
  Assert.greater(
    Glean.testOnly.mirrorTime.testGetValue(),
    10 * NANOS_IN_MILLIS - EPSILON
  );
  // Mirrored to milliseconds.
  Assert.greaterOrEqual(scalarValue("telemetry.test.mirror_for_timespan"), 10);
});

add_task(async function test_gifft_timespan_raw() {
  Glean.testOnly.mirrorTimeNanos.setRaw(15 /*ns*/);

  Assert.equal(15, Glean.testOnly.mirrorTimeNanos.testGetValue());
  // setRaw, unlike start/stop, mirrors the raw value directly.
  Assert.equal(scalarValue("telemetry.test.mirror_for_timespan_nanos"), 15);
});

add_task(async function test_gifft_labeled_boolean() {
  Assert.equal(
    undefined,
    Glean.testOnly.mirrorsForLabeledBools.a_label.testGetValue(),
    "New labels with no values should return undefined"
  );
  Glean.testOnly.mirrorsForLabeledBools.a_label.set(true);
  Glean.testOnly.mirrorsForLabeledBools.another_label.set(false);
  Assert.equal(
    true,
    Glean.testOnly.mirrorsForLabeledBools.a_label.testGetValue()
  );
  Assert.equal(
    false,
    Glean.testOnly.mirrorsForLabeledBools.another_label.testGetValue()
  );
  // What about invalid/__other__?
  Assert.equal(
    undefined,
    Glean.testOnly.mirrorsForLabeledBools.__other__.testGetValue()
  );
  Glean.testOnly.mirrorsForLabeledBools.InvalidLabel.set(true);
  Assert.equal(
    true,
    Glean.testOnly.mirrorsForLabeledBools.__other__.testGetValue()
  );

  // In Telemetry there is no invalid label
  let value = keyedScalarValue("telemetry.test.mirror_for_labeled_bool");
  Assert.deepEqual(
    {
      a_label: true,
      another_label: false,
      InvalidLabel: true,
    },
    value
  );
});
