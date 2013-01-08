/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;


Cu.import("resource://gre/modules/commonjs/promise/core.js");
Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/services-common/utils.js");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");


function run_test() {
  run_next_test();
}

add_test(function test_constructor() {
  let provider = new SessionsProvider();

  run_next_test();
});

add_task(function test_init() {
  let storage = yield Metrics.Storage("init");
  let provider = new SessionsProvider();
  yield provider.init(storage);
  yield provider.shutdown();

  yield storage.close();
});

function getProvider(name, now=new Date()) {
  return Task.spawn(function () {
    let storage = yield Metrics.Storage(name);
    let provider = new SessionsProvider();
    monkeypatchStartupInfo(provider, now);
    yield provider.init(storage);

    throw new Task.Result([provider, storage]);
  });
}

function monkeypatchStartupInfo(provider, start=new Date(), offset=500) {
  Object.defineProperty(provider, "_getStartupInfo", {
    value: function _getStartupInfo() {
      return {
        process: start,
        main: new Date(start.getTime() + offset),
        firstPaint: new Date(start.getTime() + 2 * offset),
        sessionRestored: new Date(start.getTime() + 3 * offset),
      };
    }
  });
}

add_task(function test_record_current_on_init() {
  let [provider, storage] = yield getProvider("record_current_on_init");

  let now = new Date();

  let current = provider.getMeasurement("current", 1);
  let values = yield current.getValues();
  let fields = values.singular;
  do_check_eq(fields.size, 6);
  do_check_eq(fields.get("main")[1], 500);
  do_check_eq(fields.get("firstPaint")[1], 1000);
  do_check_eq(fields.get("sessionRestored")[1], 1500);
  do_check_eq(fields.get("startDay")[1], provider._dateToDays(now));
  do_check_eq(fields.get("activeTime")[1], 0);
  do_check_eq(fields.get("totalTime")[1], 0);

  yield provider.shutdown();
  yield storage.close();
});

add_task(function test_current_moved_on_shutdown() {
  let [provider, storage] = yield getProvider("current_moved_on_shutdown");
  let now = new Date();

  let previous = provider.getMeasurement("previous", 1);

  yield provider.shutdown();

  // This relies on undocumented behavior of the underlying measurement not
  // being invalidated on provider shutdown. If this breaks, we should rewrite
  // the test and not hold up implementation changes.
  let values = yield previous.getValues();
  do_check_eq(values.days.size, 1);
  do_check_true(values.days.hasDay(now));
  let fields = values.days.getDay(now);

  // 3 startup + 2 clean.
  do_check_eq(fields.size, 5);
  for (let field of ["cleanActiveTime", "cleanTotalTime", "main", "firstPaint", "sessionRestored"]) {
    do_check_true(fields.has(field));
    do_check_true(Array.isArray(fields.get(field)));
    do_check_eq(fields.get(field).length, 1);
  }

  do_check_eq(fields.get("main")[0], 500);
  do_check_eq(fields.get("firstPaint")[0], 1000);
  do_check_eq(fields.get("sessionRestored")[0], 1500);
  do_check_true(fields.get("cleanActiveTime")[0] > 0);
  do_check_true(fields.get("cleanTotalTime")[0] > 0);

  yield storage.close();
});

add_task(function test_detect_abort() {
  let [provider, storage] = yield getProvider("detect_abort");

  let now = new Date();

  let m = provider.getMeasurement("current", 1);
  let original = yield m.getValues().singular;

  let provider2 = new SessionsProvider();
  monkeypatchStartupInfo(provider2);
  yield provider2.init(storage);

  let previous = provider.getMeasurement("previous", 1);
  let values = yield previous.getValues();
  do_check_true(values.days.hasDay(now));
  let day = values.days.getDay(now);
  do_check_eq(day.size, 5);
  do_check_true(day.has("abortedActiveTime"));
  do_check_true(day.has("abortedTotalTime"));
  do_check_eq(day.get("abortedActiveTime")[0], 0);
  do_check_eq(day.get("abortedTotalTime")[0], 0);

  yield provider.shutdown();
  yield provider2.shutdown();
  yield storage.close();
});

// This isn't a perfect test because we only simulate the observer
// notifications. We should probably supplement this with a mochitest.
add_task(function test_record_browser_activity() {
  let [provider, storage] = yield getProvider("record_browser_activity");

  function waitOnDB () {
    return provider.enqueueStorageOperation(function () {
      return storage._connection.execute("SELECT 1");
    });
  }

  let current = provider.getMeasurement("current", 1);

  Services.obs.notifyObservers(null, "user-interaction-active", null);
  yield waitOnDB();

  let values = yield current.getValues();
  let fields = values.singular;
  let activeTime = fields.get("activeTime")[1];
  let totalTime = fields.get("totalTime")[1];

  do_check_eq(activeTime, totalTime);
  do_check_true(activeTime > 0);

  // Another active should have similar effects.
  Services.obs.notifyObservers(null, "user-interaction-active", null);
  yield waitOnDB();

  values = yield current.getValues();
  fields = values.singular;

  do_check_true(fields.get("activeTime")[1] > activeTime);
  activeTime = fields.get("activeTime")[1];
  totalTime = fields.get("totalTime")[1];
  do_check_eq(activeTime, totalTime);

  // Now send inactive. We should increment total time but not active.
  Services.obs.notifyObservers(null, "user-interaction-inactive", null);
  yield waitOnDB();
  values = yield current.getValues();
  fields = values.singular;
  do_check_eq(fields.get("activeTime")[1], activeTime);
  totalTime = fields.get("totalTime")[1];
  do_check_true(totalTime > activeTime);

  // If we send an active again, this should be counted as inactive.
  Services.obs.notifyObservers(null, "user-interaction-active", null);
  yield waitOnDB();
  values = yield current.getValues();
  fields = values.singular;

  do_check_eq(fields.get("activeTime")[1], activeTime);
  do_check_true(fields.get("totalTime")[1] > totalTime);
  do_check_neq(fields.get("activeTime")[1], fields.get("totalTime")[1]);
  activeTime = fields.get("activeTime")[1];
  totalTime = fields.get("totalTime")[1];

  // Another active should increment active this time.
  Services.obs.notifyObservers(null, "user-interaction-active", null);
  yield waitOnDB();
  values = yield current.getValues();
  fields = values.singular;
  do_check_true(fields.get("activeTime")[1] > activeTime);
  do_check_true(fields.get("totalTime")[1] > totalTime);

  yield provider.shutdown();
  yield storage.close();
});

