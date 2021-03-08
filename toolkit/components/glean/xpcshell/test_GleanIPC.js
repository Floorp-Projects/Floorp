/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* FIXME: Remove these global markers.
 * FOG doesn't follow the stricter naming patterns as expected by tool configuration yet.
 * See https://searchfox.org/mozilla-central/source/.eslintrc.js#24
 * Reorganizing the directory structure will take this into account.
 */
/* global add_task, Assert, do_get_profile, run_test_in_child, runningInParent */
"use strict";

Cu.importGlobalProperties(["Glean", "GleanPings"]);
const { MockRegistrar } = ChromeUtils.import(
  "resource://testing-common/MockRegistrar.jsm"
);
const { ObjectUtils } = ChromeUtils.import(
  "resource://gre/modules/ObjectUtils.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
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

add_task({ skip_if: () => !runningInParent }, function test_setup() {
  // Give FOG a temp profile to init within.
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
  Glean.testOnlyIpc.anEvent.record();

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

  // FIXME(bug 1688281): Can't yet do IPC for labeled counters
  // Glean.testOnly.mabelsKitchenCounters.near_the_sink.add(
  //   COUNTERS_NEAR_THE_SINK
  // );
  // Glean.testOnly.mabelsKitchenCounters.with_junk_on_them.add(
  //   COUNTERS_WITH_JUNK_ON_THEM
  // );
  // Glean.testOnly.mabelsKitchenCounters.InvalidLabel.add(INVALID_COUNTERS);
});

add_task(
  { skip_if: () => !runningInParent },
  async function test_child_metrics() {
    await run_test_in_child("test_GleanIPC.js");
    let FOG = Cc["@mozilla.org/toolkit/glean;1"].createInstance(Ci.nsIFOG);
    await FOG.testFlushAllChildren();

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

    // FIXME(bug 1678567): Can't yet check Events using testGetValue.
    // FIXME(bug 1672198): Can't yet replay Events over IPC.
    // Assert.deepEqual(
    //   ["test.only.ipc", "no_extra_event"],
    //   Glean.testOnlyIpc.noExtraEvent.testGetValue().slice(1)
    // ); // etc.

    /* FIXME(bug 1695228): Timing Distributions cannot replay.
    const NANOS_IN_MILLIS = 1e6;
    const times = Glean.testOnly.whatTimeIsIt.testGetValue();
    Assert.greater(times.sum, 15 * NANOS_IN_MILLIS);
    // We can't guarantee any specific time values (thank you clocks),
    // but we can assert there are only two samples.
    Assert.equal(2, Object.entries(times.values).reduce((acc, [bucket, count]) => acc + count, 0));
    */
  }
);
