/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;

const nsIBLS = Ci.nsIBlocklistService;

// Finds the test nsIPluginTag
function get_test_plugintag() {
  var host = Cc["@mozilla.org/plugin/host;1"].getService(Ci.nsIPluginHost);
  var tags = host.getPluginTags();
  for (var i = 0; i < tags.length; i++) {
    if (tags[i].name == "Test Plug-in")
      return tags[i];
  }
  return null;
}

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  var blocklistFile = gProfD.clone();
  blocklistFile.append("blocklist.xml");
  if (blocklistFile.exists())
    blocklistFile.remove(false);
  var source = do_get_file("data/test_bug514327_2.xml");
  source.copyTo(gProfD, "blocklist.xml");

  var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].getService(nsIBLS);
  var prefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

  var plugin = get_test_plugintag();
  if (!plugin)
    do_throw("Plugin tag not found");

  //run the code after the blocklist is closed
  Services.obs.notifyObservers(null, "addon-blocklist-closed", null);
  do_execute_soon(function() {
    // should be marked as outdated by the blocklist
    do_check_true(blocklist.getPluginBlocklistState(plugin, "1", "1.9") == nsIBLS.STATE_OUTDATED);

    // should indicate that a warning should be shown
    do_check_true(prefs.getBoolPref("plugins.update.notifyUser"));
  });
}
