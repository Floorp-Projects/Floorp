/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/

/**
 * This test case populates the profile with some fake stored
 * pings, and checks that:
 *
 * 1) Pings that are considered "expired" are deleted and never sent.
 * 2) Pings that are considered "overdue" trigger a send of all
 *    overdue and recent pings.
 */

"use strict"

Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://testing-common/httpd.js", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/TelemetryStorage.jsm", this);
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
let {OS: {File, Path, Constants}} = Cu.import("resource://gre/modules/osfile.jsm", {});

XPCOMUtils.defineLazyGetter(this, "gDatareportingService",
  () => Cc["@mozilla.org/datareporting/service;1"]
          .getService(Ci.nsISupports)
          .wrappedJSObject);

// We increment TelemetryStorage's MAX_PING_FILE_AGE and
// OVERDUE_PING_FILE_AGE by 1 minute so that our test pings exceed
// those points in time, even taking into account file system imprecision.
const ONE_MINUTE_MS = 60 * 1000;
const EXPIRED_PING_FILE_AGE = TelemetryStorage.MAX_PING_FILE_AGE + ONE_MINUTE_MS;
const OVERDUE_PING_FILE_AGE = TelemetryStorage.OVERDUE_PING_FILE_AGE + ONE_MINUTE_MS;

const PING_SAVE_FOLDER = "saved-telemetry-pings";
const PING_TIMEOUT_LENGTH = 5000;
const EXPIRED_PINGS = 5;
const OVERDUE_PINGS = 6;
const OLD_FORMAT_PINGS = 4;
const RECENT_PINGS = 4;
const LRU_PINGS = TelemetryStorage.MAX_LRU_PINGS;

const TOTAL_EXPECTED_PINGS = OVERDUE_PINGS + RECENT_PINGS + OLD_FORMAT_PINGS;

