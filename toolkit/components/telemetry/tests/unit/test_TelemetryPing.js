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

const PATH = "/submit/telemetry/test-ping"
const BinaryInputStream = Components.Constructor(
  "@mozilla.org/binaryinputstream;1",
  "nsIBinaryInputStream",
  "setInputStream");

var httpserver = new nsHttpServer();

function telemetry_ping () {
  let tp = Cc["@mozilla.org/base/telemetry-ping;1"].getService(Ci.nsIObserver);
  tp.observe(tp, "test-ping", "http://localhost:4444");
}

function telemetryObserver(aSubject, aTopic, aData) {
  Services.obs.removeObserver(telemetryObserver, aTopic);
  httpserver.registerPathHandler(PATH, checkHistograms);
  telemetry_ping();
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");
  httpserver.start(4444);

  Services.obs.addObserver(telemetryObserver, "telemetry-test-xhr-complete", false);

  telemetry_ping();
  // spin the event loop
  do_test_pending();
}

function readBytesFromInputStream(inputStream, count) {
  if (!count) {
    count = inputStream.available();
  }
  return new BinaryInputStream(inputStream).readBytes(count);
}

function checkHistograms(request, response) {
  // do not need the http server anymore
  httpserver.stop(do_test_finished);
  let s = request.bodyInputStream
  let payload = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON)
                                             .decode(readBytesFromInputStream(s))

  do_check_true(payload.info.uptime >= 0)

  // get rid of the non-deterministic field
  payload.info.uptime = 0;
  const expected_info = {
    uptime: 0,
    reason: "test-ping",
    OS: "XPCShell", 
    XPCOMABI: "noarch-spidermonkey", 
    ID: "xpcshell@tests.mozilla.org", 
    version: "1", 
    name: "XPCShell", 
    appBuildID: "2007010101",
    platformBuildID: "2007010101"
  };

  do_check_eq(uneval(payload.info), 
              uneval(expected_info));

  const TELEMETRY_PING = "telemetry.ping (ms)";
  const TELEMETRY_SUCCESS = "telemetry.success (No, Yes)";
  do_check_true(TELEMETRY_PING in payload.histograms)

  // There should be one successful report from the previos telemetry ping
  const expected_tc = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 1,
    values: {1:0, 2:1}
  }
  let tc = payload.histograms[TELEMETRY_SUCCESS]
  do_check_eq(uneval(tc), 
              uneval(expected_tc));
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
