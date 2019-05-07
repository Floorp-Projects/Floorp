/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * MockPlugin mimics the behaviour of a plugin.
 */
function MockPlugin(name, version, enabledState) {
  this.name = name;
  this.version = version;
  this.enabledState = enabledState;
}

MockPlugin.prototype = {
  get disabled() {
    return this.enabledState == Ci.nsIPluginTag.STATE_DISABLED;
  },
};

// The mocked blocked plugin used to test the blocklist.
const PLUGINS = [
  new MockPlugin("test_with_infoURL", "5", Ci.nsIPluginTag.STATE_ENABLED),
  new MockPlugin("test_with_altInfoURL", "5", Ci.nsIPluginTag.STATE_ENABLED),
  new MockPlugin("test_no_infoURL", "5", Ci.nsIPluginTag.STATE_ENABLED),
  new MockPlugin("test_newVersion", "1", Ci.nsIPluginTag.STATE_ENABLED),
  new MockPlugin("test_newVersion", "3", Ci.nsIPluginTag.STATE_ENABLED),
];

/**
 * The entry point of the unit tests, which is also responsible of
 * copying the blocklist file to the profile folder.
 */
add_task(async function setup() {
  copyBlocklistToProfile(do_get_file("../data/pluginInfoURL_block.xml"));

  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "3", "8");
  await promiseStartupManager();
});

/**
 * Test that the blocklist service correctly loads and returns the infoURL for
 * a plugin that matches the first entry in the blocklist.
 */
add_task(async function test_infoURL() {
  // The testInfoURL must match the value within the
  // <infoURL> tag in pluginInfoURL_block.xml.
  let testInfoURL = "http://test.url.com/";

  Assert.strictEqual(await Blocklist.getPluginBlockURL(PLUGINS[0]),
    testInfoURL, "Should be the provided url when an infoURL tag is available");
});

/**
 * Test that the blocklist service correctly loads and returns the infoURL for
 * a plugin that partially matches an earlier entry in the blocklist.
 */
add_task(async function test_altInfoURL() {
  let altTestInfoURL = "http://alt.test.url.com/";

  Assert.strictEqual(await Blocklist.getPluginBlockURL(PLUGINS[1]),
    altTestInfoURL, "Should be the alternative infoURL");
});

/**
 * Test that the blocklist service correctly returns the fallback value
 * if the infoURL tag is missing in the blocklist.xml file.
 */
add_task(async function test_infoURL_missing() {
  let fallback_URL = Services.prefs.getStringPref("extensions.blocklist.detailsURL")
    + "test_plugin_noInfoURL.html";

  Assert.strictEqual(await Blocklist.getPluginBlockURL(PLUGINS[2]), fallback_URL,
    "Should be using fallback when no infoURL tag is available.");
});

add_task(async function test_intoURL_newVersion() {
  let testInfoURL = "http://test.url2.com/";
  Assert.strictEqual(await Blocklist.getPluginBlockURL(PLUGINS[3]),
    testInfoURL, "Old plugin should match");
  Assert.strictEqual(await Blocklist.getPluginBlockURL(PLUGINS[4]),
    null, "New plugin should not match");
});
