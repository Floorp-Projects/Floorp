/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://services-common/utils.js");


const MILLISECONDS_PER_DAY = 24 * 60 * 60 * 1000;


function run_test() {
  run_next_test();
}

add_test(function test_days_date_conversion() {
  let toDays = Metrics.dateToDays;
  let toDate = Metrics.daysToDate;

  let d = new Date(0);
  do_check_eq(toDays(d), 0);

  d = new Date(MILLISECONDS_PER_DAY);
  do_check_eq(toDays(d), 1);

  d = new Date(MILLISECONDS_PER_DAY - 1);
  do_check_eq(toDays(d), 0);

  d = new Date("1970-12-31T23:59:59.999Z");
  do_check_eq(toDays(d), 364);

  d = new Date("1971-01-01T00:00:00Z");
  do_check_eq(toDays(d), 365);

  d = toDate(0);
  do_check_eq(d.getTime(), 0);

  d = toDate(1);
  do_check_eq(d.getTime(), MILLISECONDS_PER_DAY);

  d = toDate(365);
  do_check_eq(d.getUTCFullYear(), 1971);
  do_check_eq(d.getUTCMonth(), 0);
  do_check_eq(d.getUTCDate(), 1);
  do_check_eq(d.getUTCHours(), 0);
  do_check_eq(d.getUTCMinutes(), 0);
  do_check_eq(d.getUTCSeconds(), 0);
  do_check_eq(d.getUTCMilliseconds(), 0);

  run_next_test();
});

add_task(function test_get_sqlite_backend() {
  let backend = yield Metrics.Storage("get_sqlite_backend.sqlite");

  do_check_neq(backend._connection, null);

  // Ensure WAL and auto checkpoint are enabled.
  do_check_neq(backend._enabledWALCheckpointPages, null);
  let rows = yield backend._connection.execute("PRAGMA journal_mode");
  do_check_eq(rows[0].getResultByIndex(0), "wal");
  rows = yield backend._connection.execute("PRAGMA wal_autocheckpoint");
  do_check_eq(rows[0].getResultByIndex(0), backend._enabledWALCheckpointPages);

  yield backend.close();
  do_check_null(backend._connection);
});

add_task(function test_reconnect() {
  let backend = yield Metrics.Storage("reconnect");
  yield backend.close();

  let backend2 = yield Metrics.Storage("reconnect");
  yield backend2.close();
});

add_task(function test_future_schema_errors() {
  let backend = yield Metrics.Storage("future_schema_errors");
  yield backend._connection.setSchemaVersion(2);
  yield backend.close();

  let backend2;
  let failed = false;
  try {
    backend2 = yield Metrics.Storage("future_schema_errors");
  } catch (ex) {
    failed = true;
    do_check_true(ex.message.startsWith("Unknown database schema"));
  }

  do_check_null(backend2);
  do_check_true(failed);
});

add_task(function test_checkpoint_apis() {
  let backend = yield Metrics.Storage("checkpoint_apis");
  let c = backend._connection;
  let count = c._connectionData._statementCounter;

  yield backend.setAutoCheckpoint(0);
  do_check_eq(c._connectionData._statementCounter, count + 1);

  let rows = yield c.execute("PRAGMA wal_autocheckpoint");
  do_check_eq(rows[0].getResultByIndex(0), 0);
  count = c._connectionData._statementCounter;

  yield backend.setAutoCheckpoint(1);
  do_check_eq(c._connectionData._statementCounter, count + 1);

  rows = yield c.execute("PRAGMA wal_autocheckpoint");
  do_check_eq(rows[0].getResultByIndex(0), backend._enabledWALCheckpointPages);
  count = c._connectionData._statementCounter;

  yield backend.checkpoint();
  do_check_eq(c._connectionData._statementCounter, count + 1);

  yield backend.checkpoint();
  do_check_eq(c._connectionData._statementCounter, count + 2);

  yield backend.close();
});

