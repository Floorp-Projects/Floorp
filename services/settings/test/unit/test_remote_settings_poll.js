/* import-globals-from ../../../common/tests/unit/head_helpers.js */

const { AppConstants } = ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { setTimeout } = ChromeUtils.import("resource://gre/modules/Timer.jsm");

const { UptakeTelemetry } = ChromeUtils.import("resource://services-common/uptake-telemetry.js");
const { Kinto } = ChromeUtils.import("resource://services-common/kinto-offline-client.js");
const { pushBroadcastService } = ChromeUtils.import("resource://gre/modules/PushBroadcastService.jsm");
const {
  RemoteSettings,
  remoteSettingsBroadcastHandler,
  BROADCAST_ID,
} = ChromeUtils.import("resource://services-settings/remote-settings.js");
const { Utils } = ChromeUtils.import("resource://services-settings/Utils.jsm");
const { TelemetryTestUtils } = ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm");


const IS_ANDROID = AppConstants.platform == "android";

const PREF_SETTINGS_SERVER = "services.settings.server";
const PREF_SETTINGS_SERVER_BACKOFF = "services.settings.server.backoff";
const PREF_LAST_UPDATE = "services.settings.last_update_seconds";
const PREF_LAST_ETAG = "services.settings.last_etag";
const PREF_CLOCK_SKEW_SECONDS = "services.settings.clock_skew_seconds";

const DB_NAME = "remote-settings";
// Telemetry report result.
const TELEMETRY_HISTOGRAM_POLL_KEY = "settings-changes-monitoring";
const TELEMETRY_HISTOGRAM_SYNC_KEY = "settings-sync";
const CHANGES_PATH = "/v1" + Utils.CHANGES_PATH;

var server;

async function clear_state() {
  // set up prefs so the kinto updater talks to the test server
  Services.prefs.setCharPref(PREF_SETTINGS_SERVER,
    `http://localhost:${server.identity.primaryPort}/v1`);

  // set some initial values so we can check these are updated appropriately
  Services.prefs.setIntPref(PREF_LAST_UPDATE, 0);
  Services.prefs.setIntPref(PREF_CLOCK_SKEW_SECONDS, 0);
  Services.prefs.clearUserPref(PREF_LAST_ETAG);

  // Clear events snapshot.
  TelemetryTestUtils.assertEvents([], {}, {process: "dummy"});
}

function serveChangesEntries(serverTime, entries) {
  return (request, response) => {
    response.setStatusLine(null, 200, "OK");
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("Date", (new Date(serverTime)).toUTCString());
    if (entries.length) {
      const latest = entries[0].last_modified;
      response.setHeader("ETag", `"${latest}"`);
      response.setHeader("Last-Modified", (new Date(latest)).toGMTString());
    }
    response.write(JSON.stringify({ "data": entries }));
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


add_task(async function test_an_event_is_sent_on_start() {
  server.registerPathHandler(CHANGES_PATH, (request, response) => {
    response.write(JSON.stringify({ data: [] }));
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("ETag", '"42"');
    response.setHeader("Date", (new Date()).toUTCString());
    response.setStatusLine(null, 200, "OK");
  });
  let notificationObserved = null;
  const observer = {
    observe(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, "remote-settings:changes-poll-start");
      notificationObserved = JSON.parse(aData);
    },
  };
  Services.obs.addObserver(observer, "remote-settings:changes-poll-start");

  await RemoteSettings.pollChanges({ expectedTimestamp: 13 });

  Assert.equal(notificationObserved.expectedTimestamp, 13, "start notification should have been observed");
});
add_task(clear_state);


add_task(async function test_check_success() {
  const startPollHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);
  const startSyncHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_SYNC_KEY);
  const serverTime = 8000;

  server.registerPathHandler(CHANGES_PATH, serveChangesEntries(serverTime, [{
    id: "330a0c5f-fadf-ff0b-40c8-4eb0d924ff6a",
    last_modified: 1100,
    host: "localhost",
    bucket: "some-other-bucket",
    collection: "test-collection",
  }, {
    id: "254cbb9e-6888-4d9f-8e60-58b74faa8778",
    last_modified: 1000,
    host: "localhost",
    bucket: "test-bucket",
    collection: "test-collection",
  }]));

  // add a test kinto client that will respond to lastModified information
  // for a collection called 'test-collection'.
  // Let's use a bucket that is not the default one (`test-bucket`).
  Services.prefs.setCharPref("services.settings.test_bucket", "test-bucket");
  const c = RemoteSettings("test-collection", { bucketNamePref: "services.settings.test_bucket" });
  let maybeSyncCalled = false;
  c.maybeSync = () => { maybeSyncCalled = true; };

  // Ensure that the remote-settings:changes-poll-end notification works
  let notificationObserved = false;
  const observer = {
    observe(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, "remote-settings:changes-poll-end");
      notificationObserved = true;
    },
  };
  Services.obs.addObserver(observer, "remote-settings:changes-poll-end");

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
  const endPollHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.SUCCESS]: 1,
  };
  checkUptakeTelemetry(startPollHistogram, endPollHistogram, expectedIncrements);
  const endSyncHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_SYNC_KEY);
  checkUptakeTelemetry(startSyncHistogram, endSyncHistogram, expectedIncrements);
});
add_task(clear_state);


