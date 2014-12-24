/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function getCountryCodePref() {
  try {
    return Services.prefs.getCharPref("browser.search.countryCode");
  } catch (_) {
    return undefined;
  }
}

function getIsUSPref() {
  try {
    return Services.prefs.getBoolPref("browser.search.isUS");
  } catch (_) {
    return undefined;
  }
}

function run_test() {
  removeMetadata();
  removeCacheFile();

  ok(!Services.search.isInitialized);

  let engineDummyFile = gProfD.clone();
  engineDummyFile.append("searchplugins");
  engineDummyFile.append("test-search-engine.xml");
  let engineDir = engineDummyFile.parent;
  engineDir.create(Ci.nsIFile.DIRECTORY_TYPE, FileUtils.PERMS_DIRECTORY);

  do_get_file("data/engine.xml").copyTo(engineDir, "engine.xml");

  do_register_cleanup(function() {
    removeMetadata();
    removeCacheFile();
  });

  run_next_test();
}

// Force a sync init and ensure the right thing happens (ie, that no xhr
// request is made and we fall back to the timezone-only trick)
add_task(function* test_simple() {
  deepEqual(getCountryCodePref(), undefined, "no countryCode pref");
  deepEqual(getIsUSPref(), undefined, "no isUS pref");

  // Still set a geoip pref so we can (indirectly) check it wasn't used.
  Services.prefs.setCharPref("browser.search.geoip.url", 'data:application/json,{"country_code": "AU"}');

  ok(!Services.search.isInitialized);

  // fetching the engines forces a sync init, and should have caused us to
  // check the timezone.
  let engines = Services.search.getEngines();
  ok(Services.search.isInitialized);
  deepEqual(getIsUSPref(), isUSTimezone(), "isUS pref was set by sync init.");

  // a little wait to check we didn't do the xhr thang.
  yield new Promise(resolve => {
    do_timeout(500, resolve);
  });

  deepEqual(getCountryCodePref(), undefined, "didn't do the geoip xhr");
  // and no telemetry evidence of geoip.
  for (let hid of [
    "SEARCH_SERVICE_COUNTRY_FETCH_MS",
    "SEARCH_SERVICE_COUNTRY_SUCCESS",
    "SEARCH_SERVICE_COUNTRY_SUCCESS_WITHOUT_DATA",
    "SEARCH_SERVICE_COUNTRY_FETCH_TIMEOUT",
    "SEARCH_SERVICE_US_COUNTRY_MISMATCHED_TIMEZONE",
    "SEARCH_SERVICE_US_TIMEZONE_MISMATCHED_COUNTRY",
    ]) {
      let histogram = Services.telemetry.getHistogramById(hid);
      let snapshot = histogram.snapshot();
      equal(snapshot.sum, 0);
    }
});
