/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that TelemetrySession notifies correctly on idle-daily.

ChromeUtils.import("resource://testing-common/httpd.js", this);
ChromeUtils.import("resource://gre/modules/PromiseUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Services.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryStorage.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetrySession.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetrySend.jsm", this);

var gHttpServer = null;

add_task(async function test_setup() {
  do_get_profile();

  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  Services.prefs.setBoolPref(TelemetryUtils.Preferences.FhrUploadEnabled, true);

  // Start the webserver to check if the pending ping correctly arrives.
  gHttpServer = new HttpServer();
  gHttpServer.start(-1);
});

add_task(async function testSendPendingOnIdleDaily() {
  // Create a valid pending ping.
  const PENDING_PING = {
    id: "2133234d-4ea1-44f4-909e-ce8c6c41e0fc",
    type: "test-ping",
    version: 4,
    application: {},
    payload: {},
  };
  await TelemetryStorage.savePing(PENDING_PING, true);

  // Telemetry will not send this ping at startup, because it's not overdue.
  await TelemetryController.testSetup();
  TelemetrySend.setServer(
    "http://localhost:" + gHttpServer.identity.primaryPort
  );

  let pendingPromise = new Promise(resolve =>
    gHttpServer.registerPrefixHandler("/submit/telemetry/", request =>
      resolve(request)
    )
  );

  let gatherPromise = PromiseUtils.defer();
  Services.obs.addObserver(gatherPromise.resolve, "gather-telemetry");

  // Check that we are correctly receiving the gather-telemetry notification.
  TelemetrySession.observe(null, "idle-daily", null);
  await gatherPromise.promise;
  Assert.ok(true, "Received gather-telemetry notification.");

  Services.obs.removeObserver(gatherPromise.resolve, "gather-telemetry");

  // Check that the pending ping is correctly received.
  let module = ChromeUtils.import(
    "resource://gre/modules/TelemetrySend.jsm",
    null
  );
  module.TelemetrySendImpl.observe(null, "idle-daily", null);
  let request = await pendingPromise;
  let ping = decodeRequestPayload(request);

  // Validate the ping data.
  Assert.equal(ping.id, PENDING_PING.id);
  Assert.equal(ping.type, PENDING_PING.type);

  await new Promise(resolve => gHttpServer.stop(resolve));
});
