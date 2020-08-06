/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/
 */

"use strict";

ChromeUtils.import("resource://gre/modules/TelemetryController.jsm", this);
ChromeUtils.import("resource://gre/modules/TelemetryUtils.jsm", this);
ChromeUtils.import("resource://gre/modules/Preferences.jsm", this);

XPCOMUtils.defineLazyModuleGetters(this, {
  ONLOGIN_NOTIFICATION: "resource://gre/modules/FxAccountsCommon.js",
  ONLOGOUT_NOTIFICATION: "resource://gre/modules/FxAccountsCommon.js",
  ONVERIFIED_NOTIFICATION: "resource://gre/modules/FxAccountsCommon.js",
});
ChromeUtils.defineModuleGetter(
  this,
  "EcosystemTelemetry",
  "resource://gre/modules/EcosystemTelemetry.jsm"
);

const TEST_PING_TYPE = "test-ping-type";

const RE_VALID_GUID = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/;

function fakeIdleNotification(topic) {
  let scheduler = ChromeUtils.import(
    "resource://gre/modules/TelemetryScheduler.jsm",
    null
  );
  return scheduler.TelemetryScheduler.observe(null, topic, null);
}

async function promiseNoPing() {
  // We check there's not one of our pings pending by sending a test ping, then
  // immediately fetching a pending ping and checking it's that test one.
  TelemetryController.submitExternalPing(TEST_PING_TYPE, {}, {});
  let ping = await PingServer.promiseNextPing();
  Assert.equal(ping.type, TEST_PING_TYPE, "Should be a test ping.");
}

function checkPingStructure(ping, reason) {
  Assert.equal(
    ping.type,
    EcosystemTelemetry.PING_TYPE,
    "Should be an ecosystem ping."
  );

  Assert.ok(!("clientId" in ping), "Ping must not contain a client ID.");
  Assert.ok("environment" in ping, "Ping must contain an environment.");
  let environment = ping.environment;

  // Check that the environment is indeed minimal
  const ALLOWED_ENVIRONMENT_KEYS = ["settings", "system", "profile"];
  Assert.deepEqual(
    ALLOWED_ENVIRONMENT_KEYS,
    Object.keys(environment),
    "Environment should only contain a limited set of keys."
  );

  // Check that fields of the environment are indeed minimal
  Assert.deepEqual(
    ["locale"],
    Object.keys(environment.settings),
    "Settings environment should only contain locale"
  );
  Assert.deepEqual(
    ["cpu", "memoryMB", "os"],
    Object.keys(environment.system).sort(),
    "System environment should contain a limited set of keys"
  );
  Assert.deepEqual(
    ["locale", "name", "version"],
    Object.keys(environment.system.os).sort(),
    "system.environment.os should contain a limited set of keys"
  );

  // Check the payload for required fields.
  let payload = ping.payload;
  Assert.equal(payload.reason, reason, "Ping reason must match.");
  Assert.ok(
    payload.duration >= 0,
    "Payload must have a duration greater or equal to 0"
  );
  Assert.ok("ecosystemAnonId" in payload, "payload must have ecosystemAnonId");
  Assert.ok(
    RE_VALID_GUID.test(payload.ecosystemClientId),
    "ecosystemClientId must be a valid GUID"
  );

  Assert.ok("scalars" in payload, "Payload must contain scalars");
  Assert.ok("keyedScalars" in payload, "Payload must contain keyed scalars");
  Assert.ok("histograms" in payload, "Payload must contain histograms");
  Assert.ok(
    "keyedHistograms" in payload,
    "Payload must contain keyed histograms"
  );
}

function fakeAnonId(fn) {
  const m = ChromeUtils.import(
    "resource://gre/modules/EcosystemTelemetry.jsm",
    null
  );
  let oldFn = m.Policy.getEcosystemAnonId;
  m.Policy.getEcosystemAnonId = fn;
  return oldFn;
}

registerCleanupFunction(function() {
  PingServer.stop();
});

add_task(async function setup() {
  // Trigger a proper telemetry init.
  do_get_profile(true);
  // Make sure we don't generate unexpected pings due to pref changes.
  await setEmptyPrefWatchlist();

  // Start the local ping server and setup Telemetry to use it during the tests.
  PingServer.start();
  Preferences.set(
    TelemetryUtils.Preferences.Server,
    "http://localhost:" + PingServer.port
  );
  TelemetrySend.setServer("http://localhost:" + PingServer.port);

  await TelemetryController.testSetup();
});

// We make absolute sure the Ecosystem ping is never triggered on Fennec/Non-unified Telemetry
add_task(
  {
    skip_if: () => !gIsAndroid,
  },
  async function test_no_ecosystem_ping_on_fennec() {
    // Force preference to true, we should have an additional check on Android/Unified Telemetry
    Preferences.set(TelemetryUtils.Preferences.EcosystemTelemetryEnabled, true);
    EcosystemTelemetry.testReset();

    // This is invoked in regular intervals by the timer.
    // Would trigger ping sending.
    EcosystemTelemetry.periodicPing();
    await promiseNoPing();
  }
);

