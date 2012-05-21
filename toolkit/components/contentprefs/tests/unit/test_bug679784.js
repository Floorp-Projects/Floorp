/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var prefObserver = {
    setCalledNum: 0,
    onContentPrefSet: function(aGroup, aName, aValue) {
        this.setCalledNum++;
    },
    removedCalledNum: 0,
    onContentPrefRemoved: function(aGroup, aName) {
        this.removedCalledNum++;
    }
};

function run_test() {
  var pbs;
  try {
    pbs = Cc["@mozilla.org/privatebrowsing;1"].getService(Ci.nsIPrivateBrowsingService);
  } catch(e) {
    // Private Browsing might not be available
    return;
  }

  Cc["@mozilla.org/preferences-service;1"].
    getService(Ci.nsIPrefBranch).
      setBoolPref("browser.privatebrowsing.keep_current_session", true);

  var cps = Cc["@mozilla.org/content-pref/service;1"].getService(Ci.nsIContentPrefService);
  cps.removeGroupedPrefs();

  var uri = ContentPrefTest.getURI("http://www.example.com/");
  var group = cps.grouper.group(uri);

  // first, set a pref in normal mode
  cps.setPref(uri, "value", "foo");
  cps.setPref(null, "value-global", "foo-global");

  var num;
  cps.addObserver("value", prefObserver);
  cps.addObserver("value-global", prefObserver);

  pbs.privateBrowsingEnabled = true;

  // test setPref
  num = prefObserver.setCalledNum;
  cps.setPref(uri, "value", "foo-private-browsing");
  do_check_eq(cps.hasPref(uri, "value"), true);
  do_check_eq(cps.getPref(uri, "value"), "foo-private-browsing");
  do_check_eq(prefObserver.setCalledNum, num + 1);

  num = prefObserver.setCalledNum;
  cps.setPref(null, "value-global", "foo-private-browsing-global");
  do_check_eq(cps.hasPref(null, "value-global"), true);
  do_check_eq(cps.getPref(null, "value-global"), "foo-private-browsing-global");
  do_check_eq(prefObserver.setCalledNum, num + 1);

  // test removePref
  num = prefObserver.removedCalledNum;
  cps.removePref(uri, "value");
  do_check_eq(cps.hasPref(uri, "value"), true);
  // fallback to non private mode value
  do_check_eq(cps.getPref(uri, "value"), "foo"); 
  do_check_eq(prefObserver.removedCalledNum, num + 1);

  num = prefObserver.removedCalledNum;
  cps.removePref(null, "value-global");
  do_check_eq(cps.hasPref(null, "value-global"), true);
  // fallback to non private mode value
  do_check_eq(cps.getPref(null, "value-global"), "foo-global") ;
  do_check_eq(prefObserver.removedCalledNum, num + 1);

  // test removeGroupedPrefs
  cps.setPref(uri, "value", "foo-private-browsing");
  cps.removeGroupedPrefs();
  do_check_eq(cps.hasPref(uri, "value"), false);
  do_check_eq(cps.getPref(uri, "value"), undefined);

  cps.setPref(null, "value-global", "foo-private-browsing-global");
  cps.removeGroupedPrefs();
  do_check_eq(cps.hasPref(null, "value-global"), true);
  do_check_eq(cps.getPref(null, "value-global"), "foo-private-browsing-global");

  // test removePrefsByName
  num = prefObserver.removedCalledNum;
  cps.setPref(uri, "value", "foo-private-browsing");
  cps.removePrefsByName("value");
  do_check_eq(cps.hasPref(uri, "value"), false);
  do_check_eq(cps.getPref(uri, "value"), undefined);
  do_check_true(prefObserver.removedCalledNum > num);

  num = prefObserver.removedCalledNum;
  cps.setPref(null, "value-global", "foo-private-browsing");
  cps.removePrefsByName("value-global");
  do_check_eq(cps.hasPref(null, "value-global"), false);
  do_check_eq(cps.getPref(null, "value-global"), undefined);
  do_check_true(prefObserver.removedCalledNum > num);

  // test getPrefs
  cps.setPref(uri, "value", "foo-private-browsing");
  do_check_eq(cps.getPrefs(uri).getProperty("value"), "foo-private-browsing");

  cps.setPref(null, "value-global", "foo-private-browsing-global");
  do_check_eq(cps.getPrefs(null).getProperty("value-global"), "foo-private-browsing-global");

  // test getPrefsByName
  do_check_eq(cps.getPrefsByName("value").getProperty(group), "foo-private-browsing");
  do_check_eq(cps.getPrefsByName("value-global").getProperty(null), "foo-private-browsing-global");

  cps.removeObserver("value", prefObserver);
  cps.removeObserver("value-global", prefObserver);
}
