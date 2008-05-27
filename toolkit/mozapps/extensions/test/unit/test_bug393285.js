/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 *      Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK *****
 */

function run_test() {
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "1", "1.9");

  // We cannot force the blocklist to update so just copy our test list to the profile
  var source = do_get_file("toolkit/mozapps/extensions/test/unit/data/test_bug393285.xml");
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
