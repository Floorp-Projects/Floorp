const { TelemetryTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/TelemetryTestUtils.sys.mjs"
);
const { UptakeTelemetry } = ChromeUtils.importESModule(
  "resource://services-common/uptake-telemetry.sys.mjs"
);

const COMPONENT = "remotesettings";

async function withFakeClientID(uuid, f) {
  const { Policy } = ChromeUtils.importESModule(
    "resource://services-common/uptake-telemetry.sys.mjs"
  );
  let oldGetClientID = Policy.getClientID;
  Policy._clientIDHash = null;
  Policy.getClientID = () => Promise.resolve(uuid);
  try {
    return await f();
  } finally {
    Policy.getClientID = oldGetClientID;
  }
}

add_task(async function test_unknown_status_is_not_reported() {
  const source = "update-source";
  const startSnapshot = getUptakeTelemetrySnapshot(COMPONENT, source);

  try {
    await UptakeTelemetry.report(COMPONENT, "unknown-status", { source });
  } catch (e) {}

  const endSnapshot = getUptakeTelemetrySnapshot(COMPONENT, source);
  Assert.deepEqual(startSnapshot, endSnapshot);
});

add_task(async function test_age_is_converted_to_string_and_reported() {
  const status = UptakeTelemetry.STATUS.SUCCESS;
  const age = 42;

  await withFakeChannel("nightly", async () => {
    await UptakeTelemetry.report(COMPONENT, status, { source: "s", age });
  });

  TelemetryTestUtils.assertEvents([
    [
      "uptake.remotecontent.result",
      "uptake",
      COMPONENT,
      status,
      { source: "s", age: `${age}` },
    ],
  ]);
});

add_task(async function test_each_status_can_be_caught_in_snapshot() {
  const source = "some-source";
  const startSnapshot = getUptakeTelemetrySnapshot(COMPONENT, source);

  const expectedIncrements = {};
  await withFakeChannel("nightly", async () => {
    for (const status of Object.values(UptakeTelemetry.STATUS)) {
      expectedIncrements[status] = 1;
      await UptakeTelemetry.report(COMPONENT, status, { source });
    }
  });

  const endSnapshot = getUptakeTelemetrySnapshot(COMPONENT, source);
  checkUptakeTelemetry(startSnapshot, endSnapshot, expectedIncrements);
});

add_task(async function test_events_are_sent_when_hash_is_mod_0() {
  const source = "some-source";
  const startSnapshot = getUptakeTelemetrySnapshot(COMPONENT, source);
  const startSuccess = startSnapshot.success || 0;
  const uuid = "d81bbfad-d741-41f5-a7e6-29f6bde4972a"; // hash % 100 = 0
  await withFakeClientID(uuid, async () => {
    await withFakeChannel("release", async () => {
      await UptakeTelemetry.report(COMPONENT, UptakeTelemetry.STATUS.SUCCESS, {
        source,
      });
    });
  });
  const endSnapshot = getUptakeTelemetrySnapshot(COMPONENT, source);
  Assert.equal(endSnapshot.success, startSuccess + 1);
});

add_task(
  async function test_events_are_not_sent_when_hash_is_greater_than_pref() {
    const source = "some-source";
    const startSnapshot = getUptakeTelemetrySnapshot(COMPONENT, source);
    const startSuccess = startSnapshot.success || 0;
    const uuid = "d81bbfad-d741-41f5-a7e6-29f6bde49721"; // hash % 100 = 1
    await withFakeClientID(uuid, async () => {
      await withFakeChannel("release", async () => {
        await UptakeTelemetry.report(
          COMPONENT,
          UptakeTelemetry.STATUS.SUCCESS,
          { source }
        );
      });
    });
    const endSnapshot = getUptakeTelemetrySnapshot(COMPONENT, source);
    Assert.equal(endSnapshot.success || 0, startSuccess);
  }
);

add_task(async function test_events_are_sent_when_nightly() {
  const source = "some-source";
  const startSnapshot = getUptakeTelemetrySnapshot(COMPONENT, source);
  const startSuccess = startSnapshot.success || 0;
  const uuid = "d81bbfad-d741-41f5-a7e6-29f6bde49721"; // hash % 100 = 1
  await withFakeClientID(uuid, async () => {
    await withFakeChannel("nightly", async () => {
      await UptakeTelemetry.report(COMPONENT, UptakeTelemetry.STATUS.SUCCESS, {
        source,
      });
    });
  });
  const endSnapshot = getUptakeTelemetrySnapshot(COMPONENT, source);
  Assert.equal(endSnapshot.success, startSuccess + 1);
});
