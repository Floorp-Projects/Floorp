/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");


function run_test() {
  run_next_test();
}

add_test(function test_constructor() {
  let provider = new SysInfoProvider();

  run_next_test();
});

add_task(function test_collect_smoketest() {
  let storage = yield Metrics.Storage("collect_smoketest");
  let provider = new SysInfoProvider();
  yield provider.init(storage);

  yield provider.collectConstantData();

  let m = provider.getMeasurement("sysinfo", 2);
  let data = yield storage.getMeasurementValues(m.id);
  let serializer = m.serializer(m.SERIALIZE_JSON);
  let d = serializer.singular(data.singular);

  do_check_eq(d._v, 2);
  do_check_true(d.cpuCount > 0);
  do_check_neq(d.name, null);

  yield storage.close();
});

