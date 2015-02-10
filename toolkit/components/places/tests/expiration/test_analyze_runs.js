/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

////////////////////////////////////////////////////////////////////////////////
//// Constants

const TOPIC_AUTOCOMPLETE_FEEDBACK_INCOMING = "autocomplete-will-enter-text";

////////////////////////////////////////////////////////////////////////////////
//// Helpers

/**
 * Ensures that we have no data in the tables created by ANALYZE.
 */
function clearAnalyzeData()
{
  let db = DBConn();
  if (!db.tableExists("sqlite_stat1")) {
    return;
  }
  db.executeSimpleSQL("DELETE FROM sqlite_stat1");
}

/**
 * Checks that we ran ANALYZE on the specified table.
 *
 * @param aTableName
 *        The table to check if ANALYZE was ran.
 * @param aRan
 *        True if it was expected to run, false otherwise
 */
function do_check_analyze_ran(aTableName, aRan)
{
  let db = DBConn();
  do_check_true(db.tableExists("sqlite_stat1"));
  let stmt = db.createStatement("SELECT idx FROM sqlite_stat1 WHERE tbl = :table");
  stmt.params.table = aTableName;
  try {
    if (aRan) {
      do_check_true(stmt.executeStep());
      do_check_neq(stmt.row.idx, null);
    }
    else {
      do_check_false(stmt.executeStep());
    }
  }
  finally {
    stmt.finalize();
  }
}

////////////////////////////////////////////////////////////////////////////////
//// Tests

function run_test()
{
  run_next_test();
}

add_task(function init_tests()
{
  const TEST_URI = NetUtil.newURI("http://mozilla.org/");
  const TEST_TITLE = "This is a test";
  let bs = PlacesUtils.bookmarks;
  bs.insertBookmark(PlacesUtils.unfiledBookmarksFolderId, TEST_URI,
                    bs.DEFAULT_INDEX, TEST_TITLE);
  yield PlacesTestUtils.addVisits(TEST_URI);
  let thing = {
    QueryInterface: XPCOMUtils.generateQI([Ci.nsIAutoCompleteInput,
                                           Ci.nsIAutoCompletePopup,
                                           Ci.nsIAutoCompleteController]),
    get popup() { return thing; },
    get controller() { return thing; },
    popupOpen: true,
    selectedIndex: 0,
    getValueAt: function() { return TEST_URI.spec; },
    searchString: TEST_TITLE,
  };
  Services.obs.notifyObservers(thing, TOPIC_AUTOCOMPLETE_FEEDBACK_INCOMING,
                               null);
});

add_task(function test_timed()
{
  clearAnalyzeData();

  // Set a low interval and wait for the timed expiration to start.
  let promise = promiseTopicObserved(PlacesUtils.TOPIC_EXPIRATION_FINISHED);
  setInterval(3);
  yield promise;
  setInterval(3600);

  do_check_analyze_ran("moz_places", false);
  do_check_analyze_ran("moz_bookmarks", false);
  do_check_analyze_ran("moz_historyvisits", false);
  do_check_analyze_ran("moz_inputhistory", true);
});

add_task(function test_debug()
{
  clearAnalyzeData();

  yield promiseForceExpirationStep(1);

  do_check_analyze_ran("moz_places", true);
  do_check_analyze_ran("moz_bookmarks", true);
  do_check_analyze_ran("moz_historyvisits", true);
  do_check_analyze_ran("moz_inputhistory", true);
});

add_task(function test_clear_history()
{
  clearAnalyzeData();

  let promise = promiseTopicObserved(PlacesUtils.TOPIC_EXPIRATION_FINISHED);
  let listener = Cc["@mozilla.org/places/expiration;1"]
                 .getService(Ci.nsINavHistoryObserver);
  listener.onClearHistory();
  yield promise;

  do_check_analyze_ran("moz_places", true);
  do_check_analyze_ran("moz_bookmarks", false);
  do_check_analyze_ran("moz_historyvisits", true);
  do_check_analyze_ran("moz_inputhistory", true);
});
