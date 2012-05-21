/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var _PBSvc = null;
function get_PBSvc() {
  if (_PBSvc)
    return _PBSvc;

  try {
    _PBSvc = Components.classes["@mozilla.org/privatebrowsing;1"].
             getService(Components.interfaces.nsIPrivateBrowsingService);
    return _PBSvc;
  } catch (e) {}
  return null;
}

var _FHSvc = null;
function get_FormHistory() {
  if (_FHSvc)
    return _FHSvc;

  return _FHSvc = Components.classes["@mozilla.org/satchel/form-history;1"].
                  getService(Components.interfaces.nsIFormHistory2);
}

function run_test() {
  var pb = get_PBSvc();
  if (pb) { // Private Browsing might not be available
    var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefBranch);
    prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);

    var fh = get_FormHistory();
    do_check_neq(fh, null);

    // remove possible Pair-A and Pair-B left-overs from previous tests
    fh.removeEntriesForName("pair-A");
    fh.removeEntriesForName("pair-B");

    // save Pair-A
    fh.addEntry("pair-A", "value-A");
    // ensure that Pair-A exists
    do_check_true(fh.entryExists("pair-A", "value-A"));
    // enter private browsing mode
    pb.privateBrowsingEnabled = true;
    // make sure that Pair-A exists
    do_check_true(fh.entryExists("pair-A", "value-A"));
    // attempt to save Pair-B
    fh.addEntry("pair-B", "value-B");
    // make sure that Pair-B does not exist
    do_check_false(fh.entryExists("pair-B", "value-B"));
    // exit private browsing mode
    pb.privateBrowsingEnabled = false;
    // ensure that Pair-A exists
    do_check_true(fh.entryExists("pair-A", "value-A"));
    // make sure that Pair-B does not exist
    do_check_false(fh.entryExists("pair-B", "value-B"));

    prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
  }
}
