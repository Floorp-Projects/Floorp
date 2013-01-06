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
  let provider = new AppInfoProvider();

  run_next_test();
});

add_task(function test_collect_smoketest() {
  let storage = yield Metrics.Storage("collect_smoketest");
  let provider = new AppInfoProvider();
  yield provider.init(storage);

  yield provider.collectConstantData();

  let m = provider.getMeasurement("appinfo", 1);
  let data = yield storage.getMeasurementValues(m.id);
  let serializer = m.serializer(m.SERIALIZE_JSON);
  let d = serializer.singular(data.singular);

  do_check_eq(d.vendor, "Mozilla");
  do_check_eq(d.name, "xpcshell");
  do_check_eq(d.id, "xpcshell@tests.mozilla.org");
  do_check_eq(d.version, "1");
  do_check_eq(d.appBuildID, "20121107");
  do_check_eq(d.platformVersion, "p-ver");
  do_check_eq(d.platformBuildID, "20121106");
  do_check_eq(d.os, "XPCShell");
  do_check_eq(d.xpcomabi, "noarch-spidermonkey");

  yield storage.close();
});

