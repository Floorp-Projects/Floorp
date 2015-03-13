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
Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/LightweightThemeManager.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/TelemetryPing.jsm", this);
Cu.import("resource://gre/modules/TelemetrySession.jsm", this);
Cu.import("resource://gre/modules/TelemetryFile.jsm", this);
Cu.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/osfile.jsm", this);

const PING_FORMAT_VERSION = 4;
const PING_TYPE_MAIN = "main";
const PING_TYPE_SAVED_SESSION = "saved-session";

const REASON_SAVED_SESSION = "saved-session";
const REASON_SHUTDOWN = "shutdown";
const REASON_TEST_PING = "test-ping";
const REASON_DAILY = "daily";
const REASON_ENVIRONMENT_CHANGE = "environment-change";

const PLATFORM_VERSION = "1.9.2";
const APP_VERSION = "1";
const APP_ID = "xpcshell@tests.mozilla.org";
const APP_NAME = "XPCShell";

const IGNORE_HISTOGRAM = "test::ignore_me";
const IGNORE_HISTOGRAM_TO_CLONE = "MEMORY_HEAP_ALLOCATED";
const IGNORE_CLONED_HISTOGRAM = "test::ignore_me_also";
const ADDON_NAME = "Telemetry test addon";
const ADDON_HISTOGRAM = "addon-histogram";
// Add some unicode characters here to ensure that sending them works correctly.
const SHUTDOWN_TIME = 10000;
const FAILED_PROFILE_LOCK_ATTEMPTS = 2;

// Constants from prio.h for nsIFileOutputStream.init
const PR_WRONLY = 0x2;
const PR_CREATE_FILE = 0x8;
const PR_TRUNCATE = 0x20;
const RW_OWNER = parseInt("0600", 8);

const NUMBER_OF_THREADS_TO_LAUNCH = 30;
let gNumberOfThreadsLaunched = 0;

const SEC_IN_ONE_DAY  = 24 * 60 * 60;
const MS_IN_ONE_DAY   = SEC_IN_ONE_DAY * 1000;

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_ENABLED = PREF_BRANCH + "enabled";
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";
const PREF_FHR_SERVICE_ENABLED = "datareporting.healthreport.service.enabled";

const HAS_DATAREPORTINGSERVICE = "@mozilla.org/datareporting/service;1" in Cc;
const SESSION_RECORDER_EXPECTED = HAS_DATAREPORTINGSERVICE &&
                                  Preferences.get(PREF_FHR_SERVICE_ENABLED, true);

const Telemetry = Cc["@mozilla.org/base/telemetry;1"].getService(Ci.nsITelemetry);

let gHttpServer = new HttpServer();
let gServerStarted = false;
let gRequestIterator = null;
let gDataReportingClientID = null;

XPCOMUtils.defineLazyGetter(this, "gDatareportingService",
  () => Cc["@mozilla.org/datareporting/service;1"]
          .getService(Ci.nsISupports)
          .wrappedJSObject);

function generateUUID() {
  let str = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  // strip {}
  return str.substring(1, str.length - 1);
}

