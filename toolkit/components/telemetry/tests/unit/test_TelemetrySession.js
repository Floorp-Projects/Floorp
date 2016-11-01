/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
/* This testcase triggers two telemetry pings.
 *
 * Telemetry code keeps histograms of past telemetry pings. The first
 * ping populates these histograms. One of those histograms is then
 * checked in the second request.
 */

Cu.import("resource://services-common/utils.js");
Cu.import("resource://gre/modules/ClientID.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/LightweightThemeManager.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm", this);
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetrySession.jsm", this);
Cu.import("resource://gre/modules/TelemetryStorage.jsm", this);
Cu.import("resource://gre/modules/TelemetryEnvironment.jsm", this);
Cu.import("resource://gre/modules/TelemetrySend.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm");
Cu.import("resource://gre/modules/osfile.jsm", this);

const PING_FORMAT_VERSION = 4;
const PING_TYPE_MAIN = "main";
const PING_TYPE_SAVED_SESSION = "saved-session";

const REASON_ABORTED_SESSION = "aborted-session";
const REASON_SAVED_SESSION = "saved-session";
const REASON_SHUTDOWN = "shutdown";
const REASON_TEST_PING = "test-ping";
const REASON_DAILY = "daily";
const REASON_ENVIRONMENT_CHANGE = "environment-change";

const PLATFORM_VERSION = "1.9.2";
const APP_VERSION = "1";
const APP_ID = "xpcshell@tests.mozilla.org";
const APP_NAME = "XPCShell";

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
var gNumberOfThreadsLaunched = 0;

const MS_IN_ONE_HOUR  = 60 * 60 * 1000;
const MS_IN_ONE_DAY   = 24 * MS_IN_ONE_HOUR;

const PREF_BRANCH = "toolkit.telemetry.";
const PREF_SERVER = PREF_BRANCH + "server";
const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

const DATAREPORTING_DIR = "datareporting";
const ABORTED_PING_FILE_NAME = "aborted-session-ping";
const ABORTED_SESSION_UPDATE_INTERVAL_MS = 5 * 60 * 1000;

XPCOMUtils.defineLazyGetter(this, "DATAREPORTING_PATH", function() {
  return OS.Path.join(OS.Constants.Path.profileDir, DATAREPORTING_DIR);
});

var gClientID = null;
var gMonotonicNow = 0;

function generateUUID() {
  let str = Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator).generateUUID().toString();
  // strip {}
  return str.substring(1, str.length - 1);
}

function truncateDateToDays(date) {
  return new Date(date.getFullYear(),
                  date.getMonth(),
                  date.getDate(),
                  0, 0, 0, 0);
}

function sendPing() {
  TelemetrySession.gatherStartup();
  if (PingServer.started) {
    TelemetrySend.setServer("http://localhost:" + PingServer.port);
    return TelemetrySession.testPing();
  }
  TelemetrySend.setServer("http://doesnotexist");
  return TelemetrySession.testPing();
}

function fakeGenerateUUID(sessionFunc, subsessionFunc) {
  let session = Cu.import("resource://gre/modules/TelemetrySession.jsm");
  session.Policy.generateSessionUUID = sessionFunc;
  session.Policy.generateSubsessionUUID = subsessionFunc;
}

function fakeIdleNotification(topic) {
  let session = Cu.import("resource://gre/modules/TelemetrySession.jsm");
  return session.TelemetryScheduler.observe(null, topic, null);
}

