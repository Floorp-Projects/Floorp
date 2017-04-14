const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");

add_task(function* checkTitleNotificationForNavigation() {
  const EXPECTED_URL = Services.io.newURI(TEST_PATH + "empty_page.html");
  let promiseTitleChanged = new Promise(resolve => {
    let obs = {
      onVisit(aURI, aVisitId, aTime, aSessionId, aReferrerVisitId, aTransitionType,
              aGuid, aHidden, aVisitCount, aTyped, aLastKnownTitle) {
        info("onVisit: " + aURI.spec);
        if (aURI.equals(EXPECTED_URL)) {
          Assert.equal(aLastKnownTitle, null, "Should not have a title");
        }
      },

      onTitleChanged(aURI, aTitle, aGuid) {
        if (aURI.equals(EXPECTED_URL)) {
          is(aTitle, "I am an empty page", "Should have correct title in titlechanged notification");
          PlacesUtils.history.removeObserver(obs);
          resolve();
        }
      },
    };
    PlacesUtils.history.addObserver(obs, false);
  });
  let tab = yield BrowserTestUtils.openNewForegroundTab(gBrowser, EXPECTED_URL.spec);
  yield promiseTitleChanged;
  yield BrowserTestUtils.removeTab(tab);
});
