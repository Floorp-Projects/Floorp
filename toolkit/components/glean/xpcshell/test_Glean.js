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
    Glean.fog_validation.os_version.testGetValue("fog-validation")
  );
});

add_task(function test_fog_counter_works() {
  Glean.test_only.bad_code.add(31);
  Assert.equal(31, Glean.test_only.bad_code.testGetValue("test-ping"));
});

add_task(async function test_fog_string_works() {
  const value = "a cheesy string!";
  Glean.test_only.cheesy_string.set(value);

  Assert.equal(value, Glean.test_only.cheesy_string.testGetValue("test-ping"));
});

add_task(async function test_fog_timespan_works() {
  // We start, briefly sleep and then stop.
  // That guarantees some time to measure.
  Glean.test_only.can_we_time_it.start();
  await sleep(10);
  Glean.test_only.can_we_time_it.stop();

  Assert.ok(Glean.test_only.can_we_time_it.testGetValue("test-ping") > 0);
});

add_task(async function test_fog_uuid_works() {
  const kTestUuid = "decafdec-afde-cafd-ecaf-decafdecafde";
  Glean.test_only.what_id_it.set(kTestUuid);
  Assert.equal(kTestUuid, Glean.test_only.what_id_it.testGetValue("test-ping"));

  Glean.test_only.what_id_it.generateAndSet();
  // Since we generate v4 UUIDs, and the first character of the third group
  // isn't 4, this won't ever collide with kTestUuid.
  Assert.notEqual(
    kTestUuid,
    Glean.test_only.what_id_it.testGetValue("test-ping")
  );
});

// Enable test after bug 1677448 is fixed.
add_task({ skip_if: () => true }, function test_fog_datetime_works() {
  const value = new Date("2020-06-11T12:00:00");

  Glean.test_only.what_a_date.set(value.getTime() * 1000);

  const received = Glean.test_only.what_a_date.testGetValue("test-ping");
  Assert.ok(received.startsWith("2020-06-11T12:00:00"));
});

add_task(function test_fog_boolean_works() {
  Glean.test_only.can_we_flag_it.set(false);
  Assert.equal(false, Glean.test_only.can_we_flag_it.testGetValue("test-ping"));
});

add_task(async function test_fog_event_works() {
  Glean.test_only_ipc.no_extra_event.record();
  // FIXME(bug 1678567): Check that the value was recorded when we can.
  // Assert.ok(Glean.test_only_ipc.no_extra_event.testGetValue("store1"));

  let extra = { extra1: "can set extras", extra2: "passing more data" };
  Glean.test_only_ipc.an_event.record(extra);
  // FIXME(bug 1678567): Check that the value was recorded when we can.
  // Assert.ok(Glean.test_only_ipc.an_event.testGetValue("store1"));
});
