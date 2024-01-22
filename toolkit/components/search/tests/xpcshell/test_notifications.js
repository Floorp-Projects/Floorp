/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let engine;
let appDefaultEngine;

add_setup(async function () {
  await AddonTestUtils.promiseStartupManager();
  useHttpServer();

  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );

  appDefaultEngine = await Services.search.getDefault();
});

add_task(async function test_addingEngine_opensearch() {
  const addEngineObserver = new SearchObserver(
    [
      // engine-added
      // Engine was added to the store by the search service.
      SearchUtils.MODIFIED_TYPE.ADDED,
    ],
    SearchUtils.MODIFIED_TYPE.ADDED
  );

  await SearchTestUtils.promiseNewSearchEngine({
    url: gDataUrl + "engine.xml",
  });

  engine = await addEngineObserver.promise;

  let retrievedEngine = Services.search.getEngineByName("Test search engine");
  Assert.equal(engine, retrievedEngine);
});

add_task(async function test_addingEngine_webExtension() {
  const addEngineObserver = new SearchObserver(
    [
      // engine-added
      // Engine was added to the store by the search service.
      SearchUtils.MODIFIED_TYPE.ADDED,
    ],
    SearchUtils.MODIFIED_TYPE.ADDED
  );

  await SearchTestUtils.installSearchExtension({
    name: "Example Engine",
  });

  let webExtensionEngine = await addEngineObserver.promise;

  let retrievedEngine = Services.search.getEngineByName("Example Engine");
  Assert.equal(webExtensionEngine, retrievedEngine);
});

async function defaultNotificationTest(
  setPrivateDefault,
  expectNotificationForPrivate
) {
  const defaultObserver = new SearchObserver([
    expectNotificationForPrivate
      ? SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
      : SearchUtils.MODIFIED_TYPE.DEFAULT,
  ]);

  Services.search[
    setPrivateDefault ? "defaultPrivateEngine" : "defaultEngine"
  ] = engine;
  await defaultObserver.promise;
}

add_task(async function test_defaultEngine_notifications() {
  await defaultNotificationTest(false, false);
});

add_task(async function test_defaultPrivateEngine_notifications() {
  await defaultNotificationTest(true, true);
});

add_task(
  async function test_defaultPrivateEngine_notifications_when_not_enabled() {
    await Services.search.setDefault(
      appDefaultEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );

    Services.prefs.setBoolPref(
      SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
      false
    );

    await defaultNotificationTest(true, true);
  }
);

add_task(async function test_removeEngine() {
  await Services.search.setDefault(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );
  await Services.search.setDefaultPrivate(
    engine,
    Ci.nsISearchService.CHANGE_REASON_UNKNOWN
  );

  const removedObserver = new SearchObserver([
    SearchUtils.MODIFIED_TYPE.DEFAULT,
    SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE,
    SearchUtils.MODIFIED_TYPE.REMOVED,
  ]);

  await Services.search.removeEngine(engine);

  await removedObserver;
});