add_task(function test_measurement_registration() {
  let backend = yield Metrics.Storage("measurement_registration");

  do_check_false(backend.hasProvider("foo"));
  do_check_false(backend.hasMeasurement("foo", "bar", 1));

  let id = yield backend.registerMeasurement("foo", "bar", 1);
  do_check_eq(id, 1);

  do_check_true(backend.hasProvider("foo"));
  do_check_true(backend.hasMeasurement("foo", "bar", 1));
  do_check_eq(backend.measurementID("foo", "bar", 1), id);
  do_check_false(backend.hasMeasurement("foo", "bar", 2));

  let id2 = yield backend.registerMeasurement("foo", "bar", 2);
  do_check_eq(id2, 2);
  do_check_true(backend.hasMeasurement("foo", "bar", 2));
  do_check_eq(backend.measurementID("foo", "bar", 2), id2);

  yield backend.close();
});

add_task(function test_field_registration_basic() {
  let backend = yield Metrics.Storage("field_registration_basic");

  do_check_false(backend.hasField("foo", "bar", 1, "baz"));

  let mID = yield backend.registerMeasurement("foo", "bar", 1);
  do_check_false(backend.hasField("foo", "bar", 1, "baz"));
  do_check_false(backend.hasFieldFromMeasurement(mID, "baz"));

  let bazID = yield backend.registerField(mID, "baz",
                                          backend.FIELD_DAILY_COUNTER);
  do_check_true(backend.hasField("foo", "bar", 1, "baz"));
  do_check_true(backend.hasFieldFromMeasurement(mID, "baz"));

  let bar2ID = yield backend.registerMeasurement("foo", "bar2", 1);

  yield backend.registerField(bar2ID, "baz",
                              backend.FIELD_DAILY_DISCRETE_NUMERIC);

  do_check_true(backend.hasField("foo", "bar2", 1, "baz"));

  yield backend.close();
});

// Ensure changes types of fields results in fatal error.
add_task(function test_field_registration_changed_type() {
  let backend = yield Metrics.Storage("field_registration_changed_type");

  let mID = yield backend.registerMeasurement("bar", "bar", 1);

  let id = yield backend.registerField(mID, "baz",
                                       backend.FIELD_DAILY_COUNTER);

  let caught = false;
  try {
    yield backend.registerField(mID, "baz",
                                backend.FIELD_DAILY_DISCRETE_NUMERIC);
  } catch (ex) {
    caught = true;
    do_check_true(ex.message.startsWith("Field already defined with different type"));
  }

  do_check_true(caught);

  yield backend.close();
});

add_task(function test_field_registration_repopulation() {
  let backend = yield Metrics.Storage("field_registration_repopulation");

  let mID1 = yield backend.registerMeasurement("foo", "bar", 1);
  let mID2 = yield backend.registerMeasurement("foo", "bar", 2);
  let mID3 = yield backend.registerMeasurement("foo", "biz", 1);
  let mID4 = yield backend.registerMeasurement("baz", "foo", 1);

  let fID1 = yield backend.registerField(mID1, "foo", backend.FIELD_DAILY_COUNTER);
  let fID2 = yield backend.registerField(mID1, "bar", backend.FIELD_DAILY_DISCRETE_NUMERIC);
  let fID3 = yield backend.registerField(mID4, "foo", backend.FIELD_LAST_TEXT);

  yield backend.close();

  backend = yield Metrics.Storage("field_registration_repopulation");

  do_check_true(backend.hasProvider("foo"));
  do_check_true(backend.hasProvider("baz"));
  do_check_true(backend.hasMeasurement("foo", "bar", 1));
  do_check_eq(backend.measurementID("foo", "bar", 1), mID1);
  do_check_true(backend.hasMeasurement("foo", "bar", 2));
  do_check_eq(backend.measurementID("foo", "bar", 2), mID2);
  do_check_true(backend.hasMeasurement("foo", "biz", 1));
  do_check_eq(backend.measurementID("foo", "biz", 1), mID3);
  do_check_true(backend.hasMeasurement("baz", "foo", 1));
  do_check_eq(backend.measurementID("baz", "foo", 1), mID4);

  do_check_true(backend.hasField("foo", "bar", 1, "foo"));
  do_check_eq(backend.fieldID("foo", "bar", 1, "foo"), fID1);
  do_check_true(backend.hasField("foo", "bar", 1, "bar"));
  do_check_eq(backend.fieldID("foo", "bar", 1, "bar"), fID2);
  do_check_true(backend.hasField("baz", "foo", 1, "foo"));
  do_check_eq(backend.fieldID("baz", "foo", 1, "foo"), fID3);

  yield backend.close();
});

