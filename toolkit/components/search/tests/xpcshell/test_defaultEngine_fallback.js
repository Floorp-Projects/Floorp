/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function setup() {
  useHttpServer();
  await useTestEngines();

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
});

function getDefault(privateMode) {
  return privateMode
    ? Services.search.getDefaultPrivate()
    : Services.search.getDefault();
}

async function checkBuiltinFallback(privateMode) {
  info(
    `Testing ${
      privateMode ? "private" : "normal"
    } default engine fallback (builtin)`
  );

  Assert.ok((await Services.search.getVisibleEngines()).length > 1);
  Assert.ok(Services.search.isInitialized);

  let defaultEngine = await getDefault(privateMode);
  await Services.search.removeEngine(defaultEngine);

  Assert.notEqual(
    (await getDefault(privateMode)).name,
    defaultEngine.name,
    "Should have changed the default to a different engine"
  );
  Assert.ok(defaultEngine.hidden, "Should have hidden the removed engine");

  for (let engine of await Services.search.getVisibleEngines()) {
    await Services.search.removeEngine(engine);
  }
  Assert.strictEqual(
    (await Services.search.getVisibleEngines()).length,
    0,
    "Should have no visible engines left after removal"
  );

  Assert.equal(
    (await getDefault(privateMode)).name,
    defaultEngine.name,
    "Should fallback to the original default engine after removing all engines"
  );
  Assert.ok(
    !defaultEngine.hidden,
    "Should have unhidden the original default engine"
  );
  Assert.equal(
    (await Services.search.getVisibleEngines()).length,
    1,
    "Should now have one engine visible"
  );
  if (privateMode) {
    // When all engines have been hidden, and the default is re-obtained,
    // then it first looks at visible engines, as the private engine has been
    // re-established, we get that here.
    Assert.equal(
      (await getDefault(false)).name,
      defaultEngine.name,
      "Should still have the correct default in normal mode after adjusting the private mode default"
    );
    await Services.search.setDefault(
      await Services.search.originalDefaultEngine
    );
  } else {
    await Services.search.setDefaultPrivate(
      await Services.search.originalPrivateDefaultEngine
    );
  }

  // Re-enable all engines ready for the next test.
  Services.search.restoreDefaultEngines();
}

add_task(async function test_default_fallback_builtin() {
  await checkBuiltinFallback(false);
});

add_task(async function test_default_fallback_builtin_private() {
  await checkBuiltinFallback(true);
});

async function checkNonBuiltinFallback(privateMode) {
  info(
    `Testing ${
      privateMode ? "private" : "normal"
    } default engine fallback (non-builtin)`
  );

  const [addedEngine] = await addTestEngines([
    { name: "A second test engine", xmlFileName: "engine2.xml" },
  ]);
  const defaultEngine = await getDefault(privateMode);

  if (privateMode) {
    await Services.search.setDefaultPrivate(addedEngine);
  } else {
    await Services.search.setDefault(addedEngine);
  }

  // Remove the current engine...
  await Services.search.removeEngine(addedEngine);

  // ... and verify we've reverted to the normal default engine.
  Assert.equal(
    (await getDefault(privateMode)).name,
    defaultEngine.name,
    "Should revert to the original default engine"
  );
}

add_task(async function test_default_fallback_non_builtin() {
  await checkNonBuiltinFallback(false);
});

add_task(async function test_default_fallback_non_builtin_private() {
  await checkNonBuiltinFallback(true);
});
