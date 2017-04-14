/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Check that TelemetrySession notifies correctly on idle-daily.

Cu.import("resource://testing-common/httpd.js", this);
Cu.import("resource://gre/modules/PromiseUtils.jsm", this);
Cu.import("resource://gre/modules/Services.jsm", this);
Cu.import("resource://gre/modules/Task.jsm", this);
Cu.import("resource://gre/modules/TelemetryStorage.jsm", this);
Cu.import("resource://gre/modules/TelemetryController.jsm", this);
Cu.import("resource://gre/modules/TelemetrySession.jsm", this);
Cu.import("resource://gre/modules/TelemetrySend.jsm", this);

const PREF_FHR_UPLOAD_ENABLED = "datareporting.healthreport.uploadEnabled";

var gHttpServer = null;

add_task(function* test_setup() {
  do_get_profile();

  // Make sure we don't generate unexpected pings due to pref changes.
  yield setEmptyPrefWatchlist();

  Services.prefs.setBoolPref(PREF_TELEMETRY_ENABLED, true);
  Services.prefs.setBoolPref(PREF_FHR_UPLOAD_ENABLED, true);

  // Start the webserver to check if the pending ping correctly arrives.
  gHttpServer = new HttpServer();
  gHttpServer.start(-1);
});

add_task(function* testSendPendingOnIdleDaily() {
  // Create a valid pending ping.
  const PENDING_PING = {
    id: "2133234d-4ea1-44f4-909e-ce8c6c41e0fc",
    type: "test-ping",
    version: 4,
    application: {},
    payload: {},
  };
  yield TelemetryStorage.savePing(PENDING_PING, true);

  // Telemetry will not send this ping at startup, because it's not overdue.
  yield TelemetryController.testSetup();
  TelemetrySend.setServer("http://localhost:" + gHttpServer.identity.primaryPort);

  let pendingPromise = new Promise(resolve =>
    gHttpServer.registerPrefixHandler("/submit/telemetry/", request => resolve(request)));

  let gatherPromise = PromiseUtils.defer();
  Services.obs.addObserver(gatherPromise.resolve, "gather-telemetry");

  // Check that we are correctly receiving the gather-telemetry notification.
  TelemetrySession.observe(null, "idle-daily", null);
  yield gatherPromise;
  Assert.ok(true, "Received gather-telemetry notification.");

  Services.obs.removeObserver(gatherPromise.resolve, "gather-telemetry");

  // Check that the pending ping is correctly received.
  let ns = {};
  let module = Cu.import("resource://gre/modules/TelemetrySend.jsm", ns);
  module.TelemetrySendImpl.observe(null, "idle-daily", null);
  let request = yield pendingPromise;
  let ping = decodeRequestPayload(request);

  // Validate the ping data.
  Assert.equal(ping.id, PENDING_PING.id);
  Assert.equal(ping.type, PENDING_PING.type);

  yield new Promise(resolve => gHttpServer.stop(resolve));
});
