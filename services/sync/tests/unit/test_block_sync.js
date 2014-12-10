/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
Cu.import("resource://services-sync/main.js");
Cu.import("resource://services-sync/util.js");

// Simple test for block/unblock.
add_task(function *() {
  Assert.ok(!Weave.Service.scheduler.isBlocked, "sync is not blocked.")
  Assert.ok(!Svc.Prefs.has("scheduler.blocked-until"), "have no blocked pref");
  Weave.Service.scheduler.blockSync();

  Assert.ok(Weave.Service.scheduler.isBlocked, "sync is blocked.")
  Assert.ok(Svc.Prefs.has("scheduler.blocked-until"), "have the blocked pref");

  Weave.Service.scheduler.unblockSync();
  Assert.ok(!Weave.Service.scheduler.isBlocked, "sync is not blocked.")
  Assert.ok(!Svc.Prefs.has("scheduler.blocked-until"), "have no blocked pref");

  // now check the "until" functionality.
  let until = Date.now() + 1000;
  Weave.Service.scheduler.blockSync(until);
  Assert.ok(Weave.Service.scheduler.isBlocked, "sync is blocked.")
  Assert.ok(Svc.Prefs.has("scheduler.blocked-until"), "have the blocked pref");

  // wait for 'until' to pass.
  yield new Promise((resolve, reject) => {
    CommonUtils.namedTimer(resolve, 1000, {}, "timer");
  });

  // should have automagically unblocked and removed the pref.
  Assert.ok(!Weave.Service.scheduler.isBlocked, "sync is not blocked.")
  Assert.ok(!Svc.Prefs.has("scheduler.blocked-until"), "have no blocked pref");
});

function run_test() {
  run_next_test();
}
