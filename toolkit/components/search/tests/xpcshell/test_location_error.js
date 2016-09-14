/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  installTestEngine();

  // using a port > 2^32 causes an error to be reported.
  let url = "http://localhost:111111111";

  Services.prefs.setCharPref("browser.search.geoip.url", url);
  Services.search.init(() => {
    try {
      Services.prefs.getCharPref("browser.search.countryCode");
      ok(false, "not expecting countryCode to be set");
    } catch (ex) {}
    // should have an error recorded.
    checkCountryResultTelemetry(TELEMETRY_RESULT_ENUM.ERROR);
    // but false values for timeout and forced-sync-init.
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
