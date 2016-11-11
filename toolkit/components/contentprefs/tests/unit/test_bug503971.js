/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
  var cps = new ContentPrefInstance(null);

  var uri = ContentPrefTest.getURI("http://www.example.com/");

  do_check_thrown(function() { cps.setPref(uri, null, 8); });
  do_check_thrown(function() { cps.hasPref(uri, null); });
  do_check_thrown(function() { cps.getPref(uri, null); });
  do_check_thrown(function() { cps.removePref(uri, null); });
  do_check_thrown(function() { cps.getPrefsByName(null); });
  do_check_thrown(function() { cps.removePrefsByName(null); });

  do_check_thrown(function() { cps.setPref(uri, "", 21); });
  do_check_thrown(function() { cps.hasPref(uri, ""); });
  do_check_thrown(function() { cps.getPref(uri, ""); });
  do_check_thrown(function() { cps.removePref(uri, ""); });
  do_check_thrown(function() { cps.getPrefsByName(""); });
  do_check_thrown(function() { cps.removePrefsByName(""); });
}

function do_check_thrown(aCallback) {
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