let gHttpServer = new HttpServer();
let gCreatedPings = 0;
let gSeenPings = 0;

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
let createSavedPings = Task.async(function* (aPingInfos) {
  let pingIds = [];
  let now = Date.now();

  for (let type in aPingInfos) {
    let num = aPingInfos[type].num;
    let age = now - aPingInfos[type].age;
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
let clearPings = Task.async(function* (aPingIds) {
  for (let pingId of aPingIds) {
    let filePath = getSavePathForPingId(pingId);
    yield File.remove(filePath);
  }
});

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
let assertNotSaved = Task.async(function* (aPingIds) {
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

/**
 * Returns a Promise that resolves when gHttpServer has been
 * successfully shut down.
 *
 * @returns Promise
 */
function stopHttpServer() {
  let deferred = Promise.defer();
  gHttpServer.stop(function() {
    deferred.resolve();
  })
  return deferred.promise;
}

/**
 * Reset Telemetry state.
 */
function resetTelemetry() {
  // Quick and dirty way to clear TelemetryStorage's pendingPings
  // collection, and put it back in its initial state.
  let gen = TelemetryStorage.popPendingPings();
  for (let item of gen) {};
}

/**
 * Creates and returns a TelemetryController instance in "testing"
 * mode.
 */
function startTelemetry() {
  return TelemetryController.setup();
}

function run_test() {
  gHttpServer.registerPrefixHandler("/submit/telemetry/", pingHandler);
  gHttpServer.start(-1);
  do_get_profile();
  loadAddonManager("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9.2");

  // Send the needed startup notifications to the datareporting service
  // to ensure that it has been initialized.
  gDatareportingService.observe(null, "app-startup", null);
  gDatareportingService.observe(null, "profile-after-change", null);

  Services.prefs.setBoolPref(TelemetryController.Constants.PREF_ENABLED, true);
  Services.prefs.setCharPref(TelemetryController.Constants.PREF_SERVER,
                             "http://localhost:" + gHttpServer.identity.primaryPort);
  run_next_test();
}

/**
 * Setup the tests by making sure the ping storage directory is available, otherwise
 * |TelemetryController.testSaveDirectoryToFile| could fail.
 */
add_task(function* setupEnvironment() {
  yield TelemetryController.setup();

  let directory = TelemetryStorage.pingDirectoryPath;
  yield File.makeDir(directory, { ignoreExisting: true, unixMode: OS.Constants.S_IRWXU });

  yield resetTelemetry();
});

/**
 * Test that pings that are considered too old are just chucked out
 * immediately and never sent.
 */
add_task(function* test_expired_pings_are_deleted() {
  let pingTypes = [{ num: EXPIRED_PINGS, age: EXPIRED_PING_FILE_AGE }];
  let expiredPings = yield createSavedPings(pingTypes);
  yield startTelemetry();
  assertReceivedPings(0);
  yield assertNotSaved(expiredPings);
  yield resetTelemetry();
});

/**
 * Test that really recent pings are not sent on Telemetry initialization.
 */
add_task(function* test_recent_pings_not_sent() {
  let pingTypes = [{ num: RECENT_PINGS }];
  let recentPings = yield createSavedPings(pingTypes);
  yield startTelemetry();
  assertReceivedPings(0);
  yield resetTelemetry();
  yield clearPings(recentPings);
});

/**
 * Test that only the most recent LRU_PINGS pings are kept at startup.
 */
add_task(function* test_most_recent_pings_kept() {
  let pingTypes = [
    { num: LRU_PINGS },
    { num: 3, age: ONE_MINUTE_MS },
  ];
  let pings = yield createSavedPings(pingTypes);
  let head = pings.slice(0, LRU_PINGS);
  let tail = pings.slice(-3);

  yield startTelemetry();
  let gen = TelemetryStorage.popPendingPings();

  for (let item of gen) {
    for (let id of tail) {
      do_check_neq(id, item.id);
    }
  }

  assertNotSaved(tail);
  yield resetTelemetry();
  yield clearPings(pings);
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
    Path.join(Constants.Path.profileDir, PING_SAVE_FOLDER, PING_OLD_FORMAT.slug),
    Path.join(Constants.Path.profileDir, PING_SAVE_FOLDER, PING_NO_INFO.slug),
    Path.join(Constants.Path.profileDir, PING_SAVE_FOLDER, PING_NO_PAYLOAD.slug),
    Path.join(Constants.Path.profileDir, PING_SAVE_FOLDER, "no-slug-file"),
  ];

  // Write the ping to file and make it overdue.
  yield TelemetryStorage.savePing(PING_OLD_FORMAT, true);
  yield TelemetryStorage.savePing(PING_NO_INFO, true);
  yield TelemetryStorage.savePing(PING_NO_PAYLOAD, true);
  yield TelemetryStorage.savePingToFile(PING_NO_SLUG, PING_FILES_PATHS[3], true);

  for (let f in PING_FILES_PATHS) {
    yield File.setDates(PING_FILES_PATHS[f], null, Date.now() - OVERDUE_PING_FILE_AGE);
  }

  yield startTelemetry();
  assertReceivedPings(OLD_FORMAT_PINGS);

  // |TelemetryStorage.cleanup| doesn't know how to remove a ping with no slug or id,
  // so remove it manually so that the next test doesn't fail.
  yield OS.File.remove(PING_FILES_PATHS[3]);

  yield resetTelemetry();
});

/**
 * Create some recent, expired and overdue pings. The overdue pings should
 * trigger a send of all recent and overdue pings, but the expired pings
 * should just be deleted.
 */
add_task(function* test_overdue_pings_trigger_send() {
  let pingTypes = [
    { num: RECENT_PINGS },
    { num: EXPIRED_PINGS, age: EXPIRED_PING_FILE_AGE },
    { num: OVERDUE_PINGS, age: OVERDUE_PING_FILE_AGE },
  ];
  let pings = yield createSavedPings(pingTypes);
  let recentPings = pings.slice(0, RECENT_PINGS);
  let expiredPings = pings.slice(RECENT_PINGS, RECENT_PINGS + EXPIRED_PINGS);
  let overduePings = pings.slice(-OVERDUE_PINGS);

  yield startTelemetry();
  assertReceivedPings(TOTAL_EXPECTED_PINGS);

  yield assertNotSaved(recentPings);
  yield assertNotSaved(expiredPings);
  yield assertNotSaved(overduePings);
  yield resetTelemetry();
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
  gHttpServer.registerPrefixHandler("/submit/telemetry/", request => {
    // Check that we have a version query parameter in the URL.
    Assert.notEqual(request.queryString, "");

    // Make sure the version in the query string matches the old ping format version.
    let params = request.queryString.split("&");
    Assert.ok(params.find(p => p == "v=1"));

    receivedPings++;
  });

  yield startTelemetry();
  Assert.equal(receivedPings, 1, "We must receive a ping in the old format.");

  yield resetTelemetry();
});

add_task(function* teardown() {
  yield stopHttpServer();
});