function setupTestData() {

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

function checkPingFormat(aPing, aType, aHasClientId, aHasEnvironment) {
  const MANDATORY_PING_FIELDS = [
    "type", "id", "creationDate", "version", "application", "payload"
  ];

  const APPLICATION_TEST_DATA = {
    buildId: gAppInfo.appBuildID,
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
  let numberCheck = arg => { return (typeof arg == "number"); };
  let positiveNumberCheck = arg => { return numberCheck(arg) && (arg >= 0); };
  let stringCheck = arg => { return (typeof arg == "string") && (arg != ""); };
  let revisionCheck = arg => {
    return (Services.appinfo.isOfficial) ? stringCheck(arg) : (typeof arg == "string");
  };
  let uuidCheck = arg => {
    return UUID_REGEX.test(arg);
  };
  let isoDateCheck = arg => {
    // We expect use of this version of the ISO format:
    // 2015-04-12T18:51:19.1+00:00
    const isoDateRegEx = /^\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d+[+-]\d{2}:\d{2}$/;
    return stringCheck(arg) && !Number.isNaN(Date.parse(arg)) &&
           isoDateRegEx.test(arg);
  };

  const EXPECTED_INFO_FIELDS_TYPES = {
    reason: stringCheck,
    revision: revisionCheck,
    timezoneOffset: numberCheck,
    sessionId: uuidCheck,
    subsessionId: uuidCheck,
    // Special cases: previousSessionId and previousSubsessionId are null on first run.
    previousSessionId: (arg) => { return (arg) ? uuidCheck(arg) : true; },
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

function checkScalars(processes) {
  // Check that the scalars section is available in the ping payload.
  const parentProcess = processes.parent;
  Assert.ok("scalars" in parentProcess, "The scalars section must be available in the parent process.");
  Assert.ok("keyedScalars" in parentProcess, "The keyedScalars section must be available in the parent process.");
  Assert.equal(typeof parentProcess.scalars, "object", "The scalars entry must be an object.");
  Assert.equal(typeof parentProcess.keyedScalars, "object", "The keyedScalars entry must be an object.");

  let checkScalar = function(scalar) {
    // Check if the value is of a supported type.
    const valueType = typeof(scalar);
    switch (valueType) {
      case "string":
        Assert.ok(scalar.length <= 50,
                  "String values can't have more than 50 characters");
      break;
      case "number":
        Assert.ok(scalar >= 0,
                  "We only support unsigned integer values in scalars.");
      break;
      case "boolean":
        Assert.ok(true,
                  "Boolean scalar found.");
      break;
      default:
        Assert.ok(false,
                  name + " contains an unsupported value type (" + valueType + ")");
    }
  }

  // Check that we have valid scalar entries.
  const scalars = parentProcess.scalars;
  for (let name in scalars) {
    Assert.equal(typeof name, "string", "Scalar names must be strings.");
    checkScalar(scalar[name]);
  }

  // Check that we have valid keyed scalar entries.
  const keyedScalars = parentProcess.keyedScalars;
  for (let name in keyedScalars) {
    Assert.equal(typeof name, "string", "Scalar names must be strings.");
    Assert.ok(Object.keys(keyedScalars[name]).length,
              "The reported keyed scalars must contain at least 1 key.");
    for (let key in keyedScalars[name]) {
      Assert.equal(typeof key, "string", "Keyed scalar keys must be strings.");
      Assert.ok(key.length <= 70, "Keyed scalar keys can't have more than 70 characters.");
      checkScalar(scalar[name][key]);
    }
  }
}

function checkPayload(payload, reason, successfulPings, savedPings) {
  Assert.ok("info" in payload, "Payload must contain an info section.");
  checkPayloadInfo(payload.info);

  Assert.ok(payload.simpleMeasurements.totalTime >= 0);
  Assert.ok(payload.simpleMeasurements.uptime >= 0);
  Assert.equal(payload.simpleMeasurements.startupInterrupted, 1);
  Assert.equal(payload.simpleMeasurements.shutdownDuration, SHUTDOWN_TIME);
  Assert.equal(payload.simpleMeasurements.savedPings, savedPings);
  Assert.ok("maximalNumberOfConcurrentThreads" in payload.simpleMeasurements);
  Assert.ok(payload.simpleMeasurements.maximalNumberOfConcurrentThreads >= gNumberOfThreadsLaunched);

  let activeTicks = payload.simpleMeasurements.activeTicks;
  Assert.ok(activeTicks >= 0);

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

  if (successfulPings > 0) {
    Assert.ok(TELEMETRY_PING in payload.histograms);
  }
  Assert.ok(TELEMETRY_TEST_FLAG in payload.histograms);
  Assert.ok(TELEMETRY_TEST_COUNT in payload.histograms);

  let rh = Telemetry.registeredHistograms(Ci.nsITelemetry.DATASET_RELEASE_CHANNEL_OPTIN, []);
  for (let name of rh) {
    if (/SQLITE/.test(name) && name in payload.histograms) {
      let histogramName = ("STARTUP_" + name);
      Assert.ok(histogramName in payload.histograms, histogramName + " must be available.");
    }
  }

  Assert.ok(!(IGNORE_CLONED_HISTOGRAM in payload.histograms));

  // Flag histograms should automagically spring to life.
  const expected_flag = {
    range: [1, 2],
    bucket_count: 3,
    histogram_type: 3,
    values: {0:1, 1:0},
    sum: 0
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
  };
  let count = payload.histograms[TELEMETRY_TEST_COUNT];
  Assert.equal(uneval(count), uneval(expected_count));

  // There should be one successful report from the previous telemetry ping.
  if (successfulPings > 0) {
    const expected_tc = {
      range: [1, 2],
      bucket_count: 3,
      histogram_type: 2,
      values: {0:2, 1:successfulPings, 2:0},
      sum: successfulPings
    };
    let tc = payload.histograms[TELEMETRY_SUCCESS];
    Assert.equal(uneval(tc), uneval(expected_tc));
  }

  // The ping should include data from memory reporters.  We can't check that
  // this data is correct, because we can't control the values returned by the
  // memory reporters.  But we can at least check that the data is there.
  //
  // It's important to check for the presence of reporters with a mix of units,
  // because TelemetryController has separate logic for each one.  But we can't
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

  Assert.ok(("IceCandidatesStats" in payload.webrtc) &&
                ("webrtc" in payload.webrtc.IceCandidatesStats));

  // Check keyed histogram payload.

  Assert.ok("keyedHistograms" in payload);
  let keyedHistograms = payload.keyedHistograms;
  Assert.ok(!(TELEMETRY_TEST_KEYED_FLAG in keyedHistograms));
  Assert.ok(TELEMETRY_TEST_KEYED_COUNT in keyedHistograms);

  const expected_keyed_count = {
    "a": {
      range: [1, 2],
      bucket_count: 3,
      histogram_type: 4,
      values: {0:2, 1:0},
      sum: 2,
    },
    "b": {
      range: [1, 2],
      bucket_count: 3,
      histogram_type: 4,
      values: {0:1, 1:0},
      sum: 1,
    },
  };
  Assert.deepEqual(expected_keyed_count, keyedHistograms[TELEMETRY_TEST_KEYED_COUNT]);

  Assert.ok("processes" in payload, "The payload must have a processes section.");
  Assert.ok("parent" in payload.processes, "There must be at least a parent process.");
  checkScalars(payload.processes);
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

add_task(function* test_setup() {
  // Addon manager needs a profile directory
  do_get_profile();
  loadAddonManager(APP_ID, APP_NAME, APP_VERSION, PLATFORM_VERSION);
  // Make sure we don't generate unexpected pings due to pref changes.
  yield setEmptyPrefWatchlist();

  Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
  Services.prefs.setBoolPref(PREF_FHR_UPLOAD_ENABLED, true);

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

  yield new Promise(resolve =>
    Telemetry.asyncFetchTelemetryData(wrapWithExceptionHandler(resolve)));
});

add_task(function* asyncSetup() {
  yield TelemetryController.testSetup();
  // Load the client ID from the client ID provider to check for pings sanity.
  gClientID = yield ClientID.getClientID();
});

// Ensures that expired histograms are not part of the payload.
add_task(function* test_expiredHistogram() {

  let dummy = Telemetry.getHistogramById("TELEMETRY_TEST_EXPIRED");

  dummy.add(1);

  do_check_eq(TelemetrySession.getPayload()["histograms"]["TELEMETRY_TEST_EXPIRED"], undefined);
});

// Sends a ping to a non existing server. If we remove this test, we won't get
// all the histograms we need in the main ping.
add_task(function* test_noServerPing() {
  yield sendPing();
  // We need two pings in order to make sure STARTUP_MEMORY_STORAGE_SQLIE histograms
  // are initialised. See bug 1131585.
  yield sendPing();
  // Allowing Telemetry to persist unsent pings as pending. If omitted may cause
  // problems to the consequent tests.
  yield TelemetryController.testShutdown();
});

// Checks that a sent ping is correctly received by a dummy http server.
add_task(function* test_simplePing() {
  yield TelemetryStorage.testClearPendingPings();
  PingServer.start();
  Preferences.set(PREF_SERVER, "http://localhost:" + PingServer.port);

  let now = new Date(2020, 1, 1, 12, 0, 0);
  let expectedDate = new Date(2020, 1, 1, 0, 0, 0);
  fakeNow(now);
  gMonotonicNow = fakeMonotonicNow(gMonotonicNow + 5000);

  const expectedSessionUUID = "bd314d15-95bf-4356-b682-b6c4a8942202";
  const expectedSubsessionUUID = "3e2e5f6c-74ba-4e4d-a93f-a48af238a8c7";
  fakeGenerateUUID(() => expectedSessionUUID, () => expectedSubsessionUUID);
  yield TelemetryController.testReset();

  // Session and subsession start dates are faked during TelemetrySession setup. We can
  // now fake the session duration.
  const SESSION_DURATION_IN_MINUTES = 15;
  fakeNow(new Date(2020, 1, 1, 12, SESSION_DURATION_IN_MINUTES, 0));
  gMonotonicNow = fakeMonotonicNow(gMonotonicNow + SESSION_DURATION_IN_MINUTES * 60 * 1000);

  yield sendPing();
  let ping = yield PingServer.promiseNextPing();

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
// saved ping and the new one.
add_task(function* test_saveLoadPing() {
  // Let's start out with a defined state.
  yield TelemetryStorage.testClearPendingPings();
  yield TelemetryController.testReset();
  PingServer.clearRequests();

  // Setup test data and trigger pings.
  setupTestData();
  yield TelemetrySession.testSavePendingPing();
  yield sendPing();

  // Get requests received by dummy server.
  const requests = yield PingServer.promiseNextRequests(2);

  for (let req of requests) {
    Assert.equal(req.getHeader("content-type"), "application/json; charset=UTF-8",
                 "The request must have the correct content-type.");
  }

  // We decode both requests to check for the |reason|.
  let pings = Array.from(requests, decodeRequestPayload);

  // Check we have the correct two requests. Ordering is not guaranteed. The ping type
  // is encoded in the URL.
  if (pings[0].type != PING_TYPE_MAIN) {
    pings.reverse();
  }

  checkPingFormat(pings[0], PING_TYPE_MAIN, true, true);
  checkPayload(pings[0].payload, REASON_TEST_PING, 0, 1);
  checkPingFormat(pings[1], PING_TYPE_SAVED_SESSION, true, true);
  checkPayload(pings[1].payload, REASON_SAVED_SESSION, 0, 0);
});

add_task(function* test_checkSubsessionScalars() {
  if (gIsAndroid) {
    // We don't support subsessions yet on Android.
    return;
  }

  // Clear the scalars.
  Telemetry.clearScalars();
  yield TelemetryController.testReset();

  // Set some scalars.
  const UINT_SCALAR = "telemetry.test.unsigned_int_kind";
  const STRING_SCALAR = "telemetry.test.string_kind";
  let expectedUint = 37;
  let expectedString = "Test value. Yay.";
  Telemetry.scalarSet(UINT_SCALAR, expectedUint);
  Telemetry.scalarSet(STRING_SCALAR, expectedString);

  // Check that scalars are not available in classic pings but are in subsession
  // pings. Also clear the subsession.
  let classic = TelemetrySession.getPayload();
  let subsession = TelemetrySession.getPayload("environment-change", true);

  const TEST_SCALARS = [ UINT_SCALAR, STRING_SCALAR ];
  for (let name of TEST_SCALARS) {
    // Scalar must be reported in subsession pings (e.g. main).
    Assert.ok(name in subsession.processes.parent.scalars,
              name + " must be reported in a subsession ping.");
  }
  // No scalar must be reported in classic pings (e.g. saved-session).
  Assert.ok(Object.keys(classic.processes.parent.scalars).length == 0,
            "Scalars must not be reported in a classic ping.");

  // And make sure that we're getting the right values in the
  // subsession ping.
  Assert.equal(subsession.processes.parent.scalars[UINT_SCALAR], expectedUint,
               UINT_SCALAR + " must contain the expected value.");
  Assert.equal(subsession.processes.parent.scalars[STRING_SCALAR], expectedString,
               STRING_SCALAR + " must contain the expected value.");

  // Since we cleared the subsession in the last getPayload(), check that
  // breaking subsessions clears the scalars.
  subsession = TelemetrySession.getPayload("environment-change");
  for (let name of TEST_SCALARS) {
    Assert.ok(!(name in subsession.processes.parent.scalars),
              name + " must be cleared with the new subsession.");
  }

  // Check if setting the scalars again works as expected.
  expectedUint = 85;
  expectedString = "A creative different value";
  Telemetry.scalarSet(UINT_SCALAR, expectedUint);
  Telemetry.scalarSet(STRING_SCALAR, expectedString);
  subsession = TelemetrySession.getPayload("environment-change");
  Assert.equal(subsession.processes.parent.scalars[UINT_SCALAR], expectedUint,
               UINT_SCALAR + " must contain the expected value.");
  Assert.equal(subsession.processes.parent.scalars[STRING_SCALAR], expectedString,
               STRING_SCALAR + " must contain the expected value.");
});

add_task(function* test_checkSubsessionHistograms() {
  if (gIsAndroid) {
    // We don't support subsessions yet on Android.
    return;
  }

  let now = new Date(2020, 1, 1, 12, 0, 0);
  let expectedDate = new Date(2020, 1, 1, 0, 0, 0);
  fakeNow(now);
  yield TelemetryController.testReset();

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
  Assert.ok(!(KEYED_ID in classic.keyedHistograms));
  Assert.ok(!(KEYED_ID in subsession.keyedHistograms));

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
  Assert.ok(!(KEYED_ID in classic.keyedHistograms));
  Assert.ok(!(KEYED_ID in subsession.keyedHistograms));

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
  Assert.ok(!(KEYED_ID in subsession.keyedHistograms));
  Assert.equal(classic.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(classic.keyedHistograms[KEYED_ID]["b"].sum, 1);

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

add_task(function* test_checkSubsessionData() {
  if (gIsAndroid) {
    // We don't support subsessions yet on Android.
    return;
  }

  // Keep track of the active ticks count if the session recorder is available.
  let sessionRecorder = TelemetryController.getSessionRecorder();
  let activeTicksAtSubsessionStart = sessionRecorder.activeTicks;
  let expectedActiveTicks = activeTicksAtSubsessionStart;

  incrementActiveTicks = () => {
    sessionRecorder.incrementActiveTicks();
    ++expectedActiveTicks;
  }

  yield TelemetryController.testReset();

  // Both classic and subsession payload data should be the same on the first subsession.
  incrementActiveTicks();
  let classic = TelemetrySession.getPayload();
  let subsession = TelemetrySession.getPayload("environment-change");
  Assert.equal(classic.simpleMeasurements.activeTicks, expectedActiveTicks,
               "Classic pings must count active ticks since the beginning of the session.");
  Assert.equal(subsession.simpleMeasurements.activeTicks, expectedActiveTicks,
               "Subsessions must count active ticks as classic pings on the first subsession.");

  // Start a new subsession and check that the active ticks are correctly reported.
  incrementActiveTicks();
  activeTicksAtSubsessionStart = sessionRecorder.activeTicks;
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change", true);
  Assert.equal(classic.simpleMeasurements.activeTicks, expectedActiveTicks,
               "Classic pings must count active ticks since the beginning of the session.");
  Assert.equal(subsession.simpleMeasurements.activeTicks, expectedActiveTicks,
               "Pings must not loose the tick count when starting a new subsession.");

  // Get a new subsession payload without clearing the subsession.
  incrementActiveTicks();
  classic = TelemetrySession.getPayload();
  subsession = TelemetrySession.getPayload("environment-change");
  Assert.equal(classic.simpleMeasurements.activeTicks, expectedActiveTicks,
               "Classic pings must count active ticks since the beginning of the session.");
  Assert.equal(subsession.simpleMeasurements.activeTicks,
               expectedActiveTicks - activeTicksAtSubsessionStart,
               "Subsessions must count active ticks since the last new subsession.");
});

add_task(function* test_dailyCollection() {
  if (gIsAndroid) {
    // We don't do daily collections yet on Android.
    return;
  }

  let now = new Date(2030, 1, 1, 12, 0, 0);
  let nowDay = new Date(2030, 1, 1, 0, 0, 0);
  let schedulerTickCallback = null;

  PingServer.clearRequests();

  fakeNow(now);

  // Fake scheduler functions to control daily collection flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});

  // Init and check timer.
  yield TelemetryStorage.testClearPendingPings();
  yield TelemetryController.testSetup();
  TelemetrySend.setServer("http://localhost:" + PingServer.port);

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

  // Make sure the daily ping gets triggered.
  let expectedDate = nowDay;
  now = futureDate(nowDay, MS_IN_ONE_DAY);
  fakeNow(now);

  Assert.ok(!!schedulerTickCallback);
  // Run a scheduler tick: it should trigger the daily ping.
  yield schedulerTickCallback();

  // Collect the daily ping.
  let ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);
  let subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), expectedDate.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["b"].sum, 2);

  // The daily ping is rescheduled for "tomorrow".
  expectedDate = futureDate(expectedDate, MS_IN_ONE_DAY);
  now = futureDate(now, MS_IN_ONE_DAY);
  fakeNow(now);

  // Run a scheduler tick. Trigger and collect another ping. The histograms should be reset.
  yield schedulerTickCallback();

  ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);
  subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), expectedDate.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 0);
  Assert.ok(!(KEYED_ID in ping.payload.keyedHistograms));

  // Trigger and collect another daily ping, with the histograms being set again.
  count.add(1);
  keyed.add("a", 1);
  keyed.add("b", 1);

  // The daily ping is rescheduled for "tomorrow".
  expectedDate = futureDate(expectedDate, MS_IN_ONE_DAY);
  now = futureDate(now, MS_IN_ONE_DAY);
  fakeNow(now);

  yield schedulerTickCallback();
  ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);
  subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), expectedDate.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["a"].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["b"].sum, 1);

  // Shutdown to cleanup the aborted-session if it gets created.
  yield TelemetryController.testShutdown();
});

add_task(function* test_dailyDuplication() {
  if (gIsAndroid) {
    // We don't do daily collections yet on Android.
    return;
  }

  yield TelemetrySend.reset();
  yield TelemetryStorage.testClearPendingPings();
  PingServer.clearRequests();

  let schedulerTickCallback = null;
  let now = new Date(2030, 1, 1, 0, 0, 0);
  fakeNow(now);
  // Fake scheduler functions to control daily collection flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetryController.testReset();

  // Make sure the daily ping gets triggered at midnight.
  // We need to make sure that we trigger this after the period where we wait for
  // the user to become idle.
  let firstDailyDue = new Date(2030, 1, 2, 0, 0, 0);
  fakeNow(firstDailyDue);

  // Run a scheduler tick: it should trigger the daily ping.
  Assert.ok(!!schedulerTickCallback);
  yield schedulerTickCallback();

  // Get the first daily ping.
  let ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);

  // We don't expect to receive any other daily ping in this test, so assert if we do.
  PingServer.registerPingHandler((req, res) => {
    Assert.ok(false, "No more daily pings should be sent/received in this test.");
  });

  // Set the current time to a bit after midnight.
  let secondDailyDue = new Date(firstDailyDue);
  secondDailyDue.setHours(0);
  secondDailyDue.setMinutes(15);
  fakeNow(secondDailyDue);

  // Run a scheduler tick: it should NOT trigger the daily ping.
  Assert.ok(!!schedulerTickCallback);
  yield schedulerTickCallback();

  // Shutdown to cleanup the aborted-session if it gets created.
  PingServer.resetPingHandler();
  yield TelemetryController.testShutdown();
});

