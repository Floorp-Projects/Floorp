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
  
  // No info in blocklist, shouldn't be blocked
  do_check_false(blocklist.isAddonBlocklisted("test_bug393285_1@tests.mozilla.org", "1", "1", "1.9"));

  // Should always be blocked
  do_check_true(blocklist.isAddonBlocklisted("test_bug393285_2@tests.mozilla.org", "1", "1", "1.9"));

  // Only version 1 should be blocked
  do_check_true(blocklist.isAddonBlocklisted("test_bug393285_3@tests.mozilla.org", "1", "1", "1.9"));
  do_check_false(blocklist.isAddonBlocklisted("test_bug393285_3@tests.mozilla.org", "2", "1", "1.9"));

  // Should be blocked for app version 1
  do_check_true(blocklist.isAddonBlocklisted("test_bug393285_4@tests.mozilla.org", "1", "1", "1.9"));
  do_check_false(blocklist.isAddonBlocklisted("test_bug393285_4@tests.mozilla.org", "1", "2", "1.9"));

  // Not blocklisted because we are a different OS
  do_check_false(blocklist.isAddonBlocklisted("test_bug393285_5@tests.mozilla.org", "1", "2", "1.9"));

  // Blocklisted based on OS
  do_check_true(blocklist.isAddonBlocklisted("test_bug393285_6@tests.mozilla.org", "1", "2", "1.9"));
  do_check_true(blocklist.isAddonBlocklisted("test_bug393285_7@tests.mozilla.org", "1", "2", "1.9"));

  // Not blocklisted because we are a different ABI
  do_check_false(blocklist.isAddonBlocklisted("test_bug393285_8@tests.mozilla.org", "1", "2", "1.9"));

  // Blocklisted based on ABI
  do_check_true(blocklist.isAddonBlocklisted("test_bug393285_9@tests.mozilla.org", "1", "2", "1.9"));
  do_check_true(blocklist.isAddonBlocklisted("test_bug393285_10@tests.mozilla.org", "1", "2", "1.9"));

  // Doesnt match both os and abi so not blocked
  do_check_false(blocklist.isAddonBlocklisted("test_bug393285_11@tests.mozilla.org", "1", "2", "1.9"));
  do_check_false(blocklist.isAddonBlocklisted("test_bug393285_12@tests.mozilla.org", "1", "2", "1.9"));
  do_check_false(blocklist.isAddonBlocklisted("test_bug393285_13@tests.mozilla.org", "1", "2", "1.9"));

  // Matches both os and abi so blocked
  do_check_true(blocklist.isAddonBlocklisted("test_bug393285_14@tests.mozilla.org", "1", "2", "1.9"));
}
