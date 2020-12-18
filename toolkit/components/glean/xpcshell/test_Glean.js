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
    arc: "x64",
  };
  MockRegistrar.register("@mozilla.org/system-info;1", SysInfo);

  // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
  let FOG = Cc["@mozilla.org/toolkit/glean;1"].createInstance(Ci.nsIFOG);
  FOG.initializeFOG();
});

add_task(function test_osversion_is_set() {
  Assert.equal(
    "1.2.3",
    Glean.fogValidation.osVersion.testGetValue("fog-validation")
  );
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

add_task(async function test_fog_timespan_works() {
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
  // Don't bother sending it, we'll test that in the integration suite.
  // See also bug 1681742.
});