add_task(function test_enqueue_operation_execution_order() {
  let backend = yield Metrics.Storage("enqueue_operation_execution_order");

  let executionCount = 0;

  let fns = {
    op1: function () {
      do_check_eq(executionCount, 1);
    },

    op2: function () {
      do_check_eq(executionCount, 2);
    },

    op3: function () {
      do_check_eq(executionCount, 3);
    },
  };

  function enqueuedOperation(fn) {
    let deferred = Promise.defer();

    CommonUtils.nextTick(function onNextTick() {
      executionCount++;
      fn();
      deferred.resolve();
    });

    return deferred.promise;
  }

  let promises = [];
  for (let i = 1; i <= 3; i++) {
    let fn = fns["op" + i];
    promises.push(backend.enqueueOperation(enqueuedOperation.bind(this, fn)));
  }

  for (let promise of promises) {
    yield promise;
  }

  yield backend.close();
});

add_task(function test_enqueue_operation_many() {
  let backend = yield Metrics.Storage("enqueue_operation_many");

  let promises = [];
  for (let i = 0; i < 100; i++) {
    promises.push(backend.registerMeasurement("foo", "bar" + i, 1));
  }

  for (let promise of promises) {
    yield promise;
  }

  yield backend.close();
});

// If the operation did not return a promise, everything should still execute.
add_task(function test_enqueue_operation_no_return_promise() {
  let backend = yield Metrics.Storage("enqueue_operation_no_return_promise");

  let mID = yield backend.registerMeasurement("foo", "bar", 1);
  let fID = yield backend.registerField(mID, "baz", backend.FIELD_DAILY_COUNTER);
  let now = new Date();

  let promises = [];
  for (let i = 0; i < 10; i++) {
    promises.push(backend.enqueueOperation(function op() {
      backend.incrementDailyCounterFromFieldID(fID, now);
    }));
  }

  let deferred = Promise.defer();

  let finished = 0;
  for (let promise of promises) {
    promise.then(
      do_throw.bind(this, "Unexpected resolve."),
      function onError() {
        finished++;

        if (finished == promises.length) {
          backend.getDailyCounterCountFromFieldID(fID, now).then(function onCount(count) {
            // There should not be a race condition here because storage
            // serializes all statements. So, for the getDailyCounterCount
            // query to finish means that all counter update statements must
            // have completed.
            do_check_eq(count, promises.length);
            deferred.resolve();
          });
        }
      }
    );
  }

  yield deferred.promise;
  yield backend.close();
});

// If an operation throws, subsequent operations should still execute.
add_task(function test_enqueue_operation_throw_exception() {
  let backend = yield Metrics.Storage("enqueue_operation_rejected_promise");

  let mID = yield backend.registerMeasurement("foo", "bar", 1);
  let fID = yield backend.registerField(mID, "baz", backend.FIELD_DAILY_COUNTER);
  let now = new Date();

  let deferred = Promise.defer();
  backend.enqueueOperation(function bad() {
    throw new Error("I failed.");
  }).then(do_throw, function onError(error) {
    do_check_true(error.message.includes("I failed."));
    deferred.resolve();
  });

  let promise = backend.enqueueOperation(function () {
    return backend.incrementDailyCounterFromFieldID(fID, now);
  });

  yield deferred.promise;
  yield promise;

  let count = yield backend.getDailyCounterCountFromFieldID(fID, now);
  do_check_eq(count, 1);
  yield backend.close();
});

