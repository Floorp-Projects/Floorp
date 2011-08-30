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
 * The Original Code is Download Manager UI Test Code.
 *
 * The Initial Developer of the Original Code is
 * Edward Lee <edward.lee@engineering.uiuc.edu>.
 * Portions created by the Initial Developer are Copyright (C) 2008
 * the Initial Developer. All Rights Reserved.
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
 * ***** END LICENSE BLOCK ***** */

/**
 * Test bug 251337 to make sure old downloads are expired and removed.
 * Make sure bug 431346 and bug 431597 don't cause crashes when batching.
 */

function run_test()
{
  // Like the code, we check to see if nav-history-service exists
  // (i.e MOZ_PLACES is enabled), so that we don't run this test if it doesn't.
  if (!("@mozilla.org/browser/nav-history-service;1" in Cc))
    return;

  // Ensure places is enabled.
  Services.prefs.setBoolPref("places.history.enabled", true);

  let dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  let db = dm.DBConnection;

  // Empty any old downloads
  db.executeSimpleSQL("DELETE FROM moz_downloads");

  let stmt = db.createStatement(
    "INSERT INTO moz_downloads (id, source, target, state) " +
    "VALUES (?1, ?2, ?3, ?4)");

  let iosvc = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
  let theId = 1337;
  let theURI = iosvc.newURI("http://expireme/please", null, null);

  try {
    // Add a download from the URI
    stmt.bindByIndex(0, theId);
    stmt.bindByIndex(1, theURI.spec);

    // Give a dummy file name
    let file = Cc["@mozilla.org/file/directory_service;1"].
               getService(Ci.nsIProperties).get("TmpD", Ci.nsIFile);
    file.append("expireTest");
    stmt.bindByIndex(2, Cc["@mozilla.org/network/io-service;1"].
      getService(Ci.nsIIOService).newFileURI(file).spec);

    // Give it some state
    stmt.bindByIndex(3, dm.DOWNLOAD_FINISHED);

    // Add it!
    stmt.execute();
  }
  finally {
    stmt.reset();
    stmt.finalize();
  }

  let histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
                getService(Ci.nsINavHistoryService);
  // Add the download to places
  // Add the visit in the past to circumvent possible VM timing bugs
  // Go back by 8 days, since expiration ignores history in the last 7 days.
  let expirableTime = Date.now() - 8 * 24 * 60 * 60 * 1000;
  histsvc.addVisit(theURI, expirableTime * 1000, null,
                   histsvc.TRANSITION_DOWNLOAD, false, 0);

  // Get the download manager as history observer and batch expirations
  let histobs = dm.QueryInterface(Ci.nsINavHistoryObserver);
  histobs.onBeginUpdateBatch();

  // Look for the removed download notification
  let obs = Cc["@mozilla.org/observer-service;1"].
            getService(Ci.nsIObserverService);
  const kRemoveTopic = "download-manager-remove-download";
  let testObs = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic != kRemoveTopic)
        return;

      // Make sure the removed/expired download was the one we added
      let id = aSubject.QueryInterface(Ci.nsISupportsPRUint32);
      do_check_eq(id.data, theId);

      // We're done!
      histobs.onEndUpdateBatch();
      obs.removeObserver(testObs, kRemoveTopic);
      do_test_finished();
    }
  };
  obs.addObserver(testObs, kRemoveTopic, false);

  // Set expiration stuff to 0 to make the download expire
  Services.prefs.setIntPref("places.history.expiration.max_pages", 0);

  // Force a history expiration.
  let expire = Cc["@mozilla.org/places/expiration;1"].getService(Ci.nsIObserver);
  expire.observe(null, "places-debug-start-expiration", -1);

  // Expiration happens on a timeout, about 3.5s after we set the pref
  do_test_pending();
}
