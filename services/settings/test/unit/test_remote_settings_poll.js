/* import-globals-from ../../../common/tests/unit/head_helpers.js */

ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://testing-common/httpd.js");

const { UptakeTelemetry } = ChromeUtils.import("resource://services-common/uptake-telemetry.js", {});
const { RemoteSettings } = ChromeUtils.import("resource://services-settings/remote-settings.js", {});
const { Kinto } = ChromeUtils.import("resource://services-common/kinto-offline-client.js", {});

const IS_ANDROID = AppConstants.platform == "android";

const PREF_SETTINGS_SERVER = "services.settings.server";
const PREF_SETTINGS_SERVER_BACKOFF = "services.settings.server.backoff";
const PREF_LAST_UPDATE = "services.settings.last_update_seconds";
const PREF_LAST_ETAG = "services.settings.last_etag";
const PREF_CLOCK_SKEW_SECONDS = "services.settings.clock_skew_seconds";

// Telemetry report result.
const TELEMETRY_HISTOGRAM_KEY = "settings-changes-monitoring";
const CHANGES_PATH = "/v1/buckets/monitor/collections/changes/records";

var server;

async function clear_state() {
  // set up prefs so the kinto updater talks to the test server
  Services.prefs.setCharPref(PREF_SETTINGS_SERVER,
    `http://localhost:${server.identity.primaryPort}/v1`);

  // set some initial values so we can check these are updated appropriately
  Services.prefs.setIntPref(PREF_LAST_UPDATE, 0);
  Services.prefs.setIntPref(PREF_CLOCK_SKEW_SECONDS, 0);
  Services.prefs.clearUserPref(PREF_LAST_ETAG);
}

function serveChangesEntries(serverTime, entries) {
  return (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("Date", (new Date(serverTime)).toUTCString());
    if (entries.length) {
      response.setHeader("ETag", `"${entries[0].last_modified}"`);
    }
    response.write(JSON.stringify({"data": entries}));
  };
}

function run_test() {
  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  run_next_test();

  registerCleanupFunction(function() {
    server.stop(function() { });
  });
}

add_task(clear_state);

add_task(async function test_check_success() {
  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);
  const serverTime = 8000;

  server.registerPathHandler(CHANGES_PATH, serveChangesEntries(serverTime, [{
    id: "330a0c5f-fadf-ff0b-40c8-4eb0d924ff6a",
    last_modified: 1100,
    host: "localhost",
    bucket: "some-other-bucket",
    collection: "test-collection"
  }, {
    id: "254cbb9e-6888-4d9f-8e60-58b74faa8778",
    last_modified: 1000,
    host: "localhost",
    bucket: "test-bucket",
    collection: "test-collection"
  }]));

  // add a test kinto client that will respond to lastModified information
  // for a collection called 'test-collection'
  let maybeSyncCalled = false;
  const c = RemoteSettings("test-collection", {
    bucketName: "test-bucket",
  });
  c.maybeSync = () => { maybeSyncCalled = true; };

  // Ensure that the remote-settings-changes-polled notification works
  let notificationObserved = false;
  const observer = {
    observe(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, "remote-settings-changes-polled");
      notificationObserved = true;
    }
  };
  Services.obs.addObserver(observer, "remote-settings-changes-polled");

  await RemoteSettings.pollChanges();

  // It didn't fail, hence we are sure that the unknown collection ``some-other-bucket/test-collection``
  // was ignored, otherwise it would have tried to reach the network.

  Assert.ok(maybeSyncCalled, "maybeSync was called");
  Assert.ok(notificationObserved, "a notification should have been observed");
  // Last timestamp was saved. An ETag header value is a quoted string.
  Assert.equal(Services.prefs.getCharPref(PREF_LAST_ETAG), "\"1100\"");
  // check the last_update is updated
  Assert.equal(Services.prefs.getIntPref(PREF_LAST_UPDATE), serverTime / 1000);
  // ensure that we've accumulated the correct telemetry
  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.SUCCESS]: 1,
  };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);