function sendPing() {
  TelemetrySession.gatherStartup();
  if (gServerStarted) {
    TelemetryPing.setServer("http://localhost:" + gHttpServer.identity.primaryPort);
    return TelemetrySession.testPing();
  } else {
    TelemetryPing.setServer("http://doesnotexist");
    return TelemetrySession.testPing();
  }
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

function futureDate(date, offset) {
  return new Date(date.getTime() + offset);
}

function fakeNow(date) {
  let session = Cu.import("resource://gre/modules/TelemetrySession.jsm");
  session.Policy.now = () => date;
}

function fakeGenerateUUID(sessionFunc, subsessionFunc) {
  let session = Cu.import("resource://gre/modules/TelemetrySession.jsm");
  session.Policy.generateSessionUUID = sessionFunc;
  session.Policy.generateSubsessionUUID = subsessionFunc;
}

function registerPingHandler(handler) {
  gHttpServer.registerPrefixHandler("/submit/telemetry/",
				   wrapWithExceptionHandler(handler));
}

function setupTestData() {
  Telemetry.newHistogram(IGNORE_HISTOGRAM, "never", Telemetry.HISTOGRAM_BOOLEAN);
  Telemetry.histogramFrom(IGNORE_CLONED_HISTOGRAM, IGNORE_HISTOGRAM_TO_CLONE);
  Services.startup.interrupted = true;
  Telemetry.registerAddonHistogram(ADDON_NAME, ADDON_HISTOGRAM,
                                   Telemetry.HISTOGRAM_LINEAR,
                                   1, 5, 6);
  let h1 = Telemetry.getAddonHistogram(ADDON_NAME, ADDON_HISTOGRAM);
  h1.add(1);
  let h2 = Telemetry.getHistogramById("TELEMETRY_TEST_COUNT");
  h2.add();

  let k1 = Telemetry.getKeyedHistogramById("TELEMETRY_TEST_KEYED_COUNT");
  k1.add("a");
  k1.add("a");
  k1.add("b");
}

function getSavedPingFile(basename) {
  let tmpDir = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let pingFile = tmpDir.clone();
  pingFile.append(basename);
  if (pingFile.exists()) {
    pingFile.remove(true);
  }
  do_register_cleanup(function () {
    try {
      pingFile.remove(true);
    } catch (e) {
    }
  });
  return pingFile;
}

function decodeRequestPayload(request) {
  let s = request.bodyInputStream;
  let payload = null;
  let decoder = Cc["@mozilla.org/dom/json;1"].createInstance(Ci.nsIJSON)

  if (request.getHeader("content-encoding") == "gzip") {
    let observer = {
      buffer: "",
      onStreamComplete: function(loader, context, status, length, result) {
        this.buffer = String.fromCharCode.apply(this, result);
      }
    };

    let scs = Cc["@mozilla.org/streamConverters;1"]
              .getService(Ci.nsIStreamConverterService);
    let listener = Cc["@mozilla.org/network/stream-loader;1"]
                  .createInstance(Ci.nsIStreamLoader);
    listener.init(observer);
    let converter = scs.asyncConvertData("gzip", "uncompressed",
                                         listener, null);
    converter.onStartRequest(null, null);
    converter.onDataAvailable(null, null, s, 0, s.available());
    converter.onStopRequest(null, null, null);
    let unicodeConverter = Cc["@mozilla.org/intl/scriptableunicodeconverter"]
                    .createInstance(Ci.nsIScriptableUnicodeConverter);
    unicodeConverter.charset = "UTF-8";
    let utf8string = unicodeConverter.ConvertToUnicode(observer.buffer);
    utf8string += unicodeConverter.Finish();
    payload = decoder.decode(utf8string);
  } else {
    payload = decoder.decodeFromStream(s, s.available());
  }

  return payload;
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
    Assert.ok(f in aPing, f + "must be available.");
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

function checkPayloadInfo(data) {
  const ALLOWED_REASONS = [
    "environment-change", "shutdown", "daily", "saved-session", "test-ping"
  ];
  const uuidRegex = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
  let numberCheck = arg => { return (typeof arg == "number"); };
  let positiveNumberCheck = arg => { return numberCheck(arg) && (arg >= 0); };
  let stringCheck = arg => { return (typeof arg == "string") && (arg != ""); };
  let isoDateCheck = arg => { return stringCheck(arg) && !Number.isNaN(Date.parse(arg)); }
  let revisionCheck = arg => {
    return (Services.appinfo.isOfficial) ? stringCheck(arg) : (typeof arg == "string");
  };
  let uuidCheck = arg => uuidRegex.test(arg);

  const EXPECTED_INFO_FIELDS_TYPES = {
    reason: stringCheck,
    revision: revisionCheck,
    timezoneOffset: numberCheck,
    sessionId: uuidCheck,
    subsessionId: uuidCheck,
    // Special case: previousSubsessionId is null on first run.
    previousSubsessionId: (arg) => { return (arg) ? uuidCheck(arg) : true; },
    subsessionCounter: positiveNumberCheck,
    profileSubsessionCounter: positiveNumberCheck,
    sessionStartDate: isoDateCheck,
    subsessionStartDate: isoDateCheck,
    subsessionLength: positiveNumberCheck,
  };

  for (let f in EXPECTED_INFO_FIELDS_TYPES) {
    Assert.ok(f in data, f + " must be available.");

    let checkFunc = EXPECTED_INFO_FIELDS_TYPES[f];
    Assert.ok(checkFunc(data[f]),
              f + " must have the correct type and valid data " + data[f]);
  }

  // Previous buildId is not mandatory.
  if (data.previousBuildId) {
    Assert.ok(stringCheck(data.previousBuildId));
  }

  Assert.ok(ALLOWED_REASONS.find(r => r == data.reason),
            "Payload must contain an allowed reason.");

  Assert.ok(Date.parse(data.subsessionStartDate) >= Date.parse(data.sessionStartDate));
  Assert.ok(data.profileSubsessionCounter >= data.subsessionCounter);
  Assert.ok(data.timezoneOffset >= -12*60, "The timezone must be in a valid range.");
  Assert.ok(data.timezoneOffset <= 12*60, "The timezone must be in a valid range.");
}

function checkPayload(payload, reason, successfulPings) {
  Assert.ok("info" in payload, "Payload must contain an info section.");
  checkPayloadInfo(payload.info);

  Assert.ok(payload.simpleMeasurements.totalTime >= 0);
  Assert.ok(payload.simpleMeasurements.uptime >= 0);
  Assert.equal(payload.simpleMeasurements.startupInterrupted, 1);
  Assert.equal(payload.simpleMeasurements.shutdownDuration, SHUTDOWN_TIME);
  Assert.equal(payload.simpleMeasurements.savedPings, 1);
  Assert.ok("maximalNumberOfConcurrentThreads" in payload.simpleMeasurements);
  Assert.ok(payload.simpleMeasurements.maximalNumberOfConcurrentThreads >= gNumberOfThreadsLaunched);

  let activeTicks = payload.simpleMeasurements.activeTicks;
  Assert.ok(SESSION_RECORDER_EXPECTED ? activeTicks >= 0 : activeTicks == -1);

  Assert.equal(payload.simpleMeasurements.failedProfileLockCount,
              FAILED_PROFILE_LOCK_ATTEMPTS);
  let profileDirectory = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let failedProfileLocksFile = profileDirectory.clone();
  failedProfileLocksFile.append("Telemetry.FailedProfileLocks.txt");
  Assert.ok(!failedProfileLocksFile.exists());


  let isWindows = ("@mozilla.org/windows-registry-key;1" in Components.classes);
  if (isWindows) {
    Assert.ok(payload.simpleMeasurements.startupSessionRestoreReadBytes > 0);
    Assert.ok(payload.simpleMeasurements.startupSessionRestoreWriteBytes > 0);
  }

  const TELEMETRY_PING = "TELEMETRY_PING";
  const TELEMETRY_SUCCESS = "TELEMETRY_SUCCESS";
  const TELEMETRY_TEST_FLAG = "TELEMETRY_TEST_FLAG";
  const TELEMETRY_TEST_COUNT = "TELEMETRY_TEST_COUNT";
  const TELEMETRY_TEST_KEYED_FLAG = "TELEMETRY_TEST_KEYED_FLAG";
  const TELEMETRY_TEST_KEYED_COUNT = "TELEMETRY_TEST_KEYED_COUNT";
  const READ_SAVED_PING_SUCCESS = "READ_SAVED_PING_SUCCESS";

  Assert.ok(TELEMETRY_PING in payload.histograms);
  Assert.ok(READ_SAVED_PING_SUCCESS in payload.histograms);
  Assert.ok(TELEMETRY_TEST_FLAG in payload.histograms);
  Assert.ok(TELEMETRY_TEST_COUNT in payload.histograms);

  let rh = Telemetry.registeredHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, []);
  for (let name of rh) {
    if (/SQLITE/.test(name) && name in payload.histograms) {
      let histogramName = ("STARTUP_" + name);
      Assert.ok(histogramName in payload.histograms, histogramName + " must be available.");
    }
  }
  Assert.ok(!(IGNORE_HISTOGRAM in payload.histograms));
  Assert.ok(!(IGNORE_CLONED_HISTOGRAM in payload.histograms));

  // Flag histograms should automagically spring to life.
  const expected_flag = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 3,
    values: {0:1, 1:0},
    sum: 0,
    sum_squares_lo: 0,
    sum_squares_hi: 0
  };
  let flag = payload.histograms[TELEMETRY_TEST_FLAG];
  Assert.equal(uneval(flag), uneval(expected_flag));

  // We should have a test count.
  const expected_count = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 4,
    values: {0:1, 1:0},
    sum: 1,
    sum_squares_lo: 1,
    sum_squares_hi: 0,
  };
  let count = payload.histograms[TELEMETRY_TEST_COUNT];
  Assert.equal(uneval(count), uneval(expected_count));

  // There should be one successful report from the previous telemetry ping.
  const expected_tc = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 2,
    values: {0:2, 1:successfulPings, 2:0},
    sum: successfulPings,
    sum_squares_lo: successfulPings,
    sum_squares_hi: 0
  };
  let tc = payload.histograms[TELEMETRY_SUCCESS];
  Assert.equal(uneval(tc), uneval(expected_tc));

  let h = payload.histograms[READ_SAVED_PING_SUCCESS];
  Assert.equal(h.values[0], 1);

  // The ping should include data from memory reporters.  We can't check that
  // this data is correct, because we can't control the values returned by the
  // memory reporters.  But we can at least check that the data is there.
  //
  // It's important to check for the presence of reporters with a mix of units,
  // because TelemetryPing has separate logic for each one.  But we can't
  // currently check UNITS_COUNT_CUMULATIVE or UNITS_PERCENTAGE because
  // Telemetry doesn't touch a memory reporter with these units that's
  // available on all platforms.

  Assert.ok('MEMORY_JS_GC_HEAP' in payload.histograms); // UNITS_BYTES
  Assert.ok('MEMORY_JS_COMPARTMENTS_SYSTEM' in payload.histograms); // UNITS_COUNT

  // We should have included addon histograms.
  Assert.ok("addonHistograms" in payload);
  Assert.ok(ADDON_NAME in payload.addonHistograms);
  Assert.ok(ADDON_HISTOGRAM in payload.addonHistograms[ADDON_NAME]);

  Assert.ok(("mainThread" in payload.slowSQL) &&
                ("otherThreads" in payload.slowSQL));

  // Check keyed histogram payload.

  Assert.ok("keyedHistograms" in payload);
  let keyedHistograms = payload.keyedHistograms;
  Assert.ok(TELEMETRY_TEST_KEYED_FLAG in keyedHistograms);
  Assert.ok(TELEMETRY_TEST_KEYED_COUNT in keyedHistograms);

  Assert.deepEqual({}, keyedHistograms[TELEMETRY_TEST_KEYED_FLAG]);

  const expected_keyed_count = {
    "a": {
      range: [1, 2],
      bucket_count: 3,
      histogram_type: 4,
      values: {0:2, 1:0},
      sum: 2,
      sum_squares_lo: 2,
      sum_squares_hi: 0,
    },
    "b": {
      range: [1, 2],
      bucket_count: 3,
      histogram_type: 4,
      values: {0:1, 1:0},
      sum: 1,
      sum_squares_lo: 1,
      sum_squares_hi: 0,
    },
  };
  Assert.deepEqual(expected_keyed_count, keyedHistograms[TELEMETRY_TEST_KEYED_COUNT]);
}

