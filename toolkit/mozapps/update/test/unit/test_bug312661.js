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
 * The Original Code is the Application Update Service.
 *
 * The Initial Developer of the Original Code is
 * Robert Strong <robert.bugzilla@gmail.com>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
 *
 * Contributor(s):
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
 * ***** END LICENSE BLOCK *****
 */

/* General Update Check Tests */

const PREF_APP_UPDATE_URL_OVERRIDE = "app.update.url.override";

var gStatusText;
var gCheckFunc;

function run_test() {
  do_test_pending();
  var ioService = AUS_Cc["@mozilla.org/network/io-service;1"]
                    .getService(AUS_Ci.nsIIOService);
  try {
    ioService.manageOfflineStatus = false;
  }
  catch (e) {
  }
  ioService.offline = true;
  startAUS();
  do_timeout(0, "run_test_pt1()");
}

function end_test() {
  do_test_finished();
}

// one update available and the update's property values
function run_test_pt1() {
  gStatusText = null;
  gCheckFunc = check_test_pt1;
  dump("Testing: update,statusText when xml is not cached and network is offline\n");
  gPrefs.setCharPref(PREF_APP_UPDATE_URL_OVERRIDE, "http://localhost:4444/");
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt1() {
  const URI_UPDATES_PROPERTIES = "chrome://mozapps/locale/update/updates.properties";
  const updateBundle = AUS_Cc["@mozilla.org/intl/stringbundle;1"]
                         .getService(AUS_Ci.nsIStringBundleService)
                         .createBundle(URI_UPDATES_PROPERTIES);
  var statusText = updateBundle.GetStringFromName("checker_error-2152398918");
  do_check_eq(statusText, gStatusText);
  end_test();
}

// Update check listener
const updateCheckListener = {
  onProgress: function(request, position, totalSize) {
  },

  onCheckComplete: function(request, updates, updateCount) {
    dump("onCheckComplete request.status = " + request.status + "\n\n");
    // Use a timeout to allow the XHR to complete
    do_timeout(0, "gCheckFunc()");
  },

  onError: function(request, update) {
    gStatusText = update.statusText;
    dump("onError update.statusText = " + update.statusText + "\n\n");
    // Use a timeout to allow the XHR to complete
    do_timeout(0, "gCheckFunc()");
  },

  QueryInterface: function(aIID) {
    if (!aIID.equals(AUS_Ci.nsIUpdateCheckListener) &&
        !aIID.equals(AUS_Ci.nsISupports))
      throw AUS_Cr.NS_ERROR_NO_INTERFACE;
    return this;
  }
};
