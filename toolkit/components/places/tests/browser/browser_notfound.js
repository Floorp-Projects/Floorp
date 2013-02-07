/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

function test() {
  waitForExplicitFinish();

  gBrowser.selectedTab = gBrowser.addTab();
  registerCleanupFunction(function() {
    gBrowser.removeCurrentTab();
  });
  gBrowser.selectedTab.linkedBrowser.loadURI("http://mochi.test:8888/notFoundPage.html");

  // Create and add history observer.
  let historyObserver = {
    onVisit: function (aURI, aVisitID, aTime, aSessionID, aReferringID,
                      aTransitionType) {
      PlacesUtils.history.removeObserver(historyObserver);
      info("Received onVisit: " + aURI.spec);
      fieldForUrl(aURI, "frecency", function (aFrecency) {
        is(aFrecency, 0, "Frecency should be 0");
        fieldForUrl(aURI, "hidden", function (aHidden) {
          is(aHidden, 0, "Page should not be hidden");
          promiseClearHistory().then(finish);
        });
      });
    },
    onBeginUpdateBatch: function () {},
    onEndUpdateBatch: function () {},
    onTitleChanged: function () {},
    onDeleteURI: function () {},
    onClearHistory: function () {},
    onPageChanged: function () {},
    onDeleteVisits: function () {},
    QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
  };
  PlacesUtils.history.addObserver(historyObserver, false);
}