function writeStringToFile(file, contents) {
  let ostream = Cc["@mozilla.org/network/safe-file-output-stream;1"]
                .createInstance(Ci.nsIFileOutputStream);
  ostream.init(file, PR_WRONLY | PR_CREATE_FILE | PR_TRUNCATE,
	       RW_OWNER, ostream.DEFER_OPEN);
  ostream.write(contents, contents.length);
  ostream.QueryInterface(Ci.nsISafeOutputStream).finish();
  ostream.close();
}

function write_fake_shutdown_file() {
  let profileDirectory = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let file = profileDirectory.clone();
  file.append("Telemetry.ShutdownTime.txt");
  let contents = "" + SHUTDOWN_TIME;
  writeStringToFile(file, contents);
}

function write_fake_failedprofilelocks_file() {
  let profileDirectory = Services.dirsvc.get("ProfD", Ci.nsIFile);
  let file = profileDirectory.clone();
  file.append("Telemetry.FailedProfileLocks.txt");
  let contents = "" + FAILED_PROFILE_LOCK_ATTEMPTS;
  writeStringToFile(file, contents);
}

function run_test() {
  do_test_pending();

  // Addon manager needs a profile directory
  do_get_profile();
  loadAddonManager(APP_ID, APP_NAME, APP_VERSION, PLATFORM_VERSION);

  Services.prefs.setBoolPref(PREF_ENABLED, true);
  Services.prefs.setBoolPref(PREF_FHR_UPLOAD_ENABLED, true);

  // Send the needed startup notifications to the datareporting service
  // to ensure that it has been initialized.
  if (HAS_DATAREPORTINGSERVICE) {
    gDatareportingService.observe(null, "app-startup", null);
    gDatareportingService.observe(null, "profile-after-change", null);
  }

  // Make it look like we've previously failed to lock a profile a couple times.
  write_fake_failedprofilelocks_file();

  // Make it look like we've shutdown before.
  write_fake_shutdown_file();

  let currentMaxNumberOfThreads = Telemetry.maximalNumberOfConcurrentThreads;
  do_check_true(currentMaxNumberOfThreads > 0);

  // Try to augment the maximal number of threads currently launched
  let threads = [];
  try {
    for (let i = 0; i < currentMaxNumberOfThreads + 10; ++i) {
      threads.push(Services.tm.newThread(0));
    }
  } catch (ex) {
    // If memory is too low, it is possible that not all threads will be launched.
  }
  gNumberOfThreadsLaunched = threads.length;

  do_check_true(Telemetry.maximalNumberOfConcurrentThreads >= gNumberOfThreadsLaunched);

  do_register_cleanup(function() {
    threads.forEach(function(thread) {
      thread.shutdown();
    });
  });

  Telemetry.asyncFetchTelemetryData(wrapWithExceptionHandler(run_next_test));
}