add_task(
  {
    skip_if: () => gIsAndroid,
  },
  async function test_disabled_non_fxa_production() {
    Preferences.set(TelemetryUtils.Preferences.EcosystemTelemetryEnabled, true);
    Assert.ok(EcosystemTelemetry.enabled(), "enabled by default");
    Preferences.set("identity.fxaccounts.autoconfig.uri", "http://");
    Assert.ok(!EcosystemTelemetry.enabled(), "disabled if non-prod");
    Preferences.set(
      TelemetryUtils.Preferences.EcosystemTelemetryAllowForNonProductionFxA,
      true
    );
    Assert.ok(
      EcosystemTelemetry.enabled(),
      "enabled for non-prod but preference override"
    );
    Preferences.reset("identity.fxaccounts.autoconfig.uri");
    Preferences.reset(
      TelemetryUtils.Preferences.EcosystemTelemetryAllowForNonProductionFxA
    );
  }
);

add_task(
  {
    skip_if: () => gIsAndroid,
  },
  async function test_nosending_if_disabled() {
    Preferences.set(
      TelemetryUtils.Preferences.EcosystemTelemetryEnabled,
      false
    );
    EcosystemTelemetry.testReset();

    // This is invoked in regular intervals by the timer.
    // Would trigger ping sending.
    EcosystemTelemetry.periodicPing();
    await promiseNoPing();
  }
);

add_task(
  {
    skip_if: () => gIsAndroid,
  },
  async function test_no_default_send() {
    // No user's logged in, nothing is mocked, so nothing is sent.
    Preferences.set(TelemetryUtils.Preferences.EcosystemTelemetryEnabled, true);
    EcosystemTelemetry.testReset();

    // This is invoked in regular intervals by the timer.
    EcosystemTelemetry.periodicPing();

    await promiseNoPing();
  }
);

add_task(
  {
    skip_if: () => gIsAndroid,
  },
  async function test_login_workflow() {
    // Fake the whole login/logout workflow by triggering the events directly.

    Preferences.set(TelemetryUtils.Preferences.EcosystemTelemetryEnabled, true);
    EcosystemTelemetry.testReset();

    let originalAnonId = fakeAnonId(() => null);
    let ping;

    // 1. No user, timer invoked
    EcosystemTelemetry.periodicPing();
    await promiseNoPing();

    // 2. User logs in, but we fail to obtain a valid uid.
    //    No ping will be generated.
    fakeAnonId(() => null);
    EcosystemTelemetry.observe(null, ONLOGIN_NOTIFICATION, null);

    EcosystemTelemetry.periodicPing();
    await promiseNoPing();

    // Once we've failed to get the ID, we don't try again until next startup
    // or another login-related event - so...
    // 3. uid becomes available after verification.
    fakeAnonId(() => "test_login_workflow:my.anon.id");
    EcosystemTelemetry.observe(null, ONVERIFIED_NOTIFICATION, null);
    print("triggering ping now that we have an anon-id");
    EcosystemTelemetry.periodicPing();
    ping = await PingServer.promiseNextPing();
    checkPingStructure(ping, "periodic");
    Assert.equal(
      ping.payload.ecosystemAnonId,
      "test_login_workflow:my.anon.id"
    );

    // 4. User disconnects account, should get an immediate ping.
    print("user disconnects");
    // We need to arrange for the new empty anonid before the notification.
    fakeAnonId(() => null);
    await EcosystemTelemetry.observe(null, ONLOGOUT_NOTIFICATION, null);
    ping = await PingServer.promiseNextPing();
    checkPingStructure(ping, "logout");
    Assert.equal(
      ping.payload.ecosystemAnonId,
      "test_login_workflow:my.anon.id",
      "should have been submitted with the old anonid"
    );
    Assert.equal(
      await EcosystemTelemetry.promiseEcosystemAnonId,
      null,
      "should resolve to null immediately after logout"
    );

    // 5. No user, timer invoked
    print("timer fires after disconnection");
    EcosystemTelemetry.periodicPing();
    await promiseNoPing();

    // 6. Transition back to logged in, pings should again be sent.
    fakeAnonId(() => "test_login_workflow:my.anon.id.2");
    EcosystemTelemetry.observe(null, ONVERIFIED_NOTIFICATION, null);
    print("triggering ping now the user has logged back in");
    EcosystemTelemetry.periodicPing();
    ping = await PingServer.promiseNextPing();
    checkPingStructure(ping, "periodic");
    Assert.equal(
      ping.payload.ecosystemAnonId,
      "test_login_workflow:my.anon.id.2"
    );

    // Reset policy.
    fakeAnonId(originalAnonId);
  }
);

