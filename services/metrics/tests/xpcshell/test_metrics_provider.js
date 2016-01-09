/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var {utils: Cu} = Components;

Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/Task.jsm");
Cu.import("resource://testing-common/services/metrics/mocks.jsm");


const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;


function getProvider(storageName) {
  return Task.spawn(function () {
    let provider = new DummyProvider();
    let storage = yield Metrics.Storage(storageName);

    yield provider.init(storage);

    throw new Task.Result(provider);
  });
}


function run_test() {
  run_next_test();
};

add_test(function test_constructor() {
  let failed = false;
  try {
    new Metrics.Provider();
  } catch(ex) {
    do_check_true(ex.message.startsWith("Provider must define a name"));
    failed = true;
  }
  finally {
    do_check_true(failed);
  }

  run_next_test();
});

add_task(function test_init() {
  let provider = new DummyProvider();
  let storage = yield Metrics.Storage("init");

  yield provider.init(storage);

  let m = provider.getMeasurement("DummyMeasurement", 1);
  do_check_true(m instanceof Metrics.Measurement);
  do_check_eq(m.id, 1);
  do_check_eq(Object.keys(m._fields).length, 7);
  do_check_true(m.hasField("daily-counter"));
  do_check_false(m.hasField("does-not-exist"));

  yield storage.close();
});

add_task(function test_default_collectors() {
  let provider = new DummyProvider();
  let storage = yield Metrics.Storage("default_collectors");
  yield provider.init(storage);

  for (let property in Metrics.Provider.prototype) {
    if (!property.startsWith("collect")) {
      continue;
    }

    let result = provider[property]();
    do_check_neq(result, null);
    do_check_eq(typeof(result.then), "function");
  }

  yield storage.close();
});

add_task(function test_measurement_storage_basic() {
  let provider = yield getProvider("measurement_storage_basic");
  let m = provider.getMeasurement("DummyMeasurement", 1);

  let now = new Date();
  let yesterday = new Date(now.getTime() - MILLISECONDS_PER_DAY);

  // Daily counter.
  let counterID = m.fieldID("daily-counter");
  yield m.incrementDailyCounter("daily-counter", now);
  yield m.incrementDailyCounter("daily-counter", now);
  yield m.incrementDailyCounter("daily-counter", yesterday);
  let count = yield provider.storage.getDailyCounterCountFromFieldID(counterID, now);
  do_check_eq(count, 2);

  count = yield provider.storage.getDailyCounterCountFromFieldID(counterID, yesterday);
  do_check_eq(count, 1);

  yield m.incrementDailyCounter("daily-counter", now, 4);
  count = yield provider.storage.getDailyCounterCountFromFieldID(counterID, now);
  do_check_eq(count, 6);

  // Daily discrete numeric.
  let dailyDiscreteNumericID = m.fieldID("daily-discrete-numeric");
  yield m.addDailyDiscreteNumeric("daily-discrete-numeric", 5, now);
  yield m.addDailyDiscreteNumeric("daily-discrete-numeric", 6, now);
  yield m.addDailyDiscreteNumeric("daily-discrete-numeric", 7, yesterday);

  let values = yield provider.storage.getDailyDiscreteNumericFromFieldID(
    dailyDiscreteNumericID, now);

  do_check_eq(values.size, 1);
  do_check_true(values.hasDay(now));
  let actual = values.getDay(now);
  do_check_eq(actual.length, 2);
  do_check_eq(actual[0], 5);
  do_check_eq(actual[1], 6);

  values = yield provider.storage.getDailyDiscreteNumericFromFieldID(
    dailyDiscreteNumericID, yesterday);

  do_check_eq(values.size, 1);
  do_check_true(values.hasDay(yesterday));
  do_check_eq(values.getDay(yesterday)[0], 7);

  // Daily discrete text.
  let dailyDiscreteTextID = m.fieldID("daily-discrete-text");
  yield m.addDailyDiscreteText("daily-discrete-text", "foo", now);
  yield m.addDailyDiscreteText("daily-discrete-text", "bar", now);
  yield m.addDailyDiscreteText("daily-discrete-text", "biz", yesterday);

  values = yield provider.storage.getDailyDiscreteTextFromFieldID(
    dailyDiscreteTextID, now);

  do_check_eq(values.size, 1);
  do_check_true(values.hasDay(now));
  actual = values.getDay(now);
  do_check_eq(actual.length, 2);
  do_check_eq(actual[0], "foo");
  do_check_eq(actual[1], "bar");

  values = yield provider.storage.getDailyDiscreteTextFromFieldID(
    dailyDiscreteTextID, yesterday);
  do_check_true(values.hasDay(yesterday));
  do_check_eq(values.getDay(yesterday)[0], "biz");

  // Daily last numeric.
  let lastDailyNumericID = m.fieldID("daily-last-numeric");
  yield m.setDailyLastNumeric("daily-last-numeric", 5, now);
  yield m.setDailyLastNumeric("daily-last-numeric", 6, yesterday);

  let result = yield provider.storage.getDailyLastNumericFromFieldID(
    lastDailyNumericID, now);
  do_check_eq(result.size, 1);
  do_check_true(result.hasDay(now));
  do_check_eq(result.getDay(now), 5);

  result = yield provider.storage.getDailyLastNumericFromFieldID(
    lastDailyNumericID, yesterday);
  do_check_true(result.hasDay(yesterday));
  do_check_eq(result.getDay(yesterday), 6);

  yield m.setDailyLastNumeric("daily-last-numeric", 7, now);
  result = yield provider.storage.getDailyLastNumericFromFieldID(
    lastDailyNumericID, now);
  do_check_eq(result.getDay(now), 7);

  // Daily last text.
  let lastDailyTextID = m.fieldID("daily-last-text");
  yield m.setDailyLastText("daily-last-text", "foo", now);
  yield m.setDailyLastText("daily-last-text", "bar", yesterday);

  result = yield provider.storage.getDailyLastTextFromFieldID(
    lastDailyTextID, now);
  do_check_eq(result.size, 1);
  do_check_true(result.hasDay(now));
  do_check_eq(result.getDay(now), "foo");

  result = yield provider.storage.getDailyLastTextFromFieldID(
    lastDailyTextID, yesterday);
  do_check_true(result.hasDay(yesterday));
  do_check_eq(result.getDay(yesterday), "bar");

  yield m.setDailyLastText("daily-last-text", "biz", now);
  result = yield provider.storage.getDailyLastTextFromFieldID(
    lastDailyTextID, now);
  do_check_eq(result.getDay(now), "biz");

  // Last numeric.
  let lastNumericID = m.fieldID("last-numeric");
  yield m.setLastNumeric("last-numeric", 1, now);
  result = yield provider.storage.getLastNumericFromFieldID(lastNumericID);
  do_check_eq(result[1], 1);
  do_check_true(result[0].getTime() < now.getTime());
  do_check_true(result[0].getTime() > yesterday.getTime());

  yield m.setLastNumeric("last-numeric", 2, now);
  result = yield provider.storage.getLastNumericFromFieldID(lastNumericID);
  do_check_eq(result[1], 2);

  // Last text.
  let lastTextID = m.fieldID("last-text");
  yield m.setLastText("last-text", "foo", now);
  result = yield provider.storage.getLastTextFromFieldID(lastTextID);
  do_check_eq(result[1], "foo");
  do_check_true(result[0].getTime() < now.getTime());
  do_check_true(result[0].getTime() > yesterday.getTime());

  yield m.setLastText("last-text", "bar", now);
  result = yield provider.storage.getLastTextFromFieldID(lastTextID);
  do_check_eq(result[1], "bar");

  yield provider.storage.close();
});

