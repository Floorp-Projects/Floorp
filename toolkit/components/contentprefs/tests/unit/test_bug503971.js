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

  var uri = ContentPrefTest.getURI("http://www.example.com/");
  
  do_check_thrown(function () { cps.setPref(uri, null, 8); });
  do_check_thrown(function () { cps.hasPref(uri, null); });
  do_check_thrown(function () { cps.getPref(uri, null); });
  do_check_thrown(function () { cps.removePref(uri, null); });
  do_check_thrown(function () { cps.getPrefsByName(null); });
  do_check_thrown(function () { cps.removePrefsByName(null); });

  do_check_thrown(function () { cps.setPref(uri, "", 21); });
  do_check_thrown(function () { cps.hasPref(uri, ""); });
  do_check_thrown(function () { cps.getPref(uri, ""); });
  do_check_thrown(function () { cps.removePref(uri, ""); });
  do_check_thrown(function () { cps.getPrefsByName(""); });
  do_check_thrown(function () { cps.removePrefsByName(""); });
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
