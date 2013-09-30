/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

AUS_Cu.import("resource://gre/modules/FileUtils.jsm");

const TEST_ID = "bug794211";
const KEY_UPDATE_ARCHIVE_DIR = "UpdArchD"

let gActiveUpdate = null;

function FakeDirProvider() {}
FakeDirProvider.prototype = {
  classID: Components.ID("{f30b43a7-2bfa-4e5f-8c4f-abc7dd4ac486}"),
  QueryInterface: XPCOMUtils.generateQI([AUS_Ci.nsIDirectoryServiceProvider]),

  getFile: function(prop, persistent) {
    if (prop == KEY_UPDATE_ARCHIVE_DIR) {
      if (gActiveUpdate) {
        gActiveUpdate.errorCode = AUS_Cr.NS_ERROR_FILE_TOO_BIG;
      }
    }
    return null;
  }
};

function run_test() {
  do_test_pending();
  DEBUG_AUS_TEST = true;

  // adjustGeneralPaths registers a cleanup function that calls end_test.
  adjustGeneralPaths();

  removeUpdateDirsAndFiles();
  setUpdateURLOverride();
  overrideXHR(xhr_pt1);
  standardInit();

  logTestInfo("testing that error codes set from a directory provider propagate" +
              "up to AUS.downloadUpdate() correctly");

  gDirProvider = new FakeDirProvider();
  gOldProvider = AUS_Cc["@mozilla.org/browser/directory-provider;1"]
                       .createInstance(AUS_Ci.nsIDirectoryServiceProvider);

  gDirService = AUS_Cc["@mozilla.org/file/directory_service;1"]
                       .getService(AUS_Ci.nsIProperties);

  gDirService.unregisterProvider(gOldProvider);
  gDirService.registerProvider(gDirProvider);

  Services.prefs.setBoolPref(PREF_APP_UPDATE_SILENT, true);
  do_execute_soon(run_test_pt1);
}

function xhr_pt1() {
  gXHR.status = 200;
  gXHR.responseText = gResponseBody;
  try {
    var parser = AUS_Cc["@mozilla.org/xmlextras/domparser;1"].
                 createInstance(AUS_Ci.nsIDOMParser);
    gXHR.responseXML = parser.parseFromString(gResponseBody, "application/xml");
  }
  catch(e) {
    gXHR.responseXML = null;
  }
  var e = { target: gXHR };
  gXHR.onload(e);
}

function run_test_pt1() {
  gUpdates = null;
  gUpdateCount = null;
  gCheckFunc = check_test_pt1;


  let patches = getRemotePatchString();
  let updates = getRemoteUpdateString(patches);
  gResponseBody = getRemoteUpdatesXMLString(updates);
  gUpdateChecker.checkForUpdates(updateCheckListener, true);
}

function check_test_pt1() {
  do_check_eq(gUpdateCount, 1);

  gActiveUpdate = gUpdates[0];
  do_check_neq(gActiveUpdate, null);

  let state = gAUS.downloadUpdate(gActiveUpdate, true);
  do_check_eq(state, "null");
  do_check_eq(gActiveUpdate.errorCode >>> 0 , AUS_Cr.NS_ERROR_FILE_TOO_BIG);
  do_test_finished();
}

function end_test() {
  gDirService.unregisterProvider(gDirProvider);
  gDirService.registerProvider(gOldProvider);
  gActiveUpdate = null;
  gDirService = null;
  gDirProvider = null;

  cleanUp();
}
