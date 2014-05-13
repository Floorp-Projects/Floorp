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

  yield manager.addCrash(manager.PROCESS_TYPE_MAIN,
                         manager.CRASH_TYPE_CRASH,
                         "mc1", day1);
  yield manager.addCrash(manager.PROCESS_TYPE_MAIN,
                         manager.CRASH_TYPE_CRASH,
                         "mc2", day1);
  yield manager.addCrash(manager.PROCESS_TYPE_CONTENT,
                         manager.CRASH_TYPE_HANG,
                         "ch", day1);
  yield manager.addCrash(manager.PROCESS_TYPE_PLUGIN,
                         manager.CRASH_TYPE_CRASH,
                         "pc", day1);

  yield manager.addCrash(manager.PROCESS_TYPE_MAIN,
                         manager.CRASH_TYPE_HANG,
                         "mh", day2);
  yield manager.addCrash(manager.PROCESS_TYPE_CONTENT,
                         manager.CRASH_TYPE_CRASH,
                         "cc", day2);
  yield manager.addCrash(manager.PROCESS_TYPE_PLUGIN,
                         manager.CRASH_TYPE_HANG,
                         "ph", day2);

  yield provider.collectDailyData();

  let m = provider.getMeasurement("crashes", 3);
  let values = yield m.getValues();
  do_check_eq(values.days.size, 2);
  do_check_true(values.days.hasDay(day1));
  do_check_true(values.days.hasDay(day2));

  let value = values.days.getDay(day1);
  do_check_true(value.has("main-crash"));
  do_check_eq(value.get("main-crash"), 2);
  do_check_true(value.has("content-hang"));
  do_check_eq(value.get("content-hang"), 1);
  do_check_true(value.has("plugin-crash"));
  do_check_eq(value.get("plugin-crash"), 1);

  value = values.days.getDay(day2);
  do_check_true(value.has("main-hang"));
  do_check_eq(value.get("main-hang"), 1);
  do_check_true(value.has("content-crash"));
  do_check_eq(value.get("content-crash"), 1);
  do_check_true(value.has("plugin-hang"));
  do_check_eq(value.get("plugin-hang"), 1);

  // Check that adding a new crash increments counter on next collect.
  yield manager.addCrash(manager.PROCESS_TYPE_MAIN,
                         manager.CRASH_TYPE_HANG,
                         "mc3", day2);

  yield provider.collectDailyData();
  values = yield m.getValues();
  value = values.days.getDay(day2);
  do_check_eq(value.get("main-hang"), 2);

  yield provider.shutdown();
  yield storage.close();
});

