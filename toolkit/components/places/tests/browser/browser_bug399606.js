/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

gBrowser.selectedTab = gBrowser.addTab();

function test() {
  waitForExplicitFinish();

  var URIs = [
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-window.location.href.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-history.go-0.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-location.replace.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-location.reload.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-httprefresh.html",
    "http://example.com/tests/toolkit/components/places/tests/browser/399606-window.location.html",
  ];
  var hs = Cc["@mozilla.org/browser/nav-history-service;1"].
           getService(Ci.nsINavHistoryService);

  // Create and add history observer.
  var historyObserver = {
    visitCount: Array(),
    onBeginUpdateBatch() {},
    onEndUpdateBatch() {},
    onVisit(aURI, aVisitID, aTime, aSessionID, aReferringID,
                      aTransitionType) {
      info("Received onVisit: " + aURI.spec);
      if (aURI.spec in this.visitCount)
        this.visitCount[aURI.spec]++;
      else
        this.visitCount[aURI.spec] = 1;
    },
    onTitleChanged() {},
    onDeleteURI() {},
    onClearHistory() {},
    onPageChanged() {},
    onDeleteVisits() {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
  };
  hs.addObserver(historyObserver, false);

  function confirm_results() {
    gBrowser.removeCurrentTab();
    hs.removeObserver(historyObserver, false);
    for (let aURI in historyObserver.visitCount) {
      is(historyObserver.visitCount[aURI], 1,
         "onVisit has been received right number of times for " + aURI);
    }
    PlacesTestUtils.clearHistory().then(finish);
  }

  var loadCount = 0;
  function handleLoad(aEvent) {
    loadCount++;
    info("new load count is " + loadCount);

    if (loadCount == 3) {
      gBrowser.removeEventListener("DOMContentLoaded", handleLoad, true);
      gBrowser.loadURI("about:blank");
      executeSoon(check_next_uri);
    }
  }

  function check_next_uri() {
    if (URIs.length) {
      let uri = URIs.shift();
      loadCount = 0;
      gBrowser.addEventListener("DOMContentLoaded", handleLoad, true);
      gBrowser.loadURI(uri);
    }
    else {
      confirm_results();
    }
  }
  executeSoon(check_next_uri);
}
