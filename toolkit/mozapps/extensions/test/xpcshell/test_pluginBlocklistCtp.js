/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const nsIBLS = Components.interfaces.nsIBlocklistService;

var PLUGINS = [{
  // severity=0, vulnerabilitystatus=0 -> outdated
  name: "test_plugin_0",
  version: "5",
  disabled: false,
  blocklisted: false
},
{
  // severity=0, vulnerabilitystatus=1 -> update available
  name: "test_plugin_1",
  version: "5",
  disabled: false,
  blocklisted: false
},
{
  // severity=0, vulnerabilitystatus=2 -> no update
  name: "test_plugin_2",
  version: "5",
  disabled: false,
  blocklisted: false
},
{
  // no severity field -> severity=3 by default -> hardblock
  name: "test_plugin_3",
  version: "5",
  disabled: false,
  blocklisted: false
},
{
  // severity=1, vulnerabilitystatus=2 -> softblock
  name: "test_plugin_4",
  version: "5",
  disabled: false,
  blocklisted: false
},
{
  // not in the blocklist -> not blocked
  name: "test_plugin_5",
  version: "5",
  disabled: false,
  blocklisted: false
}];


function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  // We cannot force the blocklist to update so just copy our test list to the profile
  var blocklistFile = gProfD.clone();
  blocklistFile.append("blocklist.xml");
  if (blocklistFile.exists())
    blocklistFile.remove(false);
  var source = do_get_file("data/test_pluginBlocklistCtp.xml");
  source.copyTo(gProfD, "blocklist.xml");

  var blocklist = Components.classes["@mozilla.org/extensions/blocklist;1"]
                            .getService(nsIBLS);

  do_check_true(blocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9") == nsIBLS.STATE_OUTDATED);

  do_check_true(blocklist.getPluginBlocklistState(PLUGINS[1], "1", "1.9") == nsIBLS.STATE_VULNERABLE_UPDATE_AVAILABLE);
  
  do_check_true(blocklist.getPluginBlocklistState(PLUGINS[2], "1", "1.9") == nsIBLS.STATE_VULNERABLE_NO_UPDATE);

  do_check_true(blocklist.getPluginBlocklistState(PLUGINS[3], "1", "1.9") == nsIBLS.STATE_BLOCKED);

  do_check_true(blocklist.getPluginBlocklistState(PLUGINS[4], "1", "1.9") == nsIBLS.STATE_SOFTBLOCKED);

  do_check_true(blocklist.getPluginBlocklistState(PLUGINS[5], "1", "1.9") == nsIBLS.STATE_NOT_BLOCKED);
}
