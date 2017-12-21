/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

function run_test() {
  const Cc = Components.classes;
  const Ci = Components.interfaces;
  const PREF_NAME = "testPref";

  var ps = Cc["@mozilla.org/preferences-service;1"]
           .getService(Ci.nsIPrefService);
  var prefs = ps.getDefaultBranch(null);
  var userprefs = ps.getBranch(null);

  prefs.setCharPref(PREF_NAME, "test0");
  prefs.lockPref(PREF_NAME);
  Assert.equal("test0", userprefs.getCharPref(PREF_NAME));
  Assert.equal(false, userprefs.prefHasUserValue(PREF_NAME));

  var file = do_get_profile();
  file.append("prefs.js");
  ps.savePrefFile(file);

  prefs.unlockPref(PREF_NAME);
  prefs.setCharPref(PREF_NAME, "test1");
  ps.readUserPrefsFromFile(file);

  Assert.equal("test1", userprefs.getCharPref(PREF_NAME));
  Assert.equal(false, userprefs.prefHasUserValue(PREF_NAME));
}
