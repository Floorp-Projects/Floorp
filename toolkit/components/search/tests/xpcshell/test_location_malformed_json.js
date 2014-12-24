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
    // should have a false value for success.
    let histogram = Services.telemetry.getHistogramById("SEARCH_SERVICE_COUNTRY_SUCCESS");
    let snapshot = histogram.snapshot();
    equal(snapshot.sum, 0);

    // and a flag for SEARCH_SERVICE_COUNTRY_SUCCESS_WITHOUT_DATA
    histogram = Services.telemetry.getHistogramById("SEARCH_SERVICE_COUNTRY_SUCCESS_WITHOUT_DATA");
    snapshot = histogram.snapshot();
    equal(snapshot.sum, 1);

    do_test_finished();
    run_next_test();
  });
  do_test_pending();
}
