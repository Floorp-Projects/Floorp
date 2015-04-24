/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ 
*/
/* This testcase triggers two telemetry pings.
 *
 * Telemetry code keeps histograms of past telemetry pings. The first
 * ping populates these histograms. One of those histograms is then
 * checked in the second request.
 */

Cu.import("resource://testing-common/httpd.js", this);
Cu.import("resource://gre/modules/ClientID.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/TelemetryPing.jsm", this);
Cu.import("resource://gre/modules/TelemetryStorage.jsm", this);
Cu.import("resource://gre/modules/TelemetryArchive.jsm", this);
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
const PREF_ARCHIVE_ENABLED = PREF_BRANCH + "archive.enabled";
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
  Services.prefs.setBoolPref(PREF_FHR_SERVICE_ENABLED, true);

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
  yield TelemetryStorage.savePing(ping, true);
  yield TelemetryStorage.savePing(ping, false);
  yield TelemetryStorage.cleanupPingFile(ping);
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

add_task(function* test_archivePings() {
  const ARCHIVE_PATH =
    OS.Path.join(OS.Constants.Path.profileDir, "datareporting", "archived");

  let now = new Date(2009, 10, 18, 12, 0, 0);
  fakeNow(now);

  // Disable FHR upload so that pings don't get sent.
  Preferences.set(PREF_FHR_UPLOAD_ENABLED, false);

  // Register a new Ping Handler that asserts if a ping is received, then send a ping.
  registerPingHandler(() => Assert.ok(false, "Telemetry must not send pings if not allowed to."));
  let pingId = yield sendPing(true, true);

  // Check that the ping was archived, even with upload disabled.
  let ping = yield TelemetryArchive.promiseArchivedPingById(pingId);
  Assert.equal(ping.id, pingId, "TelemetryPing must archive pings if FHR is enabled.");

  // Check that pings don't get archived if not allowed to.
  now = new Date(2010, 10, 18, 12, 0, 0);
  fakeNow(now);
  Preferences.set(PREF_ARCHIVE_ENABLED, false);
  pingId = yield sendPing(true, true);
  let promise = TelemetryArchive.promiseArchivedPingById(pingId);
  Assert.ok((yield promiseRejects(promise)),
            "TelemetryPing must not archive pings if the archive pref is disabled.");

  // Enable archiving and the upload so that pings get sent and archived again.
  Preferences.set(PREF_FHR_UPLOAD_ENABLED, true);
  Preferences.set(PREF_ARCHIVE_ENABLED, true);

  now = new Date(2014, 06, 18, 22, 0, 0);
  fakeNow(now);
  // Restore the non asserting ping handler. This is done by the Request() constructor.
  gRequestIterator = Iterator(new Request());
  pingId = yield sendPing(true, true);

  // Check that we archive pings when successfully sending them.
  yield gRequestIterator.next();
  ping = yield TelemetryArchive.promiseArchivedPingById(pingId);
  Assert.equal(ping.id, pingId, "TelemetryPing must archive pings if FHR is enabled.");
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
