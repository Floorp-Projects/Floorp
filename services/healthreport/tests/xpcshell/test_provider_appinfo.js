/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");
Cu.import("resource://gre/modules/services/metrics/dataprovider.jsm");

function run_test() {
  let appInfo = {
    vendor: "Mozilla",
    name: "xpcshell",
    ID: "xpcshell@tests.mozilla.org",
    version: "1",
    appBuildID: "20121107",
    platformVersion: "p-ver",
    platformBuildID: "20121106",
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo, Ci.nsIXULRuntime]),
    invalidateCachesOnRestart: function() {},
  };

  let factory = {
    createInstance: function createInstance(outer, iid) {
      if (outer != null) {
        throw Cr.NS_ERROR_NO_AGGREGATION;
      }

      return appInfo.QueryInterface(iid);
    },
  };

  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(Components.ID("{fbfae60b-64a4-44ef-a911-08ceb70b9f31}"),
                            "XULAppInfo", "@mozilla.org/xre/app-info;1",
                            factory);

  run_next_test();
}

add_test(function test_constructor() {
  let provider = new AppInfoProvider();

  run_next_test();
});

add_test(function test_collect_smoketest() {
  let provider = new AppInfoProvider();

  let result = provider.collectConstantMeasurements();
  do_check_true(result instanceof MetricsCollectionResult);

  result.onFinished(function onFinished() {
    do_check_eq(result.expectedMeasurements.size, 1);
    do_check_true(result.expectedMeasurements.has("appinfo"));
    do_check_eq(result.measurements.size, 1);
    do_check_true(result.measurements.has("appinfo"));
    do_check_eq(result.errors.length, 0);

    let ai = result.measurements.get("appinfo");
    do_check_eq(ai.getValue("vendor"), "Mozilla");
    do_check_eq(ai.getValue("name"), "xpcshell");
    do_check_eq(ai.getValue("id"), "xpcshell@tests.mozilla.org");
    do_check_eq(ai.getValue("version"), "1");
    do_check_eq(ai.getValue("appBuildID"), "20121107");
    do_check_eq(ai.getValue("platformVersion"), "p-ver");
    do_check_eq(ai.getValue("platformBuildID"), "20121106");
    do_check_eq(ai.getValue("os"), "XPCShell");
    do_check_eq(ai.getValue("xpcomabi"), "noarch-spidermonkey");

    run_next_test();
  });

  result.populate(result);
});
