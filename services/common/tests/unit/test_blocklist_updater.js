Cu.import("resource://testing-common/httpd.js");
const { UptakeTelemetry } = Cu.import("resource://services-common/uptake-telemetry.js", {});

var server;

const PREF_SETTINGS_SERVER = "services.settings.server";
const PREF_SETTINGS_SERVER_BACKOFF = "services.settings.server.backoff";
const PREF_LAST_UPDATE = "services.blocklist.last_update_seconds";
const PREF_LAST_ETAG = "services.blocklist.last_etag";
const PREF_CLOCK_SKEW_SECONDS = "services.blocklist.clock_skew_seconds";

// Telemetry report result.
const TELEMETRY_HISTOGRAM_KEY = "settings-changes-monitoring";

// Check to ensure maybeSync is called with correct values when a changes
// document contains information on when a collection was last modified
add_task(async function test_check_maybeSync() {
  const changesPath = "/v1/buckets/monitor/collections/changes/records";

  // register a handler
  function handleResponse(serverTimeMillis, request, response) {
    try {
      const sampled = getSampleResponse(request, server.identity.primaryPort);
      if (!sampled) {
        do_throw(`unexpected ${request.method} request for ${request.path}?${request.queryString}`);
      }

      response.setStatusLine(null, sampled.status.status,
                             sampled.status.statusText);
      // send the headers
      for (let headerLine of sampled.sampleHeaders) {
        let headerElements = headerLine.split(":");
        response.setHeader(headerElements[0], headerElements[1].trimLeft());
      }

      // set the server date
      response.setHeader("Date", (new Date(serverTimeMillis)).toUTCString());

      response.write(sampled.responseBody);
    } catch (e) {
      dump(`${e}\n`);
    }
  }

  server.registerPathHandler(changesPath, handleResponse.bind(null, 2000));

  // set up prefs so the kinto updater talks to the test server
  Services.prefs.setCharPref(PREF_SETTINGS_SERVER,
    `http://localhost:${server.identity.primaryPort}/v1`);

  // set some initial values so we can check these are updated appropriately
  Services.prefs.setIntPref(PREF_LAST_UPDATE, 0);
  Services.prefs.setIntPref(PREF_CLOCK_SKEW_SECONDS, 0);
  Services.prefs.clearUserPref(PREF_LAST_ETAG);


  let startTime = Date.now();

  let updater = Cu.import("resource://services-common/blocklist-updater.js", {});

  // ensure we get the maybeSync call
  // add a test kinto client that will respond to lastModified information
  // for a collection called 'test-collection'
  updater.addTestBlocklistClient("test-collection", {
    bucketName: "blocklists",
    maybeSync(lastModified, serverTime) {
      Assert.equal(lastModified, 1000);
      Assert.equal(serverTime, 2000);
    }
  });

  const startHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);

  let notificationObserved = false;

  // Ensure that the blocklist-updater-versions-checked notification works
  let certblockObserver = {
    observe(aSubject, aTopic, aData) {
      Services.obs.removeObserver(this, "blocklist-updater-versions-checked");
      notificationObserved = true;
    }
  };

  Services.obs.addObserver(certblockObserver, "blocklist-updater-versions-checked");

  await updater.checkVersions();

  Assert.ok(notificationObserved, "a notification should have been observed");

  // check the last_update is updated
  Assert.equal(Services.prefs.getIntPref(PREF_LAST_UPDATE), 2);

  // How does the clock difference look?
  let endTime = Date.now();
  let clockDifference = Services.prefs.getIntPref(PREF_CLOCK_SKEW_SECONDS);
  // we previously set the serverTime to 2 (seconds past epoch)
  Assert.ok(clockDifference <= endTime / 1000
              && clockDifference >= Math.floor(startTime / 1000) - 2);
  // Last timestamp was saved. An ETag header value is a quoted string.
  let lastEtag = Services.prefs.getCharPref(PREF_LAST_ETAG);
  Assert.equal(lastEtag, "\"1100\"");

  // Simulate a poll with up-to-date collection.
  Services.prefs.setIntPref(PREF_LAST_UPDATE, 0);
  // If server has no change, a 304 is received, maybeSync() is not called.
  updater.addTestBlocklistClient("test-collection", {
    maybeSync: () => { throw new Error("Should not be called"); }
  });
  await updater.checkVersions();
  // Last update is overwritten
  Assert.equal(Services.prefs.getIntPref(PREF_LAST_UPDATE), 2);


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
  server.registerPathHandler(changesPath, simulateErrorResponse);

  // checkVersions() fails with adequate error and no notification.
  let error;
  notificationObserved = false;
  Services.obs.addObserver(certblockObserver, "blocklist-updater-versions-checked");
  try {
    await updater.checkVersions();
  } catch (e) {
    error = e;
  }
  Assert.ok(!notificationObserved, "a notification should not have been observed");
  Assert.ok(/Polling for changes failed/.test(error.message));
  // When an error occurs, last update was not overwritten (see Date header above).
  Assert.equal(Services.prefs.getIntPref(PREF_LAST_UPDATE), 2);

  // check negative clock skew times

  // set to a time in the future
  server.registerPathHandler(changesPath, handleResponse.bind(null, Date.now() + 10000));

  await updater.checkVersions();

  clockDifference = Services.prefs.getIntPref(PREF_CLOCK_SKEW_SECONDS);
  // we previously set the serverTime to Date.now() + 10000 ms past epoch
  Assert.ok(clockDifference <= 0 && clockDifference >= -10);

  //
  // Backoff
  //
  function simulateBackoffResponse(request, response) {
    response.setHeader("Content-Type", "application/json; charset=UTF-8");
    response.setHeader("Backoff", "10");
    response.write(JSON.stringify({data: []}));
    response.setStatusLine(null, 200, "OK");
  }
  server.registerPathHandler(changesPath, simulateBackoffResponse);
  // First will work.
  await updater.checkVersions();
  // Second will fail because we haven't waited.
  try {
    await updater.checkVersions();
    // The previous line should have thrown an error.
    Assert.ok(false);
  } catch (e) {
    Assert.ok(/Server is asking clients to back off; retry in \d+s./.test(e.message));
  }
  // Once backoff time has expired, polling for changes can start again.
  server.registerPathHandler(changesPath, handleResponse.bind(null, 2000));
  Services.prefs.setCharPref(PREF_SETTINGS_SERVER_BACKOFF, `${Date.now() - 1000}`);
  await updater.checkVersions();
  // Backoff tracking preference was cleared.
  Assert.ok(!Services.prefs.prefHasUserValue(PREF_SETTINGS_SERVER_BACKOFF));


  // Simulate a network error (to check telemetry report).
  Services.prefs.setCharPref(PREF_SETTINGS_SERVER, "http://localhost:42/v1");
  try {
    await updater.checkVersions();
  } catch (e) {}

  const endHistogram = getUptakeTelemetrySnapshot(TELEMETRY_HISTOGRAM_KEY);
  // ensure that we've accumulated the correct telemetry
  const expectedIncrements = {
    [UptakeTelemetry.STATUS.UP_TO_DATE]: 4,
    [UptakeTelemetry.STATUS.SUCCESS]: 1,
    [UptakeTelemetry.STATUS.BACKOFF]: 1,
    [UptakeTelemetry.STATUS.SERVER_ERROR]: 1,
    [UptakeTelemetry.STATUS.NETWORK_ERROR]: 1,
    [UptakeTelemetry.STATUS.UNKNOWN_ERROR]: 0,
  };
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});