add_task(function* asyncSetup() {
  yield TelemetrySession.setup();
  yield TelemetryPing.setup();

  if (HAS_DATAREPORTINGSERVICE) {
    // force getSessionRecorder()==undefined to check the payload's activeTicks
    gDatareportingService.simulateNoSessionRecorder();
  }

  // When no DRS or no DRS.getSessionRecorder(), activeTicks should be -1.
  do_check_eq(TelemetrySession.getPayload().simpleMeasurements.activeTicks, -1);

  if (HAS_DATAREPORTINGSERVICE) {
    // Restore normal behavior for getSessionRecorder()
    gDatareportingService.simulateRestoreSessionRecorder();

    gDataReportingClientID = yield gDatareportingService.getClientID();

    // We should have cached the client id now. Lets confirm that by
    // checking the client id before the async ping setup is finished.
    let promisePingSetup = TelemetryPing.reset();
    do_check_eq(TelemetryPing.clientID, gDataReportingClientID);
    yield promisePingSetup;
  }
});

// Ensures that expired histograms are not part of the payload.
add_task(function* test_expiredHistogram() {
  let histogram_id = "FOOBAR";
  let dummy = Telemetry.newHistogram(histogram_id, "30", Telemetry.HISTOGRAM_EXPONENTIAL, 1, 2, 3);

  dummy.add(1);

  do_check_eq(TelemetrySession.getPayload()["histograms"][histogram_id], undefined);
  do_check_eq(TelemetrySession.getPayload()["histograms"]["TELEMETRY_TEST_EXPIRED"], undefined);
});

// Checks that an invalid histogram file is deleted if TelemetryFile fails to parse it.
add_task(function* test_runInvalidJSON() {
  let pingFile = getSavedPingFile("invalid-histograms.dat");

  writeStringToFile(pingFile, "this.is.invalid.JSON");
  do_check_true(pingFile.exists());

  yield TelemetryFile.testLoadHistograms(pingFile);
  do_check_false(pingFile.exists());
});

