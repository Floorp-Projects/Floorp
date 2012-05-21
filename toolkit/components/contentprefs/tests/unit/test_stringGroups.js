/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {

  var cps = Cc["@mozilla.org/content-pref/service;1"].
            getService(Ci.nsIContentPrefService);

  // Make sure disk synchronization checking is turned off by default.
  var statement = cps.DBConnection.createStatement("PRAGMA synchronous");
  statement.executeStep();
  do_check_eq(0, statement.getInt32(0));

  // These are the different types of aGroup arguments we'll test.
  var anObject = {"foo":"bar"};                                // a simple object
  var uri = ContentPrefTest.getURI("http://www.example.com/"); // nsIURI
  var stringURI = "www.example.com";                           // typeof = "string"
  var stringObjectURI = new String("www.example.com");         // typeof = "object"

  {
    // First check that all the methods work or don't work.
    function simple_test_methods(aGroup, shouldThrow) {
      var prefName = "test.pref.0";
      var prefValue = Math.floor(Math.random() * 100);

      if (shouldThrow) {
        do_check_thrown(function () { cps.getPref(aGroup, prefName); });
        do_check_thrown(function () { cps.setPref(aGroup, prefName, prefValue); });
        do_check_thrown(function () { cps.hasPref(aGroup, prefName); });
        do_check_thrown(function () { cps.removePref(aGroup, prefName); });
        do_check_thrown(function () { cps.getPrefs(aGroup); });
      } else {
        do_check_eq(cps.setPref(aGroup, prefName, prefValue), undefined);
        do_check_true(cps.hasPref(aGroup, prefName));
        do_check_eq(cps.getPref(aGroup, prefName), prefValue);
        do_check_eq(cps.removePref(aGroup, prefName), undefined);
        do_check_false(cps.hasPref(aGroup, prefName));
      }
    }

    simple_test_methods(cps, true); // arbitrary nsISupports object, should throw too
    simple_test_methods(anObject, true);
    simple_test_methods(uri, false);
    simple_test_methods(stringURI, false);
    simple_test_methods(stringObjectURI, false);
  }

  {
    // Now we'll check that each argument produces the same result.
    function complex_test_methods(aGroup) {
      var prefName = "test.pref.1";
      var prefValue = Math.floor(Math.random() * 100);

      do_check_eq(cps.setPref(aGroup, prefName, prefValue), undefined);

      do_check_true(cps.hasPref(uri, prefName));
      do_check_true(cps.hasPref(stringURI, prefName));
      do_check_true(cps.hasPref(stringObjectURI, prefName));

      do_check_eq(cps.getPref(uri, prefName), prefValue);
      do_check_eq(cps.getPref(stringURI, prefName), prefValue);
      do_check_eq(cps.getPref(stringObjectURI, prefName), prefValue);

      do_check_eq(cps.removePref(aGroup, prefName), undefined);

      do_check_false(cps.hasPref(uri, prefName));
      do_check_false(cps.hasPref(stringURI, prefName));
      do_check_false(cps.hasPref(stringObjectURI, prefName));
    }

    complex_test_methods(uri);
    complex_test_methods(stringURI);
    complex_test_methods(stringObjectURI);
  }

  {
    // test getPrefs returns the same prefs
    do_check_eq(cps.setPref(stringObjectURI, "test.5", 5), undefined);
    do_check_eq(cps.setPref(stringURI, "test.2", 2), undefined);
    do_check_eq(cps.setPref(uri, "test.1", 1), undefined);

    enumerateAndCheck(cps.getPrefs(uri), 8, ["test.1","test.2","test.5"]);
    enumerateAndCheck(cps.getPrefs(stringURI), 8, ["test.1","test.2","test.5"]);
    enumerateAndCheck(cps.getPrefs(stringObjectURI), 8, ["test.1","test.2","test.5"]);

    do_check_eq(cps.setPref(uri, "test.4", 4), undefined);
    do_check_eq(cps.setPref(stringObjectURI, "test.0", 0), undefined);

    enumerateAndCheck(cps.getPrefs(uri), 12, ["test.0","test.1","test.2","test.4","test.5"]);
    enumerateAndCheck(cps.getPrefs(stringURI), 12, ["test.0","test.1","test.2","test.4","test.5"]);
    enumerateAndCheck(cps.getPrefs(stringObjectURI), 12, ["test.0","test.1","test.2","test.4","test.5"]);

    do_check_eq(cps.setPref(stringURI, "test.3", 3), undefined);

    enumerateAndCheck(cps.getPrefs(uri), 15, ["test.0","test.1","test.2","test.3","test.4","test.5"]);
    enumerateAndCheck(cps.getPrefs(stringURI), 15, ["test.0","test.1","test.2","test.3","test.4","test.5"]);
    enumerateAndCheck(cps.getPrefs(stringObjectURI), 15, ["test.0","test.1","test.2","test.3","test.4","test.5"]);
  }
}

function do_check_thrown (aCallback) {
  var exThrown = false;
  try {
    aCallback();
    do_throw("NS_ERROR_ILLEGAL_VALUE should have been thrown here");
  } catch (e) {
    do_check_eq(e.result, Cr.NS_ERROR_ILLEGAL_VALUE);
    exThrown = true;
  }
  do_check_true(exThrown);
}

function enumerateAndCheck(prefs, expectedSum, expectedNames) {
  var enumerator = prefs.enumerator;
  var sum = 0;
  while (enumerator.hasMoreElements()) {
    var property = enumerator.getNext().QueryInterface(Components.interfaces.nsIProperty);
    sum += parseInt(property.value);

    // remove the pref name from the list of expected names
    var index = expectedNames.indexOf(property.name);
    do_check_true(index >= 0);
    expectedNames.splice(index, 1);
  }
  do_check_eq(sum, expectedSum);
  // check all pref names have been removed from the array
  do_check_eq(expectedNames.length, 0);
}
