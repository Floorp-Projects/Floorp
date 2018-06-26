ChromeUtils.import("resource://gre/modules/Services.jsm");
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

XPCOMUtils.defineLazyGlobalGetters(this, ["DOMParser"]);

const gParser = new DOMParser();

const EVENT_NAME = "blocklist-data-gfxItems";

const SAMPLE_GFX_RECORD = {
  "driverVersionComparator": "LESS_THAN_OR_EQUAL",
  "driverVersion": "8.17.12.5896",
  "vendor": "0x10de",
  "blockID": "g36",
  "feature": "DIRECT3D_9_LAYERS",
  "devices": ["0x0a6c", "geforce"],
  "featureStatus": "BLOCKED_DRIVER_VERSION",
  "last_modified": 1458035931837,
  "os": "WINNT 6.1",
  "id": "3f947f16-37c2-4e96-d356-78b26363729b",
  "versionRange": {"minVersion": 0, "maxVersion": "*"}
};


function getBlocklist() {
  Blocklist._clear();
  return Blocklist;
}

async function updateBlocklistWithInput(input) {
  let blocklist = getBlocklist();
  let promiseObserved = TestUtils.topicObserved(EVENT_NAME);
  blocklist._loadBlocklistFromXML(gParser.parseFromString(input, "text/xml"));
  let [, received] = await promiseObserved;
  return [blocklist, received];
}


add_task(async function test_sends_serialized_data() {
  const blocklist = getBlocklist();
  blocklist._gfxEntries = [SAMPLE_GFX_RECORD];

  const expected = "blockID:g36\tdevices:0x0a6c,geforce\tdriverVersion:8.17.12.5896\t" +
                   "driverVersionComparator:LESS_THAN_OR_EQUAL\tfeature:DIRECT3D_9_LAYERS\t" +
                   "featureStatus:BLOCKED_DRIVER_VERSION\tos:WINNT 6.1\tvendor:0x10de\t" +
                   "versionRange:0,*";
  let promiseObserved = TestUtils.topicObserved(EVENT_NAME);
  blocklist._notifyObserversBlocklistGFX();
  let [, received] = await promiseObserved;
  equal(received, expected);
});


add_task(async function test_parsing_fails_if_devices_contains_comma() {
  const input = "<blocklist xmlns=\"http://www.mozilla.org/2006/addons-blocklist\">" +
  "<gfxItems>" +
  " <gfxBlacklistEntry>" +
  "   <devices>" +
  "     <device>0x2,582</device>" +
  "     <device>0x2782</device>" +
  "   </devices>" +
  " </gfxBlacklistEntry>" +
  "</gfxItems>" +
  "</blocklist>";
  let [blocklist] = await updateBlocklistWithInput(input);

  equal(blocklist._gfxEntries[0].devices.length, 1);
  equal(blocklist._gfxEntries[0].devices[0], "0x2782");
});


add_task(async function test_empty_values_are_ignored() {
  const input = "<blocklist xmlns=\"http://www.mozilla.org/2006/addons-blocklist\">" +
  "<gfxItems>" +
  " <gfxBlacklistEntry>" +
  "   <os></os>" +
  " </gfxBlacklistEntry>" +
  "</gfxItems>" +
  "</blocklist>";
  let [, received] = await updateBlocklistWithInput(input);
  ok(received.indexOf("os" < 0));
});

add_task(async function test_empty_devices_are_ignored() {
  const input = "<blocklist xmlns=\"http://www.mozilla.org/2006/addons-blocklist\">" +
  "<gfxItems>" +
  " <gfxBlacklistEntry>" +
  "   <devices></devices>" +
  " </gfxBlacklistEntry>" +
  "</gfxItems>" +
  "</blocklist>";
  let [, received] = await updateBlocklistWithInput(input);
  ok(received.indexOf("devices" < 0));
});

add_task(async function test_version_range_default_values() {
  const input = "<blocklist xmlns=\"http://www.mozilla.org/2006/addons-blocklist\">" +
  "<gfxItems>" +
  " <gfxBlacklistEntry>" +
  "   <versionRange minVersion=\"13.0b2\" maxVersion=\"42.0\"/>" +
  " </gfxBlacklistEntry>" +
  " <gfxBlacklistEntry>" +
  "   <versionRange maxVersion=\"2.0\"/>" +
  " </gfxBlacklistEntry>" +
  " <gfxBlacklistEntry>" +
  "   <versionRange minVersion=\"1.0\"/>" +
  " </gfxBlacklistEntry>" +
  " <gfxBlacklistEntry>" +
  "   <versionRange minVersion=\"  \"/>" +
  " </gfxBlacklistEntry>" +
  " <gfxBlacklistEntry>" +
  "   <versionRange/>" +
  " </gfxBlacklistEntry>" +
  "</gfxItems>" +
  "</blocklist>";
  let [blocklist] = await updateBlocklistWithInput(input);
  equal(blocklist._gfxEntries[0].versionRange.minVersion, "13.0b2");
  equal(blocklist._gfxEntries[0].versionRange.maxVersion, "42.0");
  equal(blocklist._gfxEntries[1].versionRange.minVersion, "0");
  equal(blocklist._gfxEntries[1].versionRange.maxVersion, "2.0");
  equal(blocklist._gfxEntries[2].versionRange.minVersion, "1.0");
  equal(blocklist._gfxEntries[2].versionRange.maxVersion, "*");
  equal(blocklist._gfxEntries[3].versionRange.minVersion, "0");
  equal(blocklist._gfxEntries[3].versionRange.maxVersion, "*");
  equal(blocklist._gfxEntries[4].versionRange.minVersion, "0");
  equal(blocklist._gfxEntries[4].versionRange.maxVersion, "*");
});

add_task(async function test_blockid_attribute() {
  const input = "<blocklist xmlns=\"http://www.mozilla.org/2006/addons-blocklist\">" +
  "<gfxItems>" +
  " <gfxBlacklistEntry blockID=\"g60\">" +
  "   <vendor> 0x10de </vendor>" +
  " </gfxBlacklistEntry>" +
  " <gfxBlacklistEntry>" +
  "   <feature> DIRECT3D_9_LAYERS </feature>" +
  " </gfxBlacklistEntry>" +
  "</gfxItems>" +
  "</blocklist>";
  let [blocklist] = await updateBlocklistWithInput(input);
  equal(blocklist._gfxEntries[0].blockID, "g60");
  ok(!blocklist._gfxEntries[1].hasOwnProperty("blockID"));
});