add_task(async function test_update_timer_interface() {
  const remoteSettings = Cc["@mozilla.org/services/settings;1"]
    .getService(Ci.nsITimerCallback);

  const serverTime = 8000;
  server.registerPathHandler(CHANGES_PATH, serveChangesEntries(serverTime, [{
    id: "028261ad-16d4-40c2-a96a-66f72914d125",
    last_modified: 42,
    host: "localhost",
    bucket: "main",
    collection: "whatever-collection",
  }]));

  await new Promise((resolve) => {
    const e = "remote-settings:changes-poll-end";
    const changesPolledObserver = {
      observe(aSubject, aTopic, aData) {
        Services.obs.removeObserver(this, e);
        resolve();
      },
    };
    Services.obs.addObserver(changesPolledObserver, e);
    remoteSettings.notify(null);
  });

  // Everything went fine.
  Assert.equal(Services.prefs.getCharPref(PREF_LAST_ETAG), "\"42\"");
  Assert.equal(Services.prefs.getIntPref(PREF_LAST_UPDATE), serverTime / 1000);
});
add_task(clear_state);


add_task(async function test_check_up_to_date() {
  // Simulate a poll with up-to-date collection.
  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);

  const serverTime = 4000;
  function server304(request, response) {
    if (request.hasHeader("if-none-match") && request.getHeader("if-none-match") == "\"1100\"") {
      response.setHeader("Date", (new Date(serverTime)).toUTCString());
      response.setStatusLine(null, 304, "Service Not Modified");
    }
  }
  server.registerPathHandler(CHANGES_PATH, server304);

  Services.prefs.setCharPref(PREF_LAST_ETAG, '"1100"');

  // Ensure that the remote-settings:changes-poll-end notification is sent.
  let notificationObserved = false;
  const observer = {
    observe(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, "remote-settings:changes-poll-end");
      notificationObserved = true;
    },
  };
  Services.obs.addObserver(observer, "remote-settings:changes-poll-end");

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
  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.UP_TO_DATE]: 1,
  };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);