add_task(function* test_dailyOverdue() {
  if (gIsAndroid) {
    // We don't do daily collections yet on Android.
    return;
  }

  let schedulerTickCallback = null;
  let now = new Date(2030, 1, 1, 11, 0, 0);
  fakeNow(now);
  // Fake scheduler functions to control daily collection flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetryStorage.testClearPendingPings();
  yield TelemetryController.testReset();

  // Skip one hour ahead: nothing should be due.
  now.setHours(now.getHours() + 1);
  fakeNow(now);

  // Assert if we receive something!
  PingServer.registerPingHandler((req, res) => {
    Assert.ok(false, "No daily ping should be received if not overdue!.");
  });

  // This tick should not trigger any daily ping.
  Assert.ok(!!schedulerTickCallback);
  yield schedulerTickCallback();

  // Restore the non asserting ping handler.
  PingServer.resetPingHandler();
  PingServer.clearRequests();

  // Simulate an overdue ping: we're not close to midnight, but the last daily ping
  // time is too long ago.
  let dailyOverdue = new Date(2030, 1, 2, 13, 0, 0);
  fakeNow(dailyOverdue);

  // Run a scheduler tick: it should trigger the daily ping.
  Assert.ok(!!schedulerTickCallback);
  yield schedulerTickCallback();

  // Get the first daily ping.
  let ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.payload.info.reason, REASON_DAILY);

  // Shutdown to cleanup the aborted-session if it gets created.
  yield TelemetryController.testShutdown();
});

