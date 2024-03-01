/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/  */

// Regression test for bug 345529 - crash removing an observer during an
// nsPref:changed notification.
function run_test() {
  const PREF_NAME = "testPref";

  var observer = {
    QueryInterface: ChromeUtils.generateQI(["nsIObserver"]),

    observe: function observe() {
      Services.prefs.removeObserver(PREF_NAME, observer);
    },
  };
  Services.prefs.addObserver(PREF_NAME, observer);

  Services.prefs.setCharPref(PREF_NAME, "test0");
  // This second call isn't needed on a clean profile: it makes sure
  // the observer gets called even if the pref already had the value
  // "test0" before this test.
  Services.prefs.setCharPref(PREF_NAME, "test1");

  Assert.ok(true);
}
