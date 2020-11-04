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
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

function sleep(ms) {
  /* eslint-disable mozilla/no-arbitrary-setTimeout */
  return new Promise(resolve => setTimeout(resolve, ms));
}

add_task(function test_setup() {
  // FOG needs a profile directory to put its data in.
  do_get_profile();

  // We need to initialize it once, otherwise operations will be stuck in the pre-init queue.
  let FOG = Cc["@mozilla.org/toolkit/glean;1"].createInstance(Ci.nsIFOG);
  FOG.initializeFOG();
});

add_task(function test_fog_counter_works() {
  Glean.test_only.bad_code.add(31);
  Assert.ok(Glean.test_only.bad_code.testHasValue("test-ping"));
  Assert.equal(31, Glean.test_only.bad_code.testGetValue("test-ping"));
});

add_task(async function test_fog_timespan_works() {
  // We start, briefly sleep and then stop.
  // That guarantees some time to measure.
  Glean.test_only.can_we_time_it.start();
  await sleep(10);
  Glean.test_only.can_we_time_it.stop();

  Assert.ok(Glean.test_only.can_we_time_it.testHasValue("test-ping"));
  Assert.ok(Glean.test_only.can_we_time_it.testGetValue("test-ping") > 0);
});

add_task(async function test_fog_uuid_works() {
  const kTestUuid = "decafdec-afde-cafd-ecaf-decafdecafde";
  Glean.test_only.what_id_it.set(kTestUuid);
  Assert.ok(Glean.test_only.what_id_it.testHasValue("test-ping"));
  Assert.equal(kTestUuid, Glean.test_only.what_id_it.testGetValue("test-ping"));

  Glean.test_only.what_id_it.generateAndSet();
  // Since we generate v4 UUIDs, and the first character of the third group
  // isn't 4, this won't ever collide with kTestUuid.
  Assert.notEqual(
    kTestUuid,
    Glean.test_only.what_id_it.testGetValue("test-ping")
  );
});
