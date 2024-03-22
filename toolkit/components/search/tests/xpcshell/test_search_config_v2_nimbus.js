/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

/* Test to verify search-config-v2 preference is correctly toggled via a Nimbus
   variable. */

"use strict";

ChromeUtils.defineESModuleGetters(this, {
  SearchTestUtils: "resource://testing-common/SearchTestUtils.sys.mjs",
  ExperimentAPI: "resource://nimbus/ExperimentAPI.sys.mjs",
  ExperimentFakes: "resource://testing-common/NimbusTestUtils.sys.mjs",
  ExperimentManager: "resource://nimbus/lib/ExperimentManager.sys.mjs",
  AppProvidedSearchEngine:
    "resource://gre/modules/AppProvidedSearchEngine.sys.mjs",
});

add_task(async function test_nimbus_experiment_enabled() {
  Assert.equal(
    Services.prefs.getBoolPref("browser.search.newSearchConfig.enabled"),
    false,
    "newSearchConfig.enabled PREF should initially be false."
  );

  await ExperimentManager.onStartup();
  await ExperimentAPI.ready();

  let doExperimentCleanup = await ExperimentFakes.enrollWithFeatureConfig(
    {
      featureId: "search",
      value: {
        newSearchConfigEnabled: true,
      },
    },
    { isRollout: true }
  );

  Assert.equal(
    Services.prefs.getBoolPref("browser.search.newSearchConfig.enabled"),
    true,
    "After toggling the Nimbus variable, the current value of newSearchConfig.enabled PREF should be true."
  );

  Assert.equal(
    SearchUtils.newSearchConfigEnabled,
    true,
    "After toggling the Nimbus variable, newSearchConfig.enabled should be cached as true."
  );

  await AddonTestUtils.promiseStartupManager();
  await Services.search.init();
  await SearchTestUtils.useTestEngines();

  let { engines: engines2 } =
    await Services.search.wrappedJSObject._fetchEngineSelectorEngines();

  Assert.ok(
    engines2.some(engine => engine.identifier),
    "Engines in the search-config-v2 format should have an identifier."
  );

  let appProvidedEngines =
    await Services.search.wrappedJSObject.getAppProvidedEngines();

  Assert.ok(
    appProvidedEngines.every(
      engine => engine instanceof AppProvidedSearchEngine
    ),
    "All application provided engines for search-config-v2 should be instances of AppProvidedSearchEngine."
  );

  await doExperimentCleanup();

  Assert.equal(
    Services.prefs.getBoolPref("browser.search.newSearchConfig.enabled"),
    false,
    "After experiment unenrollment, the newSearchConfig.enabled should be false."
  );

  Assert.equal(
    SearchUtils.newSearchConfigEnabled,
    true,
    "After experiment unenrollment, newSearchConfig.enabled should be cached as true."
  );
});
