/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Content Preferences (cpref).
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Geoff Lankow <geoff@darktrojan.net>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