// If an operation rejects, subsequent operations should still execute.
add_task(function test_enqueue_operation_reject_promise() {
  let backend = yield Metrics.Storage("enqueue_operation_reject_promise");

  let mID = yield backend.registerMeasurement("foo", "bar", 1);
  let fID = yield backend.registerField(mID, "baz", backend.FIELD_DAILY_COUNTER);
  let now = new Date();

  let deferred = Promise.defer();
  backend.enqueueOperation(function reject() {
    let d = Promise.defer();

    CommonUtils.nextTick(function nextTick() {
      d.reject("I failed.");
    });

    return d.promise;
  }).then(do_throw, function onError(error) {
    deferred.resolve();
  });

  let promise = backend.enqueueOperation(function () {
    return backend.incrementDailyCounterFromFieldID(fID, now);
  });

  yield deferred.promise;
  yield promise;

  let count = yield backend.getDailyCounterCountFromFieldID(fID, now);
  do_check_eq(count, 1);
  yield backend.close();
});

add_task(function test_enqueue_transaction() {
  let backend = yield Metrics.Storage("enqueue_transaction");

  let mID = yield backend.registerMeasurement("foo", "bar", 1);
  let fID = yield backend.registerField(mID, "baz", backend.FIELD_DAILY_COUNTER);
  let now = new Date();

  yield backend.incrementDailyCounterFromFieldID(fID, now);

  yield backend.enqueueTransaction(function transaction() {
    yield backend.incrementDailyCounterFromFieldID(fID, now);
  });

  let count = yield backend.getDailyCounterCountFromFieldID(fID, now);
  do_check_eq(count, 2);

  let errored = false;
  try {
    yield backend.enqueueTransaction(function aborted() {
      yield backend.incrementDailyCounterFromFieldID(fID, now);

      throw new Error("Some error.");
    });
  } catch (ex) {
    errored = true;
  } finally {
    do_check_true(errored);
  }

  count = yield backend.getDailyCounterCountFromFieldID(fID, now);
  do_check_eq(count, 2);

  yield backend.close();
});

add_task(function test_increment_daily_counter_basic() {
  let backend = yield Metrics.Storage("increment_daily_counter_basic");

  let mID = yield backend.registerMeasurement("foo", "bar", 1);

  let fieldID = yield backend.registerField(mID, "baz",
                                            backend.FIELD_DAILY_COUNTER);

  let now = new Date();
  yield backend.incrementDailyCounterFromFieldID(fieldID, now);

  let count = yield backend.getDailyCounterCountFromFieldID(fieldID, now);
  do_check_eq(count, 1);

  yield backend.incrementDailyCounterFromFieldID(fieldID, now);
  count = yield backend.getDailyCounterCountFromFieldID(fieldID, now);
  do_check_eq(count, 2);

  yield backend.incrementDailyCounterFromFieldID(fieldID, now, 10);
  count = yield backend.getDailyCounterCountFromFieldID(fieldID, now);
  do_check_eq(count, 12);

  yield backend.close();
});

add_task(function test_increment_daily_counter_multiple_days() {
  let backend = yield Metrics.Storage("increment_daily_counter_multiple_days");

  let mID = yield backend.registerMeasurement("foo", "bar", 1);
  let fieldID = yield backend.registerField(mID, "baz",
                                            backend.FIELD_DAILY_COUNTER);

  let days = [];
  let now = Date.now();
  for (let i = 0; i < 100; i++) {
    days.push(new Date(now - i * MILLISECONDS_PER_DAY));
  }

  for (let day of days) {
    yield backend.incrementDailyCounterFromFieldID(fieldID, day);
  }

  let result = yield backend.getDailyCounterCountsFromFieldID(fieldID);
  do_check_eq(result.size, 100);
  for (let day of days) {
    do_check_true(result.hasDay(day));
    do_check_eq(result.getDay(day), 1);
  }

  let fields = yield backend.getMeasurementDailyCountersFromMeasurementID(mID);
  do_check_eq(fields.size, 1);
  do_check_true(fields.has("baz"));
  do_check_eq(fields.get("baz").size, 100);

  for (let day of days) {
    do_check_true(fields.get("baz").hasDay(day));
    do_check_eq(fields.get("baz").getDay(day), 1);
  }

  yield backend.close();
});