// Sends a ping to a non existing server. If we remove this test, we won't get
// all the histograms we need in the main ping.
add_task(function* test_noServerPing() {
  yield sendPing();
  // We need two pings in order to make sure STARTUP_MEMORY_STORAGE_SQLIE histograms
  // are initialised. See bug 1131585.
  yield sendPing();
});

// Checks that a sent ping is correctly received by a dummy http server.
add_task(function* test_simplePing() {
  gHttpServer.start(-1);
  gServerStarted = true;
  gRequestIterator = Iterator(new Request());

  let now = new Date(2020, 1, 1, 12, 0, 0);
  let expectedDate = new Date(2020, 1, 1, 0, 0, 0);
  fakeNow(now);

  const expectedSessionUUID = "bd314d15-95bf-4356-b682-b6c4a8942202";
  const expectedSubsessionUUID = "3e2e5f6c-74ba-4e4d-a93f-a48af238a8c7";
  fakeGenerateUUID(() => expectedSessionUUID, () => expectedSubsessionUUID);
  yield TelemetrySession.reset();

  // Session and subsession start dates are faked during TelemetrySession setup. We can
  // now fake the session duration.
  const SESSION_DURATION_IN_MINUTES = 15;
  fakeNow(new Date(2020, 1, 1, 12, SESSION_DURATION_IN_MINUTES, 0));

  yield sendPing();
  let request = yield gRequestIterator.next();
  let ping = decodeRequestPayload(request);

  checkPingFormat(ping, PING_TYPE_MAIN, true, true);

  // Check that we get the data we expect.
  let payload = ping.payload;
  Assert.equal(payload.info.sessionId, expectedSessionUUID);
  Assert.equal(payload.info.subsessionId, expectedSubsessionUUID);
  let sessionStartDate = new Date(payload.info.sessionStartDate);
  Assert.equal(sessionStartDate.toISOString(), expectedDate.toISOString());
  let subsessionStartDate = new Date(payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), expectedDate.toISOString());
  Assert.equal(payload.info.subsessionLength, SESSION_DURATION_IN_MINUTES * 60);

  // Restore the UUID generator so we don't mess with other tests.
  fakeGenerateUUID(generateUUID, generateUUID);
});

// Saves the current session histograms, reloads them, performs a ping
// and checks that the dummy http server received both the previously
// saved histograms and the new ones.
add_task(function* test_saveLoadPing() {
  let histogramsFile = getSavedPingFile("saved-histograms.dat");

  setupTestData();
  yield TelemetrySession.testSaveHistograms(histogramsFile);
  yield TelemetryFile.testLoadHistograms(histogramsFile);
  yield sendPing();

  // Get requests received by dummy server.
  let request1 = yield gRequestIterator.next();
  let request2 = yield gRequestIterator.next();

  Assert.equal(request1.getHeader("content-type"), "application/json; charset=UTF-8",
               "The request must have the correct content-type.");
  Assert.equal(request2.getHeader("content-type"), "application/json; charset=UTF-8",
               "The request must have the correct content-type.");

  // We decode both requests to check for the |reason|.
  let ping1 = decodeRequestPayload(request1);
  let ping2 = decodeRequestPayload(request2);

  // Check we have the correct two requests. Ordering is not guaranteed. The ping type
  // is encoded in the URL.
  let requestTypeComponent = request1.path.split("/")[4];
  if (requestTypeComponent === PING_TYPE_MAIN) {
    checkPingFormat(ping1, PING_TYPE_MAIN, true, true);
    checkPayload(ping1.payload, REASON_TEST_PING, 1);
    checkPingFormat(ping2, PING_TYPE_SAVED_SESSION, true, true);
    checkPayload(ping2.payload, REASON_SAVED_SESSION, 1);
  } else {
    checkPingFormat(ping1, PING_TYPE_SAVED_SESSION, true, true);
    checkPayload(ping1.payload, REASON_SAVED_SESSION, 1);
    checkPingFormat(ping2, PING_TYPE_MAIN, true, true);
    checkPayload(ping2.payload, REASON_TEST_PING, 1);
  }
});

