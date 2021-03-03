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
  Services.search.restoreDefaultEngines();

  let otherEngine = Services.search.getEngineByName("engine-chromeicon");
  await setDefault(private, otherEngine);

  Assert.notEqual(otherEngine, original, "Sanity check engines are different");

  await Services.search.removeEngine(otherEngine);

  Assert.ok(otherEngine.hidden, "Should have hidden the removed engine");
  Assert.equal(
    (await getDefault(private)).name,
    original.name,
    "Should have reverted to the default to the region default"
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
  Services.search.restoreDefaultEngines();

  let visibleEngines = await Services.search.getVisibleEngines();

  let otherEngine = Services.search.getEngineByName("engine-chromeicon");
  await setDefault(private, otherEngine);
  await Services.search.removeEngine(original);

  Assert.notEqual(otherEngine, original, "Sanity check engines are different");

  await Services.search.removeEngine(otherEngine);

  Assert.equal(
    (await getDefault(private)).name,
    // In data/engines.json, there are different engines for normal and private
    // modes. This gives a different first visible engine out of the original
    // list.
    visibleEngines[private ? 0 : 1].name,
    "Should have set the default engine to the first visible"
  );
}

add_task(async function test_default_fallback_to_first_visible() {
  await checkFallbackFirstVisible(false);
});

add_task(async function test_default_private_fallback_to_first_visible() {
  await checkFallbackFirstVisible(true);
});

// Removing all visible engines affects both the default and private default
// engines.
add_task(async function test_default_fallback_when_no_others_visible() {
  for (let engine of await Services.search.getVisibleEngines()) {
    await Services.search.removeEngine(engine);
  }
  Assert.strictEqual(
    (await Services.search.getVisibleEngines()).length,
    0,
    "Should have no visible engines left after removal"
  );

  Assert.equal(
    (await getDefault(false)).name,
    originalDefault.name,
    "Should fallback to the original default engine after removing all engines"
  );
  Assert.equal(
    (await getDefault(true)).name,
    originalDefault.name,
    "Should have changed the default private to be the same as the original default engine after removing all engines"
  );
  Assert.ok(
    !originalDefault.hidden,
    "Should have unhidden the original default engine"
  );
  Assert.equal(
    (await Services.search.getVisibleEngines()).length,
    1,
    "Should now have one engine visible"
  );
});

// Test the other remove engine route - for removing non-application provided
// engines.

async function checkNonBuiltinFallback(private) {
  let original = private ? originalPrivateDefault : originalDefault;
  Services.search.restoreDefaultEngines();

  const [addedEngine] = await addTestEngines([
    { name: "A second test engine", xmlFileName: "engine2.xml" },
  ]);

  await setDefault(private, addedEngine);

  // Remove the current engine...
  await Services.search.removeEngine(addedEngine);

  // ... and verify we've reverted to the normal default engine.
  Assert.equal(
    (await getDefault(private)).name,
    original.name,
    "Should revert to the original default engine"
  );
}

add_task(async function test_default_fallback_non_builtin() {
  await checkNonBuiltinFallback(false);
});

add_task(async function test_default_fallback_non_builtin_private() {
  await checkNonBuiltinFallback(true);
});