add_task(async function test_expected_timestamp() {
  function withCacheBust(request, response) {
    const entries = [{
      id: "695c2407-de79-4408-91c7-70720dd59d78",
      last_modified: 1100,
      host: "localhost",
      bucket: "main",
      collection: "with-cache-busting",
    }];
    if (request.queryString == `_expected=${encodeURIComponent('"42"')}`) {
      response.write(JSON.stringify({
        data: entries,
      }));
    }
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("ETag", '"1100"');
    response.setHeader("Date", (new Date()).toUTCString());
    response.setStatusLine(null, 200, "OK");
  }
  server.registerPathHandler(CHANGES_PATH, withCacheBust);

  const c = RemoteSettings("with-cache-busting");
  let maybeSyncCalled = false;
  c.maybeSync = () => { maybeSyncCalled = true; };

  await RemoteSettings.pollChanges({ expectedTimestamp: '"42"'});

  Assert.ok(maybeSyncCalled, "maybeSync was called");
});
add_task(clear_state);


add_task(async function test_client_last_check_is_saved() {
  server.registerPathHandler(CHANGES_PATH, (request, response) => {
      response.write(JSON.stringify({
      data: [{
        id: "695c2407-de79-4408-91c7-70720dd59d78",
        last_modified: 1100,
        host: "localhost",
        bucket: "main",
        collection: "models-recipes",
      }],
    }));
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("ETag", '"42"');
    response.setHeader("Date", (new Date()).toUTCString());
    response.setStatusLine(null, 200, "OK");
  });

  const c = RemoteSettings("models-recipes");
  c.maybeSync = () => {};

  equal(c.lastCheckTimePref, "services.settings.main.models-recipes.last_check");
  Services.prefs.setIntPref(c.lastCheckTimePref, 0);

  await RemoteSettings.pollChanges({ expectedTimestamp: '"42"' });

  notEqual(Services.prefs.getIntPref(c.lastCheckTimePref), 0);
});
add_task(clear_state);


add_task(async function test_age_of_data_is_reported_in_uptake_status() {
  await withFakeChannel("nightly", async () => {
    const serverTime = 1552323900000;
    server.registerPathHandler(CHANGES_PATH, serveChangesEntries(serverTime, [{
      id: "b6ba7fab-a40a-4d03-a4af-6b627f3c5b36",
      last_modified: serverTime - 3600 * 1000,
      host: "localhost",
      bucket: "main",
      collection: "some-entry",
    }]));

    await RemoteSettings.pollChanges();

    TelemetryTestUtils.assertEvents([
      ["uptake.remotecontent.result", "uptake", "remotesettings", UptakeTelemetry.STATUS.SUCCESS,
        { source: TELEMETRY_HISTOGRAM_POLL_KEY, age: "3600", trigger: "manual" }],
      ["uptake.remotecontent.result", "uptake", "remotesettings", UptakeTelemetry.STATUS.SUCCESS,
        { source: TELEMETRY_HISTOGRAM_SYNC_KEY, duration: () => true, trigger: "manual" }],
    ]);
  });
});
add_task(clear_state);

add_task(async function test_synchronization_duration_is_reported_in_uptake_status() {
  await withFakeChannel("nightly", async () => {
    server.registerPathHandler(CHANGES_PATH, serveChangesEntries(10000, [{
      id: "b6ba7fab-a40a-4d03-a4af-6b627f3c5b36",
      last_modified: 42,
      host: "localhost",
      bucket: "main",
      collection: "some-entry",
    }]));
    const c = RemoteSettings("some-entry");
    // Simulate a synchronization that lasts 1 sec.
    // eslint-disable-next-line mozilla/no-arbitrary-setTimeout
    c.maybeSync = () => new Promise(resolve => setTimeout(resolve, 1000));

    await RemoteSettings.pollChanges();

    TelemetryTestUtils.assertEvents([
      ["uptake.remotecontent.result", "uptake", "remotesettings", "success",
        { source: TELEMETRY_HISTOGRAM_POLL_KEY, age: () => true, trigger: "manual" }],
      ["uptake.remotecontent.result", "uptake", "remotesettings", "success",
        { source: TELEMETRY_HISTOGRAM_SYNC_KEY, duration: (v) => v >= 1000, trigger: "manual" }],
    ]);
  });
});
add_task(clear_state);

