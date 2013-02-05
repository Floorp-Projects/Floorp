/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");


function run_test() {
  run_next_test();
}

add_test(function test_constructor() {
  let provider = new PlacesProvider();

  run_next_test();
});

add_task(function test_collect_smoketest() {
  let storage = yield Metrics.Storage("collect_smoketest");
  let provider = new PlacesProvider();

  yield provider.init(storage);

  let now = new Date();
  yield provider.collectDailyData();

  let m = provider.getMeasurement("places", 1);
  let data = yield storage.getMeasurementValues(m.id);
  do_check_eq(data.days.size, 1);
  do_check_true(data.days.hasDay(now));

  let serializer = m.serializer(m.SERIALIZE_JSON);
  let day = serializer.daily(data.days.getDay(now));

  do_check_eq(day._v, 1);
  do_check_eq(Object.keys(day).length, 3);
  do_check_eq(day.pages, 0);
  do_check_eq(day.bookmarks, 0);

  yield storage.close();
});

