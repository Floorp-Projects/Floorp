var conn = PlacesUtils.history.DBConnection;

/**
 * Gets a single column value from either the places or historyvisits table.
 */
function getColumn(table, column, url) {
  var stmt = conn.createStatement(
    `SELECT ${column} FROM ${table} WHERE url_hash = hash(:val) AND url = :val`);
  try {
    stmt.params.val = url;
    stmt.executeStep();
    return stmt.row[column];
  } finally {
    stmt.finalize();
  }
}

add_task(async function() {
  // Make sure titles are correctly saved for a URI with the proper
  // notifications.

  // Create and add history observer.
  let titleChangedPromise = new Promise(resolve => {
    var historyObserver = {
      data: [],
      onBeginUpdateBatch() {},
      onEndUpdateBatch() {},
      onVisits() {},
      onTitleChanged(aURI, aPageTitle, aGUID) {
        this.data.push({ uri: aURI, title: aPageTitle, guid: aGUID });

        // We only expect one title change.
        //
        // Although we are loading two different pages, the first page does not
        // have a title.  Since the title starts out as empty and then is set
        // to empty, there is no title change notification.

        PlacesUtils.history.removeObserver(this);
        resolve(this.data);
      },
      onDeleteURI() {},
      onClearHistory() {},
      onPageChanged() {},
      onDeleteVisits() {},
      QueryInterface: ChromeUtils.generateQI([Ci.nsINavHistoryObserver])
    };
    PlacesUtils.history.addObserver(historyObserver);
  });

  const url1 = "http://example.com/tests/toolkit/components/places/tests/browser/title1.html";
  await BrowserTestUtils.openNewForegroundTab(gBrowser, url1);

  const url2 = "http://example.com/tests/toolkit/components/places/tests/browser/title2.html";
  let loadPromise = BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser);
  BrowserTestUtils.loadURI(gBrowser.selectedBrowser, url2);
  await loadPromise;

  let data = await titleChangedPromise;
  is(data[0].uri.spec, "http://example.com/tests/toolkit/components/places/tests/browser/title2.html");
  is(data[0].title, "Some title");
  is(data[0].guid, getColumn("moz_places", "guid", data[0].uri.spec));

  data.forEach(function(item) {
    var title = getColumn("moz_places", "title", data[0].uri.spec);
    is(title, item.title);
  });

  gBrowser.removeCurrentTab();
  await PlacesUtils.history.clear();
});