add_task(function* test_checkSubsession() {
  if (gIsAndroid) {
    // We don't support subsessions yet on Android.
    return;
  }

  let now = new Date(2020, 1, 1, 12, 0, 0);
  let expectedDate = new Date(2020, 1, 1, 0, 0, 0);
  fakeNow(now);
  yield TelemetrySession.setup();

  const COUNT_ID = "TELEMETRY_TEST_COUNT";
  const KEYED_ID = "TELEMETRY_TEST_KEYED_COUNT";
  const count = Telemetry.getHistogramById(COUNT_ID);
  const keyed = Telemetry.getKeyedHistogramById(KEYED_ID);
  const registeredIds =
    new Set(Telemetry.registeredHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, []));

  const stableHistograms = new Set([
    "TELEMETRY_TEST_FLAG",
    "TELEMETRY_TEST_COUNT",
    "TELEMETRY_TEST_RELEASE_OPTOUT",
    "TELEMETRY_TEST_RELEASE_OPTIN",
    "STARTUP_CRASH_DETECTED",
  ]);

  const stableKeyedHistograms = new Set([
    "TELEMETRY_TEST_KEYED_FLAG",
    "TELEMETRY_TEST_KEYED_COUNT",
    "TELEMETRY_TEST_KEYED_RELEASE_OPTIN",
    "TELEMETRY_TEST_KEYED_RELEASE_OPTOUT",
  ]);

  // Compare the two sets of histograms.
  // The "subsession" histograms should match the registered
  // "classic" histograms. However, histograms can change
  // between us collecting the different payloads, so we only
  // check for deep equality on known stable histograms.
  checkHistograms = (classic, subsession) => {
    for (let id of Object.keys(classic)) {
      if (!registeredIds.has(id)) {
        continue;
      }

      Assert.ok(id in subsession);
      if (stableHistograms.has(id)) {
        Assert.deepEqual(classic[id],
                         subsession[id]);
      } else {
        Assert.equal(classic[id].histogram_type,
                     subsession[id].histogram_type);
      }
    }
  };

  // Same as above, except for keyed histograms.
  checkKeyedHistograms = (classic, subsession) => {
    for (let id of Object.keys(classic)) {
      if (!registeredIds.has(id)) {
        continue;
      }

      Assert.ok(id in subsession);
      if (stableKeyedHistograms.has(id)) {
        Assert.deepEqual(classic[id],
                         subsession[id]);
      }
    }
  };

  // Both classic and subsession payload histograms should start the same.
  // The payloads should be identical for now except for the reason.
  count.clear();
  keyed.clear();
  let classic = TelemetrySession.getPayload();
  let subsession = TelemetrySession.getPayload("environment-change");

  Assert.equal(classic.info.reason, "gather-payload");
  Assert.equal(subsession.info.reason, "environment-change");
  Assert.ok(!(COUNT_ID in classic.histograms));
  Assert.ok(!(COUNT_ID in subsession.histograms));
  Assert.ok(KEYED_ID in classic.keyedHistograms);
  Assert.ok(KEYED_ID in subsession.keyedHistograms);
  Assert.deepEqual(classic.keyedHistograms[KEYED_ID], {});
  Assert.deepEqual(subsession.keyedHistograms[KEYED_ID], {});

  checkHistograms(classic.histograms, subsession.histograms);
  checkKeyedHistograms(classic.keyedHistograms, subsession.keyedHistograms);

  // Adding values should get picked up in both.
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change");

  Assert.ok(COUNT_ID in classic.histograms);
  Assert.ok(COUNT_ID in subsession.histograms);
  Assert.ok(KEYED_ID in classic.keyedHistograms);
  Assert.ok(KEYED_ID in subsession.keyedHistograms);
  Assert.equal(classic.histograms[COUNT_ID].sum, 1);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["b"].sum, 1);

  checkHistograms(classic.histograms, subsession.histograms);
  checkKeyedHistograms(classic.keyedHistograms, subsession.keyedHistograms);

  // Values should still reset properly.
  count.clear();
  keyed.clear();
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change");

  Assert.ok(!(COUNT_ID in classic.histograms));
  Assert.ok(!(COUNT_ID in subsession.histograms));
  Assert.ok(KEYED_ID in classic.keyedHistograms);
  Assert.ok(KEYED_ID in subsession.keyedHistograms);
  Assert.deepEqual(classic.keyedHistograms[KEYED_ID], {});

  checkHistograms(classic.histograms, subsession.histograms);
  checkKeyedHistograms(classic.keyedHistograms, subsession.keyedHistograms);

  // Adding values should get picked up in both.
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change");

  Assert.ok(COUNT_ID in classic.histograms);
  Assert.ok(COUNT_ID in subsession.histograms);
  Assert.ok(KEYED_ID in classic.keyedHistograms);
  Assert.ok(KEYED_ID in subsession.keyedHistograms);
  Assert.equal(classic.histograms[COUNT_ID].sum, 1);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["b"].sum, 1);

  checkHistograms(classic.histograms, subsession.histograms);
  checkKeyedHistograms(classic.keyedHistograms, subsession.keyedHistograms);

  // We should be able to reset only the subsession histograms.
  // First check that "snapshot and clear" still returns the old state...
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change", true);

  let subsessionStartDate = new Date(classic.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), expectedDate.toISOString());
  subsessionStartDate = new Date(subsession.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), expectedDate.toISOString());
  checkHistograms(classic.histograms, subsession.histograms);
  checkKeyedHistograms(classic.keyedHistograms, subsession.keyedHistograms);

  // ... then check that the next snapshot shows the subsession
  // histograms got reset.
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change");

  Assert.ok(COUNT_ID in classic.histograms);
  Assert.ok(COUNT_ID in subsession.histograms);
  Assert.equal(classic.histograms[COUNT_ID].sum, 1);
  Assert.equal(subsession.histograms[COUNT_ID].sum, 0);

  Assert.ok(KEYED_ID in classic.keyedHistograms);
  Assert.ok(KEYED_ID in subsession.keyedHistograms);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["b"].sum, 1);
  Assert.deepEqual(subsession.keyedHistograms[KEYED_ID], {});

  // Adding values should get picked up in both again.
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change");

  Assert.ok(COUNT_ID in classic.histograms);
  Assert.ok(COUNT_ID in subsession.histograms);
  Assert.equal(classic.histograms[COUNT_ID].sum, 2);
  Assert.equal(subsession.histograms[COUNT_ID].sum, 1);

  Assert.ok(KEYED_ID in classic.keyedHistograms);
  Assert.ok(KEYED_ID in subsession.keyedHistograms);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["a"].sum, 2);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["b"].sum, 2);
  Assert.equal(subsession.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(subsession.keyedHistograms[KEYED_ID]["b"].sum, 1);
});

