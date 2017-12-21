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

  //**************************************************************************//
  // prefs are not dirty after a write
  ps.savePrefFile(null);
  Assert.ok(!ps.dirty);

  // set a new a user value, we should become dirty
  userBranch.setBoolPref("DirtyTest.new.bool", true);
  Assert.ok(ps.dirty);
  ps.savePrefFile(null);
  // Overwrite a pref with the same value => not dirty
  userBranch.setBoolPref("DirtyTest.new.bool", true);
  Assert.ok(!ps.dirty);

  // Repeat for the other two types
  userBranch.setIntPref("DirtyTest.new.int", 1);
  Assert.ok(ps.dirty);
  ps.savePrefFile(null);
  // Overwrite a pref with the same value => not dirty
  userBranch.setIntPref("DirtyTest.new.int", 1);
  Assert.ok(!ps.dirty);

  userBranch.setCharPref("DirtyTest.new.char", "oop");
  Assert.ok(ps.dirty);
  ps.savePrefFile(null);
  // Overwrite a pref with the same value => not dirty
  userBranch.setCharPref("DirtyTest.new.char", "oop");
  Assert.ok(!ps.dirty);

  // change *type* of a user value -> dirty
  userBranch.setBoolPref("DirtyTest.new.char", false);
  Assert.ok(ps.dirty);
  ps.savePrefFile(null);

  // Set a default pref => not dirty (defaults don't go into prefs.js)
  defaultBranch.setBoolPref("DirtyTest.existing.bool", true);
  Assert.ok(!ps.dirty);
  // Fail to change type of a pref with default value -> not dirty
  do_check_throws(function() {
    userBranch.setCharPref("DirtyTest.existing.bool", "boo"); }, Cr.NS_ERROR_UNEXPECTED);
  Assert.ok(!ps.dirty);

  // Set user value same as default, not dirty
  userBranch.setBoolPref("DirtyTest.existing.bool", true);
  Assert.ok(!ps.dirty);
  // User value different from default, dirty
  userBranch.setBoolPref("DirtyTest.existing.bool", false);
  Assert.ok(ps.dirty);
  ps.savePrefFile(null);
  // Back to default value, dirty again
  userBranch.setBoolPref("DirtyTest.existing.bool", true);
  Assert.ok(ps.dirty);
  ps.savePrefFile(null);
}
