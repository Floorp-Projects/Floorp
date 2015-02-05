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

    // simple checks for our platform-specific telemetry.  We can't influence
    // what they return (as we can't influence the countryCode the platform
    // thinks we are in), but we can check the values are correct given reality.
    // NOTE: head_search.js mocks the XULRuntime values, but saves the original
    // OS in an OS global
    if (Services.appinfo.OS == "Darwin") {
      let gfxInfo2 = Cc["@mozilla.org/gfx/info;1"].getService(Ci.nsIGfxInfo2);
      print("OSX says the country-code is", gfxInfo2.countryCode);
      let expectedResult;
      let hid;
      // We know geoip said AU - if mac thinks US then we expect
      // SEARCH_SERVICE_US_COUNTRY_MISMATCHED_PLATFORM_OSX with true (ie, a mismatch)
      if (gfxInfo2.countryCode == "US") {
        hid = "SEARCH_SERVICE_US_COUNTRY_MISMATCHED_PLATFORM_OSX";
        expectedResult = [0,1,0]; // boolean probe so 3 buckets, expect 1 result for |1|.
      } else {
        // We are expecting SEARCH_SERVICE_NONUS_COUNTRY_MISMATCHED_PLATFORM_OSX
        // with false if OSX says AU (not a mismatch) and true otherwise.
        hid = "SEARCH_SERVICE_NONUS_COUNTRY_MISMATCHED_PLATFORM_OSX";
        expectedResult = gfxInfo2.countryCode == "AU" ? [1,0,0] : [0,1,0];
      }

      let histogram = Services.telemetry.getHistogramById(hid);
      let snapshot = histogram.snapshot();
      deepEqual(snapshot.counts, expectedResult);
    }
    do_test_finished();
    run_next_test();
  });
  do_test_pending();
}
