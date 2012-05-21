/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  // We cannot force the blocklist to update so just copy our test list to the profile
  var blocklistFile = gProfD.clone();
  blocklistFile.append("blocklist.xml");
  if (blocklistFile.exists())
    blocklistFile.remove(false);
  var source = do_get_file("data/test_bug393285.xml");
  source.copyTo(gProfD, "blocklist.xml");

  var blocklist = Components.classes["@mozilla.org/extensions/blocklist;1"]
                            .getService(Components.interfaces.nsIBlocklistService);
  
  // All these should be blocklisted for the current app.
  do_check_false(blocklist.isAddonBlocklisted("test_bug393285_1@tests.mozilla.org", "1", null, null));
  do_check_true(blocklist.isAddonBlocklisted("test_bug393285_2@tests.mozilla.org", "1", null, null));
  do_check_true(blocklist.isAddonBlocklisted("test_bug393285_3@tests.mozilla.org", "1", null, null));
  do_check_true(blocklist.isAddonBlocklisted("test_bug393285_4@tests.mozilla.org", "1", null, null));

}
