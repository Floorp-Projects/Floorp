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
  let count = 0;
  let expectedURI = null;
  function onVisitsListener(aEvents) {
    for (let event of aEvents) {
      info("Received onVisits: " + event.url);
      if (event.url == expectedURI) {
        count++;
      }
    }
  }

  async function promiseLoadedThreeTimes(uri) {
    count = 0;
    expectedURI = uri;
    let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
    PlacesObservers.addListener(["page-visited"], onVisitsListener);
    gBrowser.loadURI(uri);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, uri);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, uri);
    await BrowserTestUtils.browserLoaded(gBrowser.selectedBrowser, false, uri);
    PlacesObservers.removeListener(["page-visited"], onVisitsListener);
    BrowserTestUtils.removeTab(tab);
  }

  for (let uri of URIS) {
    await promiseLoadedThreeTimes(uri);
    is(count, 1,
      "'page-visited' has been received right number of times for " + uri);
  }
});
