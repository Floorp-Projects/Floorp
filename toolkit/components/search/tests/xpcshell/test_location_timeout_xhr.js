/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// This is testing the long, last-resort XHR-based timeout for the location
// search.

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

function verifyProbeSum(probe, sum) {
  let histogram = Services.telemetry.getHistogramById(probe);
  let snapshot = histogram.snapshot();
  equal(snapshot.sum, sum, probe);
}

add_task(async function setup() {
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_location_timeout_xhr() {
  let resolveContinuePromise;
  let continuePromise = new Promise(resolve => {
    resolveContinuePromise = resolve;
  });

  let server = startServer(continuePromise);
  let url =
    "http://localhost:" + server.identity.primaryPort + "/lookup_country";
  Services.prefs.setCharPref("browser.search.geoip.url", url);
  // The timeout for the timer.
  Services.prefs.setIntPref("browser.search.geoip.timeout", 10);
  let promiseXHRStarted = SearchTestUtils.promiseSearchNotification(
    "geoip-lookup-xhr-starting"
  );
  await Services.search.init();
  ok(
    !Services.prefs.prefHasUserValue("browser.search.region"),
    "should be no region pref"
  );
  // should be no result recorded at all.
  checkCountryResultTelemetry(null);

  // should not have SEARCH_SERVICE_COUNTRY_FETCH_TIME_MS recorded as our
  // test server is still blocked on our promise.
  verifyProbeSum("SEARCH_SERVICE_COUNTRY_FETCH_TIME_MS", 0);

  promiseXHRStarted.then(xhr => {
    // Set the timeout on the xhr object to an extremely low value, so it
    // should timeout immediately.
    xhr.timeout = 10;
    // wait for the xhr timeout to fire.
    SearchTestUtils.promiseSearchNotification("geoip-lookup-xhr-complete").then(
      () => {
        // should have the XHR timeout recorded.
        checkCountryResultTelemetry(TELEMETRY_RESULT_ENUM.TIMEOUT);
        // still should not have a report of how long the response took as we
        // only record that on success responses.
        verifyProbeSum("SEARCH_SERVICE_COUNTRY_FETCH_TIME_MS", 0);
        // and we still don't know the country code or region.
        ok(
          !Services.prefs.prefHasUserValue("browser.search.region"),
          "should be no region pref"
        );

        // unblock the server even though nothing is listening.
        resolveContinuePromise();

        return new Promise(resolve => {
          server.stop(resolve);
        });
      }
    );
  });
});