add_task(function test_last_values() {
  let backend = yield Metrics.Storage("set_last");

  let mID = yield backend.registerMeasurement("foo", "bar", 1);
  let numberID = yield backend.registerField(mID, "number",
                                             backend.FIELD_LAST_NUMERIC);
  let textID = yield backend.registerField(mID, "text",
                                             backend.FIELD_LAST_TEXT);
  let now = new Date();
  let nowDay = new Date(Math.floor(now.getTime() / MILLISECONDS_PER_DAY) * MILLISECONDS_PER_DAY);

  yield backend.setLastNumericFromFieldID(numberID, 42, now);
  yield backend.setLastTextFromFieldID(textID, "hello world", now);

  let result = yield backend.getLastNumericFromFieldID(numberID);
  do_check_true(Array.isArray(result));
  do_check_eq(result[0].getTime(), nowDay.getTime());
  do_check_eq(typeof(result[1]), "number");
  do_check_eq(result[1], 42);

  result = yield backend.getLastTextFromFieldID(textID);
  do_check_true(Array.isArray(result));
  do_check_eq(result[0].getTime(), nowDay.getTime());
  do_check_eq(typeof(result[1]), "string");
  do_check_eq(result[1], "hello world");

  let missingID = yield backend.registerField(mID, "missing",
                                              backend.FIELD_LAST_NUMERIC);
  do_check_null(yield backend.getLastNumericFromFieldID(missingID));

  let fields = yield backend.getMeasurementLastValuesFromMeasurementID(mID);
  do_check_eq(fields.size, 2);
  do_check_true(fields.has("number"));
  do_check_true(fields.has("text"));
  do_check_eq(fields.get("number")[1], 42);
  do_check_eq(fields.get("text")[1], "hello world");

  yield backend.close();
});

add_task(function test_discrete_values_basic() {
  let backend = yield Metrics.Storage("discrete_values_basic");

  let mID = yield backend.registerMeasurement("foo", "bar", 1);
  let numericID = yield backend.registerField(mID, "numeric",
                                              backend.FIELD_DAILY_DISCRETE_NUMERIC);
  let textID = yield backend.registerField(mID, "text",
                                           backend.FIELD_DAILY_DISCRETE_TEXT);

  let now = new Date();
  let expectedNumeric = [];
  let expectedText = [];
  for (let i = 0; i < 100; i++) {
    expectedNumeric.push(i);
    expectedText.push("value" + i);
    yield backend.addDailyDiscreteNumericFromFieldID(numericID, i, now);
    yield backend.addDailyDiscreteTextFromFieldID(textID, "value" + i, now);
  }

  let values = yield backend.getDailyDiscreteNumericFromFieldID(numericID);
  do_check_eq(values.size, 1);
  do_check_true(values.hasDay(now));
  do_check_true(Array.isArray(values.getDay(now)));
  do_check_eq(values.getDay(now).length, expectedNumeric.length);

  for (let i = 0; i < expectedNumeric.length; i++) {
    do_check_eq(values.getDay(now)[i], expectedNumeric[i]);
  }

  values = yield backend.getDailyDiscreteTextFromFieldID(textID);
  do_check_eq(values.size, 1);
  do_check_true(values.hasDay(now));
  do_check_true(Array.isArray(values.getDay(now)));
  do_check_eq(values.getDay(now).length, expectedText.length);

  for (let i = 0; i < expectedText.length; i++) {
    do_check_eq(values.getDay(now)[i], expectedText[i]);
  }

  let fields = yield backend.getMeasurementDailyDiscreteValuesFromMeasurementID(mID);
  do_check_eq(fields.size, 2);
  do_check_true(fields.has("numeric"));
  do_check_true(fields.has("text"));

  let numeric = fields.get("numeric");
  let text = fields.get("text");
  do_check_true(numeric.hasDay(now));
  do_check_true(text.hasDay(now));
  do_check_eq(numeric.getDay(now).length, expectedNumeric.length);
  do_check_eq(text.getDay(now).length, expectedText.length);

  for (let i = 0; i < expectedNumeric.length; i++) {
    do_check_eq(numeric.getDay(now)[i], expectedNumeric[i]);
  }

  for (let i = 0; i < expectedText.length; i++) {
    do_check_eq(text.getDay(now)[i], expectedText[i]);
  }

  yield backend.close();
});

