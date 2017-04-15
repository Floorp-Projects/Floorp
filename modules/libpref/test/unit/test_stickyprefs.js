/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

Components.utils.import("resource://gre/modules/XPCOMUtils.jsm");
Components.utils.import("resource://gre/modules/Services.jsm");

const ps = Services.prefs;

// Once we fetch the profile directory the xpcshell test harness will send
// a profile-before-change notification at shutdown. This causes the prefs
// service to flush the prefs file - and the prefs file it uses ends up being
// testPrefSticky*.js in the test dir. This upsets things in confusing ways :)
// We avoid this by ensuring our "temp" prefs.js is the current prefs file.
do_get_profile();
do_register_cleanup(saveAndReload);

// A little helper to reset the service and load some pref files
function resetAndLoad(filenames) {
  ps.resetPrefs();
  for (let filename of filenames) {
    ps.readUserPrefs(do_get_file(filename));
  }
}

// A little helper that saves the current state to a file in the profile
// dir, then resets the service and re-reads the file it just saved.
// Used to test what gets actually written - things the pref service decided
// not to write don't exist at all after this call.
function saveAndReload() {
  let file = do_get_profile();
  file.append("prefs.js");
  ps.savePrefFile(file);

  // Now reset the pref service and re-read what we saved.
  ps.resetPrefs();
  ps.readUserPrefs(file);
}

function run_test() {
  run_next_test();
}

// A sticky pref should not be written if the value is unchanged.
add_test(function notWrittenWhenUnchanged() {
  resetAndLoad(["data/testPrefSticky.js"]);
  Assert.strictEqual(ps.getBoolPref("testPref.unsticky.bool"), true);
  Assert.strictEqual(ps.getBoolPref("testPref.sticky.bool"), false);

  // write prefs - but we haven't changed the sticky one, so it shouldn't be written.
  saveAndReload();
  // sticky should not have been written to the new file.
  try {
    ps.getBoolPref("testPref.sticky.bool");
    Assert.ok(false, "expected failure reading this pref");
  } catch (ex) {
    Assert.ok(ex, "exception reading regular pref");
  }
  run_next_test();
});

// Loading a sticky_pref then a user_pref for the same pref means it should
// always be written.
add_test(function writtenOnceLoadedWithoutChange() {
  // Load the same pref file *as well as* a pref file that has a user_pref for
  // our sticky with the default value. It should be re-written without us
  // touching it.
  resetAndLoad(["data/testPrefSticky.js", "data/testPrefStickyUser.js"]);
  // reset and re-read what we just wrote - it should be written.
  saveAndReload();
  Assert.strictEqual(ps.getBoolPref("testPref.sticky.bool"), false,
                     "user_pref was written with default value");
  run_next_test();
});

// If a sticky pref is explicicitly changed, even to the default, it is written.
add_test(function writtenOnceLoadedWithChangeNonDefault() {
  // Load the same pref file *as well as* a pref file that has a user_pref for
  // our sticky - then change the pref. It should be written.
  resetAndLoad(["data/testPrefSticky.js", "data/testPrefStickyUser.js"]);
  // Set a new val and check we wrote it.
  ps.setBoolPref("testPref.sticky.bool", false);
  saveAndReload();
  Assert.strictEqual(ps.getBoolPref("testPref.sticky.bool"), false,
                     "user_pref was written with custom value");
  run_next_test();
});

// If a sticky pref is changed to the non-default value, it is written.
add_test(function writtenOnceLoadedWithChangeNonDefault() {
  // Load the same pref file *as well as* a pref file that has a user_pref for
  // our sticky - then change the pref. It should be written.
  resetAndLoad(["data/testPrefSticky.js", "data/testPrefStickyUser.js"]);
  // Set a new val and check we wrote it.
  ps.setBoolPref("testPref.sticky.bool", true);
  saveAndReload();
  Assert.strictEqual(ps.getBoolPref("testPref.sticky.bool"), true,
                     "user_pref was written with custom value");
  run_next_test();
});

// Test that prefHasUserValue always returns true whenever there is a sticky
// value, even when that value matches the default. This is mainly for
// about:config semantics - prefs with a sticky value always remain bold and
// always offer "reset" (which fully resets and drops the sticky value as if
// the pref had never changed.)
add_test(function hasUserValue() {
  // sticky pref without user value.
  resetAndLoad(["data/testPrefSticky.js"]);
  Assert.strictEqual(ps.getBoolPref("testPref.sticky.bool"), false);
  Assert.ok(!ps.prefHasUserValue("testPref.sticky.bool"),
            "should not initially reflect a user value");

  ps.setBoolPref("testPref.sticky.bool", false);
  Assert.ok(ps.prefHasUserValue("testPref.sticky.bool"),
            "should reflect a user value after set to default");

  ps.setBoolPref("testPref.sticky.bool", true);
  Assert.ok(ps.prefHasUserValue("testPref.sticky.bool"),
            "should reflect a user value after change to non-default");

  ps.clearUserPref("testPref.sticky.bool");
  Assert.ok(!ps.prefHasUserValue("testPref.sticky.bool"),
            "should reset to no user value");
  ps.setBoolPref("testPref.sticky.bool", false, "expected default");

  // And make sure the pref immediately reflects a user value after load.
  resetAndLoad(["data/testPrefSticky.js", "data/testPrefStickyUser.js"]);
  Assert.strictEqual(ps.getBoolPref("testPref.sticky.bool"), false);
  Assert.ok(ps.prefHasUserValue("testPref.sticky.bool"),
            "should have a user value when loaded value is the default");
  run_next_test();
});

// Test that clearUserPref removes the "sticky" value.
add_test(function clearUserPref() {
  // load things such that we have a sticky value which is the same as the
  // default.
  resetAndLoad(["data/testPrefSticky.js", "data/testPrefStickyUser.js"]);
  ps.clearUserPref("testPref.sticky.bool");

  // Once we save prefs the sticky pref should no longer be written.
  saveAndReload();
  try {
    ps.getBoolPref("testPref.sticky.bool");
    Assert.ok(false, "expected failure reading this pref");
  } catch (ex) {
    Assert.ok(ex, "pref doesn't have a sticky value");
  }
  run_next_test();
});

// Test that a pref observer gets a notification fired when a sticky pref
// has it's value changed to the same value as the default. The reason for
// this behaviour is that later we might have other code that cares about a
// pref being sticky (IOW, we notify due to the "state" of the pref changing
// even if the value has not)
add_test(function observerFires() {
  // load things so there's no sticky value.
  resetAndLoad(["data/testPrefSticky.js"]);

  function observe(subject, topic, data) {
    Assert.equal(data, "testPref.sticky.bool");
    ps.removeObserver("testPref.sticky.bool", observe);
    run_next_test();
  }
  ps.addObserver("testPref.sticky.bool", observe);

  ps.setBoolPref("testPref.sticky.bool", ps.getBoolPref("testPref.sticky.bool"));
  // and the observer will fire triggering the next text.
});
