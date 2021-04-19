/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* FIXME: Remove these global markers.
 * FOG doesn't follow the stricter naming patterns as expected by tool configuration yet.
 * See https://searchfox.org/mozilla-central/source/.eslintrc.js#24
 * Reorganizing the directory structure will take this into account.
 */
/* global add_task, Assert, do_get_profile */
"use strict";

Cu.importGlobalProperties(["Glean", "GleanPings"]);
const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);
const { ObjectUtils } = ChromeUtils.import(
  "resource://gre/modules/ObjectUtils.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

/**
 * Mock the SysInfo object used to read System data in Gecko.
 */
var SysInfo = {
  overrides: {},

  /**
   * Checks if overrides are present and return them.
   *
   * @returns the overridden value or undefined if not present.
   */
  _getOverridden(name) {
    if (name in this.overrides) {
      return this.overrides[name];
    }

    return undefined;
  },

  // To support nsIPropertyBag.
  getProperty(name) {
    let override = this._getOverridden(name);
    return override !== undefined
      ? override
      : this._genuine.QueryInterface(Ci.nsIPropertyBag).getProperty(name);
  },

  // To support nsIPropertyBag2.
  get(name) {
    let override = this._getOverridden(name);
    return override !== undefined
      ? override
      : this._genuine.QueryInterface(Ci.nsIPropertyBag2).get(name);
  },

  // To support nsIPropertyBag2.
  getPropertyAsACString(name) {
    return this.get(name);
  },

  QueryInterface: ChromeUtils.generateQI(["nsIPropertyBag2", "nsISystemInfo"]),
};

function sleep(ms) {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  return new Promise(resolve => setTimeout(resolve, ms));
}

add_task(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // Mock SysInfo.
  SysInfo.overrides = {
    version: "1.2.3",
    arch: "x64",
  };
  MockRegistrar.register("@mozilla.org/system-info;1", SysInfo);

  // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
  let FOG = Cc["@mozilla.org/toolkit/glean;1"].createInstance(Ci.nsIFOG);
  FOG.initializeFOG();
});

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

// Enable test after bug 1677448 is fixed.
add_task({ skip_if: () => true }, function test_fog_datetime_works() {
  const value = new Date("2020-06-11T12:00:00");

  Glean.testOnly.whatADate.set(value.getTime() * 1000);

  const received = Glean.testOnly.whatADate.testGetValue("test-ping");
  Assert.ok(received.startsWith("2020-06-11T12:00:00"));
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

add_task(function test_fog_custom_pings() {
  Assert.ok("onePingOnly" in GleanPings);
  let submitted = false;
  Glean.testOnly.onePingOneBool.set(false);
  GleanPings.onePingOnly.testBeforeNextSubmit(reason => {
    submitted = true;
    Assert.equal(false, Glean.testOnly.onePingOneBool.testGetValue());
  });
  GleanPings.onePingOnly.submit();
  Assert.ok(submitted, "Ping was submitted, callback was called.");
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
  Assert.equal(
    1,
    Glean.testOnly.mabelsKitchenCounters.__other__.testGetValue()
  );
  // TODO: Test that we have the right number and type of errors (bug 1683171)
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
  Assert.equal(
    "valid",
    Glean.testOnly.mabelsBalloonStrings.__other__.testGetValue()
  );
  // TODO: Test that we have the right number and type of errors (bug 1683171)
});
