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

function promiseDefaultNotification() {
  return SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.DEFAULT,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
}

add_task(async function test_defaultEngine() {
  let search = Services.search;
  await search.init();

  let originalDefault = search.defaultEngine;

  let [engine1, engine2] = await addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
    { name: "A second test engine", xmlFileName: "engine2.xml" },
  ]);

  let promise = promiseDefaultNotification();
  search.defaultEngine = engine1;
  Assert.equal(await promise, engine1);
  Assert.equal(search.defaultEngine, engine1);

  promise = promiseDefaultNotification();
  search.defaultEngine = engine2;
  Assert.equal(await promise, engine2);
  Assert.equal(search.defaultEngine, engine2);

  promise = promiseDefaultNotification();
  search.defaultEngine = engine1;
  Assert.equal(await promise, engine1);
  Assert.equal(search.defaultEngine, engine1);

  // Test that hiding the currently-default engine affects the defaultEngine getter
  // We fallback first to the original default...
  engine1.hidden = true;
  Assert.equal(search.defaultEngine, originalDefault);

  // ... and then to the first visible engine in the list, so move our second
  // engine to that position.
  await search.moveEngine(engine2, 0);
  originalDefault.hidden = true;
  Assert.equal(search.defaultEngine, engine2);

  // Test that setting defaultEngine to an already-hidden engine works, but
  // doesn't change the return value of the getter
  promise = promiseDefaultNotification();
  search.defaultEngine = engine1;
  Assert.equal(await promise, engine1);
  Assert.equal(search.defaultEngine, engine2);
});
