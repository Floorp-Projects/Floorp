const REDIRECT_URI = NetUtil.newURI("http://mochi.test:8888/tests/toolkit/components/places/tests/browser/redirect_thrice.sjs");
const INTERMEDIATE_URI_1 = NetUtil.newURI("http://mochi.test:8888/tests/toolkit/components/places/tests/browser/redirect_twice_perma.sjs");
const INTERMEDIATE_URI_2 = NetUtil.newURI("http://mochi.test:8888/tests/toolkit/components/places/tests/browser/redirect_once.sjs");
const TARGET_URI = NetUtil.newURI("http://mochi.test:8888/tests/toolkit/components/places/tests/browser/final.html");

const REDIRECT_SOURCE_VISIT_BONUS =
  Services.prefs.getIntPref("places.frecency.redirectSourceVisitBonus");
// const LINK_VISIT_BONUS =
//   Services.prefs.getIntPref("places.frecency.linkVisitBonus");
// const TYPED_VISIT_BONUS =
//   Services.prefs.getIntPref("places.frecency.typedVisitBonus");
const PERM_REDIRECT_VISIT_BONUS =
  Services.prefs.getIntPref("places.frecency.permRedirectVisitBonus");

// Ensure that decay frecency doesn't kick in during tests (as a result
// of idle-daily).
Services.prefs.setCharPref("places.frecency.decayRate", "1.0");

registerCleanupFunction(function*() {
  Services.prefs.clearUserPref("places.frecency.decayRate");
  yield PlacesTestUtils.clearHistory();
});

function promiseVisitedURIObserver(redirectURI, targetURI, expectedTargetFrecency) {
  // Create and add history observer.
  return new Promise(resolve => {
    let historyObserver = {
      _redirectNotified: false,
      onVisit(aURI, aVisitID, aTime, aSessionID, aReferringID,
                       aTransitionType) {
       info("Received onVisit: " + aURI.spec);

       if (aURI.equals(redirectURI)) {
         this._redirectNotified = true;
         // Wait for the target page notification.
         return;
       }

       if (!aURI.equals(targetURI)) {
         return;
       }

       PlacesUtils.history.removeObserver(historyObserver);

       ok(this._redirectNotified, "The redirect should have been notified");

       resolve();
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
    PlacesUtils.history.addObserver(historyObserver, false);
  });
}

function* testURIFields(url, expectedFrecency, expectedHidden) {
  let frecency = yield promiseFieldForUrl(url, "frecency");
  is(frecency, expectedFrecency,
     `Frecency of the page is the expected one (${url.spec})`);

  let hidden = yield promiseFieldForUrl(url, "hidden");
  is(hidden, expectedHidden, `The redirecting page should be hidden (${url.spec})`);
}

let expectedRedirectSourceFrecency = 0;
let expectedTargetFrecency = 0;

add_task(function* test_multiple_redirect() {
  // Used to verify the redirect bonus overrides the typed bonus.
  PlacesUtils.history.markPageAsTyped(REDIRECT_URI);

  expectedRedirectSourceFrecency += REDIRECT_SOURCE_VISIT_BONUS;
  // TODO Bug 487813 - This should be TYPED_VISIT_BONUS, however as we don't
  // currently track redirects across multiple redirects, we fallback to the
  // PERM_REDIRECT_VISIT_BONUS.
  expectedTargetFrecency += PERM_REDIRECT_VISIT_BONUS;

  let visitedURIPromise = promiseVisitedURIObserver(REDIRECT_URI, TARGET_URI, expectedTargetFrecency);

  let newTabPromise = BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  yield Promise.all([visitedURIPromise, newTabPromise]);

  yield testURIFields(REDIRECT_URI, expectedRedirectSourceFrecency, 1);
  yield testURIFields(INTERMEDIATE_URI_1, expectedRedirectSourceFrecency, 1);
  yield testURIFields(INTERMEDIATE_URI_2, expectedRedirectSourceFrecency, 1);
  yield testURIFields(TARGET_URI, expectedTargetFrecency, 0);

  gBrowser.removeCurrentTab();
});

add_task(function* redirect_check_second_typed_visit() {
  // A second visit with a typed url.
  PlacesUtils.history.markPageAsTyped(REDIRECT_URI);

  expectedRedirectSourceFrecency += REDIRECT_SOURCE_VISIT_BONUS;
  // TODO Bug 487813 - This should be TYPED_VISIT_BONUS, however as we don't
  // currently track redirects across multiple redirects, we fallback to the
  // PERM_REDIRECT_VISIT_BONUS.
  expectedTargetFrecency += PERM_REDIRECT_VISIT_BONUS;

  let visitedURIPromise = promiseVisitedURIObserver(REDIRECT_URI, TARGET_URI, expectedTargetFrecency);

  let newTabPromise = BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  yield Promise.all([visitedURIPromise, newTabPromise]);

  yield testURIFields(REDIRECT_URI, expectedRedirectSourceFrecency, 1);
  yield testURIFields(INTERMEDIATE_URI_1, expectedRedirectSourceFrecency, 1);
  yield testURIFields(INTERMEDIATE_URI_2, expectedRedirectSourceFrecency, 1);
  yield testURIFields(TARGET_URI, expectedTargetFrecency, 0);

  gBrowser.removeCurrentTab();
});


add_task(function* redirect_check_subsequent_link_visit() {
  // Another visit, but this time as a visited url.
  expectedRedirectSourceFrecency += REDIRECT_SOURCE_VISIT_BONUS;
  // TODO Bug 487813 - This should be LINK_VISIT_BONUS, however as we don't
  // currently track redirects across multiple redirects, we fallback to the
  // PERM_REDIRECT_VISIT_BONUS.
  expectedTargetFrecency += PERM_REDIRECT_VISIT_BONUS;

  let visitedURIPromise = promiseVisitedURIObserver(REDIRECT_URI, TARGET_URI, expectedTargetFrecency);

  let newTabPromise = BrowserTestUtils.openNewForegroundTab(gBrowser, REDIRECT_URI.spec);
  yield Promise.all([visitedURIPromise, newTabPromise]);

  yield testURIFields(REDIRECT_URI, expectedRedirectSourceFrecency, 1);
  yield testURIFields(INTERMEDIATE_URI_1, expectedRedirectSourceFrecency, 1);
  yield testURIFields(INTERMEDIATE_URI_2, expectedRedirectSourceFrecency, 1);
  yield testURIFields(TARGET_URI, expectedTargetFrecency, 0);

  gBrowser.removeCurrentTab();
});
