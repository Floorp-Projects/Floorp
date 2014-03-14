/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;


Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");
Cu.import("resource://testing-common/AppData.jsm");
Cu.import("resource://testing-common/services/healthreport/utils.jsm");
Cu.import("resource://testing-common/CrashManagerTest.jsm");


function run_test() {
  run_next_test();
}

add_task(function* init() {
  do_get_profile();
  yield makeFakeAppDir();
});

add_task(function test_constructor() {
  let provider = new CrashesProvider();
});

add_task(function* test_init() {
  let storage = yield Metrics.Storage("init");
  let provider = new CrashesProvider();
  yield provider.init(storage);
  yield provider.shutdown();

  yield storage.close();
});

add_task(function* test_collect() {
  let storage = yield Metrics.Storage("collect");
  let provider = new CrashesProvider();
  yield provider.init(storage);

  // Install custom manager so we don't interfere with other tests.
  let manager = yield getManager();
  provider._manager = manager;

  let day1 = new Date(2014, 0, 1, 0, 0, 0);
  let day2 = new Date(2014, 0, 3, 0, 0, 0);

  // FUTURE Bug 982836 CrashManager will grow public APIs for adding crashes.
  // Switch to that here.
  let store = yield manager._getStore();
  store.addMainProcessCrash("id1", day1);
  store.addMainProcessCrash("id2", day1);
  store.addMainProcessCrash("id3", day2);

  // Flush changes (this may not be needed but it doesn't hurt).
  yield store.save();

  yield provider.collectDailyData();

  let m = provider.getMeasurement("crashes", 2);
  let values = yield m.getValues();
  do_check_eq(values.days.size, 2);
  do_check_true(values.days.hasDay(day1));
  do_check_true(values.days.hasDay(day2));

  let value = values.days.getDay(day1);
  do_check_true(value.has("mainCrash"));
  do_check_eq(value.get("mainCrash"), 2);

  value = values.days.getDay(day2);
  do_check_eq(value.get("mainCrash"), 1);

  // Check that adding a new crash increments counter on next collect.
  store = yield manager._getStore();
  store.addMainProcessCrash("id4", day2);
  yield store.save();

  yield provider.collectDailyData();
  values = yield m.getValues();
  value = values.days.getDay(day2);
  do_check_eq(value.get("mainCrash"), 2);

  yield provider.shutdown();
  yield storage.close();
});