add_task(function* test_environmentChange() {
  if (gIsAndroid) {
    // We don't split subsessions on environment changes yet on Android.
    return;
  }

  let timerCallback = null;
  let timerDelay = null;

  yield TelemetryStorage.testClearPendingPings();
  PingServer.clearRequests();

  let now = fakeNow(2040, 1, 1, 12, 0, 0);
  gMonotonicNow = fakeMonotonicNow(gMonotonicNow + 10 * MILLISECONDS_PER_MINUTE);

  const PREF_TEST = "toolkit.telemetry.test.pref1";
  Preferences.reset(PREF_TEST);

  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, {what: TelemetryEnvironment.RECORD_PREF_VALUE}],
  ]);

  // Setup.
  yield TelemetryController.testReset();
  TelemetrySend.setServer("http://localhost:" + PingServer.port);
  TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);

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
  gMonotonicNow = fakeMonotonicNow(gMonotonicNow + 10 * MILLISECONDS_PER_MINUTE);
  let startDay = truncateDateToDays(now);
  now = fakeNow(futureDate(now, 10 * MILLISECONDS_PER_MINUTE));

  Preferences.set(PREF_TEST, 1);
  let ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.environment.settings.userPrefs[PREF_TEST], undefined);
  Assert.equal(ping.payload.info.reason, REASON_ENVIRONMENT_CHANGE);
  let subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), startDay.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 1);
  Assert.equal(ping.payload.keyedHistograms[KEYED_ID]["a"].sum, 1);

  // Trigger and collect another ping. The histograms should be reset.
  startDay = truncateDateToDays(now);
  gMonotonicNow = fakeMonotonicNow(gMonotonicNow + 10 * MILLISECONDS_PER_MINUTE);
  now = fakeNow(futureDate(now, 10 * MILLISECONDS_PER_MINUTE));

  Preferences.set(PREF_TEST, 2);
  ping = yield PingServer.promiseNextPing();
  Assert.ok(!!ping);

  Assert.equal(ping.type, PING_TYPE_MAIN);
  Assert.equal(ping.environment.settings.userPrefs[PREF_TEST], 1);
  Assert.equal(ping.payload.info.reason, REASON_ENVIRONMENT_CHANGE);
  subsessionStartDate = new Date(ping.payload.info.subsessionStartDate);
  Assert.equal(subsessionStartDate.toISOString(), startDay.toISOString());

  Assert.equal(ping.payload.histograms[COUNT_ID].sum, 0);
  Assert.ok(!(KEYED_ID in ping.payload.keyedHistograms));
});

add_task(function* test_savedPingsOnShutdown() {
  // On desktop, we expect both "saved-session" and "shutdown" pings. We only expect
  // the former on Android.
  const expectedPingCount = (gIsAndroid) ? 1 : 2;
  // Assure that we store the ping properly when saving sessions on shutdown.
  // We make the TelemetryController shutdown to trigger a session save.
  const dir = TelemetryStorage.pingDirectoryPath;
  yield OS.File.removeDir(dir, {ignoreAbsent: true});
  yield OS.File.makeDir(dir);
  yield TelemetryController.testShutdown();

  PingServer.clearRequests();
  yield TelemetryController.testReset();

  const pings = yield PingServer.promiseNextPings(expectedPingCount);

  for (let ping of pings) {
    Assert.ok("type" in ping);

    let expectedReason =
      (ping.type == PING_TYPE_SAVED_SESSION) ? REASON_SAVED_SESSION : REASON_SHUTDOWN;

    checkPingFormat(ping, ping.type, true, true);
    Assert.equal(ping.payload.info.reason, expectedReason);
    Assert.equal(ping.clientId, gClientID);
  }
});

