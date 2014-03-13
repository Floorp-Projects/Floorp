/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;


Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");
Cu.import("resource://testing-common/services/healthreport/utils.jsm");
Cu.import("resource://testing-common/AppData.jsm");


const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;


function run_test() {
  run_next_test();
}

// run_test() needs to finish synchronously, so we do async init here.
add_task(function test_init() {
  yield makeFakeAppDir();
});

let gPending = {};
let gSubmitted = {};

add_task(function test_directory_service() {
  let d = new CrashDirectoryService();

  let entries = yield d.getPendingFiles();
  do_check_eq(typeof(entries), "object");
  do_check_eq(Object.keys(entries).length, 0);

  entries = yield d.getSubmittedFiles();
  do_check_eq(typeof(entries), "object");
  do_check_eq(Object.keys(entries).length, 0);

  let now = new Date();

  // We lose granularity when writing to filesystem.
  now.setUTCMilliseconds(0);
  let dates = [];
  for (let i = 0; i < 10; i++) {
    dates.push(new Date(now.getTime() - i * MILLISECONDS_PER_DAY));
  }

  let pending = {};
  let submitted = {};
  for (let date of dates) {
    pending[createFakeCrash(false, date)] = date;
    submitted[createFakeCrash(true, date)] = date;
  }

  entries = yield d.getPendingFiles();
  do_check_eq(Object.keys(entries).length, Object.keys(pending).length);
  for (let id in pending) {
    let filename = id + ".dmp";
    do_check_true(filename in entries);
    do_check_eq(entries[filename].modified.getTime(), pending[id].getTime());
  }

  entries = yield d.getSubmittedFiles();
  do_check_eq(Object.keys(entries).length, Object.keys(submitted).length);
  for (let id in submitted) {
    let filename = "bp-" + id + ".txt";
    do_check_true(filename in entries);
    do_check_eq(entries[filename].modified.getTime(), submitted[id].getTime());
  }

  gPending = pending;
  gSubmitted = submitted;
});

add_test(function test_constructor() {
  let provider = new CrashesProvider();

  run_next_test();
});

add_task(function test_init() {
  let storage = yield Metrics.Storage("init");
  let provider = new CrashesProvider();
  yield provider.init(storage);
  yield provider.shutdown();

  yield storage.close();
});

add_task(function test_collect() {
  let storage = yield Metrics.Storage("collect");
  let provider = new CrashesProvider();
  yield provider.init(storage);

  // FUTURE Don't rely on state from previous test.
  yield provider.collectConstantData();

  let m = provider.getMeasurement("crashes", 1);
  let values = yield m.getValues();
  do_check_eq(values.days.size, Object.keys(gPending).length);
  for each (let date in gPending) {
    do_check_true(values.days.hasDay(date));

    let value = values.days.getDay(date);
    do_check_true(value.has("pending"));
    do_check_true(value.has("submitted"));
    do_check_eq(value.get("pending"), 1);
    do_check_eq(value.get("submitted"), 1);
  }

  let currentState = yield provider.getState("lastCheck");
  do_check_eq(typeof(currentState), "string");
  do_check_true(currentState.length > 0);
  let lastState = currentState;

  // If we collect again, we should get no new data.
  yield provider.collectConstantData();
  values = yield m.getValues();
  for each (let date in gPending) {
    let day = values.days.getDay(date);
    do_check_eq(day.get("pending"), 1);
    do_check_eq(day.get("submitted"), 1);
  }

  currentState = yield provider.getState("lastCheck");
  do_check_neq(currentState, lastState);
  do_check_true(currentState > lastState);

  let now = new Date();
  let tomorrow = new Date(now.getTime() + MILLISECONDS_PER_DAY);
  let yesterday = new Date(now.getTime() - MILLISECONDS_PER_DAY);

  createFakeCrash(false, yesterday);

  // Create multiple to test that multiple are handled properly.
  createFakeCrash(false, tomorrow);
  createFakeCrash(false, tomorrow);
  createFakeCrash(false, tomorrow);

  yield provider.collectConstantData();
  values = yield m.getValues();
  do_check_eq(values.days.size, 11);
  do_check_eq(values.days.getDay(tomorrow).get("pending"), 3);

  for each (let date in gPending) {
    let day = values.days.getDay(date);
    do_check_eq(day.get("pending"), 1);
    do_check_eq(day.get("submitted"), 1);
  }

  yield provider.shutdown();
  yield storage.close();
});

