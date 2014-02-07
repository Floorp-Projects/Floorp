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

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://testing-common/httpd.js", this);
Cu.import("resource://gre/modules/Promise.jsm", this);
Cu.import("resource://gre/modules/TelemetryFile.jsm", this);
Cu.import("resource://gre/modules/TelemetryPing.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/osfile.jsm", this);

// We increment TelemetryFile's MAX_PING_FILE_AGE and
// OVERDUE_PING_FILE_AGE by 1ms so that our test pings exceed
// those points in time.
const EXPIRED_PING_FILE_AGE = TelemetryFile.MAX_PING_FILE_AGE + 1;
const OVERDUE_PING_FILE_AGE = TelemetryFile.OVERDUE_PING_FILE_AGE + 1;

const PING_SAVE_FOLDER = "saved-telemetry-pings";
const PING_TIMEOUT_LENGTH = 5000;
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
  return Task.spawn(function*(){
    // Create a TelemetryPing service that we can generate payloads from.
    // Luckily, the TelemetryPing constructor does nothing that we need to
    // clean up.
    let pings = [];
    let age = Date.now() - aAge;

    for (let i = 0; i < aNum; ++i) {
      let payload = TelemetryPing.getPayload();
      let ping = { slug: "test-ping-" + gCreatedPings, reason: "test", payload: payload };

      yield TelemetryFile.savePing(ping);

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
  });
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
 * Check if the number of TelemetryPings received by the 
 * HttpServer is not equal to aExpectedNum.
 *
 * @param aExpectedNum the number of pings we expect to receive.
 */
function assertReceivedPings(aExpectedNum) {
  do_check_eq(gSeenPings, aExpectedNum);
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
function resetTelemetry() {
  TelemetryPing.uninstall();
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
  return TelemetryPing.setup();
}

function run_test() {
  gHttpServer.registerPrefixHandler("/submit/telemetry/", pingHandler);
  gHttpServer.start(-1);
  do_get_profile();
  Services.prefs.setBoolPref(TelemetryPing.Constants.PREF_ENABLED, true);
  Services.prefs.setCharPref(TelemetryPing.Constants.PREF_SERVER,
                             "http://localhost:" + gHttpServer.identity.primaryPort);
  run_next_test();
}

/**
 * Test that pings that are considered too old are just chucked out
 * immediately and never sent.
 */
add_task(function test_expired_pings_are_deleted() {
  let expiredPings = yield createSavedPings(EXPIRED_PINGS, EXPIRED_PING_FILE_AGE);
  yield startTelemetry();
  assertReceivedPings(0);
  assertNotSaved(expiredPings);
  yield resetTelemetry();
});

/**
 * Test that really recent pings are not sent on Telemetry initialization.
 */
add_task(function test_recent_pings_not_sent() {
  let recentPings = yield createSavedPings(RECENT_PINGS);
  yield startTelemetry();
  assertReceivedPings(0);
  yield resetTelemetry();
  clearPings(recentPings);
});

/**
 * Create some recent, expired and overdue pings. The overdue pings should
 * trigger a send of all recent and overdue pings, but the expired pings
 * should just be deleted.
 */
add_task(function test_overdue_pings_trigger_send() {
  let recentPings = yield createSavedPings(RECENT_PINGS);
  let expiredPings = yield createSavedPings(EXPIRED_PINGS, EXPIRED_PING_FILE_AGE);
  let overduePings = yield createSavedPings(OVERDUE_PINGS, OVERDUE_PING_FILE_AGE);

  yield startTelemetry();
  assertReceivedPings(TOTAL_EXPECTED_PINGS);

  assertNotSaved(recentPings);
  assertNotSaved(expiredPings);
  assertNotSaved(overduePings);
  yield resetTelemetry();
});

add_task(function teardown() {
  yield stopHttpServer();
});
