/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {interfaces: Ci, results: Cr, utils: Cu} = Components;

Cu.import("resource://gre/modules/Metrics.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/services/healthreport/providers.jsm");
Cu.import("resource://testing-common/services/healthreport/utils.jsm");


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

  let now = new Date();
  yield provider.collectConstantData();

  let m = provider.getMeasurement("appinfo", 2);
  let data = yield storage.getMeasurementValues(m.id);
  let serializer = m.serializer(m.SERIALIZE_JSON);
  let d = serializer.singular(data.singular);

  do_check_eq(d._v, 2);
  do_check_eq(d.vendor, "Mozilla");
  do_check_eq(d.name, "xpcshell");
  do_check_eq(d.id, "xpcshell@tests.mozilla.org");
  do_check_eq(d.version, "1");
  do_check_eq(d.appBuildID, "20121107");
  do_check_eq(d.platformVersion, "p-ver");
  do_check_eq(d.platformBuildID, "20121106");
  do_check_eq(d.os, "XPCShell");
  do_check_eq(d.xpcomabi, "noarch-spidermonkey");

  do_check_eq(data.days.size, 1);
  do_check_true(data.days.hasDay(now));
  let day = data.days.getDay(now);
  do_check_eq(day.size, 3);
  do_check_true(day.has("isDefaultBrowser"));
  do_check_true(day.has("isTelemetryEnabled"));
  do_check_true(day.has("isBlocklistEnabled"));

  // TODO Bug 827189 Actually test this properly. On some local builds, this
  // is always -1 (the service throws). On buildbot, it seems to always be 0.
  do_check_neq(day.get("isDefaultBrowser"), 1);

  yield provider.shutdown();
  yield storage.close();
});

add_task(function test_record_version() {
  let storage = yield Metrics.Storage("record_version");

  let provider = new AppInfoProvider();
  let now = new Date();
  yield provider.init(storage);

  // The provider records information on startup.
  let m = provider.getMeasurement("versions", 2);
  let data = yield m.getValues();

  do_check_true(data.days.hasDay(now));
  let day = data.days.getDay(now);
  do_check_eq(day.size, 4);
  do_check_true(day.has("appVersion"));
  do_check_true(day.has("platformVersion"));
  do_check_true(day.has("appBuildID"));
  do_check_true(day.has("platformBuildID"));

  let value = day.get("appVersion");
  do_check_true(Array.isArray(value));
  do_check_eq(value.length, 1);
  let ai = getAppInfo();
  do_check_eq(value[0], ai.version);

  value = day.get("platformVersion");
  do_check_true(Array.isArray(value));
  do_check_eq(value.length, 1);
  do_check_eq(value[0], ai.platformVersion);

  value = day.get("appBuildID");
  do_check_true(Array.isArray(value));
  do_check_eq(value.length, 1);
  do_check_eq(value[0], ai.appBuildID);

  value = day.get("platformBuildID");
  do_check_true(Array.isArray(value));
  do_check_eq(value.length, 1);
  do_check_eq(value[0], ai.platformBuildID);

  yield provider.shutdown();
  yield storage.close();
});

add_task(function test_record_version_change() {
  let storage = yield Metrics.Storage("record_version_change");

  let provider = new AppInfoProvider();
  let now = new Date();
  yield provider.init(storage);
  yield provider.shutdown();

  let ai = getAppInfo();
  ai.version = "new app version";
  ai.platformVersion = "new platform version";
  ai.appBuildID = "new app id";
  ai.platformBuildID = "new platform id";
  updateAppInfo(ai);

  provider = new AppInfoProvider();
  yield provider.init(storage);

  // There should be 2 records in the versions history.
  let m = provider.getMeasurement("versions", 2);
  let data = yield m.getValues();
  do_check_true(data.days.hasDay(now));
  let day = data.days.getDay(now);

  let value = day.get("appVersion");
  do_check_true(Array.isArray(value));
  do_check_eq(value.length, 2);
  do_check_eq(value[1], "new app version");

  value = day.get("platformVersion");
  do_check_true(Array.isArray(value));
  do_check_eq(value.length, 2);
  do_check_eq(value[1], "new platform version");

  // There should be 2 records in the buildID history.
  value = day.get("appBuildID");
  do_check_true(Array.isArray(value));
  do_check_eq(value.length, 2);
  do_check_eq(value[1], "new app id");

  value = day.get("platformBuildID");
  do_check_true(Array.isArray(value));
  do_check_eq(value.length, 2);
  do_check_eq(value[1], "new platform id");

  yield provider.shutdown();
  yield storage.close();
});

