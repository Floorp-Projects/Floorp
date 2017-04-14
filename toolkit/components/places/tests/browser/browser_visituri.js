/**
 * One-time observer callback.
 */
function promiseObserve(name, checkFn) {
  return new Promise(resolve => {
    Services.obs.addObserver(function observer(subject) {
      if (checkFn(subject)) {
        Services.obs.removeObserver(observer, name);
        resolve();
      }
    }, name);
  });
}

var conn = PlacesUtils.history.QueryInterface(Ci.nsPIPlacesDatabase).DBConnection;

/**
 * Gets a single column value from either the places or historyvisits table.
 */
function getColumn(table, column, fromColumnName, fromColumnValue) {
  let sql = `SELECT ${column}
             FROM ${table}
             WHERE ${fromColumnName} = :val
             ${fromColumnName == "url" ? "AND url_hash = hash(:val)" : ""}
             LIMIT 1`;
  let stmt = conn.createStatement(sql);
  try {
    stmt.params.val = fromColumnValue;
    ok(stmt.executeStep(), "Expect to get a row");
    return stmt.row[column];
  } finally {
    stmt.reset();
  }
}

add_task(function* () {
  // Make sure places visit chains are saved correctly with a redirect
  // transitions.

  // Part 1: observe history events that fire when a visit occurs.
  // Make sure visits appear in order, and that the visit chain is correct.
  var expectedUrls = [
    "http://example.com/tests/toolkit/components/places/tests/browser/begin.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/redirect_twice.sjs",
    "http://example.com/tests/toolkit/components/places/tests/browser/redirect_once.sjs",
    "http://example.com/tests/toolkit/components/places/tests/browser/final.html"
  ];
  var currentIndex = 0;

  function checkObserver(subject) {
    var uri = subject.QueryInterface(Ci.nsIURI);
    var expected = expectedUrls[currentIndex];
    is(uri.spec, expected, "Saved URL visit " + uri.spec);

    var placeId = getColumn("moz_places", "id", "url", uri.spec);
    var fromVisitId = getColumn("moz_historyvisits", "from_visit", "place_id", placeId);

    if (currentIndex == 0) {
      is(fromVisitId, 0, "First visit has no from visit");
    } else {
      var lastVisitId = getColumn("moz_historyvisits", "place_id", "id", fromVisitId);
      var fromVisitUrl = getColumn("moz_places", "url", "id", lastVisitId);
      is(fromVisitUrl, expectedUrls[currentIndex - 1],
         "From visit was " + expectedUrls[currentIndex - 1]);
    }

    currentIndex++;
    return (currentIndex >= expectedUrls.length);
  }
  let visitUriPromise = promiseObserve("uri-visit-saved", checkObserver);

  const testUrl = "http://example.com/tests/toolkit/components/places/tests/browser/begin.html";
  yield BrowserTestUtils.openNewForegroundTab(gBrowser, testUrl);

  // Load begin page, click link on page to record visits.
  yield BrowserTestUtils.synthesizeMouseAtCenter("#clickme", { }, gBrowser.selectedBrowser);
  yield visitUriPromise;

  yield PlacesTestUtils.clearHistory();

  gBrowser.removeCurrentTab();
});
