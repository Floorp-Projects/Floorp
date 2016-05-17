/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/

/**
 * This test case populates the profile with some fake stored
 * pings, and checks that pending pings are immediatlely sent
 * after delayed init.
 */

"use strict"

Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/TelemetryStorage.jsm", this);
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetrySend.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
var {OS: {File, Path, Constants}} = Cu.import("resource://gre/modules/osfile.jsm", {});

// We increment TelemetryStorage's MAX_PING_FILE_AGE and
// OVERDUE_PING_FILE_AGE by 1 minute so that our test pings exceed
// those points in time, even taking into account file system imprecision.
const ONE_MINUTE_MS = 60 * 1000;
const OVERDUE_PING_FILE_AGE = TelemetrySend.OVERDUE_PING_FILE_AGE + ONE_MINUTE_MS;

const PING_SAVE_FOLDER = "saved-telemetry-pings";
const PING_TIMEOUT_LENGTH = 5000;
const OVERDUE_PINGS = 6;
const OLD_FORMAT_PINGS = 4;
const RECENT_PINGS = 4;

const TOTAL_EXPECTED_PINGS = OVERDUE_PINGS + RECENT_PINGS + OLD_FORMAT_PINGS;

const PREF_FHR_UPLOAD = "datareporting.healthreport.uploadEnabled";

var gCreatedPings = 0;
var gSeenPings = 0;

/**
 * Creates some Telemetry pings for the and saves them to disk. Each ping gets a
 * unique ID based on an incrementor.
 *
 * @param {Array} aPingInfos An array of ping type objects. Each entry must be an
 *                object containing a "num" field for the number of pings to create and
 *                an "age" field. The latter representing the age in milliseconds to offset
 *                from now. A value of 10 would make the ping 10ms older than now, for
 *                example.
 * @returns Promise
 * @resolve an Array with the created pings ids.
 */
var createSavedPings = Task.async(function* (aPingInfos) {
  let pingIds = [];
  let now = Date.now();

  for (let type in aPingInfos) {
    let num = aPingInfos[type].num;
    let age = now - (aPingInfos[type].age || 0);
    for (let i = 0; i < num; ++i) {
      let pingId = yield TelemetryController.addPendingPing("test-ping", {}, { overwrite: true });
      if (aPingInfos[type].age) {
        // savePing writes to the file synchronously, so we're good to
        // modify the lastModifedTime now.
        let filePath = getSavePathForPingId(pingId);
        yield File.setDates(filePath, null, age);
      }
      gCreatedPings++;
      pingIds.push(pingId);
    }
  }

  return pingIds;
});

/**
 * Deletes locally saved pings if they exist.
 *
 * @param aPingIds an Array of ping ids to delete.
 * @returns Promise
 */
var clearPings = Task.async(function* (aPingIds) {
  for (let pingId of aPingIds) {
    yield TelemetryStorage.removePendingPing(pingId);
  }
});

/**
 * Fakes the pending pings storage quota.
 * @param {Integer} aPendingQuota The new quota, in bytes.
 */
function fakePendingPingsQuota(aPendingQuota) {
  let storage = Cu.import("resource://gre/modules/TelemetryStorage.jsm");
  storage.Policy.getPendingPingsQuota = () => aPendingQuota;
}

/**
 * Returns a handle for the file that a ping should be
 * stored in locally.
 *
 * @returns path
 */
function getSavePathForPingId(aPingId) {
  return Path.join(Constants.Path.profileDir, PING_SAVE_FOLDER, aPingId);
}

/**
 * Check if the number of Telemetry pings received by the HttpServer is not equal
 * to aExpectedNum.
 *
 * @param aExpectedNum the number of pings we expect to receive.
 */
function assertReceivedPings(aExpectedNum) {
  do_check_eq(gSeenPings, aExpectedNum);
}

/**
 * Throws if any pings with the id in aPingIds is saved locally.
 *
 * @param aPingIds an Array of pings ids to check.
 * @returns Promise
 */
var assertNotSaved = Task.async(function* (aPingIds) {
  let saved = 0;
  for (let id of aPingIds) {
    let filePath = getSavePathForPingId(id);
    if (yield File.exists(filePath)) {
      saved++;
    }
  }
  if (saved > 0) {
    do_throw("Found " + saved + " unexpected saved pings.");
  }
});

/**
 * Our handler function for the HttpServer that simply
 * increments the gSeenPings global when it successfully
 * receives and decodes a Telemetry payload.
 *
 * @param aRequest the HTTP request sent from HttpServer.
 */
