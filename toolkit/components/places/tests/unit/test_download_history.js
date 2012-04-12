/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the nsIDownloadHistory interface.
 */

////////////////////////////////////////////////////////////////////////////////
/// Globals

XPCOMUtils.defineLazyServiceGetter(this, "gDownloadHistory",
                                   "@mozilla.org/browser/download-history;1",
                                   "nsIDownloadHistory");

XPCOMUtils.defineLazyServiceGetter(this, "gHistory",
                                   "@mozilla.org/browser/history;1",
                                   "mozIAsyncHistory");

const DOWNLOAD_URI = NetUtil.newURI("http://www.example.com/");
const REFERRER_URI = NetUtil.newURI("http://www.example.org/");
const PRIVATE_URI = NetUtil.newURI("http://www.example.net/");

/**
 * Waits for the first visit notification to be received.
 *
 * @param aCallback
 *        This function is called with the same arguments of onVisit.
 */
function waitForOnVisit(aCallback) {
  let historyObserver = {
    __proto__: NavHistoryObserver.prototype,
    onVisit: function HO_onVisit() {
      PlacesUtils.history.removeObserver(this);
      aCallback.apply(null, arguments);
    }
  };
  PlacesUtils.history.addObserver(historyObserver, false);
}

/**
 * Checks to see that a URI is in the database.
 *
 * @param aURI
 *        The URI to check.
 * @param aExpected
 *        Boolean result expected from the db lookup.
 */
function uri_in_db(aURI, aExpected)
{
  let options = PlacesUtils.history.getNewQueryOptions();
  options.maxResults = 1;
  options.includeHidden = true;

  let query = PlacesUtils.history.getNewQuery();
  query.uri = aURI;

  let root = PlacesUtils.history.executeQuery(query, options).root;
  root.containerOpen = true;

  do_check_eq(root.childCount, aExpected ? 1 : 0);

  // Close the container explicitly to free resources up earlier.
  root.containerOpen = false;
}

////////////////////////////////////////////////////////////////////////////////
/// Tests

function run_test()
{
  run_next_test();
}

add_test(function test_dh_is_from_places()
{
  // Test that this nsIDownloadHistory is the one places implements.
  do_check_true(gDownloadHistory instanceof Ci.mozIAsyncHistory);

  waitForClearHistory(run_next_test);
});

add_test(function test_dh_addDownload()
{
  waitForOnVisit(function DHAD_onVisit(aURI) {
    do_check_true(aURI.equals(DOWNLOAD_URI));

    // Verify that the URI is already available in results at this time.
    uri_in_db(DOWNLOAD_URI, true);

    waitForClearHistory(run_next_test);
  });

  gDownloadHistory.addDownload(DOWNLOAD_URI, null, Date.now() * 1000);
});

add_test(function test_dh_addDownload_referrer()
{
  waitForOnVisit(function DHAD_prepareReferrer(aURI, aVisitID) {
    do_check_true(aURI.equals(REFERRER_URI));
    let referrerVisitId = aVisitID;

    waitForOnVisit(function DHAD_onVisit(aURI, aVisitID, aTime, aSessionID,
                                              aReferringID) {
      do_check_true(aURI.equals(DOWNLOAD_URI));
      do_check_eq(aReferringID, referrerVisitId);

      // Verify that the URI is already available in results at this time.
      uri_in_db(DOWNLOAD_URI, true);

      waitForClearHistory(run_next_test);
    });

    gDownloadHistory.addDownload(DOWNLOAD_URI, REFERRER_URI, Date.now() * 1000);
  });

  // Note that we don't pass the optional callback argument here because we must
  // ensure that we receive the onVisit notification before we call addDownload.
  gHistory.updatePlaces({
    uri: REFERRER_URI,
    visits: [{
      transitionType: Ci.nsINavHistoryService.TRANSITION_TYPED,
      visitDate: Date.now() * 1000
    }]
  });
});

add_test(function test_dh_addDownload_privateBrowsing()
{
  if (!("@mozilla.org/privatebrowsing;1" in Cc)) {
    do_log_info("Private Browsing service is not available, bail out.");
    run_next_test();
    return;
  }

  waitForOnVisit(function DHAD_onVisit(aURI) {
    // We should only receive the notification for the non-private URI.  This
    // test is based on the assumption that visit notifications are received
    // in the same order of the addDownload calls, which is currently true
    // because database access is serialized on the same worker thread.
    do_check_true(aURI.equals(DOWNLOAD_URI));

    uri_in_db(DOWNLOAD_URI, true);
    uri_in_db(PRIVATE_URI, false);

    waitForClearHistory(run_next_test);
  });

  let pb = Cc["@mozilla.org/privatebrowsing;1"]
           .getService(Ci.nsIPrivateBrowsingService);
  Services.prefs.setBoolPref("browser.privatebrowsing.keep_current_session",
                             true);
  pb.privateBrowsingEnabled = true;
  gDownloadHistory.addDownload(PRIVATE_URI, REFERRER_URI, Date.now() * 1000);

  // The addDownload functions calls CanAddURI synchronously, thus we can
  // exit Private Browsing Mode immediately.
  pb.privateBrowsingEnabled = false;
  Services.prefs.clearUserPref("browser.privatebrowsing.keep_current_session");
  gDownloadHistory.addDownload(DOWNLOAD_URI, REFERRER_URI, Date.now() * 1000);
});

