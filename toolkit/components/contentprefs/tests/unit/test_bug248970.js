/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function run_test() {
    let loadContext = { get usePrivateBrowsing() { return gInPrivateBrowsing; } };

    ContentPrefTest.deleteDatabase();
    var cp = new ContentPrefInstance(loadContext);
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
      enterPBMode();
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
      exitPBMode();
      // make sure Zoom-A change has not persisted
      do_check_eq(cp.getPref(uri1, pref_name), zoomA);
      // make sure Zoom-B change has not persisted
      do_check_eq(cp.hasPref(uri2, pref_name), false);
    } catch (e) {
      do_throw("Unexpected exception: " + e);
    }
}