add_task(async function test_check_up_to_date() {
  // Simulate a poll with up-to-date collection.
  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);

  const serverTime = 4000;
  function server304(request, response) {
    if (request.hasHeader("if-none-match") && request.getHeader("if-none-match") == "\"1100\"") {
      response.setHeader("Date", (new Date(serverTime)).toUTCString());
      response.setStatusLine(null, 304, "Service Not Modified");
    }
  }
  server.registerPathHandler(CHANGES_PATH, server304);

  Services.prefs.setCharPref(PREF_LAST_ETAG, '"1100"');

  // Ensure that the remote-settings-changes-polled notification is sent.
  let notificationObserved = false;
  const observer = {
    observe(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, "remote-settings-changes-polled");
      notificationObserved = true;
    }
  };
  Services.obs.addObserver(observer, "remote-settings-changes-polled");

  // If server has no change, a 304 is received, maybeSync() is not called.
  let maybeSyncCalled = false;
  const c = RemoteSettings("test-collection", {
    bucketName: "test-bucket",
  });
  c.maybeSync = () => { maybeSyncCalled = true; };

  await RemoteSettings.pollChanges();

  Assert.ok(notificationObserved, "a notification should have been observed");
  Assert.ok(!maybeSyncCalled, "maybeSync should not be called");
  // Last update is overwritten
  Assert.equal(Services.prefs.getIntPref(PREF_LAST_UPDATE), serverTime / 1000);

  // ensure that we've accumulated the correct telemetry
  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.UP_TO_DATE]: 1,
  };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);


add_task(async function test_server_error() {
  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);

  // Simulate a server error.
  function simulateErrorResponse(request, response) {
    response.setHeader("Date", (new Date(3000)).toUTCString());
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.write(JSON.stringify({
      code: 503,
      errno: 999,
      error: "Service Unavailable",
    }));
    response.setStatusLine(null, 503, "Service Unavailable");
  }
  server.registerPathHandler(CHANGES_PATH, simulateErrorResponse);

  let notificationObserved = false;
  const observer = {
    observe(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, "remote-settings-changes-polled");
      notificationObserved = true;
    }
  };
  Services.obs.addObserver(observer, "remote-settings-changes-polled");
  Services.prefs.setIntPref(PREF_LAST_UPDATE, 42);

  // pollChanges() fails with adequate error and no notification.
  let error;
  try {
    await RemoteSettings.pollChanges();
  } catch (e) {
    error = e;
  }

  Assert.ok(!notificationObserved, "a notification should not have been observed");
  Assert.ok(/Polling for changes failed/.test(error.message));
  // When an error occurs, last update was not overwritten.
  Assert.equal(Services.prefs.getIntPref(PREF_LAST_UPDATE), 42);
  // ensure that we've accumulated the correct telemetry
  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.SERVER_ERROR]: 1,
  };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);


add_task(async function test_check_clockskew_is_updated() {
  const serverTime = 2000;

  function serverResponse(request, response) {
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("Date", (new Date(serverTime)).toUTCString());
    response.write(JSON.stringify({data: []}));
    response.setStatusLine(null, 200, "OK");
  }
  server.registerPathHandler(CHANGES_PATH, serverResponse);

  let startTime = Date.now();

  await RemoteSettings.pollChanges();

  // How does the clock difference look?
  let endTime = Date.now();
  let clockDifference = Services.prefs.getIntPref(PREF_CLOCK_SKEW_SECONDS);
  // we previously set the serverTime to 2 (seconds past epoch)
  Assert.ok(clockDifference <= endTime / 1000
              && clockDifference >= Math.floor(startTime / 1000) - (serverTime / 1000));

  // check negative clock skew times
  // set to a time in the future
  server.registerPathHandler(CHANGES_PATH, serveChangesEntries(Date.now() + 10000, []));

  await RemoteSettings.pollChanges();

  clockDifference = Services.prefs.getIntPref(PREF_CLOCK_SKEW_SECONDS);
  // we previously set the serverTime to Date.now() + 10000 ms past epoch
  Assert.ok(clockDifference <= 0 && clockDifference >= -10);
});
add_task(clear_state);


