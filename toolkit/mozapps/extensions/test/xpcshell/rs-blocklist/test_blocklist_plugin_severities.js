/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var PLUGINS = [{
  // blocklisted - default severity
  name: "test_bug514327_1",
  version: "5",
  disabled: false,
  blocklisted: false,
},
{
  // outdated - severity of "0"
  name: "test_bug514327_2",
  version: "5",
  disabled: false,
  blocklisted: false,
},
{
  // outdated - severity of "0"
  name: "test_bug514327_3",
  version: "5",
  disabled: false,
  blocklisted: false,
},
{
  // not blocklisted, not outdated
  name: "test_bug514327_4",
  version: "5",
  disabled: false,
  blocklisted: false,
  outdated: false,
}];

let BLOCKLIST_DATA = {
  plugins: [
    {
      matchName: "^test_bug514327_1",
      versionRange: [],
    },
    {
      matchName: "^test_bug514327_2",
      versionRange: [{severity: "0"}],
    },
    {
      matchName: "^test_bug514327_3",
      versionRange: [{severity: "0"}],
    },
  ],
};

add_task(async function checkBlocklistSeverities() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  await AddonTestUtils.loadBlocklistRawData(BLOCKLIST_DATA);

  var {blocklist} = Services;

  // The blocklist service defers plugin request until the Blocklist
  // module loads. Make sure it loads, or we'll wait forever.
  executeSoon(() => {
    void Blocklist;
  });

  // blocked (sanity check)
  Assert.equal(await blocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9"), blocklist.STATE_BLOCKED);

  // outdated
  Assert.equal(await blocklist.getPluginBlocklistState(PLUGINS[1], "1", "1.9"), blocklist.STATE_OUTDATED);

  // outdated
  Assert.equal(await blocklist.getPluginBlocklistState(PLUGINS[2], "1", "1.9"), blocklist.STATE_OUTDATED);

  // not blocked
  Assert.equal(await blocklist.getPluginBlocklistState(PLUGINS[3], "1", "1.9"), blocklist.STATE_NOT_BLOCKED);
});
