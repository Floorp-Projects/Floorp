/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const { TelemetryController } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryController.sys.mjs"
);
const { ContentTaskUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ContentTaskUtils.sys.mjs"
);

const UTILITY_ONLY_UINT_SCALAR = "telemetry.test.utility_only_uint";

const utilityProcessTest = () => {
  return Cc["@mozilla.org/utility-process-test;1"].createInstance(
    Ci.nsIUtilityProcessTest
  );
};

/**
 * This function waits until utility scalars are reported into the
 * scalar snapshot.
 */
async function waitForUtilityScalars() {
  await ContentTaskUtils.waitForCondition(() => {
    const scalars = Telemetry.getSnapshotForScalars("main", false);
    return Object.keys(scalars).includes("utility");
  }, "Waiting for utility scalars to have been set");
}

async function waitForUtilityValue() {
  await ContentTaskUtils.waitForCondition(() => {
    return (
      UTILITY_ONLY_UINT_SCALAR in
      Telemetry.getSnapshotForScalars("main", false).utility
    );
  }, "Waiting for utility uint value");
}

add_setup(async function setup_telemetry_utility() {
  info("Start a UtilityProcess");
  await utilityProcessTest().startProcess();

  do_get_profile(true);
  await TelemetryController.testSetup();
});

add_task(async function test_scalars_in_utility_process() {
  Telemetry.clearScalars();
  await utilityProcessTest().testTelemetryProbes();

  // Once scalars are set by the utility process, they don't immediately get
  // sent to the parent process. Wait for the Telemetry IPC Timer to trigger
  // and batch send the data back to the parent process.
  await waitForUtilityScalars();
  await waitForUtilityValue();

  Assert.equal(
    Telemetry.getSnapshotForScalars("main", false).utility[
      UTILITY_ONLY_UINT_SCALAR
    ],
    42,
    `${UTILITY_ONLY_UINT_SCALAR} must have the correct value (utility process).`
  );
});