add_task(function test_record_telemetry() {
  let storage = yield Metrics.Storage("record_telemetry");
  let provider;

  // We set both prefs, so we don't have to fight the preprocessor.
  function setTelemetry(bool) {
    Services.prefs.setBoolPref("toolkit.telemetry.enabled", bool);
    Services.prefs.setBoolPref("toolkit.telemetry.enabledPreRelease", bool);
  }

  let now = new Date();

  setTelemetry(true);
  provider = new AppInfoProvider();
  yield provider.init(storage);
  yield provider.collectConstantData();

  let m = provider.getMeasurement("appinfo", 2);
  let data = yield m.getValues();
  let d = yield m.serializer(m.SERIALIZE_JSON).daily(data.days.getDay(now));
  do_check_eq(1, d.isTelemetryEnabled);
  yield provider.shutdown();

  setTelemetry(false);
  provider = new AppInfoProvider();
  yield provider.init(storage);
  yield provider.collectConstantData();

  m = provider.getMeasurement("appinfo", 2);
  data = yield m.getValues();
  d = yield m.serializer(m.SERIALIZE_JSON).daily(data.days.getDay(now));
  do_check_eq(0, d.isTelemetryEnabled);
  yield provider.shutdown();

  yield storage.close();
});

add_task(function test_record_blocklist() {
  let storage = yield Metrics.Storage("record_blocklist");

  let now = new Date();

  Services.prefs.setBoolPref("extensions.blocklist.enabled", true);
  let provider = new AppInfoProvider();
  yield provider.init(storage);
  yield provider.collectConstantData();

  let m = provider.getMeasurement("appinfo", 2);
  let data = yield m.getValues();
  let d = yield m.serializer(m.SERIALIZE_JSON).daily(data.days.getDay(now));
  do_check_eq(d.isBlocklistEnabled, 1);
  yield provider.shutdown();

  Services.prefs.setBoolPref("extensions.blocklist.enabled", false);
  provider = new AppInfoProvider();
  yield provider.init(storage);
  yield provider.collectConstantData();

  m = provider.getMeasurement("appinfo", 2);
  data = yield m.getValues();
  d = yield m.serializer(m.SERIALIZE_JSON).daily(data.days.getDay(now));
  do_check_eq(d.isBlocklistEnabled, 0);
  yield provider.shutdown();

  yield storage.close();
});

add_task(function test_record_app_update () {
  let storage = yield Metrics.Storage("record_update");

  Services.prefs.setBoolPref("app.update.enabled", true);
  Services.prefs.setBoolPref("app.update.auto", true);
  let provider = new AppInfoProvider();
  yield provider.init(storage);
  let now = new Date();
  yield provider.collectDailyData();

  let m = provider.getMeasurement("update", 1);
  let data = yield m.getValues();
  let d = yield m.serializer(m.SERIALIZE_JSON).daily(data.days.getDay(now));
  do_check_eq(d.enabled, 1);
  do_check_eq(d.autoDownload, 1);

  Services.prefs.setBoolPref("app.update.enabled", false);
  Services.prefs.setBoolPref("app.update.auto", false);

  yield provider.collectDailyData();
  data = yield m.getValues();
  d = yield m.serializer(m.SERIALIZE_JSON).daily(data.days.getDay(now));
  do_check_eq(d.enabled, 0);
  do_check_eq(d.autoDownload, 0);

  yield provider.shutdown();
  yield storage.close();
});

add_task(function test_healthreporter_integration () {
  let reporter = getHealthReporter("healthreporter_integration");
  yield reporter.init();

  try {
    yield reporter._providerManager.registerProviderFromType(AppInfoProvider);
    yield reporter.collectMeasurements();

    let payload = yield reporter.getJSONPayload(true);
    let days = payload['data']['days'];

    for (let [day, measurements] in Iterator(days)) {
      do_check_eq(Object.keys(measurements).length, 3);
      do_check_true("org.mozilla.appInfo.appinfo" in measurements);
      do_check_true("org.mozilla.appInfo.update" in measurements);
      do_check_true("org.mozilla.appInfo.versions" in measurements);
    }
  } finally {
    yield reporter._shutdown();
  }
});
