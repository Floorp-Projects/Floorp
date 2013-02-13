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
  let provider = new SearchesProvider();

  run_next_test();
});

add_task(function test_record() {
  let storage = yield Metrics.Storage("record");
  let provider = new SearchesProvider();

  yield provider.init(storage);

  const ENGINES = [
    "amazon.com",
    "bing",
    "google",
    "yahoo",
    "foobar",
  ];

  let now = new Date();

  for (let engine of ENGINES) {
    yield provider.recordSearch(engine, "abouthome");
    yield provider.recordSearch(engine, "contextmenu");
    yield provider.recordSearch(engine, "searchbar");
    yield provider.recordSearch(engine, "urlbar");
  }

  // Invalid sources should throw.
  let errored = false;
  try {
    yield provider.recordSearch("google", "bad source");
  } catch (ex) {
    errored = true;
  } finally {
    do_check_true(errored);
  }

  let m = provider.getMeasurement("counts", 1);
  let data = yield m.getValues();
  do_check_eq(data.days.size, 1);
  do_check_true(data.days.hasDay(now));

  let day = data.days.getDay(now);
  for (let engine of ENGINES) {
    if (engine == "foobar") {
      engine = "other";
    }

    for (let source of ["abouthome", "contextmenu", "searchbar", "urlbar"]) {
      let field = engine + "." + source;
      do_check_true(day.has(field));
      do_check_eq(day.get(field), 1);
    }
  }

  yield storage.close();
});

