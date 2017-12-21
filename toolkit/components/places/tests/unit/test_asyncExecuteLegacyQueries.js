/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

// This is a test for asyncExecuteLegacyQueries API.

add_task(async function test_history_query() {
  let uri = "http://test.visit.mozilla.com/";
  let title = "Test visit";
  await PlacesTestUtils.addVisits({ uri, title });

  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_DATE_DESCENDING;
  let query = PlacesUtils.history.getNewQuery();

  return new Promise(resolve => {
    PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                       .asyncExecuteLegacyQueries([query], 1, options, {
      handleResult(aResultSet) {
        for (let row; (row = aResultSet.getNextRow());) {
          try {
            Assert.equal(row.getResultByIndex(1), uri);
            Assert.equal(row.getResultByIndex(2), title);
          } catch (e) {
            do_throw("Error while fetching page data.");
          }
        }
      },
      handleError(aError) {
        do_throw("Async execution error (" + aError.result + "): " + aError.message);
      },
      handleCompletion(aReason) {
        cleanupTest().then(resolve);
      },
    });
  });
});

add_task(async function test_bookmarks_query() {
  let url = "http://test.bookmark.mozilla.com/";
  let title = "Test bookmark";
  await PlacesUtils.bookmarks.insert({
    parentGuid: PlacesUtils.bookmarks.unfiledGuid,
    title,
    url,
  });

  let options = PlacesUtils.history.getNewQueryOptions();
  options.sortingMode = Ci.nsINavHistoryQueryOptions.SORT_BY_LASMODIFIED_DESCENDING;
  options.queryType = options.QUERY_TYPE_BOOKMARKS;
  let query = PlacesUtils.history.getNewQuery();

  return new Promise(resolve => {
    PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase)
                       .asyncExecuteLegacyQueries([query], 1, options, {
      handleResult(aResultSet) {
        for (let row; (row = aResultSet.getNextRow());) {
          try {
            Assert.equal(row.getResultByIndex(1), url);
            Assert.equal(row.getResultByIndex(2), title);
          } catch (e) {
            do_throw("Error while fetching page data.");
          }
        }
      },
      handleError(aError) {
        do_throw("Async execution error (" + aError.result + "): " + aError.message);
      },
      handleCompletion(aReason) {
        cleanupTest().then(resolve);
      },
    });
  });
});

function cleanupTest() {
  return Promise.all([
    PlacesTestUtils.clearHistory(),
    PlacesUtils.bookmarks.eraseEverything()
  ]);
}
