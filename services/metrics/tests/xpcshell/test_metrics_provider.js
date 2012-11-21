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
  let provider = new MetricsProvider("foo");

  let failed = false;
  try {
    new MetricsProvider();
  } catch(ex) {
    do_check_true(ex.message.startsWith("MetricsProvider must have a name"));
    failed = true;
  }
  finally {
    do_check_true(failed);
  }

  run_next_test();
});

add_test(function test_default_collectors() {
  let provider = new MetricsProvider("foo");

  for (let property in MetricsProvider.prototype) {
    if (!property.startsWith("collect")) {
      continue;
    }

    let result = provider[property]();
    do_check_null(result);
  }

  run_next_test();
});

add_test(function test_collect_constant_synchronous() {
  let provider = new DummyProvider();

  let result = provider.collectConstantMeasurements();
  do_check_true(result instanceof MetricsCollectionResult);
  result.populate(result);

  result.onFinished(function onResult(res2) {
    do_check_eq(result, res2);

    let m = result.measurements.get("DummyMeasurement");
    do_check_eq(m.getValue("uint32"), 24);

    run_next_test();
  });
});

