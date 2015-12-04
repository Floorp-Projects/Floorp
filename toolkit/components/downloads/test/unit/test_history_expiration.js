/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Test bug 251337 to make sure old downloads are expired and removed.
 * Make sure bug 431346 and bug 431597 don't cause crashes when batching.
 */

/**
 * Returns a PRTime in the past usable to add expirable visits.
 *
 * @note Expiration ignores any visit added in the last 7 days, but it's
 *       better be safe against DST issues, by going back one day more.
 */
function getExpirablePRTime() {
  let dateObj = new Date();
  // Normalize to midnight
  dateObj.setHours(0);
  dateObj.setMinutes(0);
  dateObj.setSeconds(0);
  dateObj.setMilliseconds(0);
  dateObj = new Date(dateObj.getTime() - 8 * 86400000);
  return dateObj.getTime() * 1000;
}

function run_test()
{
  if (oldDownloadManagerDisabled()) {
    return;
  }

  run_next_test();
}

add_task(function* test_execute()
{
  // Like the code, we check to see if nav-history-service exists
  // (i.e MOZ_PLACES is enabled), so that we don't run this test if it doesn't.
  if (!("@mozilla.org/browser/nav-history-service;1" in Cc))
    return;

  let dm = Cc["@mozilla.org/download-manager;1"].
           getService(Ci.nsIDownloadManager);
  let db = dm.DBConnection;

  // Empty any old downloads
  db.executeSimpleSQL("DELETE FROM moz_downloads");

  let stmt = db.createStatement(
    "INSERT INTO moz_downloads (id, source, target, state, guid) " +
    "VALUES (?1, ?2, ?3, ?4, ?5)");

  let iosvc = Cc["@mozilla.org/network/io-service;1"].
              getService(Ci.nsIIOService);
  let theId = 1337;
  let theURI = iosvc.newURI("http://expireme/please", null, null);
  let theGUID = "a1bcD23eF4g5";

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
    
    stmt.bindByIndex(4, theGUID);

    // Add it!
    stmt.execute();
  }
  finally {
    stmt.reset();
    stmt.finalize();
  }

  // Add an expirable visit to this download.
  yield PlacesTestUtils.addVisits({
    uri: theURI, visitDate: getExpirablePRTime(),
    transition: PlacesUtils.history.TRANSITION_DOWNLOAD
  });

  // Get the download manager as history observer and batch expirations
  let histobs = dm.QueryInterface(Ci.nsINavHistoryObserver);
  histobs.onBeginUpdateBatch();

  // Look for the removed download notification
  let obs = Cc["@mozilla.org/observer-service;1"].
            getService(Ci.nsIObserverService);
  const kRemoveTopic = "download-manager-remove-download-guid";
  let testObs = {
    observe: function(aSubject, aTopic, aData) {
      if (aTopic != kRemoveTopic)
        return;

      // Make sure the removed/expired download was the one we added
      let id = aSubject.QueryInterface(Ci.nsISupportsCString);
      do_check_eq(id.data, theGUID);

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
});

