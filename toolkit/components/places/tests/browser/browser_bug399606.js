/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function() {
  registerCleanupFunction(PlacesUtils.history.clear);

  const URIS = [
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-window.location.href.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-history.go-0.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-location.replace.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-location.reload.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-httprefresh.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-window.location.html",
  ];

  // Create and add history observer.
  let historyObserver = {
    count: 0,
    expectedURI: null,
    onVisits(aVisits) {
      for (let {uri} of aVisits) {
        info("Received onVisits: " + uri.spec);
        if (uri.equals(this.expectedURI)) {
          this.count++;
        }
      }
    },
    onBeginUpdateBatch() {},
    onEndUpdateBatch() {},
    onTitleChanged() {},
    onDeleteURI() {},
    onClearHistory() {},
    onPageChanged() {},
    onDeleteVisits() {},
    QueryInterface: ChromeUtils.generateQI([Ci.nsINavHistoryObserver])
  };

  async function promiseLoadedThreeTimes(uri) {
    historyObserver.count = 0;
    historyObserver.expectedURI = Services.io.newURI(uri);
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    PlacesUtils.history.addObserver(historyObserver);
    gBrowser.loadURI(uri);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, uri);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, uri);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, uri);
    PlacesUtils.history.removeObserver(historyObserver);
    BrowserTestUtils.removeTab(tab);
  }

  for (let uri of URIS) {
    await promiseLoadedThreeTimes(uri);
    is(historyObserver.count, 1,
      "onVisit has been received right number of times for " + uri);
  }
});
