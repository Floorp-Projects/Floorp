/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

function sleep(ms) {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  return new Promise(resolve => setTimeout(resolve, ms));
}

add_setup(
  /* on Android FOG is set up through head.js */
  { skip_if: () => AppConstants.platform == "android" },
  function test_setup() {
    // FOG needs a profile directory to put its data in.
    do_get_profile();

    // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
    Services.fog.initializeFOG();
  }
);

add_task(function test_fog_counter_works() {
  Glean.testOnly.badCode.add(31);
  Assert.equal(31, Glean.testOnly.badCode.testGetValue("test-ping"));
});

add_task(async function test_fog_string_works() {
  const value = "a cheesy string!";
  Glean.testOnly.cheesyString.set(value);

  Assert.equal(value, Glean.testOnly.cheesyString.testGetValue("test-ping"));
});

add_task(async function test_fog_string_list_works() {
  const value = "a cheesy string!";
  const value2 = "a cheesier string!";
  const value3 = "the cheeziest of strings.";

  const cheeseList = [value, value2];
  Glean.testOnly.cheesyStringList.set(cheeseList);

  let val = Glean.testOnly.cheesyStringList.testGetValue();
  // Note: This is incredibly fragile and will break if we ever rearrange items
  // in the string list.
  Assert.deepEqual(cheeseList, val);

  Glean.testOnly.cheesyStringList.add(value3);
  Assert.ok(Glean.testOnly.cheesyStringList.testGetValue().includes(value3));
});

add_task(async function test_fog_timespan_works() {
  Glean.testOnly.canWeTimeIt.start();
  Glean.testOnly.canWeTimeIt.cancel();
  Assert.equal(undefined, Glean.testOnly.canWeTimeIt.testGetValue());

  // We start, briefly sleep and then stop.
  // That guarantees some time to measure.
  Glean.testOnly.canWeTimeIt.start();
  await sleep(10);
  Glean.testOnly.canWeTimeIt.stop();

  Assert.ok(Glean.testOnly.canWeTimeIt.testGetValue("test-ping") > 0);
});

add_task(async function test_fog_uuid_works() {
  const kTestUuid = "decafdec-afde-cafd-ecaf-decafdecafde";
  Glean.testOnly.whatIdIt.set(kTestUuid);
  Assert.equal(kTestUuid, Glean.testOnly.whatIdIt.testGetValue("test-ping"));

  Glean.testOnly.whatIdIt.generateAndSet();
  // Since we generate v4 UUIDs, and the first character of the third group
  // isn't 4, this won't ever collide with kTestUuid.
  Assert.notEqual(kTestUuid, Glean.testOnly.whatIdIt.testGetValue("test-ping"));
});

add_task(function test_fog_datetime_works() {
  const value = new Date("2020-06-11T12:00:00");

  Glean.testOnly.whatADate.set(value.getTime() * 1000);

  const received = Glean.testOnly.whatADate.testGetValue("test-ping");
  Assert.equal(received.getTime(), value.getTime());
});

add_task(function test_fog_boolean_works() {
  Glean.testOnly.canWeFlagIt.set(false);
  Assert.equal(false, Glean.testOnly.canWeFlagIt.testGetValue("test-ping"));
  // While you're here, might as well test that the ping name's optional.
  Assert.equal(false, Glean.testOnly.canWeFlagIt.testGetValue());
});