add_task(function* test_savedSessionData() {
  // Create the directory which will contain the data file, if it doesn't already
  // exist.
  yield OS.File.makeDir(DATAREPORTING_PATH);
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_LOAD").clear();
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_PARSE").clear();
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").clear();

  // Write test data to the session data file.
  const dataFilePath = OS.Path.join(DATAREPORTING_PATH, "session-state.json");
  const sessionState = {
    sessionId: null,
    subsessionId: null,
    profileSubsessionCounter: 3785,
  };
  yield CommonUtils.writeJSON(sessionState, dataFilePath);

  const PREF_TEST = "toolkit.telemetry.test.pref1";
  Preferences.reset(PREF_TEST);
  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, {what: TelemetryEnvironment.RECORD_PREF_VALUE}],
  ]);

  // We expect one new subsession when starting TelemetrySession and one after triggering
  // an environment change.
  const expectedSubsessions = sessionState.profileSubsessionCounter + 2;
  const expectedSessionUUID = "ff602e52-47a1-b7e8-4c1a-ffffffffc87a";
  const expectedSubsessionUUID = "009fd1ad-b85e-4817-b3e5-000000003785";
  fakeGenerateUUID(() => expectedSessionUUID, () => expectedSubsessionUUID);

  if (gIsAndroid) {
    // We don't support subsessions yet on Android, so skip the next checks.
    return;
  }

  // Start TelemetrySession so that it loads the session data file.
  yield TelemetryController.testReset();
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_LOAD").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_PARSE").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").sum);

  // Watch a test preference, trigger and environment change and wait for it to propagate.
  // _watchPreferences triggers a subsession notification
  let now = fakeNow(new Date(2050, 1, 1, 12, 0, 0));
  gMonotonicNow = fakeMonotonicNow(gMonotonicNow + 10 * MILLISECONDS_PER_MINUTE);
  TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);
  let changePromise = new Promise(resolve =>
    TelemetryEnvironment.registerChangeListener("test_fake_change", resolve));
  Preferences.set(PREF_TEST, 1);
  yield changePromise;
  TelemetryEnvironment.unregisterChangeListener("test_fake_change");

  let payload = TelemetrySession.getPayload();
  Assert.equal(payload.info.profileSubsessionCounter, expectedSubsessions);
  yield TelemetryController.testShutdown();

  // Restore the UUID generator so we don't mess with other tests.
  fakeGenerateUUID(generateUUID, generateUUID);

  // Load back the serialised session data.
  let data = yield CommonUtils.readJSON(dataFilePath);
  Assert.equal(data.profileSubsessionCounter, expectedSubsessions);
  Assert.equal(data.sessionId, expectedSessionUUID);
  Assert.equal(data.subsessionId, expectedSubsessionUUID);
});

add_task(function* test_sessionData_ShortSession() {
  if (gIsAndroid) {
    // We don't support subsessions yet on Android, so skip the next checks.
    return;
  }

  const SESSION_STATE_PATH = OS.Path.join(DATAREPORTING_PATH, "session-state.json");

  // Shut down Telemetry and remove the session state file.
  yield TelemetryController.testReset();
  yield OS.File.remove(SESSION_STATE_PATH, { ignoreAbsent: true });
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_LOAD").clear();
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_PARSE").clear();
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").clear();

  const expectedSessionUUID = "ff602e52-47a1-b7e8-4c1a-ffffffffc87a";
  const expectedSubsessionUUID = "009fd1ad-b85e-4817-b3e5-000000003785";
  fakeGenerateUUID(() => expectedSessionUUID, () => expectedSubsessionUUID);

  // We intentionally don't wait for the setup to complete and shut down to simulate
  // short sessions. We expect the profile subsession counter to be 1.
  TelemetryController.testReset();
  yield TelemetryController.testShutdown();

  Assert.equal(1, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_LOAD").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_PARSE").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").sum);

  // Restore the UUID generation functions.
  fakeGenerateUUID(generateUUID, generateUUID);

  // Start TelemetryController so that it loads the session data file. We expect the profile
  // subsession counter to be incremented by 1 again.
  yield TelemetryController.testReset();

  // We expect 2 profile subsession counter updates.
  let payload = TelemetrySession.getPayload();
  Assert.equal(payload.info.profileSubsessionCounter, 2);
  Assert.equal(payload.info.previousSessionId, expectedSessionUUID);
  Assert.equal(payload.info.previousSubsessionId, expectedSubsessionUUID);
  Assert.equal(1, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_LOAD").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_PARSE").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").sum);
});

