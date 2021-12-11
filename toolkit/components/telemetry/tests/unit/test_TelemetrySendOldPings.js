/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/

/**
 * This test case populates the profile with some fake stored
 * pings, and checks that pending pings are immediatlely sent
 * after delayed init.
 */

"use strict";

const {
  OS: { File, Path, Constants },
} = ChromeUtils.import("resource://gre/modules/osfile.jsm");

const PING_SAVE_FOLDER = "saved-telemetry-pings";
const PING_TIMEOUT_LENGTH = 5000;
const OLD_FORMAT_PINGS = 4;
const RECENT_PINGS = 4;

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
var createSavedPings = async function(aPingInfos) {
  let pingIds = [];
  let now = Date.now();

  for (let type in aPingInfos) {
    let num = aPingInfos[type].num;
    let age = now - (aPingInfos[type].age || 0);
    for (let i = 0; i < num; ++i) {
      let pingId = await TelemetryController.addPendingPing(
        "test-ping",
        {},
        { overwrite: true }
      );
      if (aPingInfos[type].age) {
        // savePing writes to the file synchronously, so we're good to
        // modify the lastModifedTime now.
        let filePath = getSavePathForPingId(pingId);
        await File.setDates(filePath, null, age);
      }
      gCreatedPings++;
      pingIds.push(pingId);
    }
  }

  return pingIds;
};

/**
 * Deletes locally saved pings if they exist.
 *
 * @param aPingIds an Array of ping ids to delete.
 * @returns Promise
 */
var clearPings = async function(aPingIds) {
  for (let pingId of aPingIds) {
    await TelemetryStorage.removePendingPing(pingId);
  }
};

/**
 * Fakes the pending pings storage quota.
 * @param {Integer} aPendingQuota The new quota, in bytes.
 */
function fakePendingPingsQuota(aPendingQuota) {
  let { Policy } = ChromeUtils.import(
    "resource://gre/modules/TelemetryStorage.jsm"
  );
  Policy.getPendingPingsQuota = () => aPendingQuota;
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
  Assert.equal(gSeenPings, aExpectedNum);
}

/**
 * Throws if any pings with the id in aPingIds is saved locally.
 *
 * @param aPingIds an Array of pings ids to check.
 * @returns Promise
 */
var assertNotSaved = async function(aPingIds) {
  let saved = 0;
  for (let id of aPingIds) {
    let filePath = getSavePathForPingId(id);
    if (await File.exists(filePath)) {
      saved++;
    }
  }
  if (saved > 0) {
    do_throw("Found " + saved + " unexpected saved pings.");
  }
};

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

add_task(async function test_setup() {
  PingServer.start();
  PingServer.registerPingHandler(pingHandler);
  do_get_profile();
  await loadAddonManager(
    "xpcshell@tests.mozilla.org",
    "XPCShell",
    "1",
    "1.9.2"
  );
  finishAddonManagerStartup();
  fakeIntlReady();
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  Services.prefs.setCharPref(
    TelemetryUtils.Preferences.Server,
    "http://localhost:" + PingServer.port
  );
});

/**
 * Setup the tests by making sure the ping storage directory is available, otherwise
 * |TelemetryController.testSaveDirectoryToFile| could fail.
 */
add_task(async function setupEnvironment() {
  // The following tests assume this pref to be true by default.
  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  await TelemetryController.testSetup();

  let directory = TelemetryStorage.pingDirectoryPath;
  await File.makeDir(directory, {
    ignoreExisting: true,
    unixMode: OS.Constants.S_IRWXU,
  });

  await TelemetryStorage.testClearPendingPings();
});

/**
 * Test that really recent pings are sent on Telemetry initialization.
 */
add_task(async function test_recent_pings_sent() {
  let pingTypes = [{ num: RECENT_PINGS }];
  await createSavedPings(pingTypes);

  await TelemetryController.testReset();
  await TelemetrySend.testWaitOnOutgoingPings();
  assertReceivedPings(RECENT_PINGS);

  await TelemetryStorage.testClearPendingPings();
});

/**
 * Create an overdue ping in the old format and try to send it.
 */
