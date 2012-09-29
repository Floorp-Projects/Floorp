/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

function run_test() {
  const Cc = Components.classes;
  const Ci = Components.interfaces;
  const Cr = Components.results;
  const PREF_NAME = "testPref";

  var ps = Cc["@mozilla.org/preferences-service;1"]
           .getService(Ci.nsIPrefService);
  var prefs = ps.getDefaultBranch(null);
  var userprefs = ps.getBranch(null);

  /* First, test to make sure we can parse a float from a string properly. */
  prefs.setCharPref(PREF_NAME, "9.674");
  prefs.lockPref(PREF_NAME);
  var myFloat = 9.674;
  var fudge = 0.001;
  var floatPref = userprefs.getFloatPref(PREF_NAME);
  do_check_true(myFloat+fudge >= floatPref);
  do_check_true(myFloat-fudge <= floatPref);

  /* Now test some failure conditions. */
  prefs.unlockPref(PREF_NAME);
  prefs.setCharPref(PREF_NAME, "");
  prefs.lockPref(PREF_NAME);
  do_check_throws(function() {
    userprefs.getFloatPref(PREF_NAME);
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  prefs.unlockPref(PREF_NAME);
  prefs.setCharPref(PREF_NAME, "18.0a1");
  prefs.lockPref(PREF_NAME);

  do_check_throws(function() {
    userprefs.getFloatPref(PREF_NAME);
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  prefs.unlockPref(PREF_NAME);
  prefs.setCharPref(PREF_NAME, "09.25.2012");
  prefs.lockPref(PREF_NAME);

  do_check_throws(function() {
    userprefs.getFloatPref(PREF_NAME);
  }, Cr.NS_ERROR_ILLEGAL_VALUE);

  prefs.unlockPref(PREF_NAME);
  prefs.setCharPref(PREF_NAME, "aString");
  prefs.lockPref(PREF_NAME);

  do_check_throws(function() {
    userprefs.getFloatPref(PREF_NAME);
  }, Cr.NS_ERROR_ILLEGAL_VALUE);
}
