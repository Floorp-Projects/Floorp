/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://testing-common/AppData.jsm", this);

function run_test() {
  run_next_test();
}

add_task(function* test_setup() {
  do_get_profile();
  yield makeFakeAppDir();
});

add_task(function* test_main_process_crash() {
  let cm = Services.crashmanager;
  Assert.ok(cm, "CrashManager available.");

  let basename;
  let deferred = Promise.defer();
  do_crash("crashType = CrashTestUtils.CRASH_RUNTIMEABORT;",
    (minidump, extra) => {
      basename = minidump.leafName;
      cm._eventsDirs = [getEventDir()];
      cm.aggregateEventsFiles().then(deferred.resolve, deferred.reject);
    },
    true);

  let count = yield deferred.promise;
  Assert.equal(count, 1, "A single crash event file was seen.");
  let crashes = yield cm.getCrashes();
  Assert.equal(crashes.length, 1);
  let crash = crashes[0];
  Assert.ok(crash.isOfType(cm.PROCESS_TYPE_MAIN, cm.CRASH_TYPE_CRASH));
  Assert.equal(crash.id + ".dmp", basename, "ID recorded properly");
});