add_task(async function test_old_formats() {
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
    payload: {},
  };

  // A ping with no payload.
  const PING_NO_PAYLOAD = {
    slug: "5678-no-payload",
    reason: "test-ping",
  };

  // A ping with no info and no slug.
  const PING_NO_SLUG = {
    reason: "test-ping",
    payload: {},
  };

  const PING_FILES_PATHS = [
    getSavePathForPingId(PING_OLD_FORMAT.slug),
    getSavePathForPingId(PING_NO_INFO.slug),
    getSavePathForPingId(PING_NO_PAYLOAD.slug),
    getSavePathForPingId("no-slug-file"),
  ];

  // Write the ping to file
  await TelemetryStorage.savePing(PING_OLD_FORMAT, true);
  await TelemetryStorage.savePing(PING_NO_INFO, true);
  await TelemetryStorage.savePing(PING_NO_PAYLOAD, true);
  await TelemetryStorage.savePingToFile(
    PING_NO_SLUG,
    PING_FILES_PATHS[3],
    true
  );

  gSeenPings = 0;
  await TelemetryController.testReset();
  await TelemetrySend.testWaitOnOutgoingPings();
  assertReceivedPings(OLD_FORMAT_PINGS);

  // |TelemetryStorage.cleanup| doesn't know how to remove a ping with no slug or id,
  // so remove it manually so that the next test doesn't fail.
  await OS.File.remove(PING_FILES_PATHS[3]);

  await TelemetryStorage.testClearPendingPings();
});

add_task(async function test_corrupted_pending_pings() {
  const TEST_TYPE = "test_corrupted";

  Telemetry.getHistogramById("TELEMETRY_PENDING_LOAD_FAILURE_READ").clear();
  Telemetry.getHistogramById("TELEMETRY_PENDING_LOAD_FAILURE_PARSE").clear();

  // Save a pending ping and get its id.
  let pendingPingId = await TelemetryController.addPendingPing(
    TEST_TYPE,
    {},
    {}
  );

  // Try to load it: there should be no error.
  await TelemetryStorage.loadPendingPing(pendingPingId);

  let h = Telemetry.getHistogramById(
    "TELEMETRY_PENDING_LOAD_FAILURE_READ"
  ).snapshot();
  Assert.equal(
    h.sum,
    0,
    "Telemetry must not report a pending ping load failure"
  );
  h = Telemetry.getHistogramById(
    "TELEMETRY_PENDING_LOAD_FAILURE_PARSE"
  ).snapshot();
  Assert.equal(
    h.sum,
    0,
    "Telemetry must not report a pending ping parse failure"
  );

  // Delete it from the disk, so that its id will be kept in the cache but it will
  // fail loading the file.
  await OS.File.remove(getSavePathForPingId(pendingPingId));

  // Try to load a pending ping which isn't there anymore.
  await Assert.rejects(
    TelemetryStorage.loadPendingPing(pendingPingId),
    /PingReadError/,
    "Telemetry must fail loading a ping which isn't there"
  );

  h = Telemetry.getHistogramById(
    "TELEMETRY_PENDING_LOAD_FAILURE_READ"
  ).snapshot();
  Assert.equal(h.sum, 1, "Telemetry must report a pending ping load failure");
  h = Telemetry.getHistogramById(
    "TELEMETRY_PENDING_LOAD_FAILURE_PARSE"
  ).snapshot();
  Assert.equal(
    h.sum,
    0,
    "Telemetry must not report a pending ping parse failure"
  );

  // Save a new ping, so that it gets in the pending pings cache.
  pendingPingId = await TelemetryController.addPendingPing(TEST_TYPE, {}, {});
  // Overwrite it with a corrupted JSON file and then try to load it.
  const INVALID_JSON = "{ invalid,JSON { {1}";
  await OS.File.writeAtomic(getSavePathForPingId(pendingPingId), INVALID_JSON, {
    encoding: "utf-8",
  });

  // Try to load the ping with the corrupted JSON content.
  await Assert.rejects(
    TelemetryStorage.loadPendingPing(pendingPingId),
    /PingParseError/,
    "Telemetry must fail loading a corrupted ping"
  );

  h = Telemetry.getHistogramById(
    "TELEMETRY_PENDING_LOAD_FAILURE_READ"
  ).snapshot();
  Assert.equal(h.sum, 1, "Telemetry must report a pending ping load failure");
  h = Telemetry.getHistogramById(
    "TELEMETRY_PENDING_LOAD_FAILURE_PARSE"
  ).snapshot();
  Assert.equal(h.sum, 1, "Telemetry must report a pending ping parse failure");

  let exists = await OS.File.exists(getSavePathForPingId(pendingPingId));
  Assert.ok(!exists, "The unparseable ping should have been removed");

  await TelemetryStorage.testClearPendingPings();
});

