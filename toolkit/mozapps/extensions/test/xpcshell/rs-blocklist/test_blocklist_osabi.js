/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const profileDir = gProfD.clone();
profileDir.append("extensions");

const ADDONS = [
  {
    id: "test_bug393285_1@tests.mozilla.org",
    name: "extension 1",
    version: "1.0",

    // No info in blocklist, shouldn't be blocked
    notBlocklisted: [["1", "1.9"], [null, null]],
  },
  {
    id: "test_bug393285_2@tests.mozilla.org",
    name: "extension 2",
    version: "1.0",

    // Should always be blocked
    blocklisted: [["1", "1.9"], [null, null]],
  },
  {
    id: "test_bug393285_3a@tests.mozilla.org",
    name: "extension 3a",
    version: "1.0",

    // Only version 1 should be blocked
    blocklisted: [["1", "1.9"], [null, null]],
  },
  {
    id: "test_bug393285_3b@tests.mozilla.org",
    name: "extension 3b",
    version: "2.0",

    // Only version 1 should be blocked
    notBlocklisted: [["1", "1.9"]],
  },
  {
    id: "test_bug393285_4@tests.mozilla.org",
    name: "extension 4",
    version: "1.0",

    // Should be blocked for app version 1
    blocklisted: [["1", "1.9"], [null, null]],
    notBlocklisted: [["2", "1.9"]],
  },
  {
    id: "test_bug393285_5@tests.mozilla.org",
    name: "extension 5",
      version: "1.0",

    // Not blocklisted because we are a different OS
    notBlocklisted: [["2", "1.9"]],
  },
  {
    id: "test_bug393285_6@tests.mozilla.org",
    name: "extension 6",
    version: "1.0",

    // Blocklisted based on OS
    blocklisted: [["2", "1.9"]],
  },
  {
    id: "test_bug393285_7@tests.mozilla.org",
    name: "extension 7",
    version: "1.0",

    // Blocklisted based on OS
    blocklisted: [["2", "1.9"]],
  },
  {
    id: "test_bug393285_8@tests.mozilla.org",
    name: "extension 8",
    version: "1.0",

    // Not blocklisted because we are a different ABI
    notBlocklisted: [["2", "1.9"]],
  },
  {
    id: "test_bug393285_9@tests.mozilla.org",
    name: "extension 9",
    version: "1.0",

    // Blocklisted based on ABI
    blocklisted: [["2", "1.9"]],
  },
  {
    id: "test_bug393285_10@tests.mozilla.org",
    name: "extension 10",
    version: "1.0",

    // Blocklisted based on ABI
    blocklisted: [["2", "1.9"]],
  },
  {
    id: "test_bug393285_11@tests.mozilla.org",
    name: "extension 11",
    version: "1.0",

    // Doesn't match both os and abi so not blocked
    notBlocklisted: [["2", "1.9"]],
  },
  {
    id: "test_bug393285_12@tests.mozilla.org",
    name: "extension 12",
    version: "1.0",

    // Doesn't match both os and abi so not blocked
    notBlocklisted: [["2", "1.9"]],
  },
  {
    id: "test_bug393285_13@tests.mozilla.org",
    name: "extension 13",
    version: "1.0",

    // Doesn't match both os and abi so not blocked
    notBlocklisted: [["2", "1.9"]],
  },
  {
    id: "test_bug393285_14@tests.mozilla.org",
    name: "extension 14",
    version: "1.0",

    // Matches both os and abi so blocked
    blocklisted: [["2", "1.9"]],
  },
];

const ADDON_IDS = ADDONS.map(a => a.id);

const BLOCKLIST_DATA = [
  {
    guid: "test_bug393285_2@tests.mozilla.org",
    versionRange: [],
  },
  {
    guid: "test_bug393285_3a@tests.mozilla.org",
    versionRange: [{maxVersion: "1.0", minVersion: "1.0"}],
  },
  {
    guid: "test_bug393285_3b@tests.mozilla.org",
    versionRange: [{maxVersion: "1.0", minVersion: "1.0"}],
  },
  {
    guid: "test_bug393285_4@tests.mozilla.org",
    versionRange: [
      {
        maxVersion: "1.0",
        minVersion: "1.0",
        targetApplication: [
          {
            guid: "xpcshell@tests.mozilla.org",
            maxVersion: "1.0",
            minVersion: "1.0",
          },
        ],
      },
    ],
  },
  {
    guid: "test_bug393285_5@tests.mozilla.org",
    os: "Darwin",
    versionRange: [],
  },
  {
    guid: "test_bug393285_6@tests.mozilla.org",
    os: "XPCShell",
    versionRange: [],
  },
  {
    guid: "test_bug393285_7@tests.mozilla.org",
    os: "Darwin,XPCShell,WINNT",
    versionRange: [],
  },
  {
    guid: "test_bug393285_8@tests.mozilla.org",
    xpcomabi: "x86-msvc",
    versionRange: [],
  },
  {
    guid: "test_bug393285_9@tests.mozilla.org",
    xpcomabi: "noarch-spidermonkey",
    versionRange: [],
  },
  {
    guid: "test_bug393285_10@tests.mozilla.org",
    xpcomabi: "ppc-gcc3,noarch-spidermonkey,x86-msvc",
    versionRange: [],
  },
  {
    guid: "test_bug393285_11@tests.mozilla.org",
    os: "Darwin",
    xpcomabi: "ppc-gcc3,x86-msvc",
    versionRange: [],
  },
  {
    guid: "test_bug393285_12@tests.mozilla.org",
    os: "Darwin",
    xpcomabi: "ppc-gcc3,noarch-spidermonkey,x86-msvc",
    versionRange: [],
  },
  {
    guid: "test_bug393285_13@tests.mozilla.org",
    os: "XPCShell",
    xpcomabi: "ppc-gcc3,x86-msvc",
    versionRange: [],
  },
  {
    guid: "test_bug393285_14@tests.mozilla.org",
    os: "XPCShell,WINNT",
    xpcomabi: "ppc-gcc3,x86-msvc,noarch-spidermonkey",
    versionRange: [],
  },
];

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");
  await promiseStartupManager();

  for (let addon of ADDONS) {
    await promiseInstallWebExtension({
      manifest: {
        name: addon.name,
        version: addon.version,
        applications: {gecko: {id: addon.id}},
      },
    });
  }

  let addons = await getAddons(ADDON_IDS);
  for (let id of ADDON_IDS) {
    equal(addons.get(id).blocklistState, Ci.nsIBlocklistService.STATE_NOT_BLOCKED,
          `Add-on ${id} should not initially be blocked`);
  }
});

add_task(async function test_1() {
  await AddonTestUtils.loadBlocklistRawData({extensions: BLOCKLIST_DATA});

  let addons = await getAddons(ADDON_IDS);
  async function isBlocklisted(addon, appVer, toolkitVer) {
    let state = await Blocklist.getAddonBlocklistState(addon, appVer, toolkitVer);
    return state != Services.blocklist.STATE_NOT_BLOCKED;
  }
  for (let addon of ADDONS) {
    let {id} = addon;
    for (let blocklisted of addon.blocklisted || []) {
      ok(await isBlocklisted(addons.get(id), ...blocklisted),
         `Add-on ${id} should be blocklisted in app/platform version ${blocklisted}`);
    }
    for (let notBlocklisted of addon.notBlocklisted || []) {
      ok(!(await isBlocklisted(addons.get(id), ...notBlocklisted)),
         `Add-on ${id} should not be blocklisted in app/platform version ${notBlocklisted}`);
    }
  }
});
