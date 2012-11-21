/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");
Cu.import("resource://gre/modules/services/metrics/dataprovider.jsm");


function run_test() {
  run_next_test();
}

add_test(function test_constructor() {
  let provider = new SysInfoProvider();

  run_next_test();
});

add_test(function test_collect_smoketest() {
  let provider = new SysInfoProvider();

  let result = provider.collectConstantMeasurements();
  do_check_true(result instanceof MetricsCollectionResult);

  result.onFinished(function onFinished() {
    do_check_eq(result.expectedMeasurements.size, 1);
    do_check_true(result.expectedMeasurements.has("sysinfo"));
    do_check_eq(result.measurements.size, 1);
    do_check_true(result.measurements.has("sysinfo"));
    do_check_eq(result.errors.length, 0);

    let si = result.measurements.get("sysinfo");
    do_check_true(si.getValue("cpuCount") > 0);
    do_check_neq(si.getValue("name"), null);

    run_next_test();
  });

  result.populate(result);
});

