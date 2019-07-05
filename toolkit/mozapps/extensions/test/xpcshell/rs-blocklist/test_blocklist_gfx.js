const EVENT_NAME = "blocklist-data-gfxItems";

const SAMPLE_GFX_RECORD = {
  driverVersionComparator: "LESS_THAN_OR_EQUAL",
  driverVersion: "8.17.12.5896",
  vendor: "0x10de",
  blockID: "g36",
  feature: "DIRECT3D_9_LAYERS",
  devices: ["0x0a6c", "geforce"],
  featureStatus: "BLOCKED_DRIVER_VERSION",
  last_modified: 1458035931837,
  os: "WINNT 6.1",
  id: "3f947f16-37c2-4e96-d356-78b26363729b",
  versionRange: { minVersion: 0, maxVersion: "*" },
};

add_task(async function test_sends_serialized_data() {
  const expected =
    "blockID:g36\tdevices:0x0a6c,geforce\tdriverVersion:8.17.12.5896\t" +
    "driverVersionComparator:LESS_THAN_OR_EQUAL\tfeature:DIRECT3D_9_LAYERS\t" +
    "featureStatus:BLOCKED_DRIVER_VERSION\tos:WINNT 6.1\tvendor:0x10de\t" +
    "versionRange:0,*";
  let received;
  const observe = (subject, topic, data) => {
    received = data;
  };
  Services.obs.addObserver(observe, EVENT_NAME);
  await mockGfxBlocklistItems([SAMPLE_GFX_RECORD]);
  Services.obs.removeObserver(observe, EVENT_NAME);

  equal(received, expected);
});

add_task(async function test_parsing_skips_devices_with_comma() {
  let clonedItem = Cu.cloneInto(SAMPLE_GFX_RECORD, this);
  clonedItem.devices[0] = "0x2,582";
  let rv = await mockGfxBlocklistItems([clonedItem]);
  equal(rv[0].devices.length, 1);
  equal(rv[0].devices[0], "geforce");
});

add_task(async function test_empty_values_are_ignored() {
  let received;
  const observe = (subject, topic, data) => {
    received = data;
  };
  Services.obs.addObserver(observe, EVENT_NAME);
  let clonedItem = Cu.cloneInto(SAMPLE_GFX_RECORD, this);
  clonedItem.os = "";
  await mockGfxBlocklistItems([clonedItem]);
  ok(!received.includes("os"), "Shouldn't send empty values");
  Services.obs.removeObserver(observe, EVENT_NAME);
});

add_task(async function test_empty_devices_are_ignored() {
  let received;
  const observe = (subject, topic, data) => {
    received = data;
  };
  Services.obs.addObserver(observe, EVENT_NAME);
  let clonedItem = Cu.cloneInto(SAMPLE_GFX_RECORD, this);
  clonedItem.devices = [];
  await mockGfxBlocklistItems([clonedItem]);
  ok(!received.includes("devices"), "Shouldn't send empty values");
  Services.obs.removeObserver(observe, EVENT_NAME);
});

add_task(async function test_version_range_default_values() {
  const kTests = [
    {
      input: { minVersion: "13.0b2", maxVersion: "42.0" },
      output: { minVersion: "13.0b2", maxVersion: "42.0" },
    },
    {
      input: { maxVersion: "2.0" },
      output: { minVersion: "0", maxVersion: "2.0" },
    },
    {
      input: { minVersion: "1.0" },
      output: { minVersion: "1.0", maxVersion: "*" },
    },
    {
      input: { minVersion: "  " },
      output: { minVersion: "0", maxVersion: "*" },
    },
    {
      input: {},
      output: { minVersion: "0", maxVersion: "*" },
    },
  ];
  for (let test of kTests) {
    let parsedEntries = await mockGfxBlocklistItems([
      { versionRange: test.input },
    ]);
    equal(parsedEntries[0].versionRange.minVersion, test.output.minVersion);
    equal(parsedEntries[0].versionRange.maxVersion, test.output.maxVersion);
  }
});

add_task(async function test_blockid_attribute() {
  const kTests = [
    { blockID: "g60", vendor: " 0x10de " },
    { feature: " DIRECT3D_9_LAYERS " },
  ];
  for (let test of kTests) {
    let [rv] = await mockGfxBlocklistItems([test]);
    if (test.blockID) {
      equal(rv.blockID, test.blockID);
    } else {
      ok(!rv.hasOwnProperty("blockID"), "not expecting a blockID");
    }
  }
});
