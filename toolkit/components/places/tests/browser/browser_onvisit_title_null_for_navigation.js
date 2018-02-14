const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");

add_task(async function checkTitleNotificationForNavigation() {
  const EXPECTED_URL = Services.io.newURI(TEST_PATH + "empty_page.html");
  let promiseTitleChanged = new Promise(resolve => {
    function onVisits(aEvents) {
      Assert.equal(aEvents.length, 1, "Right number of visits notified");
      Assert.equal(aEvents[0].type, "page-visited");
      let {
        url,
        lastKnownTitle,
      } = aEvents[0];
      info("'page-visited': " + url);
      if (url == EXPECTED_URL.spec) {
        Assert.equal(lastKnownTitle, null, "Should not have a title");
      }
      PlacesObservers.removeListener(["page-visited"], onVisits);
    }
    let obs = {
      onTitleChanged(aURI, aTitle, aGuid) {
        if (aURI.equals(EXPECTED_URL)) {
          is(aTitle, "I am an empty page", "Should have correct title in titlechanged notification");
          PlacesUtils.history.removeObserver(obs);
          resolve();
        }
      },
    };
    PlacesUtils.history.addObserver(obs);
    PlacesObservers.addListener(["page-visited"], onVisits);
  });
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, EXPECTED_URL.spec);
  await promiseTitleChanged;
  BrowserTestUtils.removeTab(tab);
});
