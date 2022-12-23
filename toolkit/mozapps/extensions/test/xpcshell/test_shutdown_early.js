/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { TelemetryEnvironment } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryEnvironment.sys.mjs"
);

// Regression test for bug 1665568: verifies that AddonManager unblocks shutdown
// when startup is interrupted very early.
add_task(async function test_shutdown_immediately_after_startup() {
  // Set as migrated to prevent sync DB load at startup.
  Services.prefs.setCharPref("extensions.lastAppVersion", "42");
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "42");

  Cc["@mozilla.org/addons/integration;1"]
    .getService(Ci.nsIObserver)
    .observe(null, "addons-startup", null);

  // Above, we have configured the runtime to avoid a forced synchronous load
  // of the database. Confirm that this is indeed the case.
  equal(AddonManagerPrivate.isDBLoaded(), false, "DB not loaded synchronously");

  let shutdownCount = 0;
  AddonManager.beforeShutdown.addBlocker("count", async () => ++shutdownCount);

  let databaseLoaded = false;
  AddonManagerPrivate.databaseReady.then(() => {
    databaseLoaded = true;
  });

  // Accessing TelemetryEnvironment.currentEnvironment triggers initialization
  // of TelemetryEnvironment / EnvironmentAddonBuilder, which registers a
  // shutdown blocker.
  equal(
    TelemetryEnvironment.currentEnvironment.addons,
    undefined,
    "TelemetryEnvironment.currentEnvironment.addons is uninitialized"
  );

  info("Immediate exit at startup, without quit-application-granted");
  Services.startup.advanceShutdownPhase(
    Services.startup.SHUTDOWN_PHASE_APPSHUTDOWN
  );
  let shutdownPromise = MockAsyncShutdown.profileBeforeChange.trigger();
  equal(shutdownCount, 1, "AddonManager.beforeShutdown has started");

  // Note: Until now everything ran in the same tick of the event loop.

  // Waiting for AddonManager to have shut down.
  await shutdownPromise;

  ok(databaseLoaded, "Addon DB loaded for use by TelemetryEnvironment");
  equal(AddonManagerPrivate.isDBLoaded(), false, "DB unloaded after shutdown");

  Assert.deepEqual(
    TelemetryEnvironment.currentEnvironment.addons.activeAddons,
    {},
    "TelemetryEnvironment.currentEnvironment.addons is initialized"
  );
});
