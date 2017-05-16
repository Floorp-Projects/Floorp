/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* Tests for handling of the preferences 'dirty' flag (bug 985998) */

const PREF_INVALID = 0;
const PREF_BOOL    = 128;
const PREF_INT     = 64;
const PREF_STRING  = 32;

function run_test() {

  var ps = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefService);

  let defaultBranch = ps.getDefaultBranch("");
  let userBranch = ps.getBranch("");

  let prefFile = do_get_profile();
  prefFile.append("prefs.js");

  // dirty flag only applies to the default pref file save, not all of them,
  // so we need to set the default pref file first.  Chances are, the file
  // does not exist, but we don't need it to - we just want to set the
  // name/location to match
  //
  try {
    ps.readUserPrefs(prefFile);
  } catch (e) {
    // we're fine if the file isn't there
  }

  //**************************************************************************//
  // prefs are not dirty after a write
  ps.savePrefFile(null);
  do_check_false(ps.dirty);

  // set a new a user value, we should become dirty
  userBranch.setBoolPref("DirtyTest.new.bool", true);
  do_check_true(ps.dirty);
  ps.savePrefFile(null);
  // Overwrite a pref with the same value => not dirty
  userBranch.setBoolPref("DirtyTest.new.bool", true);
  do_check_false(ps.dirty);

  // Repeat for the other two types
  userBranch.setIntPref("DirtyTest.new.int", 1);
  do_check_true(ps.dirty);
  ps.savePrefFile(null);
  // Overwrite a pref with the same value => not dirty
  userBranch.setIntPref("DirtyTest.new.int", 1);
  do_check_false(ps.dirty);

  userBranch.setCharPref("DirtyTest.new.char", "oop");
  do_check_true(ps.dirty);
  ps.savePrefFile(null);
  // Overwrite a pref with the same value => not dirty
  userBranch.setCharPref("DirtyTest.new.char", "oop");
  do_check_false(ps.dirty);

  // change *type* of a user value -> dirty
  userBranch.setBoolPref("DirtyTest.new.char", false);
  do_check_true(ps.dirty);
  ps.savePrefFile(null);

  // Set a default pref => not dirty (defaults don't go into prefs.js)
  defaultBranch.setBoolPref("DirtyTest.existing.bool", true);
  do_check_false(ps.dirty);
  // Fail to change type of a pref with default value -> not dirty
  do_check_throws(function() {
    userBranch.setCharPref("DirtyTest.existing.bool", "boo"); }, Cr.NS_ERROR_UNEXPECTED);
  do_check_false(ps.dirty);

  // Set user value same as default, not dirty
  userBranch.setBoolPref("DirtyTest.existing.bool", true);
  do_check_false(ps.dirty);
  // User value different from default, dirty
  userBranch.setBoolPref("DirtyTest.existing.bool", false);
  do_check_true(ps.dirty);
  ps.savePrefFile(null);
  // Back to default value, dirty again
  userBranch.setBoolPref("DirtyTest.existing.bool", true);
  do_check_true(ps.dirty);
  ps.savePrefFile(null);
}
