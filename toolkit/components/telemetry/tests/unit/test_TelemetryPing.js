/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ 
*/
/* This testcase triggers two telemetry pings.
 *
 * Telemetry code keeps histograms of past telemetry pings. The first
 * ping populates these histograms. One of those histograms is then
 * checked in the second request.
 */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

Cu.import("resource://testing-common/httpd.js", this);
Cu.import("resource://gre/modules/ClientID.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/TelemetryPing.jsm", this);
Cu.import("resource://gre/modules/TelemetryFile.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm");

const PING_FORMAT_VERSION = 4;
const TEST_PING_TYPE = "test-ping-type";
const TEST_PING_RETENTION = 180;

const PLATFORM_VERSION = "1.9.2";
const APP_VERSION = "1";
const APP_NAME = "XPCShell";

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_ENABLED = PREF_BRANCH + "enabled";
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";
const PREF_FHR_SERVICE_ENABLED = "datareporting.healthreport.service.enabled";

const Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);

let gHttpServer = new HttpServer();
let gServerStarted = false;
let gRequestIterator = null;
let gClientID = null;

function sendPing(aSendClientId, aSendEnvironment) {
  if (gServerStarted) {
    TelemetryPing.setServer("http://localhost:" + gHttpServer.identity.primaryPort);
  } else {
    TelemetryPing.setServer("http://doesnotexist");
  }

  let options = {
    addClientId: aSendClientId,
    addEnvironment: aSendEnvironment,
    retentionDays: TEST_PING_RETENTION,
  };
  return TelemetryPing.send(TEST_PING_TYPE, {}, options);
}

function wrapWithExceptionHandler(f) {
  function wrapper(...args) {
    try {
      f(...args);
    } catch (ex if typeof(ex) == 'object') {
      dump("Caught exception: " + ex.message + "\n");
      dump(ex.stack);
      do_test_finished();
    }
  }
  return wrapper;
}

function registerPingHandler(handler) {
  gHttpServer.registerPrefixHandler("/submit/telemetry/",
				   wrapWithExceptionHandler(handler));
}

function checkPingFormat(aPing, aType, aHasClientId, aHasEnvironment) {
  const MANDATORY_PING_FIELDS = [
    "type", "id", "creationDate", "version", "application", "payload"
  ];

  const APPLICATION_TEST_DATA = {
    buildId: "2007010101",
    name: APP_NAME,
    version: APP_VERSION,
    vendor: "Mozilla",
    platformVersion: PLATFORM_VERSION,
    xpcomAbi: "noarch-spidermonkey",
  };

  // Check that the ping contains all the mandatory fields.
  for (let f of MANDATORY_PING_FIELDS) {
    Assert.ok(f in aPing, f + " must be available.");
  }

  Assert.equal(aPing.type, aType, "The ping must have the correct type.");
  Assert.equal(aPing.version, PING_FORMAT_VERSION, "The ping must have the correct version.");

  // Test the application section.
  for (let f in APPLICATION_TEST_DATA) {
    Assert.equal(aPing.application[f], APPLICATION_TEST_DATA[f],
                 f + " must have the correct value.");
  }

  // We can't check the values for channel and architecture. Just make
  // sure they are in.
  Assert.ok("architecture" in aPing.application,
            "The application section must have an architecture field.");
  Assert.ok("channel" in aPing.application,
            "The application section must have a channel field.");

  // Check the clientId and environment fields, as needed.
  Assert.equal("clientId" in aPing, aHasClientId);
  Assert.equal("environment" in aPing, aHasEnvironment);
}

/**
 * Start the webserver used in the tests.
 */
function startWebserver() {
  gHttpServer.start(-1);
  gServerStarted = true;
  gRequestIterator = Iterator(new Request());
}

