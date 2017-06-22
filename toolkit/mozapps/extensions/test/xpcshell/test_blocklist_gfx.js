const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");


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


function Blocklist() {
  let blocklist = Cc["@mozilla.org/extensions/blocklist;1"].
                  getService().wrappedJSObject;
  blocklist._clear();
  return blocklist;
}


function run_test() {
  run_next_test();
}


add_task(async function test_sends_serialized_data() {
  const blocklist = Blocklist();
  blocklist._gfxEntries = [SAMPLE_GFX_RECORD];

  const expected = "blockID:g36\tdevices:0x0a6c,geforce\tdriverVersion:8.17.12.5896\t" +
                   "driverVersionComparator:LESS_THAN_OR_EQUAL\tfeature:DIRECT3D_9_LAYERS\t" +
                   "featureStatus:BLOCKED_DRIVER_VERSION\tos:WINNT 6.1\tvendor:0x10de\t" +
                   "versionRange:0,*";
  let received;
  const observe = (subject, topic, data) => { received = data };
  Services.obs.addObserver(observe, EVENT_NAME);
  blocklist._notifyObserversBlocklistGFX();
  equal(received, expected);
  Services.obs.removeObserver(observe, EVENT_NAME);
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
  const blocklist = Blocklist();
  blocklist._loadBlocklistFromString(input);
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
  const blocklist = Blocklist();
  let received;
  const observe = (subject, topic, data) => { received = data };
  Services.obs.addObserver(observe, EVENT_NAME);
  blocklist._loadBlocklistFromString(input);
  ok(received.indexOf("os" < 0));
  Services.obs.removeObserver(observe, EVENT_NAME);
});

add_task(async function test_empty_devices_are_ignored() {
  const input = "<blocklist xmlns=\"http://www.mozilla.org/2006/addons-blocklist\">" +
  "<gfxItems>" +
  " <gfxBlacklistEntry>" +
  "   <devices></devices>" +
  " </gfxBlacklistEntry>" +
  "</gfxItems>" +
  "</blocklist>";
  const blocklist = Blocklist();
  let received;
  const observe = (subject, topic, data) => { received = data };
  Services.obs.addObserver(observe, EVENT_NAME);
  blocklist._loadBlocklistFromString(input);
  ok(received.indexOf("devices" < 0));
  Services.obs.removeObserver(observe, EVENT_NAME);
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
  const blocklist = Blocklist();
  blocklist._loadBlocklistFromString(input);
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
  const blocklist = Blocklist();
  blocklist._loadBlocklistFromString(input);
  equal(blocklist._gfxEntries[0].blockID, "g60");
  ok(!blocklist._gfxEntries[1].hasOwnProperty("blockID"));
});
