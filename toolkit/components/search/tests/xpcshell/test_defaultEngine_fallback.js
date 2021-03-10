/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * This test is checking the fallbacks when an engine that is default is
 * removed or hidden.
 *
 * The fallback procedure is:
 *
 * - Region/Locale default (if visible)
 * - First visible engine
 * - If no other visible engines, unhide the region/locale default and use it.
 */

let originalDefault;
let originalPrivateDefault;

add_task(async function setup() {
  useHttpServer();
  await SearchTestUtils.useTestEngines();

  Services.prefs.setCharPref(SearchUtils.BROWSER_SEARCH_PREF + "region", "US");
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault.ui.enabled",
    true
  );
  Services.prefs.setBoolPref(
    SearchUtils.BROWSER_SEARCH_PREF + "separatePrivateDefault",
    true
  );

  Services.search.wrappedJSObject.GENERAL_SEARCH_ENGINE_IDS = new Set([
    "engine-resourceicon@search.mozilla.org",
    "engine-reordered@search.mozilla.org",
  ]);

  await AddonTestUtils.promiseStartupManager();

  originalDefault = await Services.search.getDefault();
  originalPrivateDefault = await Services.search.getDefaultPrivate();
});

function getDefault(privateMode) {
  return privateMode
    ? Services.search.getDefaultPrivate()
    : Services.search.getDefault();
}

function setDefault(privateMode, engine) {
  return privateMode
    ? Services.search.setDefaultPrivate(engine)
    : Services.search.setDefault(engine);
}

async function checkFallbackDefaultRegion(private) {
  let original = private ? originalPrivateDefault : originalDefault;
  let expectedDefaultNotification = private
    ? SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
    : SearchUtils.MODIFIED_TYPE.DEFAULT;
  Services.search.restoreDefaultEngines();

  let otherEngine = Services.search.getEngineByName("engine-chromeicon");
  await setDefault(private, otherEngine);

  Assert.notEqual(otherEngine, original, "Sanity check engines are different");

  const observer = new SearchObserver(
    [
      expectedDefaultNotification,
      // For hiding (removing) the engine.
      SearchUtils.MODIFIED_TYPE.CHANGED,
      SearchUtils.MODIFIED_TYPE.REMOVED,
    ],
    expectedDefaultNotification
  );

  await Services.search.removeEngine(otherEngine);

  let notified = await observer.promise;

  Assert.ok(otherEngine.hidden, "Should have hidden the removed engine");
  Assert.equal(
    (await getDefault(private)).name,
    original.name,
    "Should have reverted to the default to the region default"
  );
  Assert.equal(
    notified.name,
    original.name,
    "Should have notified the correct default engine"
  );
}

add_task(async function test_default_fallback_to_region_default() {
  await checkFallbackDefaultRegion(false);
});

add_task(async function test_default_private_fallback_to_region_default() {
  await checkFallbackDefaultRegion(true);
});

async function checkFallbackFirstVisible(private) {
  let original = private ? originalPrivateDefault : originalDefault;
  let expectedDefaultNotification = private
    ? SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
    : SearchUtils.MODIFIED_TYPE.DEFAULT;
  Services.search.restoreDefaultEngines();

  let otherEngine = Services.search.getEngineByName("engine-chromeicon");
  await setDefault(private, otherEngine);
  await Services.search.removeEngine(original);

  Assert.notEqual(otherEngine, original, "Sanity check engines are different");

  const observer = new SearchObserver(
    private
      ? [
          expectedDefaultNotification,
          // For hiding (removing) the engine.
          SearchUtils.MODIFIED_TYPE.CHANGED,
          SearchUtils.MODIFIED_TYPE.REMOVED,
        ]
      : [
          expectedDefaultNotification,
          // For hiding (removing) the engine.
          SearchUtils.MODIFIED_TYPE.CHANGED,
          SearchUtils.MODIFIED_TYPE.REMOVED,
        ],
    expectedDefaultNotification
  );

  await Services.search.removeEngine(otherEngine);

  let notified = await observer.promise;

  Assert.equal(
    (await getDefault(private)).name,
    "engine-resourceicon",
    "Should have set the default engine to the first visible general engine"
  );
  Assert.equal(
    notified.name,
    "engine-resourceicon",
    "Should have notified the correct default general engine"
  );
}

