/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Here we are testing the "migration" when both isUS and countryCode are
// set.
function run_test() {
  installTestEngine();

  // Set the prefs we care about.
  Services.prefs.setBoolPref("browser.search.isUS", true);
  Services.prefs.setCharPref("browser.search.countryCode", "US");
  // And the geoip request that will return AU - it shouldn't be used.
  Services.prefs.setCharPref("browser.search.geoip.url", 'data:application/json,{"country_code": "AU"}');
  Services.search.init(() => {
    // "region" and countryCode should still reflect US.
    equal(Services.prefs.getCharPref("browser.search.region"), "US", "got the correct region.");
    equal(Services.prefs.getCharPref("browser.search.countryCode"), "US", "got the correct country code.");
    // should be no geoip evidence.
    checkCountryResultTelemetry(null);
    do_test_finished();
    run_next_test();
  });
  do_test_pending();
}
