/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryController } = ChromeUtils.import(
  "resource://gre/modules/TelemetryController.jsm"
);
const { ContentTaskUtils } = ChromeUtils.import(
  "resource://testing-common/ContentTaskUtils.jsm"
);

const SOCKET_ONLY_UINT_SCALAR = "telemetry.test.socket_only_uint";

/**
 * This function waits until socket scalars are reported into the
 * scalar snapshot.
 */
async function waitForSocketScalars() {
  await ContentTaskUtils.waitForCondition(() => {
    const scalars = Telemetry.getSnapshotForScalars("main", false);
    return Object.keys(scalars).includes("socket");
  });
}

add_task(async function() {
  if (!Services.prefs.getBoolPref("network.process.enabled")) {
    Assert.ok(
      true,
      "Test finished: no point to test telemetry from socket process with lanuching the process"
    );
    return;
  }

  do_test_pending();

  do_get_profile(true);
  await TelemetryController.testSetup();

  Services.io.socketProcessTelemetryPing();

  // Once scalars are set by the socket process, they don't immediately get
  // sent to the parent process. Wait for the Telemetry IPC Timer to trigger
  // and batch send the data back to the parent process.
  await waitForSocketScalars();

  Assert.equal(
    Telemetry.getSnapshotForScalars("main", false).socket[
      SOCKET_ONLY_UINT_SCALAR
    ],
    42,
    `${SOCKET_ONLY_UINT_SCALAR} must have the correct value (socket process).`
  );
  do_test_finished();
});
