const TEST_PATH = getRootDirectory(gTestPath).replace("chrome://mochitests/content", "http://example.com");

add_task(async function checkTitleNotificationForNavigation() {
  const EXPECTED_URL = Services.io.newURI(TEST_PATH + "empty_page.html");
  let promiseTitleChanged = new Promise(resolve => {
    let obs = {
      onVisits(aVisits) {
        Assert.equal(aVisits.length, 1, "Right number of visits notified");
        let {
          uri,
          lastKnownTitle,
        } = aVisits[0];
        info("onVisits: " + uri.spec);
        if (uri.equals(EXPECTED_URL)) {
          Assert.equal(lastKnownTitle, null, "Should not have a title");
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
    PlacesUtils.history.addObserver(obs);
  });
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, EXPECTED_URL.spec);
  await promiseTitleChanged;
  BrowserTestUtils.removeTab(tab);
});
