/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu, classes: Cc, interfaces: Ci} = Components;


Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/UpdaterHealthProvider.jsm");

// Create a profile
let gProfile = do_get_profile();

function run_test() {
  run_next_test();
}

add_test(function test_constructor() {
  let provider = new UpdateProvider();
  run_next_test();
});

add_task(function test_init() {
  let storage = yield Metrics.Storage("init");
  let provider = new UpdateProvider();
  yield provider.init(storage);
  yield provider.shutdown();

  yield storage.close();
});

add_task(function test_collect() {
  let storage = yield Metrics.Storage("collect");
  let provider = new UpdateProvider();
  yield provider.init(storage);

  let now = new Date();

  let m = provider.getMeasurement("update", 1);

  let fieldcount = 0;
  for (let field of ["updateCheckStart",
                     "updateCheckSuccess",
                     "completeUpdateStart",
                     "partialUpdateStart",
                     "completeUpdateSuccess",
                     "partialUpdateSuccess"]) {
    fieldcount++; // One new day per iteration

    yield provider.recordUpdate(field, 0);

    let data = yield m.getValues();
    do_check_eq(data.days.size, 1);

    let day = data.days.getDay(now);
    do_check_eq(day.size, fieldcount);
    do_check_eq(day.get(field + "Count"), 1);

    yield provider.recordUpdate(field, 0);

    data = yield m.getValues();
    day = data.days.getDay(now);
    do_check_eq(day.size, fieldcount);
    do_check_eq(day.get(field + "Count"), 2);
  }

  for (let field of ["updateCheckFailed", "updateFailed"]) {
    fieldcount += 2; // Two fields added per iteration

    yield provider.recordUpdate(field, 500);

    let data = yield m.getValues();
    let day = data.days.getDay(now);
    do_check_eq(day.size, fieldcount);

    do_check_eq(day.get(field + "Statuses"), 500);

    yield provider.recordUpdate(field, 800);

    data = yield m.getValues();
    day = data.days.getDay(now);
    do_check_eq(day.size, fieldcount);
    do_check_eq(day.get(field + "Statuses")[0], 500);
    do_check_eq(day.get(field + "Statuses")[1], 800);
  }

  yield provider.shutdown();
  yield storage.close();
});

