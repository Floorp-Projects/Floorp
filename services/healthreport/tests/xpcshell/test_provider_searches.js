/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Metrics.jsm");
let bsp = Cu.import("resource://gre/modules/services/healthreport/providers.jsm");

const DEFAULT_ENGINES = [
  {name: "Amazon.com",    identifier: "amazondotcom"},
  {name: "Bing",          identifier: "bing"},
  {name: "Google",        identifier: "google"},
  {name: "Yahoo",         identifier: "yahoo"},
  {name: "Foobar Search", identifier: "foobar"},
];

function MockSearchCountMeasurement() {
  bsp.SearchCountMeasurement2.call(this);
}
MockSearchCountMeasurement.prototype = {
  __proto__: bsp.SearchCountMeasurement2.prototype,
  getDefaultEngines: function () {
    return DEFAULT_ENGINES;
  },
};

function MockSearchesProvider() {
  SearchesProvider.call(this);
}
MockSearchesProvider.prototype = {
  __proto__: SearchesProvider.prototype,
  measurementTypes: [MockSearchCountMeasurement],
};

function run_test() {
  run_next_test();
}

add_test(function test_constructor() {
  let provider = new SearchesProvider();

  run_next_test();
});

add_task(function test_record() {
  let storage = yield Metrics.Storage("record");
  let provider = new MockSearchesProvider();

  yield provider.init(storage);

  let now = new Date();

  for (let engine of DEFAULT_ENGINES) {
    yield provider.recordSearch(engine.name, "abouthome");
    yield provider.recordSearch(engine.name, "contextmenu");
    yield provider.recordSearch(engine.name, "searchbar");
    yield provider.recordSearch(engine.name, "urlbar");
  }

  // Invalid sources should throw.
  let errored = false;
  try {
    yield provider.recordSearch(DEFAULT_ENGINES[0].name, "bad source");
  } catch (ex) {
    errored = true;
  } finally {
    do_check_true(errored);
  }

  let m = provider.getMeasurement("counts", 2);
  let data = yield m.getValues();
  do_check_eq(data.days.size, 1);
  do_check_true(data.days.hasDay(now));

  let day = data.days.getDay(now);
  for (let engine of DEFAULT_ENGINES) {
    let identifier = engine.identifier;
    if (identifier == "foobar") {
      identifier = "other";
    }

    for (let source of ["abouthome", "contextmenu", "searchbar", "urlbar"]) {
      let field = identifier + "." + source;
      do_check_true(day.has(field));
      do_check_eq(day.get(field), 1);
    }
  }

  yield storage.close();
});

add_task(function test_includes_other_fields() {
  let storage = yield Metrics.Storage("includes_other_fields");
  let provider = new MockSearchesProvider();

  yield provider.init(storage);
  let m = provider.getMeasurement("counts", 2);

  // Register a search against a provider that isn't live in this session.
  let id = yield m.storage.registerField(m.id, "test.searchbar",
                                         Metrics.Storage.FIELD_DAILY_COUNTER);

  let testField = "test.searchbar";
  let now = new Date();
  yield m.storage.incrementDailyCounterFromFieldID(id, now);

  // Make sure we don't know about it.
  do_check_false(testField in m.fields);

  // But we want to include it in payloads.
  do_check_true(m.shouldIncludeField(testField));

  // And we do so.
  let data = yield provider.storage.getMeasurementValues(m.id);
  let serializer = m.serializer(m.SERIALIZE_JSON);
  let formatted = serializer.daily(data.days.getDay(now));
  do_check_true(testField in formatted);
  do_check_eq(formatted[testField], 1);

  yield storage.close();
});