function pingHandler(aRequest) {
  gSeenPings++;
}

function run_test() {
  PingServer.start();
  PingServer.registerPingHandler(pingHandler);
  do_get_profile();
  loadAddonManager("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
  Services.prefs.setCharPref(TelemetryController.Constants.PREF_SERVER,
                             "http://localhost:" + PingServer.port);
  run_next_test();
}

/**
 * Setup the tests by making sure the ping storage directory is available, otherwise
 * |TelemetryController.testSaveDirectoryToFile| could fail.
 */
add_task(function* setupEnvironment() {
  // The following tests assume this pref to be true by default.
  Services.prefs.setBoolPref(PREF_FHR_UPLOAD, true);

  yield TelemetryController.testSetup();

  let directory = TelemetryStorage.pingDirectoryPath;
  yield File.makeDir(directory, { ignoreExisting: true, unixMode: OS.Constants.S_IRWXU });

  yield TelemetryStorage.testClearPendingPings();
});

/**
 * Test that really recent pings are sent on Telemetry initialization.
 */
add_task(function* test_recent_pings_sent() {
  let pingTypes = [{ num: RECENT_PINGS }];
  let recentPings = yield createSavedPings(pingTypes);

  yield TelemetryController.testReset();
  yield TelemetrySend.testWaitOnOutgoingPings();
  assertReceivedPings(RECENT_PINGS);

  yield TelemetryStorage.testClearPendingPings();
});

/**
 * Create an overdue ping in the old format and try to send it.
 */
add_task(function* test_overdue_old_format() {
  // A test ping in the old, standard format.
  const PING_OLD_FORMAT = {
    slug: "1234567abcd",
    reason: "test-ping",
    payload: {
      info: {
        reason: "test-ping",
        OS: "XPCShell",
        appID: "SomeId",
        appVersion: "1.0",
        appName: "XPCShell",
        appBuildID: "123456789",
        appUpdateChannel: "Test",
        platformBuildID: "987654321",
      },
    },
  };

  // A ping with no info section, but with a slug.
  const PING_NO_INFO = {
    slug: "1234-no-info-ping",
    reason: "test-ping",
    payload: {}
  };

  // A ping with no payload.
  const PING_NO_PAYLOAD = {
    slug: "5678-no-payload",
    reason: "test-ping",
  };

  // A ping with no info and no slug.
  const PING_NO_SLUG = {
    reason: "test-ping",
    payload: {}
  };

  const PING_FILES_PATHS = [
    getSavePathForPingId(PING_OLD_FORMAT.slug),
    getSavePathForPingId(PING_NO_INFO.slug),
    getSavePathForPingId(PING_NO_PAYLOAD.slug),
    getSavePathForPingId("no-slug-file"),
  ];

  // Write the ping to file and make it overdue.
  yield TelemetryStorage.savePing(PING_OLD_FORMAT, true);
  yield TelemetryStorage.savePing(PING_NO_INFO, true);
  yield TelemetryStorage.savePing(PING_NO_PAYLOAD, true);
  yield TelemetryStorage.savePingToFile(PING_NO_SLUG, PING_FILES_PATHS[3], true);

  for (let f in PING_FILES_PATHS) {
    yield File.setDates(PING_FILES_PATHS[f], null, Date.now() - OVERDUE_PING_FILE_AGE);
  }

  gSeenPings = 0;
  yield TelemetryController.testReset();
  yield TelemetrySend.testWaitOnOutgoingPings();
  assertReceivedPings(OLD_FORMAT_PINGS);

  // |TelemetryStorage.cleanup| doesn't know how to remove a ping with no slug or id,
  // so remove it manually so that the next test doesn't fail.
  yield OS.File.remove(PING_FILES_PATHS[3]);

  yield TelemetryStorage.testClearPendingPings();
});

add_task(function* test_corrupted_pending_pings() {
  const TEST_TYPE = "test_corrupted";

  Telemetry.getHistogramById("TELEMETRY_PENDING_LOAD_FAILURE_READ").clear();
  Telemetry.getHistogramById("TELEMETRY_PENDING_LOAD_FAILURE_PARSE").clear();

  // Save a pending ping and get its id.
  let pendingPingId = yield TelemetryController.addPendingPing(TEST_TYPE, {}, {});

  // Try to load it: there should be no error.
  yield TelemetryStorage.loadPendingPing(pendingPingId);

  let h = Telemetry.getHistogramById("TELEMETRY_PENDING_LOAD_FAILURE_READ").snapshot();
  Assert.equal(h.sum, 0, "Telemetry must not report a pending ping load failure");
  h = Telemetry.getHistogramById("TELEMETRY_PENDING_LOAD_FAILURE_PARSE").snapshot();
  Assert.equal(h.sum, 0, "Telemetry must not report a pending ping parse failure");

  // Delete it from the disk, so that its id will be kept in the cache but it will
  // fail loading the file.
  yield OS.File.remove(getSavePathForPingId(pendingPingId));

  // Try to load a pending ping which isn't there anymore.
  yield Assert.rejects(TelemetryStorage.loadPendingPing(pendingPingId),
                       "Telemetry must fail loading a ping which isn't there");

  h = Telemetry.getHistogramById("TELEMETRY_PENDING_LOAD_FAILURE_READ").snapshot();
  Assert.equal(h.sum, 1, "Telemetry must report a pending ping load failure");
  h = Telemetry.getHistogramById("TELEMETRY_PENDING_LOAD_FAILURE_PARSE").snapshot();
  Assert.equal(h.sum, 0, "Telemetry must not report a pending ping parse failure");

  // Save a new ping, so that it gets in the pending pings cache.
  pendingPingId = yield TelemetryController.addPendingPing(TEST_TYPE, {}, {});
  // Overwrite it with a corrupted JSON file and then try to load it.
  const INVALID_JSON = "{ invalid,JSON { {1}";
  yield OS.File.writeAtomic(getSavePathForPingId(pendingPingId), INVALID_JSON, { encoding: "utf-8" });

  // Try to load the ping with the corrupted JSON content.
  yield Assert.rejects(TelemetryStorage.loadPendingPing(pendingPingId),
                       "Telemetry must fail loading a corrupted ping");

  h = Telemetry.getHistogramById("TELEMETRY_PENDING_LOAD_FAILURE_READ").snapshot();
  Assert.equal(h.sum, 1, "Telemetry must report a pending ping load failure");
  h = Telemetry.getHistogramById("TELEMETRY_PENDING_LOAD_FAILURE_PARSE").snapshot();
  Assert.equal(h.sum, 1, "Telemetry must report a pending ping parse failure");

  let exists = yield OS.File.exists(getSavePathForPingId(pendingPingId));
  Assert.ok(!exists, "The unparseable ping should have been removed");

  yield TelemetryStorage.testClearPendingPings();
});

/**
 * Create some recent and overdue pings and verify that they get sent.
 */
add_task(function* test_overdue_pings_trigger_send() {
  let pingTypes = [
    { num: RECENT_PINGS },
    { num: OVERDUE_PINGS, age: OVERDUE_PING_FILE_AGE },
  ];
  let pings = yield createSavedPings(pingTypes);
  let recentPings = pings.slice(0, RECENT_PINGS);
  let overduePings = pings.slice(-OVERDUE_PINGS);

  yield TelemetryController.testReset();
  yield TelemetrySend.testWaitOnOutgoingPings();
  assertReceivedPings(TOTAL_EXPECTED_PINGS);

  yield assertNotSaved(recentPings);
  yield assertNotSaved(overduePings);

  Assert.equal(TelemetrySend.overduePingsCount, overduePings.length,
               "Should have tracked the correct amount of overdue pings");

  yield TelemetryStorage.testClearPendingPings();
});

/**
 * Create a ping in the old format, send it, and make sure the request URL contains
 * the correct version query parameter.
 */
add_task(function* test_overdue_old_format() {
  // A test ping in the old, standard format.
  const PING_OLD_FORMAT = {
    slug: "1234567abcd",
    reason: "test-ping",
    payload: {
      info: {
        reason: "test-ping",
        OS: "XPCShell",
        appID: "SomeId",
        appVersion: "1.0",
        appName: "XPCShell",
        appBuildID: "123456789",
        appUpdateChannel: "Test",
        platformBuildID: "987654321",
      },
    },
  };

  const filePath =
    Path.join(Constants.Path.profileDir, PING_SAVE_FOLDER, PING_OLD_FORMAT.slug);

  // Write the ping to file and make it overdue.
  yield TelemetryStorage.savePing(PING_OLD_FORMAT, true);
  yield File.setDates(filePath, null, Date.now() - OVERDUE_PING_FILE_AGE);

  let receivedPings = 0;
  // Register a new prefix handler to validate the URL.
  PingServer.registerPingHandler(request => {
    // Check that we have a version query parameter in the URL.
    Assert.notEqual(request.queryString, "");

    // Make sure the version in the query string matches the old ping format version.
    let params = request.queryString.split("&");
    Assert.ok(params.find(p => p == "v=1"));

    receivedPings++;
  });

  yield TelemetryController.testReset();
  yield TelemetrySend.testWaitOnOutgoingPings();
  Assert.equal(receivedPings, 1, "We must receive a ping in the old format.");

  yield TelemetryStorage.testClearPendingPings();
  PingServer.resetPingHandler();
});

add_task(function* test_pendingPingsQuota() {
  const PING_TYPE = "foo";

  // Disable upload so pings don't get sent and removed from the pending pings directory.
  Services.prefs.setBoolPref(PREF_FHR_UPLOAD, false);

  // Remove all the pending pings then startup and wait for the cleanup task to complete.
  // There should be nothing to remove.
  yield TelemetryStorage.testClearPendingPings();
  yield TelemetryController.testReset();
  yield TelemetrySend.testWaitOnOutgoingPings();
  yield TelemetryStorage.testPendingQuotaTaskPromise();

  // Remove the pending deletion ping generated when flipping FHR upload off.
  yield TelemetryStorage.testClearPendingPings();

  let expectedPrunedPings = [];
  let expectedNotPrunedPings = [];

  let checkPendingPings = Task.async(function*() {
    // Check that the pruned pings are not on disk anymore.
    for (let prunedPingId of expectedPrunedPings) {
      yield Assert.rejects(TelemetryStorage.loadPendingPing(prunedPingId),
                           "Ping " + prunedPingId + " should have been pruned.");
      const pingPath = getSavePathForPingId(prunedPingId);
      Assert.ok(!(yield OS.File.exists(pingPath)), "The ping should not be on the disk anymore.");
    }

    // Check that the expected pings are there.
    for (let expectedPingId of expectedNotPrunedPings) {
      Assert.ok((yield TelemetryStorage.loadPendingPing(expectedPingId)),
                "Ping" + expectedPingId + " should be among the pending pings.");
    }
  });

  let pendingPingsInfo = [];
  let pingsSizeInBytes = 0;

  // Create 10 pings to test the pending pings quota.
  for (let days = 1; days < 11; days++) {
    const date = fakeNow(2010, 1, days, 1, 1, 0);
    const pingId = yield TelemetryController.addPendingPing(PING_TYPE, {}, {});

    // Find the size of the ping.
    const pingFilePath = getSavePathForPingId(pingId);
    const pingSize = (yield OS.File.stat(pingFilePath)).size;
    // Add the info at the beginning of the array, so that most recent pings come first.
    pendingPingsInfo.unshift({id: pingId, size: pingSize, timestamp: date.getTime() });

    // Set the last modification date.
    yield OS.File.setDates(pingFilePath, null, date.getTime());

    // Add it to the pending ping directory size.
    pingsSizeInBytes += pingSize;
  }

  // We need to test the pending pings size before we hit the quota, otherwise a special
  // value is recorded.
  Telemetry.getHistogramById("TELEMETRY_PENDING_PINGS_SIZE_MB").clear();
  Telemetry.getHistogramById("TELEMETRY_PENDING_PINGS_EVICTED_OVER_QUOTA").clear();
  Telemetry.getHistogramById("TELEMETRY_PENDING_EVICTING_OVER_QUOTA_MS").clear();

  yield TelemetryController.testReset();
  yield TelemetryStorage.testPendingQuotaTaskPromise();

  // Check that the correct values for quota probes are reported when no quota is hit.
  let h = Telemetry.getHistogramById("TELEMETRY_PENDING_PINGS_SIZE_MB").snapshot();
  Assert.equal(h.sum, Math.round(pingsSizeInBytes / 1024 / 1024),
               "Telemetry must report the correct pending pings directory size.");
  h = Telemetry.getHistogramById("TELEMETRY_PENDING_PINGS_EVICTED_OVER_QUOTA").snapshot();
  Assert.equal(h.sum, 0, "Telemetry must report 0 evictions if quota is not hit.");
  h = Telemetry.getHistogramById("TELEMETRY_PENDING_EVICTING_OVER_QUOTA_MS").snapshot();
  Assert.equal(h.sum, 0, "Telemetry must report a null elapsed time if quota is not hit.");

  // Set the quota to 80% of the space.
  const testQuotaInBytes = pingsSizeInBytes * 0.8;
  fakePendingPingsQuota(testQuotaInBytes);

  // The storage prunes pending pings until we reach 90% of the requested storage quota.
  // Based on that, find how many pings should be kept.
  const safeQuotaSize = Math.round(testQuotaInBytes * 0.9);
  let sizeInBytes = 0;
  let pingsWithinQuota = [];
  let pingsOutsideQuota = [];

  for (let pingInfo of pendingPingsInfo) {
    sizeInBytes += pingInfo.size;
    if (sizeInBytes >= safeQuotaSize) {
      pingsOutsideQuota.push(pingInfo.id);
      continue;
    }
    pingsWithinQuota.push(pingInfo.id);
  }

  expectedNotPrunedPings = pingsWithinQuota;
  expectedPrunedPings = pingsOutsideQuota;

  // Reset TelemetryController to start the pending pings cleanup.
  yield TelemetryController.testReset();
  yield TelemetryStorage.testPendingQuotaTaskPromise();
  yield checkPendingPings();

  h = Telemetry.getHistogramById("TELEMETRY_PENDING_PINGS_EVICTED_OVER_QUOTA").snapshot();
  Assert.equal(h.sum, pingsOutsideQuota.length,
               "Telemetry must correctly report the over quota pings evicted from the pending pings directory.");
  h = Telemetry.getHistogramById("TELEMETRY_PENDING_PINGS_SIZE_MB").snapshot();
  Assert.equal(h.sum, 17, "Pending pings quota was hit, a special size must be reported.");

  // Trigger a cleanup again and make sure we're not removing anything.
  yield TelemetryController.testReset();
  yield TelemetryStorage.testPendingQuotaTaskPromise();
  yield checkPendingPings();

  const OVERSIZED_PING_ID = "9b21ec8f-f762-4d28-a2c1-44e1c4694f24";
  // Create a pending oversized ping.
  const OVERSIZED_PING = {
    id: OVERSIZED_PING_ID,
    type: PING_TYPE,
    creationDate: (new Date()).toISOString(),
    // Generate a 2MB string to use as the ping payload.
    payload: generateRandomString(2 * 1024 * 1024),
  };
  yield TelemetryStorage.savePendingPing(OVERSIZED_PING);

  // Reset the histograms.
  Telemetry.getHistogramById("TELEMETRY_PING_SIZE_EXCEEDED_PENDING").clear();
  Telemetry.getHistogramById("TELEMETRY_DISCARDED_PENDING_PINGS_SIZE_MB").clear();

  // Try to manually load the oversized ping.
  yield Assert.rejects(TelemetryStorage.loadPendingPing(OVERSIZED_PING_ID),
                       "The oversized ping should have been pruned.");
  Assert.ok(!(yield OS.File.exists(getSavePathForPingId(OVERSIZED_PING_ID))),
            "The ping should not be on the disk anymore.");

  // Make sure we're correctly updating the related histograms.
  h = Telemetry.getHistogramById("TELEMETRY_PING_SIZE_EXCEEDED_PENDING").snapshot();
  Assert.equal(h.sum, 1, "Telemetry must report 1 oversized ping in the pending pings directory.");
  h = Telemetry.getHistogramById("TELEMETRY_DISCARDED_PENDING_PINGS_SIZE_MB").snapshot();
  Assert.equal(h.counts[2], 1, "Telemetry must report a 2MB, oversized, ping.");

  // Save the ping again to check if it gets pruned when scanning the pings directory.
  yield TelemetryStorage.savePendingPing(OVERSIZED_PING);
  expectedPrunedPings.push(OVERSIZED_PING_ID);

  // Scan the pending pings directory.
  yield TelemetryController.testReset();
  yield TelemetryStorage.testPendingQuotaTaskPromise();
  yield checkPendingPings();

  // Make sure we're correctly updating the related histograms.
  h = Telemetry.getHistogramById("TELEMETRY_PING_SIZE_EXCEEDED_PENDING").snapshot();
  Assert.equal(h.sum, 2, "Telemetry must report 1 oversized ping in the pending pings directory.");
  h = Telemetry.getHistogramById("TELEMETRY_DISCARDED_PENDING_PINGS_SIZE_MB").snapshot();
  Assert.equal(h.counts[2], 2, "Telemetry must report two 2MB, oversized, pings.");

  Services.prefs.setBoolPref(PREF_FHR_UPLOAD, true);
});

add_task(function* teardown() {
  yield PingServer.stop();
});
