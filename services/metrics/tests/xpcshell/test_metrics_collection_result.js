/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {utils: Cu} = Components;

Cu.import("resource://gre/modules/services/metrics/dataprovider.jsm");
Cu.import("resource://testing-common/services/metrics/mocks.jsm");


function run_test() {
  run_next_test();
};

add_test(function test_constructor() {
  let result = new MetricsCollectionResult("foo");
  do_check_eq(result.name, "foo");

  let failed = false;
  try {
    new MetricsCollectionResult();
  } catch (ex) {
    do_check_true(ex.message.startsWith("Must provide name argument to Metrics"));
    failed = true;
  } finally {
    do_check_true(failed);
    failed = false;
  }

  try {
    result.populate();
  } catch (ex) {
    do_check_true(ex.message.startsWith("populate() must be defined"));
    failed = true;
  } finally {
    do_check_true(failed);
    failed = false;
  }

  run_next_test();
});

add_test(function test_expected_measurements() {
  let result = new MetricsCollectionResult("foo");
  do_check_eq(result.missingMeasurements.size0);

  result.expectMeasurement("foo");
  result.expectMeasurement("bar");
  do_check_eq(result.missingMeasurements.size, 2);
  do_check_true(result.missingMeasurements.has("foo"));
  do_check_true(result.missingMeasurements.has("bar"));

  run_next_test();
});

add_test(function test_missing_measurements() {
  let result = new MetricsCollectionResult("foo");

  let missing = result.missingMeasurements;
  do_check_eq(missing.size, 0);

  result.expectMeasurement("DummyMeasurement");
  result.expectMeasurement("b");

  missing = result.missingMeasurements;
  do_check_eq(missing.size, 2);
  do_check_true(missing.has("DummyMeasurement"));
  do_check_true(missing.has("b"));

  result.addMeasurement(new DummyMeasurement());
  missing = result.missingMeasurements;
  do_check_eq(missing.size, 1);
  do_check_true(missing.has("b"));

  run_next_test();
});

add_test(function test_add_measurement() {
  let result = new MetricsCollectionResult("add_measurement");

  let failed = false;
  try {
    result.addMeasurement(new DummyMeasurement());
  } catch (ex) {
    do_check_true(ex.message.startsWith("Not expecting this measurement"));
    failed = true;
  } finally {
    do_check_true(failed);
    failed = false;
  }

  result.expectMeasurement("foo");
  result.addMeasurement(new DummyMeasurement("foo"));

  do_check_eq(result.measurements.size, 1);
  do_check_true(result.measurements.has("foo"));

  run_next_test();
});

add_test(function test_set_value() {
  let result = new MetricsCollectionResult("set_value");
  result.expectMeasurement("DummyMeasurement");
  result.addMeasurement(new DummyMeasurement());

  do_check_true(result.setValue("DummyMeasurement", "string", "hello world"));

  let failed = false;
  try {
    result.setValue("unknown", "irrelevant", "irrelevant");
  } catch (ex) {
    do_check_true(ex.message.startsWith("Attempting to operate on an undefined measurement"));
    failed = true;
  } finally {
    do_check_true(failed);
    failed = false;
  }

  do_check_eq(result.errors.length, 0);
  do_check_false(result.setValue("DummyMeasurement", "string", 42));
  do_check_eq(result.errors.length, 1);

  try {
    result.setValue("DummyMeasurement", "string", 42, true);
  } catch (ex) {
    failed = true;
  } finally {
    do_check_true(failed);
    failed = false;
  }

  run_next_test();
});

add_test(function test_aggregate_bad_argument() {
  let result = new MetricsCollectionResult("bad_argument");

  let failed = false;
  try {
    result.aggregate(null);
  } catch (ex) {
    do_check_true(ex.message.startsWith("aggregate expects a MetricsCollection"));
    failed = true;
  } finally {
    do_check_true(failed);
    failed = false;
  }

  try {
    let result2 = new MetricsCollectionResult("bad_argument2");
    result.aggregate(result2);
  } catch (ex) {
    do_check_true(ex.message.startsWith("Can only aggregate"));
    failed = true;
  } finally {
    do_check_true(failed);
    failed = false;
  }

  run_next_test();
});

add_test(function test_aggregate_side_effects() {
  let result1 = new MetricsCollectionResult("aggregate");
  let result2 = new MetricsCollectionResult("aggregate");

  result1.expectMeasurement("dummy1");
  result1.expectMeasurement("foo");

  result2.expectMeasurement("dummy2");
  result2.expectMeasurement("bar");

  result1.addMeasurement(new DummyMeasurement("dummy1"));
  result1.setValue("dummy1", "invalid", "invalid");

  result2.addMeasurement(new DummyMeasurement("dummy2"));
  result2.setValue("dummy2", "another", "invalid");

  result1.aggregate(result2);

  do_check_eq(result1.expectedMeasurements.size, 4);
  do_check_true(result1.expectedMeasurements.has("bar"));

  do_check_eq(result1.measurements.size, 2);
  do_check_true(result1.measurements.has("dummy1"));
  do_check_true(result1.measurements.has("dummy2"));

  do_check_eq(result1.missingMeasurements.size, 2);
  do_check_true(result1.missingMeasurements.has("bar"));

  do_check_eq(result1.errors.length, 2);

  run_next_test();
});

add_test(function test_json() {
  let result = new MetricsCollectionResult("json");
  result.expectMeasurement("dummy1");
  result.expectMeasurement("dummy2");
  result.expectMeasurement("missing1");
  result.expectMeasurement("missing2");

  result.addMeasurement(new DummyMeasurement("dummy1"));
  result.addMeasurement(new DummyMeasurement("dummy2"));

  result.setValue("dummy1", "string", "hello world");
  result.setValue("dummy2", "uint32", 42);
  result.setValue("dummy1", "invalid", "irrelevant");

  let json = JSON.parse(JSON.stringify(result));

  do_check_eq(Object.keys(json).length, 3);
  do_check_true("measurements" in json);
  do_check_true("missing" in json);
  do_check_true("errors" in json);

  do_check_eq(Object.keys(json.measurements).length, 2);
  do_check_true("dummy1" in json.measurements);
  do_check_true("dummy2" in json.measurements);

  do_check_eq(json.missing.length, 2);
  let missing = new Set(json.missing);
  do_check_true(missing.has("missing1"));
  do_check_true(missing.has("missing2"));

  do_check_eq(json.errors.length, 1);

  run_next_test();
});

add_test(function test_finish() {
  let result = new MetricsCollectionResult("finish");

  result.onFinished(function onFinished(result2) {
    do_check_eq(result, result2);

    run_next_test();
  });

  result.finish();
});

