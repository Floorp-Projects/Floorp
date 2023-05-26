/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/
*/

const { TelemetryController } = ChromeUtils.importESModule(
  "resource://gre/modules/TelemetryController.sys.mjs"
);
const { ContentTaskUtils } = ChromeUtils.importESModule(
  "resource://testing-common/ContentTaskUtils.sys.mjs"
);

const RDD_ONLY_UINT_SCALAR = "telemetry.test.rdd_only_uint";

const rddProcessTest = () => {
  return Cc["@mozilla.org/rdd-process-test;1"].createInstance(
    Ci.nsIRddProcessTest
  );
};

/**
 * This function waits until rdd scalars are reported into the
 * scalar snapshot.
 */
async function waitForRddScalars() {
  await ContentTaskUtils.waitForCondition(() => {
    const scalars = Telemetry.getSnapshotForScalars("main", false);
    return Object.keys(scalars).includes("rdd");
  }, "Waiting for rdd scalars to have been set");
}

add_setup(async function setup_telemetry_rdd() {
  do_get_profile(true);
  await TelemetryController.testSetup();
});

add_task(async function test_scalars_in_rdd_process() {
  Telemetry.clearScalars();
  const pid = await rddProcessTest().testTelemetryProbes();
  info(`Started some RDD: ${pid}`);

  // Once scalars are set by the rdd process, they don't immediately get
  // sent to the parent process. Wait for the Telemetry IPC Timer to trigger
  // and batch send the data back to the parent process.
  await waitForRddScalars();

  Assert.equal(
    Telemetry.getSnapshotForScalars("main", false).rdd[RDD_ONLY_UINT_SCALAR],
    42,
    `${RDD_ONLY_UINT_SCALAR} must have the correct value (rdd process).`
  );

  await rddProcessTest().stopProcess();
});
