/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_location_error() {
  // We use an invalid port that parses but won't open
  let url = "http://localhost:0";

  Services.prefs.setCharPref("browser.search.geoip.url", url);
  await Services.search.init();
  try {
    Services.prefs.getCharPref("browser.search.region");
    ok(false, "not expecting region to be set");
  } catch (ex) {}
  // should have an error recorded.
  checkCountryResultTelemetry(TELEMETRY_RESULT_ENUM.ERROR);
  // but false values for timeout and forced-sync-init.
  for (let hid of ["SEARCH_SERVICE_COUNTRY_TIMEOUT",
                   "SEARCH_SERVICE_COUNTRY_FETCH_CAUSED_SYNC_INIT"]) {
    let histogram = Services.telemetry.getHistogramById(hid);
    let snapshot = histogram.snapshot();
    deepEqual(snapshot.values, {0: 1, 1: 0}); // boolean probe so 3 buckets, expect 1 result for |0|.
  }
});