add_task(async function test_fog_event_works() {
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

  let extra2 = {
    extra1: "can set extras",
    extra2: 37,
    extra3_longer_name: false,
  };
  Glean.testOnlyIpc.eventWithExtra.record(extra2);
  events = Glean.testOnlyIpc.eventWithExtra.testGetValue();
  Assert.equal(1, events.length);
  Assert.equal("test_only.ipc", events[0].category);
  Assert.equal("event_with_extra", events[0].name);
  let expectedExtra = {
    extra1: "can set extras",
    extra2: "37",
    extra3_longer_name: "false",
  };
  Assert.deepEqual(expectedExtra, events[0].extra);

  // Quantities need to be non-negative.
  // This does not record a Glean error.
  let extra4 = {
    extra2: -1,
  };
  Glean.testOnlyIpc.eventWithExtra.record(extra4);
  events = Glean.testOnlyIpc.eventWithExtra.testGetValue();
  // Unchanged number of events
  Assert.equal(1, events.length, "Recorded one event too many.");

  // Invalid extra keys don't crash, the event is not recorded,
  // but an error is recorded.
  let extra3 = {
    extra1_nonexistent_extra: "this does not crash",
  };
  Glean.testOnlyIpc.eventWithExtra.record(extra3);
  Assert.throws(
    () => Glean.testOnlyIpc.eventWithExtra.testGetValue(),
    /NS_ERROR_LOSS_OF_SIGNIFICANT_DATA/,
    "Should throw because of a recording error."
  );
});

add_task(async function test_fog_memory_distribution_works() {
  Glean.testOnly.doYouRemember.accumulate(7);
  Glean.testOnly.doYouRemember.accumulate(17);

  let data = Glean.testOnly.doYouRemember.testGetValue("test-ping");
  // `data.sum` is in bytes, but the metric is in MB.
  Assert.equal(24 * 1024 * 1024, data.sum, "Sum's correct");
  for (let [bucket, count] of Object.entries(data.values)) {
    Assert.ok(
      count == 0 || (count == 1 && (bucket == 17520006 || bucket == 7053950)),
      "Only two buckets have a sample"
    );
  }
});

add_task(async function test_fog_custom_distribution_works() {
  Glean.testOnlyIpc.aCustomDist.accumulateSamples([7, 268435458]);

  let data = Glean.testOnlyIpc.aCustomDist.testGetValue("store1");
  Assert.equal(7 + 268435458, data.sum, "Sum's correct");
  for (let [bucket, count] of Object.entries(data.values)) {
    Assert.ok(
      count == 0 || (count == 1 && (bucket == 1 || bucket == 268435456)),
      `Only two buckets have a sample ${bucket} ${count}`
    );
  }

  // Negative values will not be recorded, instead an error is recorded.
  Glean.testOnlyIpc.aCustomDist.accumulateSamples([-7]);
  Assert.throws(
    () => Glean.testOnlyIpc.aCustomDist.testGetValue(),
    /NS_ERROR_LOSS_OF_SIGNIFICANT_DATA/
  );
});

add_task(
  /* TODO(bug 1737520): Enable custom ping support on Android */
  { skip_if: () => AppConstants.platform == "android" },
  function test_fog_custom_pings() {
    Assert.ok("onePingOnly" in GleanPings);
    let submitted = false;
    Glean.testOnly.onePingOneBool.set(false);
    GleanPings.onePingOnly.testBeforeNextSubmit(reason => {
      submitted = true;
      Assert.equal(false, Glean.testOnly.onePingOneBool.testGetValue());
    });
    GleanPings.onePingOnly.submit();
    Assert.ok(submitted, "Ping was submitted, callback was called.");
  }
);

add_task(async function test_fog_timing_distribution_works() {
  let t1 = Glean.testOnly.whatTimeIsIt.start();
  let t2 = Glean.testOnly.whatTimeIsIt.start();

  await sleep(5);

  let t3 = Glean.testOnly.whatTimeIsIt.start();
  Glean.testOnly.whatTimeIsIt.cancel(t1);

  await sleep(5);

  Glean.testOnly.whatTimeIsIt.stopAndAccumulate(t2); // 10ms
  Glean.testOnly.whatTimeIsIt.stopAndAccumulate(t3); // 5ms

  let data = Glean.testOnly.whatTimeIsIt.testGetValue();
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
});