/**
 * Create a ping in the old format, send it, and make sure the request URL contains
 * the correct version query parameter.
 */
add_task(async function test_overdue_old_format() {
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

  // Write the ping to file
  await TelemetryStorage.savePing(PING_OLD_FORMAT, true);

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

  await TelemetryController.testReset();
  await TelemetrySend.testWaitOnOutgoingPings();
  Assert.equal(receivedPings, 1, "We must receive a ping in the old format.");

  await TelemetryStorage.testClearPendingPings();
  PingServer.resetPingHandler();
});

add_task(async function test_pendingPingsQuota() {
  const PING_TYPE = "foo";

  // Disable upload so pings don't get sent and removed from the pending pings directory.
  Services.prefs.setBoolPref(
    TelemetryUtils.Preferences.FhrUploadEnabled,
    false
  );

  // Remove all the pending pings then startup and wait for the cleanup task to complete.
  // There should be nothing to remove.
  await TelemetryStorage.testClearPendingPings();
  await TelemetryController.testReset();
  await TelemetrySend.testWaitOnOutgoingPings();
  await TelemetryStorage.testPendingQuotaTaskPromise();

  // Remove the pending optout ping generated when flipping FHR upload off.
  await TelemetryStorage.testClearPendingPings();

  let expectedPrunedPings = [];
  let expectedNotPrunedPings = [];

  let checkPendingPings = async function() {
    // Check that the pruned pings are not on disk anymore.
    for (let prunedPingId of expectedPrunedPings) {
      await Assert.rejects(
        TelemetryStorage.loadPendingPing(prunedPingId),
        /TelemetryStorage.loadPendingPing - no ping with id/,
        "Ping " + prunedPingId + " should have been pruned."
      );
      const pingPath = getSavePathForPingId(prunedPingId);
      Assert.ok(
        !(await OS.File.exists(pingPath)),
        "The ping should not be on the disk anymore."
      );
    }

    // Check that the expected pings are there.
    for (let expectedPingId of expectedNotPrunedPings) {
      Assert.ok(
        await TelemetryStorage.loadPendingPing(expectedPingId),
        "Ping" + expectedPingId + " should be among the pending pings."
      );
    }
  };

  let pendingPingsInfo = [];
  let pingsSizeInBytes = 0;

  // Create 10 pings to test the pending pings quota.
  for (let days = 1; days < 11; days++) {
    const date = fakeNow(2010, 1, days, 1, 1, 0);
    const pingId = await TelemetryController.addPendingPing(PING_TYPE, {}, {});

    // Find the size of the ping.
    const pingFilePath = getSavePathForPingId(pingId);
    const pingSize = (await OS.File.stat(pingFilePath)).size;
    // Add the info at the beginning of the array, so that most recent pings come first.
    pendingPingsInfo.unshift({
      id: pingId,
      size: pingSize,
      timestamp: date.getTime(),
    });

    // Set the last modification date.
    await OS.File.setDates(pingFilePath, null, date.getTime());

    // Add it to the pending ping directory size.
    pingsSizeInBytes += pingSize;
  }

  // We need to test the pending pings size before we hit the quota, otherwise a special
  // value is recorded.
  Telemetry.getHistogramById("TELEMETRY_PENDING_PINGS_SIZE_MB").clear();
  Telemetry.getHistogramById(
    "TELEMETRY_PENDING_PINGS_EVICTED_OVER_QUOTA"
  ).clear();
  Telemetry.getHistogramById(
    "TELEMETRY_PENDING_EVICTING_OVER_QUOTA_MS"
  ).clear();

  await TelemetryController.testReset();
  await TelemetryStorage.testPendingQuotaTaskPromise();

  // Check that the correct values for quota probes are reported when no quota is hit.
  let h = Telemetry.getHistogramById(
    "TELEMETRY_PENDING_PINGS_SIZE_MB"
  ).snapshot();
  Assert.equal(
    h.sum,
    Math.round(pingsSizeInBytes / 1024 / 1024),
    "Telemetry must report the correct pending pings directory size."
  );
  h = Telemetry.getHistogramById(
    "TELEMETRY_PENDING_PINGS_EVICTED_OVER_QUOTA"
  ).snapshot();
  Assert.equal(
    h.sum,
    0,
    "Telemetry must report 0 evictions if quota is not hit."
  );
  h = Telemetry.getHistogramById(
    "TELEMETRY_PENDING_EVICTING_OVER_QUOTA_MS"
  ).snapshot();
  Assert.equal(
    h.sum,
    0,
    "Telemetry must report a null elapsed time if quota is not hit."
  );

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
  await TelemetryController.testReset();
  await TelemetryStorage.testPendingQuotaTaskPromise();
  await checkPendingPings();

  h = Telemetry.getHistogramById(
    "TELEMETRY_PENDING_PINGS_EVICTED_OVER_QUOTA"
  ).snapshot();
  Assert.equal(
    h.sum,
    pingsOutsideQuota.length,
    "Telemetry must correctly report the over quota pings evicted from the pending pings directory."
  );
  h = Telemetry.getHistogramById("TELEMETRY_PENDING_PINGS_SIZE_MB").snapshot();
  Assert.equal(
    h.sum,
    17,
    "Pending pings quota was hit, a special size must be reported."
  );

  // Trigger a cleanup again and make sure we're not removing anything.
  await TelemetryController.testReset();
  await TelemetryStorage.testPendingQuotaTaskPromise();
  await checkPendingPings();

  const OVERSIZED_PING_ID = "9b21ec8f-f762-4d28-a2c1-44e1c4694f24";
  // Create a pending oversized ping.
  const OVERSIZED_PING = {
    id: OVERSIZED_PING_ID,
    type: PING_TYPE,
    creationDate: new Date().toISOString(),
    // Generate a 2MB string to use as the ping payload.
    payload: generateRandomString(2 * 1024 * 1024),
  };
  await TelemetryStorage.savePendingPing(OVERSIZED_PING);

  // Reset the histograms.
  Telemetry.getHistogramById("TELEMETRY_PING_SIZE_EXCEEDED_PENDING").clear();
  Telemetry.getHistogramById(
    "TELEMETRY_DISCARDED_PENDING_PINGS_SIZE_MB"
  ).clear();

  // Try to manually load the oversized ping.
  await Assert.rejects(
    TelemetryStorage.loadPendingPing(OVERSIZED_PING_ID),
    /loadPendingPing - exceeded the maximum ping size/,
    "The oversized ping should have been pruned."
  );
  Assert.ok(
    !(await OS.File.exists(getSavePathForPingId(OVERSIZED_PING_ID))),
    "The ping should not be on the disk anymore."
  );

  // Make sure we're correctly updating the related histograms.
  h = Telemetry.getHistogramById(
    "TELEMETRY_PING_SIZE_EXCEEDED_PENDING"
  ).snapshot();
  Assert.equal(
    h.sum,
    1,
    "Telemetry must report 1 oversized ping in the pending pings directory."
  );
  h = Telemetry.getHistogramById(
    "TELEMETRY_DISCARDED_PENDING_PINGS_SIZE_MB"
  ).snapshot();
  Assert.equal(h.values[2], 1, "Telemetry must report a 2MB, oversized, ping.");

  // Save the ping again to check if it gets pruned when scanning the pings directory.
  await TelemetryStorage.savePendingPing(OVERSIZED_PING);
  expectedPrunedPings.push(OVERSIZED_PING_ID);

  // Scan the pending pings directory.
  await TelemetryController.testReset();
  await TelemetryStorage.testPendingQuotaTaskPromise();
  await checkPendingPings();

  // Make sure we're correctly updating the related histograms.
  h = Telemetry.getHistogramById(
    "TELEMETRY_PING_SIZE_EXCEEDED_PENDING"
  ).snapshot();
  Assert.equal(
    h.sum,
    2,
    "Telemetry must report 1 oversized ping in the pending pings directory."
  );
  h = Telemetry.getHistogramById(
    "TELEMETRY_DISCARDED_PENDING_PINGS_SIZE_MB"
  ).snapshot();
  Assert.equal(
    h.values[2],
    2,
    "Telemetry must report two 2MB, oversized, pings."
  );

  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);
});

add_task(async function teardown() {
  await PingServer.stop();
});