add_task(function test_discrete_values_multiple_days() {
  let backend = yield Metrics.Storage("discrete_values_multiple_days");

  let mID = yield backend.registerMeasurement("foo", "bar", 1);
  let id = yield backend.registerField(mID, "baz",
                                       backend.FIELD_DAILY_DISCRETE_NUMERIC);

  let now = new Date();
  let dates = [];
  for (let i = 0; i < 50; i++) {
    let date = new Date(now.getTime() + i * MILLISECONDS_PER_DAY);
    dates.push(date);

    yield backend.addDailyDiscreteNumericFromFieldID(id, i, date);
  }

  let values = yield backend.getDailyDiscreteNumericFromFieldID(id);
  do_check_eq(values.size, 50);

  let i = 0;
  for (let date of dates) {
    do_check_true(values.hasDay(date));
    do_check_eq(values.getDay(date)[0], i);
    i++;
  }

  let fields = yield backend.getMeasurementDailyDiscreteValuesFromMeasurementID(mID);
  do_check_eq(fields.size, 1);
  do_check_true(fields.has("baz"));
  let baz = fields.get("baz");
  do_check_eq(baz.size, 50);
  i = 0;
  for (let date of dates) {
    do_check_true(baz.hasDay(date));
    do_check_eq(baz.getDay(date).length, 1);
    do_check_eq(baz.getDay(date)[0], i);
    i++;
  }

  yield backend.close();
});

add_task(function test_daily_last_values() {
  let backend = yield Metrics.Storage("daily_last_values");

  let mID = yield backend.registerMeasurement("foo", "bar", 1);
  let numericID = yield backend.registerField(mID, "numeric",
                                              backend.FIELD_DAILY_LAST_NUMERIC);
  let textID = yield backend.registerField(mID, "text",
                                           backend.FIELD_DAILY_LAST_TEXT);

  let now = new Date();
  let yesterday = new Date(now.getTime() - MILLISECONDS_PER_DAY);
  let dayBefore = new Date(yesterday.getTime() - MILLISECONDS_PER_DAY);

  yield backend.setDailyLastNumericFromFieldID(numericID, 1, yesterday);
  yield backend.setDailyLastNumericFromFieldID(numericID, 2, now);
  yield backend.setDailyLastNumericFromFieldID(numericID, 3, dayBefore);
  yield backend.setDailyLastTextFromFieldID(textID, "foo", now);
  yield backend.setDailyLastTextFromFieldID(textID, "bar", yesterday);
  yield backend.setDailyLastTextFromFieldID(textID, "baz", dayBefore);

  let days = yield backend.getDailyLastNumericFromFieldID(numericID);
  do_check_eq(days.size, 3);
  do_check_eq(days.getDay(yesterday), 1);
  do_check_eq(days.getDay(now), 2);
  do_check_eq(days.getDay(dayBefore), 3);

  days = yield backend.getDailyLastTextFromFieldID(textID);
  do_check_eq(days.size, 3);
  do_check_eq(days.getDay(now), "foo");
  do_check_eq(days.getDay(yesterday), "bar");
  do_check_eq(days.getDay(dayBefore), "baz");

  yield backend.setDailyLastNumericFromFieldID(numericID, 4, yesterday);
  days = yield backend.getDailyLastNumericFromFieldID(numericID);
  do_check_eq(days.getDay(yesterday), 4);

  yield backend.setDailyLastTextFromFieldID(textID, "biz", yesterday);
  days = yield backend.getDailyLastTextFromFieldID(textID);
  do_check_eq(days.getDay(yesterday), "biz");

  days = yield backend.getDailyLastNumericFromFieldID(numericID, yesterday);
  do_check_eq(days.size, 1);
  do_check_eq(days.getDay(yesterday), 4);

  days = yield backend.getDailyLastTextFromFieldID(textID, yesterday);
  do_check_eq(days.size, 1);
  do_check_eq(days.getDay(yesterday), "biz");

  let fields = yield backend.getMeasurementDailyLastValuesFromMeasurementID(mID);
  do_check_eq(fields.size, 2);
  do_check_true(fields.has("numeric"));
  do_check_true(fields.has("text"));
  let numeric = fields.get("numeric");
  let text = fields.get("text");
  do_check_true(numeric.hasDay(yesterday));
  do_check_true(numeric.hasDay(dayBefore));
  do_check_true(numeric.hasDay(now));
  do_check_true(text.hasDay(yesterday));
  do_check_true(text.hasDay(dayBefore));
  do_check_true(text.hasDay(now));
  do_check_eq(numeric.getDay(yesterday), 4);
  do_check_eq(text.getDay(yesterday), "biz");

  yield backend.close();
});

