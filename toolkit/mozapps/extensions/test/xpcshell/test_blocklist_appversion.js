/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const Cm = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

var testserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
testserver.registerDirectory("/data/", do_get_file("data"));

var ADDONS = [{
  id: "test_bug449027_1@tests.mozilla.org",
  name: "Bug 449027 Addon Test 1",
  version: "5",
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  id: "test_bug449027_2@tests.mozilla.org",
  name: "Bug 449027 Addon Test 2",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_3@tests.mozilla.org",
  name: "Bug 449027 Addon Test 3",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_4@tests.mozilla.org",
  name: "Bug 449027 Addon Test 4",
  version: "5",
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  id: "test_bug449027_5@tests.mozilla.org",
  name: "Bug 449027 Addon Test 5",
  version: "5",
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  id: "test_bug449027_6@tests.mozilla.org",
  name: "Bug 449027 Addon Test 6",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_7@tests.mozilla.org",
  name: "Bug 449027 Addon Test 7",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_8@tests.mozilla.org",
  name: "Bug 449027 Addon Test 8",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_9@tests.mozilla.org",
  name: "Bug 449027 Addon Test 9",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_10@tests.mozilla.org",
  name: "Bug 449027 Addon Test 10",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_11@tests.mozilla.org",
  name: "Bug 449027 Addon Test 11",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_12@tests.mozilla.org",
  name: "Bug 449027 Addon Test 12",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_13@tests.mozilla.org",
  name: "Bug 449027 Addon Test 13",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: false
}, {
  id: "test_bug449027_14@tests.mozilla.org",
  name: "Bug 449027 Addon Test 14",
  version: "5",
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  id: "test_bug449027_15@tests.mozilla.org",
  name: "Bug 449027 Addon Test 15",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_16@tests.mozilla.org",
  name: "Bug 449027 Addon Test 16",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_17@tests.mozilla.org",
  name: "Bug 449027 Addon Test 17",
  version: "5",
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  id: "test_bug449027_18@tests.mozilla.org",
  name: "Bug 449027 Addon Test 18",
  version: "5",
  start: false,
  appBlocks: false,
  toolkitBlocks: false
}, {
  id: "test_bug449027_19@tests.mozilla.org",
  name: "Bug 449027 Addon Test 19",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_20@tests.mozilla.org",
  name: "Bug 449027 Addon Test 20",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_21@tests.mozilla.org",
  name: "Bug 449027 Addon Test 21",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_22@tests.mozilla.org",
  name: "Bug 449027 Addon Test 22",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_23@tests.mozilla.org",
  name: "Bug 449027 Addon Test 23",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_24@tests.mozilla.org",
  name: "Bug 449027 Addon Test 24",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}, {
  id: "test_bug449027_25@tests.mozilla.org",
  name: "Bug 449027 Addon Test 25",
  version: "5",
  start: false,
  appBlocks: true,
  toolkitBlocks: true
}];

class MockPlugin extends MockPluginTag {
  constructor(name, version, start, appBlocks, toolkitBlocks) {
    super({name, version});
    this.start = start;
    this.appBlocks = appBlocks;
    this.toolkitBlocks = toolkitBlocks;
  }
}

var PLUGINS = [
  new MockPlugin("test_bug449027_1", "5", false, false, false),
  new MockPlugin("test_bug449027_2", "5", false, true, false),
  new MockPlugin("test_bug449027_3", "5", false, true, false),
  new MockPlugin("test_bug449027_4", "5", false, false, false),
  new MockPlugin("test_bug449027_5", "5", false, false, false),
  new MockPlugin("test_bug449027_6", "5", false, true, false),
  new MockPlugin("test_bug449027_7", "5", false, true, false),
  new MockPlugin("test_bug449027_8", "5", false, true, false),
  new MockPlugin("test_bug449027_9", "5", false, true, false),
  new MockPlugin("test_bug449027_10", "5", false, true, false),
  new MockPlugin("test_bug449027_11", "5", false, true, false),
  new MockPlugin("test_bug449027_12", "5", false, true, false),
  new MockPlugin("test_bug449027_13", "5", false, true, false),
  new MockPlugin("test_bug449027_14", "5", false, false, false),
  new MockPlugin("test_bug449027_15", "5", false, true, true),
  new MockPlugin("test_bug449027_16", "5", false, true, true),
  new MockPlugin("test_bug449027_17", "5", false, false, false),
  new MockPlugin("test_bug449027_18", "5", false, false, false),
  new MockPlugin("test_bug449027_19", "5", false, true, true),
  new MockPlugin("test_bug449027_20", "5", false, true, true),
  new MockPlugin("test_bug449027_21", "5", false, true, true),
  new MockPlugin("test_bug449027_22", "5", false, true, true),
  new MockPlugin("test_bug449027_23", "5", false, true, true),
  new MockPlugin("test_bug449027_24", "5", false, true, true),
  new MockPlugin("test_bug449027_25", "5", false, true, true)
];

