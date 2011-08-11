/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ 
*/
/* This testcase triggers two telemetry pings.
 * 
 * Telemetry code keeps histograms of past telemetry pings. The first
 * ping populates these histograms. One of those histograms is then
 * checked in the second request.
 */

do_load_httpd_js();
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/LightweightThemeManager.jsm");

const PATH = "/submit/telemetry/test-ping";
const SERVER = "http://localhost:4444";
const IGNORE_HISTOGRAM = "test::ignore_me";

const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream");

var httpserver = new nsHttpServer();
var gFinished = false;

function telemetry_ping () {
  const TelemetryPing = Cc["@mozilla.org/base/telemetry-ping;1"].getService(Ci.nsIObserver);
  TelemetryPing.observe(null, "test-ping", SERVER);
}

function nonexistentServerObserver(aSubject, aTopic, aData) {
  Services.obs.removeObserver(nonexistentServerObserver, aTopic);

  httpserver.start(4444);

  // Provide a dummy function so it returns 200 instead of 404 to telemetry.
  httpserver.registerPathHandler(PATH, function () {});
  Services.obs.addObserver(telemetryObserver, "telemetry-test-xhr-complete", false);
  telemetry_ping();
}

function telemetryObserver(aSubject, aTopic, aData) {
  Services.obs.removeObserver(telemetryObserver, aTopic);
  httpserver.registerPathHandler(PATH, checkHistograms);
  const Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);
  Telemetry.newHistogram(IGNORE_HISTOGRAM, 1, 2, 3, Telemetry.HISTOGRAM_BOOLEAN);
  telemetry_ping();
}

function checkHistograms(request, response) {
  // do not need the http server anymore
  httpserver.stop(do_test_finished);
  let s = request.bodyInputStream
  let payload = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON)
                                             .decodeFromStream(s, s.available())

  do_check_eq(request.getHeader("content-type"), "application/json; charset=UTF-8");
  do_check_true(payload.simpleMeasurements.uptime >= 0)

  // get rid of the non-deterministic field
  const expected_info = {
    reason: "test-ping",
    OS: "XPCShell", 
    appID: "xpcshell@tests.mozilla.org", 
    appVersion: "1", 
    appName: "XPCShell", 
    appBuildID: "2007010101",
    platformBuildID: "2007010101"
  };

  for (let f in expected_info) {
    do_check_eq(payload.info[f], expected_info[f]);
  }

  const TELEMETRY_PING = "TELEMETRY_PING";
  const TELEMETRY_SUCCESS = "TELEMETRY_SUCCESS";
  do_check_true(TELEMETRY_PING in payload.histograms);
  do_check_false(IGNORE_HISTOGRAM in payload.histograms);

  // There should be one successful report from the previous telemetry ping.
  const expected_tc = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 2,
    values: {0:1, 1:1, 2:0},
    sum: 1
  }
  let tc = payload.histograms[TELEMETRY_SUCCESS]
  do_check_eq(uneval(tc), 
              uneval(expected_tc));
  gFinished = true;
}

// copied from toolkit/mozapps/extensions/test/xpcshell/head_addons.js
const XULAPPINFO_CONTRACTID = "@mozilla.org/xre/app-info;1";
const XULAPPINFO_CID = Components.ID("{c763b610-9d49-455a-bbd2-ede71682a1ac}");

function createAppInfo(id, name, version, platformVersion) {
  gAppInfo = {
    // nsIXULAppInfo
    vendor: "Mozilla",
    name: name,
    ID: id,
    version: version,
    appBuildID: "2007010101",
    platformVersion: platformVersion,
    platformBuildID: "2007010101",

    // nsIXULRuntime
    inSafeMode: false,
    logConsoleErrors: true,
    OS: "XPCShell",
    XPCOMABI: "noarch-spidermonkey",
    invalidateCachesOnRestart: function invalidateCachesOnRestart() {
      // Do nothing
    },

    // nsICrashReporter
    annotations: {},

    annotateCrashReport: function(key, data) {
      this.annotations[key] = data;
    },

    QueryInterface: XPCOMUtils.generateQI([Ci.nsIXULAppInfo,
                                           Ci.nsIXULRuntime,
                                           Ci.nsICrashReporter,
                                           Ci.nsISupports])
  };

  var XULAppInfoFactory = {
    createInstance: function (outer, iid) {
      if (outer != null)
        throw Components.results.NS_ERROR_NO_AGGREGATION;
      return gAppInfo.QueryInterface(iid);
    }
  };
  var registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(XULAPPINFO_CID, "XULAppInfo",
                            XULAPPINFO_CONTRACTID, XULAppInfoFactory);
}

function dummyTheme(id) {
  return {
    id: id,
    name: Math.random().toString(),
    headerURL: "http://lwttest.invalid/a.png",
    footerURL: "http://lwttest.invalid/b.png",
    textcolor: Math.random().toString(),
    accentcolor: Math.random().toString()
  };
}

function run_test() {
  // Addon manager needs a profile directory
  do_get_profile();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  // try to make LightweightThemeManager do stuff
  let gInternalManager = Cc["@mozilla.org/addons/integration;1"]
                         .getService(Ci.nsIObserver)
                         .QueryInterface(Ci.nsITimerCallback);

  gInternalManager.observe(null, "addons-startup", null);
  LightweightThemeManager.currentTheme = dummyTheme("1234");

  Services.obs.addObserver(nonexistentServerObserver, "telemetry-test-xhr-complete", false);
  telemetry_ping();
  // spin the event loop
  do_test_pending();
  // ensure that test runs to completion
  do_register_cleanup(function () do_check_true(gFinished))
 }