add_task(function* test_invalidSessionData() {
  // Create the directory which will contain the data file, if it doesn't already
  // exist.
  yield OS.File.makeDir(DATAREPORTING_PATH);
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_LOAD").clear();
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_PARSE").clear();
  getHistogram("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").clear();

  // Write test data to the session data file. This should fail to parse.
  const dataFilePath = OS.Path.join(DATAREPORTING_PATH, "session-state.json");
  const unparseableData = "{asdf:@äü";
  OS.File.writeAtomic(dataFilePath, unparseableData,
                      {encoding: "utf-8", tmpPath: dataFilePath + ".tmp"});

  // Start TelemetryController so that it loads the session data file.
  yield TelemetryController.testReset();

  // The session data file should not load. Only expect the current subsession.
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_LOAD").sum);
  Assert.equal(1, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_PARSE").sum);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").sum);

  // Write test data to the session data file. This should fail validation.
  const sessionState = {
    profileSubsessionCounter: "not-a-number?",
    someOtherField: 12,
  };
  yield CommonUtils.writeJSON(sessionState, dataFilePath);

  // The session data file should not load. Only expect the current subsession.
  const expectedSubsessions = 1;
  const expectedSessionUUID = "ff602e52-47a1-b7e8-4c1a-ffffffffc87a";
  const expectedSubsessionUUID = "009fd1ad-b85e-4817-b3e5-000000003785";
  fakeGenerateUUID(() => expectedSessionUUID, () => expectedSubsessionUUID);

  // Start TelemetryController so that it loads the session data file.
  yield TelemetryController.testReset();

  let payload = TelemetrySession.getPayload();
  Assert.equal(payload.info.profileSubsessionCounter, expectedSubsessions);
  Assert.equal(0, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_LOAD").sum);
  Assert.equal(1, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_PARSE").sum);
  Assert.equal(1, getSnapshot("TELEMETRY_SESSIONDATA_FAILED_VALIDATION").sum);

  yield TelemetryController.testShutdown();

  // Restore the UUID generator so we don't mess with other tests.
  fakeGenerateUUID(generateUUID, generateUUID);

  // Load back the serialised session data.
  let data = yield CommonUtils.readJSON(dataFilePath);
  Assert.equal(data.profileSubsessionCounter, expectedSubsessions);
  Assert.equal(data.sessionId, expectedSessionUUID);
  Assert.equal(data.subsessionId, expectedSubsessionUUID);
});

add_task(function* test_abortedSession() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session ping here.
    return;
  }

  const ABORTED_FILE = OS.Path.join(DATAREPORTING_PATH, ABORTED_PING_FILE_NAME);

  // Make sure the aborted sessions directory does not exist to test its creation.
  yield OS.File.removeDir(DATAREPORTING_PATH, { ignoreAbsent: true });

  let schedulerTickCallback = null;
  let now = new Date(2040, 1, 1, 0, 0, 0);
  fakeNow(now);
  // Fake scheduler functions to control aborted-session flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetryController.testReset();

  Assert.ok((yield OS.File.exists(DATAREPORTING_PATH)),
            "Telemetry must create the aborted session directory when starting.");

  // Fake now again so that the scheduled aborted-session save takes place.
  now = futureDate(now, ABORTED_SESSION_UPDATE_INTERVAL_MS);
  fakeNow(now);
  // The first aborted session checkpoint must take place right after the initialisation.
  Assert.ok(!!schedulerTickCallback);
  // Execute one scheduler tick.
  yield schedulerTickCallback();
  // Check that the aborted session is due at the correct time.
  Assert.ok((yield OS.File.exists(ABORTED_FILE)),
            "There must be an aborted session ping.");

  // This ping is not yet in the pending pings folder, so we can't access it using
  // TelemetryStorage.popPendingPings().
  let pingContent = yield OS.File.read(ABORTED_FILE, { encoding: "utf-8" });
  let abortedSessionPing = JSON.parse(pingContent);

  // Validate the ping.
  checkPingFormat(abortedSessionPing, PING_TYPE_MAIN, true, true);
  Assert.equal(abortedSessionPing.payload.info.reason, REASON_ABORTED_SESSION);

  // Trigger a another aborted-session ping and check that it overwrites the previous one.
  now = futureDate(now, ABORTED_SESSION_UPDATE_INTERVAL_MS);
  fakeNow(now);
  yield schedulerTickCallback();

  pingContent = yield OS.File.read(ABORTED_FILE, { encoding: "utf-8" });
  let updatedAbortedSessionPing = JSON.parse(pingContent);
  checkPingFormat(updatedAbortedSessionPing, PING_TYPE_MAIN, true, true);
  Assert.equal(updatedAbortedSessionPing.payload.info.reason, REASON_ABORTED_SESSION);
  Assert.notEqual(abortedSessionPing.id, updatedAbortedSessionPing.id);
  Assert.notEqual(abortedSessionPing.creationDate, updatedAbortedSessionPing.creationDate);

  yield TelemetryController.testShutdown();
  Assert.ok(!(yield OS.File.exists(ABORTED_FILE)),
            "No aborted session ping must be available after a shutdown.");

  // Write the ping to the aborted-session file. TelemetrySession will add it to the
  // saved pings directory when it starts.
  yield TelemetryStorage.savePingToFile(abortedSessionPing, ABORTED_FILE, false);
  Assert.ok((yield OS.File.exists(ABORTED_FILE)),
            "The aborted session ping must exist in the aborted session ping directory.");

  yield TelemetryStorage.testClearPendingPings();
  PingServer.clearRequests();
  yield TelemetryController.testReset();

  Assert.ok(!(yield OS.File.exists(ABORTED_FILE)),
            "The aborted session ping must be removed from the aborted session ping directory.");

  // Restarting Telemetry again to trigger sending pings in TelemetrySend.
  yield TelemetryController.testReset();

  // We should have received an aborted-session ping.
  const receivedPing = yield PingServer.promiseNextPing();
  Assert.equal(receivedPing.type, PING_TYPE_MAIN, "Should have the correct type");
  Assert.equal(receivedPing.payload.info.reason, REASON_ABORTED_SESSION, "Ping should have the correct reason");

  yield TelemetryController.testShutdown();
});

add_task(function* test_abortedSession_Shutdown() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session ping here.
    return;
  }

  const ABORTED_FILE = OS.Path.join(DATAREPORTING_PATH, ABORTED_PING_FILE_NAME);

  let schedulerTickCallback = null;
  let now = fakeNow(2040, 1, 1, 0, 0, 0);
  // Fake scheduler functions to control aborted-session flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetryController.testReset();

  Assert.ok((yield OS.File.exists(DATAREPORTING_PATH)),
            "Telemetry must create the aborted session directory when starting.");

  // Fake now again so that the scheduled aborted-session save takes place.
  now = fakeNow(futureDate(now, ABORTED_SESSION_UPDATE_INTERVAL_MS));
  // The first aborted session checkpoint must take place right after the initialisation.
  Assert.ok(!!schedulerTickCallback);
  // Execute one scheduler tick.
  yield schedulerTickCallback();
  // Check that the aborted session is due at the correct time.
  Assert.ok((yield OS.File.exists(ABORTED_FILE)), "There must be an aborted session ping.");

  // Remove the aborted session file and then shut down to make sure exceptions (e.g file
  // not found) do not compromise the shutdown.
  yield OS.File.remove(ABORTED_FILE);

  yield TelemetryController.testShutdown();
});

add_task(function* test_abortedDailyCoalescing() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session or the daily ping here.
    return;
  }

  const ABORTED_FILE = OS.Path.join(DATAREPORTING_PATH, ABORTED_PING_FILE_NAME);

  // Make sure the aborted sessions directory does not exist to test its creation.
  yield OS.File.removeDir(DATAREPORTING_PATH, { ignoreAbsent: true });

  let schedulerTickCallback = null;
  PingServer.clearRequests();

  let nowDate = new Date(2009, 10, 18, 0, 0, 0);
  fakeNow(nowDate);

  // Fake scheduler functions to control aborted-session flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetryStorage.testClearPendingPings();
  PingServer.clearRequests();
  yield TelemetryController.testReset();

  Assert.ok((yield OS.File.exists(DATAREPORTING_PATH)),
            "Telemetry must create the aborted session directory when starting.");

  // Delay the callback around midnight so that the aborted-session ping gets merged with the
  // daily ping.
  let dailyDueDate = futureDate(nowDate, MS_IN_ONE_DAY);
  fakeNow(dailyDueDate);
  // Trigger both the daily ping and the saved-session.
  Assert.ok(!!schedulerTickCallback);
  // Execute one scheduler tick.
  yield schedulerTickCallback();

  // Wait for the daily ping.
  let dailyPing = yield PingServer.promiseNextPing();
  Assert.equal(dailyPing.payload.info.reason, REASON_DAILY);

  // Check that an aborted session ping was also written to disk.
  Assert.ok((yield OS.File.exists(ABORTED_FILE)),
            "There must be an aborted session ping.");

  // Read aborted session ping and check that the session/subsession ids equal the
  // ones in the daily ping.
  let pingContent = yield OS.File.read(ABORTED_FILE, { encoding: "utf-8" });
  let abortedSessionPing = JSON.parse(pingContent);
  Assert.equal(abortedSessionPing.payload.info.sessionId, dailyPing.payload.info.sessionId);
  Assert.equal(abortedSessionPing.payload.info.subsessionId, dailyPing.payload.info.subsessionId);

  yield TelemetryController.testShutdown();
});

