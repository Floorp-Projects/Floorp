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
 * Portions created by the Initial Developer are Copyright (C) 2009
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

/* General Update Manager Tests */

function run_test() {
  dump("Testing: addition of a successful update to " + FILE_UPDATES_DB +
       " and verification of update properties\n");
  removeUpdateDirsAndFiles();
  var defaults = gPrefs.QueryInterface(AUS_Ci.nsIPrefService)
                   .getDefaultBranch(null);
  defaults.setCharPref("app.update.channel", "bogus_channel");

  var patches = getLocalPatchString(null, null, null, null, null, null,
                                    STATE_FAILED);
  var updates = getLocalUpdateString(patches, "Existing", null, "3.0", "3.0",
                                     "3.0", null, null, null, null, null,
                                     getString("patchApplyFailure"));
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), false);

  patches = getLocalPatchString(null, null, null, null, null, null,
                                STATE_PENDING);
  updates = getLocalUpdateString(patches, "New");
  writeUpdatesToXMLFile(getLocalUpdatesXMLString(updates), true);
  writeStatusFile(STATE_SUCCEEDED);

  startAUS();
  startUpdateManager();

  do_check_eq(gUpdateManager.activeUpdate, null);
  do_check_eq(gUpdateManager.updateCount, 2);

  var update = gUpdateManager.getUpdateAt(0);
  do_check_eq(update.state, STATE_SUCCEEDED);
  do_check_eq(update.name, "New");
  do_check_eq(update.type, "major");
  do_check_eq(update.version, "4.0");
  do_check_eq(update.platformVersion, "4.0");
  do_check_eq(update.extensionVersion, "4.0");
  do_check_eq(update.detailsURL, "http://dummydetails/");
  do_check_eq(update.licenseURL, "http://dummylicense/");
  do_check_eq(update.serviceURL, "http://dummyservice/");
  do_check_eq(update.installDate, "1238441400314");
  do_check_eq(update.statusText, getString("installSuccess"));
  do_check_eq(update.buildID, "20080811053724");
  do_check_true(update.isCompleteUpdate);
  do_check_eq(update.channel, "bogus_channel");

  var patch = update.selectedPatch;
  do_check_eq(patch.type, "complete");
  do_check_eq(patch.URL, "http://localhost:4444/data/empty.mar");
  do_check_eq(patch.hashFunction, "MD5");
  do_check_eq(patch.hashValue, "6232cd43a1c77e30191c53a329a3f99d");
  do_check_eq(patch.size, "775");
  do_check_true(patch.selected);
  do_check_eq(patch.state, STATE_SUCCEEDED);

  update = gUpdateManager.getUpdateAt(1);
  do_check_eq(update.state, STATE_FAILED);
  do_check_eq(update.name, "Existing");
  do_check_eq(update.type, "major");
  do_check_eq(update.version, "3.0");
  do_check_eq(update.platformVersion, "3.0");
  do_check_eq(update.extensionVersion, "3.0");
  do_check_eq(update.detailsURL, "http://dummydetails/");
  do_check_eq(update.licenseURL, "http://dummylicense/");
  do_check_eq(update.serviceURL, "http://dummyservice/");
  do_check_eq(update.installDate, "1238441400314");
  do_check_eq(update.statusText, getString("patchApplyFailure"));
  do_check_eq(update.buildID, "20080811053724");
  do_check_true(update.isCompleteUpdate);
  do_check_eq(update.channel, "bogus_channel");

  patch = update.selectedPatch;
  do_check_eq(patch.type, "complete");
  do_check_eq(patch.URL, "http://localhost:4444/data/empty.mar");
  do_check_eq(patch.hashFunction, "MD5");
  do_check_eq(patch.hashValue, "6232cd43a1c77e30191c53a329a3f99d");
  do_check_eq(patch.size, "775");
  do_check_true(patch.selected);
  do_check_eq(patch.state, STATE_FAILED);
}