function run_test() {
  do_test_pending();

  // Addon manager needs a profile directory
  do_get_profile();
  loadAddonManager("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  Services.prefs.setBoolPref(PREF_ENABLED, true);
  Services.prefs.setBoolPref(PREF_FHR_UPLOAD_ENABLED, true);

  Telemetry.asyncFetchTelemetryData(wrapWithExceptionHandler(run_next_test));
}

add_task(function* asyncSetup() {
  yield TelemetryPing.setup();

  gClientID = yield ClientID.getClientID();

  // We should have cached the client id now. Lets confirm that by
  // checking the client id before the async ping setup is finished.
  let promisePingSetup = TelemetryPing.reset();
  do_check_eq(TelemetryPing.clientID, gClientID);
  yield promisePingSetup;
});

// Ensure that not overwriting an existing file fails silently
add_task(function* test_overwritePing() {
  let ping = {id: "foo"}
  yield TelemetryFile.savePing(ping, true);
  yield TelemetryFile.savePing(ping, false);
  yield TelemetryFile.cleanupPingFile(ping);
});

// Sends a ping to a non existing server.
add_task(function* test_noServerPing() {
  yield sendPing(false, false);
});

// Checks that a sent ping is correctly received by a dummy http server.
add_task(function* test_simplePing() {
  startWebserver();

  yield sendPing(false, false);
  let request = yield gRequestIterator.next();

  // Check that we have a version query parameter in the URL.
  Assert.notEqual(request.queryString, "");

  // Make sure the version in the query string matches the new ping format version.
  let params = request.queryString.split("&");
  Assert.ok(params.find(p => p == ("v=" + PING_FORMAT_VERSION)));

  let ping = decodeRequestPayload(request);
  checkPingFormat(ping, TEST_PING_TYPE, false, false);
});

add_task(function* test_pingHasClientId() {
  // Send a ping with a clientId.
  yield sendPing(true, false);

  let request = yield gRequestIterator.next();
  let ping = decodeRequestPayload(request);
  checkPingFormat(ping, TEST_PING_TYPE, true, false);

  if (HAS_DATAREPORTINGSERVICE &&
      Services.prefs.getBoolPref(PREF_FHR_UPLOAD_ENABLED)) {
    Assert.equal(ping.clientId, gClientID,
                 "The correct clientId must be reported.");
  }
});

add_task(function* test_pingHasEnvironment() {
  // Send a ping with the environment data.
  yield sendPing(false, true);
  let request = yield gRequestIterator.next();
  let ping = decodeRequestPayload(request);
  checkPingFormat(ping, TEST_PING_TYPE, false, true);

  // Test a field in the environment build section.
  Assert.equal(ping.application.buildId, ping.environment.build.buildId);
});

add_task(function* test_pingHasEnvironmentAndClientId() {
  // Send a ping with the environment data and client id.
  yield sendPing(true, true);
  let request = yield gRequestIterator.next();
  let ping = decodeRequestPayload(request);
  checkPingFormat(ping, TEST_PING_TYPE, true, true);

  // Test a field in the environment build section.
  Assert.equal(ping.application.buildId, ping.environment.build.buildId);
  // Test that we have the correct clientId.
  if (HAS_DATAREPORTINGSERVICE &&
      Services.prefs.getBoolPref(PREF_FHR_UPLOAD_ENABLED)) {
    Assert.equal(ping.clientId, gClientID,
                 "The correct clientId must be reported.");
  }
});

add_task(function* stopServer(){
  gHttpServer.stop(do_test_finished);
});

// An iterable sequence of http requests
function Request() {
  let defers = [];
  let current = 0;

  function RequestIterator() {}

  // Returns a promise that resolves to the next http request
  RequestIterator.prototype.next = function() {
    let deferred = defers[current++];
    return deferred.promise;
  }

  this.__iterator__ = function(){
    return new RequestIterator();
  }

  registerPingHandler((request, response) => {
    let deferred = defers[defers.length - 1];
    defers.push(Promise.defer());
    deferred.resolve(request);
  });

  defers.push(Promise.defer());
}