function run_test() {
  // Set up an HTTP Server
  server = new HttpServer();
  server.start(-1);

  run_next_test();

  registerCleanupFunction(function() {
    server.stop(function() { });
  });
}

// get a response for a given request from sample data
function getSampleResponse(req, port) {
  const responses = {
    "GET:/v1/buckets/monitor/collections/changes/records?": {
      "sampleHeaders": [
        "Content-Type: application/json; charset=UTF-8",
        "ETag: \"1100\""
      ],
      "status": {status: 200, statusText: "OK"},
      "responseBody": JSON.stringify({"data": [{
        "host": "localhost",
        "last_modified": 1100,
        "bucket": "blocklists:aurora",
        "id": "330a0c5f-fadf-ff0b-40c8-4eb0d924ff6a",
        "collection": "test-collection"
      }, {
        "host": "localhost",
        "last_modified": 1000,
        "bucket": "blocklists",
        "id": "254cbb9e-6888-4d9f-8e60-58b74faa8778",
        "collection": "test-collection"
      }]})
    }
  };

  if (req.hasHeader("if-none-match") && req.getHeader("if-none-match", "") == "\"1100\"")
    return {sampleHeaders: [], status: {status: 304, statusText: "Not Modified"}, responseBody: ""};

  return responses[`${req.method}:${req.path}?${req.queryString}`] ||
         responses[req.method];
}
