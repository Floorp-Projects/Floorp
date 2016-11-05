/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the nsIDownloadHistory Places implementation.
 */

XPCOMUtils.defineLazyServiceGetter(this, "gDownloadHistory",
                                   "@mozilla.org/browser/download-history;1",
                                   "nsIDownloadHistory");

const DOWNLOAD_URI = NetUtil.newURI("http://www.example.com/");
const REFERRER_URI = NetUtil.newURI("http://www.example.org/");
const PRIVATE_URI = NetUtil.newURI("http://www.example.net/");

/**
 * Waits for the first visit notification to be received.
 *
 * @param aCallback
 *        Callback function to be called with the same arguments of onVisit.
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
 * Waits for the first onDeleteURI notification to be received.
 *
 * @param aCallback
 *        Callback function to be called with the same arguments of onDeleteURI.
 */
function waitForOnDeleteURI(aCallback) {
  let historyObserver = {
    __proto__: NavHistoryObserver.prototype,
    onDeleteURI: function HO_onDeleteURI() {
      PlacesUtils.history.removeObserver(this);
      aCallback.apply(null, arguments);
    }
  };
  PlacesUtils.history.addObserver(historyObserver, false);
}

/**
 * Waits for the first onDeleteVisits notification to be received.
 *
 * @param aCallback
 *        Callback function to be called with the same arguments of onDeleteVisits.
 */
function waitForOnDeleteVisits(aCallback) {
  let historyObserver = {
    __proto__: NavHistoryObserver.prototype,
    onDeleteVisits: function HO_onDeleteVisits() {
      PlacesUtils.history.removeObserver(this);
      aCallback.apply(null, arguments);
    }
  };
  PlacesUtils.history.addObserver(historyObserver, false);
}

function run_test()
{
  run_next_test();
}

add_test(function test_dh_is_from_places()
{
  // Test that this nsIDownloadHistory is the one places implements.
  do_check_true(gDownloadHistory instanceof Ci.mozIAsyncHistory);

  run_next_test();
});

add_test(function test_dh_addRemoveDownload()
{
  waitForOnVisit(function DHAD_onVisit(aURI) {
    do_check_true(aURI.equals(DOWNLOAD_URI));

    // Verify that the URI is already available in results at this time.
    do_check_true(!!page_in_database(DOWNLOAD_URI));

    waitForOnDeleteURI(function DHRAD_onDeleteURI(aDeletedURI) {
      do_check_true(aDeletedURI.equals(DOWNLOAD_URI));

      // Verify that the URI is already available in results at this time.
      do_check_false(!!page_in_database(DOWNLOAD_URI));

      run_next_test();
    });
    gDownloadHistory.removeAllDownloads();
  });

  gDownloadHistory.addDownload(DOWNLOAD_URI, null, Date.now() * 1000);
});

add_test(function test_dh_addMultiRemoveDownload()
{
  PlacesTestUtils.addVisits({
    uri: DOWNLOAD_URI,
    transition: TRANSITION_TYPED
  }).then(function () {
    waitForOnVisit(function DHAD_onVisit(aURI) {
      do_check_true(aURI.equals(DOWNLOAD_URI));
      do_check_true(!!page_in_database(DOWNLOAD_URI));

      waitForOnDeleteVisits(function DHRAD_onDeleteVisits(aDeletedURI) {
        do_check_true(aDeletedURI.equals(DOWNLOAD_URI));
        do_check_true(!!page_in_database(DOWNLOAD_URI));

        PlacesTestUtils.clearHistory().then(run_next_test);
      });
      gDownloadHistory.removeAllDownloads();
    });

    gDownloadHistory.addDownload(DOWNLOAD_URI, null, Date.now() * 1000);
  });
});

add_test(function test_dh_addBookmarkRemoveDownload()
{
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       DOWNLOAD_URI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       "A bookmark");
  waitForOnVisit(function DHAD_onVisit(aURI) {
    do_check_true(aURI.equals(DOWNLOAD_URI));
    do_check_true(!!page_in_database(DOWNLOAD_URI));

    waitForOnDeleteVisits(function DHRAD_onDeleteVisits(aDeletedURI) {
      do_check_true(aDeletedURI.equals(DOWNLOAD_URI));
      do_check_true(!!page_in_database(DOWNLOAD_URI));

      PlacesTestUtils.clearHistory().then(run_next_test);
    });
    gDownloadHistory.removeAllDownloads();
  });

  gDownloadHistory.addDownload(DOWNLOAD_URI, null, Date.now() * 1000);
});

add_test(function test_dh_addDownload_referrer()
{
  waitForOnVisit(function DHAD_prepareReferrer(aURI, aVisitID) {
    do_check_true(aURI.equals(REFERRER_URI));
    let referrerVisitId = aVisitID;

    waitForOnVisit(function DHAD_onVisit(aVisitedURI, unused, unused2, unused3,
                                         aReferringID) {
      do_check_true(aVisitedURI.equals(DOWNLOAD_URI));
      do_check_eq(aReferringID, referrerVisitId);

      // Verify that the URI is already available in results at this time.
      do_check_true(!!page_in_database(DOWNLOAD_URI));

      PlacesTestUtils.clearHistory().then(run_next_test);
    });

    gDownloadHistory.addDownload(DOWNLOAD_URI, REFERRER_URI, Date.now() * 1000);
  });

  // Note that we don't pass the optional callback argument here because we must
  // ensure that we receive the onVisit notification before we call addDownload.
  PlacesUtils.asyncHistory.updatePlaces({
    uri: REFERRER_URI,
    visits: [{
      transitionType: Ci.nsINavHistoryService.TRANSITION_TYPED,
      visitDate: Date.now() * 1000
    }]
  });
});

add_test(function test_dh_addDownload_disabledHistory()
{
  waitForOnVisit(function DHAD_onVisit(aURI) {
    // We should only receive the notification for the non-private URI.  This
    // test is based on the assumption that visit notifications are received in
    // the same order of the addDownload calls, which is currently true because
    // database access is serialized on the same worker thread.
    do_check_true(aURI.equals(DOWNLOAD_URI));

    do_check_true(!!page_in_database(DOWNLOAD_URI));
    do_check_false(!!page_in_database(PRIVATE_URI));

    PlacesTestUtils.clearHistory().then(run_next_test);
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

      PlacesTestUtils.clearHistory().then(run_next_test);
    }
  }

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