add_task(function test_serialize_json_default() {
  let provider = yield getProvider("serialize_json_default");

  let now = new Date();
  let yesterday = new Date(now.getTime() - MILLISECONDS_PER_DAY);

  let m = provider.getMeasurement("DummyMeasurement", 1);

  m.incrementDailyCounter("daily-counter", now);
  m.incrementDailyCounter("daily-counter", now);
  m.incrementDailyCounter("daily-counter", yesterday);

  m.addDailyDiscreteNumeric("daily-discrete-numeric", 1, now);
  m.addDailyDiscreteNumeric("daily-discrete-numeric", 2, now);
  m.addDailyDiscreteNumeric("daily-discrete-numeric", 3, yesterday);

  m.addDailyDiscreteText("daily-discrete-text", "foo", now);
  m.addDailyDiscreteText("daily-discrete-text", "bar", now);
  m.addDailyDiscreteText("daily-discrete-text", "baz", yesterday);

  m.setDailyLastNumeric("daily-last-numeric", 4, now);
  m.setDailyLastNumeric("daily-last-numeric", 5, yesterday);

  m.setDailyLastText("daily-last-text", "apple", now);
  m.setDailyLastText("daily-last-text", "orange", yesterday);

  m.setLastNumeric("last-numeric", 6, now);
  yield m.setLastText("last-text", "hello", now);

  let data = yield provider.storage.getMeasurementValues(m.id);

  let serializer = m.serializer(m.SERIALIZE_JSON);
  let formatted = serializer.singular(data.singular);

  do_check_eq(Object.keys(formatted).length, 3);  // Our keys + _v.
  do_check_true("last-numeric" in formatted);
  do_check_true("last-text" in formatted);
  do_check_eq(formatted["last-numeric"], 6);
  do_check_eq(formatted["last-text"], "hello");
  do_check_eq(formatted["_v"], 1);

  formatted = serializer.daily(data.days.getDay(now));
  do_check_eq(Object.keys(formatted).length, 6);  // Our keys + _v.
  do_check_eq(formatted["daily-counter"], 2);
  do_check_eq(formatted["_v"], 1);

  do_check_true(Array.isArray(formatted["daily-discrete-numeric"]));
  do_check_eq(formatted["daily-discrete-numeric"].length, 2);
  do_check_eq(formatted["daily-discrete-numeric"][0], 1);
  do_check_eq(formatted["daily-discrete-numeric"][1], 2);

  do_check_true(Array.isArray(formatted["daily-discrete-text"]));
  do_check_eq(formatted["daily-discrete-text"].length, 2);
  do_check_eq(formatted["daily-discrete-text"][0], "foo");
  do_check_eq(formatted["daily-discrete-text"][1], "bar");

  do_check_eq(formatted["daily-last-numeric"], 4);
  do_check_eq(formatted["daily-last-text"], "apple");

  formatted = serializer.daily(data.days.getDay(yesterday));
  do_check_eq(formatted["daily-last-numeric"], 5);
  do_check_eq(formatted["daily-last-text"], "orange");

  // Now let's turn off a field so that it's present in the DB
  // but not present in the output.
  let called = false;
  let excluded = "daily-last-numeric";
  Object.defineProperty(m, "shouldIncludeField", {
    value: function fakeShouldIncludeField(field) {
      called = true;
      return field != excluded;
    },
  });

  let limited = serializer.daily(data.days.getDay(yesterday));
  do_check_true(called);
  do_check_false(excluded in limited);
  do_check_eq(formatted["daily-last-text"], "orange");

  yield provider.storage.close();
});
