/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

const Ci = Components.interfaces;
Components.utils.import("resource://gre/modules/Services.jsm");

/**
 * MockPlugin mimics the behaviour of a plugin.
 */
function MockPlugin(name, version, enabledState) {
  this.name = name;
  this.version = version;
  this.enabledState = enabledState;
}

MockPlugin.prototype = {
  get blocklisted() {
    let bls = Services.blocklist;
    return bls.getPluginBlocklistState(this) == bls.STATE_BLOCKED;
  },

  get disabled() {
    return this.enabledState == Ci.nsIPluginTag.STATE_DISABLED;
  }
};

// The mocked blocked plugin used to test the blocklist.
const PLUGINS = [
  new MockPlugin('test_with_infoURL', '5', Ci.nsIPluginTag.STATE_ENABLED),
  new MockPlugin('test_no_infoURL', '5', Ci.nsIPluginTag.STATE_ENABLED)
];

/**
 * The entry point of the unit tests, which is also responsible of
 * copying the blocklist file to the profile folder.
 */
function run_test() {
  copyBlocklistToProfile(do_get_file('data/pluginInfoURL_block.xml'));

  createAppInfo('xpcshell@tests.mozilla.org', 'XPCShell', '3', '8');
  startupManager();

  run_next_test();
}

/**
 * Test that the blocklist service correctly loads and returns the infoURL
 * from the blocklist file for a matched plugin.
 */
add_task(function* test_infoURL() {
  // The testInfoURL must match the value within the
  // <infoURL> tag in pluginInfoURL_block.xml.
  let testInfoURL = 'http://test.url.com/';

  Assert.strictEqual(Services.blocklist.getPluginInfoURL(PLUGINS[0]),
    testInfoURL, 'Plugin info urls should match');
});

/**
 * Test that the blocklist service correctly returns null
 * if the infoURL tag is missing in the blocklist.xml file.
 */
add_task(function* test_infoURL_missing() {
  Assert.strictEqual(Services.blocklist.getPluginInfoURL(PLUGINS[1]), null,
    'Should be null when no infoURL tag is available.');
});
