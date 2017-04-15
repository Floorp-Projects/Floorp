/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

const REDIRECT_URI = NetUtil.newURI("http://mochi.test:8888/tests/toolkit/components/places/tests/browser/redirect.sjs");
const TARGET_URI = NetUtil.newURI("http://mochi.test:8888/tests/toolkit/components/places/tests/browser/redirect-target.html");

const REDIRECT_SOURCE_VISIT_BONUS =
  Services.prefs.getIntPref("places.frecency.redirectSourceVisitBonus");
const LINK_VISIT_BONUS =
  Services.prefs.getIntPref("places.frecency.linkVisitBonus");
const TYPED_VISIT_BONUS =
  Services.prefs.getIntPref("places.frecency.typedVisitBonus");

// Ensure that decay frecency doesn't kick in during tests (as a result
// of idle-daily).
Services.prefs.setCharPref("places.frecency.decayRate", "1.0");

registerCleanupFunction(function*() {
  Services.prefs.clearUserPref("places.frecency.decayRate");
  yield PlacesTestUtils.clearHistory();
});

function promiseVisitedWithFrecency(expectedRedirectFrecency, expectedTargetFrecency) {
  // Create and add history observer.
  return new Promise(resolve => {
    let historyObserver = {
      _redirectNotified: false,
      onVisit(aURI, aVisitID, aTime, aSessionID, aReferringID,
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
         is(aFrecency, expectedRedirectFrecency,
            "Frecency of the redirecting page is the expected one");

         fieldForUrl(REDIRECT_URI, "hidden", function(aHidden) {
           is(aHidden, 1, "The redirecting page should be hidden");

           fieldForUrl(TARGET_URI, "frecency", function(aFrecency2) {
             is(aFrecency2, expectedTargetFrecency,
                "Frecency of the target page is the expected one");

             fieldForUrl(TARGET_URI, "hidden", function(aHidden2) {
               is(aHidden2, 0, "The target page should not be hidden");
               resolve();
             });
           });
         });
       });
      },
      onBeginUpdateBatch() {},
      onEndUpdateBatch() {},
      onTitleChanged() {},
      onDeleteURI() {},
      onClearHistory() {},
      onPageChanged() {},
      onDeleteVisits() {},
      QueryInterface: XPCOMUtils.generateQI([Ci.nsINavHistoryObserver])
    };
    PlacesUtils.history.addObserver(historyObserver);
  });
}

let expectedRedirectSourceFrecency = 0;
let expectedTypedVisitBonus = 0;

add_task(function* redirect_check_new_typed_visit() {
  // Used to verify the redirect bonus overrides the typed bonus.
  PlacesUtils.history.markPageAsTyped(REDIRECT_URI);

  expectedRedirectSourceFrecency += REDIRECT_SOURCE_VISIT_BONUS;
  expectedTypedVisitBonus += TYPED_VISIT_BONUS;

  let visitedPromise = promiseVisitedWithFrecency(expectedRedirectSourceFrecency,
                                                  expectedTypedVisitBonus);

  let newTabPromise = BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  yield Promise.all([visitedPromise, newTabPromise]);

  gBrowser.removeCurrentTab();
});

add_task(function* redirect_check_second_typed_visit() {
  // A second visit with a typed url.
  PlacesUtils.history.markPageAsTyped(REDIRECT_URI);

  expectedRedirectSourceFrecency += REDIRECT_SOURCE_VISIT_BONUS;
  expectedTypedVisitBonus += TYPED_VISIT_BONUS;

  let visitedPromise = promiseVisitedWithFrecency(expectedRedirectSourceFrecency,
                                                  expectedTypedVisitBonus);

  let newTabPromise = BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  yield Promise.all([visitedPromise, newTabPromise]);

  gBrowser.removeCurrentTab();
});

add_task(function* redirect_check_subsequent_link_visit() {
  // Another visit, but this time as a visited url.
  expectedRedirectSourceFrecency += REDIRECT_SOURCE_VISIT_BONUS;
  expectedTypedVisitBonus += LINK_VISIT_BONUS;

  let visitedPromise = promiseVisitedWithFrecency(expectedRedirectSourceFrecency,
                                                  expectedTypedVisitBonus);

  let newTabPromise = BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  yield Promise.all([visitedPromise, newTabPromise]);

  gBrowser.removeCurrentTab();
});
