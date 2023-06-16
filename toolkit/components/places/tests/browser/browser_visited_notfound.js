add_task(async function test() {
  const TEST_URL = "http://mochi.test:8888/notFoundPage.html";
  // Ensure that decay frecency doesn't kick in during tests (as a result
  // of idle-daily).
  Services.prefs.setCharPref("places.frecency.decayRate", "1.0");
  let tab = await BrowserTestUtils.openNewForegroundTab(gBrowser);
  registerCleanupFunction(async function () {
    Services.prefs.clearUserPref("places.frecency.decayRate");
    BrowserTestUtils.removeTab(tab);
    await PlacesUtils.history.clear();
  });

  // First add a visit to the page, this will ensure that later we skip
  // updating the frecency for a newly not-found page.
  await PlacesTestUtils.addVisits({ uri: TEST_URL });
  let frecency = await PlacesTestUtils.getDatabaseValue(
    "moz_places",
    "frecency",
    { url: TEST_URL }
  );
  is(frecency, 100, "Check initial frecency");

  // Used to verify errors are not marked as typed.
  PlacesUtils.history.markPageAsTyped(NetUtil.newURI(TEST_URL));

  let promiseVisit = new Promise(resolve => {
    function onVisits(events) {
      PlacesObservers.removeListener(["page-visited"], onVisits);
      is(events.length, 1, "Right number of visits");
      is(events[0].type, "page-visited");
      is(events[0].url, TEST_URL, "Check visited url");
      resolve();
    }
    PlacesObservers.addListener(["page-visited"], onVisits);
  });
  BrowserTestUtils.loadURIString(gBrowser.selectedBrowser, TEST_URL);
  await promiseVisit;

  is(
    await PlacesTestUtils.getDatabaseValue("moz_places", "frecency", {
      url: TEST_URL,
    }),
    frecency,
    "Frecency should be unchanged"
  );
  is(
    await PlacesTestUtils.getDatabaseValue("moz_places", "hidden", {
      url: TEST_URL,
    }),
    0,
    "Page should not be hidden"
  );
  is(
    await PlacesTestUtils.getDatabaseValue("moz_places", "typed", {
      url: TEST_URL,
    }),
    0,
    "page should not be marked as typed"
  );
});
