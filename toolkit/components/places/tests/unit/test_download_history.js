/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This file tests the nsIDownloadHistory interface.
 */

////////////////////////////////////////////////////////////////////////////////
/// Globals

const downloadHistory = Cc["@mozilla.org/browser/download-history;1"]
                        .getService(Ci.nsIDownloadHistory);

const TEST_URI = NetUtil.newURI("http://google.com/");
const REFERRER_URI = NetUtil.newURI("http://yahoo.com");

const NS_LINK_VISITED_EVENT_TOPIC = "link-visited";
const ENABLE_HISTORY_PREF = "places.history.enabled";
const PB_KEEP_SESSION_PREF = "browser.privatebrowsing.keep_current_session";

/**
 * Sets a flag when the link visited notification is received.
 */
const visitedObserver = {
  topicReceived: false,
  observe: function VO_observe(aSubject, aTopic, aData)
  {
    this.topicReceived = true;
  }
};
Services.obs.addObserver(visitedObserver, NS_LINK_VISITED_EVENT_TOPIC, false);
do_register_cleanup(function() {
  Services.obs.removeObserver(visitedObserver, NS_LINK_VISITED_EVENT_TOPIC);
});

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

/**
 * Cleanup function called by each individual test if necessary.
 */
function cleanup_and_run_next_test()
{
  visitedObserver.topicReceived = false;
  waitForClearHistory(run_next_test);
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
  do_check_true(downloadHistory instanceof Ci.nsINavHistoryService);

  run_next_test();
});

add_test(function test_dh_addDownload()
{
  // Sanity checks.
  uri_in_db(TEST_URI, false);
  uri_in_db(REFERRER_URI, false);

  downloadHistory.addDownload(TEST_URI, REFERRER_URI, Date.now() * 1000);

  do_check_true(visitedObserver.topicReceived);
  uri_in_db(TEST_URI, true);
  uri_in_db(REFERRER_URI, true);

  cleanup_and_run_next_test();
});

add_test(function test_dh_privateBrowsing()
{
  // Sanity checks.
  uri_in_db(TEST_URI, false);
  uri_in_db(REFERRER_URI, false);

  var pb = null;
  try {
    // PrivateBrowsing component is not always available to Places implementers.
     pb = Cc["@mozilla.org/privatebrowsing;1"].
          getService(Ci.nsIPrivateBrowsingService);
  } catch (ex) {
    // Skip this test.
    run_next_test();
    return;
  }
  Services.prefs.setBoolPref(PB_KEEP_SESSION_PREF, true);
  pb.privateBrowsingEnabled = true;

  downloadHistory.addDownload(TEST_URI, REFERRER_URI, Date.now() * 1000);

  do_check_false(visitedObserver.topicReceived);
  uri_in_db(TEST_URI, false);
  uri_in_db(REFERRER_URI, false);

  pb.privateBrowsingEnabled = false;
  cleanup_and_run_next_test();
});

add_test(function test_dh_disabledHistory()
{
  // Sanity checks.
  uri_in_db(TEST_URI, false);
  uri_in_db(REFERRER_URI, false);

  // Disable history.
  Services.prefs.setBoolPref(ENABLE_HISTORY_PREF, false);

  downloadHistory.addDownload(TEST_URI, REFERRER_URI, Date.now() * 1000);

  do_check_false(visitedObserver.topicReceived);
  uri_in_db(TEST_URI, false);
  uri_in_db(REFERRER_URI, false);

  Services.prefs.setBoolPref(ENABLE_HISTORY_PREF, true);
  cleanup_and_run_next_test();
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

      cleanup_and_run_next_test();
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
  downloadHistory.addDownload(SOURCE_URI, null, Date.now() * 1000);
  downloadHistory.addDownload(SOURCE_URI, null, Date.now() * 1000, null);
  downloadHistory.addDownload(SOURCE_URI, null, Date.now() * 1000, REMOTE_URI);

  // Valid local file URIs should cause the download details to be saved.
  downloadHistory.addDownload(SOURCE_URI, null, Date.now() * 1000, destFileUri);
});