add_task(function* test_dailyCollection() {
  if (gIsAndroid) {
    // We don't do daily collections yet on Android.
    return;
  }

  let now = new Date(2030, 1, 1, 12, 0, 0);
  let nowDay = new Date(2030, 1, 1, 0, 0, 0);
  let timerCallback = null;
  let timerDelay = null;

  gRequestIterator = Iterator(new Request());

  fakeNow(now);
  fakeDailyTimers((callback, timeout) => {
    dump("fake setDailyTimeout(" + callback + ", " + timeout + ")\n");
    timerCallback = callback;
    timerDelay = timeout;
    return 1;
  }, () => {});

  // Init and check timer.
  yield TelemetrySession.setup();
  TelemetryPing.setServer("http://localhost:" + gHttpServer.identity.primaryPort);

  Assert.ok(!!timerCallback);
  Assert.ok(Number.isFinite(timerDelay));
  let timerDate = futureDate(now, timerDelay);
  let expectedDate = futureDate(nowDay, MS_IN_ONE_DAY);
  Assert.equal(timerDate.toISOString(), expectedDate.toISOString());

  // Set histograms to expected state.
  const COUNT_ID = "TELEMETRY_TEST_COUNT";
  const KEYED_ID = "TELEMETRY_TEST_KEYED_COUNT";
  const count = Telemetry.getHistogramById(COUNT_ID);
  const keyed = Telemetry.getKeyedHistogramById(KEYED_ID);

  count.clear();
  keyed.clear();
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);
  keyed.add("b", 1);

  // Trigger and collect daily ping.
  yield timerCallback();
  let request = yield gRequestIterator.next();
  Assert.ok(!!request);
  let ping = decodeRequestPayload(request);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);
  let subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), nowDay.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["b"].sum, 2);

  // Trigger and collect another ping. The histograms should be reset.
  yield timerCallback();
  request = yield gRequestIterator.next();
  Assert.ok(!!request);
  ping = decodeRequestPayload(request);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);
  subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), nowDay.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 0);
  Assert.deepEqual(ping.payload.keyedHistograms[KEYED_ID], {});

  // Trigger and collect another daily ping, with the histograms being set again.
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);

  yield timerCallback();
  request = yield gRequestIterator.next();
  Assert.ok(!!request);
  ping = decodeRequestPayload(request);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);
  subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), nowDay.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["b"].sum, 1);
});

add_task(function* test_environmentChange() {
  if (gIsAndroid) {
    // We don't split subsessions on environment changes yet on Android.
    return;
  }

  let now = new Date(2040, 1, 1, 12, 0, 0);
  let nowDay = new Date(2040, 1, 1, 0, 0, 0);
  let timerCallback = null;
  let timerDelay = null;

  gRequestIterator = Iterator(new Request());

  fakeNow(now);
  fakeDailyTimers(() => {}, () => {});

  const PREF_TEST = "toolkit.telemetry.test.pref1";
  Preferences.reset(PREF_TEST);
  let prefsToWatch = {};
  prefsToWatch[PREF_TEST] = TelemetryEnvironment.RECORD_PREF_VALUE;

  // Setup.
  yield TelemetrySession.setup();
  TelemetryPing.setServer("http://localhost:" + gHttpServer.identity.primaryPort);
  TelemetryEnvironment._watchPreferences(prefsToWatch);

  // Set histograms to expected state.
  const COUNT_ID = "TELEMETRY_TEST_COUNT";
  const KEYED_ID = "TELEMETRY_TEST_KEYED_COUNT";
  const count = Telemetry.getHistogramById(COUNT_ID);
  const keyed = Telemetry.getKeyedHistogramById(KEYED_ID);

  count.clear();
  keyed.clear();
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);

  // Trigger and collect environment-change ping.
  Preferences.set(PREF_TEST, 1);
  let request = yield gRequestIterator.next();
  Assert.ok(!!request);
  let ping = decodeRequestPayload(request);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.environment.settings.userPrefs[PREF_TEST], 1);
  Assert.equal(ping.payload.info.reason, REASON_ENVIRONMENT_CHANGE);
  let subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), nowDay.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["a"].sum, 1);

  // Trigger and collect another ping. The histograms should be reset.
  Preferences.set(PREF_TEST, 2);
  request = yield gRequestIterator.next();
  Assert.ok(!!request);
  ping = decodeRequestPayload(request);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.environment.settings.userPrefs[PREF_TEST], 2);
  Assert.equal(ping.payload.info.reason, REASON_ENVIRONMENT_CHANGE);
  subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), nowDay.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 0);
  Assert.deepEqual(ping.payload.keyedHistograms[KEYED_ID], {});
});

