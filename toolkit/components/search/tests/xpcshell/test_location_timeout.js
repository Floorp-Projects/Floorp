/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function startServer() {
  let srv = new HttpServer();
  function lookupCountry(metadata, response) {
    response.processAsync();
    // wait 200 ms before writing a valid response - the search service
    // should timeout before the response is written so the response should
    // be ignored.
    do_timeout(200, () => {
      response.setStatusLine("1.1", 200, "OK");
      response.write('{"country_code" : "AU"}');
    });
  }
  srv.registerPathHandler("/lookup_country", lookupCountry);
  srv.start(-1);
  return srv;
}

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

  let server = startServer();
  let url = "http://localhost:" + server.identity.primaryPort + "/lookup_country";
  Services.prefs.setCharPref("browser.search.geoip.url", url);
  Services.prefs.setIntPref("browser.search.geoip.timeout", 50);
  Services.search.init(() => {
    try {
      Services.prefs.getCharPref("browser.search.countryCode");
      ok(false, "not expecting countryCode to be set");
    } catch (ex) {}
    // should be no success recorded.
    let histogram = Services.telemetry.getHistogramById("SEARCH_SERVICE_COUNTRY_SUCCESS");
    let snapshot = histogram.snapshot();
    equal(snapshot.sum, 0);

    // should be a timeout.
    histogram = Services.telemetry.getHistogramById("SEARCH_SERVICE_COUNTRY_FETCH_TIMEOUT");
    snapshot = histogram.snapshot();
    equal(snapshot.sum, 1);

    do_test_finished();
    server.stop(run_next_test);
  });
  do_test_pending();
}
