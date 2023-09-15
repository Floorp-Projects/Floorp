var conn = PlacesUtils.history.DBConnection;

/**
 * Gets a single column value from either the places or historyvisits table.
 */
function getColumn(table, column, url) {
  var stmt = conn.createStatement(
    `SELECT ${column} FROM ${table} WHERE url_hash = hash(:val) AND url = :val`
  );
  try {
    stmt.params.val = url;
    stmt.executeStep();
    return stmt.row[column];
  } finally {
    stmt.finalize();
  }
}

add_task(async function () {
  // Make sure titles are correctly saved for a URI with the proper
  // notifications.
  const titleChangedPromise =
    PlacesTestUtils.waitForNotification("page-title-changed");

  const url1 =
    "http://example.com/tests/toolkit/components/places/tests/browser/title1.html";
  await BrowserTestUtils.openNewForegroundTab(gBrowser, url1);

  const url2 =
    "http://example.com/tests/toolkit/components/places/tests/browser/title2.html";
  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.startLoadingURIString(gBrowser.selectedBrowser, url2);
  await loadPromise;

  const events = await titleChangedPromise;
  is(
    events[0].url,
    "http://example.com/tests/toolkit/components/places/tests/browser/title2.html"
  );
  is(events[0].title, "Some title");
  is(events[0].pageGuid, getColumn("moz_places", "guid", events[0].url));

  const title = getColumn("moz_places", "title", events[0].url);
  is(title, events[0].title);

  gBrowser.removeCurrentTab();
  await PlacesUtils.history.clear();
});
