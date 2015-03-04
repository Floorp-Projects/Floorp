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
  new MockPlugin('test_with_altInfoURL', '5', Ci.nsIPluginTag.STATE_ENABLED),
  new MockPlugin('test_no_infoURL', '5', Ci.nsIPluginTag.STATE_ENABLED),
  new MockPlugin('test_newVersion', '1', Ci.nsIPluginTag.STATE_ENABLED),
  new MockPlugin('test_newVersion', '3', Ci.nsIPluginTag.STATE_ENABLED)
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
 * Test that the blocklist service correctly loads and returns the infoURL for
 * a plugin that matches the first entry in the blocklist.
 */
add_task(function* test_infoURL() {
  // The testInfoURL must match the value within the
  // <infoURL> tag in pluginInfoURL_block.xml.
  let testInfoURL = 'http://test.url.com/';

  Assert.strictEqual(Services.blocklist.getPluginInfoURL(PLUGINS[0]),
    testInfoURL, 'Should be the provided url when an infoURL tag is available');
});

/**
 * Test that the blocklist service correctly loads and returns the infoURL for
 * a plugin that partially matches an earlier entry in the blocklist.
 */
add_task(function* test_altInfoURL() {
  let altTestInfoURL = 'http://alt.test.url.com/';

  Assert.strictEqual(Services.blocklist.getPluginInfoURL(PLUGINS[1]),
    altTestInfoURL, 'Should be the alternative infoURL');
});

/**
 * Test that the blocklist service correctly returns null
 * if the infoURL tag is missing in the blocklist.xml file.
 */
add_task(function* test_infoURL_missing() {
  Assert.strictEqual(Services.blocklist.getPluginInfoURL(PLUGINS[2]), null,
    'Should be null when no infoURL tag is available.');
});

add_task(function* test_intoURL_newVersion() {
  let testInfoURL = 'http://test.url2.com/';
  Assert.strictEqual(Services.blocklist.getPluginInfoURL(PLUGINS[3]),
    testInfoURL, 'Old plugin should match');
  Assert.strictEqual(Services.blocklist.getPluginInfoURL(PLUGINS[4]),
    null, 'New plugin should not match');
});
