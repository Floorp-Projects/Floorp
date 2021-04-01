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

  let engine1 = await SearchTestUtils.promiseNewSearchEngine(
    `${gDataUrl}engine.xml`
  );
  let engine2 = await SearchTestUtils.promiseNewSearchEngine(
    `${gDataUrl}engine2.xml`
  );

  let promise = promiseDefaultNotification();
  search.defaultEngine = engine1;
  Assert.equal((await promise).wrappedJSObject.wrappedJSObject, engine1);
  Assert.equal(search.defaultEngine.wrappedJSObject, engine1);

  promise = promiseDefaultNotification();
  search.defaultEngine = engine2;
  Assert.equal((await promise).wrappedJSObject, engine2);
  Assert.equal(search.defaultEngine.wrappedJSObject, engine2);

  promise = promiseDefaultNotification();
  search.defaultEngine = engine1;
  Assert.equal((await promise).wrappedJSObject, engine1);
  Assert.equal(search.defaultEngine.wrappedJSObject, engine1);
});
