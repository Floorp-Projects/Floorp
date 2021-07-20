/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* FIXME: Remove these global markers.
 * FOG doesn't follow the stricter naming patterns as expected by tool configuration yet.
 * See https://searchfox.org/mozilla-central/source/.eslintrc.js#24
 * Reorganizing the directory structure will take this into account.
 */
/* global add_task, Assert */
"use strict";

Cu.importGlobalProperties(["Glean", "GleanPings"]);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

function sleep(ms) {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  return new Promise(resolve => setTimeout(resolve, ms));
}

// No call to `FOG.initializeFOG()`.
// Glean is not initialized on Android and we need to ensure
// both the regular API and the test APIs work regardless.

add_task(function test_fog_counter_works() {
  Glean.testOnly.badCode.add(31);
  Assert.equal(undefined, Glean.testOnly.badCode.testGetValue("test-ping"));
});

add_task(async function test_fog_string_works() {
  const value = "a cheesy string!";
  Glean.testOnly.cheesyString.set(value);

  Assert.equal(
    undefined,
    Glean.testOnly.cheesyString.testGetValue("test-ping")
  );
});

add_task(async function test_fog_string_list_works() {
  const value = "a cheesy string!";
  const value2 = "a cheesier string!";
  const value3 = "the cheeziest of strings.";

  const cheeseList = [value, value2];
  Glean.testOnly.cheesyStringList.set(cheeseList);

  let val = Glean.testOnly.cheesyStringList.testGetValue();
  Assert.equal(undefined, val);

  Glean.testOnly.cheesyStringList.add(value3);
  Assert.equal(undefined, Glean.testOnly.cheesyStringList.testGetValue());
});

add_task(async function test_fog_timespan_works() {
  // We start, briefly sleep and then stop.
  // That guarantees some time to measure.
  Glean.testOnly.canWeTimeIt.start();
  await sleep(10);
  Glean.testOnly.canWeTimeIt.stop();

  Assert.equal(undefined, Glean.testOnly.canWeTimeIt.testGetValue("test-ping"));
});

add_task(async function test_fog_uuid_works() {
  const kTestUuid = "decafdec-afde-cafd-ecaf-decafdecafde";
  Glean.testOnly.whatIdIt.set(kTestUuid);
  Assert.equal(undefined, Glean.testOnly.whatIdIt.testGetValue("test-ping"));

  Glean.testOnly.whatIdIt.generateAndSet();
  // Since we generate v4 UUIDs, and the first character of the third group
  // isn't 4, this won't ever collide with kTestUuid.
  Assert.equal(undefined, Glean.testOnly.whatIdIt.testGetValue("test-ping"));
});

// Enable test after bug 1677448 is fixed.
add_task({ skip_if: () => true }, function test_fog_datetime_works() {
  const value = new Date("2020-06-11T12:00:00");

  Glean.testOnly.whatADate.set(value.getTime() * 1000);

  const received = Glean.testOnly.whatADate.testGetValue("test-ping");
  Assert.equal(undefined, received);
});

add_task(function test_fog_boolean_works() {
  Glean.testOnly.canWeFlagIt.set(false);
  Assert.equal(undefined, Glean.testOnly.canWeFlagIt.testGetValue("test-ping"));
  // While you're here, might as well test that the ping name's optional.
  Assert.equal(undefined, Glean.testOnly.canWeFlagIt.testGetValue());
});

add_task(async function test_fog_event_works() {
  Glean.testOnlyIpc.noExtraEvent.record();
  // FIXME(bug 1678567): Check that the value was recorded when we can.
  // Assert.ok(Glean.testOnlyIpc.noExtraEvent.testGetValue("store1"));

  let extra = { extra1: "can set extras", extra2: "passing more data" };
  Glean.testOnlyIpc.anEvent.record(extra);
  // FIXME(bug 1678567): Check that the value was recorded when we can.
  // Assert.ok(Glean.testOnlyIpc.anEvent.testGetValue("store1"));
});

add_task(async function test_fog_memory_distribution_works() {
  Glean.testOnly.doYouRemember.accumulate(7);
  Glean.testOnly.doYouRemember.accumulate(17);

  let data = Glean.testOnly.doYouRemember.testGetValue("test-ping");
  Assert.equal(undefined, data);
});

add_task(function test_fog_custom_pings() {
  Assert.ok("onePingOnly" in GleanPings);
  // Don't bother sending it, we'll test that in the integration suite.
  // See also bug 1681742.
});

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

  Assert.equal(undefined, data);
});

add_task(async function test_fog_labeled_boolean_works() {
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsLikeBalloons.at_parties.testGetValue()
  );
  Glean.testOnly.mabelsLikeBalloons.at_parties.set(true);
  Glean.testOnly.mabelsLikeBalloons.at_funerals.set(false);
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsLikeBalloons.at_parties.testGetValue()
  );
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsLikeBalloons.at_funerals.testGetValue()
  );
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsLikeBalloons.__other__.testGetValue()
  );
  Glean.testOnly.mabelsLikeBalloons.InvalidLabel.set(true);
  Assert.equal(
    undefined,
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
    undefined,
    Glean.testOnly.mabelsKitchenCounters.near_the_sink.testGetValue()
  );
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsKitchenCounters.with_junk_on_them.testGetValue()
  );
  // What about invalid/__other__?
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsKitchenCounters.__other__.testGetValue()
  );
  Glean.testOnly.mabelsKitchenCounters.InvalidLabel.add(1);
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsKitchenCounters.__other__.testGetValue()
  );
  // TODO: Test that we have the right number and type of errors (bug 1683171)
});

add_task(async function test_fog_labeled_string_works() {
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsBalloonStrings.colour_of_99.testGetValue()
  );
  Glean.testOnly.mabelsBalloonStrings.colour_of_99.set("crimson");
  Glean.testOnly.mabelsBalloonStrings.string_lengths.set("various");
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsBalloonStrings.colour_of_99.testGetValue()
  );
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsBalloonStrings.string_lengths.testGetValue()
  );
  // What about invalid/__other__?
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsBalloonStrings.__other__.testGetValue()
  );
  Glean.testOnly.mabelsBalloonStrings.InvalidLabel.set("valid");
  Assert.equal(
    undefined,
    Glean.testOnly.mabelsBalloonStrings.__other__.testGetValue()
  );
  // TODO: Test that we have the right number and type of errors (bug 1683171)
});
