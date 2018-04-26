/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var PLUGINS = [{
  // Normal blocklisted plugin, before an invalid regexp
  name: "test_bug468528_1",
  version: "5",
  disabled: false,
  blocklisted: false
},
{
  // Normal blocklisted plugin, with an invalid regexp
  name: "test_bug468528_2",
  version: "5",
  disabled: false,
  blocklisted: false
},
{
  // Normal blocklisted plugin, after an invalid regexp
  name: "test_bug468528_3",
  version: "5",
  disabled: false,
  blocklisted: false
},
{
  // Non-blocklisted plugin
  name: "test_bug468528_4",
  version: "5",
  disabled: false,
  blocklisted: false
}];


add_task(async function checkBlocklistForRegexes() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  // We cannot force the blocklist to update so just copy our test list to the profile
  copyBlocklistToProfile(do_get_file("data/test_bug468528.xml"));

  var {blocklist} = Services;

  // The blocklist service defers plugin request until the Blocklist
  // module loads. Make sure it loads, or we'll wait forever.
  executeSoon(() => {
    void Blocklist;
  });

  // blocked (sanity check)
  Assert.equal(await blocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9"), blocklist.STATE_BLOCKED);

  // not blocked - won't match due to invalid regexp
  Assert.equal(await blocklist.getPluginBlocklistState(PLUGINS[1], "1", "1.9"), blocklist.STATE_NOT_BLOCKED);

  // blocked - the invalid regexp for the previous item shouldn't affect this one
  Assert.equal(await blocklist.getPluginBlocklistState(PLUGINS[2], "1", "1.9"), blocklist.STATE_BLOCKED);

  // not blocked - the previous invalid regexp shouldn't act as a wildcard
  Assert.equal(await blocklist.getPluginBlocklistState(PLUGINS[3], "1", "1.9"), blocklist.STATE_NOT_BLOCKED);
});
