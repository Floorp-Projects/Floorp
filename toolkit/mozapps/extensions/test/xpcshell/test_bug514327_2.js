/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var Cc = Components.classes;
var Ci = Components.interfaces;

const nsIBLS = Ci.nsIBlocklistService;

// Finds the test nsIPluginTag
function get_test_plugintag() {
  var host = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  var tags = host.getPluginTags();
  for (let tag of tags) {
    if (tag.name == "Test Plug-in")
      return tag;
  }
  return null;
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  copyBlocklistToProfile(do_get_file("data/test_bug514327_2.xml"));

  var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].getService(nsIBLS);

  Services.prefs.setBoolPref("plugin.load_flash_only", false);

  var plugin = get_test_plugintag();
  if (!plugin)
    do_throw("Plugin tag not found");

  // run the code after the blocklist is closed
  Services.obs.notifyObservers(null, "addon-blocklist-closed");
  do_execute_soon(function() {
    // should be marked as outdated by the blocklist
    Assert.ok(blocklist.getPluginBlocklistState(plugin, "1", "1.9") == nsIBLS.STATE_OUTDATED);
  });
}
