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
 * Dave Townsend <dtownsend@oxymoronical.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2009
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
 * the terms of any one of the MPL, the GPL or the LGPL
 *
 * ***** END LICENSE BLOCK *****
 */

function run_test() {
  do_test_pending();
  createAppInfo("xpcshell@tests.mozilla.org", "XPCShell", "2.2.3", "2");
  Services.prefs.setBoolPref(PREF_EM_STRICT_COMPATIBILITY, true);

  // inject the add-ons into the profile
  var dest = gProfD.clone();
  dest.append("extensions");
  dest.append("bug470377_1@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  var source = do_get_file("data/test_bug470377/install_1.rdf");
  source.copyTo(dest, "install.rdf");
  dest = gProfD.clone();
  dest.append("extensions");
  dest.append("bug470377_2@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  source = do_get_file("data/test_bug470377/install_2.rdf");
  source.copyTo(dest, "install.rdf");
  dest = gProfD.clone();
  dest.append("extensions");
  dest.append("bug470377_3@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  source = do_get_file("data/test_bug470377/install_3.rdf");
  source.copyTo(dest, "install.rdf");
  dest = gProfD.clone();
  dest.append("extensions");
  dest.append("bug470377_4@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  source = do_get_file("data/test_bug470377/install_4.rdf");
  source.copyTo(dest, "install.rdf");
  dest = gProfD.clone();
  dest.append("extensions");
  dest.append("bug470377_5@tests.mozilla.org");
  dest.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0755);
  source = do_get_file("data/test_bug470377/install_5.rdf");
  source.copyTo(dest, "install.rdf");

  startupManager();

  run_test_1();
}

function run_test_1() {
  AddonManager.getAddonsByIDs(["bug470377_1@tests.mozilla.org",
                               "bug470377_2@tests.mozilla.org",
                               "bug470377_3@tests.mozilla.org",
                               "bug470377_4@tests.mozilla.org",
                               "bug470377_5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {
    do_check_neq(a1, null);
    do_check_false(a1.isActive);
    do_check_neq(a2, null);
    do_check_false(a2.isActive);
    do_check_neq(a3, null);
    do_check_false(a3.isActive);
    do_check_neq(a4, null);
    do_check_true(a4.isActive);
    do_check_neq(a5, null);
    do_check_true(a5.isActive);

    run_test_2();
  });
}

function run_test_2() {
  // Disable compatibility checks
  var channel = "default";
  try {
    channel = Services.prefs.getCharPref("app.update.channel");
  }
  catch (e) { }

  if (channel != "aurora" &&
      channel != "beta" &&
      channel != "release") {
    Services.prefs.setBoolPref("extensions.checkCompatibility.nightly", false);
  }
  else {
    Services.prefs.setBoolPref("extensions.checkCompatibility.2.2", false);
  }
  restartManager();

  AddonManager.getAddonsByIDs(["bug470377_1@tests.mozilla.org",
                               "bug470377_2@tests.mozilla.org",
                               "bug470377_3@tests.mozilla.org",
                               "bug470377_4@tests.mozilla.org",
                               "bug470377_5@tests.mozilla.org"],
                               function([a1, a2, a3, a4, a5]) {
    do_check_neq(a1, null);
    do_check_false(a1.isActive);
    do_check_neq(a2, null);
    do_check_true(a2.isActive);
    do_check_neq(a3, null);
    do_check_true(a3.isActive);
    do_check_neq(a4, null);
    do_check_true(a4.isActive);
    do_check_neq(a5, null);
    do_check_true(a5.isActive);

    do_test_finished();
  });
}
