/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

/*
 * Test that defaultEngine property can be set and yields the proper events and\
 * behavior (search results)
 */

"use strict";

let engine1;
let engine2;

add_task(async function setup() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();

  await Services.search.init();

  engine1 = await SearchTestUtils.promiseNewSearchEngine(
    `${gDataUrl}engine.xml`
  );
  engine2 = await SearchTestUtils.promiseNewSearchEngine(
    `${gDataUrl}engine2.xml`
  );
});

function promiseDefaultNotification() {
  return SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.DEFAULT,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );
}

add_task(async function test_defaultEngine() {
  let promise = promiseDefaultNotification();
  Services.search.defaultEngine = engine1;
  Assert.equal((await promise).wrappedJSObject, engine1);
  Assert.equal(Services.search.defaultEngine.wrappedJSObject, engine1);

  promise = promiseDefaultNotification();
  Services.search.defaultEngine = engine2;
  Assert.equal((await promise).wrappedJSObject, engine2);
  Assert.equal(Services.search.defaultEngine.wrappedJSObject, engine2);

  promise = promiseDefaultNotification();
  Services.search.defaultEngine = engine1;
  Assert.equal((await promise).wrappedJSObject, engine1);
  Assert.equal(Services.search.defaultEngine.wrappedJSObject, engine1);
});

add_task(async function test_switch_with_invalid_overriddenBy() {
  engine1.wrappedJSObject.setAttr("overriddenBy", "random@id");

  consoleAllowList.push(
    "Test search engine had overriddenBy set, but no _overriddenData"
  );

  let promise = promiseDefaultNotification();
  Services.search.defaultEngine = engine2;
  Assert.equal((await promise).wrappedJSObject, engine2);
  Assert.equal(Services.search.defaultEngine.wrappedJSObject, engine2);
});
