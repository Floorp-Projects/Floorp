/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/* Test that user-set metadata isn't lost on engine update */

"use strict";

add_task(async function setup() {
  useHttpServer();
  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
});

add_task(async function test_engineUpdate() {
  const KEYWORD = "keyword";
  const ONE_DAY_IN_MS = 24 * 60 * 60 * 1000;

  let [engine] = await addTestEngines([
    { name: "Test search engine", xmlFileName: "engine.xml" },
  ]);

  engine.alias = KEYWORD;
  await Services.search.moveEngine(engine, 0);

  Assert.ok(
    !Services.search.getEngineByName("A second test engine"),
    "Should not be able to get the engine by the new name"
  );

  // can't have an accurate updateURL in the file since we can't know the test
  // server origin, so manually set it
  engine.wrappedJSObject._updateURL = gDataUrl + "engine2.xml";

  let promiseUpdate = SearchTestUtils.promiseSearchNotification(
    SearchUtils.MODIFIED_TYPE.CHANGED,
    SearchUtils.TOPIC_ENGINE_MODIFIED
  );

  // set last update to 8 days ago, since the default interval is 7, then
  // trigger an update
  engine.wrappedJSObject.setAttr("updateexpir", Date.now() - ONE_DAY_IN_MS * 8);
  Services.search.QueryInterface(Ci.nsITimerCallback).notify(null);

  let changedEngine = await promiseUpdate;

  Assert.equal(
    engine.name,
    "A second test engine",
    "Should have update the engines name"
  );

  Assert.equal(changedEngine.alias, KEYWORD, "Keyword not cleared by update");
  Assert.equal(
    changedEngine.wrappedJSObject.getAttr("order"),
    1,
    "Order not cleared by update"
  );

  Assert.ok(
    !!Services.search.getEngineByName("A second test engine"),
    "Should be able to get the engine by the new name"
  );
  Assert.ok(
    !Services.search.getEngineByName("Test search engine"),
    "Should not be able to get the engine by the old name"
  );
});
