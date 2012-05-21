/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;

const nsIBLS = Ci.nsIBlocklistService;

var PLUGINS = [{
  // blocklisted - default severity
  name: "test_bug514327_1",
  version: "5",
  disabled: false,
  blocklisted: false
},
{
  // outdated - severity of "0"
  name: "test_bug514327_2",
  version: "5",
  disabled: false,
  blocklisted: false
},
{
  // outdated - severity of "0"
  name: "test_bug514327_3",
  version: "5",
  disabled: false,
  blocklisted: false
},
{
  // not blocklisted, not outdated
  name: "test_bug514327_4",
  version: "5",
  disabled: false,
  blocklisted: false,
  outdated: false
}];


function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  var blocklistFile = gProfD.clone();
  blocklistFile.append("blocklist.xml");
  if (blocklistFile.exists())
    blocklistFile.remove(false);
  var source = do_get_file("data/test_bug514327_1.xml");
  source.copyTo(gProfD, "blocklist.xml");

  var blocklist = Cc["@mozilla.org/extensions/blocklist;1"].getService(nsIBLS);

  // blocked (sanity check)
  do_check_true(blocklist.getPluginBlocklistState(PLUGINS[0], "1", "1.9") == nsIBLS.STATE_BLOCKED);

  // outdated
  do_check_true(blocklist.getPluginBlocklistState(PLUGINS[1], "1", "1.9") == nsIBLS.STATE_OUTDATED);

  // outdated
  do_check_true(blocklist.getPluginBlocklistState(PLUGINS[2], "1", "1.9") == nsIBLS.STATE_OUTDATED);

  // not blocked
  do_check_true(blocklist.getPluginBlocklistState(PLUGINS[3], "1", "1.9") == nsIBLS.STATE_NOT_BLOCKED);
}
