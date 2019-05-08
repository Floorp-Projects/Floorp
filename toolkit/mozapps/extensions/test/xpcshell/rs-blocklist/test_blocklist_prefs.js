/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// Tests resetting of preferences in blocklist entry when an add-on is blocked.
// See bug 802434.

const BLOCKLIST_DATA = [
  {
    guid: "block1@tests.mozilla.org",
    versionRange: [
      {
        severity: "1",
        targetApplication: [
          {
            guid: "xpcshell@tests.mozilla.org",
            maxVersion: "2.*",
            minVersion: "1",
          },
        ],
      },
    ],
    prefs: [
      "test.blocklist.pref1",
      "test.blocklist.pref2",
    ],
  },
  {
    guid: "block2@tests.mozilla.org",
    versionRange: [
      {
        severity: "3",
        targetApplication: [
          {
            guid: "xpcshell@tests.mozilla.org",
            maxVersion: "2.*",
            minVersion: "1",
          },
        ],
      },
    ],
    prefs: [
      "test.blocklist.pref3",
      "test.blocklist.pref4",
    ],
  },
];

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1");

  await promiseStartupManager();

  // Add 2 extensions
  await promiseInstallWebExtension({
    manifest: {
      name: "Blocked add-on-1 with to-be-reset prefs",
      version: "1.0",
      applications: {gecko: {id: "block1@tests.mozilla.org"}},
    },
  });
  await promiseInstallWebExtension({
    manifest: {
      name: "Blocked add-on-2 with to-be-reset prefs",
      version: "1.0",
      applications: {gecko: {id: "block2@tests.mozilla.org"}},
    },
  });

  // Pre-set the preferences that we expect to get reset.
  Services.prefs.setIntPref("test.blocklist.pref1", 15);
  Services.prefs.setIntPref("test.blocklist.pref2", 15);
  Services.prefs.setBoolPref("test.blocklist.pref3", true);
  Services.prefs.setBoolPref("test.blocklist.pref4", true);


  // Before blocklist is loaded.
  let [a1, a2] = await AddonManager.getAddonsByIDs(["block1@tests.mozilla.org",
                                                    "block2@tests.mozilla.org"]);
  Assert.equal(a1.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);
  Assert.equal(a2.blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED);

  Assert.equal(Services.prefs.getIntPref("test.blocklist.pref1"), 15);
  Assert.equal(Services.prefs.getIntPref("test.blocklist.pref2"), 15);
  Assert.equal(Services.prefs.getBoolPref("test.blocklist.pref3"), true);
  Assert.equal(Services.prefs.getBoolPref("test.blocklist.pref4"), true);
});


add_task(async function test_blocks() {
  await AddonTestUtils.loadBlocklistRawData({extensions: BLOCKLIST_DATA});

  // Blocklist changes should have applied and the prefs must be reset.
  let [a1, a2] = await AddonManager.getAddonsByIDs(["block1@tests.mozilla.org",
                                                    "block2@tests.mozilla.org"]);
  Assert.notEqual(a1, null);
  Assert.equal(a1.blocklistState, Ci.nsIBlocklistService.STATE_SOFTBLOCKED);
  Assert.notEqual(a2, null);
  Assert.equal(a2.blocklistState, Ci.nsIBlocklistService.STATE_BLOCKED);

  // All these prefs must be reset to defaults.
  Assert.equal(Services.prefs.prefHasUserValue("test.blocklist.pref1"), false);
  Assert.equal(Services.prefs.prefHasUserValue("test.blocklist.pref2"), false);
  Assert.equal(Services.prefs.prefHasUserValue("test.blocklist.pref3"), false);
  Assert.equal(Services.prefs.prefHasUserValue("test.blocklist.pref4"), false);
});
