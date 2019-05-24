const { ClientID } = ChromeUtils.import("resource://gre/modules/ClientID.jsm");
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { TelemetryTestUtils } = ChromeUtils.import("resource://testing-common/TelemetryTestUtils.jsm");
const { UptakeTelemetry } = ChromeUtils.import("resource://services-common/uptake-telemetry.js");

const COMPONENT = "remotesettings";

async function withFakeClientID(uuid, f) {
  const module = ChromeUtils.import("resource://services-common/uptake-telemetry.js", null);
  const oldPolicy = module.Policy;
  module.Policy = {
    ...oldPolicy,
    _clientIDHash: null,
    getClientID: () => Promise.resolve(uuid),
  };
  try {
    return await f();
  } finally {
    module.Policy = oldPolicy;
  }
}

add_task(async function test_unknown_status_is_not_reported() {
  const source = "update-source";
  const startHistogram = getUptakeTelemetrySnapshot(source);

  await UptakeTelemetry.report(COMPONENT, "unknown-status", { source });

  const endHistogram = getUptakeTelemetrySnapshot(source);
  const expectedIncrements = {};
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});

add_task(async function test_age_is_converted_to_string_and_reported() {
  const status = UptakeTelemetry.STATUS.SUCCESS;
  const age = 42;

  await withFakeChannel("nightly", async () => { // no sampling.
    await UptakeTelemetry.report(COMPONENT, status, { source: "s", age });
  });

  TelemetryTestUtils.assertEvents(
    [["uptake.remotecontent.result", "uptake", COMPONENT, status, { source: "s", age: `${age}` }]]);
});

add_task(async function test_each_status_can_be_caught_in_snapshot() {
  const source = "some-source";
  const startHistogram = getUptakeTelemetrySnapshot(source);

  const expectedIncrements = {};
  for (const label of Object.keys(UptakeTelemetry.STATUS)) {
    const status = UptakeTelemetry.STATUS[label];
    await UptakeTelemetry.report(COMPONENT, status, { source });
    expectedIncrements[status] = 1;
  }

  const endHistogram = getUptakeTelemetrySnapshot(source);
  checkUptakeTelemetry(startHistogram, endHistogram, expectedIncrements);
});

add_task(async function test_events_are_sent_when_hash_is_mod_0() {
  const source = "some-source";
  const startSnapshot = getUptakeTelemetrySnapshot(source);
  const startSuccess = startSnapshot.events.success || 0;
  const uuid = "d81bbfad-d741-41f5-a7e6-29f6bde4972a"; // hash % 100 = 0
  await withFakeClientID(uuid, async () => {
    await withFakeChannel("release", async () => {
      await UptakeTelemetry.report(COMPONENT, UptakeTelemetry.STATUS.SUCCESS, { source });
    });
  });
  const endSnapshot = getUptakeTelemetrySnapshot(source);
  Assert.equal(endSnapshot.events.success, startSuccess + 1);
});

add_task(async function test_events_are_not_sent_when_hash_is_greater_than_pref() {
  const source = "some-source";
  const startSnapshot = getUptakeTelemetrySnapshot(source);
  const startSuccess = startSnapshot.events.success || 0;
  const uuid = "d81bbfad-d741-41f5-a7e6-29f6bde49721"; // hash % 100 = 1
  await withFakeClientID(uuid, async () => {
    await withFakeChannel("release", async () => {
      await UptakeTelemetry.report(COMPONENT, UptakeTelemetry.STATUS.SUCCESS, { source });
    });
  });
  const endSnapshot = getUptakeTelemetrySnapshot(source);
  Assert.equal(endSnapshot.events.success || 0, startSuccess);
});

add_task(async function test_events_are_sent_when_nightly() {
  const source = "some-source";
  const startSnapshot = getUptakeTelemetrySnapshot(source);
  const startSuccess = startSnapshot.events.success || 0;
  const uuid = "d81bbfad-d741-41f5-a7e6-29f6bde49721"; // hash % 100 = 1
  await withFakeClientID(uuid, async () => {
    await withFakeChannel("nightly", async () => {
      await UptakeTelemetry.report(COMPONENT, UptakeTelemetry.STATUS.SUCCESS, { source });
    });
  });
  const endSnapshot = getUptakeTelemetrySnapshot(source);
  Assert.equal(endSnapshot.events.success, startSuccess + 1);
});
