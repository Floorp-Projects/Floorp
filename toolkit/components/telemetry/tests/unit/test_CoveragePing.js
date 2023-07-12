/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

const { HttpServer } = ChromeUtils.importESModule(
  "resource://testing-common/httpd.sys.mjs"
);

const COVERAGE_VERSION = "2";

const COVERAGE_ENABLED_PREF = "toolkit.coverage.enabled";
const OPT_OUT_PREF = "toolkit.coverage.opt-out";
const ALREADY_RUN_PREF = `toolkit.coverage.already-run.v${COVERAGE_VERSION}`;
const COVERAGE_UUID_PREF = `toolkit.coverage.uuid.v${COVERAGE_VERSION}`;
const TELEMETRY_ENABLED_PREF = "datareporting.healthreport.uploadEnabled";
const REPORTING_ENDPOINT_BASE_PREF = "toolkit.coverage.endpoint.base";
const REPORTING_ENDPOINT = "submit/coverage/coverage";

Services.prefs.setIntPref("toolkit.coverage.log-level", 20);

add_task(async function setup() {
  let uuid = "test123";
  Services.prefs.setCharPref(COVERAGE_UUID_PREF, uuid);

  const server = new HttpServer();
  server.start(-1);
  const serverPort = server.identity.primaryPort;

  Services.prefs.setCharPref(
    REPORTING_ENDPOINT_BASE_PREF,
    `http://localhost:${serverPort}`
  );

  server.registerPathHandler(
    `/${REPORTING_ENDPOINT}/${COVERAGE_VERSION}/${uuid}`,
    (request, response) => {
      equal(request.method, "PUT");
      let telemetryEnabled = Services.prefs.getBoolPref(
        TELEMETRY_ENABLED_PREF,
        false
      );

      let requestBody = NetUtil.readInputStreamToString(
        request.bodyInputStream,
        request.bodyInputStream.available()
      );

      let resultObj = JSON.parse(requestBody);

      deepEqual(Object.keys(resultObj), [
        "appUpdateChannel",
        "osName",
        "osVersion",
        "telemetryEnabled",
      ]);

      if (telemetryEnabled) {
        ok(resultObj.telemetryEnabled);
      } else {
        ok(!resultObj.telemetryEnabled);
      }

      const response_body = "OK";
      response.bodyOutputStream.write(response_body, response_body.length);
      server.stop();
    }
  );

  // Trigger a proper telemetry init.
  do_get_profile(true);
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  await TelemetryController.testSetup();
});

add_task(async function test_prefs() {
  // Telemetry reporting setting does not control this ping, but it
  // reported by this ping.
  Services.prefs.setBoolPref(TELEMETRY_ENABLED_PREF, false);

  // should not run if enabled pref is false
  Services.prefs.setBoolPref(COVERAGE_ENABLED_PREF, false);
  Services.prefs.setBoolPref(ALREADY_RUN_PREF, false);
  Services.prefs.setBoolPref(OPT_OUT_PREF, false);

  await TelemetryController.testReset();

  let alreadyRun = Services.prefs.getBoolPref(ALREADY_RUN_PREF, false);
  ok(!alreadyRun, "should not have run with enabled pref false");

  // should not run if opt-out pref is true
  Services.prefs.setBoolPref(COVERAGE_ENABLED_PREF, true);
  Services.prefs.setBoolPref(ALREADY_RUN_PREF, false);
  Services.prefs.setBoolPref(OPT_OUT_PREF, true);

  await TelemetryController.testReset();

  // should run if opt-out pref is false and coverage is enabled
  Services.prefs.setBoolPref(COVERAGE_ENABLED_PREF, true);
  Services.prefs.setBoolPref(ALREADY_RUN_PREF, false);
  Services.prefs.setBoolPref(OPT_OUT_PREF, false);

  await TelemetryController.testReset();

  // the telemetry setting should be set correctly
  Services.prefs.setBoolPref(TELEMETRY_ENABLED_PREF, true);

  await TelemetryController.testReset();

  alreadyRun = Services.prefs.getBoolPref(ALREADY_RUN_PREF, false);

  ok(alreadyRun, "should run if no opt-out and enabled");
});
