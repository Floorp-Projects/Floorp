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

function test_timed()
{
  clearAnalyzeData();

  let observer = function(aSubject, aTopic, aData) {
    Services.obs.removeObserver(observer,
                                PlacesUtils.TOPIC_EXPIRATION_FINISHED);
    setInterval(3600);
    do_check_analyze_ran("moz_places", false);
    do_check_analyze_ran("moz_bookmarks", false);
    do_check_analyze_ran("moz_historyvisits", false);
    do_check_analyze_ran("moz_inputhistory", true);
    run_next_test();
  };

  Services.obs.addObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED,
                           false);
  // Set a low interval and wait for the timed expiration to start.
  setInterval(3);
}

function test_debug()
{
  clearAnalyzeData();

  let observer = function(aSubject, aTopic, aData) {
    Services.obs.removeObserver(observer,
                                PlacesUtils.TOPIC_EXPIRATION_FINISHED);
    do_check_analyze_ran("moz_places", true);
    do_check_analyze_ran("moz_bookmarks", true);
    do_check_analyze_ran("moz_historyvisits", true);
    do_check_analyze_ran("moz_inputhistory", true);
    run_next_test();
  };

  Services.obs.addObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED,
                           false);
  force_expiration_step(1);
}

function test_clear_history()
{
  clearAnalyzeData();

  let observer = function(aSubject, aTopic, aData) {
    Services.obs.removeObserver(observer,
                                PlacesUtils.TOPIC_EXPIRATION_FINISHED);
    do_check_analyze_ran("moz_places", true);
    do_check_analyze_ran("moz_bookmarks", false);
    do_check_analyze_ran("moz_historyvisits", true);
    do_check_analyze_ran("moz_inputhistory", true);
    run_next_test();
  };

  Services.obs.addObserver(observer, PlacesUtils.TOPIC_EXPIRATION_FINISHED,
                           false);
  let listener = Cc["@mozilla.org/places/expiration;1"].
                 getService(Ci.nsINavHistoryObserver);
  listener.onClearHistory();
}

////////////////////////////////////////////////////////////////////////////////
//// Test Harness

[
  test_timed,
  test_debug,
  test_clear_history,
].forEach(add_test);

function run_test()
{
  const TEST_URI = NetUtil.newURI("http://mozilla.org/");
  const TEST_TITLE = "This is a test";
  let bs = PlacesUtils.bookmarks;
  bs.insertBookmark(PlacesUtils.unfiledBookmarksFolderId, TEST_URI,
                    bs.DEFAULT_INDEX, TEST_TITLE);
  let hs = PlacesUtils.history;
  hs.addVisit(TEST_URI, Date.now() * 1000, null, hs.TRANSITION_TYPED, false, 0);

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

  run_next_test();
}
