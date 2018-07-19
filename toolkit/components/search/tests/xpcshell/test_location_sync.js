/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function getRegionPref() {
  try {
    return Services.prefs.getCharPref("browser.search.region");
  } catch (_) {
    return undefined;
  }
}

// Force a sync init and ensure the right thing happens (ie, that no xhr
// request is made )
add_task(async function test_simple() {
  deepEqual(getRegionPref(), undefined, "no region pref");

  // Still set a geoip pref so we can (indirectly) check it wasn't used.
  Services.prefs.setCharPref("browser.search.geoip.url", 'data:application/json,{"country_code": "AU"}');

  ok(!Services.search.isInitialized);

  // fetching the engines forces a sync init
  Services.search.getEngines();
  ok(Services.search.isInitialized);

  // a little wait to check we didn't do the xhr thang.
  await new Promise(resolve => {
    do_timeout(500, resolve);
  });

  deepEqual(getRegionPref(), undefined, "didn't do the geoip xhr");
  // and no telemetry evidence of geoip.
  for (let hid of [
    "SEARCH_SERVICE_COUNTRY_FETCH_RESULT",
    "SEARCH_SERVICE_COUNTRY_FETCH_TIME_MS",
    "SEARCH_SERVICE_COUNTRY_TIMEOUT",
    "SEARCH_SERVICE_US_COUNTRY_MISMATCHED_TIMEZONE",
    "SEARCH_SERVICE_US_TIMEZONE_MISMATCHED_COUNTRY",
    "SEARCH_SERVICE_COUNTRY_FETCH_CAUSED_SYNC_INIT",
    ]) {
      let histogram = Services.telemetry.getHistogramById(hid);
      let snapshot = histogram.snapshot();
      equal(snapshot.sum, 0, hid);
      switch (snapshot.histogram_type) {
        case Ci.nsITelemetry.HISTOGRAM_FLAG:
          // flags are a special case in that they are initialized with a default
          // of one |0|.
          deepEqual(snapshot.counts, [1, 0, 0], hid);
          break;
        case Ci.nsITelemetry.HISTOGRAM_BOOLEAN:
          // booleans aren't initialized at all, so should have all zeros.
          deepEqual(snapshot.counts, [0, 0, 0], hid);
          break;
        case Ci.nsITelemetry.HISTOGRAM_EXPONENTIAL:
        case Ci.nsITelemetry.HISTOGRAM_LINEAR:
          equal(snapshot.counts.reduce((a, b) => a + b), 0, hid);
          break;
        default:
          ok(false, "unknown histogram type " + snapshot.histogram_type + " for " + hid);
      }
    }
});
