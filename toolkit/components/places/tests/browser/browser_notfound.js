/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function() {
  const TEST_URL = "http://mochi.test:8888/notFoundPage.html";

  // Used to verify errors are not marked as typed.
  PlacesUtils.history.markPageAsTyped(NetUtil.newURI(TEST_URL));

  // Create and add history observer.
  let visitedPromise = new Promise(resolve => {
    let historyObserver = {
      onVisit(aURI, aVisitID, aTime, aReferringID, aTransitionType) {
        PlacesUtils.history.removeObserver(historyObserver);
        info("Received onVisit: " + aURI.spec);
        fieldForUrl(aURI, "frecency", function(aFrecency) {
          is(aFrecency, 0, "Frecency should be 0");
          fieldForUrl(aURI, "hidden", function(aHidden) {
            is(aHidden, 0, "Page should not be hidden");
            fieldForUrl(aURI, "typed", function(aTyped) {
              is(aTyped, 0, "page should not be marked as typed");
              resolve();
            });
          });
        });
      },
      onVisits(aVisits) {
        is(aVisits.length, 1, "Right number of visits notified");
        let {
          uri,
          visitId,
          time,
          referrerId,
          transitionType,
        } = aVisits[0];
        this.onVisit(uri, visitId, time, referrerId, transitionType);
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
    PlacesUtils.history.addObserver(historyObserver);
  });

  let newTabPromise = BrowserTestUtils.openNewForegroundTab(gBrowser, TEST_URL);
  await Promise.all([visitedPromise, newTabPromise]);

  await PlacesUtils.history.clear();
  gBrowser.removeCurrentTab();
});
