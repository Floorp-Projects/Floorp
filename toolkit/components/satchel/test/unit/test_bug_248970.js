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