add_task(async function test_fog_labeled_boolean_works() {
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsLikeBalloons.at_parties.testGetValue(),
    "New labels with no values should return undefined"
  );
  Glean.testOnly.mabelsLikeBalloons.at_parties.set(true);
  Glean.testOnly.mabelsLikeBalloons.at_funerals.set(false);
  Assert.equal(
    true,
    Glean.testOnly.mabelsLikeBalloons.at_parties.testGetValue()
  );
  Assert.equal(
    false,
    Glean.testOnly.mabelsLikeBalloons.at_funerals.testGetValue()
  );
  // What about invalid/__other__?
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsLikeBalloons.__other__.testGetValue()
  );
  Glean.testOnly.mabelsLikeBalloons.InvalidLabel.set(true);
  Assert.equal(
    true,
    Glean.testOnly.mabelsLikeBalloons.__other__.testGetValue()
  );
  // TODO: Test that we have the right number and type of errors (bug 1683171)
});

add_task(async function test_fog_labeled_counter_works() {
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsKitchenCounters.near_the_sink.testGetValue(),
    "New labels with no values should return undefined"
  );
  Glean.testOnly.mabelsKitchenCounters.near_the_sink.add(1);
  Glean.testOnly.mabelsKitchenCounters.with_junk_on_them.add(2);
  Assert.equal(
    1,
    Glean.testOnly.mabelsKitchenCounters.near_the_sink.testGetValue()
  );
  Assert.equal(
    2,
    Glean.testOnly.mabelsKitchenCounters.with_junk_on_them.testGetValue()
  );
  // What about invalid/__other__?
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsKitchenCounters.__other__.testGetValue()
  );
  Glean.testOnly.mabelsKitchenCounters.InvalidLabel.add(1);
  Assert.throws(
    () => Glean.testOnly.mabelsKitchenCounters.__other__.testGetValue(),
    /NS_ERROR_LOSS_OF_SIGNIFICANT_DATA/,
    "Should throw because of a recording error."
  );
});

add_task(async function test_fog_labeled_string_works() {
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsBalloonStrings.colour_of_99.testGetValue(),
    "New labels with no values should return undefined"
  );
  Glean.testOnly.mabelsBalloonStrings.colour_of_99.set("crimson");
  Glean.testOnly.mabelsBalloonStrings.string_lengths.set("various");
  Assert.equal(
    "crimson",
    Glean.testOnly.mabelsBalloonStrings.colour_of_99.testGetValue()
  );
  Assert.equal(
    "various",
    Glean.testOnly.mabelsBalloonStrings.string_lengths.testGetValue()
  );
  // What about invalid/__other__?
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsBalloonStrings.__other__.testGetValue()
  );
  Glean.testOnly.mabelsBalloonStrings.InvalidLabel.set("valid");
  Assert.throws(
    () => Glean.testOnly.mabelsBalloonStrings.__other__.testGetValue(),
    /NS_ERROR_LOSS_OF_SIGNIFICANT_DATA/
  );
});

add_task(function test_fog_quantity_works() {
  Glean.testOnly.meaningOfLife.set(42);
  Assert.equal(42, Glean.testOnly.meaningOfLife.testGetValue());
});

add_task(function test_fog_rate_works() {
  // 1) Standard rate with internal denominator
  Glean.testOnlyIpc.irate.addToNumerator(22);
  Glean.testOnlyIpc.irate.addToDenominator(7);
  Assert.deepEqual(
    { numerator: 22, denominator: 7 },
    Glean.testOnlyIpc.irate.testGetValue()
  );

  // 2) Rate with external denominator
  Glean.testOnlyIpc.anExternalDenominator.add(11);
  Glean.testOnlyIpc.rateWithExternalDenominator.addToNumerator(121);
  Assert.equal(11, Glean.testOnlyIpc.anExternalDenominator.testGetValue());
  Assert.deepEqual(
    { numerator: 121, denominator: 11 },
    Glean.testOnlyIpc.rateWithExternalDenominator.testGetValue()
  );
});

add_task(async function test_fog_url_works() {
  const value = "https://www.example.com/fog";
  Glean.testOnlyIpc.aUrl.set(value);

  Assert.equal(value, Glean.testOnlyIpc.aUrl.testGetValue("store1"));
});