add_test(function test_dh_addDownload_disabledHistory()
{
  waitForOnVisit(function DHAD_onVisit(aURI) {
    // We should only receive the notification for the non-private URI.  This
    // test is based on the assumption that visit notifications are received in
    // the same order of the addDownload calls, which is currently true because
    // database access is serialized on the same worker thread.
    do_check_true(aURI.equals(DOWNLOAD_URI));

    uri_in_db(DOWNLOAD_URI, true);
    uri_in_db(PRIVATE_URI, false);

    waitForClearHistory(run_next_test);
  });

  Services.prefs.setBoolPref("places.history.enabled", false);
  gDownloadHistory.addDownload(PRIVATE_URI, REFERRER_URI, Date.now() * 1000);

  // The addDownload functions calls CanAddURI synchronously, thus we can set
  // the preference back to true immediately (not all apps enable places by
  // default).
  Services.prefs.setBoolPref("places.history.enabled", true);
  gDownloadHistory.addDownload(DOWNLOAD_URI, REFERRER_URI, Date.now() * 1000);
});

/**
 * Tests that nsIDownloadHistory::AddDownload saves the additional download
 * details if the optional destination URL is specified.
 */
add_test(function test_dh_details()
{
  const REMOTE_URI = NetUtil.newURI("http://localhost/");
  const SOURCE_URI = NetUtil.newURI("http://example.com/test_dh_details");
  const DEST_FILE_NAME = "dest.txt";

  // We must build a real, valid file URI for the destination.
  let destFileUri = NetUtil.newURI(FileUtils.getFile("TmpD", [DEST_FILE_NAME]));

  let titleSet = false;
  let destinationFileUriSet = false;
  let destinationFileNameSet = false;

  function checkFinished()
  {
    if (titleSet && destinationFileUriSet && destinationFileNameSet) {
      PlacesUtils.annotations.removeObserver(annoObserver);
      PlacesUtils.history.removeObserver(historyObserver);

      waitForClearHistory(run_next_test);
    }
  };

  let annoObserver = {
    onPageAnnotationSet: function AO_onPageAnnotationSet(aPage, aName)
    {
      if (aPage.equals(SOURCE_URI)) {
        let value = PlacesUtils.annotations.getPageAnnotation(aPage, aName);
        switch (aName)
        {
          case "downloads/destinationFileURI":
            destinationFileUriSet = true;
            do_check_eq(value, destFileUri.spec);
            break;
          case "downloads/destinationFileName":
            destinationFileNameSet = true;
            do_check_eq(value, DEST_FILE_NAME);
            break;
        }
        checkFinished();
      }
    },
    onItemAnnotationSet: function() {},
    onPageAnnotationRemoved: function() {},
    onItemAnnotationRemoved: function() {}
  }

  let historyObserver = {
    onBeginUpdateBatch: function() {},
    onEndUpdateBatch: function() {},
    onVisit: function() {},
    onTitleChanged: function HO_onTitleChanged(aURI, aPageTitle)
    {
      if (aURI.equals(SOURCE_URI)) {
        titleSet = true;
        do_check_eq(aPageTitle, DEST_FILE_NAME);
        checkFinished();
      }
    },
    onBeforeDeleteURI: function() {},
    onDeleteURI: function() {},
    onClearHistory: function() {},
    onPageChanged: function() {},
    onDeleteVisits: function() {}
  };

  PlacesUtils.annotations.addObserver(annoObserver, false);
  PlacesUtils.history.addObserver(historyObserver, false);

  // Both null values and remote URIs should not cause errors.
  gDownloadHistory.addDownload(SOURCE_URI, null, Date.now() * 1000);
  gDownloadHistory.addDownload(SOURCE_URI, null, Date.now() * 1000, null);
  gDownloadHistory.addDownload(SOURCE_URI, null, Date.now() * 1000, REMOTE_URI);

  // Valid local file URIs should cause the download details to be saved.
  gDownloadHistory.addDownload(SOURCE_URI, null, Date.now() * 1000,
                               destFileUri);
});