add_task(function* test_schedulerComputerSleep() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session or the daily ping here.
    return;
  }

  const ABORTED_FILE = OS.Path.join(DATAREPORTING_PATH, ABORTED_PING_FILE_NAME);

  yield TelemetryStorage.testClearPendingPings();
  yield TelemetryController.testReset();
  PingServer.clearRequests();

  // Remove any aborted-session ping from the previous tests.
  yield OS.File.removeDir(DATAREPORTING_PATH, { ignoreAbsent: true });

  // Set a fake current date and start Telemetry.
  let nowDate = fakeNow(2009, 10, 18, 0, 0, 0);
  let schedulerTickCallback = null;
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetryController.testReset();

  // Set the current time 3 days in the future at midnight, before running the callback.
  nowDate = fakeNow(futureDate(nowDate, 3 * MS_IN_ONE_DAY));
  Assert.ok(!!schedulerTickCallback);
  // Execute one scheduler tick.
  yield schedulerTickCallback();

  let dailyPing = yield PingServer.promiseNextPing();
  Assert.equal(dailyPing.payload.info.reason, REASON_DAILY,
               "The wake notification should have triggered a daily ping.");
  Assert.equal(dailyPing.creationDate, nowDate.toISOString(),
               "The daily ping date should be correct.");

  Assert.ok((yield OS.File.exists(ABORTED_FILE)),
            "There must be an aborted session ping.");

  // Now also test if we are sending a daily ping if we wake up on the next
  // day even when the timer doesn't trigger.
  // This can happen due to timeouts not running out during sleep times,
  // see bug 1262386, bug 1204823 et al.
  // Note that we don't get wake notifications on Linux due to bug 758848.
  nowDate = fakeNow(futureDate(nowDate, 1 * MS_IN_ONE_DAY));

  // We emulate the mentioned timeout behavior by sending the wake notification
  // instead of triggering the timeout callback.
  // This should trigger a daily ping, because we passed midnight.
  Services.obs.notifyObservers(null, "wake_notification", null);

  dailyPing = yield PingServer.promiseNextPing();
  Assert.equal(dailyPing.payload.info.reason, REASON_DAILY,
               "The wake notification should have triggered a daily ping.");
  Assert.equal(dailyPing.creationDate, nowDate.toISOString(),
               "The daily ping date should be correct.");

  yield TelemetryController.testShutdown();
});

add_task(function* test_schedulerEnvironmentReschedules() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session or the daily ping here.
    return;
  }

  // Reset the test preference.
  const PREF_TEST = "toolkit.telemetry.test.pref1";
  Preferences.reset(PREF_TEST);
  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, {what: TelemetryEnvironment.RECORD_PREF_VALUE}],
  ]);

  yield TelemetryStorage.testClearPendingPings();
  PingServer.clearRequests();
  yield TelemetryController.testReset();

  // Set a fake current date and start Telemetry.
  let nowDate = fakeNow(2060, 10, 18, 0, 0, 0);
  gMonotonicNow = fakeMonotonicNow(gMonotonicNow + 10 * MILLISECONDS_PER_MINUTE);
  let schedulerTickCallback = null;
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetryController.testReset();
  TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);

  // Set the current time at midnight.
  let future = fakeNow(futureDate(nowDate, MS_IN_ONE_DAY));
  gMonotonicNow = fakeMonotonicNow(gMonotonicNow + 10 * MILLISECONDS_PER_MINUTE);

  // Trigger the environment change.
  Preferences.set(PREF_TEST, 1);

  // Wait for the environment-changed ping.
  yield PingServer.promiseNextPing();

  // We don't expect to receive any daily ping in this test, so assert if we do.
  PingServer.registerPingHandler((req, res) => {
    Assert.ok(false, "No ping should be sent/received in this test.");
  });

  // Execute one scheduler tick. It should not trigger a daily ping.
  Assert.ok(!!schedulerTickCallback);
  yield schedulerTickCallback();
  yield TelemetryController.testShutdown();
});

add_task(function* test_schedulerNothingDue() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session or the daily ping here.
    return;
  }

  const ABORTED_FILE = OS.Path.join(DATAREPORTING_PATH, ABORTED_PING_FILE_NAME);

  // Remove any aborted-session ping from the previous tests.
  yield OS.File.removeDir(DATAREPORTING_PATH, { ignoreAbsent: true });
  yield TelemetryStorage.testClearPendingPings();
  yield TelemetryController.testReset();

  // We don't expect to receive any ping in this test, so assert if we do.
  PingServer.registerPingHandler((req, res) => {
    Assert.ok(false, "No ping should be sent/received in this test.");
  });

  // Set a current date/time away from midnight, so that the daily ping doesn't get
  // sent.
  let nowDate = new Date(2009, 10, 18, 11, 0, 0);
  fakeNow(nowDate);
  let schedulerTickCallback = null;
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetryController.testReset();

  // Delay the callback execution to a time when no ping should be due.
  let nothingDueDate = futureDate(nowDate, ABORTED_SESSION_UPDATE_INTERVAL_MS / 2);
  fakeNow(nothingDueDate);
  Assert.ok(!!schedulerTickCallback);
  // Execute one scheduler tick.
  yield schedulerTickCallback();

  // Check that no aborted session ping was written to disk.
  Assert.ok(!(yield OS.File.exists(ABORTED_FILE)));

  yield TelemetryController.testShutdown();
  PingServer.resetPingHandler();
});

add_task(function* test_pingExtendedStats() {
  const EXTENDED_PAYLOAD_FIELDS = [
    "chromeHangs", "threadHangStats", "log", "slowSQL", "fileIOReports", "lateWrites",
    "addonHistograms", "addonDetails", "UIMeasurements", "webrtc"
  ];

  // Reset telemetry and disable sending extended statistics.
  yield TelemetryStorage.testClearPendingPings();
  PingServer.clearRequests();
  yield TelemetryController.testReset();
  Telemetry.canRecordExtended = false;

  yield sendPing();

  let ping = yield PingServer.promiseNextPing();
  checkPingFormat(ping, PING_TYPE_MAIN, true, true);

  // Check that the payload does not contain extended statistics fields.
  for (let f in EXTENDED_PAYLOAD_FIELDS) {
    Assert.ok(!(EXTENDED_PAYLOAD_FIELDS[f] in ping.payload),
              EXTENDED_PAYLOAD_FIELDS[f] + " must not be in the payload if the extended set is off.");
  }

  // We check this one separately so that we can reuse EXTENDED_PAYLOAD_FIELDS below, since
  // slowSQLStartup might not be there.
  Assert.ok(!("slowSQLStartup" in ping.payload),
            "slowSQLStartup must not be sent if the extended set is off");

  Assert.ok(!("addonManager" in ping.payload.simpleMeasurements),
            "addonManager must not be sent if the extended set is off.");
  Assert.ok(!("UITelemetry" in ping.payload.simpleMeasurements),
            "UITelemetry must not be sent if the extended set is off.");

  // Restore the preference.
  Telemetry.canRecordExtended = true;

  // Send a new ping that should contain the extended data.
  yield sendPing();
  ping = yield PingServer.promiseNextPing();
  checkPingFormat(ping, PING_TYPE_MAIN, true, true);

  // Check that the payload now contains extended statistics fields.
  for (let f in EXTENDED_PAYLOAD_FIELDS) {
    Assert.ok(EXTENDED_PAYLOAD_FIELDS[f] in ping.payload,
              EXTENDED_PAYLOAD_FIELDS[f] + " must be in the payload if the extended set is on.");
  }

  Assert.ok("addonManager" in ping.payload.simpleMeasurements,
            "addonManager must be sent if the extended set is on.");
  Assert.ok("UITelemetry" in ping.payload.simpleMeasurements,
            "UITelemetry must be sent if the extended set is on.");
});

