/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {classes: Cc, interfaces: Ci, utils: Cu} = Components;

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://testing-common/AppData.jsm", this);
let bsp = Cu.import("resource://gre/modules/CrashManager.jsm", this);

function run_test() {
  run_next_test();
}

add_task(function* test_instantiation() {
  Assert.ok(!bsp.gCrashManager, "CrashManager global instance not initially defined.");

  do_get_profile();
  yield makeFakeAppDir();

  // Fake profile creation.
  Cc["@mozilla.org/crashservice;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "profile-after-change", null);

  Assert.ok(bsp.gCrashManager, "Profile creation makes it available.");
  Assert.ok(Services.crashmanager, "CrashManager available via Services.");
  Assert.strictEqual(bsp.gCrashManager, Services.crashmanager,
                     "The objects are the same.");
});
