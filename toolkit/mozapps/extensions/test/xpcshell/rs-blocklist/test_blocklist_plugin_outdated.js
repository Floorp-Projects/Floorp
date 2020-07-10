/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cm = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

const nsIBLS = Ci.nsIBlocklistService;

var gBlocklist = null;

var PLUGINS = [
  {
    // Tests a plugin whose state goes from not-blocked, to outdated
    name: "test_bug514327_outdated",
    version: "5",
    disabled: false,
    blocklisted: false,
  },
  {
    // Used to trigger the blocklist dialog, which indicates the blocklist has updated
    name: "test_bug514327_1",
    version: "5",
    disabled: false,
    blocklisted: false,
  },
  {
    // Used to trigger the blocklist dialog, which indicates the blocklist has updated
    name: "test_bug514327_2",
    version: "5",
    disabled: false,
    blocklisted: false,
  },
].map(opts => new MockPluginTag(opts, opts.enabledState));

mockPluginHost(PLUGINS);

const BLOCKLIST_DATA = {
  empty: {},
  outdated_1: {
    plugins: [
      {
        matchName: "test_bug514327_1",
        versionRange: [],
      },
      {
        matchName: "test_bug514327_outdated",
        versionRange: [{ severity: "0" }],
      },
    ],
  },
  outdated_2: {
    plugins: [
      {
        matchName: "test_bug514327_2",
        versionRange: [],
      },
      {
        matchName: "test_bug514327_outdated",
        versionRange: [{ severity: "0" }],
      },
    ],
  },
};

var BlocklistPrompt = {
  get wrappedJSObject() {
    return this;
  },

  prompt(list) {
    // Should only include one item
    Assert.equal(list.length, 1);
    // And that item should be the blocked plugin, not the outdated one
    var item = list[0];
    Assert.ok(item.item instanceof Ci.nsIPluginTag);
    Assert.notEqual(item.name, "test_bug514327_outdated");
  },

  QueryInterface: ChromeUtils.generateQI([]),
};

let factory = ComponentUtils.generateSingletonFactory(function() {
  return BlocklistPrompt;
});
Cm.registerFactory(
  Components.ID("{26d32654-30c7-485d-b983-b4d2568aebba}"),
  "Blocklist Prompt",
  "@mozilla.org/addons/blocklist-prompt;1",
  factory
);

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  await promiseStartupManager();
  // initialize the blocklist with no entries
  await AddonTestUtils.loadBlocklistRawData(BLOCKLIST_DATA.empty);

  gBlocklist = Services.blocklist;

  // The blocklist service defers plugin request until the Blocklist
  // module loads. Make sure it loads, or we'll wait forever.
  executeSoon(() => {
    void Blocklist;
  });

  // should NOT be marked as outdated by the blocklist
  Assert.equal(
    await gBlocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9"),
    nsIBLS.STATE_NOT_BLOCKED
  );
});

add_task(async function test_part_1() {
  // update blocklist with data that marks the plugin as outdated
  await AddonTestUtils.loadBlocklistRawData(BLOCKLIST_DATA.outdated_1);

  // plugin should now be marked as outdated
  Assert.equal(
    await gBlocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9"),
    nsIBLS.STATE_OUTDATED
  );
});

add_task(async function test_part_2() {
  // update blocklist with data that marks the plugin as outdated
  await AddonTestUtils.loadBlocklistRawData(BLOCKLIST_DATA.outdated_2);

  // plugin should still be marked as outdated
  Assert.equal(
    await gBlocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9"),
    nsIBLS.STATE_OUTDATED
  );
});
