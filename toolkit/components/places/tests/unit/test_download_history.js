/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * The first part of this file runs a series of tests for the synchronous
 * behavior of the nsIDownloadHistory::AddDownload function.
 */

// Get services
var histsvc = Cc["@mozilla.org/browser/nav-history-service;1"].
              getService(Ci.nsINavHistoryService);
let bh = histsvc.QueryInterface(Ci.nsIBrowserHistory);
var os = Cc["@mozilla.org/observer-service;1"].
         getService(Ci.nsIObserverService);
var prefs = Cc["@mozilla.org/preferences-service;1"].
            getService(Ci.nsIPrefBranch);
var dh = Cc["@mozilla.org/browser/download-history;1"].
         getService(Ci.nsIDownloadHistory);
// Test that this nsIDownloadHistory is the one places implements.
do_check_true(dh instanceof Ci.nsINavHistoryService);

const NS_LINK_VISITED_EVENT_TOPIC = "link-visited";
const ENABLE_HISTORY_PREF = "places.history.enabled";
const PB_KEEP_SESSION_PREF = "browser.privatebrowsing.keep_current_session";

var testURI = uri("http://google.com/");
var referrerURI = uri("http://yahoo.com");

/**
 * Checks to see that a URI is in the database.
 *
 * @param aURI
 *        The URI to check.
 * @param aExpected
 *        Boolean result expected from the db lookup.
 */
function uri_in_db(aURI, aExpected) {
  var options = histsvc.getNewQueryOptions();
  options.maxResults = 1;
  options.resultType = options.RESULTS_AS_URI;
  options.includeHidden = true;
  var query = histsvc.getNewQuery();
  query.uri = aURI;
  var result = histsvc.executeQuery(query, options);
  var root = result.root;
  root.containerOpen = true;
  var cc = root.childCount;
  root.containerOpen = false;
  var checker = aExpected ? do_check_true : do_check_false;
  checker(cc == 1);
}

function test_dh() {
  dh.addDownload(testURI, referrerURI, Date.now() * 1000);

  do_check_true(observer.topicReceived);
  uri_in_db(testURI, true);
  uri_in_db(referrerURI, true);
}

function test_dh_privateBrowsing() {
  var pb = null;
  try {
    // PrivateBrowsing component is not always available to Places implementers.
     pb = Cc["@mozilla.org/privatebrowsing;1"].
          getService(Ci.nsIPrivateBrowsingService);
  } catch (ex) {
    // Skip this test.
    return;
  }
  prefs.setBoolPref(PB_KEEP_SESSION_PREF, true);
  pb.privateBrowsingEnabled = true;

  dh.addDownload(testURI, referrerURI, Date.now() * 1000);

  do_check_false(observer.topicReceived);
  uri_in_db(testURI, false);
  uri_in_db(referrerURI, false);

  // Cleanup
  pb.privateBrowsingEnabled = false;
}

function test_dh_disabledHistory() {
  // Disable history
  prefs.setBoolPref(ENABLE_HISTORY_PREF, false);

  dh.addDownload(testURI, referrerURI, Date.now() * 1000);

  do_check_false(observer.topicReceived);
  uri_in_db(testURI, false);
  uri_in_db(referrerURI, false);

  // Cleanup
  prefs.setBoolPref(ENABLE_HISTORY_PREF, true);
}

var tests = [
  test_dh,
  test_dh_privateBrowsing,
  test_dh_disabledHistory,
];

var observer = {
  topicReceived: false,
  observe: function tlvo_observe(aSubject, aTopic, aData)
  {
    if (NS_LINK_VISITED_EVENT_TOPIC == aTopic) {
      this.topicReceived = true;
    }
  }
};
os.addObserver(observer, NS_LINK_VISITED_EVENT_TOPIC, false);

// main
function run_test() {
  while (tests.length) {
    // Sanity checks
    uri_in_db(testURI, false);
    uri_in_db(referrerURI, false);

    (tests.shift())();

    // Cleanup
    bh.removeAllPages();
    observer.topicReceived = false;
  }

  os.removeObserver(observer, NS_LINK_VISITED_EVENT_TOPIC);

  // Asynchronous part of the test.
  test_dh_details();
}

/**
 * The second part of this file tests that nsIDownloadHistory::AddDownload saves
 * the additional download details if the optional destination URL is specified.
 */

function test_dh_details()
{
  do_test_pending();

  const SOURCE_URI = uri("http://example.com/test_download_history_details");
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

      // Cleanup.
      bh.removeAllPages();
      do_test_finished();
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
  dh.addDownload(SOURCE_URI, null, Date.now() * 1000);
  dh.addDownload(SOURCE_URI, null, Date.now() * 1000, null);
  dh.addDownload(SOURCE_URI, null, Date.now() * 1000, uri("http://localhost/"));

  // Valid local file URIs should cause the download details to be saved.
  dh.addDownload(SOURCE_URI, null, Date.now() * 1000, destFileUri);
}