add_task(async function test_success_with_partial_list() {
  function partialList(request, response) {
    const entries = [{
      id: "028261ad-16d4-40c2-a96a-66f72914d125",
      last_modified: 43,
      host: "localhost",
      bucket: "main",
      collection: "cid-1",
    }, {
      id: "98a34576-bcd6-423f-abc2-1d290b776ed8",
      last_modified: 42,
      host: "localhost",
      bucket: "main",
      collection: "poll-test-collection",
    }];
    if (request.queryString == `_since=${encodeURIComponent('"42"')}`) {
      response.write(JSON.stringify({
        data: entries.slice(0, 1),
      }));
      response.setHeader("ETag", '"43"');
    } else {
      response.write(JSON.stringify({
        data: entries,
      }));
      response.setHeader("ETag", '"42"');
    }
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("Date", (new Date()).toUTCString());
    response.setStatusLine(null, 200, "OK");
  }
  server.registerPathHandler(CHANGES_PATH, partialList);

  const c = RemoteSettings("poll-test-collection");
  let maybeSyncCount = 0;
  c.maybeSync = () => { maybeSyncCount++; };

  await RemoteSettings.pollChanges();
  await RemoteSettings.pollChanges();

  // On the second call, the server does not mention the poll-test-collection
  // and maybeSync() is not called.
  Assert.equal(maybeSyncCount, 1, "maybeSync should not be called twice");
});
add_task(clear_state);


add_task(async function test_server_bad_json() {
  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);

  function simulateBadJSON(request, response) {
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.write("<html></html>");
    response.setStatusLine(null, 200, "OK");
  }
  server.registerPathHandler(CHANGES_PATH, simulateBadJSON);

  let error;
  try {
    await RemoteSettings.pollChanges();
  } catch (e) {
    error = e;
  }
  Assert.ok(/JSON.parse: unexpected character/.test(error.message));

  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.PARSE_ERROR]: 1,
  };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);


add_task(async function test_server_bad_content_type() {
  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);

  function simulateBadContentType(request, response) {
    response.setHeader("Content-Type", "text/html");
    response.write("<html></html>");
    response.setStatusLine(null, 200, "OK");
  }
  server.registerPathHandler(CHANGES_PATH, simulateBadContentType);

  let error;
  try {
    await RemoteSettings.pollChanges();
  } catch (e) {
    error = e;
  }
  Assert.ok(/Unexpected content-type/.test(error.message));

  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.CONTENT_ERROR]: 1,
  };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);


add_task(async function test_server_404_response() {
  function simulateDummy404(request, response) {
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.write("<html></html>");
    response.setStatusLine(null, 404, "OK");
  }
  server.registerPathHandler(CHANGES_PATH, simulateDummy404);

  await RemoteSettings.pollChanges(); // Does not fail when running from tests.
});
add_task(clear_state);


add_task(async function test_server_error() {
  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);

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
      Services.obs.removeObserver(this, "remote-settings:changes-poll-end");
      notificationObserved = true;
    },
  };
  Services.obs.addObserver(observer, "remote-settings:changes-poll-end");
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
  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.SERVER_ERROR]: 1,
  };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);

