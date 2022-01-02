/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

// This file tests the `locked` attribute in default pref files.

const ps = Services.prefs;

add_test(function notChangedFromAPI() {
  ps.resetPrefs();
  ps.readDefaultPrefsFromFile(do_get_file("data/testPrefLocked.js"));
  Assert.strictEqual(ps.getIntPref("testPref.unlocked.int"), 333);
  Assert.strictEqual(ps.getIntPref("testPref.locked.int"), 444);

  // Unlocked pref: can set the user value, which is used upon reading.
  ps.setIntPref("testPref.unlocked.int", 334);
  Assert.ok(ps.prefHasUserValue("testPref.unlocked.int"), "has a user value");
  Assert.strictEqual(ps.getIntPref("testPref.unlocked.int"), 334);

  // Locked pref: can set the user value, but the default value is used upon
  // reading.
  ps.setIntPref("testPref.locked.int", 445);
  Assert.ok(ps.prefHasUserValue("testPref.locked.int"), "has a user value");
  Assert.strictEqual(ps.getIntPref("testPref.locked.int"), 444);

  // After unlocking, the user value is used.
  ps.unlockPref("testPref.locked.int");
  Assert.ok(ps.prefHasUserValue("testPref.locked.int"), "has a user value");
  Assert.strictEqual(ps.getIntPref("testPref.locked.int"), 445);

  run_next_test();
});

add_test(function notChangedFromUserPrefs() {
  ps.resetPrefs();
  ps.readDefaultPrefsFromFile(do_get_file("data/testPrefLocked.js"));
  ps.readUserPrefsFromFile(do_get_file("data/testPrefLockedUser.js"));

  Assert.strictEqual(ps.getIntPref("testPref.unlocked.int"), 333);
  Assert.strictEqual(ps.getIntPref("testPref.locked.int"), 444);

  run_next_test();
});
