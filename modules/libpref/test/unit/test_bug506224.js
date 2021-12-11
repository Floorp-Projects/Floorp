/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

function run_test() {
  const PREF_NAME = "testPref";

  var prefs = Services.prefs.getDefaultBranch(null);
  var userprefs = Services.prefs.getBranch(null);

  prefs.setCharPref(PREF_NAME, "test0");
  prefs.lockPref(PREF_NAME);
  Assert.equal("test0", userprefs.getCharPref(PREF_NAME));
  Assert.equal(false, userprefs.prefHasUserValue(PREF_NAME));

  var file = do_get_profile();
  file.append("prefs.js");
  Services.prefs.savePrefFile(file);

  prefs.unlockPref(PREF_NAME);
  prefs.setCharPref(PREF_NAME, "test1");
  Services.prefs.readUserPrefsFromFile(file);

  Assert.equal("test1", userprefs.getCharPref(PREF_NAME));
  Assert.equal(false, userprefs.prefHasUserValue(PREF_NAME));
}