add_task(async function test_client_error() {
  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_SYNC_KEY);

  server.registerPathHandler(CHANGES_PATH, serveChangesEntries(10000, [{
    id: "b6ba7fab-a40a-4d03-a4af-6b627f3c5b36",
    last_modified: 42,
    host: "localhost",
    bucket: "main",
    collection: "some-entry",
  }]));
  const c = RemoteSettings("some-entry");
  c.maybeSync = () => { throw new Error("boom"); };

  let notificationObserved = false;
  const observer = {
    observe(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, "remote-settings:changes-poll-end");
      notificationObserved = true;
    },
  };
  Services.obs.addObserver(observer, "remote-settings:changes-poll-end");
  Services.prefs.setIntPref(PREF_LAST_ETAG, 42);

  // pollChanges() fails with adequate error and no notification.
  let error;
  try {
    await RemoteSettings.pollChanges();
  } catch (e) {
    error = e;
  }

  Assert.ok(!notificationObserved, "a notification should not have been observed");
  Assert.ok(/boom/.test(error.message), "original client error is thrown");
  // When an error occurs, last etag was not overwritten.
  Assert.equal(Services.prefs.getIntPref(PREF_LAST_ETAG), 42);
  // ensure that we've accumulated the correct telemetry
  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_SYNC_KEY);
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.SYNC_ERROR]: 1,
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


add_task(async function test_check_clockskew_takes_age_into_account() {
  const currentTime = Date.now();
  const skewSeconds = 5;
  const ageCDNSeconds = 3600;
  const serverTime = currentTime - (skewSeconds * 1000) - (ageCDNSeconds * 1000);

  function serverResponse(request, response) {
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("Date", (new Date(serverTime)).toUTCString());
    response.setHeader("Age", `${ageCDNSeconds}`);
    response.write(JSON.stringify({data: []}));
    response.setStatusLine(null, 200, "OK");
  }
  server.registerPathHandler(CHANGES_PATH, serverResponse);

  await RemoteSettings.pollChanges();

  const clockSkew = Services.prefs.getIntPref(PREF_CLOCK_SKEW_SECONDS);
  Assert.ok(clockSkew >= skewSeconds, `clockSkew is ${clockSkew}`);
});
add_task(clear_state);


add_task(async function test_backoff() {
  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);

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
    collection: "some-collection",
  }]));
  Services.prefs.setCharPref(PREF_SETTINGS_SERVER_BACKOFF, `${Date.now() - 1000}`);

  await RemoteSettings.pollChanges();

  // Backoff tracking preference was cleared.
  Assert.ok(!Services.prefs.prefHasUserValue(PREF_SETTINGS_SERVER_BACKOFF));

  // Ensure that we've accumulated the correct telemetry
  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.SUCCESS]: 1,
    [UptakeTelemetry.STATUS.UP_TO_DATE]: 1,
    [UptakeTelemetry.STATUS.BACKOFF]: 1,
  };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});
add_task(clear_state);


add_task(async function test_network_error() {
  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);

  // Simulate a network error (to check telemetry report).
  Services.prefs.setCharPref(PREF_SETTINGS_SERVER, "http://localhost:42/v1");
  try {
    await RemoteSettings.pollChanges();
  } catch (e) {}

  // ensure that we've accumulated the correct telemetry
  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_POLL_KEY);
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
    collection: "some-unknown",
  }, {
    id: "39f57e4e-6023-11e8-8b74-77c8dedfb389",
    last_modified: 9000,
    host: "localhost",
    bucket: "blocklists",
    collection: "addons",
  }, {
    id: "9a594c1a-601f-11e8-9c8a-33b2239d9113",
    last_modified: 8000,
    host: "localhost",
    bucket: "main",
    collection: "recipes",
  }]));

  // This simulates what remote-settings would do when initializing a local database.
  // We don't want to instantiate a client using the RemoteSettings() API
  // since we want to test «unknown» clients that have a local database.
  await (new Kinto.adapters.IDB("blocklists/addons", { dbName: DB_NAME })).saveLastModified(42);
  await (new Kinto.adapters.IDB("main/recipes", { dbName: DB_NAME })).saveLastModified(43);

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
    collection: "some-unknown",
  }, {
    id: "39f57e4e-6023-11e8-8b74-77c8dedfb389",
    last_modified: 9000,
    host: "localhost",
    bucket: "blocklists",
    collection: "addons",
  }, {
    id: "9a594c1a-601f-11e8-9c8a-33b2239d9113",
    last_modified: 8000,
    host: "localhost",
    bucket: "main",
    collection: "example",
  }]));

  let error;
  try {
    await RemoteSettings.pollChanges();
  } catch (e) {
    error = e;
  }

  // The `main/some-unknown` should be skipped because it has no dump.
  // The `blocklists/addons` should be skipped because it is not the main bucket.
  // The `example` has a dump, and should cause a network error because the test
  // does not setup the server to receive the requests of `maybeSync()`.
  Assert.ok(/HTTP 404/.test(error.message), "server will return 404 on sync");
  Assert.equal(error.details.collection, "example");
});
add_task(clear_state);


