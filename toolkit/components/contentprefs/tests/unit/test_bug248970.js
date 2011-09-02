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
 * The Original Code is Private Browsing Tests.
 *
 * The Initial Developer of the Original Code is
 * Ehsan Akhgari.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ehsan Akhgari <ehsan.akhgari@gmail.com> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
