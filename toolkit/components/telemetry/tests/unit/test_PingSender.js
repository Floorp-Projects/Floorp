/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/
/* eslint-disable mozilla/no-arbitrary-setTimeout */

// This tests submitting a ping using the stand-alone pingsender program.

"use strict";

const { PromiseUtils } = ChromeUtils.import(
  "resource://gre/modules/PromiseUtils.jsm"
);
const { TelemetrySend } = ChromeUtils.import(
  "resource://gre/modules/TelemetrySend.jsm"
);
const { TelemetryStorage } = ChromeUtils.import(
  "resource://gre/modules/TelemetryStorage.jsm"
);
const { TelemetryUtils } = ChromeUtils.import(
  "resource://gre/modules/TelemetryUtils.jsm"
);
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

function generateTestPingData() {
  return {
    type: "test-pingsender-type",
    id: TelemetryUtils.generateUUID(),
    creationDate: new Date().toISOString(),
    version: 4,
    payload: {
      dummy: "stuff",
    },
  };
}

function testSendingPings(pingPaths) {
  const url = "http://localhost:" + PingServer.port + "/submit/telemetry/";
  const pings = pingPaths.map(path => {
    return {
      url,
      path,
    };
  });
  TelemetrySend.testRunPingSender(pings, (_, topic, __) => {
    switch (topic) {
      case "process-finished": // finished indicates an exit code of 0
        Assert.ok(true, "Pingsender should be able to post to localhost");
        break;
      case "process-failed": // failed indicates an exit code != 0
        Assert.ok(false, "Pingsender should be able to post to localhost");
        break;
    }
  });
}

/**
 * Wait for a ping file to be deleted from the pending pings directory.
 */
function waitForPingDeletion(pingId) {
  const path = OS.Path.join(TelemetryStorage.pingDirectoryPath, pingId);

  let checkFn = (resolve, reject) =>
    setTimeout(() => {
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

  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  // Start the ping server and let Telemetry know about it.
  PingServer.start();
});

add_task(async function test_pingSender() {
  // Generate a new ping and save it among the pending pings.
  const data = generateTestPingData();
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
  const errorUrl =
    "http://localhost:" + failingServer.identity.primaryPort + "/lookup_fail";
  TelemetrySend.testRunPingSender([{ url: errorUrl, path: pingPath }]);
  TelemetrySend.testRunPingSender([{ url: errorUrl, path: pingPath }]);

  // Wait until we hit the 404 server twice. After that, make sure that the ping
  // still exists locally.
  await deferred404Hit.promise;
  Assert.ok(
    await OS.File.exists(pingPath),
    "The pending ping must not be deleted if we fail to send using the PingSender"
  );

  // Try to send it using the pingsender.
  testSendingPings([pingPath]);

  let req = await PingServer.promiseNextRequest();
  let ping = decodeRequestPayload(req);

  Assert.equal(
    req.getHeader("User-Agent"),
    "pingsender/1.0",
    "Should have received the correct user agent string."
  );
  Assert.equal(
    req.getHeader("X-PingSender-Version"),
    "1.0",
    "Should have received the correct PingSender version string."
  );
  Assert.equal(
    req.getHeader("Content-Encoding"),
    "gzip",
    "Should have a gzip encoded ping."
  );
  Assert.ok(req.getHeader("Date"), "Should have received a Date header.");
  Assert.equal(ping.id, data.id, "Should have received the correct ping id.");
  Assert.equal(
    ping.type,
    data.type,
    "Should have received the correct ping type."
  );
  Assert.deepEqual(
    ping.payload,
    data.payload,
    "Should have received the correct payload."
  );

  // Check that the PingSender removed the pending ping.
  await waitForPingDeletion(data.id);

  // Shut down the failing server.
  await new Promise(r => failingServer.stop(r));
});

add_task(async function test_bannedDomains() {
  // Generate a new ping and save it among the pending pings.
  const data = generateTestPingData();
  await TelemetryStorage.savePing(data, true);

  // Get the local path of the saved ping.
  const pingPath = OS.Path.join(TelemetryStorage.pingDirectoryPath, data.id);

  // Confirm we can't send a ping to another destination url
  let bannedUris = [
    "https://example.com",
    "http://localhost.com",
    "http://localHOST.com",
    "http://localhost@example.com",
    "http://localhost:bob@example.com",
    "http://localhost:localhost@localhost.example.com",
  ];
  for (let url of bannedUris) {
    let result = await new Promise(resolve =>
      TelemetrySend.testRunPingSender(
        [{ url, path: pingPath }],
        (_, topic, __) => {
          switch (topic) {
            case "process-finished": // finished indicates an exit code of 0
            case "process-failed": // failed indicates an exit code != 0
              resolve(topic);
          }
        }
      )
    );
    Assert.equal(
      result,
      "process-failed",
      `Pingsender should not be able to post to ${url}`
    );
  }
});

add_task(async function test_pingSender_multiple_pings() {
  // Generate two new pings and save them among the pending pings.
  const data = [generateTestPingData(), generateTestPingData()];

  for (const d of data) {
    await TelemetryStorage.savePing(d, true);
  }

  // Get the local path of the saved pings.
  const pingPaths = data.map(d =>
    OS.Path.join(TelemetryStorage.pingDirectoryPath, d.id)
  );

  // Try to send them using the pingsender.
  testSendingPings(pingPaths);

  // Check the pings. We don't have an ordering guarantee, so we move the
  // elements to a new array when we find them.
  let data2 = [];
  while (data.length) {
    let req = await PingServer.promiseNextRequest();
    let ping = decodeRequestPayload(req);
    let idx = data.findIndex(d => d.id == ping.id);
    Assert.ok(
      idx >= 0,
      `Should have received the correct ping id: ${data[idx].id}`
    );
    data2.push(data[idx]);
    data.splice(idx, 1);
  }

  // Check that the PingSender removed the pending pings.
  for (const d of data2) {
    await waitForPingDeletion(d.id);
  }
});

add_task(async function cleanup() {
  await PingServer.stop();
});