add_task(async function test_adding_client_resets_polling() {
  function serve200or304(request, response) {
    const entries = [{
      id: "aa71e6cc-9f37-447a-b6e0-c025e8eabd03",
      last_modified: 42,
      host: "localhost",
      bucket: "main",
      collection: "a-collection",
    }];
    if (request.queryString == `_since=${encodeURIComponent('"42"')}`) {
      response.write(JSON.stringify({
        data: entries.slice(0, 1),
      }));
      response.setHeader("ETag", '"42"');
      response.setStatusLine(null, 304, "Not Modified");
    } else {
      response.write(JSON.stringify({
        data: entries,
      }));
      response.setHeader("ETag", '"42"');
      response.setStatusLine(null, 200, "OK");
    }
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("Date", (new Date()).toUTCString());
  }
  server.registerPathHandler(CHANGES_PATH, serve200or304);

  // Poll once, without any client for "a-collection"
  await RemoteSettings.pollChanges();

  // Register a new client.
  let maybeSyncCalled = false;
  const c = RemoteSettings("a-collection");
  c.maybeSync = () => { maybeSyncCalled = true; };

  // Poll again.
  await RemoteSettings.pollChanges();

  // The new client was called, even if the server data didn't change.
  Assert.ok(maybeSyncCalled);

  // Poll again. This time maybeSync() won't be called.
  maybeSyncCalled = false;
  await RemoteSettings.pollChanges();
  Assert.ok(!maybeSyncCalled);
});
add_task(clear_state);


add_task(async function test_broadcast_handler_passes_version_and_trigger_values() {
  // The polling will use the broadcast version as cache busting query param.
  let passedQueryString;
  function serveCacheBusted(request, response) {
    passedQueryString = request.queryString;
    const entries = [{
      id: "b6ba7fab-a40a-4d03-a4af-6b627f3c5b36",
      last_modified: 42,
      host: "localhost",
      bucket: "main",
      collection: "from-broadcast",
    }];
    response.write(JSON.stringify({
      data: entries,
    }));
    response.setHeader("ETag", '"42"');
    response.setStatusLine(null, 200, "OK");
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("Date", (new Date()).toUTCString());
  }
  server.registerPathHandler(CHANGES_PATH, serveCacheBusted);

  let passedTrigger;
  const c = RemoteSettings("from-broadcast");
  c.maybeSync = (last_modified, { trigger }) => {
    passedTrigger = trigger;
  };

  const version = "1337";

  let context = { phase: pushBroadcastService.PHASES.HELLO };
  await remoteSettingsBroadcastHandler.receivedBroadcastMessage(version, BROADCAST_ID, context);
  Assert.equal(passedTrigger, "startup");
  Assert.equal(passedQueryString, `_expected=${version}`);

  clear_state();

  context = { phase: pushBroadcastService.PHASES.REGISTER };
  await remoteSettingsBroadcastHandler.receivedBroadcastMessage(version, BROADCAST_ID, context);
  Assert.equal(passedTrigger, "startup");

  clear_state();

  context = { phase: pushBroadcastService.PHASES.BROADCAST };
  await remoteSettingsBroadcastHandler.receivedBroadcastMessage(version, BROADCAST_ID, context);
  Assert.equal(passedTrigger, "broadcast");
});
add_task(clear_state);