add_task(function test_prune_data_before() {
  let backend = yield Metrics.Storage("prune_data_before");

  let mID = yield backend.registerMeasurement("foo", "bar", 1);

  let counterID = yield backend.registerField(mID, "baz",
                                              backend.FIELD_DAILY_COUNTER);
  let text1ID = yield backend.registerField(mID, "one_text_1",
                                            backend.FIELD_LAST_TEXT);
  let text2ID = yield backend.registerField(mID, "one_text_2",
                                            backend.FIELD_LAST_TEXT);
  let numeric1ID = yield backend.registerField(mID, "one_numeric_1",
                                               backend.FIELD_LAST_NUMERIC);
  let numeric2ID = yield backend.registerField(mID, "one_numeric_2",
                                                backend.FIELD_LAST_NUMERIC);
  let text3ID = yield backend.registerField(mID, "daily_last_text_1",
                                            backend.FIELD_DAILY_LAST_TEXT);
  let text4ID = yield backend.registerField(mID, "daily_last_text_2",
                                            backend.FIELD_DAILY_LAST_TEXT);
  let numeric3ID = yield backend.registerField(mID, "daily_last_numeric_1",
                                               backend.FIELD_DAILY_LAST_NUMERIC);
  let numeric4ID = yield backend.registerField(mID, "daily_last_numeric_2",
                                               backend.FIELD_DAILY_LAST_NUMERIC);

  let now = new Date();
  let yesterday = new Date(now.getTime() - MILLISECONDS_PER_DAY);
  let dayBefore = new Date(yesterday.getTime() - MILLISECONDS_PER_DAY);

  yield backend.incrementDailyCounterFromFieldID(counterID, now);
  yield backend.incrementDailyCounterFromFieldID(counterID, yesterday);
  yield backend.incrementDailyCounterFromFieldID(counterID, dayBefore);
  yield backend.setLastTextFromFieldID(text1ID, "hello", dayBefore);
  yield backend.setLastTextFromFieldID(text2ID, "world", yesterday);
  yield backend.setLastNumericFromFieldID(numeric1ID, 42, dayBefore);
  yield backend.setLastNumericFromFieldID(numeric2ID, 43, yesterday);
  yield backend.setDailyLastTextFromFieldID(text3ID, "foo", dayBefore);
  yield backend.setDailyLastTextFromFieldID(text3ID, "bar", yesterday);
  yield backend.setDailyLastTextFromFieldID(text4ID, "hello", dayBefore);
  yield backend.setDailyLastTextFromFieldID(text4ID, "world", yesterday);
  yield backend.setDailyLastNumericFromFieldID(numeric3ID, 40, dayBefore);
  yield backend.setDailyLastNumericFromFieldID(numeric3ID, 41, yesterday);
  yield backend.setDailyLastNumericFromFieldID(numeric4ID, 42, dayBefore);
  yield backend.setDailyLastNumericFromFieldID(numeric4ID, 43, yesterday);

  let days = yield backend.getDailyCounterCountsFromFieldID(counterID);
  do_check_eq(days.size, 3);

  yield backend.pruneDataBefore(yesterday);
  days = yield backend.getDailyCounterCountsFromFieldID(counterID);
  do_check_eq(days.size, 2);
  do_check_false(days.hasDay(dayBefore));

  do_check_null(yield backend.getLastTextFromFieldID(text1ID));
  do_check_null(yield backend.getLastNumericFromFieldID(numeric1ID));

  let result = yield backend.getLastTextFromFieldID(text2ID);
  do_check_true(Array.isArray(result));
  do_check_eq(result[1], "world");

  result = yield backend.getLastNumericFromFieldID(numeric2ID);
  do_check_true(Array.isArray(result));
  do_check_eq(result[1], 43);

  result = yield backend.getDailyLastNumericFromFieldID(numeric3ID);
  do_check_eq(result.size, 1);
  do_check_true(result.hasDay(yesterday));

  result = yield backend.getDailyLastTextFromFieldID(text3ID);
  do_check_eq(result.size, 1);
  do_check_true(result.hasDay(yesterday));

  yield backend.close();
});