add_task(async function test_backoff() {
  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);

  function simulateBackoffResponse(request, response) {
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("Backoff", "10");
    response.write(JSON.stringify({data: []}));
    response.setStatusLine(null, 200, "OK");
  }
  server.registerPathHandler(CHANGES_PATH, simulateBackoffResponse);

  // First will work.
  await RemoteSettings.pollChanges();
  // Second will fail because we haven't waited.
  try {
    await RemoteSettings.pollChanges();
    // The previous line should have thrown an error.
    Assert.ok(false);
  } catch (e) {
    Assert.ok(/Server is asking clients to back off; retry in \d+s./.test(e.message));
  }

  // Once backoff time has expired, polling for changes can start again.
  server.registerPathHandler(CHANGES_PATH, serveChangesEntries(12000, [{
    id: "6a733d4a-601e-11e8-837a-0f85257529a1",
    last_modified: 1300,
    host: "localhost",
    bucket: "some-bucket",
    collection: "some-collection"
  }]));
  Services.prefs.setCharPref(PREF_SETTINGS_SERVER_BACKOFF, `${Date.now() - 1000}`);

  await RemoteSettings.pollChanges();

  // Backoff tracking preference was cleared.
  Assert.ok(!Services.prefs.prefHasUserValue(PREF_SETTINGS_SERVER_BACKOFF));

  // Ensure that we've accumulated the correct telemetry
  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.SUCCESS]: 1,
    [UptakeTelemetry.STATUS.UP_TO_DATE]: 1,
    [UptakeTelemetry.STATUS.BACKOFF]: 1,
  };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);


add_task(async function test_network_error() {
  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);

  // Simulate a network error (to check telemetry report).
  Services.prefs.setCharPref(PREF_SETTINGS_SERVER, "http://localhost:42/v1");
  try {
    await RemoteSettings.pollChanges();
  } catch (e) {}

  // ensure that we've accumulated the correct telemetry
  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.NETWORK_ERROR]: 1,
  };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);


add_task(async function test_syncs_clients_with_local_database() {
  server.registerPathHandler(CHANGES_PATH, serveChangesEntries(42000, [{
    id: "d4a14f44-601f-11e8-8b8a-030f3dc5b844",
    last_modified: 10000,
    host: "localhost",
    bucket: "main",
    collection: "some-unknown"
  }, {
    id: "39f57e4e-6023-11e8-8b74-77c8dedfb389",
    last_modified: 9000,
    host: "localhost",
    bucket: "blocklists",
    collection: "addons"
  }, {
    id: "9a594c1a-601f-11e8-9c8a-33b2239d9113",
    last_modified: 8000,
    host: "localhost",
    bucket: "main",
    collection: "recipes"
  }]));

  // This simulates what remote-settings would do when initializing a local database.
  // We don't want to instantiate a client using the RemoteSettings() API
  // since we want to test «unknown» clients that have a local database.
  await (new Kinto.adapters.IDB("blocklists/addons")).saveLastModified(42);
  await (new Kinto.adapters.IDB("main/recipes")).saveLastModified(43);

  let error;
  try {
    await RemoteSettings.pollChanges();
  } catch (e) {
    error = e;
  }

  // The `main/some-unknown` should be skipped because it has no local database.
  // The `blocklists/addons` should be skipped because it is not the main bucket.
  // The `recipes` has a local database, and should cause a network error because the test
  // does not setup the server to receive the requests of `maybeSync()`.
  Assert.ok(/HTTP 404/.test(error.message), "server will return 404 on sync");
  Assert.equal(error.details.collection, "recipes");
});
add_task(clear_state);


add_task(async function test_syncs_clients_with_local_dump() {
  if (IS_ANDROID) {
    // Skip test: we don't ship remote settings dumps on Android (see package-manifest).
    return;
  }
  server.registerPathHandler(CHANGES_PATH, serveChangesEntries(42000, [{
    id: "d4a14f44-601f-11e8-8b8a-030f3dc5b844",
    last_modified: 10000,
    host: "localhost",
    bucket: "main",
    collection: "some-unknown"
  }, {
    id: "39f57e4e-6023-11e8-8b74-77c8dedfb389",
    last_modified: 9000,
    host: "localhost",
    bucket: "blocklists",
    collection: "addons"
  }, {
    id: "9a594c1a-601f-11e8-9c8a-33b2239d9113",
    last_modified: 8000,
    host: "localhost",
    bucket: "main",
    collection: "tippytop"
  }]));

  let error;
  try {
    await RemoteSettings.pollChanges();
  } catch (e) {
    error = e;
  }

  // The `main/some-unknown` should be skipped because it has no dump.
  // The `blocklists/addons` should be skipped because it is not the main bucket.
  // The `tippytop` has a dump, and should cause a network error because the test
  // does not setup the server to receive the requests of `maybeSync()`.
  Assert.ok(/HTTP 404/.test(error.message), "server will return 404 on sync");
  Assert.equal(error.details.collection, "tippytop");
});
add_task(clear_state);
