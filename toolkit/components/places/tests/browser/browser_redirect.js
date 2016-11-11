/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(function* () {
  const REDIRECT_URI = NetUtil.newURI("http://mochi.test:8888/tests/toolkit/components/places/tests/browser/redirect.sjs");
  const TARGET_URI = NetUtil.newURI("http://mochi.test:8888/tests/toolkit/components/places/tests/browser/redirect-target.html");

  // Create and add history observer.
  let visitedPromise = new Promise(resolve => {
    let historyObserver = {
      _redirectNotified: false,
      onVisit: function(aURI, aVisitID, aTime, aSessionID, aReferringID,
                        aTransitionType) {
        info("Received onVisit: " + aURI.spec);

        if (aURI.equals(REDIRECT_URI)) {
          this._redirectNotified = true;
          // Wait for the target page notification.
          return;
        }

        PlacesUtils.history.removeObserver(historyObserver);

        ok(this._redirectNotified, "The redirect should have been notified");

        fieldForUrl(REDIRECT_URI, "frecency", function(aFrecency) {
          ok(aFrecency != 0, "Frecency or the redirecting page should not be 0");

          fieldForUrl(REDIRECT_URI, "hidden", function(aHidden) {
            is(aHidden, 1, "The redirecting page should be hidden");

            fieldForUrl(TARGET_URI, "frecency", function(aFrecency2) {
              ok(aFrecency2 != 0, "Frecency of the target page should not be 0");

              fieldForUrl(TARGET_URI, "hidden", function(aHidden2) {
                is(aHidden2, 0, "The target page should not be hidden");
                resolve();
              });
            });
          });
        });
      },
      onBeginUpdateBatch: function() {},
      onEndUpdateBatch: function() {},
      onTitleChanged: function() {},
      onDeleteURI: function() {},
      onClearHistory: function() {},
      onPageChanged: function() {},
      onDeleteVisits: function() {},
      QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
    };
    PlacesUtils.history.addObserver(historyObserver, false);
  });

  let newTabPromise = BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  yield Promise.all([visitedPromise, newTabPromise]);

  yield PlacesTestUtils.clearHistory();
  gBrowser.removeCurrentTab();
});
