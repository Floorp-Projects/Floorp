add_task(async function test() {
  const TEST_URL = "http://mochi.test:8888/notFoundPage.html";
  // Ensure that decay frecency doesn't kick in during tests (as a result
  // of idle-daily).
  Services.prefs.setCharPref("places.frecency.decayRate", "1.0");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  registerCleanupFunction(async function() {
    Services.prefs.clearUserPref("places.frecency.decayRate");
    await BrowserTestUtils.removeTab(tab);
    await PlacesTestUtils.clearHistory();
  });

  // First add a visit to the page, this will ensure that later we skip
  // updating the frecency for a newly not-found page.
  await PlacesTestUtils.addVisits({ uri: TEST_URL });
  let frecency = await PlacesTestUtils.fieldInDB(TEST_URL, "frecency");
  is(frecency, 100, "Check initial frecency");

  // Used to verify errors are not marked as typed.
  PlacesUtils.history.markPageAsTyped(NetUtil.newURI(TEST_URL));

  let promiseVisit = new Promise(resolve => {
    let historyObserver = {
      __proto__: NavHistoryObserver.prototype,
      onVisits(visits) {
        PlacesUtils.history.removeObserver(historyObserver);
        is(visits.length, 1, "Right number of visits");
        is(visits[0].uri.spec, TEST_URL, "Check visited url");
        resolve();
      }
    };
    PlacesUtils.history.addObserver(historyObserver);
  });
  gBrowser.selectedBrowser.loadURI(TEST_URL);
  await promiseVisit;

  is(await PlacesTestUtils.fieldInDB(TEST_URL, "frecency"), frecency, "Frecency should be unchanged");
  is(await PlacesTestUtils.fieldInDB(TEST_URL, "hidden"), 0, "Page should not be hidden");
  is(await PlacesTestUtils.fieldInDB(TEST_URL, "typed"), 0, "page should not be marked as typed");
});
