/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that currentEngine and defaultEngine properties can be set and yield the
 * proper events and behavior (search results)
 */

"use strict";

function run_test() {
  removeMetadata();
  updateAppInfo();
  useHttpServer();

  run_next_test();
}

add_task(function* test_defaultEngine() {
  let search = Services.search;

  let originalDefault = search.defaultEngine;

  let [engine1, engine2] = yield addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
    { name: "A second test engine", xmlFileName: "engine2.xml" },
  ]);

  search.defaultEngine = engine1;
  do_check_eq(search.defaultEngine, engine1);
  search.defaultEngine = engine2
  do_check_eq(search.defaultEngine, engine2);
  search.defaultEngine = engine1;
  do_check_eq(search.defaultEngine, engine1);

  // Test that hiding the currently-default engine affects the defaultEngine getter
  // We fallback first to the original default...
  engine1.hidden = true;
  do_check_eq(search.defaultEngine, originalDefault);

  // ... and then to the first visible engine in the list, so move our second
  // engine to that position.
  search.moveEngine(engine2, 0);
  originalDefault.hidden = true;
  do_check_eq(search.defaultEngine, engine2);

  // Test that setting defaultEngine to an already-hidden engine works, but
  // doesn't change the return value of the getter
  search.defaultEngine = engine1;
  do_check_eq(search.defaultEngine, engine2);
});
