/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
const { setTimeout } = ChromeUtils.importESModule(
  "resource://gre/modules/Timer.sys.mjs"
);

function sleep(ms) {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  return new Promise(resolve => setTimeout(resolve, ms));
}

add_setup(
  /* on Android FOG is set up through head.js */
  { skip_if: () => !runningInParent || AppConstants.platform == "android" },
  function test_setup() {
    // Give FOG a temp profile to init within.
    do_get_profile();

    // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
    Services.fog.initializeFOG();
  }
);

const BAD_CODE_COUNT = 42;
const CHEESY_STRING = "a very cheesy string!";
const CHEESIER_STRING = "a much cheesier string!";
const EVENT_EXTRA = { extra1: "so very extra" };
const MEMORIES = [13, 31];
const MEMORY_BUCKETS = ["13509772", "32131834"]; // buckets are strings : |
const COUNTERS_NEAR_THE_SINK = 3;
const COUNTERS_WITH_JUNK_ON_THEM = 5;
const INVALID_COUNTERS = 7;

add_task({ skip_if: () => runningInParent }, async function run_child_stuff() {
  Glean.testOnly.badCode.add(BAD_CODE_COUNT);
  Glean.testOnly.cheesyStringList.add(CHEESY_STRING);
  Glean.testOnly.cheesyStringList.add(CHEESIER_STRING);

  Glean.testOnlyIpc.noExtraEvent.record();
  Glean.testOnlyIpc.anEvent.record(EVENT_EXTRA);

  for (let memory of MEMORIES) {
    Glean.testOnly.doYouRemember.accumulate(memory);
  }

  let t1 = Glean.testOnly.whatTimeIsIt.start();
  let t2 = Glean.testOnly.whatTimeIsIt.start();

  await sleep(5);

  let t3 = Glean.testOnly.whatTimeIsIt.start();
  Glean.testOnly.whatTimeIsIt.cancel(t1);

  await sleep(5);

  Glean.testOnly.whatTimeIsIt.stopAndAccumulate(t2); // 10ms
  Glean.testOnly.whatTimeIsIt.stopAndAccumulate(t3); // 5ms

  Glean.testOnlyIpc.aCustomDist.accumulateSamples([3, 4]);

  Glean.testOnly.mabelsKitchenCounters.near_the_sink.add(
    COUNTERS_NEAR_THE_SINK
  );
  Glean.testOnly.mabelsKitchenCounters.with_junk_on_them.add(
    COUNTERS_WITH_JUNK_ON_THEM
  );

  Glean.testOnly.mabelsBathroomCounters["1".repeat(72)].add(INVALID_COUNTERS);

  Glean.testOnlyIpc.irate.addToNumerator(44);
  Glean.testOnlyIpc.irate.addToDenominator(14);
});

add_task(
  { skip_if: () => !runningInParent },
  async function test_child_metrics() {
    await run_test_in_child("test_GleanIPC.js");
    await Services.fog.testFlushAllChildren();

    Assert.equal(Glean.testOnly.badCode.testGetValue(), BAD_CODE_COUNT);

    // Note: this will break if string list ever rearranges its items.
    const cheesyStrings = Glean.testOnly.cheesyStringList.testGetValue();
    Assert.deepEqual(cheesyStrings, [CHEESY_STRING, CHEESIER_STRING]);

    const data = Glean.testOnly.doYouRemember.testGetValue();
    Assert.equal(MEMORIES.reduce((a, b) => a + b, 0) * 1024 * 1024, data.sum);
    for (let [bucket, count] of Object.entries(data.values)) {
      // We could assert instead, but let's skip to save the logspam.
      if (count == 0) {
        continue;
      }
      Assert.ok(count == 1 && MEMORY_BUCKETS.includes(bucket));
    }

    const customData = Glean.testOnlyIpc.aCustomDist.testGetValue("store1");
    Assert.equal(3 + 4, customData.sum, "Sum's correct");
    for (let [bucket, count] of Object.entries(customData.values)) {
      Assert.ok(
        count == 0 || (count == 2 && bucket == 1), // both values in the low bucket
        `Only two buckets have a sample ${bucket} ${count}`
      );
    }

    var events = Glean.testOnlyIpc.noExtraEvent.testGetValue();
    Assert.equal(1, events.length);
    Assert.equal("test_only.ipc", events[0].category);
    Assert.equal("no_extra_event", events[0].name);

    events = Glean.testOnlyIpc.anEvent.testGetValue();
    Assert.equal(1, events.length);
    Assert.equal("test_only.ipc", events[0].category);
    Assert.equal("an_event", events[0].name);
    Assert.deepEqual(EVENT_EXTRA, events[0].extra);

    const NANOS_IN_MILLIS = 1e6;
    const EPSILON = 40000; // bug 1701949
    const times = Glean.testOnly.whatTimeIsIt.testGetValue();
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

    const mabelsCounters = Glean.testOnly.mabelsKitchenCounters;
    Assert.equal(
      mabelsCounters.near_the_sink.testGetValue(),
      COUNTERS_NEAR_THE_SINK
    );
    Assert.equal(
      mabelsCounters.with_junk_on_them.testGetValue(),
      COUNTERS_WITH_JUNK_ON_THEM
    );

    Assert.throws(
      () => Glean.testOnly.mabelsBathroomCounters.__other__.testGetValue(),
      /DataError/,
      "Invalid labels record errors, which throw"
    );

    Assert.deepEqual(
      { numerator: 44, denominator: 14 },
      Glean.testOnlyIpc.irate.testGetValue()
    );
  }
);
