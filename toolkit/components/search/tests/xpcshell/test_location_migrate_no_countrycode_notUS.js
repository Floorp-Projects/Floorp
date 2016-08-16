/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Here we are testing the "migration" from the isUS pref being false but when
// no country-code exists.
function run_test() {
  installTestEngine();

  // Set the pref we care about.
  Services.prefs.setBoolPref("browser.search.isUS", false);
  // And the geoip request that will return US.
  Services.prefs.setCharPref("browser.search.geoip.url", 'data:application/json,{"country_code": "US"}');
  Services.search.init(() => {
    // "region" and countryCode should reflect US.
    equal(Services.prefs.getCharPref("browser.search.region"), "US", "got the correct region.");
    equal(Services.prefs.getCharPref("browser.search.countryCode"), "US", "got the correct country code.");
    // check we have "success" recorded in telemetry
    checkCountryResultTelemetry(TELEMETRY_RESULT_ENUM.SUCCESS);
    // a false value for each of SEARCH_SERVICE_COUNTRY_TIMEOUT and SEARCH_SERVICE_COUNTRY_FETCH_CAUSED_SYNC_INIT
    for (let hid of ["SEARCH_SERVICE_COUNTRY_TIMEOUT",
                     "SEARCH_SERVICE_COUNTRY_FETCH_CAUSED_SYNC_INIT"]) {
      let histogram = Services.telemetry.getHistogramById(hid);
      let snapshot = histogram.snapshot();
      deepEqual(snapshot.counts, [1, 0, 0]); // boolean probe so 3 buckets, expect 1 result for |0|.
    }
    do_test_finished();
    run_next_test();
  });
  do_test_pending();
}
