/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that defaultEngine property can be set and yields the proper events and\
 * behavior (search results)
 */

"use strict";

add_task(async function setup() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
});

add_task(async function test_defaultPrivateEngine() {
  let search = Services.search;
  await search.init();

  let originalDefault = search.defaultPrivateEngine;

  let [engine1, engine2] = await addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
    { name: "A second test engine", xmlFileName: "engine2.xml" },
  ]);

  search.defaultPrivateEngine = engine1;
  Assert.equal(search.defaultPrivateEngine, engine1);
  search.defaultPrivateEngine = engine2;
  Assert.equal(search.defaultPrivateEngine, engine2);
  search.defaultPrivateEngine = engine1;
  Assert.equal(search.defaultPrivateEngine, engine1);

  // Test that hiding the currently-default engine affects the defaultEngine getter
  // We fallback first to the original default...
  engine1.hidden = true;
  Assert.equal(search.defaultPrivateEngine, originalDefault);

  // ... and then to the first visible engine in the list, so move our second
  // engine to that position.
  await search.moveEngine(engine2, 0);
  originalDefault.hidden = true;
  Assert.equal(search.defaultPrivateEngine, engine2);

  // Test that setting defaultEngine to an already-hidden engine works, but
  // doesn't change the return value of the getter
  search.defaultPrivateEngine = engine1;
  Assert.equal(search.defaultPrivateEngine, engine2);
});
