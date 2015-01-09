/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  removeMetadata();
  removeCacheFile();

  do_check_false(Services.search.isInitialized);

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

  // Here we have malformed JSON
  Services.prefs.setCharPref("browser.search.geoip.url", 'data:application/json,{"country_code"');
  Services.search.init(() => {
    try {
      Services.prefs.getCharPref("browser.search.countryCode");
      ok(false, "should be no countryCode pref");
    } catch (_) {}
    try {
      Services.prefs.getCharPref("browser.search.isUS");
      ok(false, "should be no isUS pref yet either");
    } catch (_) {}
    // fetch the engines - this should force the timezone check
    Services.search.getEngines();
    equal(Services.prefs.getBoolPref("browser.search.isUS"),
          isUSTimezone(),
          "should have set isUS based on current timezone.");
    // should have recorded SUCCESS_WITHOUT_DATA
    checkCountryResultTelemetry(TELEMETRY_RESULT_ENUM.SUCCESS_WITHOUT_DATA);
    // and false values for timeout and forced-sync-init.
    for (let hid of ["SEARCH_SERVICE_COUNTRY_TIMEOUT",
                     "SEARCH_SERVICE_COUNTRY_FETCH_CAUSED_SYNC_INIT"]) {
      let histogram = Services.telemetry.getHistogramById(hid);
      let snapshot = histogram.snapshot();
      deepEqual(snapshot.counts, [1,0,0]); // boolean probe so 3 buckets, expect 1 result for |0|.
    }

    do_test_finished();
    run_next_test();
  });
  do_test_pending();
}