// Checks that an expired histogram file is deleted when loaded.
add_task(function* test_runOldPingFile() {
  let histogramsFile = getSavedPingFile("old-histograms.dat");

  yield TelemetrySession.testSaveHistograms(histogramsFile);
  do_check_true(histogramsFile.exists());
  let mtime = histogramsFile.lastModifiedTime;
  histogramsFile.lastModifiedTime = mtime - (14 * 24 * 60 * 60 * 1000 + 60000); // 14 days, 1m

  yield TelemetryFile.testLoadHistograms(histogramsFile);
  do_check_false(histogramsFile.exists());
});

add_task(function* test_savedPingsOnShutdown() {
  // On desktop, we expect both "saved-session" and "shutdown" pings. We only expect
  // the former on Android.
  const expectedPings = (gIsAndroid) ? 1 : 2;
  // Assure that we store the ping properly when saving sessions on shutdown.
  // We make the TelemetrySession shutdown to trigger a session save.
  const dir = TelemetryFile.pingDirectoryPath;
  yield OS.File.removeDir(dir, {ignoreAbsent: true});
  yield OS.File.makeDir(dir);
  yield TelemetrySession.shutdown();

  yield TelemetryFile.loadSavedPings();
  Assert.equal(TelemetryFile.pingsLoaded, expectedPings);

  let pingsIterator = TelemetryFile.popPendingPings();
  for (let ping of pingsIterator) {
    Assert.ok("type" in ping);

    let expectedReason =
      (ping.type == PING_TYPE_SAVED_SESSION) ? REASON_SAVED_SESSION : REASON_SHUTDOWN;

    checkPingFormat(ping, ping.type, true, true);
    Assert.equal(ping.payload.info.reason, expectedReason);
    Assert.equal(ping.clientId, gDataReportingClientID);
  }
});

add_task(function* test_savedSessionData() {
  // Create the directory which will contain the data file, if it doesn't already
  // exist.
  const dataDir  = OS.Path.join(OS.Constants.Path.profileDir, "datareporting");
  yield OS.File.makeDir(dataDir);

  // Write test data to the session data file.
  const dataFilePath = OS.Path.join(dataDir, "session-state.json");
  const sessionState = {
    previousSubsessionId: null,
    profileSubsessionCounter: 3785,
  };
  yield CommonUtils.writeJSON(sessionState, dataFilePath);

  const PREF_TEST = "toolkit.telemetry.test.pref1";
  Preferences.reset(PREF_TEST);
  let prefsToWatch = {};
  prefsToWatch[PREF_TEST] = TelemetryEnvironment.RECORD_PREF_VALUE;

  // We expect one new subsession when starting TelemetrySession and one after triggering
  // an environment change.
  const expectedSubsessions = sessionState.profileSubsessionCounter + 3;
  const expectedUUID = "009fd1ad-b85e-4817-b3e5-000000003785";
  fakeGenerateUUID(generateUUID, () => expectedUUID);

  if (gIsAndroid) {
    // We don't support subsessions yet on Android, so skip the next checks.
    return;
  }

  // Start TelemetrySession so that it loads the session data file.
  yield TelemetrySession.reset();
  // Watch a test preference, trigger and environment change and wait for it to propagate.

  // _watchPreferences triggers a subsession notification
  TelemetryEnvironment._watchPreferences(prefsToWatch);
  let changePromise = new Promise(resolve =>
    TelemetryEnvironment.registerChangeListener("test_fake_change", resolve));
  Preferences.set(PREF_TEST, 1);
  yield changePromise;
  TelemetryEnvironment.unregisterChangeListener("test_fake_change");

  let payload = TelemetrySession.getPayload();
  Assert.equal(payload.info.profileSubsessionCounter, expectedSubsessions);
  yield TelemetrySession.shutdown();

  // Load back the serialised session data.
  let data = yield CommonUtils.readJSON(dataFilePath);
  Assert.equal(data.profileSubsessionCounter, expectedSubsessions);
  Assert.equal(data.previousSubsessionId, expectedUUID);
});

add_task(function* test_invalidSessionData() {
  // Create the directory which will contain the data file, if it doesn't already
  // exist.
  const dataDir  = OS.Path.join(OS.Constants.Path.profileDir, "datareporting");
  yield OS.File.makeDir(dataDir);

  // Write test data to the session data file.
  const dataFilePath = OS.Path.join(dataDir, "session-state.json");
  const sessionState = {
    profileSubsessionCounter: "not-a-number?",
    someOtherField: 12,
  };
  yield CommonUtils.writeJSON(sessionState, dataFilePath);

  // The session data file should not load. Only expect the current subsession.
  const expectedSubsessions = 1;
  const expectedUUID = "009fd1ad-b85e-4817-b3e5-000000003785";
  fakeGenerateUUID(() => expectedUUID, () => expectedUUID);
  // Start TelemetrySession so that it loads the session data file.
  yield TelemetrySession.reset();
  let payload = TelemetrySession.getPayload();
  Assert.equal(payload.info.profileSubsessionCounter, expectedSubsessions);
  yield TelemetrySession.shutdown();

  // Load back the serialised session data.
  let data = yield CommonUtils.readJSON(dataFilePath);
  Assert.equal(data.profileSubsessionCounter, expectedSubsessions);
  Assert.equal(data.previousSubsessionId, null);
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
