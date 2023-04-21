/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

// Enable the collection (during test) for all products so even products
// that don't collect the data will be able to run the test without failure.
Services.prefs.setBoolPref(
  "toolkit.telemetry.testing.overrideProductsCheck",
  true
);

add_setup(async function test_setup() {
  // Addon manager needs a profile directory
  do_get_profile();
});

add_task(async function test_register_twice_fails() {
  TelemetryController.registerSyncPingShutdown(() => {});
  Assert.throws(
    () => TelemetryController.registerSyncPingShutdown(() => {}),
    /The sync ping shutdown handler is already registered./
  );
  await TelemetryController.testReset();
});

add_task(async function test_reset_clears_handler() {
  await TelemetryController.testSetup();
  TelemetryController.registerSyncPingShutdown(() => {});
  await TelemetryController.testReset();
  // If this works the reset must have cleared it.
  TelemetryController.registerSyncPingShutdown(() => {});
  await TelemetryController.testReset();
});

add_task(async function test_shutdown_handler_submits() {
  let handlerCalled = false;
  await TelemetryController.testSetup();
  TelemetryController.registerSyncPingShutdown(() => {
    handlerCalled = true;
    // and submit a ping.
    let ping = {
      why: "shutdown",
    };
    TelemetryController.submitExternalPing("sync", ping);
  });

  await TelemetryController.testShutdown();
  Assert.ok(handlerCalled);
  await TelemetryController.testReset();
});

add_task(async function test_shutdown_handler_no_submit() {
  let handlerCalled = false;
  await TelemetryController.testSetup();
  TelemetryController.registerSyncPingShutdown(() => {
    handlerCalled = true;
    // but don't submit a ping.
  });

  await TelemetryController.testShutdown();
  Assert.ok(handlerCalled);
});

// NB: The last test in this file *must not* restart TelemetryController, or it
// will cause intermittent timeouts for this test.
