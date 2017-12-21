/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

function run_test() {
  const Cc = Components.classes;
  const Ci = Components.interfaces;
  const Cr = Components.results;
  const BRANCH = "foo.";
  const PREF_NAME = "testPref";
  const FULL_PREF_NAME = BRANCH + PREF_NAME;

  const FLOAT = 9.674;
  const FUDGE = 0.001;

  let ps = Cc["@mozilla.org/preferences-service;1"]
           .getService(Ci.nsIPrefService);
  let prefs = ps.getDefaultBranch(null);

  /* Test with a non-default branch */
  prefs.setCharPref(FULL_PREF_NAME, FLOAT);
  let pb = ps.getBranch(BRANCH);

  let floatPref = pb.getFloatPref(PREF_NAME);
  Assert.ok(FLOAT+FUDGE >= floatPref);
  Assert.ok(FLOAT-FUDGE <= floatPref);
}
