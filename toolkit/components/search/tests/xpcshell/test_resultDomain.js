/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Tests getResultDomain API.
 */

"use strict";

const Ci = Components.interfaces;

Components.utils.import("resource://testing-common/httpd.js");

let waitForEngines = new Set([ "Test search engine",
                               "A second test engine",
                               "bacon" ]);

function promiseEnginesAdded() {
  let deferred = Promise.defer();

  let observe = function observe(aSubject, aTopic, aData) {
    let engine = aSubject.QueryInterface(Ci.nsISearchEngine);
    do_print("Observer: " + aData + " for " + engine.name);
    if (aData != "engine-added") {
      return;
    }
    waitForEngines.delete(engine.name);
    if (waitForEngines.size > 0) {
      return;
    }

    let engine1 = Services.search.getEngineByName("Test search engine");
    do_check_eq(engine1.getResultDomain(), "google.com");
    do_check_eq(engine1.getResultDomain("text/html"), "google.com");
    do_check_eq(engine1.getResultDomain("application/x-moz-default-purpose"),
                "purpose.google.com");
    do_check_eq(engine1.getResultDomain("fake-response-type"), "");
    let engine2 = Services.search.getEngineByName("A second test engine");
    do_check_eq(engine2.getResultDomain(), "duckduckgo.com");
    let engine3 = Services.search.getEngineByName("bacon");
    do_check_eq(engine3.getResultDomain(), "bacon.moz");
    deferred.resolve();
  };

  Services.obs.addObserver(observe, "browser-search-engine-modified", false);
  do_register_cleanup(function cleanup() {
    Services.obs.removeObserver(observe, "browser-search-engine-modified");
  });

  return deferred.promise;
}

function run_test() {
  removeMetadata();
  updateAppInfo();

  run_next_test();
}

add_task(function* check_resultDomain() {
  let httpServer = new HttpServer();
  httpServer.start(-1);
  httpServer.registerDirectory("/", do_get_cwd());
  let baseUrl = "http://localhost:" + httpServer.identity.primaryPort;
  do_register_cleanup(function cleanup() {
    httpServer.stop(function() {});
  });

  let promise = promiseEnginesAdded();
  Services.search.addEngine(baseUrl + "/data/engine.xml",
                            Ci.nsISearchEngine.DATA_XML,
                            null, false);
  Services.search.addEngine(baseUrl + "/data/engine2.xml",
                            Ci.nsISearchEngine.DATA_XML,
                            null, false);
  Services.search.addEngineWithDetails("bacon", "", "bacon", "Search Bacon",
                                       "GET", "http://www.bacon.moz/?search={searchTerms}");
  yield promise;
});
