/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This is testing the "normal" timer-based timeout for the location search.

function startServer(continuePromise) {
  let srv = new HttpServer();
  function lookupCountry(metadata, response) {
    response.processAsync();
    // wait for our continuePromise to resolve before writing a valid
    // response.
    // This will be resolved after the timeout period, so we can check
    // the behaviour in that case.
    continuePromise.then(() => {
      response.setStatusLine("1.1", 200, "OK");
      response.write('{"country_code" : "AU"}');
      response.finish();
    });
  }
  srv.registerPathHandler("/lookup_country", lookupCountry);
  srv.start(-1);
  return srv;
}

function getProbeSum(probe, sum) {
  let histogram = Services.telemetry.getHistogramById(probe);
  return histogram.snapshot().sum;
}

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
  Services.prefs.setBoolPref("browser.search.geoSpecificDefaults", true);
});

add_task(async function test_location_timeout() {
  let resolveContinuePromise;
  let continuePromise = new Promise(resolve => {
    resolveContinuePromise = resolve;
  });

  let server = startServer(continuePromise);
  let url =
    "http://localhost:" + server.identity.primaryPort + "/lookup_country";
  Services.prefs.setCharPref("browser.region.network.url", url);
  Services.prefs.setIntPref("browser.region.network.timeout", 50);
  await Services.search.init();
  ok(
    !Services.prefs.prefHasUserValue("browser.search.region"),
    "should be no region pref"
  );
  // should be no result recorded at all.
  checkCountryResultTelemetry(null);

  // should not yet have SEARCH_SERVICE_COUNTRY_FETCH_TIME_MS recorded as our
  // test server is still blocked on our promise.
  equal(getProbeSum("SEARCH_SERVICE_COUNTRY_FETCH_TIME_MS"), 0);

  // now tell the server to send its response.  That will end up causing the
  // search service to notify of that the response was received.
  resolveContinuePromise();
  await SearchTestUtils.promiseSearchNotification("engines-reloaded");

  // now we *should* have a report of how long the response took even though
  // it timed out.
  // The telemetry "sum" will be the actual time in ms - just check it's non-zero.
  ok(getProbeSum("SEARCH_SERVICE_COUNTRY_FETCH_TIME_MS") != 0);
  // should have reported the fetch ended up being successful.
  checkCountryResultTelemetry(TELEMETRY_RESULT_ENUM.SUCCESS);

  // and should have the result of the response that finally came in, and
  // everything dependent should also be updated.
  equal(Services.prefs.getCharPref("browser.search.region"), "AU");

  await new Promise(resolve => server.stop(resolve));
});