add_task(function test_provider_state() {
  let backend = yield Metrics.Storage("provider_state");

  yield backend.registerMeasurement("foo", "bar", 1);
  yield backend.setProviderState("foo", "apple", "orange");
  let value = yield backend.getProviderState("foo", "apple");
  do_check_eq(value, "orange");

  yield backend.setProviderState("foo", "apple", "pear");
  value = yield backend.getProviderState("foo", "apple");
  do_check_eq(value, "pear");

  yield backend.close();
});

add_task(function test_get_measurement_values() {
  let backend = yield Metrics.Storage("get_measurement_values");

  let mID = yield backend.registerMeasurement("foo", "bar", 1);
  let id1 = yield backend.registerField(mID, "id1", backend.FIELD_DAILY_COUNTER);
  let id2 = yield backend.registerField(mID, "id2", backend.FIELD_DAILY_DISCRETE_NUMERIC);
  let id3 = yield backend.registerField(mID, "id3", backend.FIELD_DAILY_DISCRETE_TEXT);
  let id4 = yield backend.registerField(mID, "id4", backend.FIELD_DAILY_LAST_NUMERIC);
  let id5 = yield backend.registerField(mID, "id5", backend.FIELD_DAILY_LAST_TEXT);
  let id6 = yield backend.registerField(mID, "id6", backend.FIELD_LAST_NUMERIC);
  let id7 = yield backend.registerField(mID, "id7", backend.FIELD_LAST_TEXT);

  let now = new Date();
  let yesterday = new Date(now.getTime() - MILLISECONDS_PER_DAY);

  yield backend.incrementDailyCounterFromFieldID(id1, now);
  yield backend.addDailyDiscreteNumericFromFieldID(id2, 3, now);
  yield backend.addDailyDiscreteNumericFromFieldID(id2, 4, now);
  yield backend.addDailyDiscreteNumericFromFieldID(id2, 5, yesterday);
  yield backend.addDailyDiscreteNumericFromFieldID(id2, 6, yesterday);
  yield backend.addDailyDiscreteTextFromFieldID(id3, "1", now);
  yield backend.addDailyDiscreteTextFromFieldID(id3, "2", now);
  yield backend.addDailyDiscreteTextFromFieldID(id3, "3", yesterday);
  yield backend.addDailyDiscreteTextFromFieldID(id3, "4", yesterday);
  yield backend.setDailyLastNumericFromFieldID(id4, 1, now);
  yield backend.setDailyLastNumericFromFieldID(id4, 2, yesterday);
  yield backend.setDailyLastTextFromFieldID(id5, "foo", now);
  yield backend.setDailyLastTextFromFieldID(id5, "bar", yesterday);
  yield backend.setLastNumericFromFieldID(id6, 42, now);
  yield backend.setLastTextFromFieldID(id7, "foo", now);

  let fields = yield backend.getMeasurementValues(mID);
  do_check_eq(Object.keys(fields).length, 2);
  do_check_true("days" in fields);
  do_check_true("singular" in fields);
  do_check_eq(fields.days.size, 2);
  do_check_true(fields.days.hasDay(now));
  do_check_true(fields.days.hasDay(yesterday));
  do_check_eq(fields.days.getDay(now).size, 5);
  do_check_eq(fields.days.getDay(yesterday).size, 4);
  do_check_eq(fields.days.getDay(now).get("id3")[0], 1);
  do_check_eq(fields.days.getDay(yesterday).get("id4"), 2);
  do_check_eq(fields.singular.size, 2);
  do_check_eq(fields.singular.get("id6")[1], 42);
  do_check_eq(fields.singular.get("id7")[1], "foo");

  yield backend.close();
});