add_task(function* test_schedulerUserIdle() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session or the daily ping here.
    return;
  }

  const SCHEDULER_TICK_INTERVAL_MS = 5 * 60 * 1000;
  const SCHEDULER_TICK_IDLE_INTERVAL_MS = 60 * 60 * 1000;

  let now = new Date(2010, 1, 1, 11, 0, 0);
  fakeNow(now);

  let schedulerTimeout = 0;
  fakeSchedulerTimer((callback, timeout) => {
    schedulerTimeout = timeout;
  }, () => {});
  yield TelemetryController.testReset();
  yield TelemetryStorage.testClearPendingPings();
  PingServer.clearRequests();

  // When not idle, the scheduler should have a 5 minutes tick interval.
  Assert.equal(schedulerTimeout, SCHEDULER_TICK_INTERVAL_MS);

  // Send an "idle" notification to the scheduler.
  fakeIdleNotification("idle");

  // When idle, the scheduler should have a 1hr tick interval.
  Assert.equal(schedulerTimeout, SCHEDULER_TICK_IDLE_INTERVAL_MS);

  // Send an "active" notification to the scheduler.
  fakeIdleNotification("active");

  // When user is back active, the scheduler tick should be 5 minutes again.
  Assert.equal(schedulerTimeout, SCHEDULER_TICK_INTERVAL_MS);

  // We should not miss midnight when going to idle.
  now.setHours(23);
  now.setMinutes(50);
  fakeNow(now);
  fakeIdleNotification("idle");
  Assert.equal(schedulerTimeout, 10 * 60 * 1000);

  yield TelemetryController.testShutdown();
});

add_task(function* test_DailyDueAndIdle() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session or the daily ping here.
    return;
  }

  yield TelemetryStorage.testClearPendingPings();
  PingServer.clearRequests();

  let receivedPingRequest = null;
  // Register a ping handler that will assert when receiving multiple daily pings.
  PingServer.registerPingHandler(req => {
    Assert.ok(!receivedPingRequest, "Telemetry must only send one daily ping.");
    receivedPingRequest = req;
  });

  // Faking scheduler timer has to happen before resetting TelemetryController
  // to be effective.
  let schedulerTickCallback = null;
  let now = new Date(2030, 1, 1, 0, 0, 0);
  fakeNow(now);
  // Fake scheduler functions to control daily collection flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetryController.testReset();

  // Trigger the daily ping.
  let firstDailyDue = new Date(2030, 1, 2, 0, 0, 0);
  fakeNow(firstDailyDue);

  // Run a scheduler tick: it should trigger the daily ping.
  Assert.ok(!!schedulerTickCallback);
  let tickPromise = schedulerTickCallback();

  // Send an idle and then an active user notification.
  fakeIdleNotification("idle");
  fakeIdleNotification("active");

  // Wait on the tick promise.
  yield tickPromise;

  yield TelemetrySend.testWaitOnOutgoingPings();

  // Decode the ping contained in the request and check that's a daily ping.
  Assert.ok(receivedPingRequest, "Telemetry must send one daily ping.");
  const receivedPing = decodeRequestPayload(receivedPingRequest);
  checkPingFormat(receivedPing, PING_TYPE_MAIN, true, true);
  Assert.equal(receivedPing.payload.info.reason, REASON_DAILY);

  yield TelemetryController.testShutdown();
});

add_task(function* test_userIdleAndSchedlerTick() {
  if (gIsAndroid || gIsGonk) {
    // We don't have the aborted session or the daily ping here.
    return;
  }

  let receivedPingRequest = null;
  // Register a ping handler that will assert when receiving multiple daily pings.
  PingServer.registerPingHandler(req => {
    Assert.ok(!receivedPingRequest, "Telemetry must only send one daily ping.");
    receivedPingRequest = req;
  });

  let schedulerTickCallback = null;
  let now = new Date(2030, 1, 1, 0, 0, 0);
  fakeNow(now);
  // Fake scheduler functions to control daily collection flow in tests.
  fakeSchedulerTimer(callback => schedulerTickCallback = callback, () => {});
  yield TelemetryStorage.testClearPendingPings();
  yield TelemetryController.testReset();
  PingServer.clearRequests();

  // Move the current date/time to midnight.
  let firstDailyDue = new Date(2030, 1, 2, 0, 0, 0);
  fakeNow(firstDailyDue);

  // The active notification should trigger a scheduler tick. The latter will send the
  // due daily ping.
  fakeIdleNotification("active");

  // Immediately running another tick should not send a daily ping again.
  Assert.ok(!!schedulerTickCallback);
  yield schedulerTickCallback();

  // A new "idle" notification should not send a new daily ping.
  fakeIdleNotification("idle");

  yield TelemetrySend.testWaitOnOutgoingPings();

  // Decode the ping contained in the request and check that's a daily ping.
  Assert.ok(receivedPingRequest, "Telemetry must send one daily ping.");
  const receivedPing = decodeRequestPayload(receivedPingRequest);
  checkPingFormat(receivedPing, PING_TYPE_MAIN, true, true);
  Assert.equal(receivedPing.payload.info.reason, REASON_DAILY);

  PingServer.resetPingHandler();
  yield TelemetryController.testShutdown();
});

add_task(function* test_changeThrottling() {
  if (gIsAndroid) {
    // We don't support subsessions yet on Android.
    return;
  }

  let getSubsessionCount = () => {
    return TelemetrySession.getPayload().info.subsessionCounter;
  };

  const PREF_TEST = "toolkit.telemetry.test.pref1";
  const PREFS_TO_WATCH = new Map([
    [PREF_TEST, {what: TelemetryEnvironment.RECORD_PREF_STATE}],
  ]);
  Preferences.reset(PREF_TEST);

  let now = fakeNow(2050, 1, 2, 0, 0, 0);
  gMonotonicNow = fakeMonotonicNow(gMonotonicNow + 10 * MILLISECONDS_PER_MINUTE);
  yield TelemetryController.testReset();
  Assert.equal(getSubsessionCount(), 1);

  // Set the Environment preferences to watch.
  TelemetryEnvironment.testWatchPreferences(PREFS_TO_WATCH);

  // The first pref change should not trigger a notification.
  Preferences.set(PREF_TEST, 1);
  Assert.equal(getSubsessionCount(), 1);

  // We should get a change notification after the 5min throttling interval.
  now = fakeNow(futureDate(now, 5 * MILLISECONDS_PER_MINUTE + 1));
  gMonotonicNow = fakeMonotonicNow(gMonotonicNow + 5 * MILLISECONDS_PER_MINUTE + 1);
  Preferences.set(PREF_TEST, 2);
  Assert.equal(getSubsessionCount(), 2);

  // After that, changes should be throttled again.
  now = fakeNow(futureDate(now, 1 * MILLISECONDS_PER_MINUTE));
  gMonotonicNow = fakeMonotonicNow(gMonotonicNow + 1 * MILLISECONDS_PER_MINUTE);
  Preferences.set(PREF_TEST, 3);
  Assert.equal(getSubsessionCount(), 2);

  // ... for 5min.
  now = fakeNow(futureDate(now, 4 * MILLISECONDS_PER_MINUTE + 1));
  gMonotonicNow = fakeMonotonicNow(gMonotonicNow + 4 * MILLISECONDS_PER_MINUTE + 1);
  Preferences.set(PREF_TEST, 4);
  Assert.equal(getSubsessionCount(), 3);

  // Unregister the listener.
  TelemetryEnvironment.unregisterChangeListener("testWatchPrefs_throttling");
});

add_task(function* stopServer() {
  yield PingServer.stop();
});
