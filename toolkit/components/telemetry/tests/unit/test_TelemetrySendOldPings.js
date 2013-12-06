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

Components.utils.import("resource://gre/modules/Services.jsm");

// Get the TelemetryPing definitions directly so we can test it without going through xpcom.
// That gives us Cc, Ci, Cr and Cu, as well as a number of consts like PREF_ENABLED,
// and PREF_SERVER.
Services.scriptloader.loadSubScript("resource://gre/components/TelemetryPing.js");

Cu.import("resource://testing-common/httpd.js");
Cu.import("resource://gre/modules/Promise.jsm");
Cu.import("resource://gre/modules/TelemetryFile.jsm");

// We increment TelemetryFile's MAX_PING_FILE_AGE and
// OVERDUE_PING_FILE_AGE by 1ms so that our test pings exceed
// those points in time.
const EXPIRED_PING_FILE_AGE = TelemetryFile.MAX_PING_FILE_AGE + 1;
const OVERDUE_PING_FILE_AGE = TelemetryFile.OVERDUE_PING_FILE_AGE + 1;

const PING_SAVE_FOLDER = "saved-telemetry-pings";
const PING_TIMEOUT_LENGTH = 1500;
const EXPIRED_PINGS = 5;
const OVERDUE_PINGS = 6;
const RECENT_PINGS = 4;

const TOTAL_EXPECTED_PINGS = OVERDUE_PINGS + RECENT_PINGS;

let gHttpServer = new HttpServer();
let gCreatedPings = 0;
let gSeenPings = 0;

/**
 * Creates some TelemetryPings for the current session and
 * saves them to disk. Each ping gets a unique ID slug based on
 * an incrementor.
 *
 * @param aNum the number of pings to create.
 * @param aAge the age in milliseconds to offset from now. A value
 *             of 10 would make the ping 10ms older than now, for
 *             example.
 * @returns an Array with the created pings.
 */
function createSavedPings(aNum, aAge) {
  // Create a TelemetryPing service that we can generate payloads from.
  // Luckily, the TelemetryPing constructor does nothing that we need to
  // clean up.
  let pingService = new TelemetryPing();
  let pings = [];
  let age = Date.now() - aAge;
  for (let i = 0; i < aNum; ++i) {
    let payload = pingService.getPayload();
    let ping = { slug: "test-ping-" + gCreatedPings, reason: "test", payload: payload };
    TelemetryFile.savePing(ping);
    if (aAge) {
      // savePing writes to the file synchronously, so we're good to
      // modify the lastModifedTime now.
      let file = getSaveFileForPing(ping);
      file.lastModifiedTime = age;
    }
    gCreatedPings++;
    pings.push(ping);
  }
  return pings;
}

/**
 * Deletes locally saved pings in aPings if they
 * exist.
 *
 * @param aPings an Array of pings to delete.
 */
function clearPings(aPings) {
  for (let ping of aPings) {
    let file = getSaveFileForPing(ping);
    if (file.exists()) {
      file.remove(false);
    }
  }
}

/**
 * Returns a handle for the file that aPing should be
 * stored in locally.
 *
 * @returns nsILocalFile
 */
function getSaveFileForPing(aPing) {
  let file = Services.dirsvc.get("ProfD", Ci.nsILocalFile).clone();
  file.append(PING_SAVE_FOLDER);
  file.append(aPing.slug);
  return file;
}

/**
 * Wait for PING_TIMEOUT_LENGTH ms, and make sure we didn't receive
 * TelemetryPings in that time.
 *
 * @returns Promise
 */
function assertReceivedNoPings() {
  let deferred = Promise.defer();

  do_timeout(PING_TIMEOUT_LENGTH, function() {
    if (gSeenPings > 0) {
      deferred.reject();
    } else {
      deferred.resolve();
    }
  });

  return deferred.promise;
}

/**
 * Returns a Promise that rejects if the number of TelemetryPings
 * received by the HttpServer is not equal to aExpectedNum.
 *
 * @param aExpectedNum the number of pings we expect to receive.
 * @returns Promise
 */
function assertReceivedPings(aExpectedNum) {
  let deferred = Promise.defer();

  do_timeout(PING_TIMEOUT_LENGTH, function() {
    if (gSeenPings == aExpectedNum) {
      deferred.resolve();
    } else {
      deferred.reject("Saw " + gSeenPings + " TelemetryPings, " +
                      "but expected " + aExpectedNum);
    }
  })

  return deferred.promise;
}

/**
 * Throws if any pings in aPings is saved locally.
 *
 * @param aPings an Array of pings to check.
 */
function assertNotSaved(aPings) {
  let saved = 0;
  for (let ping of aPings) {
    let file = getSaveFileForPing(ping);
    if (file.exists()) {
      saved++;
    }
  }
  if (saved > 0) {
    do_throw("Found " + saved + " unexpected saved pings.");
  }
}

/**
 * Our handler function for the HttpServer that simply
 * increments the gSeenPings global when it successfully
 * receives and decodes a TelemetryPing payload.
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
 * Teardown a TelemetryPing instance and clear out any pending
 * pings to put as back in the starting state.
 */
function resetTelemetry(aPingService) {
  aPingService.uninstall();
  // Quick and dirty way to clear TelemetryFile's pendingPings
  // collection, and put it back in its initial state.
  let gen = TelemetryFile.popPendingPings();
  for (let item of gen) {};
}

/**
 * Creates and returns a TelemetryPing instance in "testing"
 * mode.
 */
function startTelemetry() {
  let service = new TelemetryPing();
  service.setup(true);
  return service;
}

function run_test() {
  gHttpServer.registerPrefixHandler("/submit/telemetry/", pingHandler);
  gHttpServer.start(-1);
  do_get_profile();
  Services.prefs.setBoolPref(PREF_ENABLED, true);
  Services.prefs.setCharPref(PREF_SERVER,
                             "http://localhost:" + gHttpServer.identity.primaryPort);
  run_next_test();
}

/**
 * Test that pings that are considered too old are just chucked out
 * immediately and never sent.
 */
add_task(function test_expired_pings_are_deleted() {
  let expiredPings = createSavedPings(EXPIRED_PINGS, EXPIRED_PING_FILE_AGE);
  let pingService = startTelemetry();
  yield assertReceivedNoPings();
  assertNotSaved(expiredPings);
  resetTelemetry(pingService);
})

/**
 * Test that really recent pings are not sent on Telemetry initialization.
 */
add_task(function test_recent_pings_not_sent() {
  let recentPings = createSavedPings(RECENT_PINGS);
  let pingService = startTelemetry();
  yield assertReceivedNoPings();
  resetTelemetry(pingService);
  clearPings(recentPings);
});

/**
 * Create some recent, expired and overdue pings. The overdue pings should
 * trigger a send of all recent and overdue pings, but the expired pings
 * should just be deleted.
 */
add_task(function test_overdue_pings_trigger_send() {
  let recentPings = createSavedPings(RECENT_PINGS);
  let expiredPings = createSavedPings(EXPIRED_PINGS, EXPIRED_PING_FILE_AGE);
  let overduePings = createSavedPings(OVERDUE_PINGS, OVERDUE_PING_FILE_AGE);

  let pingService = startTelemetry();
  yield assertReceivedPings(TOTAL_EXPECTED_PINGS);

  assertNotSaved(recentPings);
  assertNotSaved(expiredPings);
  assertNotSaved(overduePings);
  resetTelemetry(pingService);
})

add_task(function teardown() {
  yield stopHttpServer();
});
