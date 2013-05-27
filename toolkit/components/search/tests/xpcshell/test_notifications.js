/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const Ci = Components.interfaces;

Components.utils.import("resource://testing-common/httpd.js");

let gTestLog = [];

/**
 * The order of notifications expected for this test is:
 *  - engine-changed (while we're installing the engine, we modify it, which notifies - bug 606886)
 *  - engine-added (engine was added to the store by the search service)
 *   -> our search observer is called, which sets:
 *    - .defaultEngine, triggering engine-default
 *    - .currentEngine, triggering engine-current (after bug 493051 - for now the search service sets this after engine-added)
 *   ...and then schedules a removal
 *  - engine-loaded (the search service's observer is garanteed to fire first, which is what causes engine-added to fire)
 *  - engine-removed (due to the removal schedule above)
 */
let expectedLog = [
  "engine-changed", // XXX bug 606886
  "engine-added",
  "engine-default",
  "engine-current",
  "engine-loaded",
  "engine-removed"
];

function search_observer(subject, topic, data) {
  let engine = subject.QueryInterface(Ci.nsISearchEngine);
  gTestLog.push(data + " for " + engine.name);

  do_print("Observer: " + data + " for " + engine.name);

  switch (data) {
    case "engine-added":
      let retrievedEngine = Services.search.getEngineByName("Test search engine");
      do_check_eq(engine, retrievedEngine);
      Services.search.defaultEngine = engine;
      // XXX bug 493051
      // Services.search.currentEngine = engine;
      do_execute_soon(function () {
        Services.search.removeEngine(engine);
      });
      break;
    case "engine-removed":
      let engineNameOutput = " for Test search engine";
      expectedLog = expectedLog.map(logLine => logLine + engineNameOutput);
      do_print("expectedLog:\n" + expectedLog.join("\n"))
      do_print("gTestLog:\n" + gTestLog.join("\n"))
      for (let i = 0; i < expectedLog.length; i++) {
        do_check_eq(gTestLog[i], expectedLog[i]);
      }
      do_check_eq(gTestLog.length, expectedLog.length);
      do_test_finished();
      break;
  }
}

function run_test() {
  removeMetadata();
  updateAppInfo();

  let httpServer = new HttpServer();
  httpServer.start(4444);
  httpServer.registerDirectory("/", do_get_cwd());

  do_register_cleanup(function cleanup() {
    httpServer.stop(function() {});
    Services.obs.removeObserver(search_observer, "browser-search-engine-modified");
  });

  do_test_pending();

  Services.obs.addObserver(search_observer, "browser-search-engine-modified", false);

  Services.search.addEngine("http://localhost:4444/data/engine.xml",
                            Ci.nsISearchEngine.DATA_XML,
                            null, false);
}