add_task(async function test_default_fallback_to_first_gen_visible() {
  await checkFallbackFirstVisible(false);
});

add_task(async function test_default_private_fallback_to_first_gen_visible() {
  await checkFallbackFirstVisible(true);
});

// Removing all visible engines affects both the default and private default
// engines.
add_task(async function test_default_fallback_when_no_others_visible() {
  // Remove all but one of the visible engines.
  let visibleEngines = await Services.search.getVisibleEngines();
  for (let i = 0; i < visibleEngines.length - 1; i++) {
    await Services.search.removeEngine(visibleEngines[i]);
  }
  Assert.equal(
    (await Services.search.getVisibleEngines()).length,
    1,
    "Should only have one visible engine"
  );

  const observer = new SearchObserver(
    [
      // Unhiding of the default engine.
      SearchUtils.MODIFIED_TYPE.CHANGED,
      // Change of the default.
      SearchUtils.MODIFIED_TYPE.DEFAULT,
      // Unhiding of the default private.
      SearchUtils.MODIFIED_TYPE.CHANGED,
      SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE,
      // Hiding the engine.
      SearchUtils.MODIFIED_TYPE.CHANGED,
      SearchUtils.MODIFIED_TYPE.REMOVED,
    ],
    SearchUtils.MODIFIED_TYPE.DEFAULT
  );

  // Now remove the last engine, which should set the new default.
  await Services.search.removeEngine(visibleEngines[visibleEngines.length - 1]);

  let notified = await observer.promise;

  Assert.equal(
    (await getDefault(false)).name,
    originalDefault.name,
    "Should fallback to the original default engine after removing all engines"
  );
  Assert.equal(
    (await getDefault(true)).name,
    originalPrivateDefault.name,
    "Should fallback to the original default private engine after removing all engines"
  );
  Assert.equal(
    notified.name,
    originalDefault.name,
    "Should have notified the correct default engine"
  );
  Assert.ok(
    !originalPrivateDefault.hidden,
    "Should have unhidden the original default private engine"
  );
  Assert.equal(
    (await Services.search.getVisibleEngines()).length,
    2,
    "Should now have two engines visible"
  );
});

add_task(async function test_default_fallback_remove_default_no_visible() {
  // Remove all but the default engine.
  Services.search.defaultPrivateEngine = Services.search.defaultEngine;
  let visibleEngines = await Services.search.getVisibleEngines();
  for (let engine of visibleEngines) {
    if (engine.name != originalDefault.name) {
      await Services.search.removeEngine(engine);
    }
  }
  Assert.equal(
    (await Services.search.getVisibleEngines()).length,
    1,
    "Should only have one visible engine"
  );

  const observer = new SearchObserver(
    [
      // Unhiding of the default engine.
      SearchUtils.MODIFIED_TYPE.CHANGED,
      // Change of the default.
      SearchUtils.MODIFIED_TYPE.DEFAULT,
      SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE,
      // Hiding the engine.
      SearchUtils.MODIFIED_TYPE.CHANGED,
      SearchUtils.MODIFIED_TYPE.REMOVED,
    ],
    SearchUtils.MODIFIED_TYPE.DEFAULT
  );

  // Now remove the last engine, which should set the new default.
  await Services.search.removeEngine(originalDefault);

  let notified = await observer.promise;

  Assert.equal(
    (await getDefault(false)).name,
    "engine-resourceicon",
    "Should fallback the default engine to the first general search engine"
  );
  Assert.equal(
    (await getDefault(true)).name,
    "engine-resourceicon",
    "Should fallback the default private engine to the first general search engine"
  );
  Assert.equal(
    notified.name,
    "engine-resourceicon",
    "Should have notified the correct default engine"
  );
  Assert.ok(
    !Services.search.getEngineByName("engine-resourceicon").hidden,
    "Should have unhidden the new engine"
  );
  Assert.equal(
    (await Services.search.getVisibleEngines()).length,
    1,
    "Should now have one engines visible"
  );
});

