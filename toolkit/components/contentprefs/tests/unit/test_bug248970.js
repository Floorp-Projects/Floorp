/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var _PBSvc = null;
function get_PBSvc() {
  if (_PBSvc)
    return _PBSvc;

  try {
    _PBSvc = Cc["@mozilla.org/privatebrowsing;1"].
             getService(Ci.nsIPrivateBrowsingService);
    return _PBSvc;
  } catch (e) {}
  return null;
}

var _CMSvc = null;
function get_ContentPrefs() {
  if (_CMSvc)
    return _CMSvc;

  return Cc["@mozilla.org/content-pref/service;1"].
         createInstance(Ci.nsIContentPrefService);
}

function run_test() {
  var pb = get_PBSvc();
  if (pb) { // Private Browsing might not be available
    var prefBranch = Cc["@mozilla.org/preferences-service;1"].
                     getService(Ci.nsIPrefBranch);
    prefBranch.setBoolPref("browser.privatebrowsing.keep_current_session", true);

    ContentPrefTest.deleteDatabase();
    var cp = get_ContentPrefs();
    do_check_neq(cp, null, "Retrieving the content prefs service failed");

    try {
      const uri1 = ContentPrefTest.getURI("http://www.example.com/");
      const uri2 = ContentPrefTest.getURI("http://www.anotherexample.com/");
      const pref_name = "browser.content.full-zoom";
      const zoomA = 1.5, zoomA_new = 0.8, zoomB = 1.3;
      // save Zoom-A
      cp.setPref(uri1, pref_name, zoomA);
      // make sure Zoom-A is retrievable
      do_check_eq(cp.getPref(uri1, pref_name), zoomA);
      // enter private browsing mode
      pb.privateBrowsingEnabled = true;
      // make sure Zoom-A is retrievable
      do_check_eq(cp.getPref(uri1, pref_name), zoomA);
      // save Zoom-B
      cp.setPref(uri2, pref_name, zoomB);
      // make sure Zoom-B is retrievable
      do_check_eq(cp.getPref(uri2, pref_name), zoomB);
      // update Zoom-A
      cp.setPref(uri1, pref_name, zoomA_new);
      // make sure Zoom-A has changed
      do_check_eq(cp.getPref(uri1, pref_name), zoomA_new);
      // exit private browsing mode
      pb.privateBrowsingEnabled = false;
      // make sure Zoom-A change has not persisted
      do_check_eq(cp.getPref(uri1, pref_name), zoomA);
      // make sure Zoom-B change has not persisted
      do_check_eq(cp.hasPref(uri2, pref_name), false);
    } catch (e) {
      do_throw("Unexpected exception: " + e);
    }

    prefBranch.clearUserPref("browser.privatebrowsing.keep_current_session");
  }
}