add_task(
  {
    skip_if: () => gIsAndroid,
  },
  async function test_shutdown_logged_in() {
    // Check shutdown when a user's logged in does the right thing.
    Preferences.set(TelemetryUtils.Preferences.EcosystemTelemetryEnabled, true);
    EcosystemTelemetry.testReset();

    let originalAnonId = fakeAnonId(() =>
      Promise.resolve("test_shutdown_logged_in:my.anon.id")
    );

    EcosystemTelemetry.observe(null, ONLOGIN_NOTIFICATION, null);

    // No ping expected yet.
    await promiseNoPing();

    // Shutdown
    EcosystemTelemetry.shutdown();
    let ping = await PingServer.promiseNextPing();
    checkPingStructure(ping, "shutdown");
    Assert.equal(
      ping.payload.ecosystemAnonId,
      "test_shutdown_logged_in:my.anon.id",
      "our anon ID is in the ping"
    );
    fakeAnonId(originalAnonId);
  }
);

add_task(
  {
    skip_if: () => gIsAndroid,
  },
  async function test_shutdown_not_logged_in() {
    // Check shutdown when no user is logged in does the right thing.
    Preferences.set(TelemetryUtils.Preferences.EcosystemTelemetryEnabled, true);
    EcosystemTelemetry.testReset();

    let originalAnonId = fakeAnonId(() => Promise.resolve(null));

    // No ping expected yet.
    await promiseNoPing();

    // Shutdown
    EcosystemTelemetry.shutdown();

    // Still no ping.
    await promiseNoPing();
    fakeAnonId(originalAnonId);
  }
);

// Test that a periodic ping is triggered by the scheduler at midnight
//
// Based on `test_TelemetrySession#test_DailyDueAndIdle`.
add_task(
  {
    skip_if: () => gIsAndroid,
  },
  async function test_periodic_ping() {
    await TelemetryStorage.testClearPendingPings();
    PingServer.clearRequests();

    let receivedPing = null;
    // Register a ping handler that will assert when receiving multiple ecosystem pings.
    // We can ignore other pings, such as the periodic ping.
    PingServer.registerPingHandler(req => {
      const ping = decodeRequestPayload(req);
      if (ping.type == EcosystemTelemetry.PING_TYPE) {
        Assert.ok(
          !receivedPing,
          "Telemetry must only send one periodic ecosystem ping."
        );
        receivedPing = ping;
      }
    });

    // Faking scheduler timer has to happen before resetting TelemetryController
    // to be effective.
    let schedulerTickCallback = null;
    let now = new Date(2040, 1, 1, 0, 0, 0);
    fakeNow(now);
    // Fake scheduler functions to control periodic collection flow in tests.
    fakeSchedulerTimer(
      callback => (schedulerTickCallback = callback),
      () => {}
    );
    await TelemetryController.testReset();

    Preferences.set(TelemetryUtils.Preferences.EcosystemTelemetryEnabled, true);
    EcosystemTelemetry.testReset();

    // Have to arrange for an anon-id to be configured.
    let originalAnonId = fakeAnonId(() => "test_periodic_ping:my.anon.id");
    EcosystemTelemetry.observe(null, ONLOGIN_NOTIFICATION, null);

    // As a sanity check we trigger a keyedHistogram and scalar declared as
    // being in our ping, just to help ensure that the payload was assembled
    // in the correct shape.
    let h = Services.telemetry.getKeyedHistogramById("SEARCH_COUNTS");
    h.add("test-key");
    Telemetry.scalarSet("browser.engagement.total_uri_count", 2);

    // Trigger the periodic ecosystem ping.
    let firstPeriodicDue = new Date(2040, 1, 2, 0, 0, 0);
    fakeNow(firstPeriodicDue);

    // Run a scheduler tick: it should trigger the periodic ping.
    Assert.ok(!!schedulerTickCallback);
    let tickPromise = schedulerTickCallback();

    // Send an idle and then an active user notification.
    fakeIdleNotification("idle");
    fakeIdleNotification("active");

    // Wait on the tick promise.
    await tickPromise;

    await TelemetrySend.testWaitOnOutgoingPings();

    // Decode the ping contained in the request and check that's a periodic ping.
    Assert.ok(receivedPing, "Telemetry must send one ecosystem periodic ping.");
    checkPingStructure(receivedPing, "periodic");
    // And check the content we expect is there.
    Assert.ok(receivedPing.payload.keyedHistograms.parent.SEARCH_COUNTS);
    Assert.equal(
      receivedPing.payload.scalars.parent["browser.engagement.total_uri_count"],
      2
    );

    fakeAnonId(originalAnonId);
  }
);