add_task(
  async function test_default_fallback_remove_default_no_visible_or_general() {
    // Reset.
    Services.search.restoreDefaultEngines();
    Services.search.defaultEngine = Services.search.defaultPrivateEngine = originalPrivateDefault;

    // Remove all but the default engine.
    let visibleEngines = await Services.search.getVisibleEngines();
    for (let engine of visibleEngines) {
      if (engine.name != originalPrivateDefault.name) {
        await Services.search.removeEngine(engine);
      }
    }
    Assert.deepEqual(
      (await Services.search.getVisibleEngines()).map(e => e.name),
      originalPrivateDefault.name,
      "Should only have one visible engine"
    );

    Services.search.wrappedJSObject.GENERAL_SEARCH_ENGINE_IDS.clear();

    const observer = new SearchObserver(
      [
        // Unhiding of the default engine.
        SearchUtils.MODIFIED_TYPE.CHANGED,
        // Change of the default.
        SearchUtils.MODIFIED_TYPE.DEFAULT,
        SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE,
        // Hiding the engine.
        SearchUtils.MODIFIED_TYPE.CHANGED,
        SearchUtils.MODIFIED_TYPE.REMOVED,
      ],
      SearchUtils.MODIFIED_TYPE.DEFAULT
    );

    // Now remove the last engine, which should set the new default.
    await Services.search.removeEngine(originalPrivateDefault);

    let notified = await observer.promise;

    Assert.equal(
      (await getDefault(false)).name,
      "Test search engine",
      "Should fallback to the first engine that isn't a general search engine"
    );
    Assert.equal(
      (await getDefault(true)).name,
      "Test search engine",
      "Should fallback the private engine to the first engine that isn't a general search engine"
    );
    Assert.equal(
      notified.name,
      "Test search engine",
      "Should have notified the correct default engine"
    );
    Assert.ok(
      !Services.search.getEngineByName("Test search engine").hidden,
      "Should have unhidden the new engine"
    );
    Assert.equal(
      (await Services.search.getVisibleEngines()).length,
      1,
      "Should now have one engines visible"
    );
  }
);

// Test the other remove engine route - for removing non-application provided
// engines.

async function checkNonBuiltinFallback(private) {
  let original = private ? originalPrivateDefault : originalDefault;
  let expectedDefaultNotification = private
    ? SearchUtils.MODIFIED_TYPE.DEFAULT_PRIVATE
    : SearchUtils.MODIFIED_TYPE.DEFAULT;
  Services.search.restoreDefaultEngines();

  const [addedEngine] = await addTestEngines([
    { name: "A second test engine", xmlFileName: "engine2.xml" },
  ]);

  await setDefault(private, addedEngine);

  const observer = new SearchObserver(
    [expectedDefaultNotification, SearchUtils.MODIFIED_TYPE.REMOVED],
    expectedDefaultNotification
  );

  // Remove the current engine...
  await Services.search.removeEngine(addedEngine);

  // ... and verify we've reverted to the normal default engine.
  Assert.equal(
    (await getDefault(private)).name,
    original.name,
    "Should revert to the original default engine"
  );

  let notified = await observer.promise;
  Assert.equal(
    notified.name,
    original.name,
    "Should have notified the correct default engine"
  );
}

add_task(async function test_default_fallback_non_builtin() {
  await checkNonBuiltinFallback(false);
});

add_task(async function test_default_fallback_non_builtin_private() {
  await checkNonBuiltinFallback(true);
});
