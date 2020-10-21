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
