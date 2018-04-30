/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cm = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);

ChromeUtils.import("resource://testing-common/httpd.js");
ChromeUtils.import("resource://testing-common/MockRegistrar.jsm");

const nsIBLS = Ci.nsIBlocklistService;

var gBlocklist = null;
var gTestserver = AddonTestUtils.createHttpServer({hosts: ["example.com"]});
gTestserver.registerDirectory("/data/", do_get_file("data"));

var PLUGINS = [{
  // Tests a plugin whose state goes from not-blocked, to outdated
  name: "test_bug514327_outdated",
  version: "5",
  disabled: false,
  blocklisted: false
}, {
  // Used to trigger the blocklist dialog, which indicates the blocklist has updated
  name: "test_bug514327_1",
  version: "5",
  disabled: false,
  blocklisted: false
}, {
  // Used to trigger the blocklist dialog, which indicates the blocklist has updated
  name: "test_bug514327_2",
  version: "5",
  disabled: false,
  blocklisted: false
}].map(opts => new MockPluginTag(opts, opts.enabledState));

mockPluginHost(PLUGINS);

var BlocklistPrompt = {
  get wrappedJSObject() { return this; },

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

add_task(async function setup() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  // initialize the blocklist with no entries
  copyBlocklistToProfile(do_get_file("data/test_bug514327_3_empty.xml"));

  await promiseStartupManager();

  gBlocklist = Services.blocklist;

  // The blocklist service defers plugin request until the Blocklist
  // module loads. Make sure it loads, or we'll wait forever.
  executeSoon(() => {
    void Blocklist;
  });

  // should NOT be marked as outdated by the blocklist
  Assert.equal(await gBlocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9"), nsIBLS.STATE_NOT_BLOCKED);
});

add_task(async function test_part_1() {
  // update blocklist with data that marks the plugin as outdated
  await loadBlocklist("test_bug514327_3_outdated_1.xml");

  // plugin should now be marked as outdated
  Assert.equal(await gBlocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9"), nsIBLS.STATE_OUTDATED);

});

add_task(async function test_part_2() {
  // update blocklist with data that marks the plugin as outdated
  await loadBlocklist("test_bug514327_3_outdated_2.xml");

  // plugin should still be marked as outdated
  Assert.equal(await gBlocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9"), nsIBLS.STATE_OUTDATED);
});
