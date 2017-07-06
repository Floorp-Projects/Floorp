/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

// This tests submitting a ping using the stand-alone pingsender program.

"use strict";

Cu.import("resource://gre/modules/osfile.jsm", this);
Cu.import("resource://gre/modules/Preferences.jsm", this);
Cu.import("resource://gre/modules/PromiseUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/TelemetrySend.jsm", this);
Cu.import("resource://gre/modules/TelemetryStorage.jsm", this);
Cu.import("resource://gre/modules/TelemetryUtils.jsm", this);
Cu.import("resource://gre/modules/Timer.jsm", this);

/**
 * Wait for a ping file to be deleted from the pending pings directory.
 */
function waitForPingDeletion(pingId) {
  const path = OS.Path.join(TelemetryStorage.pingDirectoryPath, pingId);

  let checkFn = (resolve, reject) => setTimeout(() => {
    OS.File.exists(path).then(exists => {
      if (!exists) {
        Assert.ok(true, pingId + " was deleted");
        resolve();
      } else {
        checkFn(resolve, reject);
      }
    }, reject);
  }, 250);

  return new Promise((resolve, reject) => checkFn(resolve, reject));
}

add_task(async function setup() {
  // Init the profile.
  do_get_profile(true);

  Services.prefs.setBoolPref(TelemetryUtils.Preferences.TelemetryEnabled, true);
  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  // Start the ping server and let Telemetry know about it.
  PingServer.start();
});

add_task(async function test_pingSender() {
  // Generate a new ping and save it among the pending pings.
  const data = {
    type: "test-pingsender-type",
    id: TelemetryUtils.generateUUID(),
    creationDate: (new Date(1485810000)).toISOString(),
    version: 4,
    payload: {
      dummy: "stuff"
    }
  };
  await TelemetryStorage.savePing(data, true);

  // Get the local path of the saved ping.
  const pingPath = OS.Path.join(TelemetryStorage.pingDirectoryPath, data.id);

  // Spawn an HTTP server that returns an error. We will be running the
  // PingSender twice, trying to send the ping to this server. After the
  // second time, we will resolve |deferred404Hit|.
  let failingServer = new HttpServer();
  let deferred404Hit = PromiseUtils.defer();
  let hitCount = 0;
  failingServer.registerPathHandler("/lookup_fail", (metadata, response) => {
    response.setStatusLine("1.1", 404, "Not Found");
    hitCount++;

    if (hitCount >= 2) {
      // Resolve the promise on the next tick.
      Services.tm.dispatchToMainThread(() => deferred404Hit.resolve());
    }
  });
  failingServer.start(-1);

  // Try to send the ping twice using the pingsender (we expect 404 both times).
  const errorUrl = "http://localhost:" + failingServer.identity.primaryPort + "/lookup_fail";
  TelemetrySend.testRunPingSender(errorUrl, pingPath);
  TelemetrySend.testRunPingSender(errorUrl, pingPath);

  // Wait until we hit the 404 server twice. After that, make sure that the ping
  // still exists locally.
  await deferred404Hit.promise;
  Assert.ok((await OS.File.exists(pingPath)),
            "The pending ping must not be deleted if we fail to send using the PingSender");

  // Try to send it using the pingsender.
  const url = "http://localhost:" + PingServer.port + "/submit/telemetry/";
  TelemetrySend.testRunPingSender(url, pingPath);

  let req = await PingServer.promiseNextRequest();
  let ping = decodeRequestPayload(req);

  Assert.equal(req.getHeader("User-Agent"), "pingsender/1.0",
               "Should have received the correct user agent string.");
  Assert.equal(req.getHeader("X-PingSender-Version"), "1.0",
               "Should have received the correct PingSender version string.");
  Assert.equal(req.getHeader("Content-Encoding"), "gzip",
               "Should have a gzip encoded ping.");
  Assert.ok(req.getHeader("Date"), "Should have received a Date header.");
  Assert.equal(ping.id, data.id, "Should have received the correct ping id.");
  Assert.equal(ping.type, data.type,
               "Should have received the correct ping type.");
  Assert.deepEqual(ping.payload, data.payload,
                   "Should have received the correct payload.");

  // Check that the PingSender removed the pending ping.
  await waitForPingDeletion(data.id);

  // Shut down the failing server. We do this now, and not right after using it,
  // to make sure we're not interfering with the test.
  await new Promise(r => failingServer.stop(r));
});

add_task(async function cleanup() {
  await PingServer.stop();
});
