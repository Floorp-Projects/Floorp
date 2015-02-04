/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  installTestEngine();

  Services.prefs.setCharPref("browser.search.geoip.url", 'data:application/json,{"country_code": "AU"}');
  Services.search.init(() => {
    equal(Services.prefs.getCharPref("browser.search.countryCode"), "AU", "got the correct country code.");
    equal(Services.prefs.getCharPref("browser.search.region"), "AU", "region pref also set to the countryCode.")
    // No isUS pref is ever written
    ok(!Services.prefs.prefHasUserValue("browser.search.isUS"), "no isUS pref")
    // check we have "success" recorded in telemetry
    checkCountryResultTelemetry(TELEMETRY_RESULT_ENUM.SUCCESS);
    // a false value for each of SEARCH_SERVICE_COUNTRY_TIMEOUT and SEARCH_SERVICE_COUNTRY_FETCH_CAUSED_SYNC_INIT
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