var gNewBlocks = [];

mockPluginHost(PLUGINS);

var BlocklistPrompt = {
  get wrappedJSObject() { return this; },

  prompt(list) {
    gNewBlocks = list.map(item => `${item.name} ${item.version}`);
  },

  QueryInterface: ChromeUtils.generateQI([]),
};


async function loadBlocklist(file) {
  let blocklistUpdated = TestUtils.topicObserved("blocklist-updated");

  Services.prefs.setCharPref("extensions.blocklist.url",
                             "http://example.com/data/" + file);
  Blocklist.notify();

  await blocklistUpdated;
}

let factory = XPCOMUtils.generateSingletonFactory(function() { return BlocklistPrompt; });
Cm.registerFactory(Components.ID("{26d32654-30c7-485d-b983-b4d2568aebba}"),
                   "Blocklist Prompt",
                   "@mozilla.org/addons/blocklist-prompt;1", factory);

function createAddon(addon) {
  return promiseInstallXPI({
    name: addon.name,
    id: addon.id,
    version: addon.version,
    bootstrap: true,
    targetApplications: [{
      id: "xpcshell@tests.mozilla.org",
      minVersion: "3",
      maxVersion: "3"}],
  });
}

/**
 * Checks that items are blocklisted correctly according to the current test.
 * If a lastTest is provided checks that the notification dialog got passed
 * the newly blocked items compared to the previous test.
 */
async function checkState(test, lastTest, callback) {
  let addons = await AddonManager.getAddonsByIDs(ADDONS.map(a => a.id));

  const bls = Ci.nsIBlocklistService;

  await TestUtils.waitForCondition(
    () => ADDONS.every((addon, i) => addon[test] == (addons[i].blocklistState == bls.STATE_BLOCKED))
  ).catch(() => { /* ignore exceptions; the following test will fail anyway. */ });

  for (let [i, addon] of ADDONS.entries()) {
    var blocked = addons[i].blocklistState == Ci.nsIBlocklistService.STATE_BLOCKED;
    equal(blocked, addon[test],
          `Blocklist state should match expected for extension ${addon.id}, test ${test}`);
  }

  for (let plugin of PLUGINS) {
    equal(await plugin.isBlocklisted(), plugin[test],
          `Blocklist state should match expected for plugin ${plugin.name}, test ${test}`);
  }

  if (lastTest) {
    var expected = 0;
    for (let plugin of PLUGINS) {
      if (plugin[test] && !plugin[lastTest]) {
        ok(gNewBlocks.includes(`${plugin.name} ${plugin.version}`),
           `Plugin ${plugin.name} should have been listed in the blocklist notification for test ${test}`);
        expected++;
      }
    }

    Assert.equal(expected, gNewBlocks.length);
  }
}

add_task(async function test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  await promiseStartupManager();

  for (let addon of ADDONS) {
    await createAddon(addon);
  }

  let addons = await AddonManager.getAddonsByIDs(ADDONS.map(a => a.id));
  for (var i = 0; i < ADDONS.length; i++) {
    ok(addons[i], `Addon ${i + 1} should have been correctly installed`);
  }

  await checkState("start");
});

/**
 * Load the toolkit based blocks
 */
add_task(async function test_pt2() {
  await loadBlocklist("test_bug449027_toolkit.xml");

  await checkState("toolkitBlocks", "start");
});

/**
 * Load the application based blocks
 */
add_task(async function test_pt3() {
  await loadBlocklist("test_bug449027_app.xml");

  await checkState("appBlocks", "toolkitBlocks");
});
