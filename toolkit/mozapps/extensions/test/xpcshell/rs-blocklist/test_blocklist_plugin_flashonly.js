/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsIBLS = Ci.nsIBlocklistService;

// Finds the test nsIPluginTag
function get_test_plugintag() {
  var host = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  var tags = host.getPluginTags();
  for (let tag of tags) {
    if (tag.name == "Test Plug-in") {
      return tag;
    }
  }
  return null;
}

add_task(async function checkFlashOnlyPluginState() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  Services.prefs.setBoolPref("plugin.load_flash_only", false);
  // plugin.load_flash_only is only respected if xpc::IsInAutomation is true.
  // This is not the case by default in xpcshell tests, unless the following
  // pref is also set. Fixing this generically is bug 1598804
  Services.prefs.setBoolPref(
    "security.turn_off_all_security_so_that_viruses_can_take_over_this_computer",
    true
  );

  await AddonTestUtils.loadBlocklistRawData({
    plugins: [
      {
        matchName: "Test Plug-in",
        versionRange: [{ severity: "0" }],
      },
    ],
  });

  var plugin = get_test_plugintag();
  if (!plugin) {
    do_throw("Plugin tag not found");
  }

  // run the code after the blocklist is closed
  Services.obs.notifyObservers(null, "addon-blocklist-closed");
  await new Promise(executeSoon);
  // should be marked as outdated by the blocklist
  Assert.equal(
    await Blocklist.getPluginBlocklistState(plugin, "1", "1.9"),
    nsIBLS.STATE_OUTDATED
  );
});
