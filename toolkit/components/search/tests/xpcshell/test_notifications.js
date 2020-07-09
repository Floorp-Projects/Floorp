/* Any copyright is dedicated to the Public Domain.
 *    http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

let engine;
let originalDefaultEngine;

SearchTestUtils.init(Assert, registerCleanupFunction);
SearchTestUtils.initXPCShellAddonManager(this);

/**
 * A simple observer to ensure we get only the expected notifications.
 */
class SearchObserver {
  constructor(expectedNotifications, returnAddedEngine = false) {
    this.observer = this.observer.bind(this);
    this.deferred = PromiseUtils.defer();
    this.expectedNotifications = expectedNotifications;
    this.returnAddedEngine = returnAddedEngine;

    Services.obs.addObserver(this.observer, SearchUtils.TOPIC_ENGINE_MODIFIED);
  }

  get promise() {
    return this.deferred.promise;
  }

  observer(subject, topic, data) {
    Assert.greater(
      this.expectedNotifications.length,
      0,
      "Should be expecting a notification"
    );
    Assert.equal(
      data,
      this.expectedNotifications[0],
      "Should have received the next expected notification"
    );

    if (this.returnAddedEngine && data == SearchUtils.MODIFIED_TYPE.ADDED) {
      this.addedEngine = subject.QueryInterface(Ci.nsISearchEngine);
    }

    this.expectedNotifications.shift();

    if (!this.expectedNotifications.length) {
      this.deferred.resolve(this.addedEngine);
      Services.obs.removeObserver(
        this.observer,
        SearchUtils.TOPIC_ENGINE_MODIFIED
      );
    }
  }
}

add_task(async function setup() {
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

  originalDefaultEngine = await Services.search.getDefault();
});

add_task(async function test_addingEngine_opensearch() {
  const addEngineObserver = new SearchObserver(
    [
      // engine-loaded
      // Engine was loaded.
      SearchUtils.MODIFIED_TYPE.LOADED,
      // engine-added
      // Engine was added to the store by the search service.
      SearchUtils.MODIFIED_TYPE.ADDED,
    ],
    true
  );

  await Services.search.addOpenSearchEngine(gDataUrl + "engine.xml", null);

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
    true
  );

  let extension = await SearchTestUtils.installSearchExtension({
    name: "Example Engine",
  });
  await extension.awaitStartup();

  let webExtensionEngine = await addEngineObserver.promise;

  let retrievedEngine = Services.search.getEngineByName("Example Engine");
  Assert.equal(webExtensionEngine, retrievedEngine);
  await extension.unload();
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
    await Services.search.setDefault(originalDefaultEngine);

    Services.prefs.setBoolPref(
      SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
      false
    );

    await defaultNotificationTest(true, true);
  }
);

add_task(async function test_removeEngine() {
  const removedObserver = new SearchObserver([
    SearchUtils.MODIFIED_TYPE.REMOVED,
  ]);

  await Services.search.removeEngine(engine);

  await removedObserver;
});
