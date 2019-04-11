/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const kUrlPref = "geoSpecificDefaults.url";

add_task(async function setup() {
  configureToLoadJarEngines();

  // Geo specific defaults won't be fetched if there's no country code.
  Services.prefs.setCharPref("browser.search.geoip.url",
                             'data:application/json,{"country_code": "US"}');

  // Make 'hidden' the only visible engine.
  let url = "data:application/json," + JSON.stringify({
    "interval": 31536000,
    "settings": {
      "searchDefault": "hidden",
      "visibleDefaultEngines": ["hidden"],
    },
  });
  Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF).setCharPref(kUrlPref, url);

  Assert.ok(!Services.search.isInitialized);

  await AddonTestUtils.promiseStartupManager();
});

add_task(async function async_init() {
  let commitPromise = promiseAfterCache();
  await Services.search.init();

  let engines = await Services.search.getEngines();
  Assert.equal(engines.length, 1);

  // The default test jar engine has been hidden.
  let engine = Services.search.getEngineByName("bug645970");
  Assert.equal(engine, null);

  // The hidden engine is visible.
  engine = Services.search.getEngineByName("hidden");
  Assert.notEqual(engine, null);

  // The next test does a sync init, which won't do the geoSpecificDefaults XHR,
  // so it depends on the metadata having been written to disk.
  await commitPromise;
});

add_task(async function invalid_engine() {
  // Trigger a new request.
  await forceExpiration();

  // Set the visibleDefaultEngines list to something that contains a non-existent engine.
  // This should cause the search service to ignore the list altogether and fallback to
  // local defaults.
  let url = "data:application/json," + JSON.stringify({
    "interval": 31536000,
    "settings": {
      "searchDefault": "hidden",
      "visibleDefaultEngines": ["hidden", "bogus"],
    },
  });
  Services.prefs.getDefaultBranch(BROWSER_SEARCH_PREF).setCharPref(kUrlPref, url);

  await asyncReInit({ waitForRegionFetch: true });

  let engines = await Services.search.getEngines();
  Assert.equal(engines.length, 1);

  // The default test jar engine is visible.
  let engine = Services.search.getEngineByName("bug645970");
  Assert.notEqual(engine, null);

  // The hidden engine is... hidden.
  engine = Services.search.getEngineByName("hidden");
  Assert.equal(engine, null);
});
