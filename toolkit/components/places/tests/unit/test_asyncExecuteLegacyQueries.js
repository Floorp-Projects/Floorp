/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This is a test for asyncExecuteLegacyQueries API.

var tests = [

function test_history_query() {
  let uri = NetUtil.newURI("http://test.visit.mozilla.com/");
  let title = "Test visit";
  PlacesTestUtils.addVisits({ uri, title }).then(function() {
    let options = PlacesUtils.history.getNewQueryOptions();
    options.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING;
    let query = PlacesUtils.history.getNewQuery();

    PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                       .asyncExecuteLegacyQueries([query], 1, options, {
      handleResult(aResultSet) {
        for (let row; (row = aResultSet.getNextRow());) {
          try {
            do_check_eq(row.getResultByIndex(1), uri.spec);
            do_check_eq(row.getResultByIndex(2), title);
          } catch (e) {
            do_throw("Error while fetching page data.");
          }
        }
      },
      handleError(aError) {
        do_throw("Async execution error (" + aError.result + "): " + aError.message);
      },
      handleCompletion(aReason) {
        run_next_test();
      },
    });
  });
},

function test_bookmarks_query() {
  let uri = NetUtil.newURI("http://test.bookmark.mozilla.com/");
  let title = "Test bookmark";
  bookmark(uri, title);
  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_LASMODIFIED_DESCENDING;
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  let query = PlacesUtils.history.getNewQuery();

  PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                     .asyncExecuteLegacyQueries([query], 1, options, {
    handleResult(aResultSet) {
      for (let row; (row = aResultSet.getNextRow());) {
        try {
          do_check_eq(row.getResultByIndex(1), uri.spec);
          do_check_eq(row.getResultByIndex(2), title);
        } catch (e) {
          do_throw("Error while fetching page data.");
        }
      }
    },
    handleError(aError) {
      do_throw("Async execution error (" + aError.result + "): " + aError.message);
    },
    handleCompletion(aReason) {
      run_next_test();
    },
  });
},

];

function bookmark(aURI, aTitle)
{
  PlacesUtils.bookmarks.insertBookmark(PlacesUtils.unfiledBookmarksFolderId,
                                       aURI,
                                       PlacesUtils.bookmarks.DEFAULT_INDEX,
                                       aTitle);
}

function run_test()
{
  do_test_pending();
  run_next_test();
}

function run_next_test() {
  if (tests.length == 0) {
    do_test_finished();
    return;
  }

  Promise.all([
    PlacesTestUtils.clearHistory(),
    PlacesUtils.bookmarks.eraseEverything()
  ]).then(tests.shift());
}
