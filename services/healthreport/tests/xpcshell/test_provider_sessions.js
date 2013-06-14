/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;


Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://gre/modules/services-common/utils.js");
Cu.import("resource://gre/modules/services/datareporting/sessions.jsm");
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

function monkeypatchStartupInfo(recorder, start=new Date(), offset=500) {
  Object.defineProperty(recorder, "_getStartupInfo", {
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

function sleep(wait) {
  let deferred = Promise.defer();

  let timer = CommonUtils.namedTimer(function onTimer() {
    deferred.resolve();
  }, wait, deferred.promise, "_sleepTimer");

  return deferred.promise;
}

function getProvider(name, now=new Date(), init=true) {
  return Task.spawn(function () {
    let storage = yield Metrics.Storage(name);
    let provider = new SessionsProvider();

    let recorder = new SessionRecorder("testing." + name + ".sessions.");
    monkeypatchStartupInfo(recorder, now);
    provider.healthReporter = {sessionRecorder: recorder};
    recorder.onStartup();

    if (init) {
      yield provider.init(storage);
    }

    throw new Task.Result([provider, storage, recorder]);
  });
}

add_task(function test_current_session() {
  let now = new Date();
  let [provider, storage, recorder] = yield getProvider("current_session", now);

  yield sleep(25);
  recorder.onActivity(true);

  let current = provider.getMeasurement("current", 3);
  let values = yield current.getValues();
  let fields = values.singular;

  for (let field of ["startDay", "activeTicks", "totalTime", "main", "firstPaint", "sessionRestored"]) {
    do_check_true(fields.has(field));
  }

  do_check_eq(fields.get("startDay")[1], Metrics.dateToDays(now));
  do_check_eq(fields.get("totalTime")[1], recorder.totalTime);
  do_check_eq(fields.get("activeTicks")[1], 1);
  do_check_eq(fields.get("main")[1], 500);
  do_check_eq(fields.get("firstPaint")[1], 1000);
  do_check_eq(fields.get("sessionRestored")[1], 1500);

  yield provider.shutdown();
  yield storage.close();
});

add_task(function test_collect() {
  let now = new Date();
  let [provider, storage, recorder] = yield getProvider("collect");

  recorder.onShutdown();
  yield sleep(25);

  for (let i = 0; i < 5; i++) {
    let recorder2 = new SessionRecorder("testing.collect.sessions.");
    recorder2.onStartup();
    yield sleep(25);
    recorder2.onShutdown();
    yield sleep(25);
  }

  recorder = new SessionRecorder("testing.collect.sessions.");
  recorder.onStartup();

  // Collecting the provider should prune all previous sessions.
  let sessions = recorder.getPreviousSessions();
  do_check_eq(Object.keys(sessions).length, 6);
  yield provider.collectConstantData();
  sessions = recorder.getPreviousSessions();
  do_check_eq(Object.keys(sessions).length, 0);

  // And those previous sessions should make it to storage.
  let daily = provider.getMeasurement("previous", 3);
  let values = yield daily.getValues();
  do_check_true(values.days.hasDay(now));
  do_check_eq(values.days.size, 1);
  let day = values.days.getDay(now);
  do_check_eq(day.size, 5);
  let previousStorageCount = day.get("main").length;

  for (let field of ["cleanActiveTicks", "cleanTotalTime", "main", "firstPaint", "sessionRestored"]) {
    do_check_true(day.has(field));
    do_check_true(Array.isArray(day.get(field)));
    do_check_eq(day.get(field).length, 6);
  }

  let lastIndex = yield provider.getState("lastSession");
  do_check_eq(lastIndex, "" + (previousStorageCount - 1)); // 0-indexed

  // Fake an aborted session. If we create a 2nd recorder against the same
  // prefs branch as a running one, this simulates what would happen if the
  // first recorder didn't shut down.
  let recorder2 = new SessionRecorder("testing.collect.sessions.");
  recorder2.onStartup();
  do_check_eq(Object.keys(recorder.getPreviousSessions()).length, 1);
  yield provider.collectConstantData();
  do_check_eq(Object.keys(recorder.getPreviousSessions()).length, 0);

  values = yield daily.getValues();
  day = values.days.getDay(now);
  do_check_eq(day.size, previousStorageCount + 1);
  previousStorageCount = day.get("main").length;
  for (let field of ["abortedActiveTicks", "abortedTotalTime"]) {
    do_check_true(day.has(field));
    do_check_true(Array.isArray(day.get(field)));
    do_check_eq(day.get(field).length, 1);
  }

  lastIndex = yield provider.getState("lastSession");
  do_check_eq(lastIndex, "" + (previousStorageCount - 1));

  recorder.onShutdown();
  recorder2.onShutdown();

  // If we try to insert a already-inserted session, it will be ignored.
  recorder = new SessionRecorder("testing.collect.sessions.");
  recorder._currentIndex = recorder._currentIndex - 1;
  recorder._prunedIndex = recorder._currentIndex;
  recorder.onStartup();
  // Session is left over from recorder2.
  sessions = recorder.getPreviousSessions();
  do_check_eq(Object.keys(sessions).length, 1);
  do_check_true(previousStorageCount - 1 in sessions);
  yield provider.collectConstantData();
  lastIndex = yield provider.getState("lastSession");
  do_check_eq(lastIndex, "" + (previousStorageCount - 1));
  values = yield daily.getValues();
  day = values.days.getDay(now);
  // We should not get additional entry.
  do_check_eq(day.get("main").length, previousStorageCount);
  recorder.onShutdown();

  yield provider.shutdown();
  yield storage.close();
});

add_task(function test_serialization() {
  let [provider, storage, recorder] = yield getProvider("serialization");

  yield sleep(1025);
  recorder.onActivity(true);

  let current = provider.getMeasurement("current", 3);
  let data = yield current.getValues();
  do_check_true("singular" in data);

  let serializer = current.serializer(current.SERIALIZE_JSON);
  let fields = serializer.singular(data.singular);

  do_check_eq(fields._v, 3);
  do_check_eq(fields.activeTicks, 1);
  do_check_eq(fields.startDay, Metrics.dateToDays(recorder.startDate));
  do_check_eq(fields.main, 500);
  do_check_eq(fields.firstPaint, 1000);
  do_check_eq(fields.sessionRestored, 1500);
  do_check_true(fields.totalTime > 0);

  yield provider.shutdown();
  yield storage.close();
});

